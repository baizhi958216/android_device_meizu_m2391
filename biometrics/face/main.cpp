// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "m2391.face.hal"

#include "FaceSession.h"

#include <aidl/android/hardware/biometrics/common/SensorStrength.h>
#include <aidl/android/hardware/biometrics/face/BnFace.h>
#include <aidl/android/hardware/biometrics/face/FaceSensorType.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <hidl/HidlTransportSupport.h>

#include <memory>
#include <string>
#include <vector>

namespace aidl::android::hardware::biometrics::face {

namespace {
constexpr int32_t kSensorId = 1;
constexpr int32_t kMaxEnrollmentsPerUser = 1;
constexpr int32_t kPreviewWidth = 640;
constexpr int32_t kPreviewHeight = 480;
}  // namespace

class M2391Face : public BnFace {
  public:
    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* return_val) override {
        common::ComponentInfo hwInfo;
        hwInfo.componentId = "faceSensor";
        hwInfo.hardwareVersion = "meizu/m2391/rgb";
        hwInfo.firmwareVersion = "stapp64";
        hwInfo.serialNumber = "m2391-face";
        hwInfo.softwareVersion = "";

        common::ComponentInfo swInfo;
        swInfo.componentId = "matchingAlgorithm";
        swInfo.hardwareVersion = "";
        swInfo.firmwareVersion = "";
        swInfo.serialNumber = "";
        swInfo.softwareVersion = "vendor-face@1.0";

        common::CommonProps commonProps;
        commonProps.sensorId = kSensorId;
        commonProps.sensorStrength = common::SensorStrength::WEAK;
        commonProps.maxEnrollmentsPerUser = kMaxEnrollmentsPerUser;
        commonProps.componentInfo = {std::move(hwInfo), std::move(swInfo)};

        SensorProps props;
        props.commonProps = std::move(commonProps);
        props.sensorType = FaceSensorType::RGB;
        props.halControlsPreview = true;
        props.enrollPreviewWidth = kPreviewWidth;
        props.enrollPreviewHeight = kPreviewHeight;
        props.enrollTranslationX = 0.0f;
        props.enrollTranslationY = 0.0f;
        props.enrollPreviewScale = 1.0f;
        props.supportsDetectInteraction = false;

        *return_val = {std::move(props)};
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus createSession(
            int32_t sensorId,
            int32_t userId,
            const std::shared_ptr<ISessionCallback>& cb,
            std::shared_ptr<ISession>* return_val) override {
        LOG(INFO) << "createSession: sensorId=" << sensorId << " userId=" << userId;
        auto session = ndk::SharedRefBase::make<FaceSession>(sensorId, userId, cb);
        session->init();
        mSession = session;
        *return_val = session;
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<FaceSession> mSession;
};

}  // namespace aidl::android::hardware::biometrics::face

int main() {
    ::android::base::InitLogging(nullptr, ::android::base::LogdLogger(::android::base::SYSTEM));
    LOG(INFO) << "Starting android.hardware.biometrics.face-service.m2391";

    // Start hwbinder threadpool for callbacks from vendor face HIDL 1.0.
    ::android::hardware::configureRpcThreadpool(2, false /* callerWillJoin */);

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    auto hal = ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::M2391Face>();
    const std::string instance =
            std::string(aidl::android::hardware::biometrics::face::M2391Face::descriptor) +
            "/default";
    binder_status_t status = AServiceManager_addService(hal->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        LOG(FATAL) << "Failed to register " << instance << ": " << status;
    }

    LOG(INFO) << "Registered " << instance;
    ABinderProcess_joinThreadPool();
    return 0;
}
