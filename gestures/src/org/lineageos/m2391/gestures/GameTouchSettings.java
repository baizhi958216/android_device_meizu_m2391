/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.UserHandle;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.provider.Settings;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class GameTouchSettings extends PreferenceActivity {
    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        if (getActionBar() != null) {
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(this));
    }

    @Override
    public void onResume() {
        super.onResume();
        PreferenceScreen screen = getPreferenceScreen();
        screen.removeAll();
        PackageManager packages = getPackageManager();
        Intent launcher = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
        Map<String, ApplicationInfo> installed = new HashMap<>();
        for (ResolveInfo activity : packages.queryIntentActivities(launcher, 0)) {
            ApplicationInfo app = activity.activityInfo.applicationInfo;
            installed.put(app.packageName, app);
        }
        List<ApplicationInfo> apps = new ArrayList<>(installed.values());
        apps.sort(new ApplicationInfo.DisplayNameComparator(packages));
        int userId = UserHandle.myUserId();
        Set<String> selected = GameTouchApps.read(getContentResolver(), userId);
        for (ApplicationInfo app : apps) {
            boolean automatic = GameTouchApps.isGame(app);
            CheckBoxPreference preference = new CheckBoxPreference(this);
            preference.setPersistent(false);
            preference.setTitle(app.loadLabel(packages));
            preference.setSummary(automatic
                    ? getString(R.string.game_touch_automatic) : app.packageName);
            preference.setChecked(automatic || selected.contains(app.packageName));
            preference.setEnabled(!automatic);
            preference.setOnPreferenceChangeListener((item, checked) -> {
                Set<String> updated = GameTouchApps.read(getContentResolver(), userId);
                if ((Boolean) checked) {
                    updated.add(app.packageName);
                } else {
                    updated.remove(app.packageName);
                }
                return Settings.Secure.putString(getContentResolver(), GameTouchApps.SETTING,
                        String.join(",", updated));
            });
            screen.addPreference(preference);
        }
    }

    @Override
    public boolean onNavigateUp() {
        finish();
        return true;
    }
}
