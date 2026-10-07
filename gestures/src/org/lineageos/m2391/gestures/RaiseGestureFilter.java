/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

final class RaiseGestureFilter {
    private static final int RAISE = 6;
    private static final int LOWER = 7;
    private static final int UPRIGHT = 2;
    private int mPreviousAction;

    void reset() {
        mPreviousAction = 0;
    }

    boolean shouldWake(float value) {
        if (!Float.isFinite(value) || value != (int) value) {
            return false;
        }
        int action = (int) value;
        boolean wake = action == RAISE || (action == UPRIGHT
                && mPreviousAction != RAISE && mPreviousAction != LOWER);
        mPreviousAction = action;
        return wake;
    }
}
