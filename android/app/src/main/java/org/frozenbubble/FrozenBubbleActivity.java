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

    private BillingManager mBillingManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initialize AdMob (no-op if ads already removed)
        AdsManager.init(this);

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
