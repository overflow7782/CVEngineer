#include "RenderContext.h"

#include <d2d1helper.h>

#include <filesystem>

namespace {
constexpr wchar_t kEnglishFont[] = L"JetBrains Maple Mono";
constexpr wchar_t kChineseFont[] = L"JetBrains Maple Mono";

std::wstring FontPath(std::wstring_view fileName) {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
    if (length == 0 || length >= ARRAYSIZE(modulePath)) {
        return {};
    }
    return (std::filesystem::path(modulePath, modulePath + length).parent_path()
        / L"assets" / L"fonts" / fileName).wstring();
}
}

RenderContext::~RenderContext() {
    for (const auto& path : privateFontPaths_) {
        RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
    }
}

bool RenderContext::Initialize(HWND window) {
    window_ = window;

    for (const auto fileName : {
        L"JetBrainsMapleMono-Light.ttf",
        L"JetBrainsMapleMono-Regular.ttf",
        L"JetBrainsMapleMono-SemiBold.ttf"}) {
        const auto path = FontPath(fileName);
        if (path.empty() || AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) == 0) {
            return false;
        }
        privateFontPaths_.push_back(path);
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteFactory3> factory3;
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> baseFontSetBuilder;
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
    Microsoft::WRL::ComPtr<IDWriteFontSet> fontSet;
    if (FAILED(dwriteFactory_.As(&factory3)) ||
        FAILED(factory3->CreateFontSetBuilder(&baseFontSetBuilder)) ||
        FAILED(baseFontSetBuilder.As(&fontSetBuilder))) {
        return false;
    }
    for (const auto& path : privateFontPaths_) {
        Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;
        if (FAILED(dwriteFactory_->CreateFontFileReference(path.c_str(), nullptr, &fontFile)) ||
            FAILED(fontSetBuilder->AddFontFile(fontFile.Get()))) {
            return false;
        }
    }
    if (FAILED(fontSetBuilder->CreateFontSet(&fontSet)) ||
        FAILED(factory3->CreateFontCollectionFromFontSet(fontSet.Get(), &fontCollection_))) {
        return false;
    }

    IDWriteFontCollection* textCollection = fontCollection_.Get();

    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.5F,
            L"zh-CN",
            titleFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_LIGHT,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            14.25F,
            L"zh-CN",
            bodyFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_LIGHT,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.75F,
            L"zh-CN",
            smallFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_LIGHT,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0F,
            L"zh-CN",
            codeFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0F,
            L"zh-CN",
            labelFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            kEnglishFont,
            textCollection,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0F,
            L"zh-CN",
            buttonFormat_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(dwriteFactory_->CreateTextFormat(
            L"Segoe Fluent Icons",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            19.0F,
            L"zh-CN",
            iconFormat_.GetAddressOf()))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
    Microsoft::WRL::ComPtr<IDWriteFontFallbackBuilder> fallbackBuilder;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> fontFallback;
    if (SUCCEEDED(dwriteFactory_.As(&factory2)) &&
        SUCCEEDED(factory2->CreateFontFallbackBuilder(&fallbackBuilder))) {
        const DWRITE_UNICODE_RANGE ranges[] = {
            {0x2E80, 0x2EFF}, {0x3000, 0x303F}, {0x3040, 0x30FF},
            {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xF900, 0xFAFF},
        };
        const wchar_t* families[] = {kChineseFont};
        if (SUCCEEDED(fallbackBuilder->AddMapping(
                ranges, ARRAYSIZE(ranges), families, ARRAYSIZE(families),
                fontCollection_.Get(), L"zh-CN", kEnglishFont, 1.0F)) &&
            SUCCEEDED(fallbackBuilder->CreateFontFallback(&fontFallback))) {
            const auto applyFallback = [&fontFallback](IDWriteTextFormat* format) {
                Microsoft::WRL::ComPtr<IDWriteTextFormat2> format2;
                if (SUCCEEDED(format->QueryInterface(IID_PPV_ARGS(&format2)))) {
                    format2->SetFontFallback(fontFallback.Get());
                }
            };
            applyFallback(titleFormat_.Get());
            applyFallback(bodyFormat_.Get());
            applyFallback(smallFormat_.Get());
            applyFallback(codeFormat_.Get());
            applyFallback(labelFormat_.Get());
            applyFallback(buttonFormat_.Get());
            applyFallback(iconFormat_.Get());
        }
    }

    bodyFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    codeFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    buttonFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    buttonFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return EnsureDeviceResources();
}

void RenderContext::Resize(UINT width, UINT height) {
    if (renderTarget_) {
        renderTarget_->Resize(D2D1::SizeU(width, height));
    }
}

void RenderContext::SetDpi(float dpi) {
    dpi_ = dpi;
    if (renderTarget_) {
        renderTarget_->SetDpi(dpi_, dpi_);
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
    renderTarget_->SetDpi(dpi_, dpi_);
    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    return SUCCEEDED(renderTarget_->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), brush_.GetAddressOf()));
}
