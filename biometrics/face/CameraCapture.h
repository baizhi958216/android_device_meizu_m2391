// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <android/native_window.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <media/NdkImageReader.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace aidl::android::hardware::biometrics::face {

class CameraCapture {
  public:
    using FrameCallback = std::function<void(const std::vector<uint8_t>& nv21,
                                             uint32_t width,
                                             uint32_t height,
                                             int32_t sensorOrientation)>;

    CameraCapture();
    ~CameraCapture();

    bool start(ANativeWindow* previewWindow, FrameCallback callback);
    void stop();
    bool isRunning() const { return mRunning.load(); }

  private:
    bool findFrontCamera(std::string* cameraIdOut, int32_t* orientationOut);
    bool openSession(ANativeWindow* previewWindow);
    void onImageAvailable(AImageReader* reader);

    static void sOnImageAvailable(void* context, AImageReader* reader);
    static void sOnDeviceDisconnected(void* context, ACameraDevice* device);
    static void sOnDeviceError(void* context, ACameraDevice* device, int error);
    static void sOnSessionClosed(void* context, ACameraCaptureSession* session);
    static void sOnSessionReady(void* context, ACameraCaptureSession* session);
    static void sOnSessionActive(void* context, ACameraCaptureSession* session);

    mutable std::mutex mMutex;
    std::atomic<bool> mRunning{false};
    FrameCallback mCallback;
    int32_t mSensorOrientation{270};
    uint32_t mWidth{640};
    uint32_t mHeight{480};
    std::chrono::steady_clock::time_point mLastFrameTime{};

    ACameraManager* mCameraManager{nullptr};
    ACameraDevice* mCameraDevice{nullptr};
    ACaptureSessionOutputContainer* mOutputContainer{nullptr};
    ACaptureSessionOutput* mReaderOutput{nullptr};
    ACaptureSessionOutput* mPreviewOutput{nullptr};
    ACameraCaptureSession* mCaptureSession{nullptr};
    ACaptureRequest* mCaptureRequest{nullptr};
    ACameraOutputTarget* mReaderTarget{nullptr};
    ACameraOutputTarget* mPreviewTarget{nullptr};
    AImageReader* mImageReader{nullptr};
    ANativeWindow* mReaderWindow{nullptr};
    ANativeWindow* mPreviewWindow{nullptr};

    ACameraDevice_StateCallbacks mDeviceCallbacks{};
    ACameraCaptureSession_stateCallbacks mSessionCallbacks{};
    AImageReader_ImageListener mImageListener{};
};

}  // namespace aidl::android::hardware::biometrics::face
