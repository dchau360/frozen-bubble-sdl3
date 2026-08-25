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
import android.content.Context;
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
    private static boolean sTestDeviceConfigured = false;

    /**
     * Not called from anywhere in this app today -- this app reaches ads only
     * via showLobbyAd() when C++ sends MSG_SHOW_AD, and loadAd() initializes
     * the SDK on demand. Left here because it is a reasonable entry point for
     * a caller embedding this class elsewhere; it delegates rather than
     * duplicating the init sequence, so there is only ever one of those.
     */
    public static void init(final Activity activity) {
        if (isAdsRemoved(activity)) {
            Log.d(TAG, "Ads have been removed by user purchase — skipping init");
            return;
        }
        loadAd(activity);
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

    /**
     * Set the ads-removed entitlement (call from BillingManager once Play has
     * been consulted).
     *
     * Takes a value rather than only ever granting, because the yearly plan can
     * lapse -- a subscription that expired has to put ads back, and a
     * grant-only version of this would leave them off forever after one paid
     * year.
     */
    public static void setAdsRemoved(Activity activity, boolean removed) {
        SharedPreferences.Editor ed = activity
                .getSharedPreferences(PREFS_NAME, Activity.MODE_PRIVATE).edit();
        ed.putBoolean(KEY_NO_ADS, removed);
        ed.apply();
        if (removed) {
            sInterstitial = null; // discard any loaded ad
            Log.d(TAG, "Ads removed");
        } else {
            Log.d(TAG, "Ads enabled (no active entitlement)");
        }
    }

    /**
     * Returns true if the user has purchased ad removal.
     *
     * Takes a Context rather than an Activity only so the JNI entry point in
     * FrozenBubbleActivity can share it: preference name and key live here,
     * and a second copy of those string literals elsewhere would survive a
     * rename here with no compile error and silently read the wrong flag.
     */
    public static boolean isAdsRemoved(Context context) {
        return context
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getBoolean(KEY_NO_ADS, false);
    }

    // --- private helpers ---

    /**
     * Registers this device as an AdMob test device, if android/local.properties
     * (git-ignored) set one -- see build.gradle. Must run before the *first*
     * ad request of the process, since RequestConfiguration only affects
     * requests made after it's set. This is the actual call path that runs in
     * this app (init() above does not -- see its comment), reached lazily via
     * loadAd() rather than eagerly in onCreate() for the same HWUI-thread
     * reason MobileAds.initialize() itself is deferred there.
     */
    private static void configureTestDeviceIfNeeded() {
        if (sTestDeviceConfigured) return;
        sTestDeviceConfigured = true;
        if (!BuildConfig.ADMOB_TEST_DEVICE_ID.isEmpty()) {
            RequestConfiguration config = new RequestConfiguration.Builder()
                    .setTestDeviceIds(Collections.singletonList(BuildConfig.ADMOB_TEST_DEVICE_ID))
                    .build();
            MobileAds.setRequestConfiguration(config);
            Log.d(TAG, "AdMob test device configured");
        }
    }

    /**
     * Loads an interstitial, initializing the Mobile Ads SDK first if this is
     * the first request of the process.
     *
     * MobileAds.initialize() genuinely has to run: the manifest disables
     * AdMob's own MobileAdsInitProvider (its ContentProvider init collides with
     * SDL's EGL surface on the HWUI thread and crashes at startup), so nothing
     * else will do it. This is the deferred init the manifest comment refers
     * to -- lazy, off the startup path, but real. Requesting an ad against an
     * uninitialized SDK happens to work through its own internal lazy init,
     * which is not something to depend on, and leaves the first request -- the
     * one the test-device allow-list exists to make safe -- outside any
     * guarantee that the configuration applied.
     */
    private static void loadAd(final Activity activity) {
        activity.runOnUiThread(() -> {
            configureTestDeviceIfNeeded();
            if (!sInitialized) {
                sInitialized = true;
                MobileAds.initialize(activity, initStatus -> {
                    Log.d(TAG, "AdMob initialized");
                    requestInterstitial(activity);
                });
                return;
            }
            requestInterstitial(activity);
        });
    }

    /** Issues the actual ad request. Caller must be on the UI thread. */
    private static void requestInterstitial(final Activity activity) {
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
    }
}
