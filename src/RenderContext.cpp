#include "RenderContext.h"

#include <d2d1helper.h>

bool RenderContext::Initialize(HWND window) {
    window_ = window;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())))) {
        return false;
    }

    if (FAILED(dwriteFactory_->CreateTextFormat(
            L"Segoe UI Variable",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.0F,
            L"zh-CN",
            titleFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            L"Segoe UI Variable",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0F,
            L"zh-CN",
            bodyFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            L"Segoe UI Variable",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0F,
            L"zh-CN",
            smallFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            L"Cascadia Mono",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0F,
            L"zh-CN",
            codeFormat_.GetAddressOf()))) {
        return false;
    }

    bodyFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    codeFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return EnsureDeviceResources();
}

void RenderContext::Resize(UINT width, UINT height) {
    if (renderTarget_) {
        renderTarget_->Resize(D2D1::SizeU(width, height));
    }
}

bool RenderContext::BeginDraw() {
    if (!EnsureDeviceResources()) {
        return false;
    }
    renderTarget_->BeginDraw();
    return true;
}

HRESULT RenderContext::EndDraw() {
    if (!renderTarget_) {
        return E_FAIL;
    }
    const HRESULT result = renderTarget_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
    return result;
}

void RenderContext::DiscardDeviceResources() {
    brush_.Reset();
    renderTarget_.Reset();
}

ID2D1SolidColorBrush* RenderContext::Brush(D2D1_COLOR_F color) {
    if (!brush_ && renderTarget_) {
        renderTarget_->CreateSolidColorBrush(color, brush_.GetAddressOf());
    }
    if (brush_) {
        brush_->SetColor(color);
    }
    return brush_.Get();
}

bool RenderContext::EnsureDeviceResources() {
    if (renderTarget_) {
        return true;
    }
    if (!d2dFactory_ || !window_) {
        return false;
    }

    RECT client{};
    GetClientRect(window_, &client);
    const auto size = D2D1::SizeU(
        static_cast<UINT>(client.right - client.left),
        static_cast<UINT>(client.bottom - client.top));

    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
    const auto hwndProperties = D2D1::HwndRenderTargetProperties(window_, size);
    if (FAILED(d2dFactory_->CreateHwndRenderTarget(
            properties,
            hwndProperties,
            renderTarget_.GetAddressOf()))) {
        return false;
    }

    return SUCCEEDED(renderTarget_->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), brush_.GetAddressOf()));
}
