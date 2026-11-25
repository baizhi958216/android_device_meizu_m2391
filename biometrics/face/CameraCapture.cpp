// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "m2391.face.camera"

#include "CameraCapture.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImage.h>

#include <algorithm>
#include <cstring>

namespace aidl::android::hardware::biometrics::face {

namespace {
constexpr int32_t kDefaultWidth = 640;
constexpr int32_t kDefaultHeight = 480;
constexpr int32_t kMaxImages = 4;
constexpr int64_t kMinFrameIntervalMs = 150;
}  // namespace

CameraCapture::CameraCapture() {
    mDeviceCallbacks.context = this;
    mDeviceCallbacks.onDisconnected = sOnDeviceDisconnected;
    mDeviceCallbacks.onError = sOnDeviceError;

    mSessionCallbacks.context = this;
    mSessionCallbacks.onClosed = sOnSessionClosed;
    mSessionCallbacks.onReady = sOnSessionReady;
    mSessionCallbacks.onActive = sOnSessionActive;

    mImageListener.context = this;
    mImageListener.onImageAvailable = sOnImageAvailable;
}

CameraCapture::~CameraCapture() {
    stop();
}

bool CameraCapture::findFrontCamera(std::string* cameraIdOut, int32_t* orientationOut) {
    ACameraIdList* idList = nullptr;
    camera_status_t status = ACameraManager_getCameraIdList(mCameraManager, &idList);
    if (status != ACAMERA_OK || idList == nullptr) {
        LOG(ERROR) << "ACameraManager_getCameraIdList failed: " << status;
        return false;
    }

    bool found = false;
    for (int i = 0; i < idList->numCameras; ++i) {
        const char* id = idList->cameraIds[i];
        ACameraMetadata* chars = nullptr;
        if (ACameraManager_getCameraCharacteristics(mCameraManager, id, &chars) != ACAMERA_OK ||
            chars == nullptr) {
            continue;
        }
        ACameraMetadata_const_entry facing{};
        if (ACameraMetadata_getConstEntry(chars, ACAMERA_LENS_FACING, &facing) == ACAMERA_OK &&
            facing.count > 0 && facing.data.u8[0] == ACAMERA_LENS_FACING_FRONT) {
            *cameraIdOut = id;
            ACameraMetadata_const_entry orient{};
            if (ACameraMetadata_getConstEntry(chars, ACAMERA_SENSOR_ORIENTATION, &orient) ==
                        ACAMERA_OK &&
                orient.count > 0) {
                *orientationOut = orient.data.i32[0];
            } else {
                *orientationOut = 270;
            }
            found = true;
            ACameraMetadata_free(chars);
            break;
        }
        ACameraMetadata_free(chars);
    }

    ACameraManager_deleteCameraIdList(idList);
    return found;
}

bool CameraCapture::openSession(ANativeWindow* previewWindow) {
    if (mCaptureSession != nullptr) {
        ACameraCaptureSession_close(mCaptureSession);
        mCaptureSession = nullptr;
    }
    if (mCaptureRequest != nullptr) {
        ACaptureRequest_free(mCaptureRequest);
        mCaptureRequest = nullptr;
    }
    if (mReaderTarget != nullptr) {
        ACameraOutputTarget_free(mReaderTarget);
        mReaderTarget = nullptr;
    }
    if (mPreviewTarget != nullptr) {
        ACameraOutputTarget_free(mPreviewTarget);
        mPreviewTarget = nullptr;
    }
    if (mOutputContainer != nullptr) {
        ACaptureSessionOutputContainer_free(mOutputContainer);
        mOutputContainer = nullptr;
    }
    if (mReaderOutput != nullptr) {
        ACaptureSessionOutput_free(mReaderOutput);
        mReaderOutput = nullptr;
    }
    if (mPreviewOutput != nullptr) {
        ACaptureSessionOutput_free(mPreviewOutput);
        mPreviewOutput = nullptr;
    }

    camera_status_t st = ACaptureSessionOutputContainer_create(&mOutputContainer);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACaptureSessionOutputContainer_create failed: " << st;
        return false;
    }

    st = ACaptureSessionOutput_create(mReaderWindow, &mReaderOutput);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACaptureSessionOutput_create(reader) failed: " << st;
        return false;
    }
    ACaptureSessionOutputContainer_add(mOutputContainer, mReaderOutput);

    if (previewWindow != nullptr) {
        st = ACaptureSessionOutput_create(previewWindow, &mPreviewOutput);
        if (st == ACAMERA_OK) {
            ACaptureSessionOutputContainer_add(mOutputContainer, mPreviewOutput);
        } else {
            LOG(WARNING) << "ACaptureSessionOutput_create(preview) failed: " << st;
            previewWindow = nullptr;
        }
    }

    st = ACameraDevice_createCaptureSession(
            mCameraDevice, mOutputContainer, &mSessionCallbacks, &mCaptureSession);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACameraDevice_createCaptureSession failed: " << st;
        return false;
    }

    st = ACameraDevice_createCaptureRequest(mCameraDevice, TEMPLATE_PREVIEW, &mCaptureRequest);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACameraDevice_createCaptureRequest failed: " << st;
        return false;
    }

    st = ACameraOutputTarget_create(mReaderWindow, &mReaderTarget);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACameraOutputTarget_create(reader) failed: " << st;
        return false;
    }
    ACaptureRequest_addTarget(mCaptureRequest, mReaderTarget);

    if (previewWindow != nullptr && mPreviewOutput != nullptr) {
        st = ACameraOutputTarget_create(previewWindow, &mPreviewTarget);
        if (st == ACAMERA_OK) {
            ACaptureRequest_addTarget(mCaptureRequest, mPreviewTarget);
        } else {
            LOG(WARNING) << "ACameraOutputTarget_create(preview) failed: " << st;
        }
    }

    st = ACameraCaptureSession_setRepeatingRequest(
            mCaptureSession, nullptr, 1, &mCaptureRequest, nullptr);
    if (st != ACAMERA_OK) {
        LOG(ERROR) << "ACameraCaptureSession_setRepeatingRequest failed: " << st;
        return false;
    }
    return true;
}

bool CameraCapture::start(ANativeWindow* previewWindow, FrameCallback callback) {
    stop();

    std::lock_guard<std::mutex> lock(mMutex);
    mWidth = static_cast<uint32_t>(
            ::android::base::GetIntProperty("persist.vendor.m2391.face.width", kDefaultWidth));
    mHeight = static_cast<uint32_t>(
            ::android::base::GetIntProperty("persist.vendor.m2391.face.height", kDefaultHeight));
    mCallback = std::move(callback);
    mLastFrameTime = std::chrono::steady_clock::time_point{};

    if (previewWindow != nullptr) {
        ANativeWindow_acquire(previewWindow);
        mPreviewWindow = previewWindow;
    }

    mCameraManager = ACameraManager_create();
    if (mCameraManager == nullptr) {
        LOG(ERROR) << "ACameraManager_create failed";
        return false;
    }

    std::string cameraId = "1";
    int32_t orientation = 270;
    if (!findFrontCamera(&cameraId, &orientation)) {
        LOG(WARNING) << "Front camera lookup failed, falling back to cameraId=1";
    }
    mSensorOrientation = orientation;

    media_status_t mst = AImageReader_new(
            static_cast<int32_t>(mWidth),
            static_cast<int32_t>(mHeight),
            AIMAGE_FORMAT_YUV_420_888,
            kMaxImages,
            &mImageReader);
    if (mst != AMEDIA_OK || mImageReader == nullptr) {
        LOG(ERROR) << "AImageReader_new failed: " << mst;
        return false;
    }

    AImageReader_setImageListener(mImageReader, &mImageListener);

    mst = AImageReader_getWindow(mImageReader, &mReaderWindow);
    if (mst != AMEDIA_OK || mReaderWindow == nullptr) {
        LOG(ERROR) << "AImageReader_getWindow failed: " << mst;
        return false;
    }

    camera_status_t st = ACameraManager_openCamera(
            mCameraManager, cameraId.c_str(), &mDeviceCallbacks, &mCameraDevice);
    if (st != ACAMERA_OK || mCameraDevice == nullptr) {
        LOG(ERROR) << "ACameraManager_openCamera(" << cameraId << ") failed: " << st;
        return false;
    }

    if (!openSession(mPreviewWindow)) {
        if (mPreviewWindow != nullptr) {
            LOG(WARNING) << "Retrying camera session without preview surface";
            if (!openSession(nullptr)) {
                return false;
            }
        } else {
            return false;
        }
    }

    mRunning.store(true);
    LOG(INFO) << "Started front camera " << cameraId << " (" << mWidth << "x" << mHeight
              << ", sensorOrientation=" << mSensorOrientation
              << ", preview=" << (mPreviewWindow != nullptr) << ")";
    return true;
}

void CameraCapture::stop() {
    mRunning.store(false);
    std::lock_guard<std::mutex> lock(mMutex);

    if (mCaptureSession != nullptr) {
        ACameraCaptureSession_stopRepeating(mCaptureSession);
        ACameraCaptureSession_close(mCaptureSession);
        mCaptureSession = nullptr;
    }
    if (mCaptureRequest != nullptr) {
        ACaptureRequest_free(mCaptureRequest);
        mCaptureRequest = nullptr;
    }
    if (mReaderTarget != nullptr) {
        ACameraOutputTarget_free(mReaderTarget);
        mReaderTarget = nullptr;
    }
    if (mPreviewTarget != nullptr) {
        ACameraOutputTarget_free(mPreviewTarget);
        mPreviewTarget = nullptr;
    }
    if (mOutputContainer != nullptr) {
        ACaptureSessionOutputContainer_free(mOutputContainer);
        mOutputContainer = nullptr;
    }
    if (mReaderOutput != nullptr) {
        ACaptureSessionOutput_free(mReaderOutput);
        mReaderOutput = nullptr;
    }
    if (mPreviewOutput != nullptr) {
        ACaptureSessionOutput_free(mPreviewOutput);
        mPreviewOutput = nullptr;
    }
    if (mCameraDevice != nullptr) {
        ACameraDevice_close(mCameraDevice);
        mCameraDevice = nullptr;
    }
    if (mImageReader != nullptr) {
        AImageReader_setImageListener(mImageReader, nullptr);
        AImageReader_delete(mImageReader);
        mImageReader = nullptr;
        mReaderWindow = nullptr;
    }
    if (mCameraManager != nullptr) {
        ACameraManager_delete(mCameraManager);
        mCameraManager = nullptr;
    }
    if (mPreviewWindow != nullptr) {
        ANativeWindow_release(mPreviewWindow);
        mPreviewWindow = nullptr;
    }
    mCallback = nullptr;
}

void CameraCapture::onImageAvailable(AImageReader* reader) {
    AImage* image = nullptr;
    if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || image == nullptr) {
        return;
    }

    if (!mRunning.load()) {
        AImage_delete(image);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const int64_t intervalMs = ::android::base::GetIntProperty(
            "persist.vendor.m2391.face.frame_interval_ms", kMinFrameIntervalMs);
    if (mLastFrameTime.time_since_epoch().count() != 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - mLastFrameTime)
                                     .count();
        if (elapsed < intervalMs) {
            AImage_delete(image);
            return;
        }
    }
    mLastFrameTime = now;

    int32_t width = 0;
    int32_t height = 0;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);

    uint8_t* yData = nullptr;
    uint8_t* uData = nullptr;
    uint8_t* vData = nullptr;
    int yLen = 0, uLen = 0, vLen = 0;
    int32_t yRowStride = 0, uRowStride = 0, vRowStride = 0;
    int32_t uPixelStride = 0, vPixelStride = 0;

    if (AImage_getPlaneData(image, 0, &yData, &yLen) != AMEDIA_OK ||
        AImage_getPlaneData(image, 1, &uData, &uLen) != AMEDIA_OK ||
        AImage_getPlaneData(image, 2, &vData, &vLen) != AMEDIA_OK ||
        AImage_getPlaneRowStride(image, 0, &yRowStride) != AMEDIA_OK ||
        AImage_getPlaneRowStride(image, 1, &uRowStride) != AMEDIA_OK ||
        AImage_getPlaneRowStride(image, 2, &vRowStride) != AMEDIA_OK ||
        AImage_getPlanePixelStride(image, 1, &uPixelStride) != AMEDIA_OK ||
        AImage_getPlanePixelStride(image, 2, &vPixelStride) != AMEDIA_OK ||
        width <= 0 || height <= 0) {
        AImage_delete(image);
        return;
    }

    std::vector<uint8_t> nv21(static_cast<size_t>(width) * height * 3 / 2);
    uint8_t* dst = nv21.data();

    for (int32_t row = 0; row < height; ++row) {
        std::memcpy(dst + static_cast<size_t>(row) * width,
                    yData + static_cast<size_t>(row) * yRowStride,
                    static_cast<size_t>(width));
    }

    uint8_t* uvDst = dst + static_cast<size_t>(width) * height;
    const int32_t uvHeight = height / 2;
    const int32_t uvWidth = width / 2;
    for (int32_t row = 0; row < uvHeight; ++row) {
        const uint8_t* vRow = vData + static_cast<size_t>(row) * vRowStride;
        const uint8_t* uRow = uData + static_cast<size_t>(row) * uRowStride;
        uint8_t* outRow = uvDst + static_cast<size_t>(row) * width;
        for (int32_t col = 0; col < uvWidth; ++col) {
            outRow[col * 2] = vRow[col * vPixelStride];
            outRow[col * 2 + 1] = uRow[col * uPixelStride];
        }
    }

    AImage_delete(image);

    FrameCallback cbCopy;
    int32_t orient = 270;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning.load() || !mCallback) {
            return;
        }
        cbCopy = mCallback;
        orient = mSensorOrientation;
    }

    cbCopy(nv21, static_cast<uint32_t>(width), static_cast<uint32_t>(height), orient);
}

void CameraCapture::sOnImageAvailable(void* context, AImageReader* reader) {
    if (auto* self = static_cast<CameraCapture*>(context)) {
        self->onImageAvailable(reader);
    }
}

void CameraCapture::sOnDeviceDisconnected(void* /*context*/, ACameraDevice* /*device*/) {
    LOG(WARNING) << "Front camera disconnected";
}

void CameraCapture::sOnDeviceError(void* /*context*/, ACameraDevice* /*device*/, int error) {
    LOG(ERROR) << "Front camera error: " << error;
}

void CameraCapture::sOnSessionClosed(void* /*context*/, ACameraCaptureSession* /*session*/) {}
void CameraCapture::sOnSessionReady(void* /*context*/, ACameraCaptureSession* /*session*/) {}
void CameraCapture::sOnSessionActive(void* /*context*/, ACameraCaptureSession* /*session*/) {}

}  // namespace aidl::android::hardware::biometrics::face
