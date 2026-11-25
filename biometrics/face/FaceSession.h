// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CameraCapture.h"
#include "vendor/xingji/hardware/biometrics/face/1.0/IBiometricsFace.h"

#include <aidl/android/hardware/biometrics/common/BnCancellationSignal.h>
#include <aidl/android/hardware/biometrics/face/BnSession.h>
#include <aidl/android/hardware/biometrics/face/FaceEnrollOptions.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>
#include <android/hardware/biometrics/face/1.0/IBiometricsFace.h>
#include <android/hardware/biometrics/face/1.0/IBiometricsFaceClientCallback.h>
#include <utils/StrongPointer.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace aidl::android::hardware::biometrics::face {

namespace common = aidl::android::hardware::biometrics::common;
namespace keymaster = aidl::android::hardware::keymaster;
using aidl::android::hardware::common::NativeHandle;

class FaceSession;

class HidlFaceCallback : public ::android::hardware::biometrics::face::V1_0::IBiometricsFaceClientCallback {
  public:
    explicit HidlFaceCallback(std::weak_ptr<FaceSession> session);

    ::android::hardware::Return<void> onEnrollResult(
            uint64_t deviceId, uint32_t faceId, int32_t userId, uint32_t remaining) override;
    ::android::hardware::Return<void> onAuthenticated(
            uint64_t deviceId,
            uint32_t faceId,
            int32_t userId,
            const ::android::hardware::hidl_vec<uint8_t>& token) override;
    ::android::hardware::Return<void> onAcquired(
            uint64_t deviceId,
            int32_t userId,
            ::android::hardware::biometrics::face::V1_0::FaceAcquiredInfo acquiredInfo,
            int32_t vendorCode) override;
    ::android::hardware::Return<void> onError(
            uint64_t deviceId,
            int32_t userId,
            ::android::hardware::biometrics::face::V1_0::FaceError error,
            int32_t vendorCode) override;
    ::android::hardware::Return<void> onRemoved(
            uint64_t deviceId,
            const ::android::hardware::hidl_vec<uint32_t>& removed,
            int32_t userId) override;
    ::android::hardware::Return<void> onEnumerate(
            uint64_t deviceId,
            const ::android::hardware::hidl_vec<uint32_t>& faceIds,
            int32_t userId) override;
    ::android::hardware::Return<void> onLockoutChanged(uint64_t duration) override;

  private:
    std::weak_ptr<FaceSession> mSession;
};

class FaceSession : public BnSession {
  public:
    FaceSession(int32_t sensorId, int32_t userId, std::shared_ptr<ISessionCallback> cb);
    ~FaceSession() override;

    bool init();

    ndk::ScopedAStatus generateChallenge() override;
    ndk::ScopedAStatus revokeChallenge(int64_t challenge) override;
    ndk::ScopedAStatus getEnrollmentConfig(
            EnrollmentType enrollmentType,
            std::vector<EnrollmentStageConfig>* return_val) override;
    ndk::ScopedAStatus enroll(
            const keymaster::HardwareAuthToken& hat,
            EnrollmentType enrollmentType,
            const std::vector<Feature>& features,
            const std::optional<NativeHandle>& previewSurface,
            std::shared_ptr<common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus authenticate(
            int64_t keystoreOperationId,
            std::shared_ptr<common::ICancellationSignal>* returnVal) override;
    ndk::ScopedAStatus detectInteraction(
            std::shared_ptr<common::ICancellationSignal>* returnVal) override;
    ndk::ScopedAStatus enumerateEnrollments() override;
    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& enrollmentIds) override;
    ndk::ScopedAStatus getFeatures() override;
    ndk::ScopedAStatus setFeature(
            const keymaster::HardwareAuthToken& hat, Feature feature, bool enabled) override;
    ndk::ScopedAStatus getAuthenticatorId() override;
    ndk::ScopedAStatus invalidateAuthenticatorId() override;
    ndk::ScopedAStatus resetLockout(const keymaster::HardwareAuthToken& hat) override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus authenticateWithContext(
            int64_t operationId,
            const common::OperationContext& context,
            std::shared_ptr<common::ICancellationSignal>* out) override;
    ndk::ScopedAStatus enrollWithContext(
            const keymaster::HardwareAuthToken& hat,
            EnrollmentType enrollmentType,
            const std::vector<Feature>& features,
            const std::optional<NativeHandle>& previewSurface,
            const common::OperationContext& context,
            std::shared_ptr<common::ICancellationSignal>* out) override;
    ndk::ScopedAStatus detectInteractionWithContext(
            const common::OperationContext& context,
            std::shared_ptr<common::ICancellationSignal>* out) override;
    ndk::ScopedAStatus onContextChanged(const common::OperationContext& context) override;
    ndk::ScopedAStatus enrollWithOptions(
            const FaceEnrollOptions& options,
            std::shared_ptr<common::ICancellationSignal>* out) override;

    void cancelActiveOperation();

    // Callbacks forwarded from HidlFaceCallback
    void handleEnrollResult(uint32_t faceId, int32_t userId, uint32_t remaining);
    void handleAuthenticated(uint32_t faceId, int32_t userId, const std::vector<uint8_t>& token);
    void handleAcquired(
            ::android::hardware::biometrics::face::V1_0::FaceAcquiredInfo acquiredInfo,
            int32_t vendorCode);
    void handleError(
            ::android::hardware::biometrics::face::V1_0::FaceError error, int32_t vendorCode);
    void handleRemoved(const std::vector<uint32_t>& removed);
    void handleEnumerate(const std::vector<uint32_t>& faceIds);
    void handleLockoutChanged(uint64_t duration);

  private:
    enum class OpMode {
        NONE,
        ENROLL,
        AUTHENTICATE,
    };

    bool ensureHidlConnectedLocked();
    static std::vector<uint8_t> serializeHat(const keymaster::HardwareAuthToken& hat);
    static ::vendor::xingji::hardware::biometrics::face::V1_0::FaceFormat resolveFormat();
    static ::vendor::xingji::hardware::biometrics::face::V1_0::FaceOrientation resolveOrientation(
            int32_t sensorOrientation);
    static AcquiredInfo mapAcquiredInfo(
            ::android::hardware::biometrics::face::V1_0::FaceAcquiredInfo info);
    static Error mapError(::android::hardware::biometrics::face::V1_0::FaceError err);

    ndk::ScopedAStatus startEnrollInternal(
            const keymaster::HardwareAuthToken& hat,
            const std::vector<Feature>& features,
            ANativeWindow* previewWindow,
            std::shared_ptr<common::ICancellationSignal>* out);

    int32_t mSensorId;
    int32_t mUserId;
    std::shared_ptr<ISessionCallback> mCb;

    std::mutex mMutex;
    ::android::sp<::android::hardware::biometrics::face::V1_0::IBiometricsFace> mAospFace;
    ::android::sp<::vendor::xingji::hardware::biometrics::face::V1_0::IBiometricsFace> mVendorFace;
    ::android::sp<HidlFaceCallback> mHidlCb;

    CameraCapture mCamera;
    std::atomic<OpMode> mOpMode{OpMode::NONE};
    std::atomic<bool> mCancelled{false};
    std::atomic<bool> mClosed{false};
    int32_t mCurrentFaceId{0};
};

}  // namespace aidl::android::hardware::biometrics::face
