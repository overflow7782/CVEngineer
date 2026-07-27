#include "AppWindow.h"

#include <windows.h>
#include <winrt/base.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    AppWindow app{instance};
    if (!app.Create()) {
        MessageBoxW(nullptr, L"浮贴窗口创建失败。", L"浮贴", MB_OK | MB_ICONERROR);
        return 1;
    }
    app.Show(command);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
