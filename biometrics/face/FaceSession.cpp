// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "m2391.face.session"

#include "FaceSession.h"

#include <aidl/android/hardware/biometrics/face/AuthenticateSuccess.h>
#include <aidl/android/hardware/biometrics/face/AuthenticationFrame.h>
#include <aidl/android/hardware/biometrics/face/BaseFrame.h>
#include <aidl/android/hardware/biometrics/face/EnrollmentFrame.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace aidl::android::hardware::biometrics::face {

namespace {

using HidlFace = ::android::hardware::biometrics::face::V1_0::IBiometricsFace;
using HidlStatus = ::android::hardware::biometrics::face::V1_0::Status;
using HidlAcquired = ::android::hardware::biometrics::face::V1_0::FaceAcquiredInfo;
using HidlError = ::android::hardware::biometrics::face::V1_0::FaceError;
using VendorFace = ::vendor::xingji::hardware::biometrics::face::V1_0::IBiometricsFace;
using VendorFeature = ::vendor::xingji::hardware::biometrics::face::V1_0::Feature;
using VendorFormat = ::vendor::xingji::hardware::biometrics::face::V1_0::FaceFormat;
using VendorOrientation = ::vendor::xingji::hardware::biometrics::face::V1_0::FaceOrientation;

constexpr uint32_t kEnrollTimeoutSec = 60;

uint64_t htonll_u64(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (static_cast<uint64_t>(htonl(static_cast<uint32_t>(v & 0xffffffffULL))) << 32) |
           htonl(static_cast<uint32_t>(v >> 32));
#else
    return v;
#endif
}

class SessionCancellationSignal : public common::BnCancellationSignal {
  public:
    explicit SessionCancellationSignal(std::weak_ptr<FaceSession> session)
        : mSession(std::move(session)) {}

    ndk::ScopedAStatus cancel() override {
        if (auto s = mSession.lock()) {
            s->cancelActiveOperation();
        }
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::weak_ptr<FaceSession> mSession;
};

}  // namespace

HidlFaceCallback::HidlFaceCallback(std::weak_ptr<FaceSession> session)
    : mSession(std::move(session)) {}

::android::hardware::Return<void> HidlFaceCallback::onEnrollResult(
        uint64_t /*deviceId*/, uint32_t faceId, int32_t userId, uint32_t remaining) {
    if (auto s = mSession.lock()) {
        s->handleEnrollResult(faceId, userId, remaining);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onAuthenticated(
        uint64_t /*deviceId*/,
        uint32_t faceId,
        int32_t userId,
        const ::android::hardware::hidl_vec<uint8_t>& token) {
    if (auto s = mSession.lock()) {
        std::vector<uint8_t> vec(token.begin(), token.end());
        s->handleAuthenticated(faceId, userId, vec);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onAcquired(
        uint64_t /*deviceId*/, int32_t /*userId*/, HidlAcquired acquiredInfo, int32_t vendorCode) {
    if (auto s = mSession.lock()) {
        s->handleAcquired(acquiredInfo, vendorCode);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onError(
        uint64_t /*deviceId*/, int32_t /*userId*/, HidlError error, int32_t vendorCode) {
    if (auto s = mSession.lock()) {
        s->handleError(error, vendorCode);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onRemoved(
        uint64_t /*deviceId*/,
        const ::android::hardware::hidl_vec<uint32_t>& removed,
        int32_t /*userId*/) {
    if (auto s = mSession.lock()) {
        std::vector<uint32_t> vec(removed.begin(), removed.end());
        s->handleRemoved(vec);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onEnumerate(
        uint64_t /*deviceId*/,
        const ::android::hardware::hidl_vec<uint32_t>& faceIds,
        int32_t /*userId*/) {
    if (auto s = mSession.lock()) {
        std::vector<uint32_t> vec(faceIds.begin(), faceIds.end());
        s->handleEnumerate(vec);
    }
    return ::android::hardware::Void();
}

::android::hardware::Return<void> HidlFaceCallback::onLockoutChanged(uint64_t duration) {
    if (auto s = mSession.lock()) {
        s->handleLockoutChanged(duration);
    }
    return ::android::hardware::Void();
}

FaceSession::FaceSession(
        int32_t sensorId, int32_t userId, std::shared_ptr<ISessionCallback> cb)
    : mSensorId(sensorId), mUserId(userId), mCb(std::move(cb)) {}

FaceSession::~FaceSession() {
    mCamera.stop();
}

bool FaceSession::init() {
    std::lock_guard<std::mutex> lock(mMutex);
    return ensureHidlConnectedLocked();
}

bool FaceSession::ensureHidlConnectedLocked() {
    if (mAospFace != nullptr && mVendorFace != nullptr) {
        if (mAospFace->ping().isOk() && mVendorFace->ping().isOk()) {
            return true;
        }
        mAospFace = nullptr;
        mVendorFace = nullptr;
    }

    for (int attempt = 0; attempt < 5; ++attempt) {
        mAospFace = HidlFace::tryGetService();
        mVendorFace = VendorFace::tryGetService();
        if (mAospFace != nullptr && mVendorFace != nullptr) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (mAospFace == nullptr || mVendorFace == nullptr) {
        LOG(ERROR) << "Failed to bind vendor face HIDL services (aosp="
                   << (mAospFace != nullptr) << ", ext=" << (mVendorFace != nullptr) << ")";
        return false;
    }

    mHidlCb = new HidlFaceCallback(std::weak_ptr<FaceSession>(ref<FaceSession>()));
    auto cbRet = mAospFace->setCallback(mHidlCb, [](const auto& res) {
        LOG(INFO) << "Stock HIDL setCallback status=" << static_cast<int>(res.status)
                  << " halId=" << res.value;
    });
    if (!cbRet.isOk()) {
        LOG(ERROR) << "Stock HIDL setCallback transport error: " << cbRet.description();
        return false;
    }

    std::string storePath =
            ::android::base::StringPrintf("/data/vendor_de/%d/facedata", mUserId);
    auto userRet = mAospFace->setActiveUser(mUserId, storePath);
    LOG(INFO) << "Stock HIDL setActiveUser(sensorId=" << mSensorId << ", userId=" << mUserId
              << ", " << storePath << ") status="
              << (userRet.isOk() ? static_cast<int>(static_cast<HidlStatus>(userRet)) : -1);
    return true;
}

std::vector<uint8_t> FaceSession::serializeHat(const keymaster::HardwareAuthToken& hat) {
    // Standard 69-byte hw_auth_token_t layout:
    // version(1) + challenge(8) + user_id(8) + authenticator_id(8) +
    // authenticator_type(4 BE) + timestamp(8 BE) + hmac(32)
    std::vector<uint8_t> out(69, 0);
    size_t off = 0;
    out[off++] = 0;  // HW_AUTH_TOKEN_VERSION
    uint64_t challenge = static_cast<uint64_t>(hat.challenge);
    uint64_t userId = static_cast<uint64_t>(hat.userId);
    uint64_t authId = static_cast<uint64_t>(hat.authenticatorId);
    uint32_t authTypeBe = htonl(static_cast<uint32_t>(hat.authenticatorType));
    uint64_t tsBe = htonll_u64(static_cast<uint64_t>(hat.timestamp.milliSeconds));

    std::memcpy(out.data() + off, &challenge, sizeof(challenge));
    off += sizeof(challenge);
    std::memcpy(out.data() + off, &userId, sizeof(userId));
    off += sizeof(userId);
    std::memcpy(out.data() + off, &authId, sizeof(authId));
    off += sizeof(authId);
    std::memcpy(out.data() + off, &authTypeBe, sizeof(authTypeBe));
    off += sizeof(authTypeBe);
    std::memcpy(out.data() + off, &tsBe, sizeof(tsBe));
    off += sizeof(tsBe);
    if (!hat.mac.empty()) {
        size_t macLen = std::min<size_t>(hat.mac.size(), 32);
        std::memcpy(out.data() + off, hat.mac.data(), macLen);
    }
    return out;
}

VendorFormat FaceSession::resolveFormat() {
    int32_t fmt = ::android::base::GetIntProperty(
            "persist.vendor.m2391.face.format", static_cast<int32_t>(VendorFormat::NV21));
    return static_cast<VendorFormat>(fmt);
}

VendorOrientation FaceSession::resolveOrientation(int32_t sensorOrientation) {
    int32_t overrideVal = ::android::base::GetIntProperty(
            "persist.vendor.m2391.face.orientation", -1);
    if (overrideVal >= 0) {
        return static_cast<VendorOrientation>(overrideVal);
    }
    switch ((sensorOrientation % 360 + 360) % 360) {
        case 90:
            return VendorOrientation::ROTATE_90;
        case 180:
            return VendorOrientation::ROTATE_180;
        case 270:
            return VendorOrientation::ROTATE_270;
        case 0:
        default:
            return VendorOrientation::ROTATE_0;
    }
}

AcquiredInfo FaceSession::mapAcquiredInfo(HidlAcquired info) {
    switch (info) {
        case HidlAcquired::GOOD:
            return AcquiredInfo::GOOD;
        case HidlAcquired::INSUFFICIENT:
            return AcquiredInfo::INSUFFICIENT;
        case HidlAcquired::TOO_BRIGHT:
            return AcquiredInfo::TOO_BRIGHT;
        case HidlAcquired::TOO_DARK:
            return AcquiredInfo::TOO_DARK;
        case HidlAcquired::TOO_CLOSE:
            return AcquiredInfo::TOO_CLOSE;
        case HidlAcquired::TOO_FAR:
            return AcquiredInfo::TOO_FAR;
        case HidlAcquired::FACE_TOO_HIGH:
            return AcquiredInfo::FACE_TOO_HIGH;
        case HidlAcquired::FACE_TOO_LOW:
            return AcquiredInfo::FACE_TOO_LOW;
        case HidlAcquired::FACE_TOO_RIGHT:
            return AcquiredInfo::FACE_TOO_RIGHT;
        case HidlAcquired::FACE_TOO_LEFT:
            return AcquiredInfo::FACE_TOO_LEFT;
        case HidlAcquired::POOR_GAZE:
            return AcquiredInfo::POOR_GAZE;
        case HidlAcquired::NOT_DETECTED:
            return AcquiredInfo::NOT_DETECTED;
        case HidlAcquired::TOO_MUCH_MOTION:
            return AcquiredInfo::TOO_MUCH_MOTION;
        case HidlAcquired::RECALIBRATE:
            return AcquiredInfo::RECALIBRATE;
        case HidlAcquired::TOO_DIFFERENT:
            return AcquiredInfo::TOO_DIFFERENT;
        case HidlAcquired::TOO_SIMILAR:
            return AcquiredInfo::TOO_SIMILAR;
        case HidlAcquired::PAN_TOO_EXTREME:
            return AcquiredInfo::PAN_TOO_EXTREME;
        case HidlAcquired::TILT_TOO_EXTREME:
            return AcquiredInfo::TILT_TOO_EXTREME;
        case HidlAcquired::ROLL_TOO_EXTREME:
            return AcquiredInfo::ROLL_TOO_EXTREME;
        case HidlAcquired::FACE_OBSCURED:
            return AcquiredInfo::FACE_OBSCURED;
        case HidlAcquired::START:
            return AcquiredInfo::START;
        case HidlAcquired::SENSOR_DIRTY:
            return AcquiredInfo::SENSOR_DIRTY;
        case HidlAcquired::VENDOR:
            return AcquiredInfo::VENDOR;
        default:
            return AcquiredInfo::UNKNOWN;
    }
}

Error FaceSession::mapError(HidlError err) {
    switch (err) {
        case HidlError::HW_UNAVAILABLE:
            return Error::HW_UNAVAILABLE;
        case HidlError::UNABLE_TO_PROCESS:
            return Error::UNABLE_TO_PROCESS;
        case HidlError::TIMEOUT:
            return Error::TIMEOUT;
        case HidlError::NO_SPACE:
            return Error::NO_SPACE;
        case HidlError::CANCELED:
            return Error::CANCELED;
        case HidlError::UNABLE_TO_REMOVE:
            return Error::UNABLE_TO_REMOVE;
        case HidlError::LOCKOUT:
            return Error::UNABLE_TO_PROCESS;
        case HidlError::VENDOR:
            return Error::VENDOR;
        case HidlError::LOCKOUT_PERMANENT:
            return Error::UNABLE_TO_PROCESS;
        default:
            return Error::UNKNOWN;
    }
}

ndk::ScopedAStatus FaceSession::generateChallenge() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!ensureHidlConnectedLocked()) {
        mCb->onError(Error::HW_UNAVAILABLE, 0);
        return ndk::ScopedAStatus::ok();
    }
    int64_t challenge = 0;
    HidlStatus st = HidlStatus::INTERNAL_ERROR;
    auto ret = mAospFace->generateChallenge(kEnrollTimeoutSec, [&](const auto& res) {
        st = res.status;
        challenge = static_cast<int64_t>(res.value);
    });
    if (!ret.isOk() || st != HidlStatus::OK) {
        LOG(ERROR) << "generateChallenge failed: status=" << static_cast<int>(st);
        mCb->onError(Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }
    mCb->onChallengeGenerated(challenge);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::revokeChallenge(int64_t challenge) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (ensureHidlConnectedLocked()) {
        auto ret = mAospFace->revokeChallenge();
        if (!ret.isOk()) {
            LOG(WARNING) << "revokeChallenge transport error: " << ret.description();
        }
    }
    // Always complete the challenge revocation so the framework scheduler never stalls.
    mCb->onChallengeRevoked(challenge);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::getEnrollmentConfig(
        EnrollmentType /*enrollmentType*/, std::vector<EnrollmentStageConfig>* return_val) {
    return_val->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::startEnrollInternal(
        const keymaster::HardwareAuthToken& hat,
        const std::vector<Feature>& features,
        ANativeWindow* previewWindow,
        std::shared_ptr<common::ICancellationSignal>* out) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!ensureHidlConnectedLocked()) {
            mCb->onError(Error::HW_UNAVAILABLE, 0);
            *out = ndk::SharedRefBase::make<SessionCancellationSignal>(
                    std::weak_ptr<FaceSession>(ref<FaceSession>()));
            return ndk::ScopedAStatus::ok();
        }
    }

    mCancelled.store(false);
    mOpMode.store(OpMode::ENROLL);

    auto hatBytes = serializeHat(hat);
    ::android::hardware::hidl_vec<uint8_t> hidlHat(hatBytes);
    ::android::hardware::hidl_vec<VendorFeature> hidlFeatures;
    std::vector<VendorFeature> featVec;
    for (auto f : features) {
        if (f == Feature::REQUIRE_ATTENTION) {
            featVec.push_back(VendorFeature::REQUIRE_ATTENTION);
        } else if (f == Feature::REQUIRE_DIVERSE_POSES) {
            featVec.push_back(VendorFeature::REQUIRE_DIVERSITY);
        }
    }
    hidlFeatures = featVec;

    std::weak_ptr<FaceSession> weakSelf(ref<FaceSession>());
    bool started = mCamera.start(
            previewWindow,
            [weakSelf, hidlHat = std::move(hidlHat), hidlFeatures = std::move(hidlFeatures)](
                    const std::vector<uint8_t>& nv21,
                    uint32_t width,
                    uint32_t height,
                    int32_t sensorOrientation) {
                auto self = weakSelf.lock();
                if (!self || self->mCancelled.load() || self->mOpMode.load() != OpMode::ENROLL) {
                    return;
                }
                ::android::sp<VendorFace> vendorFace;
                {
                    std::lock_guard<std::mutex> lock(self->mMutex);
                    vendorFace = self->mVendorFace;
                }
                if (vendorFace == nullptr) {
                    return;
                }
                ::android::hardware::hidl_vec<uint8_t> frameVec;
                frameVec.setToExternal(const_cast<uint8_t*>(nv21.data()), nv21.size());
                auto ret = vendorFace->enroll(
                        hidlHat,
                        kEnrollTimeoutSec,
                        hidlFeatures,
                        frameVec,
                        resolveFormat(),
                        resolveOrientation(sensorOrientation),
                        width,
                        height);
                if (!ret.isOk()) {
                    LOG(ERROR) << "vendorFace->enroll transport failed: " << ret.description();
                }
            });

    if (!started) {
        mOpMode.store(OpMode::NONE);
        mCb->onError(Error::HW_UNAVAILABLE, 0);
    }

    *out = ndk::SharedRefBase::make<SessionCancellationSignal>(
            std::weak_ptr<FaceSession>(ref<FaceSession>()));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::enroll(
        const keymaster::HardwareAuthToken& hat,
        EnrollmentType /*enrollmentType*/,
        const std::vector<Feature>& features,
        const std::optional<NativeHandle>& /*previewSurface*/,
        std::shared_ptr<common::ICancellationSignal>* return_val) {
    return startEnrollInternal(hat, features, nullptr, return_val);
}

ndk::ScopedAStatus FaceSession::enrollWithContext(
        const keymaster::HardwareAuthToken& hat,
        EnrollmentType /*enrollmentType*/,
        const std::vector<Feature>& features,
        const std::optional<NativeHandle>& /*previewSurface*/,
        const common::OperationContext& /*context*/,
        std::shared_ptr<common::ICancellationSignal>* out) {
    return startEnrollInternal(hat, features, nullptr, out);
}

ndk::ScopedAStatus FaceSession::enrollWithOptions(
        const FaceEnrollOptions& options,
        std::shared_ptr<common::ICancellationSignal>* out) {
    ANativeWindow* previewWindow = nullptr;
    if (options.surfacePreview.has_value()) {
        previewWindow = options.surfacePreview->get();
    }
    return startEnrollInternal(options.hardwareAuthToken, options.features, previewWindow, out);
}

ndk::ScopedAStatus FaceSession::authenticate(
        int64_t keystoreOperationId,
        std::shared_ptr<common::ICancellationSignal>* returnVal) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!ensureHidlConnectedLocked()) {
            mCb->onError(Error::HW_UNAVAILABLE, 0);
            *returnVal = ndk::SharedRefBase::make<SessionCancellationSignal>(
                    std::weak_ptr<FaceSession>(ref<FaceSession>()));
            return ndk::ScopedAStatus::ok();
        }
    }

    mCancelled.store(false);
    mOpMode.store(OpMode::AUTHENTICATE);

    std::weak_ptr<FaceSession> weakSelf(ref<FaceSession>());
    uint64_t opId = static_cast<uint64_t>(keystoreOperationId);
    bool started = mCamera.start(
            nullptr,
            [weakSelf, opId](
                    const std::vector<uint8_t>& nv21,
                    uint32_t width,
                    uint32_t height,
                    int32_t sensorOrientation) {
                auto self = weakSelf.lock();
                if (!self || self->mCancelled.load() ||
                    self->mOpMode.load() != OpMode::AUTHENTICATE) {
                    return;
                }
                ::android::sp<VendorFace> vendorFace;
                {
                    std::lock_guard<std::mutex> lock(self->mMutex);
                    vendorFace = self->mVendorFace;
                }
                if (vendorFace == nullptr) {
                    return;
                }
                ::android::hardware::hidl_vec<uint8_t> frameVec;
                frameVec.setToExternal(const_cast<uint8_t*>(nv21.data()), nv21.size());
                auto ret = vendorFace->authenticate(
                        opId,
                        frameVec,
                        resolveFormat(),
                        resolveOrientation(sensorOrientation),
                        width,
                        height);
                if (!ret.isOk()) {
                    LOG(ERROR) << "vendorFace->authenticate transport failed: " << ret.description();
                }
            });

    if (!started) {
        mOpMode.store(OpMode::NONE);
        mCb->onError(Error::HW_UNAVAILABLE, 0);
    }

    *returnVal = ndk::SharedRefBase::make<SessionCancellationSignal>(
            std::weak_ptr<FaceSession>(ref<FaceSession>()));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::authenticateWithContext(
        int64_t operationId,
        const common::OperationContext& /*context*/,
        std::shared_ptr<common::ICancellationSignal>* out) {
    return authenticate(operationId, out);
}

ndk::ScopedAStatus FaceSession::detectInteraction(
        std::shared_ptr<common::ICancellationSignal>* returnVal) {
    mCb->onError(Error::UNABLE_TO_PROCESS, 0);
    *returnVal = ndk::SharedRefBase::make<SessionCancellationSignal>(
            std::weak_ptr<FaceSession>(ref<FaceSession>()));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::detectInteractionWithContext(
        const common::OperationContext& /*context*/,
        std::shared_ptr<common::ICancellationSignal>* out) {
    return detectInteraction(out);
}

ndk::ScopedAStatus FaceSession::enumerateEnrollments() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!ensureHidlConnectedLocked()) {
        mCb->onEnrollmentsEnumerated({});
        return ndk::ScopedAStatus::ok();
    }
    auto ret = mAospFace->enumerate();
    if (!ret.isOk() || static_cast<HidlStatus>(ret) != HidlStatus::OK) {
        LOG(WARNING) << "enumerate failed, returning empty list";
        mCb->onEnrollmentsEnumerated({});
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!ensureHidlConnectedLocked()) {
        mCb->onError(Error::UNABLE_TO_REMOVE, 0);
        return ndk::ScopedAStatus::ok();
    }
    if (enrollmentIds.empty()) {
        mCb->onEnrollmentsRemoved({});
        return ndk::ScopedAStatus::ok();
    }
    for (int32_t id : enrollmentIds) {
        auto ret = mAospFace->remove(static_cast<uint32_t>(id));
        if (!ret.isOk() || static_cast<HidlStatus>(ret) != HidlStatus::OK) {
            LOG(WARNING) << "remove(" << id << ") failed";
            mCb->onError(Error::UNABLE_TO_REMOVE, 0);
            return ndk::ScopedAStatus::ok();
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::getFeatures() {
    std::lock_guard<std::mutex> lock(mMutex);
    // Vendor face HIDL getFeature/setFeature are stubs, while stapp64
    // enforces eye-open attention checks natively during authentication.
    mCb->onFeaturesRetrieved({Feature::REQUIRE_ATTENTION});
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::setFeature(
        const keymaster::HardwareAuthToken& /*hat*/, Feature feature, bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (feature == Feature::REQUIRE_ATTENTION && enabled) {
        mCb->onFeatureSet(feature);
    } else {
        mCb->onError(Error::UNABLE_TO_PROCESS, 0);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::getAuthenticatorId() {
    std::lock_guard<std::mutex> lock(mMutex);
    int64_t authId = 0;
    if (ensureHidlConnectedLocked()) {
        auto ret = mAospFace->getAuthenticatorId([&](const auto& res) {
            if (res.status == HidlStatus::OK) {
                authId = static_cast<int64_t>(res.value);
            }
        });
        if (!ret.isOk()) {
            LOG(WARNING) << "getAuthenticatorId transport error: " << ret.description();
        }
    }
    mCb->onAuthenticatorIdRetrieved(authId);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::invalidateAuthenticatorId() {
    mCb->onAuthenticatorIdInvalidated(0);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::resetLockout(const keymaster::HardwareAuthToken& hat) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (ensureHidlConnectedLocked()) {
        auto hatBytes = serializeHat(hat);
        ::android::hardware::hidl_vec<uint8_t> hidlHat(hatBytes);
        auto ret = mAospFace->resetLockout(hidlHat);
        if (!ret.isOk()) {
            LOG(WARNING) << "resetLockout transport error: " << ret.description();
        }
    }
    // Vendor face HIDL returns synchronously without emitting onLockoutChanged(0).
    // Always emit onLockoutCleared() so FaceResetLockoutClient completes promptly.
    mCb->onLockoutCleared();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::onContextChanged(const common::OperationContext& /*ctx*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FaceSession::close() {
    if (!mClosed.exchange(true)) {
        mCancelled.store(true);
        mOpMode.store(OpMode::NONE);
        mCamera.stop();
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mAospFace != nullptr) {
                mAospFace->cancel();
            }
        }
        mCb->onSessionClosed();
    }
    return ndk::ScopedAStatus::ok();
}

void FaceSession::cancelActiveOperation() {
    OpMode prev = mOpMode.exchange(OpMode::NONE);
    mCancelled.store(true);
    mCamera.stop();
    if (prev != OpMode::NONE) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mAospFace != nullptr) {
                mAospFace->cancel();
            }
        }
        mCb->onError(Error::CANCELED, 0);
    }
}

void FaceSession::handleEnrollResult(uint32_t faceId, int32_t /*userId*/, uint32_t remaining) {
    LOG(INFO) << "onEnrollResult: faceId=" << faceId << " remaining=" << remaining;
    if (faceId != 0) {
        mCurrentFaceId = static_cast<int32_t>(faceId);
    }
    if (remaining == 0) {
        mOpMode.store(OpMode::NONE);
        mCamera.stop();
    }
    mCb->onEnrollmentProgress(static_cast<int32_t>(faceId), static_cast<int32_t>(remaining));
}

void FaceSession::handleAuthenticated(
        uint32_t faceId, int32_t /*userId*/, const std::vector<uint8_t>& /*token*/) {
    if (mOpMode.load() != OpMode::AUTHENTICATE) {
        return;
    }
    if (faceId != 0) {
        LOG(INFO) << "onAuthenticated: matched faceId=" << faceId;
        mOpMode.store(OpMode::NONE);
        mCamera.stop();
        int32_t ifaceVersion = 0;
        auto st = mCb->getInterfaceVersion(&ifaceVersion);
        if (st.isOk() && ifaceVersion >= 5) {
            AuthenticateSuccess result;
            result.enrollmentId = static_cast<int32_t>(faceId);
            result.hat = {};
            mCb->onAuthenticationSucceededWithResult(result);
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            mCb->onAuthenticationSucceeded(static_cast<int32_t>(faceId), {});
#pragma clang diagnostic pop
        }
    } else {
        LOG(INFO) << "onAuthenticated: faceId=0 (failed)";
        mOpMode.store(OpMode::NONE);
        mCamera.stop();
        mCb->onAuthenticationFailed();
    }
}

void FaceSession::handleAcquired(HidlAcquired acquiredInfo, int32_t vendorCode) {
    OpMode mode = mOpMode.load();
    if (mode == OpMode::NONE) {
        return;
    }
    BaseFrame base;
    base.acquiredInfo = mapAcquiredInfo(acquiredInfo);
    base.vendorCode = vendorCode;
    base.pan = 0.0f;
    base.tilt = 0.0f;
    base.distance = 0.0f;
    base.isCancellable = true;

    if (mode == OpMode::ENROLL) {
        EnrollmentFrame frame;
        frame.data = base;
        frame.stage = EnrollmentStage::ENROLLING_MOVEMENT_1;
        mCb->onEnrollmentFrame(frame);
    } else if (mode == OpMode::AUTHENTICATE) {
        AuthenticationFrame frame;
        frame.data = base;
        mCb->onAuthenticationFrame(frame);
    }
}

void FaceSession::handleError(HidlError error, int32_t vendorCode) {
    LOG(WARNING) << "Stock HIDL onError: error=" << static_cast<int>(error)
                 << " vendorCode=" << vendorCode;
    if (error == HidlError::CANCELED && mCancelled.load()) {
        return;
    }
    mOpMode.store(OpMode::NONE);
    mCamera.stop();
    if (error == HidlError::LOCKOUT) {
        mCb->onLockoutTimed(30000);
    } else if (error == HidlError::LOCKOUT_PERMANENT) {
        mCb->onLockoutPermanent();
    } else {
        mCb->onError(mapError(error), vendorCode);
    }
}

void FaceSession::handleRemoved(const std::vector<uint32_t>& removed) {
    std::vector<int32_t> ids;
    ids.reserve(removed.size());
    for (uint32_t id : removed) {
        ids.push_back(static_cast<int32_t>(id));
    }
    if (!ids.empty()) {
        mCurrentFaceId = 0;
    }
    mCb->onEnrollmentsRemoved(ids);
}

void FaceSession::handleEnumerate(const std::vector<uint32_t>& faceIds) {
    std::vector<int32_t> ids;
    ids.reserve(faceIds.size());
    for (uint32_t id : faceIds) {
        ids.push_back(static_cast<int32_t>(id));
    }
    mCurrentFaceId = ids.empty() ? 0 : ids.front();
    mCb->onEnrollmentsEnumerated(ids);
}

void FaceSession::handleLockoutChanged(uint64_t duration) {
    if (duration == 0) {
        mCb->onLockoutCleared();
    } else {
        mCb->onLockoutTimed(static_cast<int64_t>(duration));
    }
}

}  // namespace aidl::android::hardware::biometrics::face
