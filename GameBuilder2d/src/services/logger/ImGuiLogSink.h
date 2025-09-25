#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

namespace gb2d::logging {

struct LogEntry {
    spdlog::level::level_enum level;
    std::chrono::system_clock::time_point time;
    std::string message;
};

class ImGuiLogBuffer {
public:
    void push(LogEntry e);
    void clear();
    void setCapacity(size_t cap);
    size_t size() const;
    void snapshot(std::vector<LogEntry>& out) const; // copy under lock

    static ImGuiLogBuffer& instance();

private:
    mutable std::mutex mtx_;
    std::vector<LogEntry> entries_;
    size_t capacity_ = 2000;
};

// Custom spdlog sink that writes into ImGuiLogBuffer
template <typename Mutex>
class imgui_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        LogEntry e;
        e.level = msg.level;
        e.time = std::chrono::system_clock::now();
        e.message.assign(formatted.data(), formatted.size());
        ImGuiLogBuffer::instance().push(std::move(e));
    }
    void flush_() override {}
};

using imgui_sink_mt = imgui_sink<std::mutex>;

// Helper to create the sink shared_ptr
std::shared_ptr<spdlog::sinks::sink> create_imgui_sink();

} // namespace gb2d::logging
