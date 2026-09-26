/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "platform-specific/HttpsGet.hpp"
#include "platform-specific/HttpsBody.hpp"

#import <Foundation/Foundation.h>

#include <atomic>
#include <string>

namespace {

std::string trim_token(const std::string& raw) {
    size_t begin = 0;
    size_t end = raw.size();
    while (begin < end && (raw[begin] == ' ' || raw[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (raw[end - 1] == ' ' || raw[end - 1] == '\t' || raw[end - 1] == '\r'
                           || raw[end - 1] == '\n')) {
        --end;
    }
    std::string token = raw.substr(begin, end - begin);
    if (token.find('\n') != std::string::npos || token.find('\r') != std::string::npos) {
        return {};
    }
    return token;
}

bool https_scheme(NSString *scheme) {
    return scheme != nil && [scheme.lowercaseString isEqualToString:@"https"];
}

}  // namespace

@interface Gs2HttpsDelegate : NSObject <NSURLSessionDataDelegate>
@property (nonatomic, assign) HttpsBodyWriter *writer;
@property (nonatomic, assign) const std::atomic<bool> *cancelFlag;
@property (nonatomic, assign) uint64_t maxBytes;
@property (nonatomic, copy) NSString *destPath;
@property (nonatomic, assign) int statusCode;
@property (nonatomic, assign) bool opened;
@property (nonatomic, assign) bool failed;
@property (nonatomic, assign) std::string *errorText;
@property (nonatomic, assign) std::string *saveToken;
@property (nonatomic, assign) dispatch_semaphore_t done;
@end

@implementation Gs2HttpsDelegate

- (void)URLSession:(NSURLSession *)session
                  task:(NSURLSessionTask *)task
    willPerformHTTPRedirection:(NSHTTPURLResponse *)response
                    newRequest:(NSURLRequest *)request
             completionHandler:(void (^)(NSURLRequest *))completionHandler {
    (void)session;
    (void)task;
    (void)response;
    if (!https_scheme(request.URL.scheme)) {
        if (self.errorText != nullptr && self.errorText->empty()) {
            *self.errorText = "Redirect left https";
        }
        self.failed = true;
        completionHandler(nil);
        return;
    }
    completionHandler(request);
}

- (void)URLSession:(NSURLSession *)session
              dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveResponse:(NSURLResponse *)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
    (void)session;
    (void)dataTask;
    NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
    self.statusCode = static_cast<int>(http.statusCode);
    if (http.statusCode != 200) {
        if (self.errorText != nullptr) {
            *self.errorText = "Server returned status " + std::to_string(http.statusCode);
        }
        self.failed = true;
        completionHandler(NSURLSessionResponseCancel);
        return;
    }
    if (http.expectedContentLength > static_cast<long long>(self.maxBytes)) {
        if (self.errorText != nullptr) {
            *self.errorText = "Pack exceeds 200 MB";
        }
        self.failed = true;
        completionHandler(NSURLSessionResponseCancel);
        return;
    }
    NSString *token = [http valueForHTTPHeaderField:@"X-GS2-Save-Token"];
    if (token != nil && self.saveToken != nullptr) {
        *self.saveToken = trim_token(std::string(token.UTF8String));
    }
    std::string error;
    const char *dest = self.destPath != nil ? self.destPath.UTF8String : "";
    if (self.writer == nullptr || !self.writer->open(dest, self.maxBytes, error)) {
        if (self.errorText != nullptr) {
            *self.errorText = error.empty() ? "Failed to write pack download" : error;
        }
        self.failed = true;
        completionHandler(NSURLSessionResponseCancel);
        return;
    }
    self.opened = true;
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {
    (void)session;
    if (self.failed) {
        return;
    }
    if (self.cancelFlag != nullptr && self.cancelFlag->load()) {
        if (self.errorText != nullptr && self.errorText->empty()) {
            *self.errorText = "Download canceled";
        }
        self.failed = true;
        [dataTask cancel];
        return;
    }
    std::string error;
    if (self.writer == nullptr
        || !self.writer->write(data.bytes, static_cast<size_t>(data.length), self.cancelFlag, error)) {
        if (self.errorText != nullptr && self.errorText->empty()) {
            *self.errorText = error.empty() ? "Failed to write pack download" : error;
        }
        self.failed = true;
        [dataTask cancel];
    }
}

- (void)URLSession:(NSURLSession *)session
                    task:(NSURLSessionTask *)task
    didCompleteWithError:(NSError *)error {
    (void)session;
    (void)task;
    if (error != nil && !self.failed && self.errorText != nullptr && self.errorText->empty()) {
        NSString *message = error.localizedDescription;
        *self.errorText = message != nil ? std::string(message.UTF8String) : "Download failed";
        self.failed = true;
    }
    if (!self.failed && self.statusCode == 200 && self.opened) {
        std::string commit_error;
        if (self.writer == nullptr || !self.writer->commit(commit_error)) {
            if (self.errorText != nullptr) {
                *self.errorText = commit_error.empty() ? "Failed to store pack download" : commit_error;
            }
            self.failed = true;
        }
    } else if (self.opened && self.writer != nullptr) {
        self.writer->abort();
        self.opened = false;
    }
    if (self.failed && self.errorText != nullptr && self.errorText->empty()) {
        *self.errorText = "Download failed";
    }
    if (self.done != nullptr) {
        dispatch_semaphore_signal(self.done);
    }
}

@end

bool https_get_to_file(const std::string& url, const std::string& dest_path, uint64_t max_bytes,
                       const std::atomic<bool> *cancel, HttpsGetResult& out) {
    out = HttpsGetResult{};
    @autoreleasepool {
        NSString *url_text = [NSString stringWithUTF8String:url.c_str()];
        NSURL *nsurl = url_text != nil ? [NSURL URLWithString:url_text] : nil;
        if (!https_scheme(nsurl.scheme) || nsurl.host.length == 0) {
            out.error = "Pack URL must be https";
            return false;
        }

        HttpsBodyWriter writer;
        std::string error;
        std::string save_token;
        Gs2HttpsDelegate *delegate = [[Gs2HttpsDelegate alloc] init];
        delegate.writer = &writer;
        delegate.cancelFlag = cancel;
        delegate.maxBytes = max_bytes;
        delegate.destPath = [NSString stringWithUTF8String:dest_path.c_str()];
        delegate.errorText = &error;
        delegate.saveToken = &save_token;
        delegate.done = dispatch_semaphore_create(0);

        NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.timeoutIntervalForRequest = 60;
        config.timeoutIntervalForResource = 3600;
        config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        NSURLSession *session = [NSURLSession sessionWithConfiguration:config
                                                              delegate:delegate
                                                         delegateQueue:nil];
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:nsurl];
        [request setValue:@"GSSquared" forHTTPHeaderField:@"User-Agent"];
        NSURLSessionDataTask *task = [session dataTaskWithRequest:request];
        [task retain];
        [task resume];

        for (;;) {
            const long wait = dispatch_semaphore_wait(
                delegate.done, dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC));
            if (wait == 0) {
                break;
            }
            if (cancel != nullptr && cancel->load()) {
                [task cancel];
            }
        }

        out.status = delegate.statusCode;
        out.save_token = save_token;
        out.error = error;
        out.ok = !delegate.failed && delegate.statusCode == 200 && error.empty();
        if (!out.ok && out.error.empty()) {
            out.error = "Download failed";
        }

        [task release];
        [session finishTasksAndInvalidate];
        dispatch_release(delegate.done);
        delegate.done = nullptr;
        [delegate release];
        return out.ok;
    }
}
