### Task 1: Transactional Android managed-asset deployment (BUG-046)

**Files:**
- Create:
  `android/app/src/main/java/org/frozenbubble/AssetDeployment.java`
- Create:
  `android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java`
- Modify:
  `android/app/src/main/java/org/frozenbubble/AssetExtractor.java`
- Modify:
  `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java`
- Modify: `android/app/build.gradle`

**Interfaces:**
- Produces:

```java
final class AssetDeployment {
    static final String MARKER_PREFIX = "schema-2:";

    interface AssetSource {
        String[] list(String path) throws IOException;
        InputStream open(String path) throws IOException;
    }

    static File deploy(AssetSource source, File filesDir, long versionCode)
            throws IOException;
}
```

- `AssetDeployment.deploy(...)` returns exactly `new File(filesDir, "share")`
  after a complete deployment or throws `IOException` before a current marker
  exists.
- `AssetExtractor.extractAll(Context)` remains the public Android entry, now
  declares/propagates `IOException`, and adapts `AssetManager.list/open` to
  `AssetSource`.
- `FrozenBubbleActivity.sExtractedDataDir` is assigned only after successful
  deployment; `super.onCreate()` is not called on failure.

- [ ] **Step 1: Add the failing JVM behavior tests**

Add JUnit 4 to `android/app/build.gradle`:

```gradle
dependencies {
    testImplementation 'junit:junit:4.13.2'
    implementation 'com.google.android.gms:play-services-ads:23.3.0'
    implementation 'com.android.billingclient:billing:7.1.1'
}
```

Create `AssetDeploymentTest` with a `FakeAssetSource` backed by literal byte
arrays and temporary directories. Each test names the production break it
catches and derives expected bytes directly from literals. Required cases:

```java
@Test public void freshDeployWritesAllAssetsAndSchemaMarker()
@Test public void legacyMarkerForcesCompleteRebuild()
@Test public void rebuildReplacesChangedAndTruncatedFiles()
@Test public void rebuildRemovesDeletedAssets()
@Test public void rebuildHandlesFileDirectoryShapeChanges()
@Test public void copyFailureDoesNotCommitCurrentMarker()
@Test public void retryAfterFailureBuildsACompleteTree()
@Test public void rebuildPreservesPreferenceSiblings()
```

The shape-change test performs two independent deployments: source v1 has a
file at `gfx/swap`, source v2 has `gfx/swap/child.png`; then reverse the shape
using another path. The sibling test places literal `settings.ini`,
`highscores`, and `highlevelshistory` files directly under `filesDir` and
asserts their bytes are unchanged after deployment.

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

Expected: compilation fails because `AssetDeployment` and its `AssetSource`
contract do not exist. This is the intended RED; Gradle/SDK setup failures do
not count.

- [ ] **Step 3: Implement the minimum pure-Java deployment algorithm**

In `AssetDeployment.deploy(...)`:

```java
File managedRoot = new File(filesDir, "share");
File marker = new File(filesDir, ".assets_version");
File markerTemp = new File(filesDir, ".assets_version.tmp");
String wanted = MARKER_PREFIX + versionCode;
String installed = marker.isFile() ? readUtf8(marker).trim() : "";

if (wanted.equals(installed) && managedRoot.isDirectory()) {
    return managedRoot;
}

deleteIfPresent(marker);
deleteIfPresent(markerTemp);
deleteRecursively(managedRoot);
if (!managedRoot.mkdirs() && !managedRoot.isDirectory()) {
    throw new IOException("Could not create managed asset root: " + managedRoot);
}
extractEntry(source, "", managedRoot);
writeAtomically(markerTemp, marker, wanted.getBytes(StandardCharsets.UTF_8));
return managedRoot;
```

`extractEntry` recurses from the APK asset root. A leaf is copied to
`<destination>.tmp`, both streams are closed with try-with-resources, then the
temporary file is renamed into place. `deleteRecursively` rejects any target
other than the `managedRoot` supplied internally and throws on every failed
delete. Removing the old marker before touching `share/` is load-bearing: a
failed rebuild must never leave a current marker beside a partial directory.
Marker replacement and file replacement use same-directory renames so API 21
and Java 8 remain sufficient; do not introduce `java.nio.file.Files` APIs that
require newer Android behavior.

No catch inside the deployment core may convert an exception into success.

- [ ] **Step 4: Adapt Android startup and fail closed**

Make `AssetExtractor` a thin adapter:

```java
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
```

Import `Build`, `PackageInfo`, and `PackageManager`. The guarded legacy
`versionCode` branch is required for API 21--27; do not call
`getLongVersionCode()` unconditionally.

At the start of `FrozenBubbleActivity.onCreate()`:

```java
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
```

Import only `Log` and `Toast`; preserve the existing ordering and JNI field.

- [ ] **Step 5: Run focused and Android build verification**

Run:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
./gradlew :app:assembleDebug --no-daemon
```

Expected: all eight JVM cases pass and the Android debug APK assembles.

- [ ] **Step 6: Commit Task 1**

```bash
git add android/app/build.gradle \
  android/app/src/main/java/org/frozenbubble/AssetDeployment.java \
  android/app/src/main/java/org/frozenbubble/AssetExtractor.java \
  android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java \
  android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java
git commit -m "fix(android): deploy packaged assets transactionally (BUG-046)"
```

---

