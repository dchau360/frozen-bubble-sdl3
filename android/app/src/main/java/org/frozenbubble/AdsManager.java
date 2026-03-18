/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;

import androidx.annotation.NonNull;

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

    // Test ad unit ID — replace with your real unit ID before publishing
    // Test IDs: https://developers.google.com/admob/android/test-ads
    private static final String AD_UNIT_ID =
            "ca-app-pub-3940256099942544/1033173712"; // test interstitial

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
