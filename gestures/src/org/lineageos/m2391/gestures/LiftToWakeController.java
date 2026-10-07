/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.app.ActivityManager;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.PowerManager;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.view.Display;

final class LiftToWakeController implements SensorEventListener {
    static final String SETTING = "wake_gesture_enabled";
    private static final String TAG = "M2391LiftToWake";
    private static final String SENSOR_TYPE = "com.meizu.sensor.raise";
    private static final long ARM_DELAY_NS = 500_000_000L;
    private final Context mContext;
    private final Handler mHandler;
    private final SensorManager mSensors;
    private final PowerManager mPower;
    private final Sensor mRaise;
    private final RaiseGestureFilter mFilter = new RaiseGestureFilter();
    private boolean mListening;
    private long mArmedAt;

    LiftToWakeController(Context context, Handler handler) {
        mContext = context;
        mHandler = handler;
        mSensors = context.getSystemService(SensorManager.class);
        mPower = context.getSystemService(PowerManager.class);
        Sensor raise = null;
        if (mSensors != null) {
            for (Sensor sensor : mSensors.getSensorList(Sensor.TYPE_ALL)) {
                if (SENSOR_TYPE.equals(sensor.getStringType()) && sensor.isWakeUpSensor()) {
                    raise = sensor;
                    break;
                }
            }
        }
        mRaise = raise;
    }

    void update() {
        boolean requested = mRaise != null && !mPower.isInteractive()
                && Settings.Secure.getIntForUser(mContext.getContentResolver(), SETTING, 0,
                        ActivityManager.getCurrentUser()) != 0;
        if (requested == mListening) {
            return;
        }
        if (requested) {
            mFilter.reset();
            mArmedAt = SystemClock.elapsedRealtimeNanos() + ARM_DELAY_NS;
            mListening = mSensors.registerListener(this, mRaise,
                    SensorManager.SENSOR_DELAY_NORMAL, mHandler);
            if (!mListening) {
                Log.w(TAG, "Unable to register raise sensor");
            }
        } else {
            mSensors.unregisterListener(this);
            mListening = false;
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (!mListening || event.values.length == 0
                || event.timestamp < mArmedAt - ARM_DELAY_NS) {
            return;
        }
        // This sensor reports action codes, not a boolean. Keep action history
        // even during the arming delay, but never wake on a cached initial sample.
        boolean wake = mFilter.shouldWake(event.values[0]);
        Log.d(TAG, "Raise action=" + event.values[0] + " wake=" + wake);
        if (!wake || event.timestamp < mArmedAt || mPower.isInteractive()) {
            return;
        }
        mArmedAt = SystemClock.elapsedRealtimeNanos() + ARM_DELAY_NS;
        mPower.wakeUpWithProximityCheck(SystemClock.uptimeMillis(),
                PowerManager.WAKE_REASON_GESTURE, "m2391:raise", Display.DEFAULT_DISPLAY);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}
}
