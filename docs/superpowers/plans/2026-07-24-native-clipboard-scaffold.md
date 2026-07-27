# Native Clipboard Scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dependency-free native Windows scaffold that displays Windows clipboard history in collapsed and expanded floating-window states.

**Architecture:** A C++20 Win32 process owns a topmost borderless window. `ClipboardHistoryService` uses C++/WinRT `Windows.ApplicationModel.DataTransfer.Clipboard` APIs as the only history source, projects at most 25 system items into lightweight view models, and notifies the UI through a private window message. Direct2D and DirectWrite render the shell without a browser or UI framework.

**Tech Stack:** C++20, Win32, C++/WinRT from Windows SDK 10.0.26100, Direct2D, DirectWrite, DWM, MSBuild/VCXPROJ.

**Repository constraints:** Do not create commits or perform other Git operations. Do not retain temporary test programs in the repository; verification is compilation plus a manual smoke run of the native executable.

---

## File Structure

- `FloatNote.vcxproj`: MSBuild project and Windows SDK link dependencies.
- `app.manifest`: DPI awareness and supported Windows declarations.
- `src/main.cpp`: COM/WinRT initialization and application entry point.
- `src/ClipboardRecord.h`: UI-facing history record and service-state types.
- `src/ContentClassifier.h/.cpp`: deterministic JSON, Markdown, link, code, image, file, and text classification.
- `src/ClipboardHistoryService.h/.cpp`: Windows history query, history-change subscription, copy, delete, and clear operations.
- `src/RenderContext.h/.cpp`: Direct2D/DirectWrite resource ownership and drawing helpers.
- `src/AppWindow.h/.cpp`: window lifecycle, topmost/glass setup, collapse/expand state, scrolling, hit testing, and history rendering.
- `README.md`: prerequisites, build command, Windows history requirement, and current scaffold scope.

## Task 1: Align The Design Contract

**Files:**
- Modify: `docs/superpowers/specs/2026-07-23-floating-clipboard-design.md`

- [ ] Replace the self-managed session history with `Clipboard::GetHistoryItemsAsync()` and `Clipboard::HistoryChanged`.
- [ ] Remove local payload caching, DPAPI storage, record pinning, and app-owned eviction rules.
- [ ] Specify that process restart discards all projections and startup re-reads the current Windows history.
- [ ] Specify that copy, delete, and clear call `SetHistoryItemAsContent`, `DeleteItemFromHistory`, and `ClearHistory`.
- [ ] Keep the collapsed/current preview and expanded/25-item interaction contract.

## Task 2: Create The Native Build Scaffold

**Files:**
- Create: `FloatNote.vcxproj`
- Create: `app.manifest`
- Create: `src/main.cpp`

- [ ] Configure an x64 Unicode Windows application using C++20 and SDK `10.0.26100.0`.
- [ ] Link only system libraries: `windowsapp`, `d2d1`, `dwrite`, `dwmapi`, `windowscodecs`, `comctl32`, and `shcore`.
- [ ] Initialize C++/WinRT with `winrt::init_apartment(winrt::apartment_type::multi_threaded)`.
- [ ] Create `AppWindow`, run the standard Win32 message loop, and uninitialize cleanly.

Verification command:

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && msbuild FloatNote.vcxproj /p:Configuration=Debug /p:Platform=x64 /m'
```

Expected result: project reaches compilation; missing source files are the only failures until later tasks are complete.

## Task 3: Add Deterministic Content Classification

**Files:**
- Create: `src/ClipboardRecord.h`
- Create: `src/ContentClassifier.h`
- Create: `src/ContentClassifier.cpp`

- [ ] Define `ContentType`, `ClipboardRecord`, and `ClipboardHistoryState` without owning clipboard payloads.
- [ ] Classify native storage items and bitmaps before text.
- [ ] Parse JSON with `Windows::Data::Json::JsonValue::TryParse`; only object and array roots are JSON.
- [ ] Treat an absolute HTTP/HTTPS URI occupying the complete text as a link.
- [ ] Detect Markdown only from ATX headings, fenced code blocks, quote blocks, or two consecutive list items.
- [ ] Use `DataPackageView` HTML `<pre>` or `<code>` presence for generic code; otherwise fall back to text.
- [ ] Generate a bounded preview and metadata without persisting the original content.

Expected API:

```cpp
class ContentClassifier final {
public:
    static ContentType ClassifyText(std::wstring_view text);
    static std::wstring BuildPreview(std::wstring_view text, std::size_t maxCharacters = 420);
    static std::wstring BuildTextMetadata(ContentType type, std::wstring_view text);
};
```

## Task 4: Read And Operate On Windows Clipboard History

**Files:**
- Create: `src/ClipboardHistoryService.h`
- Create: `src/ClipboardHistoryService.cpp`

- [ ] Subscribe to `Clipboard::HistoryChanged` and trigger an initial refresh.
- [ ] Await `Clipboard::GetHistoryItemsAsync()` off the UI call stack.
- [ ] Handle `Success`, `AccessDenied`, and `ClipboardHistoryDisabled` as explicit service states.
- [ ] Project at most 25 `ClipboardHistoryItem` objects and retain their WinRT handles only for the process lifetime.
- [ ] Read text asynchronously; represent image/file items using format metadata without decoding payloads in the scaffold.
- [ ] Swap the snapshot under a mutex and notify `AppWindow` using `WM_APP + 1`.
- [ ] Implement direct system operations:

```cpp
SetHistoryItemAsContentStatus Activate(std::wstring_view id) const;
bool Delete(std::wstring_view id);
bool Clear();
```

- [ ] Requery Windows after mutating an item instead of changing a second app-owned history list.

## Task 5: Build The Floating Window And Renderer

**Files:**
- Create: `src/RenderContext.h`
- Create: `src/RenderContext.cpp`
- Create: `src/AppWindow.h`
- Create: `src/AppWindow.cpp`

- [ ] Create a borderless `WS_EX_TOPMOST | WS_EX_TOOLWINDOW` window and restore it inside the current monitor work area.
- [ ] Apply rounded corners and `DWMSBT_TRANSIENTWINDOW` through documented DWM attributes when available.
- [ ] Render the cold-white glass shell, header, listening state, current record, type badge, preview, metadata, and item count.
- [ ] Start collapsed at approximately `340 x 136` logical pixels.
- [ ] Toggle expanded state from the header/body and resize to approximately `380 x 640` logical pixels.
- [ ] In expanded state render a virtualized visible slice from the system-provided items and support mouse-wheel scrolling.
- [ ] Support dragging through `WM_NCHITTEST`, topmost toggling, refresh, collapse/expand, and close hit regions.
- [ ] Render disabled/access-denied guidance when Windows history cannot be read.

## Task 6: Document And Verify The Scaffold

**Files:**
- Create: `README.md`

- [ ] Document that no third-party dependency is required.
- [ ] Document the required Visual Studio Build Tools components: MSVC x64/x86 tools and Windows 11 SDK.
- [ ] Document that Windows clipboard history must be enabled with `Win + V`.
- [ ] Build Debug x64 and Release x64 with MSBuild.
- [ ] Launch the Release executable, verify one topmost window appears, then close it normally.
- [ ] Record the executable path and any remaining limitations in the handoff.

Final verification commands:

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && msbuild FloatNote.vcxproj /p:Configuration=Debug /p:Platform=x64 /m'
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && msbuild FloatNote.vcxproj /p:Configuration=Release /p:Platform=x64 /m'
```

Expected result: `Build succeeded` with zero compiler errors; executable at `build\x64\Release\FloatNote.exe`.
