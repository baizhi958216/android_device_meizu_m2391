// SPDX-License-Identifier: Apache-2.0
#include "Transmit.h"

#include <aidl/android/hardware/ir/BnConsumerIr.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>
#include <fcntl.h>
#include <linux/lirc.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#include <mutex>
#include <string>

using namespace aidl::android::hardware::ir;
using Status = ndk::ScopedAStatus;

namespace {

class LircIo {
public:
    int open() {
        int fd;
        do { fd = ::open("/dev/lirc0", O_WRONLY | O_CLOEXEC); } while (fd < 0 && errno == EINTR);
        return fd < 0 ? -errno : fd;
    }
    int setCarrier(int fd, int32_t carrier) {
        int result;
        do { result = ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier); } while (result < 0 && errno == EINTR);
        return result < 0 ? -errno : 0;
    }
    ssize_t write(int fd, const int32_t* pattern, size_t bytes) {
        // Never retry an interrupted waveform: some pulses may already have
        // reached the receiver even if the driver returns an error.
        const ssize_t result = ::write(fd, pattern, bytes);
        return result < 0 ? -errno : result;
    }
    void close(int fd) { ::close(fd); }
    int waitGap(int32_t microseconds) {
        struct timespec gap = {microseconds / 1000000, (microseconds % 1000000) * 1000L};
        while (nanosleep(&gap, &gap) < 0) {
            if (errno != EINTR) return -errno;
        }
        return 0;
    }
};

class ConsumerIr final : public BnConsumerIr {
public:
    Status getCarrierFreqs(std::vector<ConsumerIrFreqRange>* ranges) override {
        ConsumerIrFreqRange carrier36;
        carrier36.minHz = carrier36.maxHz = 36000;
        ConsumerIrFreqRange carrier38;
        carrier38.minHz = 37001;
        carrier38.maxHz = 38999;
        *ranges = {carrier36, carrier38};
        return Status::ok();
    }

    Status transmit(int32_t carrier, const std::vector<int32_t>& pattern) override {
        // Carrier programming, write and the trailing off interval form one
        // transaction, even when Binder dispatches calls on multiple threads.
        std::lock_guard<std::mutex> guard(lock_);
        const int result = m2391::ir::transmitLirc(io_, carrier, pattern);
        if (result == 0) return Status::ok();
        __android_log_print(ANDROID_LOG_ERROR, "m2391-ir", "transmit %d Hz (%zu entries): %s",
                            carrier, pattern.size(), strerror(-result));
        if (result == -EOPNOTSUPP) return Status::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        return Status::fromServiceSpecificError(-result);
    }

private:
    std::mutex lock_;
    LircIo io_;
};

}  // namespace

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    const auto service = ndk::SharedRefBase::make<ConsumerIr>();
    const std::string name = std::string(IConsumerIr::descriptor) + "/default";
    if (AServiceManager_addService(service->asBinder().get(), name.c_str()) != STATUS_OK) return 1;
    ABinderProcess_joinThreadPool();
    return 1;
}
