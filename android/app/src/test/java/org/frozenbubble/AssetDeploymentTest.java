package org.frozenbubble;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.After;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class AssetDeploymentTest {
    private final List<File> temporaryDirectories = new ArrayList<>();

    @After
    public void removeTemporaryDirectories() throws IOException {
        for (File directory : temporaryDirectories) {
            deleteRecursively(directory);
        }
    }

    @Test
    public void freshDeployWritesAllAssetsAndSchemaMarker() throws Exception {
        File filesDir = temporaryDirectory();
        FakeAssetSource source = sourceOf(
                "gfx/red.png", "red-bytes",
                "snd/pop.wav", "pop-bytes");

        File deployed = AssetDeployment.deploy(source, filesDir, 42L);

        assertEquals(new File(filesDir, "share"), deployed);
        assertFileBytes(new File(deployed, "gfx/red.png"), "red-bytes");
        assertFileBytes(new File(deployed, "snd/pop.wav"), "pop-bytes");
        assertFileBytes(new File(filesDir, ".assets_version"), "schema-2:42");
    }

    @Test
    public void legacyMarkerForcesCompleteRebuild() throws Exception {
        File filesDir = temporaryDirectory();
        File share = new File(filesDir, "share");
        writeFile(new File(share, "gfx/red.png"), "stale-red");
        writeFile(new File(share, "obsolete.txt"), "obsolete");
        writeFile(new File(filesDir, ".assets_version"), "42");

        AssetDeployment.deploy(sourceOf("gfx/red.png", "fresh-red"), filesDir, 42L);

        assertFileBytes(new File(share, "gfx/red.png"), "fresh-red");
        assertFalse(new File(share, "obsolete.txt").exists());
        assertFileBytes(new File(filesDir, ".assets_version"), "schema-2:42");
    }

    @Test
    public void rebuildReplacesChangedAndTruncatedFiles() throws Exception {
        File filesDir = temporaryDirectory();
        AssetDeployment.deploy(sourceOf(
                "gfx/changed.png", "long-original-bytes",
                "gfx/truncated.png", "this file used to be longer"), filesDir, 42L);

        AssetDeployment.deploy(sourceOf(
                "gfx/changed.png", "replacement",
                "gfx/truncated.png", "tiny"), filesDir, 43L);

        File share = new File(filesDir, "share");
        assertFileBytes(new File(share, "gfx/changed.png"), "replacement");
        assertFileBytes(new File(share, "gfx/truncated.png"), "tiny");
    }

    @Test
    public void rebuildRemovesDeletedAssets() throws Exception {
        File filesDir = temporaryDirectory();
        AssetDeployment.deploy(sourceOf(
                "gfx/kept.png", "first-kept",
                "gfx/deleted.png", "first-deleted"), filesDir, 42L);

        AssetDeployment.deploy(sourceOf("gfx/kept.png", "second-kept"), filesDir, 43L);

        File share = new File(filesDir, "share");
        assertFileBytes(new File(share, "gfx/kept.png"), "second-kept");
        assertFalse(new File(share, "gfx/deleted.png").exists());
    }

    @Test
    public void rebuildHandlesFileDirectoryShapeChanges() throws Exception {
        File filesDir = temporaryDirectory();
        AssetDeployment.deploy(sourceOf("gfx/swap", "a-file"), filesDir, 42L);

        AssetDeployment.deploy(sourceOf("gfx/swap/child.png", "a-child"), filesDir, 43L);

        File share = new File(filesDir, "share");
        assertTrue(new File(share, "gfx/swap").isDirectory());
        assertFileBytes(new File(share, "gfx/swap/child.png"), "a-child");

        AssetDeployment.deploy(sourceOf("snd/reverse/child.wav", "first-child"), filesDir, 44L);
        AssetDeployment.deploy(sourceOf("snd/reverse", "a-file-again"), filesDir, 45L);

        assertTrue(new File(share, "snd/reverse").isFile());
        assertFileBytes(new File(share, "snd/reverse"), "a-file-again");
    }

    @Test
    public void copyFailureDoesNotCommitCurrentMarker() throws Exception {
        File filesDir = temporaryDirectory();
        FakeAssetSource source = sourceOf(
                "gfx/copied.png", "copied-before-error",
                "gfx/broken.png", "never-copied");
        source.failWhenOpening("gfx/broken.png");

        try {
            AssetDeployment.deploy(source, filesDir, 42L);
            fail("copy failure must propagate as IOException");
        } catch (IOException expected) {
            assertFalse(new File(filesDir, ".assets_version").exists());
        }
    }

    @Test
    public void retryAfterFailureBuildsACompleteTree() throws Exception {
        File filesDir = temporaryDirectory();
        FakeAssetSource source = sourceOf(
                "gfx/copied.png", "copied-on-retry",
                "gfx/broken.png", "recovered-on-retry");
        source.failWhenOpening("gfx/broken.png");

        try {
            AssetDeployment.deploy(source, filesDir, 42L);
            fail("initial copy failure must propagate");
        } catch (IOException expected) {
            // The next launch retries from a tree without a current marker.
        }
        source.stopFailing();

        File deployed = AssetDeployment.deploy(source, filesDir, 42L);

        assertFileBytes(new File(deployed, "gfx/copied.png"), "copied-on-retry");
        assertFileBytes(new File(deployed, "gfx/broken.png"), "recovered-on-retry");
        assertFileBytes(new File(filesDir, ".assets_version"), "schema-2:42");
    }

    @Test
    public void rebuildPreservesPreferenceSiblings() throws Exception {
        File filesDir = temporaryDirectory();
        writeFile(new File(filesDir, "settings.ini"), "saved-settings");
        writeFile(new File(filesDir, "highscores"), "saved-scores");
        writeFile(new File(filesDir, "highlevelshistory"), "saved-history");

        AssetDeployment.deploy(sourceOf("gfx/red.png", "red"), filesDir, 42L);
        AssetDeployment.deploy(sourceOf("gfx/blue.png", "blue"), filesDir, 43L);

        assertFileBytes(new File(filesDir, "settings.ini"), "saved-settings");
        assertFileBytes(new File(filesDir, "highscores"), "saved-scores");
        assertFileBytes(new File(filesDir, "highlevelshistory"), "saved-history");
    }

    @Test
    public void rebuildUnlinksManagedRootSymlinkWithoutDeletingTarget() throws Exception {
        File filesDir = temporaryDirectory();
        File outsideDirectory = temporaryDirectory();
        writeFile(new File(outsideDirectory, "must-survive.txt"), "outside-data");
        File share = new File(filesDir, "share");
        Files.createSymbolicLink(share.toPath(), outsideDirectory.toPath());
        writeFile(new File(filesDir, ".assets_version"), "schema-2:41");

        File deployed = AssetDeployment.deploy(sourceOf("gfx/red.png", "new-asset"), filesDir, 42L);

        assertFileBytes(new File(outsideDirectory, "must-survive.txt"), "outside-data");
        assertFalse(Files.isSymbolicLink(deployed.toPath()));
        assertFileBytes(new File(deployed, "gfx/red.png"), "new-asset");
    }

    private File temporaryDirectory() throws IOException {
        File directory = Files.createTempDirectory("asset-deployment-test").toFile();
        temporaryDirectories.add(directory);
        return directory;
    }

    private static FakeAssetSource sourceOf(String... pathAndContents) {
        if (pathAndContents.length % 2 != 0) {
            throw new IllegalArgumentException("paths must have literal contents");
        }
        FakeAssetSource source = new FakeAssetSource();
        for (int i = 0; i < pathAndContents.length; i += 2) {
            source.add(pathAndContents[i], pathAndContents[i + 1]);
        }
        return source;
    }

    private static void assertFileBytes(File file, String expected) throws IOException {
        assertTrue("expected file: " + file, file.isFile());
        assertArrayEquals(expected.getBytes(StandardCharsets.UTF_8), readFile(file));
    }

    private static byte[] readFile(File file) throws IOException {
        try (InputStream input = new FileInputStream(file)) {
            byte[] bytes = new byte[(int) file.length()];
            int offset = 0;
            while (offset < bytes.length) {
                int count = input.read(bytes, offset, bytes.length - offset);
                if (count < 0) {
                    throw new IOException("Unexpected end of file: " + file);
                }
                offset += count;
            }
            return bytes;
        }
    }

    private static void writeFile(File file, String contents) throws IOException {
        File parent = file.getParentFile();
        if (!parent.exists() && !parent.mkdirs()) {
            throw new IOException("Could not create " + parent);
        }
        java.io.FileOutputStream output = new java.io.FileOutputStream(file);
        try {
            output.write(contents.getBytes(StandardCharsets.UTF_8));
        } finally {
            output.close();
        }
    }

    private static void deleteRecursively(File file) throws IOException {
        if (!file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) {
                throw new IOException("Could not list " + file);
            }
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        if (!file.delete()) {
            throw new IOException("Could not delete " + file);
        }
    }

    private static final class FakeAssetSource implements AssetDeployment.AssetSource {
        private final Map<String, byte[]> assets = new LinkedHashMap<>();
        private String failingPath;

        void add(String path, String contents) {
            assets.put(path, contents.getBytes(StandardCharsets.UTF_8));
        }

        void failWhenOpening(String path) {
            failingPath = path;
        }

        void stopFailing() {
            failingPath = null;
        }

        @Override
        public String[] list(String path) {
            String prefix = path.isEmpty() ? "" : path + "/";
            List<String> children = new ArrayList<>();
            for (String assetPath : assets.keySet()) {
                if (!assetPath.startsWith(prefix)) {
                    continue;
                }
                String remainder = assetPath.substring(prefix.length());
                int separator = remainder.indexOf('/');
                String child = separator < 0 ? remainder : remainder.substring(0, separator);
                if (!children.contains(child)) {
                    children.add(child);
                }
            }
            return children.toArray(new String[children.size()]);
        }

        @Override
        public InputStream open(String path) throws IOException {
            if (path.equals(failingPath)) {
                throw new IOException("forced copy failure: " + path);
            }
            byte[] contents = assets.get(path);
            if (contents == null) {
                throw new IOException("unknown asset: " + path);
            }
            return new ByteArrayInputStream(contents);
        }
    }
}
