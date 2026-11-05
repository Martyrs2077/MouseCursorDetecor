#ifndef MOUSE_CURSOR_DETECTOR_MEDIA_IMAGE_ASSET_H_
#define MOUSE_CURSOR_DETECTOR_MEDIA_IMAGE_ASSET_H_

#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mouse_cursor_detector {
struct ImageFrame {
  std::vector<BYTE> pixels;  // Premultiplied BGRA, top-down.
  UINT delay_ms = 100;
};

struct ImageAsset {
  UINT width = 0;
  UINT height = 0;
  bool is_gif = false;
  uint64_t duration_ms = 0;
  std::vector<ImageFrame> frames;
};

struct StyleImportResult {
  std::wstring relative_path;
  std::wstring original_filename;
  std::shared_ptr<const ImageAsset> asset;
  std::shared_ptr<const ImageAsset> thumbnail;
  std::wstring error;
};

std::wstring ApplicationDirectory();
bool IsManagedStylePath(const std::wstring& relative_path);
std::shared_ptr<const ImageAsset> LoadStyleFile(const std::wstring& path,
                                                std::wstring* error,
                                                bool thumbnail = false);
StyleImportResult ImportStyleFile(
    const std::wstring& source,
    const std::vector<std::wstring>& reserved_paths);
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_MEDIA_IMAGE_ASSET_H_
