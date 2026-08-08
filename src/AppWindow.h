#pragma once

#include "ClipboardHistoryService.h"
#include "RenderContext.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

enum class WindowState {
    CollapsedPreview,
    HistoryList,
    ItemDetail,
};

enum class HitTarget {
    None,
    HeaderToggle,
    HeaderClose,
    CollapsedBody,
    RecordBody,
    RecordCopy,
    DetailCollapse,
    DetailCopy,
    DetailPlainText,
    DetailDelete,
    ClearHistory,
};

struct HitResult {
    HitTarget target{HitTarget::None};
    std::wstring recordId;

    bool operator==(const HitResult&) const = default;
};

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
    void SetWindowState(WindowState state);
    void ToggleCollapsed();
    void ResizeForState();
    void StartStateTransition(bool resizeWindow);
    void UpdateStateTransition();
    void UpdateInteractionTransition();
    void Paint();
    void DrawHeader(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawCollapsed(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawHistory(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records);
    void DrawStatus(const D2D1_SIZE_F& size, std::wstring_view message);
    void DrawHeaderButton(
        const D2D1_RECT_F& bounds,
        std::wstring_view glyph,
        HitTarget target,
        bool enabled = true,
        bool active = false);
    void DrawActionButton(
        const D2D1_RECT_F& bounds,
        std::wstring_view glyph,
        std::wstring_view label,
        HitTarget target,
        std::wstring_view recordId,
        bool enabled = true,
        bool destructive = false,
        float opacity = 1.0F);
    void DrawTypeIcon(ContentType type, const D2D1_RECT_F& bounds);
    void DrawCard(const D2D1_ROUNDED_RECT& bounds, D2D1_COLOR_F fill, float elevation = 0.0F);
    void SetHover(HitResult hit);
    void UpdateHover(D2D1_POINT_2F point);
    [[nodiscard]] float HoverProgress(HitTarget target, std::wstring_view recordId = {}) const;
    [[nodiscard]] float RecordHoverProgress(std::wstring_view recordId) const;
    [[nodiscard]] HitResult HitTest(D2D1_POINT_2F point) const;
    void ExecuteHit(const HitResult& hit);
    void ActivateRecord(std::wstring_view id);
    void CopyRecordAsPlainText(std::wstring_view id);
    void DeleteSelectedRecord();
    void ClearHistory();
    void ShowOperationError(std::wstring_view message) const;
    void OnMouseWheel(short delta);
    [[nodiscard]] float RowHeight(const ClipboardRecord& record) const;
    [[nodiscard]] float ContentHeight(const std::vector<ClipboardRecord>& records) const;
    [[nodiscard]] D2D1_POINT_2F ToLogicalPoint(LPARAM lParam) const;
    [[nodiscard]] float Scale(float logicalPixels) const;
    [[nodiscard]] bool IsHeaderControl(D2D1_POINT_2F point) const;

    HINSTANCE instance_{};
    HWND window_{};
    UINT dpi_{96};
    WindowState state_{WindowState::CollapsedPreview};
    std::wstring selectedRecordId_;
    std::wstring transitioningRecordId_;
    bool detailClosing_{false};
    float scrollOffset_{0.0F};
    HitResult hover_;
    HitResult previousHover_;
    HitResult pressed_;
    bool trackingMouse_{false};
    ULONGLONG interactionStartMs_{};
    float interactionProgress_{1.0F};
    ULONGLONG transitionStartMs_{};
    float transitionProgress_{1.0F};
    bool resizeTransition_{false};
    SIZE transitionStartSize_{};
    SIZE transitionTargetSize_{};
    RenderContext renderer_;
    std::shared_ptr<ClipboardHistoryService> historyService_;
};
