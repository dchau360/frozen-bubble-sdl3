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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

final class AssetDeployment {
    static final String MARKER_PREFIX = "schema-2:";

    interface AssetSource {
        String[] list(String path) throws IOException;
        InputStream open(String path) throws IOException;
    }

    static File deploy(AssetSource source, File filesDir, long versionCode)
            throws IOException {
        File managedRoot = new File(filesDir, "share");
        File marker = new File(filesDir, ".assets_version");
        File markerTemp = new File(filesDir, ".assets_version.tmp");
        String wanted = MARKER_PREFIX + versionCode;
        String installed = marker.isFile() ? readUtf8(marker).trim() : "";

        if (wanted.equals(installed) && !isSymbolicLink(managedRoot)
                && managedRoot.isDirectory()) {
            return managedRoot;
        }

        deleteIfPresent(marker);
        deleteIfPresent(markerTemp);
        deleteRecursively(managedRoot, managedRoot);
        if (!managedRoot.mkdirs() && !managedRoot.isDirectory()) {
            throw new IOException("Could not create managed asset root: " + managedRoot);
        }
        extractEntry(source, "", managedRoot);
        writeAtomically(markerTemp, marker, wanted.getBytes(StandardCharsets.UTF_8));
        return managedRoot;
    }

    private static void extractEntry(AssetSource source, String assetPath, File destination)
            throws IOException {
        String[] children = source.list(assetPath);
        if (children == null || children.length == 0) {
            if (!assetPath.isEmpty()) {
                copyLeaf(source, assetPath, destination);
            }
            return;
        }

        if (!destination.mkdirs() && !destination.isDirectory()) {
            throw new IOException("Could not create asset directory: " + destination);
        }
        for (String child : children) {
            String childPath = assetPath.isEmpty() ? child : assetPath + "/" + child;
            extractEntry(source, childPath, new File(destination, child));
        }
    }

    private static void copyLeaf(AssetSource source, String assetPath, File destination)
            throws IOException {
        File temporary = new File(destination.getPath() + ".tmp");
        deleteIfPresent(temporary);
        try (InputStream input = source.open(assetPath);
             FileOutputStream output = new FileOutputStream(temporary)) {
            byte[] buffer = new byte[32768];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
        if (!temporary.renameTo(destination)) {
            throw new IOException("Could not replace asset file: " + destination);
        }
    }

    private static String readUtf8(File file) throws IOException {
        try (InputStream input = new FileInputStream(file)) {
            byte[] bytes = new byte[(int) file.length()];
            int offset = 0;
            while (offset < bytes.length) {
                int count = input.read(bytes, offset, bytes.length - offset);
                if (count < 0) {
                    throw new IOException("Unexpected end of marker: " + file);
                }
                offset += count;
            }
            return new String(bytes, StandardCharsets.UTF_8);
        }
    }

    private static void writeAtomically(File temporary, File destination, byte[] contents)
            throws IOException {
        deleteIfPresent(temporary);
        try (FileOutputStream output = new FileOutputStream(temporary)) {
            output.write(contents);
        }
        if (!temporary.renameTo(destination)) {
            throw new IOException("Could not replace asset marker: " + destination);
        }
    }

    private static void deleteIfPresent(File file) throws IOException {
        if (file.exists() && !file.delete()) {
            throw new IOException("Could not delete: " + file);
        }
    }

    private static void deleteRecursively(File target, File managedRoot) throws IOException {
        if (!target.equals(managedRoot)) {
            throw new IOException("Refusing to delete unmanaged path: " + target);
        }
        deleteTree(target);
    }

    private static void deleteTree(File file) throws IOException {
        if (isSymbolicLink(file)) {
            if (!file.delete()) {
                throw new IOException("Could not unlink symbolic link: " + file);
            }
            return;
        }
        if (!file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) {
                throw new IOException("Could not list directory: " + file);
            }
            for (File child : children) {
                deleteTree(child);
            }
        }
        if (!file.delete()) {
            throw new IOException("Could not delete: " + file);
        }
    }

    private static boolean isSymbolicLink(File file) throws IOException {
        File parent = file.getParentFile();
        if (parent == null) {
            return false;
        }
        File canonicalParent = parent.getCanonicalFile();
        File canonicalFile = file.getCanonicalFile();
        return !canonicalParent.equals(canonicalFile.getParentFile());
    }
}
