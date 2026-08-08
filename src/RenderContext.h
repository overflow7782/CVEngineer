#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class RenderContext final {
public:
    ~RenderContext();
    bool Initialize(HWND window);
    void Resize(UINT width, UINT height);
    void SetDpi(float dpi);
    bool BeginDraw();
    HRESULT EndDraw();
    void DiscardDeviceResources();

    [[nodiscard]] ID2D1HwndRenderTarget* Target() const { return renderTarget_.Get(); }
    [[nodiscard]] IDWriteTextFormat* TitleFormat() const { return titleFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* BodyFormat() const { return bodyFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* SmallFormat() const { return smallFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* CodeFormat() const { return codeFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* LabelFormat() const { return labelFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* ButtonFormat() const { return buttonFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* IconFormat() const { return iconFormat_.Get(); }
    [[nodiscard]] ID2D1SolidColorBrush* Brush(D2D1_COLOR_F color);

private:
    bool EnsureDeviceResources();

    HWND window_{};
    float dpi_{96.0F};
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> bodyFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> codeFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> labelFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> buttonFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> iconFormat_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> fontCollection_;
    std::vector<std::wstring> privateFontPaths_;
};
