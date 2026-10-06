// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

// Serialize sysfs writes and cancel the old deadline when a new request arrives.
class TimedMotor {
public:
    using Writer = std::function<bool(const char*, const std::string&)>;
    explicit TimedMotor(Writer writer) : writer_(std::move(writer)), worker_([this] { loop(); }) {}
    ~TimedMotor() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quit_ = true;
            pending_ = false;
            stopHardware();
        }
        changed_.notify_all();
        worker_.join();
    }
    bool play(int milliseconds, bool tap, std::function<void()> complete) {
        if (milliseconds <= 0) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = false;
        callback_ = {};
        ++generation_;
        // enable loops the last RAM effect, which can be a short click.
        // cont drives the resonant motor continuously until this timer stops it.
        if (!stopHardware() ||
            !writer_(tap ? "set_mback" : "cont", tap ? "1 1" : "1")) {
            stopHardware();
            changed_.notify_all();
            return false;
        }
        deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        callback_ = std::move(complete);
        pending_ = true;
        changed_.notify_all();
        return true;
    }
    bool stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        pending_ = false;
        callback_ = {};
        changed_.notify_all();
        return stopHardware();
    }
private:
    bool stopHardware() {
        // Attempt both writes even if one fails: either backend may be active.
        const bool continuousStopped = writer_("cont", "0");
        const bool effectStopped = writer_("enable", "0");
        return continuousStopped && effectStopped;
    }
    void loop() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!quit_) {
            changed_.wait(lock, [this] { return quit_ || pending_; });
            if (quit_) break;
            const auto generation = generation_;
            if (changed_.wait_until(lock, deadline_, [this, generation] {
                    return quit_ || !pending_ || generation_ != generation;
                })) continue;
            const bool stopped = stopHardware();
            pending_ = false;
            auto callback = std::move(callback_);
            lock.unlock();
            if (stopped && callback) callback();
            lock.lock();
        }
    }
    Writer writer_;
    std::mutex mutex_;
    std::condition_variable changed_;
    bool quit_ = false;
    bool pending_ = false;
    unsigned long long generation_ = 0;
    std::chrono::steady_clock::time_point deadline_;
    std::function<void()> callback_;
    std::thread worker_;
};
