#include "ImGuiLogSink.h"

namespace gb2d::logging {

ImGuiLogBuffer& ImGuiLogBuffer::instance() {
    static ImGuiLogBuffer buf;
    return buf;
}

void ImGuiLogBuffer::push(LogEntry e) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (entries_.size() >= capacity_) {
        // drop oldest to keep within capacity
        const size_t to_drop = entries_.size() - capacity_ + 1;
        entries_.erase(entries_.begin(), entries_.begin() + (std::ptrdiff_t)to_drop);
    }
    entries_.emplace_back(std::move(e));
}

void ImGuiLogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    entries_.clear();
}

void ImGuiLogBuffer::setCapacity(size_t cap) {
    std::lock_guard<std::mutex> lock(mtx_);
    capacity_ = cap;
    if (entries_.size() > capacity_) {
        entries_.erase(entries_.begin(), entries_.begin() + (std::ptrdiff_t)(entries_.size() - capacity_));
    }
}

size_t ImGuiLogBuffer::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return entries_.size();
}

void ImGuiLogBuffer::snapshot(std::vector<LogEntry>& out) const {
    std::lock_guard<std::mutex> lock(mtx_);
    out = entries_;
}

std::shared_ptr<spdlog::sinks::sink> create_imgui_sink() {
    return std::make_shared<imgui_sink_mt>();
}

} // namespace gb2d::logging
