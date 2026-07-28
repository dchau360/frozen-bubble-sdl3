# SDL3 Audit File Coverage

Pinned production tree: `09d6c7bfcd864a0ad3951b87d16a88dc770392a3` (`v2.4.27`)

Inventory rule: paths selected by Task 1 Step 4 from the pinned tree. The ledger has 237 rows, one per selected path. `Pending review` and boundary-review dispositions are bootstrap states, not completed coverage claims.

| Path | Gate | Disposition | Evidence | Notes |
|---|---|---|---|---|
| `.github/workflows/build.yml` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `CMakeLists.txt` | Task 9 | Baseline exercised; static review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Configured four clean Ninja trees, built Release/sanitizer targets, registered and ran five tests, and emitted the analyzer compile database; semantic build review remains Task 9. |
| `CMakeListsEmscripten.txt` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `README.md` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `SetupServer.md` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `WASM_PORT.md` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/SETUP.md` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/CMakeLists.txt` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/build.gradle` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/jni/SDL3` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL gitlink; internals excluded, integration/build boundary retained. |
| `android/app/jni/SDL3_image` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL gitlink; internals excluded, integration/build boundary retained. |
| `android/app/jni/SDL3_mixer` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL gitlink; internals excluded, integration/build boundary retained. |
| `android/app/jni/SDL3_ttf` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL gitlink; internals excluded, integration/build boundary retained. |
| `android/app/jni/include/SDL2/SDL.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_assert.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_atomic.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_audio.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_bits.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_blendmode.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_clipboard.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config.h.cmake` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config.h.in` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_android.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_emscripten.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_iphoneos.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_macosx.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_minimal.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_ngage.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_os2.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_pandora.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_windows.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_wingdk.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_winrt.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_config_xbox.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_copying.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_cpuinfo.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_egl.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_endian.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_error.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_events.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_filesystem.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_gamecontroller.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_gesture.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_guid.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_haptic.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_hidapi.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_hints.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_image.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_joystick.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_keyboard.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_keycode.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_loadso.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_locale.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_log.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_main.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_messagebox.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_metal.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_misc.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_mixer.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_mouse.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_mutex.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_name.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengl.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengl_glext.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles2.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles2_gl2.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles2_gl2ext.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles2_gl2platform.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_opengles2_khrplatform.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_pixels.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_platform.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_power.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_quit.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_rect.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_render.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_revision.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_revision.h.cmake` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_rwops.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_scancode.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_sensor.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_shape.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_stdinc.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_surface.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_system.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_syswm.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_assert.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_common.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_compare.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_crc32.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_font.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_fuzzer.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_harness.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_images.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_log.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_md5.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_memory.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_test_random.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_thread.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_timer.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_touch.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_ttf.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_types.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_version.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_video.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/SDL_vulkan.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/begin_code.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/include/SDL2/close_code.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored legacy SDL2 header; internals excluded, include/build boundary retained. |
| `android/app/jni/iniparser/dictionary.c` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored duplicate of third_party/iniparser (byte-identical at bootstrap); internals excluded, integration/build boundary retained. |
| `android/app/jni/iniparser/dictionary.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored duplicate of third_party/iniparser (byte-identical at bootstrap); internals excluded, integration/build boundary retained. |
| `android/app/jni/iniparser/iniparser.c` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored duplicate of third_party/iniparser (byte-identical at bootstrap); internals excluded, integration/build boundary retained. |
| `android/app/jni/iniparser/iniparser.h` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored duplicate of third_party/iniparser (byte-identical at bootstrap); internals excluded, integration/build boundary retained. |
| `android/app/src/main/AndroidManifest.xml` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/java/org/frozenbubble/AdsManager.java` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/java/org/frozenbubble/AssetExtractor.java` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/java/org/frozenbubble/BillingManager.java` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/java/org/libsdl/app/HIDDevice.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/HIDDeviceBLESteamController.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/HIDDeviceManager.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/HIDDeviceUSB.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDL.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLActivity.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLAudioManager.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLControllerManager.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLDummyEdit.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLInputConnection.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/java/org/libsdl/app/SDLSurface.java` | Tasks 8-9 | Vendored; boundary review pending | - | Vendored SDL Android Java; internals excluded, project-facing lifecycle/build boundary retained. |
| `android/app/src/main/res/drawable/tv_banner.png` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/res/mipmap-hdpi/ic_launcher.png` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/app/src/main/res/values/strings.xml` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/build.gradle` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/gradle.properties` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/gradle/wrapper/gradle-wrapper.jar` | Tasks 8-9 | Generated/platform-derived validation pending | - | Binary Gradle wrapper artifact; validate through wrapper metadata and execution. |
| `android/gradle/wrapper/gradle-wrapper.properties` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/gradlew` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/gradlew.bat` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `android/settings.gradle` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `cmake/Emscripten.cmake` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `cmake/Findiniparser.cmake` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `cmake/cmake_uninstall.cmake.in` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `default.nix` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/Dockerfile` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/docker-compose.yml` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/nginx.conf` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/setup.sh` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/ssl/.gitignore` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/ssl/fullchain.pem.example` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `docker/ssl/privkey.pem.example` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `flake.lock` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `flake.nix` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `netlify.toml` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `server/AUTHORS` | Task 3 | Static reviewed; Task 3B pending | [Task 3A coverage](subsystems/01-server-protocol.md#coverage) | License/attribution boundary read; no runtime behavior. |
| `server/CMakeLists.txt` | Task 3 | Baseline exercised; static reviewed; Task 3B pending | [Task 3A build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Target/source/link boundary traced; REL-004 records conflicting server versions and REL-003 retains the Windows runtime dependency. |
| `server/README` | Task 3 | Static reviewed; Task 3B pending | [Task 3A build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Bridge, security, scale, and upload-limit claims checked against code; BUG-004 contradicts the documented limiter. |
| `server/fb-server.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A lifecycle map](subsystems/01-server-protocol.md#acceptance-input-retention-dispatch-and-teardown) | Startup, stats/atexit, daemon, and event-loop ownership traced. |
| `server/game.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) | Room/player allocation, command authority, sender identity, priority relay, stats, part/kick/closure, and every fixed-array index traced. |
| `server/game.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) | Public protocol/lifecycle declarations and fd-indexed identity globals reconciled with definitions. |
| `server/init/README` | Task 3 | Static reviewed; Task 3B pending | [Task 3A build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Legacy SysV configuration/install guidance reviewed; modernization remains Task 9. |
| `server/init/fb-server` | Task 3 | Static reviewed; Task 3B pending | [Task 3A build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Daemon/config/stop boundary reviewed; legacy quoting/service management retained for Task 9. |
| `server/init/fb-server.conf` | Task 3 | Static reviewed; Task 3B pending | [Task 3A build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Skeleton parameter format matches `create_server()` configuration parsing. |
| `server/log.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) | Fixed-size formatting, allocation, syslog/stderr paths, and signal reachability reviewed. |
| `server/log.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) | Logging interface, variadic format checking, and global state declarations reviewed. |
| `server/net.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) | Accept/select, two-buffer retention, TCP/UDP ingress, direct output, daemon signals, master HTTP, fd guards, and teardown traced; SEC-002/005 and BUG-002/004/007 recorded. |
| `server/net.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A lifecycle map](subsystems/01-server-protocol.md#acceptance-input-retention-dispatch-and-teardown) | Network API/global ownership reviewed; IMP-001 prototype cleanup recorded. |
| `server/stats.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) | Hash/file ownership, persistence formats, daily reset, synchronous saves, nested nickname leak, and BUG-005 scoring path reviewed. |
| `server/stats.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) | Stats structure/API ownership reconciled with hash destruction. |
| `server/tools.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A candidate dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) | Numeric/allocation/list helpers, re-registration, daemonization, and privilege drop traced; SEC-001/006 and REL-001 recorded. |
| `server/tools.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A candidate dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) | Helper declarations/macros reviewed; IMP-001 prototype cleanup recorded. |
| `server/win32_compat.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A platform boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Winsock/poll/syslog/user-switch boundary reviewed; REL-003 requires Windows runtime validation. |
| `server/ws.c` | Task 3 | Static reviewed; Task 3B pending | [Task 3A length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) | Handshake, SHA-1/Base64 bounds, frame length/mask/partial handling, fd flags, and direct output traced; BUG-006/007 recorded. |
| `server/ws.h` | Task 3 | Static reviewed; Task 3B pending | [Task 3A length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) | WebSocket state/send/decode contract checked against caller retention behavior. |
| `shell.nix` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/audiomixer.cpp` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/audiomixer.h` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame.h` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_board.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_input.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_internal.h` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_level.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_net.cpp` | Task 4 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_render.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_shooter.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/bubblegame_state.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/frozenbubble.cpp` | Tasks 6-8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/frozenbubble.h` | Tasks 6-8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/gamesettings.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/gamesettings.h` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/highscoremanager.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/highscoremanager.h` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/logger.cpp` | Task 8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/logger.h` | Task 8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/main.cpp` | Task 8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu.h` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu_input.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu_internal.h` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu_netpanel.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu_panels.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/mainmenu_server.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/menubutton.cpp` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/menubutton.h` | Task 6 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/netteams.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/netteams.h` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/netview.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/netview.h` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/networkclient.cpp` | Task 4 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/networkclient.h` | Task 4 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/networkclient_wasm.cpp` | Task 4 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/platform.cpp` | Task 8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/platform.h` | Task 8 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/roundstats_color.cpp` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/roundstats_color.h` | Task 5 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/sdl3_compat.h` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/shaderstuff.cpp` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/shaderstuff.h` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/socket_compat.h` | Task 4 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/transitionmanager.cpp` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/transitionmanager.h` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/ttftext.cpp` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `src/ttftext.h` | Task 7 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `start-server.sh` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `tests/net_bots_test.py` | Task 4 | Baseline exercised; semantic review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Passed in Release and retained ASan+UBSan CTest runs; strict-tree Python run also passed. Task 4 still owns test-quality review. |
| `tests/netteams_test.cpp` | Task 5 | Baseline exercised; semantic review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Compiled by analyzers and passed in Release and retained ASan+UBSan CTest runs. Task 5 still owns test-quality review. |
| `tests/netview_test.cpp` | Task 5 | Baseline exercised; semantic review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Compiled by analyzers and passed in Release and retained ASan+UBSan CTest runs. Task 5 still owns test-quality review. |
| `tests/roundstats_color_test.cpp` | Task 5 | Baseline exercised; semantic review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Compiled by analyzers and passed in Release and retained ASan+UBSan CTest runs. Task 5 still owns test-quality review. |
| `tests/server_list_cap_test.py` | Task 3 | Baseline exercised; static reviewed; Task 3B pending | [Task 3A harness review](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | REL-002 confirmed: fixed port, daemonized launcher, foreign-listener false pass, incomplete teardown, and prefix-only receive timing; Task 3B owns read-only foreground/dynamic-port replay. |
| `tools/net_bots.py` | Task 4 | Baseline exercised; semantic review pending | [Task 2 baseline](subsystems/07-build-release-tooling.md) | Imported and exercised by the passing `net-bots-test`; Task 4 still owns harness and protocol fidelity review. |
| `tools/ports/sdl3_image.py` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `tools/ports/sdl3_mixer.py` | Task 9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `tools/server_tests/test_room_caps.py` | Task 3 | Static reviewed; Task 3B pending | [Task 3A harness review](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) | Room-cap cases are semantically useful, but REL-002 also applies: fixed TCP port, daemonized launcher, unrelated UDP 1511 bind, and launcher-only teardown. |
| `web/README.md` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `web/index.html` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
| `web/shell.html` | Tasks 8-9 | Pending review | - | Project-owned or maintained integration surface; evidence pending assigned gate. |
