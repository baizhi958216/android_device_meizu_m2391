// SPDX-License-Identifier: Apache-2.0
#include "TimedMotor.h"

#include <aidl/android/hardware/vibrator/BnVibrator.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>
#include <cerrno>
#include <cmath>
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
    Vibrator() : motor_(writeNode) { motor_.initialize(); }
    Status getCapabilities(int32_t* result) override {
        *result = CAP_ON_CALLBACK | CAP_PERFORM_CALLBACK | CAP_AMPLITUDE_CONTROL |
                  CAP_COMPOSE_EFFECTS;
        return Status::ok();
    }
    Status off() override { return motor_.stop() ? Status::ok() : failure(); }
    Status on(int32_t duration, const std::shared_ptr<IVibratorCallback>& callback) override {
        if (duration <= 0) return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        // Legacy keyboards issue short timed requests instead of perform().
        // Use a single click for these rather than a continuous buzz.
        const bool shortFeedback = duration <= 50;
        return motor_.play(shortFeedback ? std::max(duration, kTapDurationMs) : duration,
                           shortFeedback ? 1 : -1, 128, completion(callback))
                       ? Status::ok() : failure();
    }
    Status perform(Effect effect, EffectStrength strength,
                   const std::shared_ptr<IVibratorCallback>& callback, int32_t* duration) override {
        *duration = 0;
        if (strength != EffectStrength::LIGHT && strength != EffectStrength::MEDIUM &&
            strength != EffectStrength::STRONG) return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        int level;
        int baseGain;
        switch (effect) {
            case Effect::CLICK:
                level = strength == EffectStrength::LIGHT ? 0 :
                        strength == EffectStrength::STRONG ? 2 : 1;
                baseGain = 128;
                break;
            case Effect::HEAVY_CLICK: level = 2; baseGain = 128; break;
            case Effect::TICK: level = 0; baseGain = 128; break;
            case Effect::TEXTURE_TICK: level = 0; baseGain = 64; break;
            default: return unsupported();
        }
        const float scale = strength == EffectStrength::LIGHT ? 0.5f : 1.0f;
        const int gain = static_cast<int>(baseGain * scale);
        if (!motor_.play(kTapDurationMs, level, gain, completion(callback))) return failure();
        *duration = kTapDurationMs;
        return Status::ok();
    }
    Status getSupportedEffects(std::vector<Effect>* result) override {
        *result = {Effect::CLICK, Effect::TICK, Effect::TEXTURE_TICK, Effect::HEAVY_CLICK};
        return Status::ok();
    }
    Status getSupportedPrimitives(std::vector<CompositePrimitive>* result) override {
        *result = {CompositePrimitive::NOOP, CompositePrimitive::CLICK,
                   CompositePrimitive::LIGHT_TICK, CompositePrimitive::LOW_TICK};
        return Status::ok();
    }
    Status getSupportedAlwaysOnEffects(std::vector<Effect>* result) override {
        result->clear(); return Status::ok();
    }
    Status getSupportedBraking(std::vector<Braking>* result) override {
        result->clear(); return unsupported();
    }
    Status setAmplitude(float amplitude) override {
        if (!std::isfinite(amplitude) || amplitude <= 0.0f || amplitude > 1.0f)
            return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        return motor_.setAmplitude(amplitude) ? Status::ok() : failure();
    }
    Status setExternalControl(bool) override { return unsupported(); }
    Status getCompositionDelayMax(int32_t* result) override { *result = 1000; return Status::ok(); }
    Status getCompositionSizeMax(int32_t* result) override { *result = 64; return Status::ok(); }
    Status getPrimitiveDuration(CompositePrimitive primitive, int32_t* result) override {
        *result = 0;
        if (primitive == CompositePrimitive::NOOP) return Status::ok();
        if (primitive != CompositePrimitive::CLICK && primitive != CompositePrimitive::LIGHT_TICK &&
            primitive != CompositePrimitive::LOW_TICK) return unsupported();
        *result = kTapDurationMs;
        return Status::ok();
    }
    Status compose(const std::vector<CompositeEffect>& effects,
                   const std::shared_ptr<IVibratorCallback>& callback) override {
        if (effects.empty() || effects.size() > 64)
            return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        std::vector<TimedMotor::Tap> taps;
        for (const auto& effect : effects) {
            if (effect.delayMs < 0 || effect.delayMs > 1000 || !std::isfinite(effect.scale) ||
                effect.scale < 0.0f || effect.scale > 1.0f)
                return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
            int level = 0;
            int baseGain;
            switch (effect.primitive) {
                case CompositePrimitive::NOOP: baseGain = 0; break;
                case CompositePrimitive::CLICK: level = 1; baseGain = 128; break;
                case CompositePrimitive::LIGHT_TICK: baseGain = 96; break;
                case CompositePrimitive::LOW_TICK: baseGain = 64; break;
                default: return unsupported();
            }
            const int gain = effect.scale == 0.0f || baseGain == 0 ? 0 :
                             std::max(1, static_cast<int>(baseGain * effect.scale + 0.5f));
            taps.push_back({effect.delayMs, baseGain == 0 ? 0 : kTapDurationMs, level, gain});
        }
        return motor_.compose(std::move(taps), completion(callback)) ? Status::ok() : failure();
    }
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
    // Shipped 170 Hz RAM mBack waves: 232 samples at 24 kHz (9.67 ms).
    // Allow 15 ms for completion instead of a fixed 50 ms delay.
    static constexpr int kTapDurationMs = 15;
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
