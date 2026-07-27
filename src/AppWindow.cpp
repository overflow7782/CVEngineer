#include "AppWindow.h"

#include <d2d1helper.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

namespace {
constexpr wchar_t kWindowClassName[] = L"FloatNote.NativeWindow";
constexpr wchar_t kWindowTitle[] = L"浮贴";

constexpr D2D1_COLOR_F kText = {0.10F, 0.13F, 0.15F, 1.0F};
constexpr D2D1_COLOR_F kSecondaryText = {0.37F, 0.41F, 0.43F, 1.0F};
constexpr D2D1_COLOR_F kAccent = {0.08F, 0.62F, 0.58F, 1.0F};
constexpr D2D1_COLOR_F kCard = {0.97F, 0.98F, 0.98F, 0.78F};
constexpr D2D1_COLOR_F kBorder = {0.76F, 0.81F, 0.82F, 0.65F};

D2D1_ROUNDED_RECT Rounded(float left, float top, float right, float bottom, float radius) {
    return D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), radius, radius);
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
    const int height = static_cast<int>(Scale(136.0F));
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
        if (point.y >= 0 && point.y < static_cast<LONG>(Scale(52.0F)) && !IsHeaderButton(point)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_LBUTTONUP: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT client{};
        GetClientRect(window_, &client);
        const auto closeLeft = client.right - static_cast<LONG>(Scale(36.0F));
        const auto toggleLeft = client.right - static_cast<LONG>(Scale(72.0F));
        if (point.y < static_cast<LONG>(Scale(52.0F)) && point.x >= closeLeft) {
            DestroyWindow(window_);
        } else if ((point.y < static_cast<LONG>(Scale(52.0F)) && point.x >= toggleLeft) ||
                   (!expanded_ && point.y >= static_cast<LONG>(Scale(52.0F)))) {
            ToggleExpanded();
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
        OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && expanded_) {
            ToggleExpanded();
        }
        return 0;

    case WM_FLOATNOTE_HISTORY_UPDATED:
        scrollOffset_ = 0.0F;
        InvalidateRect(window_, nullptr, FALSE);
        return 0;

    case WM_DESTROY:
        if (historyService_) {
            historyService_->Stop();
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
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

void AppWindow::ToggleExpanded() {
    expanded_ = !expanded_;
    scrollOffset_ = 0.0F;
    ResizeForState();
}

void AppWindow::ResizeForState() {
    RECT current{};
    GetWindowRect(window_, &current);
    const int width = static_cast<int>(Scale(expanded_ ? 380.0F : 340.0F));
    const int height = static_cast<int>(Scale(expanded_ ? 640.0F : 136.0F));
    SetWindowPos(
        window_, HWND_TOPMOST, current.left, current.top, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(window_, nullptr, FALSE);
}

void AppWindow::Paint() {
    PAINTSTRUCT paint{};
    BeginPaint(window_, &paint);

    if (renderer_.BeginDraw()) {
        auto* target = renderer_.Target();
        target->Clear(D2D1::ColorF(0.91F, 0.94F, 0.95F, 0.92F));
        const auto size = target->GetSize();
        const auto records = historyService_ ? historyService_->Snapshot() : std::vector<ClipboardRecord>{};

        DrawHeader(size, records);
        if (historyService_ && historyService_->State() != ClipboardHistoryState::Ready) {
            DrawStatus(size, historyService_->StatusMessage());
        } else if (expanded_) {
            DrawExpanded(size, records);
        } else {
            DrawCollapsed(size, records);
        }
        renderer_.EndDraw();
    }

    EndPaint(window_, &paint);
}

void AppWindow::DrawHeader(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    auto* target = renderer_.Target();
    target->FillRoundedRectangle(
        Rounded(10.0F, 10.0F, 42.0F, 42.0F, 7.0F),
        renderer_.Brush(D2D1::ColorF(0.87F, 0.97F, 0.96F, 0.95F)));
    target->DrawRoundedRectangle(
        Rounded(10.0F, 10.0F, 42.0F, 42.0F, 7.0F),
        renderer_.Brush(kAccent), 1.5F);

    target->DrawTextW(
        L"浮贴", 2, renderer_.TitleFormat(),
        D2D1::RectF(52.0F, 9.0F, size.width - 86.0F, 31.0F),
        renderer_.Brush(kText));
    const std::wstring subtitle = std::to_wstring(records.size()) + L" / 25 条 Windows 记录";
    target->DrawTextW(
        subtitle.c_str(), static_cast<UINT32>(subtitle.size()), renderer_.SmallFormat(),
        D2D1::RectF(52.0F, 30.0F, size.width - 86.0F, 48.0F),
        renderer_.Brush(kSecondaryText));

    const wchar_t* toggle = expanded_ ? L"⌃" : L"⌄";
    target->DrawTextW(
        toggle, 1, renderer_.BodyFormat(),
        D2D1::RectF(size.width - 68.0F, 16.0F, size.width - 42.0F, 42.0F),
        renderer_.Brush(kSecondaryText));
    target->DrawTextW(
        L"×", 1, renderer_.BodyFormat(),
        D2D1::RectF(size.width - 34.0F, 16.0F, size.width - 8.0F, 42.0F),
        renderer_.Brush(kSecondaryText));
}

void AppWindow::DrawCollapsed(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    auto* target = renderer_.Target();
    target->FillRoundedRectangle(
        Rounded(8.0F, 54.0F, size.width - 8.0F, size.height - 8.0F, 7.0F),
        renderer_.Brush(kCard));
    target->DrawRoundedRectangle(
        Rounded(8.0F, 54.0F, size.width - 8.0F, size.height - 8.0F, 7.0F),
        renderer_.Brush(kBorder));

    if (records.empty()) {
        DrawStatus(size, L"等待 Windows 剪贴板历史");
        return;
    }

    const auto& current = records.front();
    target->DrawTextW(
        current.typeLabel.c_str(), static_cast<UINT32>(current.typeLabel.size()), renderer_.SmallFormat(),
        D2D1::RectF(18.0F, 63.0F, 100.0F, 82.0F), renderer_.Brush(kAccent));
    target->DrawTextW(
        current.previewText.c_str(), static_cast<UINT32>(current.previewText.size()),
        current.type == ContentType::Json || current.type == ContentType::Code
            ? renderer_.CodeFormat() : renderer_.BodyFormat(),
        D2D1::RectF(18.0F, 82.0F, size.width - 18.0F, size.height - 12.0F),
        renderer_.Brush(kText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void AppWindow::DrawExpanded(const D2D1_SIZE_F& size, const std::vector<ClipboardRecord>& records) {
    if (records.empty()) {
        DrawStatus(size, L"Windows 剪贴板历史为空");
        return;
    }

    auto* target = renderer_.Target();
    constexpr float rowHeight = 78.0F;
    constexpr float top = 56.0F;
    const float bottom = size.height - 34.0F;
    const float viewportHeight = bottom - top;

    target->PushAxisAlignedClip(D2D1::RectF(0.0F, top, size.width, bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (std::size_t index = 0; index < records.size(); ++index) {
        const float y = top + static_cast<float>(index) * rowHeight - scrollOffset_;
        if (y + rowHeight < top || y > bottom) {
            continue;
        }

        const auto& record = records[index];
        target->FillRoundedRectangle(
            Rounded(8.0F, y + 3.0F, size.width - 8.0F, y + rowHeight - 4.0F, 6.0F),
            renderer_.Brush(kCard));
        target->DrawRoundedRectangle(
            Rounded(8.0F, y + 3.0F, size.width - 8.0F, y + rowHeight - 4.0F, 6.0F),
            renderer_.Brush(kBorder));

        target->DrawTextW(
            record.typeLabel.c_str(), static_cast<UINT32>(record.typeLabel.size()), renderer_.SmallFormat(),
            D2D1::RectF(18.0F, y + 10.0F, 120.0F, y + 28.0F), renderer_.Brush(kAccent));
        target->DrawTextW(
            record.previewText.c_str(), static_cast<UINT32>(record.previewText.size()), renderer_.BodyFormat(),
            D2D1::RectF(18.0F, y + 30.0F, size.width - 18.0F, y + 57.0F),
            renderer_.Brush(kText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        target->DrawTextW(
            record.metadataText.c_str(), static_cast<UINT32>(record.metadataText.size()), renderer_.SmallFormat(),
            D2D1::RectF(18.0F, y + 57.0F, size.width - 18.0F, y + 73.0F),
            renderer_.Brush(kSecondaryText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    target->PopAxisAlignedClip();

    const std::wstring footer = std::to_wstring(records.size()) + L" / 25 条记录";
    target->DrawTextW(
        footer.c_str(), static_cast<UINT32>(footer.size()), renderer_.SmallFormat(),
        D2D1::RectF(14.0F, size.height - 28.0F, size.width - 14.0F, size.height - 8.0F),
        renderer_.Brush(kSecondaryText));

    const float contentHeight = static_cast<float>(records.size()) * rowHeight;
    const float maxScroll = std::max(0.0F, contentHeight - viewportHeight);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maxScroll);
}

void AppWindow::DrawStatus(const D2D1_SIZE_F& size, std::wstring_view message) {
    auto* target = renderer_.Target();
    const std::wstring text{message};
    target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), renderer_.BodyFormat(),
        D2D1::RectF(18.0F, 68.0F, size.width - 18.0F, size.height - 16.0F),
        renderer_.Brush(kSecondaryText), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void AppWindow::OnMouseWheel(short delta) {
    if (!expanded_ || !historyService_) {
        return;
    }
    const auto records = historyService_->Snapshot();
    RECT client{};
    GetClientRect(window_, &client);
    const float viewport = static_cast<float>(client.bottom - client.top) - 90.0F;
    const float maximum = std::max(0.0F, static_cast<float>(records.size()) * 78.0F - viewport);
    scrollOffset_ = std::clamp(scrollOffset_ - static_cast<float>(delta) / WHEEL_DELTA * 48.0F, 0.0F, maximum);
    InvalidateRect(window_, nullptr, FALSE);
}

float AppWindow::Scale(float logicalPixels) const {
    return logicalPixels * static_cast<float>(dpi_) / 96.0F;
}

bool AppWindow::IsHeaderButton(POINT point) const {
    RECT client{};
    GetClientRect(window_, &client);
    return point.x >= client.right - static_cast<LONG>(Scale(72.0F));
}
