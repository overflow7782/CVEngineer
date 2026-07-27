#pragma once

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <string>

enum class ContentType {
    Text,
    Json,
    Markdown,
    Code,
    Link,
    Image,
    Files,
    Unknown,
};

enum class ClipboardHistoryState {
    Loading,
    Ready,
    Disabled,
    AccessDenied,
    Error,
};

struct ClipboardRecord {
    std::wstring id;
    ContentType type{ContentType::Unknown};
    std::wstring typeLabel;
    std::wstring previewText;
    std::wstring metadataText;
    winrt::Windows::Foundation::DateTime timestamp{};
    winrt::Windows::ApplicationModel::DataTransfer::ClipboardHistoryItem item{nullptr};
};
