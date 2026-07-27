#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

class RenderContext final {
public:
    bool Initialize(HWND window);
    void Resize(UINT width, UINT height);
    bool BeginDraw();
    HRESULT EndDraw();
    void DiscardDeviceResources();

    [[nodiscard]] ID2D1HwndRenderTarget* Target() const { return renderTarget_.Get(); }
    [[nodiscard]] IDWriteTextFormat* TitleFormat() const { return titleFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* BodyFormat() const { return bodyFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* SmallFormat() const { return smallFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* CodeFormat() const { return codeFormat_.Get(); }
    [[nodiscard]] ID2D1SolidColorBrush* Brush(D2D1_COLOR_F color);

private:
    bool EnsureDeviceResources();

    HWND window_{};
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> bodyFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> codeFormat_;
};
