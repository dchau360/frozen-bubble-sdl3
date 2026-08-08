/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// APNs registration for the "follow a server" feature.
//
// Three things have to line up before a token exists, and all three can
// legitimately fail on a normal build:
//
//   1. The app must be signed with the aps-environment entitlement. The
//      unsigned .ipa this repo produces by default is not, and neither is a
//      free-Apple-ID sideload -- free provisioning profiles do not grant it.
//      APNs then fails registration outright.
//   2. The player must allow notifications at the system prompt.
//   3. APNs must actually answer, which is asynchronous and can take a moment.
//
// So "no token" is an ordinary state, not an error path, and every caller
// treats "" as "cannot register" rather than as something to report. Nothing
// here ever blocks the game: registration is fired once and the answer is
// picked up whenever it lands.
//
// Deliberately does NOT swap out SDL's UIApplicationDelegate. SDL owns that
// delegate and the app would stop launching if it were replaced; instead the
// two APNs callbacks are grafted onto SDL's existing delegate class at runtime
// (see InstallDelegateHooks below), which is the least invasive way to observe
// them.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <objc/runtime.h>

#include <string>
#include <mutex>

namespace {

std::mutex g_tokenMutex;
std::string g_deviceToken;
bool g_registrationStarted = false;

void StoreToken(NSData *deviceToken) {
    // APNs hands back raw bytes; the wire format everyone expects is lowercase
    // hex, which is also what the relay passes straight to Apple.
    const unsigned char *bytes = (const unsigned char *)deviceToken.bytes;
    std::string hex;
    hex.reserve(deviceToken.length * 2);
    static const char kHex[] = "0123456789abcdef";
    for (NSUInteger i = 0; i < deviceToken.length; i++) {
        hex.push_back(kHex[(bytes[i] >> 4) & 0xF]);
        hex.push_back(kHex[bytes[i] & 0xF]);
    }

    std::lock_guard<std::mutex> lock(g_tokenMutex);
    g_deviceToken = hex;
    NSLog(@"[FrozenBubble] APNs device token acquired (%zu chars)", hex.size());
}

// --- Delegate hooks -------------------------------------------------------
//
// Added to whatever class SDL is using as the application delegate, rather
// than replacing it.

void OnDidRegister(id self, SEL _cmd, UIApplication *app, NSData *deviceToken) {
    (void)self; (void)_cmd; (void)app;
    StoreToken(deviceToken);
}

void OnDidFailToRegister(id self, SEL _cmd, UIApplication *app, NSError *error) {
    (void)self; (void)_cmd; (void)app;
    // Expected on an unsigned build. Logged rather than surfaced: there is
    // nothing the player can do about it, and following a server still works,
    // it just never notifies.
    NSLog(@"[FrozenBubble] APNs registration failed: %@", error.localizedDescription);
}

void InstallDelegateHooks() {
    id delegate = [UIApplication sharedApplication].delegate;
    if (delegate == nil) return;

    Class delegateClass = object_getClass(delegate);

    class_addMethod(delegateClass,
                    @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:),
                    (IMP)OnDidRegister, "v@:@@");
    class_addMethod(delegateClass,
                    @selector(application:didFailToRegisterForRemoteNotificationsWithError:),
                    (IMP)OnDidFailToRegister, "v@:@@");
}

}  // namespace

// Suppresses the banner while the app is in the foreground. Without this iOS
// would show one over the lobby, which already lists who is online -- the
// notification exists for when you are *not* looking at the game.
@interface FBPushForegroundDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation FBPushForegroundDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler {
    (void)center; (void)notification;
    completionHandler(UNNotificationPresentationOptionNone);
}
@end

static FBPushForegroundDelegate *g_foregroundDelegate = nil;

void IosRegisterForPush() {
    {
        std::lock_guard<std::mutex> lock(g_tokenMutex);
        if (g_registrationStarted) return;
        g_registrationStarted = true;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            InstallDelegateHooks();

            UNUserNotificationCenter *center =
                [UNUserNotificationCenter currentNotificationCenter];

            if (g_foregroundDelegate == nil) {
                g_foregroundDelegate = [[FBPushForegroundDelegate alloc] init];
            }
            center.delegate = g_foregroundDelegate;

            UNAuthorizationOptions options =
                UNAuthorizationOptionAlert | UNAuthorizationOptionSound;
            [center requestAuthorizationWithOptions:options
                                  completionHandler:^(BOOL granted, NSError *error) {
                if (error != nil) {
                    NSLog(@"[FrozenBubble] Notification authorization error: %@",
                          error.localizedDescription);
                }
                if (!granted) {
                    // A refusal is a normal answer. Following a server still
                    // works; the server just never reaches this device.
                    NSLog(@"[FrozenBubble] Notification permission not granted");
                    return;
                }
                // registerForRemoteNotifications must run on the main thread.
                dispatch_async(dispatch_get_main_queue(), ^{
                    [[UIApplication sharedApplication] registerForRemoteNotifications];
                });
            }];
        }
    });
}

std::string IosPushDeviceToken() {
    std::lock_guard<std::mutex> lock(g_tokenMutex);
    return g_deviceToken;
}
