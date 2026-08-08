#include "ClipboardHistoryService.h"

#include "ContentClassifier.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <algorithm>
#include <cstring>
#include <utility>

using namespace winrt::Windows::ApplicationModel::DataTransfer;

ClipboardHistoryService::ClipboardHistoryService(HWND ownerWindow)
    : ownerWindow_(ownerWindow) {
}

ClipboardHistoryService::~ClipboardHistoryService() {
    Stop();
}

void ClipboardHistoryService::Start() {
    if (subscribed_) {
        return;
    }

    try {
        const auto weak = weak_from_this();
        historyChangedToken_ = Clipboard::HistoryChanged(
            [weak](const auto&, const ClipboardHistoryChangedEventArgs&) {
                if (const auto service = weak.lock()) {
                    service->Refresh();
                }
            });
        subscribed_ = true;
    } catch (const winrt::hresult_error& error) {
        Publish(
            ++refreshGeneration_,
            ClipboardHistoryState::Error,
            L"无法订阅 Windows 剪贴板历史：" + std::wstring{error.message()},
            {});
        return;
    }

    Refresh();
}

void ClipboardHistoryService::Stop() {
    ++refreshGeneration_;
    if (subscribed_) {
        Clipboard::HistoryChanged(historyChangedToken_);
        subscribed_ = false;
    }
}

void ClipboardHistoryService::Refresh() {
    const auto generation = ++refreshGeneration_;
    RefreshAsync(shared_from_this(), generation);
}

std::vector<ClipboardRecord> ClipboardHistoryService::Snapshot() const {
    const std::scoped_lock lock{mutex_};
    return records_;
}

ClipboardHistoryState ClipboardHistoryService::State() const {
    const std::scoped_lock lock{mutex_};
    return state_;
}

std::wstring ClipboardHistoryService::StatusMessage() const {
    const std::scoped_lock lock{mutex_};
    return statusMessage_;
}

SetHistoryItemAsContentStatus ClipboardHistoryService::Activate(std::wstring_view id) const {
    const auto item = FindItem(id);
    if (!item) {
        return SetHistoryItemAsContentStatus::ItemDeleted;
    }
    return Clipboard::SetHistoryItemAsContent(item);
}

bool ClipboardHistoryService::CopyPlainText(std::wstring_view id) const {
    std::optional<std::wstring> plainText;
    {
        const std::scoped_lock lock{mutex_};
        const auto found = std::find_if(records_.begin(), records_.end(), [id](const ClipboardRecord& record) {
            return record.id == id;
        });
        if (found == records_.end() || !found->plainText) {
            return false;
        }
        plainText = found->plainText;
    }

    const SIZE_T byteCount = (plainText->size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!memory) {
        return false;
    }

    void* buffer = GlobalLock(memory);
    if (!buffer) {
        GlobalFree(memory);
        return false;
    }
    std::memcpy(buffer, plainText->c_str(), byteCount);
    GlobalUnlock(memory);

    if (!OpenClipboard(ownerWindow_)) {
        GlobalFree(memory);
        return false;
    }

    const bool copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    CloseClipboard();
    if (!copied) {
        GlobalFree(memory);
    }
    return copied;
}

bool ClipboardHistoryService::Delete(std::wstring_view id) {
    const auto item = FindItem(id);
    if (!item) {
        return false;
    }
    const bool deleted = Clipboard::DeleteItemFromHistory(item);
    if (deleted) {
        Refresh();
    }
    return deleted;
}

bool ClipboardHistoryService::Clear() {
    const bool cleared = Clipboard::ClearHistory();
    if (cleared) {
        Refresh();
    }
    return cleared;
}

winrt::fire_and_forget ClipboardHistoryService::RefreshAsync(
    std::shared_ptr<ClipboardHistoryService> self,
    std::uint64_t generation) {
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        if (result.Status() == ClipboardHistoryItemsResultStatus::ClipboardHistoryDisabled) {
            self->Publish(
                generation,
                ClipboardHistoryState::Disabled,
                L"Windows 剪贴板历史未开启，请按 Win + V 启用。",
                {});
            co_return;
        }
        if (result.Status() == ClipboardHistoryItemsResultStatus::AccessDenied) {
            self->Publish(
                generation,
                ClipboardHistoryState::AccessDenied,
                L"系统策略不允许访问剪贴板历史。",
                {});
            co_return;
        }

        std::vector<ClipboardRecord> records;
        const auto items = result.Items();
        const auto count = std::min<std::uint32_t>(items.Size(), 25U);
        records.reserve(count);

        for (std::uint32_t index = 0; index < count; ++index) {
            const auto item = items.GetAt(index);
            const auto content = item.Content();

            ClipboardRecord record;
            record.id = item.Id().c_str();
            record.timestamp = item.Timestamp();
            record.item = item;

            try {
                if (content.Contains(StandardDataFormats::StorageItems())) {
                    record.type = ContentType::Files;
                    record.previewText = L"文件记录（详情读取将在下一阶段实现）";
                    record.metadataText = L"Windows 文件列表";
                } else if (content.Contains(StandardDataFormats::Bitmap())) {
                    record.type = ContentType::Image;
                    record.previewText = L"图片记录（缩略图将在下一阶段实现）";
                    record.metadataText = L"位图";
                } else if (content.Contains(StandardDataFormats::Text())) {
                    const std::wstring text = (co_await content.GetTextAsync()).c_str();
                    record.plainText = text;
                    record.type = ContentClassifier::ClassifyText(text);

                    if (content.Contains(StandardDataFormats::Html())) {
                        const std::wstring html = (co_await content.GetHtmlFormatAsync()).c_str();
                        if (html.find(L"<pre") != std::wstring::npos ||
                            html.find(L"<code") != std::wstring::npos) {
                            record.type = ContentType::Code;
                        }
                    }

                    record.previewText = ContentClassifier::BuildPreview(text);
                    record.metadataText = ContentClassifier::BuildTextMetadata(record.type, text);
                } else if (content.Contains(StandardDataFormats::WebLink())) {
                    record.type = ContentType::Link;
                    const auto uri = co_await content.GetWebLinkAsync();
                    record.previewText = uri.AbsoluteUri().c_str();
                    record.plainText = record.previewText;
                    record.metadataText = L"Windows WebLink";
                } else {
                    record.type = ContentType::Unknown;
                    record.previewText = L"暂不支持的剪贴板格式";
                    record.metadataText = L"未知格式";
                }
            } catch (const winrt::hresult_error&) {
                record.type = ContentType::Unknown;
                record.previewText = L"无法读取此历史项";
                record.metadataText = L"读取失败";
            }

            record.typeLabel = ContentClassifier::TypeLabel(record.type);
            records.push_back(std::move(record));
        }

        self->Publish(
            generation,
            ClipboardHistoryState::Ready,
            records.empty() ? L"Windows 剪贴板历史为空" : L"正在读取 Windows 历史",
            std::move(records));
    } catch (const winrt::hresult_error& error) {
        self->Publish(
            generation,
            ClipboardHistoryState::Error,
            L"读取剪贴板历史失败：" + std::wstring{error.message()},
            {});
    }
}

void ClipboardHistoryService::Publish(
    std::uint64_t generation,
    ClipboardHistoryState state,
    std::wstring message,
    std::vector<ClipboardRecord> records) {
    if (generation != refreshGeneration_.load()) {
        return;
    }

    {
        const std::scoped_lock lock{mutex_};
        state_ = state;
        statusMessage_ = std::move(message);
        records_ = std::move(records);
    }
    PostMessageW(ownerWindow_, WM_FLOATNOTE_HISTORY_UPDATED, 0, 0);
}

ClipboardHistoryItem ClipboardHistoryService::FindItem(std::wstring_view id) const {
    const std::scoped_lock lock{mutex_};
    const auto found = std::find_if(records_.begin(), records_.end(), [id](const ClipboardRecord& record) {
        return record.id == id;
    });
    return found == records_.end() ? nullptr : found->item;
}
