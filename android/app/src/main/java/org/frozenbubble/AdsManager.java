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
import android.os.SystemClock;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;
import com.google.android.ump.ConsentInformation;
import com.google.android.ump.ConsentRequestParameters;
import com.google.android.ump.UserMessagingPlatform;

/**
 * Manages AdMob interstitial ads, the consent they need, and the "ads
 * removed" preference.
 *
 * Usage:
 *   AdsManager.prepare(activity);       // start the SDKs early (5s after launch)
 *   AdsManager.showLobbyAd(activity);   // at a break: leaving a game, Continue
 *   AdsManager.showPrivacyOptions(activity); // the settings' Ad privacy row
 *   AdsManager.setAdsRemoved(activity, removed); // from BillingManager
 *
 * AdMob directly, with Google's consent SDK (UMP). Until v2.4.163 this went
 * through Appodeal's mediation, whose SDK hit an SdkConfigurationError on
 * every start and never started AdMob at all; this is the two-week test of
 * AdMob on its own.
 */
public class AdsManager {
    private static final String TAG = "FBubble.Ads";
    private static final String PREFS_NAME  = "FrozenBubblePrefs";
    private static final String KEY_NO_ADS  = "ads_removed";

    /**
     * Least time between two interstitials, whichever triggers they came
     * from. AdMob disallows back-to-back interstitials and more than one per
     * two user actions; without this, Continue then going online, or leaving
     * and rejoining games quickly, showed one each time.
     */
    private static final long MIN_INTERVAL_MS = 3 * 60 * 1000;

    /**
     * A loaded interstitial is thrown away and fetched again after this long:
     * Google treats an ad held for over an hour as stale, and showing one
     * earns nothing.
     */
    private static final long MAX_AD_AGE_MS = 60 * 60 * 1000;

    /** True once prepare() has started the consent check. */
    private static boolean sStarted = false;
    /** True once MobileAds.initialize() has been called. */
    private static boolean sAdsInitialized = false;
    private static boolean sLoading = false;
    private static InterstitialAd sAd = null;
    /** SystemClock.elapsedRealtime() when sAd loaded. */
    private static long sAdLoadedMs = 0;
    /** SystemClock.elapsedRealtime() of the last ad shown, 0 = none yet. */
    private static long sLastShownMs = 0;

    /**
     * Ask for consent where the law needs it, then start AdMob and fetch an
     * interstitial so one is ready by the next break, without showing an ad.
     * Called a few seconds after every launch, and again on network lobby
     * entry (a no-op by then), neither of which is a break: AdMob disallows
     * an interstitial that appears as a screen loads.
     */
    public static void prepare(final Activity activity) {
        if (isAdsRemoved(activity)) return;
        if (BuildConfig.ADMOB_INTERSTITIAL_ID.isEmpty()) return;
        activity.runOnUiThread(() -> start(activity));
    }

    /**
     * Show an interstitial ad if one is ready, ads haven't been removed and
     * the last one was at least MIN_INTERVAL_MS ago. Only for a break the
     * player chose: leaving a network game, or Continue after a loss.
     */
    public static void showLobbyAd(final Activity activity) {
        if (isAdsRemoved(activity)) return;
        if (BuildConfig.ADMOB_INTERSTITIAL_ID.isEmpty()) return;

        activity.runOnUiThread(() -> {
            if (!sStarted) {
                // Nothing started the SDKs yet (a Continue in the first
                // seconds after launch): start them, so the next break has an
                // ad.
                start(activity);
                Log.d(TAG, "No ad ready yet");
                return;
            }
            long now = SystemClock.elapsedRealtime();
            if (sLastShownMs != 0 && now - sLastShownMs < MIN_INTERVAL_MS) {
                Log.d(TAG, "Ad skipped: last one " + (now - sLastShownMs) / 1000 + "s ago");
                return;
            }
            if (sAd != null && now - sAdLoadedMs > MAX_AD_AGE_MS) {
                Log.d(TAG, "Cached ad is stale, fetching a new one");
                sAd = null;
            }
            if (sAd == null) {
                Log.d(TAG, "No ad ready yet");
                loadAd(activity);
                return;
            }
            InterstitialAd ad = sAd;
            sAd = null;
            sLastShownMs = now;
            ad.setFullScreenContentCallback(new FullScreenContentCallback() {
                @Override public void onAdDismissedFullScreenContent() {
                    loadAd(activity);
                }
                @Override public void onAdFailedToShowFullScreenContent(@NonNull AdError error) {
                    Log.w(TAG, "Ad failed to show: " + error);
                    loadAd(activity);
                }
            });
            ad.show(activity);
        });
    }

    /**
     * Opens Google's privacy options form, where a player reviews or changes
     * the consent the ads act on -- the GDPR choices in the EU and UK, and the
     * opt-out of sale/sharing that US state privacy laws require an app to
     * offer at any time, not just at first launch.
     *
     * Asks for the player's consent status first rather than relying on
     * prepare() having done so: the row is reachable before the SDKs start.
     * Where no form applies the player is told so, instead of the tap doing
     * nothing.
     */
    public static void showPrivacyOptions(final Activity activity) {
        if (BuildConfig.ADMOB_INTERSTITIAL_ID.isEmpty()) {
            toast(activity, "This build shows no ads.");
            return;
        }
        activity.runOnUiThread(() -> {
            ConsentInformation info = UserMessagingPlatform.getConsentInformation(activity);
            info.requestConsentInfoUpdate(activity, new ConsentRequestParameters.Builder().build(),
                    () -> activity.runOnUiThread(() -> {
                        ConsentInformation.PrivacyOptionsRequirementStatus status =
                                info.getPrivacyOptionsRequirementStatus();
                        Log.d(TAG, "Privacy options requirement: " + status
                                + ", consent status: " + info.getConsentStatus());
                        if (status != ConsentInformation.PrivacyOptionsRequirementStatus.REQUIRED) {
                            toast(activity, "No ad privacy choices apply where you are.");
                            return;
                        }
                        UserMessagingPlatform.showPrivacyOptionsForm(activity, error -> {
                            if (error != null) {
                                Log.w(TAG, "Privacy options form failed: " + error.getMessage());
                                toast(activity, "Ad privacy choices could not be opened. Try again later.");
                            }
                        });
                    }),
                    error -> {
                        Log.w(TAG, "Consent info update failed: " + error.getMessage());
                        toast(activity, "Ad privacy choices could not be loaded. Check your connection.");
                    });
        });
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
        // it ever shows, and an ad already fetched just goes unused.
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
     * Runs the consent check, then starts AdMob. Caller must be on the UI
     * thread.
     *
     * A few seconds after launch (prepare(), sent by the game once it is
     * running), rather than in onCreate(): the AdMob SDK crashed SDL at
     * startup when initialized there (its worker threads hit a destroyed
     * mutex in HWUI's CommonPool while SDL was bringing up its EGL surface).
     * Starting after the game is already running keeps that out of startup.
     *
     * Where the law asks for consent (GDPR/UK, US state privacy laws), UMP
     * shows the form set up in AdMob's Privacy & messaging, once; ads are
     * only requested once canRequestAds() says so. A consent check that fails
     * (offline) still lets through a player who consented on an earlier
     * launch, as Google's own sample does.
     */
    private static void start(final Activity activity) {
        if (sStarted) return;
        sStarted = true;
        ConsentInformation info = UserMessagingPlatform.getConsentInformation(activity);
        info.requestConsentInfoUpdate(activity, new ConsentRequestParameters.Builder().build(),
                () -> activity.runOnUiThread(() ->
                        UserMessagingPlatform.loadAndShowConsentFormIfRequired(activity, error -> {
                            if (error != null) {
                                Log.w(TAG, "Consent form failed: " + error.getMessage());
                            }
                            if (info.canRequestAds()) initializeAds(activity);
                        })),
                error -> {
                    Log.w(TAG, "Consent info update failed: " + error.getMessage());
                    if (info.canRequestAds()) activity.runOnUiThread(() -> initializeAds(activity));
                });
        // Consent from an earlier launch: no need to wait for the update.
        if (info.canRequestAds()) initializeAds(activity);
    }

    /** Starts the Mobile Ads SDK once, then fetches the first ad. UI thread. */
    private static void initializeAds(final Activity activity) {
        if (sAdsInitialized) return;
        sAdsInitialized = true;
        MobileAds.initialize(activity, status -> {
            Log.d(TAG, "AdMob initialized");
            activity.runOnUiThread(() -> loadAd(activity));
        });
    }

    /**
     * Fetches an interstitial unless one is cached or already on its way.
     * Debug builds ask for Google's always-test unit (see build.gradle), so
     * they can never produce live traffic. UI thread.
     */
    private static void loadAd(final Activity activity) {
        if (!sAdsInitialized || sLoading || sAd != null) return;
        sLoading = true;
        InterstitialAd.load(activity, BuildConfig.ADMOB_INTERSTITIAL_ID,
                new AdRequest.Builder().build(),
                new InterstitialAdLoadCallback() {
                    @Override public void onAdLoaded(@NonNull InterstitialAd ad) {
                        sLoading = false;
                        sAd = ad;
                        sAdLoadedMs = SystemClock.elapsedRealtime();
                        Log.d(TAG, "Ad loaded");
                    }
                    @Override public void onAdFailedToLoad(@NonNull LoadAdError error) {
                        sLoading = false;
                        Log.w(TAG, "Ad failed to load: " + error.getMessage());
                    }
                });
    }
}
