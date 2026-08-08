#include "AppWindow.h"

#include <d2d1helper.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <algorithm>
#include <optional>
#include <string>

namespace {
constexpr wchar_t kWindowClassName[] = L"FloatNote.NativeWindow";
constexpr wchar_t kWindowTitle[] = L"浮贴";

constexpr D2D1_COLOR_F kText = {0.09F, 0.10F, 0.11F, 1.0F};
constexpr D2D1_COLOR_F kSecondaryText = {0.34F, 0.36F, 0.38F, 1.0F};
constexpr D2D1_COLOR_F kDisabledText = {0.60F, 0.62F, 0.64F, 0.74F};
constexpr D2D1_COLOR_F kAccent = {0.00F, 0.39F, 0.75F, 1.0F};
constexpr D2D1_COLOR_F kAccentSoft = {0.90F, 0.95F, 1.0F, 0.94F};
constexpr D2D1_COLOR_F kWindowSurface = {0.84F, 0.89F, 0.95F, 0.65F};
constexpr D2D1_COLOR_F kCard = {0.96F, 0.98F, 1.0F, 0.74F};
constexpr D2D1_COLOR_F kCardHover = {0.88F, 0.94F, 1.0F, 0.80F};
constexpr D2D1_COLOR_F kCardSelected = {0.84F, 0.92F, 1.0F, 0.84F};
constexpr D2D1_COLOR_F kBorderSubtle = {0.86F, 0.90F, 0.94F, 0.18F};
constexpr D2D1_COLOR_F kHeaderSeparator = {0.72F, 0.77F, 0.82F, 0.24F};
constexpr D2D1_COLOR_F kDanger = {0.83F, 0.24F, 0.24F, 1.0F};
constexpr float kHeaderHeight = 58.0F;
constexpr float kListTop = 62.0F;
constexpr float kFooterHeight = 42.0F;
constexpr float kCompactRowHeight = 92.0F;
constexpr float kDetailRowHeight = 310.0F;
constexpr UINT_PTR kTransitionTimer = 1;
constexpr UINT_PTR kInteractionTimer = 2;
constexpr ULONGLONG kTransitionDurationMs = 160;
constexpr ULONGLONG kInteractionDurationMs = 180;

D2D1_ROUNDED_RECT Rounded(float left, float top, float right, float bottom, float radius) {
    return D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), radius, radius);
}

bool Contains(const D2D1_RECT_F& bounds, D2D1_POINT_2F point) {
    return point.x >= bounds.left && point.x < bounds.right &&
        point.y >= bounds.top && point.y < bounds.bottom;
}

float EaseOutQuart(float value) {
    const float inverse = 1.0F - value;
    return 1.0F - inverse * inverse * inverse * inverse;
}

D2D1_COLOR_F Blend(D2D1_COLOR_F from, D2D1_COLOR_F to, float amount) {
    return {
        from.r + (to.r - from.r) * amount,
        from.g + (to.g - from.g) * amount,
        from.b + (to.b - from.b) * amount,
        from.a + (to.a - from.a) * amount,
    };
}

D2D1_COLOR_F WithOpacity(D2D1_COLOR_F color, float opacity) {
    color.a *= std::clamp(opacity, 0.0F, 1.0F);
    return color;
}

D2D1_COLOR_F TypeColor(ContentType type) {
    switch (type) {
    case ContentType::Image: return {0.15F, 0.54F, 0.31F, 1.0F};
    case ContentType::Files: return {0.82F, 0.27F, 0.28F, 1.0F};
    case ContentType::Link: return kAccent;
    case ContentType::Markdown: return {0.44F, 0.33F, 0.68F, 1.0F};
    case ContentType::Text: return {0.35F, 0.38F, 0.41F, 1.0F};
    default: return {0.36F, 0.33F, 0.66F, 1.0F};
    }
}

std::wstring_view TypeGlyph(ContentType type) {
    switch (type) {
    case ContentType::Image: return L"\xEB9F";
    case ContentType::Files: return L"\xE8A5";
    case ContentType::Link: return L"\xE71B";
    case ContentType::Text: return L"\xE8A5";
    default: return L"\xE943";
    }
}
} // namespace

AppWindow::AppWindow(HINSTANCE instance)
    : instance_(instance) {
}

AppWindow::~AppWindow() {
    if (historyService_) {
        historyService_->Stop();
    }
}

bool AppWindow::Create() {
    if (!RegisterWindowClass()) {
        return false;
    }

    const int width = static_cast<int>(Scale(340.0F));
    const int height = static_cast<int>(Scale(144.0F));
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    window_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kWindowClassName,
        kWindowTitle,
        WS_POPUP,
        workArea.right - width - 24,
        workArea.top + 24,
        width,
        height,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!window_) {
        return false;
    }

    dpi_ = GetDpiForWindow(window_);
    if (!renderer_.Initialize(window_)) {
        DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }
    renderer_.SetDpi(static_cast<float>(dpi_));

    ApplyBackdrop();
    historyService_ = std::make_shared<ClipboardHistoryService>(window_);
    historyService_->Start();
    return true;
}

void AppWindow::Show(int command) const {
    ShowWindow(window_, command);
    UpdateWindow(window_);
}

LRESULT CALLBACK AppWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    return self ? self->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        Paint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        renderer_.Resize(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(window_, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        renderer_.SetDpi(static_cast<float>(dpi_));
        const auto suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(
            window_, HWND_TOPMOST, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOACTIVATE);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window_, &point);
        const D2D1_POINT_2F logicalPoint{
            static_cast<float>(point.x) * 96.0F / static_cast<float>(dpi_),
            static_cast<float>(point.y) * 96.0F / static_cast<float>(dpi_)};
        if (logicalPoint.y >= 0.0F && logicalPoint.y < kHeaderHeight && !IsHeaderControl(logicalPoint)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            trackingMouse_ = true;
        }
        UpdateHover(ToLogicalPoint(lParam));
        return 0;
    }

    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        SetHover({});
        return 0;

    case WM_LBUTTONDOWN:
        pressed_ = HitTest(ToLogicalPoint(lParam));
        if (pressed_.target != HitTarget::None) {
            SetCapture(window_);
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        const auto released = HitTest(ToLogicalPoint(lParam));
        const auto pressed = pressed_;
        pressed_ = {};
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        InvalidateRect(window_, nullptr, FALSE);
        if (pressed.target != HitTarget::None && pressed == released) {
            ExecuteHit(released);
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (pressed_.target != HitTarget::None) {
            pressed_ = {};
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && hover_.target != HitTarget::None) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_MOUSEWHEEL:
        OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_TIMER:
        if (wParam == kTransitionTimer) {
            UpdateStateTransition();
            return 0;
        }
        if (wParam == kInteractionTimer) {
            UpdateInteractionTransition();
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && state_ == WindowState::ItemDetail) {
            SetWindowState(WindowState::HistoryList);
        } else if (wParam == VK_ESCAPE && state_ == WindowState::HistoryList) {
            SetWindowState(WindowState::CollapsedPreview);
        }
        return 0;

    case WM_FLOATNOTE_HISTORY_UPDATED: {
        if (state_ == WindowState::ItemDetail && historyService_) {
            const auto records = historyService_->Snapshot();
            const auto selected = std::find_if(records.begin(), records.end(), [this](const ClipboardRecord& record) {
                return record.id == selectedRecordId_;
            });
            if (selected == records.end()) {
                state_ = WindowState::HistoryList;
                selectedRecordId_.clear();
                transitioningRecordId_.clear();
                detailClosing_ = false;
            }
        }
        SetHover({});
        pressed_ = {};
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(window_, kTransitionTimer);
        KillTimer(window_, kInteractionTimer);
        if (historyService_) {
            historyService_->Stop();
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

bool AppWindow::RegisterWindowClass() const {
    WNDCLASSEXW windowClass{sizeof(WNDCLASSEXW)};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;

    const ATOM registered = RegisterClassExW(&windowClass);
    return registered != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void AppWindow::ApplyBackdrop() const {
    const MARGINS margins{-1};
    DwmExtendFrameIntoClientArea(window_, &margins);

    const auto corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(window_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    const auto backdrop = DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(window_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

void AppWindow::SetWindowState(WindowState state) {
    if (state_ == state) {
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    const WindowState previousState = state_;
    const bool wasCollapsed = previousState == WindowState::CollapsedPreview;
    const bool willBeCollapsed = state == WindowState::CollapsedPreview;
    const bool leavingDetail = previousState == WindowState::ItemDetail && state == WindowState::HistoryList;
    const bool enteringDetail = previousState == WindowState::HistoryList && state == WindowState::ItemDetail;
    if (leavingDetail) {
        transitioningRecordId_ = selectedRecordId_;
        detailClosing_ = true;
    } else if (enteringDetail) {
        transitioningRecordId_ = selectedRecordId_;
        detailClosing_ = false;
    } else if (state != WindowState::ItemDetail) {
        selectedRecordId_.clear();
        transitioningRecordId_.clear();
        detailClosing_ = false;
    }
    SetHover({});
    pressed_ = {};
    if (wasCollapsed || willBeCollapsed) {
        scrollOffset_ = 0.0F;
    }
    state_ = state;
    StartStateTransition(wasCollapsed != willBeCollapsed);
}

void AppWindow::ToggleCollapsed() {
    SetWindowState(
        state_ == WindowState::CollapsedPreview
            ? WindowState::HistoryList
            : WindowState::CollapsedPreview);
}

void AppWindow::ResizeForState() {
    RECT current{};
    GetWindowRect(window_, &current);
    const bool collapsed = state_ == WindowState::CollapsedPreview;
    const int width = static_cast<int>(Scale(collapsed ? 340.0F : 380.0F));
    const int height = static_cast<int>(Scale(collapsed ? 144.0F : 640.0F));
    SetWindowPos(
        window_, HWND_TOPMOST, current.left, current.top, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(window_, nullptr, FALSE);
}

void AppWindow::StartStateTransition(bool resizeWindow) {
    BOOL animationsEnabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
    if (!animationsEnabled) {
        transitionProgress_ = 1.0F;
        if (resizeWindow) {
            ResizeForState();
        } else {
            InvalidateRect(window_, nullptr, FALSE);
        }
        if (detailClosing_) {
            selectedRecordId_.clear();
            transitioningRecordId_.clear();
            detailClosing_ = false;
        }
        return;
    }

    RECT client{};
    GetClientRect(window_, &client);
    transitionStartSize_ = {client.right - client.left, client.bottom - client.top};
    const bool collapsed = state_ == WindowState::CollapsedPreview;
    transitionTargetSize_ = {
        static_cast<LONG>(Scale(collapsed ? 340.0F : 380.0F)),
        static_cast<LONG>(Scale(collapsed ? 144.0F : 640.0F))};
    resizeTransition_ = resizeWindow;
    transitionStartMs_ = GetTickCount64();
    transitionProgress_ = 0.0F;
    SetTimer(window_, kTransitionTimer, 15, nullptr);
    InvalidateRect(window_, nullptr, FALSE);
}

void AppWindow::UpdateStateTransition() {
    const ULONGLONG elapsed = GetTickCount64() - transitionStartMs_;
    const float linear = std::min(1.0F, static_cast<float>(elapsed) / static_cast<float>(kTransitionDurationMs));
    transitionProgress_ = EaseOutQuart(linear);

    if (resizeTransition_) {
        const int width = static_cast<int>(
            static_cast<float>(transitionStartSize_.cx) +
            static_cast<float>(transitionTargetSize_.cx - transitionStartSize_.cx) * transitionProgress_);
        const int height = static_cast<int>(
            static_cast<float>(transitionStartSize_.cy) +
            static_cast<float>(transitionTargetSize_.cy - transitionStartSize_.cy) * transitionProgress_);
        SetWindowPos(
            window_, HWND_TOPMOST, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        InvalidateRect(window_, nullptr, FALSE);
    }

    if (linear >= 1.0F) {
        KillTimer(window_, kTransitionTimer);
        resizeTransition_ = false;
        transitionProgress_ = 1.0F;
        if (detailClosing_) {
            selectedRecordId_.clear();
            transitioningRecordId_.clear();
            detailClosing_ = false;
        }
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void AppWindow::UpdateInteractionTransition() {
    const ULONGLONG elapsed = GetTickCount64() - interactionStartMs_;
    const float linear = std::min(
        1.0F,
        static_cast<float>(elapsed) / static_cast<float>(kInteractionDurationMs));
    interactionProgress_ = EaseOutQuart(linear);
    InvalidateRect(window_, nullptr, FALSE);
    if (linear >= 1.0F) {
        KillTimer(window_, kInteractionTimer);
        previousHover_ = {};
        interactionProgress_ = 1.0F;
    }
}

void AppWindow::Paint() {
    PAINTSTRUCT paint{};
    BeginPaint(window_, &paint);

    if (renderer_.BeginDraw()) {
        auto* target = renderer_.Target();
        target->Clear(kWindowSurface);
        const auto size = target->GetSize();
        const auto records = historyService_ ? historyService_->Snapshot() : std::vector<ClipboardRecord>{};

        DrawHeader(size, records);
        if (historyService_ && historyService_->State() != ClipboardHistoryState::Ready) {
            DrawStatus(size, historyService_->StatusMessage());
        } else if (state_ != WindowState::CollapsedPreview) {
            DrawHistory(size, records);
        } else {
            DrawCollapsed(size, records);
        }
        renderer_.EndDraw();
    }

    EndPaint(window_, &paint);
}

void AppWindow::DrawHeader(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    auto* target = renderer_.Target();
    target->DrawTextW(
        L"\xE77F", 1, renderer_.IconFormat(),
        D2D1::RectF(10.0F, 10.0F, 42.0F, 42.0F), renderer_.Brush(kAccent));
    target->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(38.0F, 39.0F), 3.0F, 3.0F), renderer_.Brush(kAccent));

    target->DrawTextW(
        L"浮贴", 2, renderer_.TitleFormat(),
        D2D1::RectF(50.0F, 8.0F, size.width - 166.0F, 30.0F),
        renderer_.Brush(kText));
    const std::wstring subtitle = std::to_wstring(records.size()) + L" / 25 条记录";
    target->DrawTextW(
        subtitle.c_str(), static_cast<UINT32>(subtitle.size()), renderer_.SmallFormat(),
        D2D1::RectF(50.0F, 30.0F, size.width - 166.0F, 48.0F),
        renderer_.Brush(kSecondaryText));

    DrawHeaderButton(D2D1::RectF(size.width - 154.0F, 13.0F, size.width - 126.0F, 41.0F), L"\xE718", HitTarget::None, true, true);
    DrawHeaderButton(D2D1::RectF(size.width - 124.0F, 13.0F, size.width - 96.0F, 41.0F), L"\xE890", HitTarget::None, false);
    DrawHeaderButton(D2D1::RectF(size.width - 94.0F, 13.0F, size.width - 66.0F, 41.0F), L"\xE713", HitTarget::None, false);
    DrawHeaderButton(
        D2D1::RectF(size.width - 64.0F, 13.0F, size.width - 36.0F, 41.0F),
        state_ == WindowState::CollapsedPreview ? L"\xE70D" : L"\xE70E",
        HitTarget::HeaderToggle);
    DrawHeaderButton(D2D1::RectF(size.width - 34.0F, 13.0F, size.width - 6.0F, 41.0F), L"\xE8BB", HitTarget::HeaderClose);

    target->DrawLine(
        D2D1::Point2F(8.0F, kHeaderHeight - 0.5F),
        D2D1::Point2F(size.width - 8.0F, kHeaderHeight - 0.5F),
        renderer_.Brush(kHeaderSeparator), 1.0F);
}

void AppWindow::DrawCollapsed(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    auto* target = renderer_.Target();

    if (records.empty()) {
        DrawCard(Rounded(8.0F, 62.0F, size.width - 8.0F, size.height - 8.0F, 7.0F), kCard);
        DrawStatus(size, L"等待 Windows 剪贴板历史");
        return;
    }

    const auto& current = records.front();
    const float hoverProgress = HoverProgress(HitTarget::CollapsedBody, current.id);
    const bool pressed = pressed_.target == HitTarget::CollapsedBody && pressed_.recordId == current.id;
    const float offset = -1.75F * hoverProgress + (pressed ? 0.8F : 0.0F);
    D2D1_MATRIX_3X2_F previousTransform{};
    target->GetTransform(&previousTransform);
    target->SetTransform(D2D1::Matrix3x2F::Translation(0.0F, offset) * previousTransform);

    DrawCard(
        Rounded(8.0F, 62.0F, size.width - 8.0F, size.height - 8.0F, 7.0F),
        Blend(kCard, kCardHover, hoverProgress),
        hoverProgress);
    DrawTypeIcon(current.type, D2D1::RectF(18.0F, 68.0F, 42.0F, 92.0F));
    target->DrawTextW(
        current.typeLabel.c_str(), static_cast<UINT32>(current.typeLabel.size()), renderer_.LabelFormat(),
        D2D1::RectF(46.0F, 68.0F, 110.0F, 92.0F), renderer_.Brush(TypeColor(current.type)));
    target->DrawTextW(
        current.metadataText.c_str(), static_cast<UINT32>(current.metadataText.size()), renderer_.SmallFormat(),
        D2D1::RectF(108.0F, 70.0F, size.width - 45.0F, 88.0F),
        renderer_.Brush(kSecondaryText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    target->DrawTextW(
        L"\xE70D", 1, renderer_.IconFormat(),
        D2D1::RectF(size.width - 43.0F, 70.0F, size.width - 15.0F, 100.0F),
        renderer_.Brush(WithOpacity(kAccent, hoverProgress)));
    target->DrawTextW(
        current.previewText.c_str(), static_cast<UINT32>(current.previewText.size()),
        current.type == ContentType::Json || current.type == ContentType::Code
            ? renderer_.CodeFormat() : renderer_.BodyFormat(),
        D2D1::RectF(18.0F, 94.0F, size.width - 18.0F, size.height - 12.0F),
        renderer_.Brush(kText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    target->SetTransform(previousTransform);
}

void AppWindow::DrawHistory(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    if (records.empty()) {
        DrawStatus(size, L"Windows 剪贴板历史为空");
        return;
    }

    auto* target = renderer_.Target();
    const float bottom = size.height - kFooterHeight;
    const float viewportHeight = bottom - kListTop;
    const float maxScroll = std::max(0.0F, ContentHeight(records) - viewportHeight);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maxScroll);

    target->PushAxisAlignedClip(
        D2D1::RectF(0.0F, kListTop, size.width, bottom),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float y = kListTop - scrollOffset_;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        const float rowHeight = RowHeight(record);
        if (y + rowHeight < kListTop || y > bottom) {
            y += rowHeight;
            continue;
        }

        const bool isOpeningDetail = state_ == WindowState::ItemDetail && record.id == selectedRecordId_;
        const bool isClosingDetail = detailClosing_ && record.id == transitioningRecordId_;
        const bool isDetail = isOpeningDetail || isClosingDetail;
        const float revealProgress = isClosingDetail ? 1.0F - transitionProgress_ : transitionProgress_;
        const float hoverProgress = RecordHoverProgress(record.id);
        const bool pressed = pressed_.recordId == record.id;
        const float rowOffset = -1.25F * hoverProgress + (pressed ? 0.6F : 0.0F);
        D2D1_MATRIX_3X2_F previousTransform{};
        target->GetTransform(&previousTransform);
        target->SetTransform(D2D1::Matrix3x2F::Translation(0.0F, rowOffset) * previousTransform);

        D2D1_COLOR_F fill = Blend(kCard, kCardHover, hoverProgress);
        if (isDetail) {
            fill = Blend(fill, kCardSelected, revealProgress * 0.58F);
        }
        DrawCard(
            Rounded(8.0F, y + 3.0F, size.width - 8.0F, y + rowHeight - 5.0F, 7.0F),
            fill,
            std::max(hoverProgress, isDetail ? 0.45F : 0.0F));

        if (index == 0) {
            target->FillRoundedRectangle(
                Rounded(8.0F, y + 10.0F, 11.0F, y + rowHeight - 12.0F, 1.5F),
                renderer_.Brush(kAccent));
        }

        DrawTypeIcon(record.type, D2D1::RectF(18.0F, y + 8.0F, 42.0F, y + 32.0F));
        target->DrawTextW(
            record.typeLabel.c_str(), static_cast<UINT32>(record.typeLabel.size()), renderer_.LabelFormat(),
            D2D1::RectF(46.0F, y + 8.0F, 128.0F, y + 32.0F), renderer_.Brush(TypeColor(record.type)));
        if (isDetail) {
            DrawActionButton(
                D2D1::RectF(size.width - 44.0F, y + 7.0F, size.width - 16.0F, y + 35.0F),
                L"\xE70E", L"", HitTarget::DetailCollapse, record.id);

            if (revealProgress > 0.16F) {
                const float contentOpacity = std::clamp((revealProgress - 0.16F) / 0.34F, 0.0F, 1.0F);
                const std::wstring& content = record.plainText ? *record.plainText : record.previewText;
                const bool codeSurface = record.type == ContentType::Json || record.type == ContentType::Code;
                const float revealOffset = 6.0F * (1.0F - contentOpacity);
                const auto contentBounds = Rounded(
                    17.0F,
                    y + 47.0F + revealOffset,
                    size.width - 17.0F,
                    y + rowHeight - 78.0F + revealOffset,
                    6.0F);
                const D2D1_COLOR_F contentFill = codeSurface
                    ? D2D1::ColorF(0.82F, 0.87F, 0.92F, 0.30F)
                    : D2D1::ColorF(0.96F, 0.98F, 1.0F, 0.28F);
                target->FillRoundedRectangle(
                    contentBounds,
                    renderer_.Brush(WithOpacity(contentFill, contentOpacity)));
                target->DrawTextW(
                    content.c_str(), static_cast<UINT32>(content.size()),
                    codeSurface
                        ? renderer_.CodeFormat() : renderer_.BodyFormat(),
                    D2D1::RectF(
                        27.0F,
                        y + 57.0F + revealOffset,
                        size.width - 27.0F,
                        y + rowHeight - 88.0F + revealOffset),
                    renderer_.Brush(WithOpacity(kText, contentOpacity)),
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
                target->DrawTextW(
                    record.metadataText.c_str(),
                    static_cast<UINT32>(record.metadataText.size()),
                    renderer_.SmallFormat(),
                    D2D1::RectF(
                        18.0F,
                        y + rowHeight - 72.0F + revealOffset,
                        size.width - 18.0F,
                        y + rowHeight - 55.0F + revealOffset),
                    renderer_.Brush(WithOpacity(kSecondaryText, contentOpacity)),
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
                DrawActionButton(
                    D2D1::RectF(
                        17.0F,
                        y + rowHeight - 50.0F + revealOffset,
                        91.0F,
                        y + rowHeight - 10.0F + revealOffset),
                    L"\xE8C8", L"复制", HitTarget::DetailCopy, record.id, true, false, contentOpacity);
                DrawActionButton(
                    D2D1::RectF(
                        99.0F,
                        y + rowHeight - 50.0F + revealOffset,
                        247.0F,
                        y + rowHeight - 10.0F + revealOffset),
                    L"\xE8A5", L"纯文本复制", HitTarget::DetailPlainText,
                    record.id, record.plainText.has_value(), false, contentOpacity);
                DrawActionButton(
                    D2D1::RectF(
                        size.width - 91.0F,
                        y + rowHeight - 50.0F + revealOffset,
                        size.width - 17.0F,
                        y + rowHeight - 10.0F + revealOffset),
                    L"\xE74D", L"删除", HitTarget::DetailDelete,
                    record.id, true, true, contentOpacity);
            }
        } else {
            DrawActionButton(
                D2D1::RectF(size.width - 44.0F, y + 7.0F, size.width - 16.0F, y + 35.0F),
                L"\xE8C8", L"", HitTarget::RecordCopy, record.id);
            target->DrawTextW(
                record.previewText.c_str(), static_cast<UINT32>(record.previewText.size()), renderer_.BodyFormat(),
                D2D1::RectF(18.0F, y + 36.0F, size.width - 50.0F, y + 65.0F),
                renderer_.Brush(kText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            target->DrawTextW(
                record.metadataText.c_str(), static_cast<UINT32>(record.metadataText.size()), renderer_.SmallFormat(),
                D2D1::RectF(18.0F, y + 68.0F, size.width - 50.0F, y + 85.0F),
                renderer_.Brush(kSecondaryText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        target->SetTransform(previousTransform);
        y += rowHeight;
    }
    target->PopAxisAlignedClip();

    const std::wstring footer = std::to_wstring(records.size()) + L" / 25 条记录";
    target->DrawLine(
        D2D1::Point2F(8.0F, size.height - kFooterHeight + 0.5F),
        D2D1::Point2F(size.width - 8.0F, size.height - kFooterHeight + 0.5F),
        renderer_.Brush(kBorderSubtle), 1.0F);
    target->DrawTextW(
        footer.c_str(), static_cast<UINT32>(footer.size()), renderer_.SmallFormat(),
        D2D1::RectF(14.0F, size.height - 29.0F, size.width - 96.0F, size.height - 9.0F),
        renderer_.Brush(kSecondaryText));
    DrawActionButton(
        D2D1::RectF(size.width - 83.0F, size.height - 36.0F, size.width - 12.0F, size.height - 6.0F),
        L"\xE74D", L"清空", HitTarget::ClearHistory, L"", true, true);
}

void AppWindow::DrawStatus(const D2D1_SIZE_F& size, std::wstring_view message) {
    auto* target = renderer_.Target();
    const std::wstring text{message};
    target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), renderer_.BodyFormat(),
        D2D1::RectF(18.0F, 68.0F, size.width - 18.0F, size.height - 16.0F),
        renderer_.Brush(kSecondaryText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void AppWindow::DrawHeaderButton(
    const D2D1_RECT_F& bounds,
    std::wstring_view glyph,
    HitTarget target,
    bool enabled,
    bool active) {
    auto* renderTarget = renderer_.Target();
    const float hoverProgress = enabled && target != HitTarget::None
        ? HoverProgress(target)
        : 0.0F;
    const bool pressed = enabled && pressed_.target == target;
    const std::wstring text{glyph};
    const D2D1_COLOR_F iconColor = active
        ? kAccent
        : Blend(kSecondaryText, kAccent, (hoverProgress + (pressed ? 0.35F : 0.0F)) * 0.55F);
    renderTarget->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), renderer_.IconFormat(), bounds,
        renderer_.Brush(enabled ? iconColor : kDisabledText));
}

void AppWindow::DrawActionButton(
    const D2D1_RECT_F& bounds,
    std::wstring_view glyph,
    std::wstring_view label,
    HitTarget target,
    std::wstring_view recordId,
    bool enabled,
    bool destructive,
    float opacity) {
    auto* renderTarget = renderer_.Target();
    const float hoverProgress = enabled ? HoverProgress(target, recordId) : 0.0F;
    const bool primary = target == HitTarget::DetailCopy || target == HitTarget::DetailPlainText;
    D2D1_COLOR_F color = !enabled
        ? kDisabledText
        : (destructive
            ? kDanger
            : (primary ? kAccent : Blend(kSecondaryText, kAccent, hoverProgress)));
    color.a *= opacity;
    const std::wstring glyphText{glyph};
    if (label.empty()) {
        renderTarget->DrawTextW(
            glyphText.c_str(), static_cast<UINT32>(glyphText.size()), renderer_.IconFormat(), bounds,
            renderer_.Brush(color));
        return;
    }

    const float iconRight = std::min(bounds.right, bounds.left + 31.0F);
    renderTarget->DrawTextW(
        glyphText.c_str(), static_cast<UINT32>(glyphText.size()), renderer_.IconFormat(),
        D2D1::RectF(bounds.left + 3.0F, bounds.top, iconRight, bounds.bottom), renderer_.Brush(color));
    const std::wstring labelText{label};
    renderTarget->DrawTextW(
        labelText.c_str(), static_cast<UINT32>(labelText.size()), renderer_.ButtonFormat(),
        D2D1::RectF(iconRight - 1.0F, bounds.top, bounds.right - 4.0F, bounds.bottom), renderer_.Brush(color));
}

void AppWindow::DrawTypeIcon(ContentType type, const D2D1_RECT_F& bounds) {
    auto* target = renderer_.Target();
    const auto color = TypeColor(type);
    const auto glyph = TypeGlyph(type);
    const std::wstring text{glyph};
    target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), renderer_.IconFormat(), bounds, renderer_.Brush(color));
}

void AppWindow::DrawCard(
    const D2D1_ROUNDED_RECT& bounds,
    D2D1_COLOR_F fill,
    float) {
    auto* target = renderer_.Target();
    target->FillRoundedRectangle(bounds, renderer_.Brush(fill));
}

void AppWindow::SetHover(HitResult hit) {
    if (hit == hover_) {
        return;
    }

    previousHover_ = hover_;
    hover_ = std::move(hit);
    interactionStartMs_ = GetTickCount64();
    interactionProgress_ = 0.0F;

    BOOL animationsEnabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
    if (!animationsEnabled) {
        KillTimer(window_, kInteractionTimer);
        previousHover_ = {};
        interactionProgress_ = 1.0F;
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    SetTimer(window_, kInteractionTimer, 15, nullptr);
    InvalidateRect(window_, nullptr, FALSE);
}

void AppWindow::UpdateHover(D2D1_POINT_2F point) {
    SetHover(HitTest(point));
}

float AppWindow::HoverProgress(HitTarget target, std::wstring_view recordId) const {
    const HitResult query{target, std::wstring{recordId}};
    float progress = 0.0F;
    if (query == hover_) {
        progress = interactionProgress_;
    }
    if (query == previousHover_) {
        progress = std::max(progress, 1.0F - interactionProgress_);
    }
    return progress;
}

float AppWindow::RecordHoverProgress(std::wstring_view recordId) const {
    float progress = 0.0F;
    if (!recordId.empty() && hover_.recordId == recordId) {
        progress = interactionProgress_;
    }
    if (!recordId.empty() && previousHover_.recordId == recordId) {
        progress = std::max(progress, 1.0F - interactionProgress_);
    }
    return progress;
}

HitResult AppWindow::HitTest(D2D1_POINT_2F point) const {
    RECT client{};
    GetClientRect(window_, &client);
    const float width = static_cast<float>(client.right - client.left) * 96.0F / static_cast<float>(dpi_);
    const float height = static_cast<float>(client.bottom - client.top) * 96.0F / static_cast<float>(dpi_);

    if (Contains(D2D1::RectF(width - 64.0F, 13.0F, width - 36.0F, 41.0F), point)) {
        return {HitTarget::HeaderToggle, {}};
    }
    if (Contains(D2D1::RectF(width - 34.0F, 13.0F, width - 6.0F, 41.0F), point)) {
        return {HitTarget::HeaderClose, {}};
    }

    if (!historyService_ || historyService_->State() != ClipboardHistoryState::Ready) {
        return {};
    }
    const auto records = historyService_->Snapshot();
    if (records.empty()) {
        return {};
    }

    if (state_ == WindowState::CollapsedPreview) {
        if (Contains(D2D1::RectF(8.0F, 62.0F, width - 8.0F, height - 8.0F), point)) {
            return {HitTarget::CollapsedBody, records.front().id};
        }
        return {};
    }

    if (Contains(D2D1::RectF(width - 83.0F, height - 36.0F, width - 12.0F, height - 6.0F), point)) {
        return {HitTarget::ClearHistory, {}};
    }

    float y = kListTop - scrollOffset_;
    for (const auto& record : records) {
        const float rowHeight = RowHeight(record);
        const bool isDetail = state_ == WindowState::ItemDetail && record.id == selectedRecordId_;
        if (point.y >= y && point.y < y + rowHeight) {
            if (detailClosing_ && record.id == transitioningRecordId_) {
                return {};
            }
            if (isDetail) {
                if (Contains(D2D1::RectF(width - 44.0F, y + 10.0F, width - 16.0F, y + 38.0F), point)) {
                    return {HitTarget::DetailCollapse, record.id};
                }
                if (transitionProgress_ < 0.5F) {
                    return {};
                }
                if (Contains(D2D1::RectF(17.0F, y + rowHeight - 50.0F, 91.0F, y + rowHeight - 10.0F), point)) {
                    return {HitTarget::DetailCopy, record.id};
                }
                if (record.plainText && Contains(D2D1::RectF(99.0F, y + rowHeight - 50.0F, 247.0F, y + rowHeight - 10.0F), point)) {
                    return {HitTarget::DetailPlainText, record.id};
                }
                if (Contains(D2D1::RectF(width - 91.0F, y + rowHeight - 50.0F, width - 17.0F, y + rowHeight - 10.0F), point)) {
                    return {HitTarget::DetailDelete, record.id};
                }
                return {};
            }
            if (Contains(D2D1::RectF(width - 44.0F, y + 11.0F, width - 16.0F, y + 41.0F), point)) {
                return {HitTarget::RecordCopy, record.id};
            }
            if (Contains(D2D1::RectF(8.0F, y + 3.0F, width - 8.0F, y + rowHeight - 5.0F), point)) {
                return {HitTarget::RecordBody, record.id};
            }
            return {};
        }
        y += rowHeight;
    }
    return {};
}

void AppWindow::ExecuteHit(const HitResult& hit) {
    switch (hit.target) {
    case HitTarget::HeaderToggle:
        ToggleCollapsed();
        break;
    case HitTarget::HeaderClose:
        DestroyWindow(window_);
        break;
    case HitTarget::CollapsedBody:
        SetWindowState(WindowState::HistoryList);
        break;
    case HitTarget::RecordBody:
        selectedRecordId_ = hit.recordId;
        SetWindowState(WindowState::ItemDetail);
        break;
    case HitTarget::RecordCopy:
    case HitTarget::DetailCopy:
        ActivateRecord(hit.recordId);
        break;
    case HitTarget::DetailCollapse:
        SetWindowState(WindowState::HistoryList);
        break;
    case HitTarget::DetailPlainText:
        CopyRecordAsPlainText(hit.recordId);
        break;
    case HitTarget::DetailDelete:
        DeleteSelectedRecord();
        break;
    case HitTarget::ClearHistory:
        ClearHistory();
        break;
    default:
        break;
    }
}

void AppWindow::ActivateRecord(std::wstring_view id) {
    if (!historyService_) {
        return;
    }
    try {
        const auto status = historyService_->Activate(id);
        if (status != winrt::Windows::ApplicationModel::DataTransfer::SetHistoryItemAsContentStatus::Success) {
            ShowOperationError(L"无法重新复制该历史记录，记录可能已被删除或访问受限。");
        }
    } catch (const winrt::hresult_error& error) {
        ShowOperationError(L"重新复制失败：" + std::wstring{error.message()});
    }
}

void AppWindow::CopyRecordAsPlainText(std::wstring_view id) {
    try {
        if (!historyService_->CopyPlainText(id)) {
            ShowOperationError(L"该记录不支持纯文本复制。");
        }
    } catch (const winrt::hresult_error& error) {
        ShowOperationError(L"纯文本复制失败：" + std::wstring{error.message()});
    }
}

void AppWindow::DeleteSelectedRecord() {
    const auto records = historyService_->Snapshot();
    const auto selected = std::find_if(records.begin(), records.end(), [this](const ClipboardRecord& record) {
        return record.id == selectedRecordId_;
    });
    if (selected == records.end()) {
        SetWindowState(WindowState::HistoryList);
        return;
    }

    const auto index = static_cast<std::size_t>(std::distance(records.begin(), selected));
    std::optional<std::wstring> adjacentId;
    if (index + 1 < records.size()) {
        adjacentId = records[index + 1].id;
    } else if (index > 0) {
        adjacentId = records[index - 1].id;
    }

    try {
        if (!historyService_->Delete(selectedRecordId_)) {
            ShowOperationError(L"删除失败，该记录可能已不存在。");
            return;
        }

        if (adjacentId) {
            selectedRecordId_ = std::move(*adjacentId);
            state_ = WindowState::ItemDetail;
            InvalidateRect(window_, nullptr, FALSE);
        } else {
            SetWindowState(WindowState::HistoryList);
        }
    } catch (const winrt::hresult_error& error) {
        ShowOperationError(L"删除失败：" + std::wstring{error.message()});
    }
}

void AppWindow::ClearHistory() {
    const int choice = MessageBoxW(
        window_,
        L"确定清空 Windows 剪贴板历史吗？此操作无法撤销。",
        L"清空剪贴板历史",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (choice != IDYES) {
        return;
    }

    try {
        if (!historyService_->Clear()) {
            ShowOperationError(L"清空 Windows 剪贴板历史失败。");
            return;
        }
        scrollOffset_ = 0.0F;
        SetWindowState(WindowState::HistoryList);
    } catch (const winrt::hresult_error& error) {
        ShowOperationError(L"清空失败：" + std::wstring{error.message()});
    }
}

void AppWindow::ShowOperationError(std::wstring_view message) const {
    const std::wstring text{message};
    MessageBoxW(window_, text.c_str(), L"浮贴", MB_OK | MB_ICONERROR);
}

void AppWindow::OnMouseWheel(short delta) {
    if (state_ == WindowState::CollapsedPreview || !historyService_) {
        return;
    }
    const auto records = historyService_->Snapshot();
    RECT client{};
    GetClientRect(window_, &client);
    const float logicalHeight = static_cast<float>(client.bottom - client.top) * 96.0F / static_cast<float>(dpi_);
    const float viewport = logicalHeight - kListTop - kFooterHeight;
    const float maximum = std::max(0.0F, ContentHeight(records) - viewport);
    scrollOffset_ = std::clamp(scrollOffset_ - static_cast<float>(delta) / WHEEL_DELTA * 48.0F, 0.0F, maximum);
    hover_ = {};
    InvalidateRect(window_, nullptr, FALSE);
}

float AppWindow::RowHeight(const ClipboardRecord& record) const {
    if (state_ == WindowState::ItemDetail && record.id == selectedRecordId_) {
        return kCompactRowHeight + (kDetailRowHeight - kCompactRowHeight) * transitionProgress_;
    }
    if (detailClosing_ && record.id == transitioningRecordId_) {
        return kCompactRowHeight + (kDetailRowHeight - kCompactRowHeight) * (1.0F - transitionProgress_);
    }
    return kCompactRowHeight;
}

float AppWindow::ContentHeight(const std::vector<ClipboardRecord>& records) const {
    float height = 0.0F;
    for (const auto& record : records) {
        height += RowHeight(record);
    }
    return height;
}

D2D1_POINT_2F AppWindow::ToLogicalPoint(LPARAM lParam) const {
    return {
        static_cast<float>(GET_X_LPARAM(lParam)) * 96.0F / static_cast<float>(dpi_),
        static_cast<float>(GET_Y_LPARAM(lParam)) * 96.0F / static_cast<float>(dpi_),
    };
}

float AppWindow::Scale(float logicalPixels) const {
    return logicalPixels * static_cast<float>(dpi_) / 96.0F;
}

bool AppWindow::IsHeaderControl(D2D1_POINT_2F point) const {
    RECT client{};
    GetClientRect(window_, &client);
    const float width = static_cast<float>(client.right - client.left) * 96.0F / static_cast<float>(dpi_);
    return point.x >= width - 160.0F;
}
