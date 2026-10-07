// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Serialize sysfs writes and cancel the old deadline when a new request arrives.
class TimedMotor {
public:
    using Writer = std::function<bool(const char*, const std::string&)>;
    struct Tap {
        int delayMs;
        int durationMs;
        int level;
        int gain;
    };
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
    // Levels 0/1/2 select the finite RAM waves 16/17/18.
    // A negative level selects continuous playback for sustained requests.
    bool play(int milliseconds, int level, int gain, std::function<void()> complete) {
        if (milliseconds <= 0 || level < -1 || level > 2 || gain < 1 || gain > 128) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = false;
        callback_ = {};
        taps_.clear();
        waitingForTap_ = false;
        ++generation_;
        // Both start controls stop previous playback internally. Extra stop
        // writes here add I2C/standby waits to every key press.
        active_ = true;
        continuous_ = level < 0;
        if (!writer_(continuous_ ? "cont" : "set_mback",
                     continuous_ ? "1" : std::to_string(level) + " 1") ||
            // The path can reset gain, so apply it after start.
            !writer_("gain", std::to_string(gain))) {
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
    bool setAmplitude(float amplitude) {
        std::lock_guard<std::mutex> lock(mutex_);
        const int gain = std::max(1, static_cast<int>(amplitude * 128.0f + 0.5f));
        return writer_("gain", std::to_string(gain));
    }
    bool compose(std::vector<Tap> taps, std::function<void()> complete) {
        if (taps.empty() || taps.size() > 64) return false;
        for (const auto& tap : taps) {
            if (tap.delayMs < 0 || tap.delayMs > 1000 || tap.durationMs < 0 ||
                tap.level < 0 || tap.level > 2 || tap.gain < 0 || tap.gain > 128) return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        pending_ = false;
        callback_ = {};
        taps_ = std::move(taps);
        tapIndex_ = 0;
        continuous_ = false;
        waitingForTap_ = taps_[0].delayMs > 0;
        if (waitingForTap_) {
            if (!stopHardware()) { taps_.clear(); changed_.notify_all(); return false; }
            deadline_ = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(taps_[0].delayMs);
        } else if (!startTap()) {
            stopHardware();
            taps_.clear();
            changed_.notify_all();
            return false;
        }
        callback_ = std::move(complete);
        pending_ = true;
        changed_.notify_all();
        return true;
    }
    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        // Cancel legacy queued playback once when taking ownership.
        const bool legacyStopped = writer_("enable", "0");
        const bool stopped = writer_("cont", "0");
        active_ = !(legacyStopped && stopped);
        return !active_;
    }
    bool stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        pending_ = false;
        callback_ = {};
        taps_.clear();
        changed_.notify_all();
        return stopHardware();
    }
private:
    bool startTap() {
        const auto& tap = taps_[tapIndex_];
        if (tap.gain == 0) {
            if (!stopHardware()) return false;
        } else {
            active_ = true;
            if (!writer_("set_mback", std::to_string(tap.level) + " 1") ||
                !writer_("gain", std::to_string(tap.gain))) return false;
        }
        deadline_ = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(tap.durationMs);
        return true;
    }
    bool stopHardware() {
        if (!active_) return true;
        // cont=0 stops the chip in both RAM and continuous playback modes.
        const bool stopped = writer_("cont", "0");
        if (stopped) active_ = false;
        return stopped;
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
            bool stopped = true;
            if (waitingForTap_) {
                waitingForTap_ = false;
                if (startTap()) continue;
                // A delayed hardware failure ends the composition, rather
                // than leaving the framework waiting for a missing callback.
                stopped = stopHardware();
                taps_.clear();
            }
            // Finite RAM waves stop themselves; preserve their complete tail.
            stopped = stopped && (!continuous_ || stopHardware());
            if (stopped) active_ = false;
            if (!taps_.empty() && ++tapIndex_ < taps_.size()) {
                waitingForTap_ = true;
                deadline_ = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(taps_[tapIndex_].delayMs);
                continue;
            }
            taps_.clear();
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
    bool active_ = false;
    bool continuous_ = false;
    unsigned long long generation_ = 0;
    std::chrono::steady_clock::time_point deadline_;
    std::function<void()> callback_;
    std::vector<Tap> taps_;
    size_t tapIndex_ = 0;
    bool waitingForTap_ = false;
    std::thread worker_;
};
