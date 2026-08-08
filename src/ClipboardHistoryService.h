#pragma once

#include "ClipboardRecord.h"

#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

inline constexpr UINT WM_FLOATNOTE_HISTORY_UPDATED = WM_APP + 1;

class ClipboardHistoryService final : public std::enable_shared_from_this<ClipboardHistoryService> {
public:
    explicit ClipboardHistoryService(HWND ownerWindow);
    ~ClipboardHistoryService();

    ClipboardHistoryService(const ClipboardHistoryService&) = delete;
    ClipboardHistoryService& operator=(const ClipboardHistoryService&) = delete;

    void Start();
    void Stop();
    void Refresh();

    [[nodiscard]] std::vector<ClipboardRecord> Snapshot() const;
    [[nodiscard]] ClipboardHistoryState State() const;
    [[nodiscard]] std::wstring StatusMessage() const;

    winrt::Windows::ApplicationModel::DataTransfer::SetHistoryItemAsContentStatus Activate(
        std::wstring_view id) const;
    bool CopyPlainText(std::wstring_view id) const;
    bool Delete(std::wstring_view id);
    bool Clear();

private:
    static winrt::fire_and_forget RefreshAsync(
        std::shared_ptr<ClipboardHistoryService> self,
        std::uint64_t generation);

    void Publish(
        std::uint64_t generation,
        ClipboardHistoryState state,
        std::wstring message,
        std::vector<ClipboardRecord> records);
    [[nodiscard]] winrt::Windows::ApplicationModel::DataTransfer::ClipboardHistoryItem FindItem(
        std::wstring_view id) const;

    HWND ownerWindow_{};
    mutable std::mutex mutex_;
    std::vector<ClipboardRecord> records_;
    ClipboardHistoryState state_{ClipboardHistoryState::Loading};
    std::wstring statusMessage_{L"正在读取 Windows 剪贴板历史…"};
    winrt::event_token historyChangedToken_{};
    bool subscribed_{false};
    std::atomic_uint64_t refreshGeneration_{0};
};
