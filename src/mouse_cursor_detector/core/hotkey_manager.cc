#include "mouse_cursor_detector/core/hotkey_manager.h"

namespace mouse_cursor_detector {

bool HotkeyManager::Replace(HWND owner, UINT modifiers, UINT vk,
                            UINT secondary_vk) {
  if (vk == 0 || vk >= pressed_.size() || secondary_vk >= pressed_.size() ||
      secondary_vk == vk) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }
  if (owner_ != owner) {
    // Observe physical keys without reserving shortcuts or suppressing input.
    RAWINPUTDEVICE device{0x01, 0x06, RIDEV_INPUTSINK, owner};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return false;
    owner_ = owner;
  }
  modifiers_ = modifiers;
  vk_ = vk;
  secondary_vk_ = secondary_vk;
  RefreshKeyState();
  return true;
}

void HotkeyManager::RefreshKeyState() {
  for (UINT key = 0; key < pressed_.size(); ++key) {
    pressed_[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
  }
  chord_active_ =
      pressed_[vk_] && (secondary_vk_ == 0 || pressed_[secondary_vk_]);
}

UINT HotkeyManager::Modifiers() const {
  UINT result = 0;
  if (pressed_[VK_LCONTROL] || pressed_[VK_RCONTROL]) result |= MOD_CONTROL;
  if (pressed_[VK_LMENU] || pressed_[VK_RMENU]) result |= MOD_ALT;
  if (pressed_[VK_LSHIFT] || pressed_[VK_RSHIFT]) result |= MOD_SHIFT;
  if (pressed_[VK_LWIN] || pressed_[VK_RWIN]) result |= MOD_WIN;
  return result;
}

bool HotkeyManager::ProcessInput(HRAWINPUT input) {
  RAWINPUT raw{};
  UINT size = sizeof(raw);
  const UINT bytes =
      GetRawInputData(input, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
  if (bytes == static_cast<UINT>(-1) ||
      bytes < sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD) ||
      raw.header.dwType != RIM_TYPEKEYBOARD) {
    return false;
  }
  const auto& keyboard = raw.data.keyboard;
  UINT key = keyboard.VKey;
  if (key == 0 || key >= 0xff || keyboard.MakeCode == 0xff) return false;
  if (key == VK_SHIFT) {
    key = MapVirtualKeyW(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);
  } else if (key == VK_CONTROL) {
    key = keyboard.Flags & RI_KEY_E0 ? VK_RCONTROL : VK_LCONTROL;
  } else if (key == VK_MENU) {
    key = keyboard.Flags & RI_KEY_E0 ? VK_RMENU : VK_LMENU;
  }
  if (key == 0 || key >= pressed_.size()) return false;
  const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
  const bool repeated = pressed_[key];
  pressed_[key] = down;
  UINT modifiers = Modifiers();
  // A modifier can itself be chosen as a single key or as the final key.
  const UINT bound_keys[] = {vk_, secondary_vk_};
  for (const UINT bound : bound_keys) {
    if (bound == VK_LCONTROL || bound == VK_RCONTROL) modifiers &= ~MOD_CONTROL;
    if (bound == VK_LMENU || bound == VK_RMENU) modifiers &= ~MOD_ALT;
    if (bound == VK_LSHIFT || bound == VK_RSHIFT) modifiers &= ~MOD_SHIFT;
    if (bound == VK_LWIN || bound == VK_RWIN) modifiers &= ~MOD_WIN;
  }
  const bool matched = owner_ && pressed_[vk_] &&
                       (secondary_vk_ == 0 || pressed_[secondary_vk_]) &&
                       modifiers == modifiers_;
  if (!matched) {
    chord_active_ = false;
    return false;
  }
  if (!down || repeated || chord_active_ ||
      (key != vk_ && key != secondary_vk_))
    return false;
  chord_active_ = true;
  return true;
}

void HotkeyManager::Reset() {
  if (owner_) {
    RAWINPUTDEVICE device{0x01, 0x06, RIDEV_REMOVE, nullptr};
    RegisterRawInputDevices(&device, 1, sizeof(device));
  }
  owner_ = nullptr;
  pressed_.fill(false);
  vk_ = 0;
  secondary_vk_ = 0;
  chord_active_ = false;
  modifiers_ = 0;
}
}  // namespace mouse_cursor_detector
