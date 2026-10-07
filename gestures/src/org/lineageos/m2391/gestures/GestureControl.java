/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

final class GestureControl {
    private static final String NODE = "/sys/class/meizu/tp/gesture_control";
    private static final int ENABLE = 1 << 31;
    private static final int DOUBLE_TAP = 1 << 4;
    private static final int FINGERPRINT = 1 << 24;

    private GestureControl() {}

    static void setGesturesEnabled(boolean doubleTapEnabled, boolean fingerprintEnabled)
            throws IOException {
        int current = readMask();
        int updated = current & ~(DOUBLE_TAP | FINGERPRINT);
        if (doubleTapEnabled) {
            updated |= DOUBLE_TAP;
        }
        if (fingerprintEnabled) {
            updated |= FINGERPRINT;
        }
        if ((updated & ~ENABLE) == 0) {
            updated = 0;
        } else {
            updated |= ENABLE;
        }
        if (updated == current) {
            return;
        }
        // show() prints hex, but store() reads a little-endian u32 directly.
        // Preserve gesture bits not owned by this controller.
        byte[] bytes = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(updated).array();
        try (FileOutputStream output = new FileOutputStream(NODE)) {
            output.write(bytes);
        }
        if (readMask() != updated) {
            throw new IOException("Gesture mask did not persist");
        }
    }

    private static int readMask() throws IOException {
        String hex = new String(Files.readAllBytes(Paths.get(NODE)),
                StandardCharsets.US_ASCII).trim();
        try {
            return Integer.parseUnsignedInt(hex, 16);
        } catch (NumberFormatException e) {
            throw new IOException("Invalid gesture mask: " + hex, e);
        }
    }
}
