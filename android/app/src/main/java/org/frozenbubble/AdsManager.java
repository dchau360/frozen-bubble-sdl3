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
import android.widget.Toast;

import com.appodeal.ads.Appodeal;
import com.appodeal.ads.InterstitialCallbacks;
import com.appodeal.consent.ConsentInfoUpdateCallback;
import com.appodeal.consent.ConsentManager;
import com.appodeal.consent.ConsentManagerError;
import com.appodeal.consent.ConsentUpdateRequestParameters;
import com.appodeal.consent.PrivacyOptionsRequirementStatus;

/**
 * Manages Appodeal interstitial ads and the "ads removed" preference.
 *
 * Usage:
 *   AdsManager.showLobbyAd(activity);   // call when lobby screen appears
 *   AdsManager.showPrivacyOptions(activity); // the settings' Ad privacy row
 *   AdsManager.setAdsRemoved(activity, removed); // from BillingManager
 */
public class AdsManager {
    private static final String TAG = "FBubble.Ads";
    private static final String PREFS_NAME  = "FrozenBubblePrefs";
    private static final String KEY_NO_ADS  = "ads_removed";

    private static boolean sInitialized = false;

    /** Show an interstitial ad if one is ready and ads haven't been removed. */
    public static void showLobbyAd(final Activity activity) {
        if (isAdsRemoved(activity)) return;
        if (BuildConfig.APPODEAL_APP_KEY.isEmpty()) return;

        activity.runOnUiThread(() -> {
            if (!sInitialized) {
                // First lobby entry of the process: start the SDK, which
                // caches an interstitial on its own (auto-cache is on by
                // default), so the next entry has one to show -- the same
                // first-entry-shows-nothing behavior the AdMob version had.
                initialize(activity);
                Log.d(TAG, "No ad ready yet");
                return;
            }
            if (Appodeal.isLoaded(Appodeal.INTERSTITIAL)) {
                Appodeal.show(activity, Appodeal.INTERSTITIAL);
            } else {
                Log.d(TAG, "No ad ready yet");
            }
        });
    }

    /**
     * Opens Appodeal's privacy options form, where a player reviews or
     * changes the consent the ad networks act on -- the GDPR choices in the
     * EU and UK, and the opt-out of sale/sharing that US state privacy laws
     * require an app to offer at any time, not just at first launch.
     *
     * Asks Appodeal for the player's consent status first rather than relying
     * on initialize() having done so: the row is reachable before the first
     * lobby entry ever starts the SDK. Where no form applies the player is
     * told so, instead of the tap doing nothing.
     */
    public static void showPrivacyOptions(final Activity activity) {
        if (BuildConfig.APPODEAL_APP_KEY.isEmpty()) {
            toast(activity, "This build shows no ads.");
            return;
        }
        activity.runOnUiThread(() -> ConsentManager.requestConsentInfoUpdate(
                new ConsentUpdateRequestParameters(activity, BuildConfig.APPODEAL_APP_KEY),
                new ConsentInfoUpdateCallback() {
                    @Override public void onUpdated() {
                        activity.runOnUiThread(() -> {
                            PrivacyOptionsRequirementStatus status =
                                    ConsentManager.getPrivacyOptionsRequirementStatus();
                            Log.d(TAG, "Privacy options requirement: " + status
                                    + ", consent status: " + ConsentManager.getStatus());
                            if (status != PrivacyOptionsRequirementStatus.Required) {
                                toast(activity, "No ad privacy choices apply where you are.");
                                return;
                            }
                            ConsentManager.showPrivacyOptionsForm(activity, error -> {
                                if (error != null) {
                                    Log.w(TAG, "Privacy options form failed: " + error);
                                    toast(activity, "Ad privacy choices could not be opened. Try again later.");
                                }
                            });
                        });
                    }
                    @Override public void onFailed(ConsentManagerError error) {
                        Log.w(TAG, "Consent info update failed: " + error);
                        toast(activity, "Ad privacy choices could not be loaded. Check your connection.");
                    }
                }));
    }

    private static void toast(final Activity activity, final String text) {
        activity.runOnUiThread(() -> Toast.makeText(activity, text, Toast.LENGTH_LONG).show());
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
        // Nothing to discard on removal: showLobbyAd() checks the flag before
        // it ever shows, and an ad the SDK already cached just goes unused.
        Log.d(TAG, removed ? "Ads removed" : "Ads enabled (no active entitlement)");
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
     * Starts the Appodeal SDK. Caller must be on the UI thread.
     *
     * Lazy, on the first lobby entry, rather than in onCreate(): the AdMob
     * SDK this replaced crashed SDL at startup when initialized there (its
     * worker threads hit a destroyed mutex in HWUI's CommonPool while SDL
     * was bringing up its EGL surface), and the networks Appodeal mediates
     * start the same kind of threads. Initializing after the game is
     * already running keeps that out of startup altogether.
     *
     * Testing mode is on in every debug build -- this developer's devices,
     * CI, anyone building from source -- so those get Appodeal's test ads
     * and can never produce live traffic, on however many devices a debug
     * APK ends up installed on. It has to be set before initialize().
     *
     * Appodeal shows its own consent form here where the law asks for one
     * (GDPR/UK, US state privacy laws), so there is no separate consent step.
     */
    private static void initialize(final Activity activity) {
        sInitialized = true;
        Appodeal.setTesting(BuildConfig.DEBUG);
        Appodeal.setInterstitialCallbacks(new InterstitialCallbacks() {
            @Override public void onInterstitialLoaded(boolean isPrecache) {
                Log.d(TAG, "Ad loaded");
            }
            @Override public void onInterstitialFailedToLoad() {
                Log.w(TAG, "Ad failed to load");
            }
            @Override public void onInterstitialShown() {}
            @Override public void onInterstitialShowFailed() {
                Log.w(TAG, "Ad failed to show");
            }
            @Override public void onInterstitialClicked() {}
            @Override public void onInterstitialClosed() {}
            @Override public void onInterstitialExpired() {}
        });
        Appodeal.initialize(activity, BuildConfig.APPODEAL_APP_KEY,
                Appodeal.INTERSTITIAL, errors -> {
            if (errors == null || errors.isEmpty()) {
                Log.d(TAG, "Appodeal initialized");
            } else {
                Log.w(TAG, "Appodeal initialized with errors: " + errors);
            }
        });
    }
}
