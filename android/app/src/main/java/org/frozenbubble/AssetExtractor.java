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
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
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
    private static final String TAG = "FBubble.Assets";

    /**
     * Extracts all APK assets to getFilesDir()/share/ if not already done.
     * Must be called before SDL starts (before super.onCreate in SDLActivity).
     *
     * @return absolute path to the extracted share/ directory
     */
    public static String extractAll(Context context) {
        File destDir = new File(context.getFilesDir(), "share");
        File markerFile = new File(context.getFilesDir(), ".assets_version");

        long versionCode = 0;
        try {
            versionCode = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0)
                    .getLongVersionCode();
        } catch (Exception e) {
            Log.w(TAG, "Could not get version code: " + e.getMessage());
        }

        // Skip if already extracted for this app version
        if (markerFile.exists()) {
            try {
                byte[] data = readBytes(markerFile);
                if (data != null && new String(data).trim().equals(String.valueOf(versionCode))) {
                    Log.d(TAG, "Assets already extracted (v" + versionCode + ") at " + destDir);
                    return destDir.getAbsolutePath();
                }
            } catch (Exception e) { /* fall through to re-extract */ }
        }

        Log.i(TAG, "Extracting assets to " + destDir.getAbsolutePath() + " ...");
        destDir.mkdirs();
        extractDir(context.getAssets(), "", destDir.getAbsolutePath());

        // Write version marker so we skip extraction next launch
        try {
            FileWriter fw = new FileWriter(markerFile);
            fw.write(String.valueOf(versionCode));
            fw.close();
        } catch (Exception e) {
            Log.w(TAG, "Could not write version marker: " + e.getMessage());
        }

        Log.i(TAG, "Asset extraction complete");
        return destDir.getAbsolutePath();
    }

    /** Recursively extract an asset directory or file. */
    private static void extractDir(AssetManager assets, String assetPath, String destPath) {
        try {
            String[] children = assets.list(assetPath);
            if (children == null || children.length == 0) {
                // Leaf file
                if (!assetPath.isEmpty()) {
                    extractFile(assets, assetPath, destPath);
                }
            } else {
                // Directory — recurse
                new File(destPath).mkdirs();
                for (String child : children) {
                    String subAsset = assetPath.isEmpty() ? child : assetPath + "/" + child;
                    String subDest  = destPath + "/" + child;
                    extractDir(assets, subAsset, subDest);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error processing '" + assetPath + "': " + e.getMessage());
        }
    }

    /** Copy a single APK asset to a destination file path. */
    private static void extractFile(AssetManager assets, String assetPath, String destPath) {
        File dest = new File(destPath);
        if (dest.exists() && dest.length() > 0) return; // already extracted
        try {
            InputStream in = assets.open(assetPath);
            FileOutputStream out = new FileOutputStream(dest);
            byte[] buf = new byte[32768];
            int len;
            while ((len = in.read(buf)) > 0) out.write(buf, 0, len);
            in.close();
            out.close();
        } catch (Exception e) {
            Log.e(TAG, "Failed to extract '" + assetPath + "': " + e.getMessage());
        }
    }

    private static byte[] readBytes(File f) {
        try (FileInputStream fis = new FileInputStream(f)) {
            byte[] data = new byte[(int) f.length()];
            fis.read(data);
            return data;
        } catch (Exception e) {
            return null;
        }
    }
}
