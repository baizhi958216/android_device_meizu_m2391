// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <thread>

// The stock Meizu timed path loops the calibrated single-cycle RAM wave 19.
// Its enable store queues work. Wait for that work to program the sequence
// before allowing an amplitude update, otherwise its initial gain can win.
class RamLoop {
public:
    using Writer = std::function<bool(const char*, const std::string&)>;
    using IndexReader = std::function<int()>;
    static bool start(const Writer& write, const IndexReader& readIndex, int gain) {
        if (gain < 1 || gain > 128) return false;
        // enable=0 drains legacy work as well as stopping the chip. Clear the
        // sequence to make the readback a new completion, not stale wave 19.
        if (!write("enable", "0") || !write("seq", "0 0") ||
            !write("waveform", "19") ||
            !write("enable", "1 " + std::to_string(gain) + " 0")) {
            write("enable", "0");
            return false;
        }
        const auto limit = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        do {
            // index_show uses the same mutex as timeout_play_routine. Seeing
            // 19 therefore also waits for its remaining gain and GO writes.
            const int index = readIndex();
            if (index == 19) return true;
            if (index < 0) break;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        } while (std::chrono::steady_clock::now() < limit);
        // Drain even if scheduling or readback failed: no queued restart may
        // survive a failed on() and its framework cancellation.
        write("enable", "0");
        return false;
    }
};
