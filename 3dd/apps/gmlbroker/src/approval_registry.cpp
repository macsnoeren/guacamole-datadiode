#include "../include/approval_registry.h"
#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

void ApprovalRegistry::Create(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    entries[channel] = Entry{std::make_shared<std::atomic<bool>>(false), ""};
}

std::shared_ptr<std::atomic<bool>> ApprovalRegistry::Flag(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    return it == entries.end() ? nullptr : it->second.approved;
}

void ApprovalRegistry::SetRequestId(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    if (it != entries.end())
        it->second.request_id = id;
}

bool ApprovalRegistry::Approve(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    if (it == entries.end() || it->second.request_id != id)
        return false;
    it->second.approved->store(true);
    return true;
}

bool ApprovalRegistry::Matches(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    return it != entries.end() && it->second.request_id == id;
}

void ApprovalRegistry::Remove(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    entries.erase(channel);
}
