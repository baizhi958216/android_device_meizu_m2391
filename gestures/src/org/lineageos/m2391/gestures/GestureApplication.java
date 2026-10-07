/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.app.ActivityManager;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;

import java.io.IOException;

/** Bridges the platform tap-to-wake setting to the stock touch driver. */
public final class GestureApplication extends Application {
    private static final String TAG = "M2391Gestures";
    private static final int MAX_RETRIES = 30;
    private Handler mHandler;
    private int mRetries;
    private final Runnable mApply = this::applySetting;

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread thread = new HandlerThread(TAG);
        thread.start();
        mHandler = new Handler(thread.getLooper());
        getContentResolver().registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.DOUBLE_TAP_TO_WAKE), false,
                new ContentObserver(mHandler) {
                    @Override
                    public void onChange(boolean selfChange) {
                        synchronizeSetting();
                    }
                }, UserHandle.USER_ALL);
        registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                synchronizeSetting();
            }
        }, new IntentFilter(Intent.ACTION_USER_SWITCHED), Context.RECEIVER_NOT_EXPORTED);
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
            boolean enabled = Settings.Secure.getIntForUser(getContentResolver(),
                    Settings.Secure.DOUBLE_TAP_TO_WAKE, 0,
                    ActivityManager.getCurrentUser()) != 0;
            GestureControl.setDoubleTapEnabled(enabled);
        } catch (IOException | RuntimeException e) {
            if (mRetries++ < MAX_RETRIES) {
                // Persistent apps may start before sysfs or the user's settings are ready.
                mHandler.postDelayed(mApply, 1000);
            } else {
                Log.e(TAG, "Unable to apply double-tap setting", e);
            }
        }
    }
}
