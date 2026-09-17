#include "DerTondehrJazzBeat.h"
#include "IPlug_include_in_plug_src.h"
#include "ui/JazzBeatUI.h"
#include "ui/UIScaling.h"
#include "preset/JazzBeatPresetFormat.h"
#include "preset/JazzBeatPresetMap.h"
#include "IControls.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
  #include <xmmintrin.h>
#endif

#if defined(OS_WIN) || defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

namespace {

std::string JazzBeatLowerASCII(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::filesystem::path JazzBeatPathFromUTF8(const std::string& text) {
  return std::filesystem::u8path(text);
}

std::string JazzBeatPathToUTF8(const std::filesystem::path& path) {
#if __cplusplus >= 202002L
  const auto value = path.u8string();
  return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
  return path.u8string();
#endif
}

std::filesystem::path JazzBeatUserConfigDirectory() {
  namespace fs = std::filesystem;
  fs::path root;
#ifdef _WIN32
  if (const wchar_t* local = _wgetenv(L"LOCALAPPDATA")) if (*local) root = fs::path(local);
  if (root.empty()) if (const wchar_t* roaming = _wgetenv(L"APPDATA")) if (*roaming) root = fs::path(roaming);
  if (root.empty()) if (const wchar_t* profile = _wgetenv(L"USERPROFILE")) if (*profile) root = fs::path(profile);
#else
  if (const char* home = std::getenv("HOME")) if (*home) root = fs::u8path(home) / ".config";
#endif
  if (root.empty()) {
    std::error_code ec;
    root = fs::current_path(ec);
    if (ec) root = fs::path(".");
  }
  const fs::path directory = root / "Der Tondehr" / "DerTondehrJazzBeat";
  std::error_code ec;
  fs::create_directories(directory, ec);
  return directory;
}

bool JazzBeatPathEntryIsLinkOrReparse(const std::filesystem::path& path) {
  namespace fs = std::filesystem;
#if defined(_WIN32)
  const DWORD attributes = ::GetFileAttributesW(path.wstring().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) return false;
  return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
  std::error_code ec;
  const fs::file_status status = fs::symlink_status(path, ec);
  return !ec && fs::is_symlink(status);
#endif
}

bool JazzBeatPathHasAnyLinkOrReparse(const std::filesystem::path& inputPath) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path path = fs::absolute(inputPath, ec);
  if (ec) { ec.clear(); path = inputPath; }
  path = path.lexically_normal();
  fs::path walk;
  for (const auto& component : path) {
    walk /= component;
    if (JazzBeatPathEntryIsLinkOrReparse(walk)) return true;
  }
  return false;
}

std::filesystem::path JazzBeatCanonicalForBoundary(const std::filesystem::path& path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path resolved = fs::weakly_canonical(path, ec);
  if (ec) {
    ec.clear();
    resolved = fs::absolute(path, ec);
    if (ec) resolved = path;
  }
  return resolved.lexically_normal();
}

std::string JazzBeatComparablePathComponent(const std::filesystem::path& component) {
  std::string value = JazzBeatPathToUTF8(component);
#ifdef _WIN32
  value = JazzBeatLowerASCII(std::move(value));
#endif
  return value;
}

bool JazzBeatLogicalPathIsSameOrBelow(const std::filesystem::path& candidatePath,
                                      const std::filesystem::path& rootPath) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path candidate = fs::absolute(candidatePath, ec);
  if (ec) { ec.clear(); candidate = candidatePath; }
  fs::path root = fs::absolute(rootPath, ec);
  if (ec) root = rootPath;
  candidate = candidate.lexically_normal();
  root = root.lexically_normal();
  auto ci = candidate.begin();
  for (auto ri = root.begin(); ri != root.end(); ++ri, ++ci) {
    if (ci == candidate.end()) return false;
    if (JazzBeatComparablePathComponent(*ci) != JazzBeatComparablePathComponent(*ri)) return false;
  }
  return true;
}

bool JazzBeatPathHasLinkOrReparseBelowRoot(const std::filesystem::path& candidatePath,
                                           const std::filesystem::path& rootPath) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path root = fs::absolute(rootPath, ec);
  if (ec) { ec.clear(); root = rootPath; }
  fs::path candidate = fs::absolute(candidatePath, ec);
  if (ec) { ec.clear(); candidate = candidatePath; }
  root = root.lexically_normal();
  candidate = candidate.lexically_normal();
  if (JazzBeatPathEntryIsLinkOrReparse(root)) return true;
  auto ci = candidate.begin();
  for (auto ri = root.begin(); ri != root.end(); ++ri, ++ci) {
    if (ci == candidate.end() || JazzBeatComparablePathComponent(*ri) != JazzBeatComparablePathComponent(*ci)) return true;
  }
  fs::path walk = root;
  for (; ci != candidate.end(); ++ci) {
    walk /= *ci;
    if (JazzBeatPathEntryIsLinkOrReparse(walk)) return true;
    ec.clear();
    if (!fs::exists(walk, ec) || ec) break;
  }
  return false;
}

bool JazzBeatPathIsSameOrBelow(const std::filesystem::path& candidatePath,
                               const std::filesystem::path& rootPath) {
  const auto candidate = JazzBeatCanonicalForBoundary(candidatePath);
  const auto root = JazzBeatCanonicalForBoundary(rootPath);
  auto ci = candidate.begin();
  for (auto ri = root.begin(); ri != root.end(); ++ri, ++ci) {
    if (ci == candidate.end()) return false;
    if (JazzBeatComparablePathComponent(*ci) != JazzBeatComparablePathComponent(*ri)) return false;
  }
  return true;
}

bool JazzBeatStablePathIsSameOrBelow(const std::filesystem::path& candidatePath,
                                     const std::filesystem::path& rootPath) {
  if (!JazzBeatLogicalPathIsSameOrBelow(candidatePath, rootPath)) return false;
  if (JazzBeatPathHasAnyLinkOrReparse(rootPath)
      || JazzBeatPathHasLinkOrReparseBelowRoot(candidatePath, rootPath)) return false;
  return JazzBeatPathIsSameOrBelow(candidatePath, rootPath);
}

bool JazzBeatPathsReferToSameLocation(const std::filesystem::path& a,
                                      const std::filesystem::path& b) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const bool equivalent = fs::equivalent(a, b, ec);
  if (!ec) return equivalent;
  const auto ca = JazzBeatCanonicalForBoundary(a);
  const auto cb = JazzBeatCanonicalForBoundary(b);
  auto ai = ca.begin(), bi = cb.begin();
  for (; ai != ca.end() && bi != cb.end(); ++ai, ++bi)
    if (JazzBeatComparablePathComponent(*ai) != JazzBeatComparablePathComponent(*bi)) return false;
  return ai == ca.end() && bi == cb.end();
}

bool JazzBeatDirectoryWritable(const std::filesystem::path& directory) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(directory, ec) || ec || !fs::is_directory(directory, ec) || ec) return false;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path probe = directory / (".jazzbeat_write_test_" + std::to_string(stamp) + ".tmp");
  {
    std::ofstream out(probe, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.put('\0'); out.flush();
    if (!out) { out.close(); fs::remove(probe, ec); return false; }
  }
  fs::remove(probe, ec);
  return true;
}

bool JazzBeatWriteBinaryAtomically(const std::filesystem::path& destination,
                                   const std::vector<std::uint8_t>& bytes) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const bool destinationExists = fs::exists(destination, ec) && !ec;
  fs::path temp = destination;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  temp += ".tmp." + std::to_string(stamp) + "." + std::to_string(reinterpret_cast<std::uintptr_t>(&bytes));
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) { out.close(); fs::remove(temp, ec); return false; }
  }
#if defined(_WIN32)
  HANDLE tempHandle = ::CreateFileW(temp.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (tempHandle == INVALID_HANDLE_VALUE || !::FlushFileBuffers(tempHandle)) {
    if (tempHandle != INVALID_HANDLE_VALUE) ::CloseHandle(tempHandle);
    fs::remove(temp, ec); return false;
  }
  ::CloseHandle(tempHandle);
  BOOL ok = destinationExists
    ? ::ReplaceFileW(destination.wstring().c_str(), temp.wstring().c_str(), nullptr,
                     REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)
    : ::MoveFileExW(temp.wstring().c_str(), destination.wstring().c_str(), MOVEFILE_WRITE_THROUGH);
  if (!ok) { fs::remove(temp, ec); return false; }
#else
  fs::rename(temp, destination, ec);
  if (ec) { fs::remove(temp, ec); return false; }
#endif
  return true;
}

bool JazzBeatIsPresetPath(const std::string& path) {
  std::string ext = JazzBeatPathToUTF8(JazzBeatPathFromUTF8(path).extension());
  ext = JazzBeatLowerASCII(std::move(ext));
  return ext == dtjb::preset::kExtension;
}

} // namespace

#if IPLUG_EDITOR
namespace
{
float GetInitialUIScaleForActiveMonitor()
{
#if defined(OS_WIN)
  HWND referenceWindow = GetForegroundWindow();
  HMONITOR monitor = referenceWindow
    ? MonitorFromWindow(referenceWindow, MONITOR_DEFAULTTONEAREST)
    : nullptr;

  if (!monitor)
  {
    POINT cursor{};
    if (GetCursorPos(&cursor))
      monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
  }

  MONITORINFO monitorInfo{};
  monitorInfo.cbSize = sizeof(monitorInfo);
  if (monitor && GetMonitorInfoW(monitor, &monitorInfo))
  {
    float dpiScale = 1.0f;
    using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = LoadLibraryW(L"Shcore.dll"))
    {
      const auto getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(
        GetProcAddress(shcore, "GetDpiForMonitor"));
      if (getDpiForMonitor)
      {
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX > 0)
          dpiScale = static_cast<float>(dpiX) / 96.0f;
      }
      FreeLibrary(shcore);
    }

    return dtjb::ui::CalculateMonitorFitScale(
      static_cast<float>(monitorInfo.rcWork.right - monitorInfo.rcWork.left),
      static_cast<float>(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top),
      dpiScale);
  }
#endif

  return GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT);
}
}
#endif

#if IPLUG_EDITOR
#if defined(APP_API)
class StandaloneMonoInputControl final : public IControl {
public:
  StandaloneMonoInputControl(const IRECT& bounds, DerTondehrJazzBeat& plugin)
  : IControl(bounds), mPlugin(plugin) {
    SetTooltip("Standalone input source inside the stereo hardware pair selected in Preferences");
  }

  void Draw(IGraphics& g) override {
    const IRECT b = mRECT.GetPadded(-0.5f);
    const IRECT label = IRECT(b.L, b.T, b.R, b.T + 15.f);
    const IRECT buttons = IRECT(b.L + 2.f, b.T + 18.f, b.R - 2.f, b.B - 3.f);
    const IColor panel(255, 38, 40, 39);
    const IColor border(235, 190, 181, 156);
    const IColor ink(255, 247, 241, 224);
    const IColor inkBright(255, 255, 249, 235);
    const IColor muted(255, 170, 176, 170);
    const IColor accent(255, 201, 83, 43);

    g.DrawText(IText(11.5f, inkBright, "DTJB_UI_BOLD", EAlign::Center), "MONO IN", label);
    g.FillRoundRect(IColor(130, 0, 0, 0), buttons.GetTranslated(0.f, 2.f), 3.4f);
    g.FillRoundRect(IColor(255, 16, 18, 18), buttons, 3.4f);
    g.FillRoundRect(panel, buttons.GetPadded(-1.2f), 2.5f);
    g.DrawRoundRect(border, buttons.GetPadded(-0.6f), 2.8f, nullptr, 0.9f);
    g.DrawLine(IColor(120, 240, 233, 213), buttons.L + 4.f, buttons.T + 1.f,
               buttons.R - 4.f, buttons.T + 1.f, nullptr, 0.65f);

    const int selected = mPlugin.StandaloneMonoInput();
    const float mid = buttons.MW();
    for (int i = 0; i < 2; ++i) {
      const IRECT r(i == 0 ? buttons.L : mid, buttons.T,
                    i == 0 ? mid : buttons.R, buttons.B);
      const IRECT fill = r.GetPadded(-2.f);
      if (selected == i)
        g.FillRoundRect(accent, fill, 1.9f);
      if (i == 1)
        g.DrawLine(IColor(170, 100, 106, 96), mid, buttons.T + 3.f, mid, buttons.B - 3.f, nullptr, 0.8f);
      g.DrawText(IText(13.5f, selected == i ? inkBright : ink, "DTJB_UI_BOLD", EAlign::Center),
                 i == 0 ? "1" : "2", r);
    }
  }

  void OnMouseDown(float x, float, const IMouseMod&) override {
    mPlugin.SetStandaloneMonoInput(x < mRECT.MW() ? 0 : 1);
    SetDirty(false);
  }

private:
  DerTondehrJazzBeat& mPlugin;
};
#endif

class JazzBeatPresetManagerControl final : public IControl {
public:
  JazzBeatPresetManagerControl(const IRECT& bounds, DerTondehrJazzBeat& plugin)
  : IControl(bounds), mPlugin(plugin) {
    mPlugin.mPresetBrowserOpen = false;
    mPlugin.ClearPresetDeleteIdentity();
    SetAnimation([this](IControl*) { mPlugin.PollPresetDirectoryChanges(); });
  }

  bool IsHit(float x, float y) const override {
    return mOpen ? mRECT.Contains(x, y) : HeaderBar().Contains(x, y);
  }

  void Draw(IGraphics& g) override {
    if (mDeleteArmed && !mPlugin.mPresetDeleteIdentityValid) mDeleteArmed = false;
    const unsigned int revision = mPlugin.mPresetUIRevision.load(std::memory_order_relaxed);
    if (revision != mLastRevision) { mLastRevision = revision; RefreshEntries(); }
    DrawHeader(g);
    if (!mOpen) return;

    const IRECT panel = PanelRect();
    g.FillRect(IColor(190, 5, 6, 6), mRECT);
    g.FillRoundRect(IColor(255, 28, 30, 29), panel, 7.f);
    g.DrawRoundRect(IColor(255, 92, 89, 78), panel, 7.f, nullptr, 1.f);
    g.DrawLine(IColor(180, 201, 83, 43), panel.L + 8.f, panel.T + 1.f,
               panel.R - 8.f, panel.T + 1.f, nullptr, 0.9f);
    g.DrawText(IText(15.f, IColor(255, 232, 222, 192), "DTJB_UI_BOLD", EAlign::Near),
               "DER TONDEHR JAZZ BEAT PRESETS", IRECT(panel.L + 12.f, panel.T + 8.f, panel.R - 50.f, panel.T + 35.f));
    DrawButton(g, CloseRect(panel), "X", false, false);

    const IRECT crumb = CrumbRect(panel);
    g.FillRoundRect(IColor(255, 18, 20, 20), crumb, 3.f);
    std::string crumbText = CurrentCrumb();
    if (mClipped) crumbText += "  |  Limited to 4096 entries";
    if (mListingIncomplete) crumbText += "  |  Listing incomplete";
    g.DrawText(IText(10.f, IColor(220, 190, 181, 156), "DTJB_UI", EAlign::Near),
               FitLabel(crumbText, crumb.W() - 12.f, 10.f).c_str(), crumb.GetPadded(-6.f));

    const IRECT list = ListRect(panel);
    g.FillRoundRect(IColor(255, 16, 18, 18), list, 3.f);
    g.DrawRoundRect(IColor(180, 77, 75, 66), list, 3.f, nullptr, 0.8f);
    constexpr float rowH = 27.f;
    const int visibleRows = std::max(1, static_cast<int>(list.H() / rowH));
    mScroll = std::clamp(mScroll, 0, std::max(0, static_cast<int>(mEntries.size()) - visibleRows));
    for (int row = 0; row < visibleRows; ++row) {
      const int idx = mScroll + row;
      if (idx >= static_cast<int>(mEntries.size())) break;
      const Entry& entry = mEntries[static_cast<std::size_t>(idx)];
      const IRECT rr(list.L, list.T + row * rowH, list.R, list.T + (row + 1) * rowH);
      const bool selected = !entry.isDirectory && !mPlugin.mPresetSelectedPath.empty()
        && JazzBeatPathsReferToSameLocation(JazzBeatPathFromUTF8(entry.path), JazzBeatPathFromUTF8(mPlugin.mPresetSelectedPath));
      if (selected) g.FillRect(IColor(255, 58, 49, 43), rr);
      const std::string prefix = entry.isDirectory ? ">  " : "   ";
      g.DrawText(IText(11.f, entry.isDirectory ? IColor(240, 201, 83, 43) : IColor(235, 232, 222, 192),
                       "DTJB_UI", EAlign::Near),
                 FitLabel(prefix + entry.name, rr.W() - 18.f, 11.f).c_str(), rr.GetPadded(-7.f));
      g.DrawLine(IColor(65, 95, 94, 84), rr.L + 4.f, rr.B, rr.R - 4.f, rr.B, nullptr, 0.6f);
    }
    if (static_cast<int>(mEntries.size()) > visibleRows) {
      const IRECT track = ScrollTrackRect(list);
      g.FillRoundRect(IColor(255, 35, 36, 34), track, 2.f);
      g.FillRoundRect(IColor(255, 116, 93, 72), ScrollThumbRect(list, visibleRows), 2.f);
    }
    if (mEntries.empty()) {
      const char* message = mPlugin.PresetRootAvailable()
        ? "No .dtjbpreset files in this folder" : "Preset directory is unavailable";
      g.DrawText(IText(12.f, IColor(180, 150, 157, 153), "DTJB_UI", EAlign::Center), message, list);
    }

    std::string status = mDeleteArmed
      ? "Delete selected preset? Click DELETE again to confirm."
      : mPlugin.mPresetStatusMessage;
    if (status.empty()) status = "Single-click selects. Double-click recalls. Folders can be opened.";
    const bool error = mDeleteArmed || mPlugin.mPresetStatusIsError;
    g.DrawText(IText(10.5f, error ? IColor(245, 235, 123, 92) : IColor(230, 201, 83, 43),
                     "DTJB_UI", EAlign::Center),
               FitLabel(status, StatusRect(panel).W() - 10.f, 10.5f).c_str(), StatusRect(panel));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override {
    const IRECT header = HeaderBar();
    if (header.Contains(x, y)) {
      if (PlusRect(header).Contains(x, y)) {
        mDeleteArmed = false; mPlugin.ClearPresetDeleteIdentity();
        if (mPlugin.mPresetRootDirectory.empty()) mPlugin.PromptSetPresetDirectory();
        return;
      }
      if (MinusRect(header).Contains(x, y)) {
        mDeleteArmed = false; mPlugin.ClearPresetDeleteIdentity();
        if (!mPlugin.mPresetRootDirectory.empty()) {
          mOpen = false; mPlugin.mPresetBrowserOpen = false; mPlugin.RemovePresetDirectory();
        }
        return;
      }
      if (SaveRect(header).Contains(x, y)) {
        mDeleteArmed = false; mPlugin.ClearPresetDeleteIdentity();
        if (mPlugin.PresetRootAvailable()) mPlugin.PromptSavePreset();
        return;
      }
      if (DeleteRect(header).Contains(x, y)) {
        if (!mPlugin.PresetRootAvailable() || mPlugin.mPresetSelectedPath.empty()) return;
        if (mDeleteArmed) {
          mDeleteArmed = false; mPlugin.DeleteSelectedPresetFile(); RefreshEntries();
        } else if (mPlugin.ArmSelectedPresetDelete()) {
          mDeleteArmed = true; SetDirty(false);
        }
        return;
      }
      if (FieldRect(header).Contains(x, y) && mPlugin.PresetRootAvailable()) {
        mDeleteArmed = false; mPlugin.ClearPresetDeleteIdentity();
        mOpen = !mOpen; mPlugin.mPresetBrowserOpen = mOpen;
        mPlugin.mPresetDirectoryFingerprintValid = false;
        if (mOpen) RefreshEntries();
        SetDirty(false);
      }
      return;
    }

    if (!mOpen) return;
    const IRECT panel = PanelRect();
    if (CloseRect(panel).Contains(x, y) || !panel.Contains(x, y)) {
      mOpen = false; mPlugin.mPresetBrowserOpen = false; mDeleteArmed = false;
      mPlugin.ClearPresetDeleteIdentity(); SetDirty(false); return;
    }
    const IRECT list = ListRect(panel);
    const int visibleRows = std::max(1, static_cast<int>(list.H() / 27.f));
    if (static_cast<int>(mEntries.size()) > visibleRows && ScrollTrackRect(list).Contains(x, y)) {
      mDraggingScrollbar = true; SetScrollFromY(y, list, visibleRows); SetDirty(false); return;
    }
    const int idx = EntryAt(y, panel);
    if (idx < 0 || idx >= static_cast<int>(mEntries.size())) return;
    const Entry entry = mEntries[static_cast<std::size_t>(idx)];
    mDeleteArmed = false; mPlugin.ClearPresetDeleteIdentity();
    if (entry.isDirectory) {
      if (mPlugin.PresetPathIsInsideRoot(entry.path)) {
        mPlugin.mPresetCurrentDirectory = entry.path;
        mPlugin.mPresetSelectedPath.clear();
        mPlugin.mPresetStatusMessage.clear();
        mPlugin.mPresetStatusIsError = false;
        mPlugin.mPresetDirectoryFingerprintValid = false;
        mScroll = 0; RefreshEntries(); SetDirty(false);
      }
      return;
    }
    mPlugin.mPresetSelectedPath = entry.path;
    mPlugin.mPresetStatusMessage = "Selected: " + entry.name + "  |  Double-click to recall";
    mPlugin.mPresetStatusIsError = false;
    RefreshEntries(); SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override {
    if (!mOpen) return;
    const IRECT panel = PanelRect();
    if (!panel.Contains(x, y) || !ListRect(panel).Contains(x, y)) return;
    const int idx = EntryAt(y, panel);
    if (idx < 0 || idx >= static_cast<int>(mEntries.size())) return;
    const Entry entry = mEntries[static_cast<std::size_t>(idx)];
    if (entry.isDirectory || mPlugin.mPresetSelectedPath.empty()) return;
    if (!JazzBeatPathsReferToSameLocation(JazzBeatPathFromUTF8(entry.path), JazzBeatPathFromUTF8(mPlugin.mPresetSelectedPath))) return;
    mDeleteArmed = false;
    if (mPlugin.LoadPresetFile(entry.path)) { mOpen = false; mPlugin.mPresetBrowserOpen = false; }
    RefreshEntries(); SetDirty(false);
  }

  void OnMouseDrag(float, float y, float, float, const IMouseMod&) override {
    if (!mOpen || !mDraggingScrollbar) return;
    const IRECT list = ListRect(PanelRect());
    SetScrollFromY(y, list, std::max(1, static_cast<int>(list.H() / 27.f)));
    SetDirty(false);
  }
  void OnMouseUp(float, float, const IMouseMod&) override { mDraggingScrollbar = false; }
  void OnMouseWheel(float, float, const IMouseMod&, float d) override {
    if (!mOpen) return;
    mScroll = std::max(0, mScroll + (d > 0.f ? -3 : 3));
    SetDirty(false);
  }

private:
  struct Entry { std::string path; std::string name; bool isDirectory = false; };

  IRECT HeaderBar() const { return IRECT(315.f, 50.f, 825.f, 78.f); }
  static IRECT FieldRect(const IRECT& b)  { return IRECT(b.L + 2.f, b.T + 2.f, b.L + 244.f, b.B - 2.f); }
  static IRECT PlusRect(const IRECT& b)   { return IRECT(b.L + 248.f, b.T + 2.f, b.L + 280.f, b.B - 2.f); }
  static IRECT MinusRect(const IRECT& b)  { return IRECT(b.L + 284.f, b.T + 2.f, b.L + 316.f, b.B - 2.f); }
  static IRECT SaveRect(const IRECT& b)   { return IRECT(b.L + 320.f, b.T + 2.f, b.L + 390.f, b.B - 2.f); }
  static IRECT DeleteRect(const IRECT& b) { return IRECT(b.L + 394.f, b.T + 2.f, b.R - 2.f, b.B - 2.f); }
  IRECT PanelRect() const { return IRECT(80.f, 88.f, 1060.f, 485.f); }
  static IRECT CloseRect(const IRECT& p) { return IRECT(p.R - 39.f, p.T + 8.f, p.R - 10.f, p.T + 35.f); }
  static IRECT CrumbRect(const IRECT& p) { return IRECT(p.L + 12.f, p.T + 43.f, p.R - 12.f, p.T + 69.f); }
  static IRECT StatusRect(const IRECT& p) { return IRECT(p.L + 12.f, p.B - 34.f, p.R - 12.f, p.B - 8.f); }
  static IRECT ListRect(const IRECT& p) { return IRECT(p.L + 12.f, p.T + 77.f, p.R - 12.f, p.B - 42.f); }
  static IRECT ScrollTrackRect(const IRECT& list) { return IRECT(list.R - 10.f, list.T + 3.f, list.R - 3.f, list.B - 3.f); }

  IRECT ScrollThumbRect(const IRECT& list, int visibleRows) const {
    const IRECT track = ScrollTrackRect(list);
    const int total = static_cast<int>(mEntries.size());
    if (total <= visibleRows) return track;
    const float h = std::max(28.f, track.H() * static_cast<float>(visibleRows) / static_cast<float>(total));
    const int maxScroll = std::max(1, total - visibleRows);
    const float t = static_cast<float>(std::clamp(mScroll, 0, maxScroll)) / static_cast<float>(maxScroll);
    const float top = track.T + (track.H() - h) * t;
    return IRECT(track.L, top, track.R, top + h);
  }

  void SetScrollFromY(float y, const IRECT& list, int visibleRows) {
    const int total = static_cast<int>(mEntries.size());
    const int maxScroll = std::max(0, total - visibleRows);
    if (maxScroll <= 0) { mScroll = 0; return; }
    const IRECT track = ScrollTrackRect(list);
    const float h = std::max(28.f, track.H() * static_cast<float>(visibleRows) / static_cast<float>(total));
    const float travel = std::max(1.f, track.H() - h);
    const float t = std::clamp((y - track.T - h * 0.5f) / travel, 0.f, 1.f);
    mScroll = std::clamp(static_cast<int>(std::lround(t * static_cast<float>(maxScroll))), 0, maxScroll);
  }

  static std::string FitLabel(const std::string& text, float widthPx, float fontPx) {
    const int maxChars = std::max(1, static_cast<int>(std::floor(std::max(1.f, widthPx) / std::max(4.2f, fontPx * 0.56f))));
    if (static_cast<int>(text.size()) <= maxChars) return text;
    if (maxChars <= 3) return std::string(static_cast<std::size_t>(maxChars), '.');
    return text.substr(0, static_cast<std::size_t>(maxChars - 3)) + "...";
  }

  static void DrawButton(IGraphics& g, const IRECT& r, const char* text, bool armed, bool disabled) {
    const IColor fill = disabled ? IColor(255, 30, 31, 30) : armed ? IColor(255, 116, 52, 36) : IColor(255, 47, 49, 47);
    const IColor frame = disabled ? IColor(255, 62, 62, 56) : armed ? IColor(255, 232, 222, 192) : IColor(220, 112, 102, 86);
    const IColor ink = disabled ? IColor(125, 150, 157, 153) : IColor(245, 232, 222, 192);
    g.FillRoundRect(fill, r, 2.5f);
    g.DrawRoundRect(frame, r, 2.5f, nullptr, 0.8f);
    g.DrawText(IText(9.3f, ink, "DTJB_UI_BOLD", EAlign::Center), text, r);
  }

  void DrawHeader(IGraphics& g) {
    const IRECT bar = HeaderBar();
    const bool hasRoot = !mPlugin.mPresetRootDirectory.empty();
    const bool available = mPlugin.PresetRootAvailable();
    const bool selected = available && !mPlugin.mPresetSelectedPath.empty();
    g.FillRoundRect(IColor(255, 22, 24, 23), bar, 4.f);
    g.DrawRoundRect(IColor(220, 92, 89, 78), bar, 4.f, nullptr, 0.8f);
    std::string label;
    IColor color(230, 190, 181, 156);
    if (!hasRoot) label = "PRESETS: choose folder (+)";
    else if (!available) { label = "Preset folder unavailable"; color = IColor(240, 230, 108, 108); }
    else if (!mPlugin.mPresetStatusMessage.empty()) {
      label = mPlugin.mPresetStatusMessage;
      color = mPlugin.mPresetStatusIsError ? IColor(240, 230, 108, 108) : IColor(235, 201, 83, 43);
    } else if (selected) label = JazzBeatPathToUTF8(JazzBeatPathFromUTF8(mPlugin.mPresetSelectedPath).stem());
    else label = "Select preset...";
    const IRECT field = FieldRect(bar);
    g.FillRoundRect(IColor(255, 17, 19, 19), field, 2.5f);
    g.DrawRoundRect(IColor(115, 76, 75, 68), field, 2.5f, nullptr, 0.7f);
    g.DrawText(IText(9.5f, color, "DTJB_UI", EAlign::Near),
               FitLabel(label, field.W() - 10.f, 9.5f).c_str(), field.GetPadded(-5.f));
    DrawButton(g, PlusRect(bar), "+", false, hasRoot);
    DrawButton(g, MinusRect(bar), "-", false, !hasRoot);
    DrawButton(g, SaveRect(bar), "SAVE", false, !available);
    DrawButton(g, DeleteRect(bar), mDeleteArmed ? "OK" : "DELETE", mDeleteArmed, !selected);
  }

  std::string CurrentCrumb() const {
    namespace fs = std::filesystem;
    if (!mPlugin.PresetRootAvailable()) return "";
    const fs::path root = JazzBeatPathFromUTF8(mPlugin.mPresetRootDirectory);
    const fs::path current = JazzBeatPathFromUTF8(mPlugin.mPresetCurrentDirectory.empty()
      ? mPlugin.mPresetRootDirectory : mPlugin.mPresetCurrentDirectory);
    std::error_code ec;
    const fs::path relative = fs::relative(current, root, ec);
    const std::string rootName = JazzBeatPathToUTF8(root.filename());
    if (ec || relative.empty() || relative == ".") return rootName;
    return rootName + " / " + JazzBeatPathToUTF8(relative);
  }

  int EntryAt(float y, const IRECT& panel) const {
    const IRECT list = ListRect(panel);
    if (y < list.T || y > list.B) return -1;
    return mScroll + static_cast<int>((y - list.T) / 27.f);
  }

  void RefreshEntries() {
    namespace fs = std::filesystem;
    mEntries.clear(); mClipped = false; mListingIncomplete = false;
    if (!mPlugin.PresetRootAvailable()) { mScroll = 0; return; }
    const fs::path root = JazzBeatPathFromUTF8(mPlugin.mPresetRootDirectory);
    fs::path current = mPlugin.mPresetCurrentDirectory.empty() ? root : JazzBeatPathFromUTF8(mPlugin.mPresetCurrentDirectory);
    std::error_code ec;
    if (!JazzBeatStablePathIsSameOrBelow(current, root)
        || !fs::exists(current, ec) || ec || !fs::is_directory(current, ec) || ec) current = root;
    mPlugin.mPresetCurrentDirectory = JazzBeatPathToUTF8(current);
    if (!JazzBeatPathsReferToSameLocation(current, root)) {
      fs::path parent = current.parent_path();
      if (!JazzBeatStablePathIsSameOrBelow(parent, root)) parent = root;
      mEntries.push_back({JazzBeatPathToUTF8(parent), "..", true});
    }
    std::vector<Entry> dirs, presets;
    int relevant = 0;
    ec.clear();
    for (fs::directory_iterator it(current, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
      const fs::directory_entry& de = *it;
      std::error_code te;
      Entry entry; bool keep = false;
      if (de.is_directory(te) && !te) {
        if (!JazzBeatPathEntryIsLinkOrReparse(de.path()) && JazzBeatStablePathIsSameOrBelow(de.path(), root)) {
          entry = {JazzBeatPathToUTF8(de.path()), JazzBeatPathToUTF8(de.path().filename()), true}; keep = true;
        }
      } else {
        te.clear();
        const std::string pathText = JazzBeatPathToUTF8(de.path());
        if (de.is_regular_file(te) && !te && JazzBeatIsPresetPath(pathText)
            && !JazzBeatPathEntryIsLinkOrReparse(de.path()) && JazzBeatStablePathIsSameOrBelow(de.path(), root)) {
          entry = {pathText, JazzBeatPathToUTF8(de.path().stem()), false}; keep = true;
        }
      }
      if (!keep) continue;
      if (relevant++ >= DerTondehrJazzBeat::kPresetMaxEntriesPerFolder) { mClipped = true; break; }
      (entry.isDirectory ? dirs : presets).push_back(std::move(entry));
    }
    if (ec) {
      mListingIncomplete = true;
      mPlugin.mPresetStatusMessage = "Folder listing incomplete";
      mPlugin.mPresetStatusIsError = true;
    }
    const auto byName = [](const Entry& a, const Entry& b) { return JazzBeatLowerASCII(a.name) < JazzBeatLowerASCII(b.name); };
    std::sort(dirs.begin(), dirs.end(), byName); std::sort(presets.begin(), presets.end(), byName);
    mEntries.insert(mEntries.end(), dirs.begin(), dirs.end());
    mEntries.insert(mEntries.end(), presets.begin(), presets.end());
    mScroll = std::clamp(mScroll, 0, std::max(0, static_cast<int>(mEntries.size()) - 1));
  }

  DerTondehrJazzBeat& mPlugin;
  std::vector<Entry> mEntries;
  unsigned int mLastRevision = std::numeric_limits<unsigned int>::max();
  int mScroll = 0;
  bool mOpen = false;
  bool mDeleteArmed = false;
  bool mClipped = false;
  bool mListingIncomplete = false;
  bool mDraggingScrollbar = false;
};
#endif


#if defined(VST3_API)
  #if !defined(IPLUG_EDITOR) || (IPLUG_EDITOR != 1)
    #error "Der Tondehr Jazz Beat VST3 must be compiled with IPLUG_EDITOR=1"
  #endif
  static_assert(PLUG_HAS_UI == 1, "VST3 custom UI must be enabled");
  static_assert(PLUG_WIDTH == 1140, "Unexpected VST3 editor width");
  static_assert(PLUG_HEIGHT == 510, "Unexpected VST3 editor height");
#endif

DerTondehrJazzBeat::DerTondehrJazzBeat(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kInputMode)->InitEnum("Input", 0, {"High", "Low"});
  GetParam(kInputTrim)->InitDouble("Input Trim", 0.0, -24.0, 24.0, 0.1, "dB");
  GetParam(kDistortionOn)->InitBool("Distortion", false);
  GetParam(kDistortion)->InitDouble("Distortion Amount", 0.0, 0.0, 100.0, 0.1, "%");
  GetParam(kBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.01, "");
  GetParam(kMiddle)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.01, "");
  GetParam(kTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.01, "");
  // ID 7 used to be the public boolean Hi-Treble switch. Preserve its type
  // and ID for VST3 compatibility, but do not expose it in the custom UI.
  GetParam(kHiTrebleLegacy)->InitBool("Hi-Treble (Legacy)", false, "", IParam::kFlagCannotAutomate);
  GetParam(kVolume)->InitDouble("Volume", 5.0, 0.0, 10.0, 0.01, "");
  GetParam(kReverb)->InitDouble("Reverb", 0.0, 0.0, 10.0, 0.01, "");
  GetParam(kChorusMode)->InitEnum("Chorus", 0, {"Off", "Fixed", "Manual"});
  GetParam(kChorusRate)->InitDouble("Chorus Rate", 35.0, 0.0, 100.0, 0.1, "%");
  GetParam(kChorusDepth)->InitDouble("Chorus Depth", 62.0, 0.0, 100.0, 0.1, "%");
  GetParam(kOversampling)->InitEnum("Oversampling", 0, {"x1", "x2", "x4"});
  GetParam(kOutputTrim)->InitDouble("Output Trim", -6.0, -24.0, 12.0, 0.1, "dB");
  // The real continuous VR3 control gets a fresh VST3 ParamID instead of
  // changing the type of the old boolean parameter in-place.
  GetParam(kHiTreble)->InitDouble("Hi-Treble", 0.0, 0.0, 10.0, 0.01, "");

  LoadPresetDirectoryPreference();
#if defined(APP_API)
  LoadStandaloneAudioPreferences();
#endif

#if IPLUG_EDITOR
  mInitialUIScale = GetInitialUIScaleForActiveMonitor();
  SetEditorSize(static_cast<int>(std::lround(static_cast<float>(PLUG_WIDTH) * mInitialUIScale)),
                static_cast<int>(std::lround(static_cast<float>(PLUG_HEIGHT) * mInitialUIScale)));
#endif
}

void DerTondehrJazzBeat::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  (void) sampleOffset;

  // Migration path for sessions saved with <= 0.1.13. The old boolean lived
  // at VST3 ParamID 7. If a host restores that legacy value, map OFF/ON to
  // 0/10 on the new continuous VR3 parameter. Newer sessions also restore the
  // fresh continuous ParamID afterwards, so their exact value wins.
  if(paramIdx == kHiTrebleLegacy &&
     (source == kHost || source == kPresetRecall))
  {
    GetParam(kHiTreble)->SetNormalized(GetParam(kHiTrebleLegacy)->Bool() ? 1.0 : 0.0);
  }
}

#if IPLUG_EDITOR
IGraphics* DerTondehrJazzBeat::CreateGraphics()
{
  return MakeGraphics(*this,
                      PLUG_WIDTH,
                      PLUG_HEIGHT,
                      PLUG_FPS,
                      mInitialUIScale);
}

void DerTondehrJazzBeat::LayoutUI(IGraphics* graphics)
{
  // APP and VST3 call the same UI builder from the common source list.
  dtjb::ui::BuildJazzBeatUI(graphics);
#if defined(APP_API)
  // Standalone only: Preferences chooses the physical stereo input pair and
  // MONO IN selects which member feeds Jazz Beat's mono front-end.
  graphics->AttachControl(new StandaloneMonoInputControl(IRECT(24.f, 18.f, 132.f, 78.f), *this),
                          kStandaloneMonoInputTag);
#endif
  // Preset manager is attached last so its modal browser always renders and
  // receives mouse input above the amplifier controls.
  graphics->AttachControl(new JazzBeatPresetManagerControl(graphics->GetBounds(), *this), kPresetManagerTag);
}
#endif

std::string DerTondehrJazzBeat::PresetPreferencesPath() const
{
  return JazzBeatPathToUTF8(JazzBeatUserConfigDirectory() / "preset_directory.txt");
}

bool DerTondehrJazzBeat::SavePresetDirectoryPreferenceValue(const std::string& root) const
{
  std::vector<std::uint8_t> bytes(root.begin(), root.end());
  bytes.push_back(static_cast<std::uint8_t>('\n'));
  return JazzBeatWriteBinaryAtomically(JazzBeatPathFromUTF8(PresetPreferencesPath()), bytes);
}

void DerTondehrJazzBeat::LoadPresetDirectoryPreference()
{
  mPresetRootDirectory.clear();
  mPresetCurrentDirectory.clear();
  mPresetSelectedPath.clear();
  mPresetStatusMessage.clear();
  mPresetStatusIsError = false;

  std::ifstream file(JazzBeatPathFromUTF8(PresetPreferencesPath()), std::ios::binary);
  if (!file) return;
  std::string line;
  std::getline(file, line);
  if (line.empty()) return;

  namespace fs = std::filesystem;
  fs::path p = JazzBeatPathFromUTF8(line).lexically_normal();
  std::error_code ec;
  const bool available = fs::exists(p, ec) && !ec && fs::is_directory(p, ec) && !ec;
  if (JazzBeatPathHasAnyLinkOrReparse(p)) {
    mPresetRootDirectory = JazzBeatPathToUTF8(p);
    mPresetCurrentDirectory = mPresetRootDirectory;
    mPresetStatusMessage = "Preset directory contains a link/junction and is blocked";
    mPresetStatusIsError = true;
    return;
  }
  const fs::path stored = available ? JazzBeatCanonicalForBoundary(p) : p;
  mPresetRootDirectory = JazzBeatPathToUTF8(stored);
  mPresetCurrentDirectory = mPresetRootDirectory;
  if (!available) {
    mPresetStatusMessage = "Preset directory is unavailable";
    mPresetStatusIsError = true;
  }
}

bool DerTondehrJazzBeat::PresetRootAvailable() const
{
  if (mPresetRootDirectory.empty()) return false;
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path root = JazzBeatPathFromUTF8(mPresetRootDirectory);
  return fs::exists(root, ec) && !ec && fs::is_directory(root, ec) && !ec
      && !JazzBeatPathHasAnyLinkOrReparse(root);
}

bool DerTondehrJazzBeat::PresetPathIsInsideRoot(const std::string& path) const
{
  if (mPresetRootDirectory.empty() || path.empty()) return false;
  return JazzBeatStablePathIsSameOrBelow(JazzBeatPathFromUTF8(path), JazzBeatPathFromUTF8(mPresetRootDirectory));
}

bool DerTondehrJazzBeat::ComputePresetFileIdentity(const std::string& path, std::uint64_t& identity) const
{
  namespace fs = std::filesystem;
  identity = 0u;
  const fs::path p = JazzBeatPathFromUTF8(path);
  std::error_code ec;
  if (!fs::exists(p, ec) || ec || !fs::is_regular_file(p, ec) || ec || JazzBeatPathEntryIsLinkOrReparse(p)) return false;

  constexpr std::uint64_t kOffset = 1469598103934665603ull;
  constexpr std::uint64_t kPrime = 1099511628211ull;
  std::uint64_t h = kOffset;
  const auto add = [&](const void* data, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) { h ^= static_cast<std::uint64_t>(b[i]); h *= kPrime; }
  };
  const std::string canonical = JazzBeatPathToUTF8(JazzBeatCanonicalForBoundary(p));
  add(canonical.data(), canonical.size());
  const auto size = fs::file_size(p, ec); if (ec) return false; add(&size, sizeof(size));
  const auto time = fs::last_write_time(p, ec); if (ec) return false;
  const auto ticks = time.time_since_epoch().count(); add(&ticks, sizeof(ticks));
#if defined(_WIN32)
  HANDLE handle = ::CreateFileW(p.wstring().c_str(), FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  BY_HANDLE_FILE_INFORMATION info {};
  const BOOL ok = ::GetFileInformationByHandle(handle, &info);
  ::CloseHandle(handle);
  if (!ok || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;
  add(&info.dwVolumeSerialNumber, sizeof(info.dwVolumeSerialNumber));
  add(&info.nFileIndexHigh, sizeof(info.nFileIndexHigh));
  add(&info.nFileIndexLow, sizeof(info.nFileIndexLow));
#endif
  identity = h;
  return true;
}

void DerTondehrJazzBeat::NotifyHostCustomPresetStateChanged()
{
  DirtyParametersFromUI();
  SendCurrentParamValuesFromDelegate();
}

bool DerTondehrJazzBeat::SavePresetFile(const std::string& path)
{
  namespace fs = std::filesystem;
  mPresetStatusMessage.clear();
  mPresetStatusIsError = false;
  if (!PresetRootAvailable()) {
    mPresetStatusMessage = "No preset directory configured";
    mPresetStatusIsError = true;
    return false;
  }

  fs::path destination = JazzBeatPathFromUTF8(path).lexically_normal();
  if (destination.extension().empty()) destination += kPresetExtension;
  else if (!JazzBeatIsPresetPath(JazzBeatPathToUTF8(destination))) destination.replace_extension(kPresetExtension);
  const std::string destinationText = JazzBeatPathToUTF8(destination);
  if (!PresetPathIsInsideRoot(destinationText)) {
    mPresetStatusMessage = "Preset must be saved inside the configured directory";
    mPresetStatusIsError = true;
    return false;
  }
  const fs::path parent = destination.parent_path();
  if (parent.empty() || !JazzBeatStablePathIsSameOrBelow(parent, JazzBeatPathFromUTF8(mPresetRootDirectory))
      || !JazzBeatDirectoryWritable(parent)) {
    mPresetStatusMessage = "Preset folder is not writable or is unsafe";
    mPresetStatusIsError = true;
    return false;
  }

  std::error_code ec;
  const bool exists = fs::exists(destination, ec) && !ec;
  if (exists) {
    if (!fs::is_regular_file(destination, ec) || ec || JazzBeatPathEntryIsLinkOrReparse(destination)) {
      mPresetStatusMessage = "Preset destination is not a regular file";
      mPresetStatusIsError = true;
      mPresetPendingOverwriteValid = false;
      return false;
    }
    std::uint64_t identity = 0u;
    if (!ComputePresetFileIdentity(destinationText, identity)) {
      mPresetStatusMessage = "Could not verify existing preset before overwrite";
      mPresetStatusIsError = true;
      mPresetPendingOverwriteValid = false;
      return false;
    }
    const bool same = mPresetPendingOverwriteValid
      && JazzBeatPathsReferToSameLocation(JazzBeatPathFromUTF8(mPresetPendingOverwritePath), destination);
    if (!same || identity != mPresetPendingOverwriteIdentity) {
      mPresetPendingOverwritePath = destinationText;
      mPresetPendingOverwriteIdentity = identity;
      mPresetPendingOverwriteValid = true;
      mPresetStatusMessage = "Preset exists - choose the same file and press SAVE again";
      mPresetStatusIsError = true;
#if IPLUG_EDITOR
      MarkPresetUIChanged();
#endif
      return false;
    }
  } else {
    mPresetPendingOverwritePath.clear();
    mPresetPendingOverwriteIdentity = 0u;
    mPresetPendingOverwriteValid = false;
  }

  std::vector<dtjb::preset::Entry> entries;
  entries.reserve(dtjb::preset::kParamSpecs.size());
  for (const auto& spec : dtjb::preset::kParamSpecs) {
    const double value = GetParam(spec.paramIdx)->Value();
    if (!dtjb::preset::ValueValid(spec, value)) {
      mPresetStatusMessage = "A control contains an invalid value; preset was not saved";
      mPresetStatusIsError = true;
      return false;
    }
    entries.push_back({spec.stableId, spec.type, value});
  }
  std::vector<std::uint8_t> bytes;
  if (!dtjb::preset::BuildFile(entries, bytes)) {
    mPresetStatusMessage = "Could not build a valid .dtjbpreset file";
    mPresetStatusIsError = true;
    return false;
  }

  if (!PresetPathIsInsideRoot(destinationText)
      || !JazzBeatStablePathIsSameOrBelow(parent, JazzBeatPathFromUTF8(mPresetRootDirectory))) {
    mPresetStatusMessage = "Preset path became unsafe before save";
    mPresetStatusIsError = true;
    return false;
  }
  if (exists) {
    std::uint64_t identity = 0u;
    if (!ComputePresetFileIdentity(destinationText, identity) || identity != mPresetPendingOverwriteIdentity) {
      mPresetStatusMessage = "Preset changed before overwrite - confirmation cancelled";
      mPresetStatusIsError = true;
      mPresetPendingOverwriteValid = false;
      return false;
    }
  }
  if (!JazzBeatWriteBinaryAtomically(destination, bytes)) {
    mPresetStatusMessage = "Could not write preset";
    mPresetStatusIsError = true;
    return false;
  }

  mPresetPendingOverwritePath.clear();
  mPresetPendingOverwriteIdentity = 0u;
  mPresetPendingOverwriteValid = false;
  mPresetSelectedPath = destinationText;
  mPresetCurrentDirectory = JazzBeatPathToUTF8(destination.parent_path());
  mPresetStatusMessage = "Saved: " + JazzBeatPathToUTF8(destination.stem());
  mPresetStatusIsError = false;
#if IPLUG_EDITOR
  ClearPresetDeleteIdentity();
  MarkPresetUIChanged();
#endif
  return true;
}

bool DerTondehrJazzBeat::LoadPresetFile(const std::string& path)
{
  namespace fs = std::filesystem;
  mPresetStatusMessage.clear();
  mPresetStatusIsError = false;
  const fs::path presetPath = JazzBeatPathFromUTF8(path).lexically_normal();
  const std::string presetPathText = JazzBeatPathToUTF8(presetPath);
  if (!PresetRootAvailable() || !PresetPathIsInsideRoot(presetPathText) || !JazzBeatIsPresetPath(presetPathText)) {
    mPresetStatusMessage = "Only valid .dtjbpreset files inside the configured directory can be loaded";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  std::error_code ec;
  if (!fs::exists(presetPath, ec) || ec || !fs::is_regular_file(presetPath, ec) || ec
      || JazzBeatPathEntryIsLinkOrReparse(presetPath)) {
    mPresetStatusMessage = "Preset is not a stable regular file";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  std::uint64_t initialIdentity = 0u;
  if (!ComputePresetFileIdentity(presetPathText, initialIdentity)) {
    mPresetStatusMessage = "Could not verify preset before reading";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  const auto fileSize = fs::file_size(presetPath, ec);
  if (ec || fileSize < dtjb::preset::kHeaderBytes || fileSize > dtjb::preset::kMaxFileBytes) {
    mPresetStatusMessage = "Preset is corrupted or invalid";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  std::ifstream in(presetPath, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
  if (!in || (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))) {
    mPresetStatusMessage = "Could not read preset";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  std::uint64_t finalIdentity = 0u;
  if (!PresetPathIsInsideRoot(presetPathText) || JazzBeatPathEntryIsLinkOrReparse(presetPath)
      || !ComputePresetFileIdentity(presetPathText, finalIdentity) || finalIdentity != initialIdentity) {
    mPresetStatusMessage = "Preset changed or became unsafe while reading";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }

  std::vector<dtjb::preset::Entry> entries;
  dtjb::preset::ParseError parseError {};
  if (!dtjb::preset::ParseFile(bytes, entries, parseError)) {
    mPresetStatusMessage = dtjb::preset::ParseErrorText(parseError);
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  if (entries.size() != dtjb::preset::kParamSpecs.size()) {
    mPresetStatusMessage = "Preset control set does not match Der Tondehr Jazz Beat";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }

  std::array<double, dtjb::preset::kParamSpecs.size()> validatedValues {};
  std::array<bool, dtjb::preset::kParamSpecs.size()> seen {};
  for (const auto& entry : entries) {
    const dtjb::preset::ParamSpec* spec = dtjb::preset::FindSpec(entry.id);
    if (!spec || spec->type != entry.type || !dtjb::preset::ValueValid(*spec, entry.value)) {
      mPresetStatusMessage = "Preset contains an unknown control, wrong control type, or invalid value";
      mPresetStatusIsError = true;
#if IPLUG_EDITOR
      MarkPresetUIChanged();
#endif
      return false;
    }
    const auto it = std::find_if(dtjb::preset::kParamSpecs.begin(), dtjb::preset::kParamSpecs.end(),
      [&](const dtjb::preset::ParamSpec& candidate) { return candidate.stableId == entry.id; });
    const std::size_t index = static_cast<std::size_t>(std::distance(dtjb::preset::kParamSpecs.begin(), it));
    if (index >= seen.size() || seen[index]) {
      mPresetStatusMessage = "Preset contains duplicate or invalid control IDs";
      mPresetStatusIsError = true;
#if IPLUG_EDITOR
      MarkPresetUIChanged();
#endif
      return false;
    }
    seen[index] = true;
    validatedValues[index] = entry.value;
  }
  if (std::find(seen.begin(), seen.end(), false) != seen.end()) {
    mPresetStatusMessage = "Preset is missing one or more Jazz Beat controls";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }

#if IPLUG_DSP
  mPresetRecallInProgress.store(true, std::memory_order_release);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
  while (mPresetAudioBlocksInFlight.load(std::memory_order_acquire) > 0) {
    if (std::chrono::steady_clock::now() >= deadline) {
      mPresetRecallInProgress.store(false, std::memory_order_release);
      mPresetStatusMessage = "Audio engine is busy - preset recall cancelled";
      mPresetStatusIsError = true;
#if IPLUG_EDITOR
      MarkPresetUIChanged();
#endif
      return false;
    }
    std::this_thread::yield();
  }
#endif

  for (std::size_t i = 0; i < dtjb::preset::kParamSpecs.size(); ++i)
    GetParam(dtjb::preset::kParamSpecs[i].paramIdx)->Set(validatedValues[i]);

#if IPLUG_DSP
  mSignalChain.setParameters(ReadParameters());
  mSignalChain.reset();
  mWasTransportRunning = false;
  mHavePreviousBlockPosition = false;
  mTransportPositionTrackingArmed = false;
  mForceFadeIn = true;
  mDeClickRemaining = 0;
  mDeClickLength = 0;
  mDeClickStartLeft = 0.0;
  mDeClickStartRight = 0.0;
  mLastOutputLeft = 0.0;
  mLastOutputRight = 0.0;
  mPresetRecallInProgress.store(false, std::memory_order_release);
#endif
  NotifyHostCustomPresetStateChanged();
  mPresetSelectedPath = presetPathText;
  mPresetCurrentDirectory = JazzBeatPathToUTF8(presetPath.parent_path());
  mPresetStatusMessage = "Loaded: " + JazzBeatPathToUTF8(presetPath.stem());
  mPresetStatusIsError = false;
#if IPLUG_EDITOR
  ClearPresetDeleteIdentity();
  MarkPresetUIChanged();
#endif
  return true;
}

bool DerTondehrJazzBeat::DeleteSelectedPresetFile()
{
  namespace fs = std::filesystem;
  if (!PresetRootAvailable() || mPresetSelectedPath.empty() || !PresetPathIsInsideRoot(mPresetSelectedPath)
      || !JazzBeatIsPresetPath(mPresetSelectedPath) || !mPresetDeleteIdentityValid) {
    mPresetStatusMessage = "Select and confirm a preset to delete";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  const fs::path target = JazzBeatPathFromUTF8(mPresetSelectedPath);
  std::uint64_t identity = 0u;
  if (!ComputePresetFileIdentity(mPresetSelectedPath, identity) || identity != mPresetDeleteIdentity
      || !PresetPathIsInsideRoot(mPresetSelectedPath) || JazzBeatPathEntryIsLinkOrReparse(target)) {
    mPresetStatusMessage = "Preset changed or became unsafe after confirmation";
    mPresetStatusIsError = true;
    mPresetDeleteIdentityValid = false;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  const std::string name = JazzBeatPathToUTF8(target.stem());
  std::error_code ec;
  fs::remove(target, ec);
  if (ec) {
    mPresetStatusMessage = "Could not delete preset";
    mPresetStatusIsError = true;
#if IPLUG_EDITOR
    MarkPresetUIChanged();
#endif
    return false;
  }
  mPresetSelectedPath.clear();
  mPresetDeleteIdentityValid = false;
  mPresetStatusMessage = "Deleted: " + name;
  mPresetStatusIsError = false;
#if IPLUG_EDITOR
  MarkPresetUIChanged();
#endif
  return true;
}

#if IPLUG_EDITOR
bool DerTondehrJazzBeat::ArmSelectedPresetDelete()
{
  mPresetDeleteIdentity = 0u;
  mPresetDeleteIdentityValid = false;
  if (!PresetRootAvailable() || mPresetSelectedPath.empty()
      || !PresetPathIsInsideRoot(mPresetSelectedPath) || !JazzBeatIsPresetPath(mPresetSelectedPath)) return false;
  std::uint64_t identity = 0u;
  if (!ComputePresetFileIdentity(mPresetSelectedPath, identity)) {
    mPresetStatusMessage = "Could not verify selected preset for deletion";
    mPresetStatusIsError = true;
    MarkPresetUIChanged();
    return false;
  }
  mPresetDeleteIdentity = identity;
  mPresetDeleteIdentityValid = true;
  return true;
}

void DerTondehrJazzBeat::ClearPresetDeleteIdentity()
{
  mPresetDeleteIdentity = 0u;
  mPresetDeleteIdentityValid = false;
}

void DerTondehrJazzBeat::MarkPresetUIChanged()
{
  mPresetUIRevision.fetch_add(1u, std::memory_order_relaxed);
  mPresetDirectoryFingerprintValid = false;
  if (auto* ui = GetUI())
    if (auto* control = ui->GetControlWithTag(kPresetManagerTag)) control->SetDirty(false);
}

void DerTondehrJazzBeat::PromptSetPresetDirectory()
{
  if (!mPresetRootDirectory.empty()) return;
  auto* ui = GetUI(); if (!ui) return;
  WDL_String directory;
  ui->PromptForDirectory(directory);
  if (directory.GetLength() == 0) return;
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path chosen = fs::u8path(directory.Get());
  if (!fs::exists(chosen, ec) || ec || !fs::is_directory(chosen, ec) || ec) {
    mPresetStatusMessage = "Could not open preset directory"; mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  if (JazzBeatPathHasAnyLinkOrReparse(chosen)) {
    mPresetStatusMessage = "Preset directory cannot contain symbolic links or junctions";
    mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  chosen = JazzBeatCanonicalForBoundary(chosen);
  if (!JazzBeatDirectoryWritable(chosen)) {
    mPresetStatusMessage = "Preset directory is not writable"; mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  const std::string candidate = JazzBeatPathToUTF8(chosen);
  if (!SavePresetDirectoryPreferenceValue(candidate)) {
    mPresetStatusMessage = "Could not save preset directory preference";
    mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  mPresetRootDirectory = candidate;
  mPresetCurrentDirectory = candidate;
  mPresetSelectedPath.clear();
  mPresetPendingOverwriteValid = false;
  ClearPresetDeleteIdentity();
  mPresetStatusMessage = "Preset directory configured";
  mPresetStatusIsError = false;
  MarkPresetUIChanged();
}

void DerTondehrJazzBeat::RemovePresetDirectory()
{
  if (mPresetRootDirectory.empty()) return;
  if (!SavePresetDirectoryPreferenceValue(std::string{})) {
    mPresetStatusMessage = "Could not clear preset directory preference";
    mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  mPresetRootDirectory.clear(); mPresetCurrentDirectory.clear(); mPresetSelectedPath.clear();
  mPresetPendingOverwritePath.clear(); mPresetPendingOverwriteValid = false;
  ClearPresetDeleteIdentity(); mPresetStatusMessage.clear(); mPresetStatusIsError = false;
  MarkPresetUIChanged();
}

void DerTondehrJazzBeat::PromptSavePreset()
{
  if (!PresetRootAvailable()) {
    mPresetStatusMessage = mPresetRootDirectory.empty() ? "No preset directory configured" : "Preset directory is unavailable";
    mPresetStatusIsError = true; MarkPresetUIChanged(); return;
  }
  namespace fs = std::filesystem;
  const fs::path root = JazzBeatPathFromUTF8(mPresetRootDirectory);
  fs::path current = mPresetCurrentDirectory.empty() ? root : JazzBeatPathFromUTF8(mPresetCurrentDirectory);
  if (!JazzBeatStablePathIsSameOrBelow(current, root)) current = root;
  std::error_code ec;
  if (!fs::exists(current, ec) || ec || !fs::is_directory(current, ec) || ec) current = root;
  auto* ui = GetUI(); if (!ui) return;
  WDL_String fileName; fileName.Set("Der Tondehr Jazz Beat Preset.dtjbpreset");
  WDL_String pathText; const std::string currentUtf8 = JazzBeatPathToUTF8(current); pathText.Set(currentUtf8.c_str());
  ui->PromptForFile(fileName, pathText, EFileAction::Save, "dtjbpreset");
  if (fileName.GetLength() == 0) return;
  fs::path destination = fs::u8path(fileName.Get());
  if (!destination.is_absolute()) destination = fs::u8path(pathText.GetLength() > 0 ? pathText.Get() : currentUtf8.c_str()) / destination;
  if (destination.extension().empty()) destination += kPresetExtension;
  else if (!JazzBeatIsPresetPath(JazzBeatPathToUTF8(destination))) destination.replace_extension(kPresetExtension);
  SavePresetFile(JazzBeatPathToUTF8(destination.lexically_normal()));
}

std::uint64_t DerTondehrJazzBeat::ComputeCurrentPresetDirectoryFingerprint() const
{
  namespace fs = std::filesystem;
  constexpr std::uint64_t kOffset = 1469598103934665603ull, kPrime = 1099511628211ull;
  std::uint64_t hash = kOffset;
  const auto add = [&](const std::string& text) {
    for (const unsigned char c : text) { hash ^= static_cast<std::uint64_t>(c); hash *= kPrime; }
    hash ^= 0xffu; hash *= kPrime;
  };
  if (!PresetRootAvailable()) return hash;
  const fs::path root = JazzBeatPathFromUTF8(mPresetRootDirectory);
  fs::path current = mPresetCurrentDirectory.empty() ? root : JazzBeatPathFromUTF8(mPresetCurrentDirectory);
  if (!JazzBeatStablePathIsSameOrBelow(current, root)) current = root;
  add(JazzBeatPathToUTF8(JazzBeatCanonicalForBoundary(current)));
  std::vector<std::string> records;
  std::error_code ec;
  for (fs::directory_iterator it(current, fs::directory_options::skip_permission_denied, ec), end;
       it != end && !ec; it.increment(ec)) {
    const fs::directory_entry& de = *it;
    std::error_code te;
    const bool dir = de.is_directory(te) && !te && !JazzBeatPathEntryIsLinkOrReparse(de.path())
      && JazzBeatStablePathIsSameOrBelow(de.path(), root);
    te.clear();
    const std::string pathText = JazzBeatPathToUTF8(de.path());
    const bool preset = de.is_regular_file(te) && !te && JazzBeatIsPresetPath(pathText)
      && !JazzBeatPathEntryIsLinkOrReparse(de.path()) && JazzBeatStablePathIsSameOrBelow(de.path(), root);
    if (!dir && !preset) continue;
    std::string record = dir ? "D:" : "P:";
    record += JazzBeatPathToUTF8(de.path().filename());
    if (preset) {
      std::error_code me;
      const auto sz = de.file_size(me); record += me ? ":size-error" : (":" + std::to_string(sz));
      me.clear(); const auto t = de.last_write_time(me); record += me ? ":time-error" : (":" + std::to_string(t.time_since_epoch().count()));
    }
    records.push_back(std::move(record));
  }
  std::sort(records.begin(), records.end());
  for (const auto& record : records) add(record);
  if (ec) add("<listing-error:" + std::to_string(ec.value()) + ">");
  add(std::to_string(records.size()));
  return hash;
}

void DerTondehrJazzBeat::PollPresetDirectoryChanges()
{
  if (!GetUI() || !mPresetBrowserOpen) return;
  using namespace std::chrono;
  const auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
  if (mPresetLastDirectoryPollMs != 0 && now - mPresetLastDirectoryPollMs < 1000) return;
  mPresetLastDirectoryPollMs = now;
  if (!mPresetRootDirectory.empty() && !PresetRootAvailable()) {
    mPresetStatusMessage = JazzBeatPathHasAnyLinkOrReparse(JazzBeatPathFromUTF8(mPresetRootDirectory))
      ? "Preset directory contains a link/junction and is blocked" : "Preset directory is unavailable";
    mPresetStatusIsError = true; mPresetCurrentDirectory = mPresetRootDirectory; mPresetSelectedPath.clear();
    ClearPresetDeleteIdentity(); mPresetDirectoryFingerprintValid = false; mPresetUIRevision.fetch_add(1u, std::memory_order_relaxed);
    if (auto* c = GetUI()->GetControlWithTag(kPresetManagerTag)) c->SetDirty(false);
    return;
  }
  if (!PresetRootAvailable()) return;
  const std::uint64_t fingerprint = ComputeCurrentPresetDirectoryFingerprint();
  if (!mPresetDirectoryFingerprintValid) {
    mPresetDirectoryFingerprint = fingerprint; mPresetDirectoryFingerprintValid = true;
  } else if (fingerprint != mPresetDirectoryFingerprint) {
    mPresetDirectoryFingerprint = fingerprint; ClearPresetDeleteIdentity(); mPresetPendingOverwriteValid = false;
    mPresetUIRevision.fetch_add(1u, std::memory_order_relaxed);
    if (auto* c = GetUI()->GetControlWithTag(kPresetManagerTag)) c->SetDirty(false);
  }
}
#endif


#if IPLUG_DSP
dtjb::dsp::Parameters DerTondehrJazzBeat::ReadParameters()
{
  dtjb::dsp::Parameters parameters;
  parameters.lowInput = GetParam(kInputMode)->Int() == 1;
  parameters.inputTrimDb = GetParam(kInputTrim)->Value();
  parameters.distortionEnabled = GetParam(kDistortionOn)->Bool();
  parameters.distortion = GetParam(kDistortion)->Value() * 0.01;
  parameters.bass = GetParam(kBass)->Value() * 0.1;
  parameters.middle = GetParam(kMiddle)->Value() * 0.1;
  parameters.treble = GetParam(kTreble)->Value() * 0.1;
  parameters.hiTreble = GetParam(kHiTreble)->Value() * 0.1;
  parameters.volume = GetParam(kVolume)->Value() * 0.1;
  parameters.reverb = GetParam(kReverb)->Value() * 0.1;
  parameters.chorusMode = static_cast<dtjb::dsp::BBDChorus::Mode>(GetParam(kChorusMode)->Int());
  parameters.chorusRate = GetParam(kChorusRate)->Value() * 0.01;
  parameters.chorusDepth = GetParam(kChorusDepth)->Value() * 0.01;
  parameters.oversampling = 1 << GetParam(kOversampling)->Int();
  parameters.outputTrimDb = GetParam(kOutputTrim)->Value();
  return parameters;
}

#if defined(APP_API)
namespace {
std::filesystem::path JazzBeatStandaloneAudioPrefsPath()
{
  namespace fs = std::filesystem;
  const char* base = std::getenv("LOCALAPPDATA");
  if (!base || !*base)
    base = std::getenv("APPDATA");

  std::error_code ec;
  fs::path root = (base && *base) ? fs::u8path(base) : fs::temp_directory_path(ec);
  if (root.empty()) root = fs::path(".");
  fs::path directory = root / "Der Tondehr" / "DerTondehrJazzBeat";
  fs::create_directories(directory, ec);
  return directory / "standalone_audio.txt";
}
}

void DerTondehrJazzBeat::LoadStandaloneAudioPreferences()
{
  int selected = 0;
  std::ifstream file(JazzBeatStandaloneAudioPrefsPath(), std::ios::binary);
  if (file) {
    std::string line;
    std::getline(file, line);
    try { selected = std::clamp(std::stoi(line), 0, 1); }
    catch (...) { selected = 0; }
  }
  mStandaloneMonoInputTarget.store(selected, std::memory_order_relaxed);
  mStandaloneMonoInputBlend = static_cast<double>(selected);
}

void DerTondehrJazzBeat::SaveStandaloneAudioPreferences() const
{
  const int selected = StandaloneMonoInput();
  const std::filesystem::path path = JazzBeatStandaloneAudioPrefsPath();
  const std::filesystem::path temp = path.string() + ".tmp";
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file) return;
    file << selected << "\n";
    file.flush();
    if (!file) return;
  }
  std::error_code ec;
  std::filesystem::remove(path, ec);
  ec.clear();
  std::filesystem::rename(temp, path, ec);
  if (ec)
    std::filesystem::remove(temp, ec);
}

void DerTondehrJazzBeat::SetStandaloneMonoInput(int inputIndex)
{
  mStandaloneMonoInputTarget.store(std::clamp(inputIndex, 0, 1), std::memory_order_release);
  SaveStandaloneAudioPreferences();
}

int DerTondehrJazzBeat::StandaloneMonoInput() const
{
  return std::clamp(mStandaloneMonoInputTarget.load(std::memory_order_acquire), 0, 1);
}
#endif

void DerTondehrJazzBeat::OnReset()
{
  mSignalChain.prepare(GetSampleRate());

  // VST3 hosts may stop/restart processing at transport boundaries. Start the
  // next processed block with a very short fade rather than exposing a hard
  // discontinuity from zeroed filter/reverb states to an arbitrary waveform
  // sample. Timeline seeks that occur while already playing are detected in
  // ProcessBlock from GetSamplePos().
  mWasTransportRunning = false;
  mHavePreviousBlockPosition = false;
  mTransportPositionTrackingArmed = false;
  mForceFadeIn = true;
  mDeClickRemaining = 0;
  mDeClickLength = 0;
  mDeClickStartLeft = 0.0;
  mDeClickStartRight = 0.0;
  mLastOutputLeft = 0.0;
  mLastOutputRight = 0.0;
#if defined(APP_API)
  mStandaloneMonoInputBlend = static_cast<double>(StandaloneMonoInput());
#endif
}

void DerTondehrJazzBeat::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
#if defined(_MSC_VER)
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

  const int outputChannels = NOutChansConnected();
  const auto silence = [&]() {
    for (int ch = 0; ch < outputChannels; ++ch)
      if (outputs[ch]) std::fill(outputs[ch], outputs[ch] + nFrames, static_cast<sample>(0));
  };
  if (mPresetRecallInProgress.load(std::memory_order_acquire)) { silence(); return; }
  mPresetAudioBlocksInFlight.fetch_add(1, std::memory_order_acq_rel);
  if (mPresetRecallInProgress.load(std::memory_order_acquire)) {
    mPresetAudioBlocksInFlight.fetch_sub(1, std::memory_order_acq_rel);
    silence();
    return;
  }

  const dtjb::dsp::Parameters parameters = ReadParameters();
  mSignalChain.setParameters(parameters);

  const bool transportRunning = GetTransportIsRunning();
  const double blockSamplePos = GetSamplePos();
  const bool positionIsFinite = std::isfinite(blockSamplePos);

  bool transportDiscontinuity = false;
  bool preservePreviousOutput = false;

  // A transition from stopped to playing is a known discontinuity. On the
  // first activation (or after OnReset), also fade in even if the host does not
  // provide useful transport state.
  if(mForceFadeIn || (transportRunning && !mWasTransportRunning))
  {
    transportDiscontinuity = true;
    preservePreviousOutput = false;
    mForceFadeIn = false;
  }

  // Once we have observed normal contiguous VST3 sample-position progression,
  // arm seek detection. This avoids false positives in hosts/standalone modes
  // that leave sample position fixed at zero.
  if(transportRunning && mWasTransportRunning && positionIsFinite && mHavePreviousBlockPosition)
  {
    const double expected = mPreviousBlockSamplePos + static_cast<double>(mPreviousBlockFrames);
    const double error = std::abs(blockSamplePos - expected);
    const double tolerance = 2.0;

    if(mTransportPositionTrackingArmed)
    {
      if(error > tolerance)
      {
        transportDiscontinuity = true;
        preservePreviousOutput = true;
      }
    }
    else if(error <= tolerance && mPreviousBlockFrames > 0)
    {
      mTransportPositionTrackingArmed = true;
    }
  }

  if(transportDiscontinuity)
  {
    const double heldLeft = preservePreviousOutput ? mLastOutputLeft : 0.0;
    const double heldRight = preservePreviousOutput ? mLastOutputRight : 0.0;

    // Clear the history-dependent circuit states at the new timeline location.
    // Parameter smoothers keep their current targets, so this does not create
    // a control-value jump.
    mSignalChain.reset();

    mDeClickStartLeft = heldLeft;
    mDeClickStartRight = heldRight;
    mDeClickLength = std::max(16, static_cast<int>(std::lround(GetSampleRate() * 0.005)));
    mDeClickRemaining = mDeClickLength;
  }

  const int inputChannels = NInChansConnected();
#if defined(APP_API)
  const double inputRate = std::max(8000.0, GetSampleRate());
  const double monoInputSlew = 1.0 - std::exp(-1.0 / (0.008 * inputRate));
  const double monoTarget = static_cast<double>(StandaloneMonoInput());
#endif
  for(int frame = 0; frame < nFrames; ++frame)
  {
#if defined(APP_API)
    const double in0 = (inputChannels > 0 && inputs && inputs[0] && std::isfinite(inputs[0][frame]))
      ? static_cast<double>(inputs[0][frame]) : 0.0;
    const double in1 = (inputChannels > 1 && inputs && inputs[1] && std::isfinite(inputs[1][frame]))
      ? static_cast<double>(inputs[1][frame]) : in0;
    mStandaloneMonoInputBlend += (monoTarget - mStandaloneMonoInputBlend) * monoInputSlew;
    const double monoIn = in0 + (in1 - in0) * std::clamp(mStandaloneMonoInputBlend, 0.0, 1.0);
#else
    const double monoIn = (inputChannels > 0 && inputs && inputs[0])
      ? static_cast<double>(inputs[0][frame]) : 0.0;
#endif
    auto out = mSignalChain.process(monoIn);

    if(mDeClickRemaining > 0 && mDeClickLength > 0)
    {
      const int completed = mDeClickLength - mDeClickRemaining;
      const double phase = static_cast<double>(completed + 1) / static_cast<double>(mDeClickLength);
      const double mix = 0.5 - 0.5 * std::cos(dtjb::dsp::kPi * dtjb::dsp::clamp(phase, 0.0, 1.0));
      out.left = mDeClickStartLeft + (out.left - mDeClickStartLeft) * mix;
      out.right = mDeClickStartRight + (out.right - mDeClickStartRight) * mix;
      --mDeClickRemaining;
    }

    mLastOutputLeft = out.left;
    mLastOutputRight = out.right;

    if(outputChannels > 0) outputs[0][frame] = static_cast<sample>(out.left);
    if(outputChannels > 1) outputs[1][frame] = static_cast<sample>(out.right);
  }

  if(transportRunning && positionIsFinite)
  {
    mPreviousBlockSamplePos = blockSamplePos;
    mPreviousBlockFrames = nFrames;
    mHavePreviousBlockPosition = true;
  }
  else if(!transportRunning)
  {
    mHavePreviousBlockPosition = false;
    mTransportPositionTrackingArmed = false;
  }

  mWasTransportRunning = transportRunning;
  mPresetAudioBlocksInFlight.fetch_sub(1, std::memory_order_acq_rel);
}
#endif
