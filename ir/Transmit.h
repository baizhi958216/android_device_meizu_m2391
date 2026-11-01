// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2391::ir {

inline constexpr size_t kLircBufferSize = 1024;
inline constexpr int64_t kLircMaxDurationUs = 500000;
inline constexpr int64_t kMaxTransmitDurationUs = 2000000;

// Match the two carrier settings implemented by pwm_ir, including its 38 kHz
// tolerance. Do not advertise the OEM HAL's unsupported 20-60 kHz interval.
inline bool supportsCarrier(int32_t carrier) {
    return carrier == 36000 || (carrier > 37000 && carrier < 39000);
}

// Io returns negative errno on failure. A LIRC write is one complete pulse
// train: a short write must not be resumed as a second, unrelated IR command.
template <typename Io>
int transmitLirc(Io& io, int32_t carrier, const std::vector<int32_t>& pattern) {
    if (!supportsCarrier(carrier)) return -EOPNOTSUPP;
    if (pattern.empty()) return -EINVAL;

    const bool hasTrailingSpace = pattern.size() % 2 == 0;
    const size_t pulses = pattern.size() - (hasTrailingSpace ? 1 : 0);
    if (pulses > kLircBufferSize) return -EINVAL;
    int64_t duration = 0, total = 0;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] <= 0) return -EINVAL;
        total += pattern[i];
        if (i < pulses) duration += pattern[i];
    }
    // LIRC limits the written pulse train to IR_MAX_DURATION; the framework
    // limits the entire request, including its final off interval, to 2 s.
    if (duration > kLircMaxDurationUs || total > kMaxTransmitDurationUs) return -EINVAL;

    const int fd = io.open();
    if (fd < 0) return fd;
    struct Close {
        Io& io;
        int fd;
        ~Close() { io.close(fd); }
    } close{io, fd};

    int result = io.setCarrier(fd, carrier);
    if (result < 0) return result;
    const size_t bytes = pulses * sizeof(int32_t);
    const auto written = io.write(fd, pattern.data(), bytes);
    if (written < 0) return static_cast<int>(written);
    if (static_cast<size_t>(written) != bytes) return -EIO;

    // LIRC requires the last entry to be a pulse. Preserve the AIDL trailing
    // off interval in userspace, before returning or starting another command.
    if (hasTrailingSpace) return io.waitGap(pattern.back());
    return 0;
}

}  // namespace m2391::ir
