// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <android/hardware/biometrics/face/1.0/types.h>
#include <android/hidl/base/1.0/IBase.h>
#include <hidl/HidlSupport.h>
#include <hidl/Status.h>
#include <utils/RefBase.h>

#include <cstdint>
#include <string>

namespace vendor::xingji::hardware::biometrics::face::V1_0 {

enum class Feature : uint32_t {
    REQUIRE_ATTENTION = 1u,
    REQUIRE_DIVERSITY = 2u,
};

enum class FaceFormat : uint32_t {
    GRAY8 = 0u,
    BGR888 = 1u,
    YUV420P = 5u,
    NV12 = 6u,
    NV21 = 7u,
};

enum class FaceOrientation : uint32_t {
    ROTATE_0 = 0u,
    ROTATE_90 = 1u,
    ROTATE_180 = 2u,
    ROTATE_270 = 3u,
};

struct IBiometricsFace : public ::android::hidl::base::V1_0::IBase {
    static const char* descriptor;

    virtual bool isRemote() const override { return false; }

    virtual ::android::hardware::Return<::android::hardware::biometrics::face::V1_0::Status> enroll(
            const ::android::hardware::hidl_vec<uint8_t>& hat,
            uint32_t timeoutSec,
            const ::android::hardware::hidl_vec<Feature>& disabledFeatures,
            const ::android::hardware::hidl_vec<uint8_t>& data,
            FaceFormat format,
            FaceOrientation orientation,
            uint32_t width,
            uint32_t height) = 0;

    virtual ::android::hardware::Return<::android::hardware::biometrics::face::V1_0::Status>
    authenticate(
            uint64_t operationId,
            const ::android::hardware::hidl_vec<uint8_t>& data,
            FaceFormat format,
            FaceOrientation orientation,
            uint32_t width,
            uint32_t height) = 0;

    static ::android::sp<IBiometricsFace> tryGetService(
            const std::string& serviceName = "default", bool getStub = false);
    static ::android::sp<IBiometricsFace> getService(
            const std::string& serviceName = "default", bool getStub = false);
};

}  // namespace vendor::xingji::hardware::biometrics::face::V1_0
