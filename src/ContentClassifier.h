#pragma once

#include "ClipboardRecord.h"

#include <cstddef>
#include <string>
#include <string_view>

class ContentClassifier final {
public:
    static ContentType ClassifyText(std::wstring_view text);
    static std::wstring TypeLabel(ContentType type);
    static std::wstring BuildPreview(std::wstring_view text, std::size_t maxCharacters = 420);
    static std::wstring BuildTextMetadata(ContentType type, std::wstring_view text);

private:
    static bool IsHttpUrl(std::wstring_view text);
    static bool IsMarkdown(std::wstring_view text);
};
