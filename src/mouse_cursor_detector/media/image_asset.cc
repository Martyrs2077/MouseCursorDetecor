#include "mouse_cursor_detector/media/image_asset.h"

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <limits>
#include <utility>

#include "mouse_cursor_detector/core/startup_manager.h"

namespace mouse_cursor_detector {
namespace {
using Microsoft::WRL::ComPtr;
constexpr uint64_t kMaxFileBytes = 20 * 1024 * 1024;
constexpr uint64_t kMaxCacheBytes = 64 * 1024 * 1024;
constexpr uint64_t kMaxDecodedPixels = 64 * 1024 * 1024;

class ComScope {
 public:
  ComScope() : status_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
  ~ComScope() {
    if (SUCCEEDED(status_)) CoUninitialize();
  }
  bool ok() const {
    return SUCCEEDED(status_) || status_ == RPC_E_CHANGED_MODE;
  }

 private:
  HRESULT status_;
};

UINT Metadata(IWICMetadataQueryReader* reader, const wchar_t* name,
              UINT fallback) {
  if (!reader) return fallback;
  PROPVARIANT value{};
  if (FAILED(reader->GetMetadataByName(name, &value))) {
    PropVariantClear(&value);
    return fallback;
  }
  UINT result = fallback;
  if (value.vt == VT_UI1) result = value.bVal;
  if (value.vt == VT_UI2) result = value.uiVal;
  if (value.vt == VT_UI4) result = value.ulVal;
  if (value.vt == VT_BOOL) result = value.boolVal != VARIANT_FALSE;
  PropVariantClear(&value);
  return result;
}

bool CopyPixels(IWICImagingFactory* factory, IWICBitmapSource* source,
                UINT width, UINT height, std::vector<BYTE>* pixels) {
  ComPtr<IWICBitmapScaler> scaler;
  if (FAILED(factory->CreateBitmapScaler(scaler.GetAddressOf())) ||
      FAILED(scaler->Initialize(source, width, height,
                                WICBitmapInterpolationModeFant)))
    return false;
  ComPtr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
      FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppPBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom))) {
    return false;
  }
  pixels->resize(static_cast<size_t>(width) * height * 4);
  return SUCCEEDED(converter->CopyPixels(
      nullptr, width * 4, static_cast<UINT>(pixels->size()), pixels->data()));
}

void FitSize(UINT width, UINT height, UINT maximum, UINT* out_width,
             UINT* out_height) {
  const double scale =
      std::min(1.0, static_cast<double>(maximum) / std::max(width, height));
  *out_width = std::max(1u, static_cast<UINT>(width * scale));
  *out_height = std::max(1u, static_cast<UINT>(height * scale));
}

bool Decode(IWICImagingFactory* factory, IWICBitmapDecoder* decoder,
            ImageAsset* asset) {
  GUID format{};
  UINT count = 0;
  if (FAILED(decoder->GetContainerFormat(&format)) ||
      FAILED(decoder->GetFrameCount(&count)) || count == 0)
    return false;
  asset->is_gif = IsEqualGUID(format, GUID_ContainerFormatGif);
  UINT selected = 0;
  // ICO frames are alternative resolutions, not animation frames.
  if (IsEqualGUID(format, GUID_ContainerFormatIco)) {
    UINT best = std::numeric_limits<UINT>::max();
    for (UINT index = 0; index < std::min(count, 256u); ++index) {
      ComPtr<IWICBitmapFrameDecode> frame;
      UINT width = 0, height = 0;
      if (FAILED(decoder->GetFrame(index, frame.GetAddressOf())) ||
          FAILED(frame->GetSize(&width, &height)))
        continue;
      const UINT score = width >= 256 ? width - 256 : 10000 + 256 - width;
      if (score < best) {
        best = score;
        selected = index;
      }
    }
  }
  ComPtr<IWICBitmapFrameDecode> first;
  UINT width = 0, height = 0;
  if (FAILED(decoder->GetFrame(selected, first.GetAddressOf())) ||
      FAILED(first->GetSize(&width, &height)) || !width || !height ||
      width > 4096 || height > 4096)
    return false;
  if (!asset->is_gif) {
    FitSize(width, height, 512, &asset->width, &asset->height);
    ImageFrame frame;
    if (!CopyPixels(factory, first.Get(), asset->width, asset->height,
                    &frame.pixels))
      return false;
    asset->frames.push_back(std::move(frame));
    asset->duration_ms = 100;
    return true;
  }
  if (count > 200) return false;
  ComPtr<IWICMetadataQueryReader> global;
  decoder->GetMetadataQueryReader(global.GetAddressOf());
  width = Metadata(global.Get(), L"/logscrdesc/Width", width);
  height = Metadata(global.Get(), L"/logscrdesc/Height", height);
  if (!width || !height || width > 1024 || height > 1024 ||
      static_cast<uint64_t>(width) * height * count > kMaxDecodedPixels) {
    return false;
  }
  FitSize(width, height, 256, &asset->width, &asset->height);
  const size_t canvas_bytes = static_cast<size_t>(width) * height * 4;
  std::vector<BYTE> canvas(canvas_bytes, 0);
  std::vector<BYTE> previous;
  // Transparent GIFs use a transparent logical canvas for desktop overlays.
  ComPtr<IWICMetadataQueryReader> first_metadata;
  first->GetMetadataQueryReader(first_metadata.GetAddressOf());
  uint32_t background = 0;
  if (!Metadata(first_metadata.Get(), L"/grctlext/TransparencyFlag", 0)) {
    ComPtr<IWICPalette> palette;
    WICColor colors[256]{};
    UINT actual = 0;
    const UINT background_index =
        Metadata(global.Get(), L"/logscrdesc/BackgroundColorIndex", 256);
    if (SUCCEEDED(factory->CreatePalette(palette.GetAddressOf())) &&
        SUCCEEDED(decoder->CopyPalette(palette.Get())) &&
        SUCCEEDED(palette->GetColors(256, colors, &actual)) &&
        background_index < actual)
      background = colors[background_index] | 0xff000000;
  }
  const auto clear = [&](UINT left, UINT top, UINT right, UINT bottom,
                         uint32_t color) {
    for (UINT y = top; y < bottom; ++y) {
      for (UINT x = left; x < right; ++x) {
        const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
        for (UINT channel = 0; channel < 4; ++channel) {
          canvas[offset + channel] = static_cast<BYTE>(color >> (8 * channel));
        }
      }
    }
  };
  clear(0, 0, width, height, background);
  for (UINT index = 0; index < count; ++index) {
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICMetadataQueryReader> metadata;
    UINT frame_width = 0, frame_height = 0;
    if (FAILED(decoder->GetFrame(index, frame.GetAddressOf())) ||
        FAILED(frame->GetSize(&frame_width, &frame_height)) || !frame_width ||
        !frame_height || frame_width > width || frame_height > height)
      return false;
    frame->GetMetadataQueryReader(metadata.GetAddressOf());
    const UINT left = Metadata(metadata.Get(), L"/imgdesc/Left", 0);
    const UINT top = Metadata(metadata.Get(), L"/imgdesc/Top", 0);
    if (left > width - frame_width || top > height - frame_height) return false;
    const UINT disposal = Metadata(metadata.Get(), L"/grctlext/Disposal", 0);
    if (disposal > 3) return false;
    if (disposal == 3) previous = canvas;
    std::vector<BYTE> raw;
    if (!CopyPixels(factory, frame.Get(), frame_width, frame_height, &raw)) {
      return false;
    }
    for (UINT y = 0; y < frame_height; ++y) {
      for (UINT x = 0; x < frame_width; ++x) {
        const size_t from = (static_cast<size_t>(y) * frame_width + x) * 4;
        const size_t to = (static_cast<size_t>(top + y) * width + left + x) * 4;
        const UINT inverse_alpha = 255 - raw[from + 3];
        for (UINT channel = 0; channel < 4; ++channel) {
          canvas[to + channel] = static_cast<BYTE>(
              raw[from + channel] +
              (canvas[to + channel] * inverse_alpha + 127) / 255);
        }
      }
    }
    const UINT delay = Metadata(metadata.Get(), L"/grctlext/Delay", 10);
    // Zero-delay intermediate frames still affect composition.
    if (delay || index + 1 == count) {
      if (static_cast<uint64_t>(asset->width) * asset->height * 4 *
              (asset->frames.size() + 1) >
          kMaxCacheBytes)
        return false;
      ComPtr<IWICBitmap> composed;
      if (FAILED(factory->CreateBitmapFromMemory(
              width, height, GUID_WICPixelFormat32bppPBGRA, width * 4,
              static_cast<UINT>(canvas.size()), canvas.data(),
              composed.GetAddressOf())))
        return false;
      ImageFrame output;
      output.delay_ms =
          delay ? std::max(std::min(delay, 6000u) * 10u, 20u) : 100;
      if (!CopyPixels(factory, composed.Get(), asset->width, asset->height,
                      &output.pixels))
        return false;
      asset->duration_ms += output.delay_ms;
      asset->frames.push_back(std::move(output));
    }
    if (disposal == 2) {
      const uint32_t fill =
          Metadata(metadata.Get(), L"/grctlext/TransparencyFlag", 0)
              ? 0
              : background;
      clear(left, top, left + frame_width, top + frame_height, fill);
    } else if (disposal == 3) {
      canvas.swap(previous);
    }
  }
  return !asset->frames.empty();
}
}  // namespace

std::wstring ApplicationDirectory() {
  return std::filesystem::path(ExecutablePath()).parent_path().wstring();
}

bool IsManagedStylePath(const std::wstring& path) {
  constexpr wchar_t prefix[] = L"styles/";
  if (path.compare(0, 7, prefix) != 0 || path.size() <= 7 || path.size() > 240)
    return false;
  const auto name = path.substr(7);
  return name != L"." && name != L".." &&
         name.find_first_of(L"/\\:\"<>|?*\r\n") == std::wstring::npos &&
         name.back() != L'.' && name.back() != L' ' &&
         std::none_of(name.begin(), name.end(),
                      [](wchar_t c) { return c < 32; });
}

std::shared_ptr<const ImageAsset> LoadStyleFile(const std::wstring& path,
                                                std::wstring* error) {
  *error = L"无法读取素材，格式不受支持、文件损坏或超出限制。";
  try {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || !bytes || bytes > kMaxFileBytes) {
      *error = L"素材不存在、无法读取或大小超过 20 MiB。";
      return nullptr;
    }
    ComScope com;
    if (!com.ok()) return nullptr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.GetAddressOf()))))
      return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf())))
      return nullptr;
    auto asset = std::make_shared<ImageAsset>();
    if (!Decode(factory.Get(), decoder.Get(), asset.get())) {
      *error =
          L"素材解码失败或超限：静态图最大 4096×4096；GIF 最大 "
          L"1024×1024、200 帧、64 Mi 像素处理量及 64 MiB 帧缓存。";
      return nullptr;
    }
    error->clear();
    return asset;
  } catch (const std::exception&) {
    *error = L"素材处理失败，文件无效或可用内存不足。";
    return nullptr;
  }
}

StyleImportResult ImportStyleFile(const std::wstring& source) {
  StyleImportResult result;
  std::filesystem::path destination;
  try {
    const auto root = ApplicationDirectory();
    if (root.empty()) {
      result.error = L"无法确定软件目录。";
      return result;
    }
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(source, ec);
    if (ec || !bytes || bytes > kMaxFileBytes) {
      result.error = L"请选择有效图片，文件大小不能超过 20 MiB。";
      return result;
    }
    const auto directory = std::filesystem::path(root) / L"styles";
    std::filesystem::create_directories(directory, ec);
    if (ec) {
      result.error = L"无法创建 styles 文件夹，请检查软件目录写入权限。";
      return result;
    }
    GUID id{};
    wchar_t name[40]{};
    if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id, name, 40)) {
      result.error = L"无法生成素材文件名。";
      return result;
    }
    auto extension = std::filesystem::path(source).extension().wstring();
    if (extension.size() > 12 ||
        extension.find_first_not_of(L".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN"
                                    L"OPQRSTUVWXYZ0123456789") !=
            std::wstring::npos)
      extension = L".image";
    const auto filename = std::wstring(name + 1, 36) + extension;
    destination = directory / filename;
    if (!CopyFileW(source.c_str(), destination.c_str(), TRUE)) {
      result.error = L"无法复制素材，请检查源文件与软件目录写入权限。";
      return result;
    }
    result.asset = LoadStyleFile(destination.wstring(), &result.error);
    if (!result.asset) {
      std::filesystem::remove(destination, ec);
      return result;
    }
    result.relative_path = L"styles/" + filename;
    result.original_filename =
        std::filesystem::path(source).filename().wstring();
  } catch (const std::exception&) {
    if (!destination.empty()) {
      std::error_code ec;
      std::filesystem::remove(destination, ec);
    }
    result.asset.reset();
    result.error = L"导入失败，请检查文件路径和可用空间。";
  }
  return result;
}
}  // namespace mouse_cursor_detector
