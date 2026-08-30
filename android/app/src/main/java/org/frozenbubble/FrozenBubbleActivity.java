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

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;
import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Frozen Bubble Android TV Activity.
 *
 * SDL3 message protocol (sent from C++ via SDL_SendAndroidMessage):
 *   0x8001 — show interstitial ad (called when entering network lobby)
 *   0x8002 — buy the yearly ad-removal subscription
 *   0x8003 — buy permanent ad removal
 */
public class FrozenBubbleActivity extends SDLActivity {

    // Custom SDL_SendAndroidMessage command codes (must match mainmenu_input.cpp)
    private static final int MSG_SHOW_AD           = 0x8001;
    private static final int MSG_BUY_ADS_YEAR      = 0x8002;
    private static final int MSG_BUY_ADS_FOREVER   = 0x8003;

    /** Extracted asset directory path — read by C++ InitDataDir() via JNI. */
    public static String sExtractedDataDir = "";

    private BillingManager mBillingManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            sExtractedDataDir = AssetExtractor.extractAll(this);
        } catch (Exception e) {
            sExtractedDataDir = "";
            Log.e("FBubble.Assets", "Asset deployment failed; SDL will not start", e);
            Toast.makeText(this,
                    "Game assets could not be prepared. Restart or reinstall the app.",
                    Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        super.onCreate(savedInstanceState);

        // NOTE: AdMob init is intentionally deferred — calling MobileAds.initialize()
        // in onCreate() spawns HWUI worker threads that conflict with SDL's EGL surface,
        // causing a "pthread_mutex_lock on destroyed mutex" crash (HWUI CommonPool).
        // Ads are loaded lazily when C++ sends MSG_SHOW_AD (0x8001).

        // Initialize billing client (restores prior purchases on connect)
        mBillingManager = new BillingManager(this);

        // Ask for notification permission (Android 13+). Cheap and does not
        // touch the SDL surface, unlike the AdMob init deferred above.
        PushManager.init(this);
    }

    @Override
    protected void onDestroy() {
        if (mBillingManager != null) mBillingManager.destroy();
        super.onDestroy();
        // SDL cannot reinitialize in the same process after main() returns.
        // Kill the process so every launch starts clean.
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    /**
     * Called by SDL3 when the C++ side calls SDL_SendAndroidMessage().
     * This runs on the SDL thread — post UI work to the main thread.
     */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        switch (command) {
            case MSG_SHOW_AD:
                AdsManager.showLobbyAd(this);
                return true;
            case MSG_BUY_ADS_YEAR:
                runOnUiThread(() ->
                        mBillingManager.launchPurchaseFlow(BillingManager.PRODUCT_YEAR));
                return true;
            case MSG_BUY_ADS_FOREVER:
                runOnUiThread(() ->
                        mBillingManager.launchPurchaseFlow(BillingManager.PRODUCT_FOREVER));
                return true;
            default:
                return false;
        }
    }

    /**
     * Called from C++ via JNI: is ad removal currently active? Reads the flag
     * BillingManager last derived from Play, so it covers both a permanent
     * purchase and an in-force yearly subscription -- and goes false again once
     * a subscription lapses.
     */
    public static boolean adsRemoved() {
        Context ctx = SDL.getContext();
        if (ctx == null) return false;
        return AdsManager.isAdsRemoved(ctx);
    }

    /**
     * Called from C++ via JNI: the localized price to show on a purchase row,
     * or "" if Play has not answered yet (the row then says so rather than
     * inventing a number). productIndex 0 = yearly, 1 = permanent -- an int
     * rather than a string keeps the JNI signature trivial.
     */
    public static String adsPrice(int productIndex) {
        // No context needed: getPrice() reads the cached ProductDetails and
        // returns "" when Play has not answered yet.
        return BillingManager.getPrice(
                productIndex == 0 ? BillingManager.PRODUCT_YEAR
                                  : BillingManager.PRODUCT_FOREVER);
    }

    /**
     * Called from C++ via JNI to fetch a URL synchronously.
     * Must be called from a background thread (not the main/UI thread).
     * Returns the response body as a string, or "" on error.
     */
    /**
     * Called from C++ via JNI to get this device's FCM push token.
     * Must be called from a background thread (not the main/UI thread) --
     * PushManager.getToken() blocks. Returns "" if there isn't one.
     *
     * A thin wrapper rather than calling PushManager directly by class name:
     * JNI's FindClass("org/frozenbubble/PushManager") fails when called from
     * a native thread the JVM did not create (SDL's game thread is exactly
     * that) because it resolves against the wrong classloader off the main
     * thread -- a well-known JNI trap. Routing through a static method
     * already on FrozenBubbleActivity sidesteps it: the native side reaches
     * this class via GetObjectClass() on the already-valid Activity object
     * (see androidFetchUrl() in networkclient.cpp for the identical pattern),
     * which needs no name-based class lookup at all.
     */
    public static String getPushToken() {
        return PushManager.getToken();
    }

    /**
     * Called from C++ via JNI. True on a TV box, false on a phone or tablet.
     *
     * The game needs this to pick a screen orientation, an aim default and a
     * chat-keyboard behaviour, and it cannot ask whether a touchscreen exists
     * to find out: <em>Android TV devices report that they have one.</em>
     * Google's own Android TV emulator lists
     * {@code android.hardware.touchscreen} in {@code pm list features}, and
     * SDL is looser still -- {@code SDLActivity.initTouch()} registers a touch
     * device for any {@code InputDevice.isVirtual()}, which every Android
     * device has, so {@code SDL_GetTouchDevices()} is non-empty even on a TV.
     * Asking about touch hardware therefore answers "yes" everywhere and
     * discriminates nothing.
     *
     * UI mode is the question that actually has different answers:
     * {@code UI_MODE_TYPE_TELEVISION} is what the platform sets for a
     * leanback device. {@code FEATURE_LEANBACK} is checked as a fallback for
     * boxes that under-report their UI mode.
     *
     * Wrapped as a static on this class for the same JNI-classloader reason
     * as {@link #getPushToken()} above.
     */
    public static boolean isTelevision() {
        Context ctx = SDL.getContext();
        if (ctx == null) {
            return false;
        }
        UiModeManager uiMode =
                (UiModeManager) ctx.getSystemService(Context.UI_MODE_SERVICE);
        if (uiMode != null
                && uiMode.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION) {
            return true;
        }
        PackageManager pm = ctx.getPackageManager();
        return pm != null
                && (pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
                    || pm.hasSystemFeature("android.hardware.type.television"));
    }

    /**
     * Called from C++ via JNI. True on a tablet, false on a phone or TV box.
     *
     * A phone and a tablet both report a touchscreen, so
     * DeviceHasTouchscreen() (native side) cannot tell them apart the way
     * isTelevision() above already separates a TV box -- this fills that
     * gap. {@code smallestScreenWidthDp >= 600} is the platform's own
     * phone/tablet threshold: it is what the "sw600dp" resource-qualifier
     * bucket means, and what Android's developer guidance recommends for
     * this exact split.
     *
     * Wrapped as a static on this class for the same JNI-classloader reason
     * as {@link #getPushToken()} above.
     */
    public static boolean isTablet() {
        Context ctx = SDL.getContext();
        if (ctx == null) {
            return false;
        }
        Configuration config = ctx.getResources().getConfiguration();
        return config.smallestScreenWidthDp >= 600;
    }

    public static String fetchUrl(String urlStr) {
        try {
            URL url = new URL(urlStr);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(8000);
            conn.setRequestMethod("GET");
            conn.setRequestProperty("User-Agent", "FrozenBubble-SDL3/1.0");
            int code = conn.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) return "";
            BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            reader.close();
            return sb.toString();
        } catch (Exception e) {
            return "";
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "SDL3_image",
            "SDL3_mixer",
            "SDL3_ttf",
            "main"
        };
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
            keyCode == KeyEvent.KEYCODE_BUTTON_A) {
            return super.onKeyDown(KeyEvent.KEYCODE_ENTER, event);
        }
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return super.onKeyDown(KeyEvent.KEYCODE_ESCAPE, event);
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
            keyCode == KeyEvent.KEYCODE_BUTTON_A) {
            return super.onKeyUp(KeyEvent.KEYCODE_ENTER, event);
        }
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return super.onKeyUp(KeyEvent.KEYCODE_ESCAPE, event);
        }
        return super.onKeyUp(keyCode, event);
    }
}
