/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.m2391.gestures;

import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.provider.Settings;

/** Settings entry for the vendor raise sensor, which has no standard wake-gesture type. */
public final class LiftToWakeSettings extends PreferenceActivity {
    private SwitchPreference mSwitch;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        if (getActionBar() != null) {
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(this);
        mSwitch = new SwitchPreference(this);
        mSwitch.setTitle(R.string.lift_to_wake_title);
        mSwitch.setSummary(R.string.lift_to_wake_summary);
        mSwitch.setPersistent(false);
        mSwitch.setOnPreferenceChangeListener((preference, value) ->
                Settings.Secure.putInt(getContentResolver(), LiftToWakeController.SETTING,
                        (Boolean) value ? 1 : 0));
        screen.addPreference(mSwitch);
        setPreferenceScreen(screen);
    }

    @Override
    public void onResume() {
        super.onResume();
        mSwitch.setChecked(Settings.Secure.getInt(getContentResolver(),
                LiftToWakeController.SETTING, 0) != 0);
    }

    @Override
    public boolean onNavigateUp() {
        finish();
        return true;
    }
}
