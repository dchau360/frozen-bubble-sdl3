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
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.lang.reflect.Method;
import java.util.concurrent.TimeUnit;

/**
 * Firebase Cloud Messaging token for the "follow a server" feature.
 *
 * Usage:
 *   PushManager.init(activity);      // from Activity.onCreate()
 *   PushManager.getToken();          // from C++ via JNI, background thread only
 *
 * <h2>Why reflection</h2>
 *
 * Firebase Messaging is an <em>optional</em> dependency: it is only compiled in
 * when {@code android/app/google-services.json} exists (see app/build.gradle).
 * Anyone building this repo without their own Firebase project — which is the
 * default, since that file is git-ignored — would otherwise hit a compile error
 * on the import. Reaching the SDK reflectively lets one source tree serve both
 * cases: with the file the token works, without it every call here returns ""
 * and the app behaves exactly as it did before push existed.
 *
 * <h2>Foreground suppression</h2>
 *
 * Deliberately not implemented here, because FCM already does it. A message
 * carrying a {@code notification} payload — which is what notify-relay sends —
 * is auto-displayed by the system only while the app is <em>not</em> in the
 * foreground. While it is, FCM routes the message to a
 * {@code FirebaseMessagingService} instead, and since this app registers no
 * such service, nothing is shown. That is the behaviour we want, so the
 * correct amount of code for it is none.
 */
public class PushManager {
    private static final String TAG = "FBubble.Push";

    /** Android 13+ requires this at runtime or notifications are silently dropped. */
    private static final String POST_NOTIFICATIONS = "android.permission.POST_NOTIFICATIONS";
    private static final int REQUEST_CODE = 0x5055;   // "PU"

    private static String sCachedToken = null;
    private static boolean sLookupFailed = false;

    /**
     * Call once from Activity.onCreate(). Asks for notification permission on
     * the versions that require it; harmless everywhere else.
     */
    public static void init(final Activity activity) {
        if (Build.VERSION.SDK_INT < 33) return;   // implicitly granted before Android 13
        try {
            if (activity.checkSelfPermission(POST_NOTIFICATIONS)
                    == PackageManager.PERMISSION_GRANTED) {
                return;
            }
            // Asked unconditionally rather than only when the player first
            // follows a server: the follow happens on a menu screen driven by
            // SDL, and interrupting that with a system dialog mid-interaction
            // is worse than asking once at startup. Declining costs nothing --
            // following still works, it just never shows a banner.
            activity.requestPermissions(new String[]{POST_NOTIFICATIONS}, REQUEST_CODE);
        } catch (Exception e) {
            Log.w(TAG, "Could not request notification permission", e);
        }
    }

    /**
     * This device's FCM registration token, or "" when there isn't one.
     *
     * Blocks on the Firebase call, so it must NOT be called from the main/UI
     * thread -- the same rule as FrozenBubbleActivity.fetchUrl(). C++ calls it
     * from the SDL thread, which satisfies that.
     *
     * Returns "" when Firebase is not compiled in, when the lookup fails, or
     * when it times out. Callers treat "" as "cannot register" and send
     * nothing.
     */
    public static String getToken() {
        if (sCachedToken != null) return sCachedToken;
        if (sLookupFailed) return "";

        try {
            Class<?> messagingClass =
                    Class.forName("com.google.firebase.messaging.FirebaseMessaging");
            Object messaging = messagingClass.getMethod("getInstance").invoke(null);
            Object task = messagingClass.getMethod("getToken").invoke(messaging);

            // Tasks/Task come from play-services-base, which is already present
            // via the AdMob dependency, so only the Firebase class above is
            // genuinely conditional.
            Class<?> taskClass = Class.forName("com.google.android.gms.tasks.Task");
            Class<?> tasksClass = Class.forName("com.google.android.gms.tasks.Tasks");
            Method await = tasksClass.getMethod("await", taskClass, long.class, TimeUnit.class);

            Object token = await.invoke(null, task, 10L, TimeUnit.SECONDS);
            if (token instanceof String && !((String) token).isEmpty()) {
                sCachedToken = (String) token;
                Log.d(TAG, "FCM token acquired");
                return sCachedToken;
            }
            Log.w(TAG, "FCM returned an empty token");
        } catch (ClassNotFoundException e) {
            // The expected path for a build with no google-services.json.
            Log.i(TAG, "Firebase Messaging not present; push notifications disabled");
        } catch (Exception e) {
            Log.w(TAG, "Could not obtain an FCM token", e);
        }

        // One failed lookup is enough -- retrying on every call would block the
        // SDL thread for ten seconds each time.
        sLookupFailed = true;
        return "";
    }
}
