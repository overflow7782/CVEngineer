#pragma once

#include "ClipboardHistoryService.h"
#include "RenderContext.h"

#include <windows.h>

#include <memory>
#include <vector>

class AppWindow final {
public:
    explicit AppWindow(HINSTANCE instance);
    ~AppWindow();

    bool Create();
    void Show(int command = SW_SHOWNORMAL) const;
    [[nodiscard]] HWND Handle() const { return window_; }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass() const;
    void ApplyBackdrop() const;
    void ToggleExpanded();
    void ResizeForState();
    void Paint();
    void DrawHeader(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawCollapsed(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawExpanded(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawStatus(const D2D1_SIZE_F& size, std::wstring_view message);
    void OnMouseWheel(short delta);
    [[nodiscard]] float Scale(float logicalPixels) const;
    [[nodiscard]] bool IsHeaderButton(POINT point) const;

    HINSTANCE instance_{};
    HWND window_{};
    UINT dpi_{96};
    bool expanded_{false};
    float scrollOffset_{0.0F};
    RenderContext renderer_;
    std::shared_ptr<ClipboardHistoryService> historyService_;
};
