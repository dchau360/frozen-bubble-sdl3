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

// iOS replacement for the desktop `popen("curl ...")` used to read the public
// server list and the geolocation endpoint. iOS forbids fork/exec inside the app
// sandbox and ships no curl binary, so popen there returns nothing at all — the
// server browser would silently show an empty list. Android already solves this
// with a JNI call to FrozenBubbleActivity.fetchUrl(); this is the same shape
// backed by NSURLSession.
//
// Deliberately synchronous, matching the popen and JNI paths: both existing
// callers run on a worker thread that expects to block until the body arrives.

#import <Foundation/Foundation.h>
#include <string>

std::string IosFetchUrl(const char* url, int timeoutSeconds) {
    if (!url || !*url) return "";

    @autoreleasepool {
        NSURL* nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (!nsurl) return "";

        NSMutableURLRequest* request =
            [NSMutableURLRequest requestWithURL:nsurl
                                    cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                timeoutInterval:(NSTimeInterval)timeoutSeconds];
        [request setHTTPMethod:@"GET"];

        // NSURLSession is callback-based; the semaphore turns it back into the
        // blocking call both callers already expect. The completion handler runs
        // on the session's own delegate queue, never on this thread, so waiting
        // here cannot deadlock against it.
        //
        // The bytes are copied into the std::string inside the handler rather than
        // holding onto the NSData: the handler's NSData is autoreleased into the
        // delegate queue's pool, which can drain the moment the handler returns,
        // so reading it after the wait would be a use-after-free under manual
        // retain/release. Copying keeps this correct whether or not ARC is on.
        __block std::string body;
        __block NSInteger status = 0;
        dispatch_semaphore_t done = dispatch_semaphore_create(0);

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
              completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
                  if (!error && [response isKindOfClass:[NSHTTPURLResponse class]]) {
                      status = [(NSHTTPURLResponse*)response statusCode];
                      if (data.length > 0) {
                          body.assign((const char*)data.bytes, (size_t)data.length);
                      }
                  }
                  dispatch_semaphore_signal(done);
              }];
        [task resume];

        // Bounded wait rather than DISPATCH_TIME_FOREVER: timeoutInterval covers a
        // stalled transfer, but not every way a request can fail to call back.
        if (dispatch_semaphore_wait(
                done,
                dispatch_time(DISPATCH_TIME_NOW,
                              (int64_t)(timeoutSeconds + 2) * NSEC_PER_SEC)) != 0) {
            [task cancel];
            return "";
        }

        if (status < 200 || status > 299) return "";
        return body;
    }
}

// Fire-and-forget POST for sendGameStats.cpp. Deliberately NOT the
// semaphore-blocking shape IosFetchUrl uses above: nothing reads the
// response, so there is nothing worth blocking the calling thread for --
// [task resume] hands the request to NSURLSession's own background queue and
// this returns immediately, same as the WASM path's fetch() and same effect
// as the desktop path's detached std::thread.
void IosPostJson(const std::string& url, const std::string& jsonBody) {
    if (url.empty()) return;

    @autoreleasepool {
        NSURL* nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        if (!nsurl) return;

        NSMutableURLRequest* request =
            [NSMutableURLRequest requestWithURL:nsurl
                                    cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                timeoutInterval:3.0];
        [request setHTTPMethod:@"POST"];
        [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
        [request setHTTPBody:[NSData dataWithBytes:jsonBody.data() length:jsonBody.size()]];

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
              completionHandler:^(NSData*, NSURLResponse*, NSError*) {
                  // Best-effort telemetry: nothing observes whether it landed.
              }];
        [task resume];
    }
}
