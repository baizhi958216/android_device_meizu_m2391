/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.app.ActivityManager;
import android.app.ActivityTaskManager;
import android.app.KeyguardManager;
import android.app.TaskStackListener;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.PowerManager;
import android.os.RemoteException;
import android.util.Log;
import android.view.Display;

import java.io.FileOutputStream;
import java.io.IOException;

/** Applies the Goodix game profile while an eligible app owns the focused task. */
final class GameTouchController {
    private static final String TAG = "M2391GameTouch";
    private static final String NODE = "/sys/devices/platform/goodix_ts.0/game_mode_node";
    private static final int MAX_RETRIES = 30;

    private final Handler mHandler;
    private final KeyguardManager mKeyguard;
    private final PowerManager mPower;
    private final PackageManager mPackages;
    private final ContentResolver mResolver;
    private final Runnable mUpdate = this::update;
    private boolean mListening;
    private Boolean mApplied;
    private int mRetries;

    private final TaskStackListener mTasks = new TaskStackListener() {
        @Override
        public void onTaskStackChanged() {
            refresh();
        }

        @Override
        public void onTaskFocusChanged(int taskId, boolean focused) {
            refresh();
        }
    };

    GameTouchController(Context context, Handler handler) {
        mHandler = handler;
        mKeyguard = context.getSystemService(KeyguardManager.class);
        mPower = context.getSystemService(PowerManager.class);
        mPackages = context.getPackageManager();
        mResolver = context.getContentResolver();
    }

    void refresh() {
        mHandler.post(() -> {
            mHandler.removeCallbacks(mUpdate);
            mRetries = 0;
            update();
        });
    }

    private void update() {
        try {
            if (!mListening) {
                ActivityTaskManager.getService().registerTaskStackListener(mTasks);
                mListening = true;
            }
            boolean enabled = isGameFocused();
            if (mApplied != null && mApplied == enabled) {
                return;
            }
            try (FileOutputStream output = new FileOutputStream(NODE)) {
                output.write(enabled ? '1' : '0');
            }
            mApplied = enabled;
            Log.i(TAG, "Touch game mode " + (enabled ? "enabled" : "disabled"));
        } catch (IOException | RemoteException | RuntimeException e) {
            mApplied = null;
            if (mRetries++ < MAX_RETRIES) {
                mHandler.postDelayed(mUpdate, 1000);
            } else {
                Log.e(TAG, "Unable to apply touch game mode", e);
            }
        }
    }

    private boolean isGameFocused() throws RemoteException {
        if (!mPower.isInteractive() || mKeyguard.isKeyguardLocked()) {
            return false;
        }
        ActivityTaskManager.RootTaskInfo task =
                ActivityTaskManager.getService().getFocusedRootTaskInfo();
        if (task == null || task.displayId != Display.DEFAULT_DISPLAY || task.topActivity == null) {
            return false;
        }
        String packageName = task.topActivity.getPackageName();
        if (GameTouchApps.read(mResolver, ActivityManager.getCurrentUser()).contains(packageName)) {
            return true;
        }
        try {
            ApplicationInfo app = mPackages.getApplicationInfoAsUser(packageName, 0, task.userId);
            return GameTouchApps.isGame(app);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }
}
