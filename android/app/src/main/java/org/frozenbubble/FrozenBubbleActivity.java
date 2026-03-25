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

import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.view.KeyEvent;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Frozen Bubble Android TV Activity.
 *
 * SDL2 message protocol (sent from C++ via SDL_AndroidSendMessage):
 *   0x8001 — show interstitial ad (called when entering network lobby)
 *   0x8002 — launch "Remove Ads" IAP purchase flow
 */
public class FrozenBubbleActivity extends SDLActivity {

    // Custom SDL_AndroidSendMessage command codes (must match mainmenu.cpp)
    private static final int MSG_SHOW_AD      = 0x8001;
    private static final int MSG_REMOVE_ADS   = 0x8002;

    /** Extracted asset directory path — read by C++ InitDataDir() via JNI. */
    public static String sExtractedDataDir = "";

    private BillingManager mBillingManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Extract APK assets to writable internal storage BEFORE SDL starts.
        // C++ code uses fopen() which cannot read APK assets directly — they
        // must be copied to the filesystem first. This is fast on subsequent
        // launches (version marker detected, extraction skipped).
        // Store the result so C++ can read the exact same path via JNI.
        sExtractedDataDir = AssetExtractor.extractAll(this);

        super.onCreate(savedInstanceState);

        // NOTE: AdMob init is intentionally deferred — calling MobileAds.initialize()
        // in onCreate() spawns HWUI worker threads that conflict with SDL's EGL surface,
        // causing a "pthread_mutex_lock on destroyed mutex" crash (HWUI CommonPool).
        // Ads are loaded lazily when C++ sends MSG_SHOW_AD (0x8001).

        // Initialize billing client (restores prior purchases on connect)
        mBillingManager = new BillingManager(this);
    }

    @Override
    protected void onDestroy() {
        if (mBillingManager != null) mBillingManager.destroy();
        super.onDestroy();
    }

    /**
     * Called by SDL2 when the C++ side calls SDL_AndroidSendMessage().
     * This runs on the SDL thread — post UI work to the main thread.
     */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        switch (command) {
            case MSG_SHOW_AD:
                AdsManager.showLobbyAd(this);
                return true;
            case MSG_REMOVE_ADS:
                runOnUiThread(() -> mBillingManager.launchPurchaseFlow());
                return true;
            default:
                return false;
        }
    }

    /**
     * Called from C++ via JNI to fetch a URL synchronously.
     * Must be called from a background thread (not the main/UI thread).
     * Returns the response body as a string, or "" on error.
     */
    public static String fetchUrl(String urlStr) {
        try {
            URL url = new URL(urlStr);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(8000);
            conn.setRequestMethod("GET");
            conn.setRequestProperty("User-Agent", "FrozenBubble-SDL2/1.0");
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
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "SDL2_ttf",
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
