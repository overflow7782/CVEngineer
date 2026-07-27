#include "ContentClassifier.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>

#include <algorithm>
#include <cwctype>
#include <sstream>

namespace {
std::wstring_view Trim(std::wstring_view value) {
    while (!value.empty() && std::iswspace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::iswspace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

bool StartsWith(std::wstring_view value, std::wstring_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool IsListItem(std::wstring_view line) {
    line = Trim(line);
    if (StartsWith(line, L"- ") || StartsWith(line, L"* ") || StartsWith(line, L"+ ")) {
        return true;
    }

    std::size_t index = 0;
    while (index < line.size() && std::iswdigit(line[index])) {
        ++index;
    }
    return index > 0 && index + 1 < line.size() && line[index] == L'.' && line[index + 1] == L' ';
}
} // namespace

ContentType ContentClassifier::ClassifyText(std::wstring_view text) {
    const auto trimmed = Trim(text);
    if (trimmed.empty()) {
        return ContentType::Text;
    }

    try {
        winrt::Windows::Data::Json::JsonValue value{nullptr};
        if (winrt::Windows::Data::Json::JsonValue::TryParse(winrt::hstring{trimmed}, value)) {
            const auto type = value.ValueType();
            if (type == winrt::Windows::Data::Json::JsonValueType::Object ||
                type == winrt::Windows::Data::Json::JsonValueType::Array) {
                return ContentType::Json;
            }
        }
    } catch (const winrt::hresult_error&) {
        // A text classification failure must not affect history loading.
    }

    if (IsHttpUrl(trimmed)) {
        return ContentType::Link;
    }
    if (IsMarkdown(trimmed)) {
        return ContentType::Markdown;
    }
    return ContentType::Text;
}

std::wstring ContentClassifier::TypeLabel(ContentType type) {
    switch (type) {
    case ContentType::Text: return L"文本";
    case ContentType::Json: return L"JSON";
    case ContentType::Markdown: return L"Markdown";
    case ContentType::Code: return L"代码";
    case ContentType::Link: return L"链接";
    case ContentType::Image: return L"图片";
    case ContentType::Files: return L"文件";
    default: return L"未知";
    }
}

std::wstring ContentClassifier::BuildPreview(std::wstring_view text, std::size_t maxCharacters) {
    std::wstring preview;
    preview.reserve(std::min(text.size(), maxCharacters) + 1);

    for (const wchar_t ch : text) {
        if (preview.size() >= maxCharacters) {
            break;
        }
        if (ch != L'\r') {
            preview.push_back(ch == L'\t' ? L' ' : ch);
        }
    }

    if (text.size() > maxCharacters) {
        preview.append(L"…");
    }
    return preview;
}

std::wstring ContentClassifier::BuildTextMetadata(ContentType type, std::wstring_view text) {
    const auto lines = text.empty() ? 0ULL : 1ULL + static_cast<unsigned long long>(std::count(text.begin(), text.end(), L'\n'));
    return TypeLabel(type) + L" · " + std::to_wstring(lines) + L" 行 · " +
        std::to_wstring(text.size()) + L" 字符";
}

bool ContentClassifier::IsHttpUrl(std::wstring_view text) {
    try {
        const winrt::Windows::Foundation::Uri uri{winrt::hstring{text}};
        const auto scheme = uri.SchemeName();
        return scheme == L"http" || scheme == L"https";
    } catch (const winrt::hresult_error&) {
        return false;
    }
}

bool ContentClassifier::IsMarkdown(std::wstring_view text) {
    std::wistringstream stream{std::wstring{text}};
    std::wstring line;
    bool previousWasListItem = false;

    while (std::getline(stream, line)) {
        const auto trimmed = Trim(line);
        if (StartsWith(trimmed, L"# ") || StartsWith(trimmed, L"## ") ||
            StartsWith(trimmed, L"```" ) || StartsWith(trimmed, L"~~~") ||
            StartsWith(trimmed, L"> ")) {
            return true;
        }

        const bool listItem = IsListItem(trimmed);
        if (listItem && previousWasListItem) {
            return true;
        }
        previousWasListItem = listItem;
    }
    return false;
}
