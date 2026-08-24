/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */
package org.frozenbubble;

import android.app.Activity;
import android.content.SharedPreferences;
import android.util.Log;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.RequestConfiguration;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;

import androidx.annotation.NonNull;

import java.util.Collections;

/**
 * Manages AdMob interstitial ads and the "ads removed" preference.
 *
 * Usage:
 *   AdsManager.init(activity);
 *   AdsManager.showLobbyAd(activity);   // call when lobby screen appears
 *   AdsManager.setAdsRemoved(activity); // call after successful IAP
 */
public class AdsManager {
    private static final String TAG = "FBubble.Ads";
    private static final String PREFS_NAME  = "FrozenBubblePrefs";
    private static final String KEY_NO_ADS  = "ads_removed";

    // Real interstitial ad unit ID (Frozen Bubble app, created in AdMob 2026-08-24)
    private static final String AD_UNIT_ID =
            "ca-app-pub-7736855769799322/5410693019";

    private static InterstitialAd sInterstitial = null;
    private static boolean sInitialized = false;

    /** Call once from Activity.onCreate() */
    public static void init(final Activity activity) {
        if (sInitialized) return;
        sInitialized = true;

        if (isAdsRemoved(activity)) {
            Log.d(TAG, "Ads have been removed by user purchase — skipping init");
            return;
        }

        // With the real ad unit ID in place, any real device that requests an
        // ad is a real (if $0) impression -- including the developer's own,
        // which AdMob's policy treats as invalid traffic and can flag a new
        // account over. Registering this device as a test device makes the
        // SDK always serve safe test creatives here regardless of the ad
        // unit ID, exactly as it did before the real ID replaced the test
        // one. Empty when unset (see build.gradle) -- no-op in that case, so
        // CI and release builds always request real ads.
        if (!BuildConfig.ADMOB_TEST_DEVICE_ID.isEmpty()) {
            RequestConfiguration config = new RequestConfiguration.Builder()
                    .setTestDeviceIds(Collections.singletonList(BuildConfig.ADMOB_TEST_DEVICE_ID))
                    .build();
            MobileAds.setRequestConfiguration(config);
            Log.d(TAG, "AdMob test device configured");
        }

        MobileAds.initialize(activity, initStatus -> {
            Log.d(TAG, "AdMob initialized");
            loadAd(activity);
        });
    }

    /** Show an interstitial ad if one is ready and ads haven't been removed. */
    public static void showLobbyAd(final Activity activity) {
        if (isAdsRemoved(activity)) return;

        activity.runOnUiThread(() -> {
            if (sInterstitial != null) {
                sInterstitial.setFullScreenContentCallback(new FullScreenContentCallback() {
                    @Override
                    public void onAdDismissedFullScreenContent() {
                        sInterstitial = null;
                        loadAd(activity); // preload next ad
                    }
                    @Override
                    public void onAdFailedToShowFullScreenContent(@NonNull AdError e) {
                        sInterstitial = null;
                        loadAd(activity);
                    }
                });
                sInterstitial.show(activity);
            } else {
                Log.d(TAG, "No ad ready yet");
                loadAd(activity); // try to load for next time
            }
        });
    }

    /** Mark ads as permanently removed (call after successful IAP). */
    public static void setAdsRemoved(Activity activity) {
        SharedPreferences.Editor ed = activity
                .getSharedPreferences(PREFS_NAME, Activity.MODE_PRIVATE).edit();
        ed.putBoolean(KEY_NO_ADS, true);
        ed.apply();
        sInterstitial = null; // discard any loaded ad
        Log.d(TAG, "Ads removed");
    }

    /** Returns true if the user has purchased ad removal. */
    public static boolean isAdsRemoved(Activity activity) {
        return activity
                .getSharedPreferences(PREFS_NAME, Activity.MODE_PRIVATE)
                .getBoolean(KEY_NO_ADS, false);
    }

    // --- private helpers ---

    private static void loadAd(final Activity activity) {
        activity.runOnUiThread(() -> {
            AdRequest req = new AdRequest.Builder().build();
            InterstitialAd.load(activity, AD_UNIT_ID, req,
                    new InterstitialAdLoadCallback() {
                @Override
                public void onAdLoaded(@NonNull InterstitialAd ad) {
                    sInterstitial = ad;
                    Log.d(TAG, "Ad loaded");
                }
                @Override
                public void onAdFailedToLoad(@NonNull LoadAdError e) {
                    sInterstitial = null;
                    Log.w(TAG, "Ad failed to load: " + e.getMessage());
                }
            });
        });
    }
}
