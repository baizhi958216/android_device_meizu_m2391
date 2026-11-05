/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.content.ContentResolver;
import android.content.pm.ApplicationInfo;
import android.provider.Settings;

import java.util.Collections;
import java.util.Set;
import java.util.TreeSet;

final class GameTouchApps {
    static final String SETTING = "m2391_game_touch_apps";

    private GameTouchApps() {}

    static boolean isGame(ApplicationInfo app) {
        return app.category == ApplicationInfo.CATEGORY_GAME
                || (app.flags & ApplicationInfo.FLAG_IS_GAME) != 0;
    }

    static Set<String> read(ContentResolver resolver, int userId) {
        Set<String> apps = new TreeSet<>();
        String value = Settings.Secure.getStringForUser(resolver, SETTING, userId);
        if (value != null && !value.isEmpty()) {
            Collections.addAll(apps, value.split(","));
        }
        return apps;
    }
}
