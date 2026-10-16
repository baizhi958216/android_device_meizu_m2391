/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.m2391.displayrecovery;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.util.Log;
import android.view.Display;
import android.view.DisplayAddress;
import android.view.SurfaceControl;

/** Repairs a missed display-mode notification when restoring QHD during boot. */
public final class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "M2391DisplayRecovery";
    // SurfaceFlinger's primary-display reconnect transaction in Android 17.
    // Recreates display state and notifies DisplayManager without restarting apps.
    private static final int RECONNECT_PRIMARY_DISPLAY = 1037;

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())
                && !Intent.ACTION_LOCKED_BOOT_COMPLETED.equals(intent.getAction())) return;
        PendingResult result = goAsync();
        new Thread(() -> {
            try {
                // Allow an in-flight normal mode switch to finish first. Require
                // two mismatching samples to avoid reconnecting a healthy display.
                SystemClock.sleep(1000);
                if (!hasResolutionMismatch(context)) return;
                SystemClock.sleep(1000);
                if (!hasResolutionMismatch(context)) return;

                IBinder compositor = ServiceManager.checkService("SurfaceFlinger");
                if (compositor == null) {
                    Log.e(TAG, "SurfaceFlinger unavailable; leaving display unchanged");
                    return;
                }
                Parcel data = Parcel.obtain();
                Parcel reply = Parcel.obtain();
                try {
                    data.writeInterfaceToken("android.ui.ISurfaceComposer");
                    if (!compositor.transact(RECONNECT_PRIMARY_DISPLAY, data, reply, 0)) {
                        Log.e(TAG, "Primary-display reconnect unsupported");
                        return;
                    }
                } finally {
                    reply.recycle();
                    data.recycle();
                }
                SystemClock.sleep(1000);
                if (hasResolutionMismatch(context)) {
                    Log.e(TAG, "Display resolution still mismatches after reconnect");
                } else {
                    Log.i(TAG, "Display resolution synchronized after boot");
                }
            } catch (Exception e) {
                Log.e(TAG, "Unable to synchronize display resolution", e);
            } finally {
                result.finish();
            }
        }, TAG).start();
    }

    private static boolean hasResolutionMismatch(Context context) {
        DisplayManager manager = context.getSystemService(DisplayManager.class);
        Display display = manager.getDisplay(Display.DEFAULT_DISPLAY);
        if (display == null) return false;
        DisplayAddress address = display.getAddress();
        if (address == null) return false;
        SurfaceControl.DynamicDisplayInfo nativeInfo =
                SurfaceControl.getDynamicDisplayInfo(address.getPhysicalDisplayId());
        if (nativeInfo == null || nativeInfo.supportedDisplayModes == null) return false;
        Display.Mode frameworkMode = display.getMode();
        for (SurfaceControl.DisplayMode mode : nativeInfo.supportedDisplayModes) {
            if (mode.id == nativeInfo.activeDisplayModeId) {
                boolean mismatch = mode.width != frameworkMode.getPhysicalWidth()
                        || mode.height != frameworkMode.getPhysicalHeight();
                if (mismatch) {
                    Log.w(TAG, "Resolution mismatch: framework="
                            + frameworkMode.getPhysicalWidth() + "x"
                            + frameworkMode.getPhysicalHeight() + ", compositor="
                            + mode.width + "x" + mode.height);
                }
                return mismatch;
            }
        }
        return false;
    }
}
