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

import android.content.Context;
import android.content.res.AssetManager;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import java.io.IOException;
import java.io.InputStream;

/**
 * Extracts APK assets to writable internal storage so C++ code can use
 * fopen() on them. Android's APK asset manager is not accessible via
 * standard POSIX file I/O — files must be copied out first.
 *
 * Assets are extracted once per app version to getFilesDir()/share/.
 * A version marker file prevents redundant re-extraction on subsequent launches.
 */
public class AssetExtractor {
    /**
     * Extracts all APK assets to getFilesDir()/share/ if not already done.
     * Must be called before SDL starts (before super.onCreate in SDLActivity).
     *
     * @return absolute path to the extracted share/ directory
     */
    public static String extractAll(Context context) throws IOException {
        PackageInfo packageInfo;
        try {
            packageInfo = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IOException("Could not determine package version", e);
        }
        long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                ? packageInfo.getLongVersionCode()
                : packageInfo.versionCode;
        AssetManager manager = context.getAssets();
        AssetDeployment.AssetSource source = new AssetDeployment.AssetSource() {
            public String[] list(String path) throws IOException {
                return manager.list(path);
            }

            public InputStream open(String path) throws IOException {
                return manager.open(path);
            }
        };
        return AssetDeployment.deploy(source, context.getFilesDir(), versionCode)
                .getAbsolutePath();
    }
}
