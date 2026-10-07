/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.app.ActivityManager;
import android.app.Application;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.hardware.biometrics.BiometricStateListener;
import android.hardware.fingerprint.FingerprintManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.PowerManager;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;

import java.io.IOException;

/** Synchronizes wake gestures and the ultrasonic fingerprint interrupt path. */
public final class GestureApplication extends Application {
    private static final String TAG = "M2391Gestures";
    private static final int MAX_RETRIES = 30;
    private static final String DOZE_PULSE_ON_AUTH = "doze_pulse_on_auth";
    private Handler mHandler;
    private FingerprintManager mFingerprintManager;
    private boolean mFingerprintListenerRegistered;
    private boolean mKeyguardAuthenticationRunning;
    private int mRetries;
    private final Runnable mApply = this::applySetting;
    private final BiometricStateListener mFingerprintListener = new BiometricStateListener() {
        @Override
        public void onStateChanged(int newState) {
            mHandler.post(() -> {
                mKeyguardAuthenticationRunning =
                        newState == BiometricStateListener.STATE_KEYGUARD_AUTH;
                synchronizeSetting();
            });
        }

        @Override
        public void onEnrollmentsChanged(int userId, int sensorId, boolean hasEnrollments) {
            synchronizeSetting();
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread thread = new HandlerThread(TAG);
        thread.start();
        mHandler = new Handler(thread.getLooper());
        ContentObserver observer = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange) {
                synchronizeSetting();
            }
        };
        getContentResolver().registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.DOUBLE_TAP_TO_WAKE), false,
                observer, UserHandle.USER_ALL);
        getContentResolver().registerContentObserver(
                Settings.Secure.getUriFor(DOZE_PULSE_ON_AUTH), false,
                observer, UserHandle.USER_ALL);
        IntentFilter filter = new IntentFilter(Intent.ACTION_USER_SWITCHED);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_USER_PRESENT);
        registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                synchronizeSetting();
            }
        }, filter, Context.RECEIVER_NOT_EXPORTED);
        synchronizeSetting();
    }

    void synchronizeSetting() {
        mHandler.post(() -> {
            mHandler.removeCallbacks(mApply);
            mRetries = 0;
            applySetting();
        });
    }

    private void applySetting() {
        try {
            if (mFingerprintManager == null) {
                mFingerprintManager = getSystemService(FingerprintManager.class);
            }
            if (mFingerprintManager != null && !mFingerprintListenerRegistered) {
                mFingerprintManager.registerBiometricStateListener(mFingerprintListener);
                mFingerprintListenerRegistered = true;
            }
            int userId = ActivityManager.getCurrentUser();
            boolean doubleTapEnabled = Settings.Secure.getIntForUser(getContentResolver(),
                    Settings.Secure.DOUBLE_TAP_TO_WAKE, 0, userId) != 0;
            // The ultrasonic HAL authenticates from its hardware interrupt. Without
            // bit 24, Goodix disables this path when the display enters DOZE_SUSPEND.
            // During AOD entry keyguard may report not showing even though its
            // authentication client is running. Keep the interrupt armed while
            // noninteractive, including when this process restarts during AOD.
            boolean fingerprintRequested = mKeyguardAuthenticationRunning
                    || !getSystemService(PowerManager.class).isInteractive()
                    || getSystemService(KeyguardManager.class).isKeyguardLocked();
            boolean fingerprintEnabled = mFingerprintManager != null
                    && fingerprintRequested
                    && mFingerprintManager.hasEnrolledFingerprints(userId)
                    && Settings.Secure.getIntForUser(getContentResolver(),
                            DOZE_PULSE_ON_AUTH, 1, userId) != 0;
            GestureControl.setGesturesEnabled(doubleTapEnabled, fingerprintEnabled);
        } catch (IOException | RuntimeException e) {
            if (mRetries++ < MAX_RETRIES) {
                // Persistent apps may start before sysfs or the user's settings are ready.
                mHandler.postDelayed(mApply, 1000);
            } else {
                Log.e(TAG, "Unable to apply wake gestures", e);
            }
        }
    }
}
