// SPDX-License-Identifier: Apache-2.0
#include "TimedMotor.h"

#include <aidl/android/hardware/vibrator/BnVibrator.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using namespace aidl::android::hardware::vibrator;
using Status = ndk::ScopedAStatus;

static constexpr const char* kMotor = "/sys/class/meizu/motor/";

static bool writeNode(const char* node, const std::string& value) {
    const std::string path = std::string(kMotor) + node;
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "m2391-vibrator", "open %s: %s",
                            path.c_str(), strerror(errno));
        return false;
    }
    ssize_t result;
    do { result = write(fd, value.data(), value.size()); } while (result < 0 && errno == EINTR);
    if (result != static_cast<ssize_t>(value.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "m2391-vibrator", "write %s failed: %s",
                            path.c_str(), result < 0 ? strerror(errno) : "short write");
    }
    close(fd);
    return result == static_cast<ssize_t>(value.size());
}

class Vibrator final : public BnVibrator {
public:
    Vibrator() : motor_(writeNode) { motor_.stop(); }
    Status getCapabilities(int32_t* result) override {
        *result = CAP_ON_CALLBACK | CAP_PERFORM_CALLBACK;
        return Status::ok();
    }
    Status off() override { return motor_.stop() ? Status::ok() : failure(); }
    Status on(int32_t duration, const std::shared_ptr<IVibratorCallback>& callback) override {
        if (duration <= 0) return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        return motor_.play(duration, false, completion(callback)) ? Status::ok() : failure();
    }
    Status perform(Effect effect, EffectStrength strength,
                   const std::shared_ptr<IVibratorCallback>& callback, int32_t* duration) override {
        *duration = 0;
        if (strength != EffectStrength::LIGHT && strength != EffectStrength::MEDIUM &&
            strength != EffectStrength::STRONG) return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        if (effect != Effect::CLICK && effect != Effect::TICK) return unsupported();
        constexpr int kTapDurationMs = 50;
        if (!motor_.play(kTapDurationMs, true, completion(callback))) return failure();
        *duration = kTapDurationMs;
        return Status::ok();
    }
    Status getSupportedEffects(std::vector<Effect>* result) override {
        *result = {Effect::CLICK, Effect::TICK};
        return Status::ok();
    }
    Status getSupportedPrimitives(std::vector<CompositePrimitive>* result) override {
        result->clear(); return Status::ok();
    }
    Status getSupportedAlwaysOnEffects(std::vector<Effect>* result) override {
        result->clear(); return Status::ok();
    }
    Status getSupportedBraking(std::vector<Braking>* result) override {
        result->clear(); return unsupported();
    }
    Status setAmplitude(float) override { return unsupported(); }
    Status setExternalControl(bool) override { return unsupported(); }
    Status getCompositionDelayMax(int32_t*) override { return unsupported(); }
    Status getCompositionSizeMax(int32_t*) override { return unsupported(); }
    Status getPrimitiveDuration(CompositePrimitive, int32_t*) override { return unsupported(); }
    Status compose(const std::vector<CompositeEffect>&,
                   const std::shared_ptr<IVibratorCallback>&) override { return unsupported(); }
    Status alwaysOnEnable(int32_t, Effect, EffectStrength) override { return unsupported(); }
    Status alwaysOnDisable(int32_t) override { return unsupported(); }
    Status getResonantFrequency(float*) override { return unsupported(); }
    Status getQFactor(float*) override { return unsupported(); }
    Status getFrequencyResolution(float*) override { return unsupported(); }
    Status getFrequencyMinimum(float*) override { return unsupported(); }
    Status getBandwidthAmplitudeMap(std::vector<float>*) override { return unsupported(); }
    Status getPwlePrimitiveDurationMax(int32_t*) override { return unsupported(); }
    Status getPwleCompositionSizeMax(int32_t*) override { return unsupported(); }
    Status composePwle(const std::vector<PrimitivePwle>&,
                       const std::shared_ptr<IVibratorCallback>&) override { return unsupported(); }
private:
    static Status unsupported() { return Status::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }
    static Status failure() { return Status::fromServiceSpecificError(EIO); }
    static std::function<void()> completion(const std::shared_ptr<IVibratorCallback>& callback) {
        return [callback] { if (callback) callback->onComplete(); };
    }
    TimedMotor motor_;
};

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(2);
    auto vibrator = ndk::SharedRefBase::make<Vibrator>();
    const std::string instance = std::string(IVibrator::descriptor) + "/default";
    if (AServiceManager_addService(vibrator->asBinder().get(), instance.c_str()) != STATUS_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "m2391-vibrator", "service registration failed");
        return 1;
    }
    __android_log_print(ANDROID_LOG_INFO, "m2391-vibrator", "registered; AW8697 mBack backend");
    ABinderProcess_joinThreadPool();
    return 1;
}
