/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Free-standing NSSavePanel (title + message visible; SDL's cocoa save
 *   dialog is an untitled sheet).
 *
 *   Existing files always appear gray in NSSavePanel on modern macOS — Apple's
 *   NSOpenSavePanelDelegate docs state panel:shouldEnableURL: is not sent for
 *   save panels ("All urls are always disabled"). That is cosmetic: clicking a
 *   dimmed file still fills Save As for overwrite (same as TextEdit Save As).
 */

#if defined(__APPLE__) && defined(__MACH__)

#include "gs2_save_dialog.hpp"

#import <Cocoa/Cocoa.h>

#include <SDL3/SDL.h>

namespace {

void invokeCallback(SDL_DialogFileCallback callback, void *userdata, const char *path) {
    if (!callback) {
        return;
    }
    if (path) {
        const char *files[2] = { path, nullptr };
        callback(userdata, files, -1);
    } else {
        const char *files[1] = { nullptr };
        callback(userdata, files, -1);
    }
}

} // namespace

void gs2_show_save_file_dialog(SDL_DialogFileCallback callback, void *userdata,
                               SDL_Window *window, const char *default_location,
                               const char *title, const char *message,
                               const char *fallback_name) {
    if (!callback) {
        return;
    }
    (void)window;

    NSSavePanel *panel = [NSSavePanel savePanel];
    if (title && title[0]) {
        [panel setTitle:[NSString stringWithUTF8String:title]];
    }
    if (message && message[0]) {
        [panel setMessage:[NSString stringWithUTF8String:message]];
    }
    [panel setPrompt:@"Save"];
    [panel setNameFieldLabel:@"Save As:"];
    [panel setCanCreateDirectories:YES];
    [panel setExtensionHidden:NO];

    if (default_location && default_location[0]) {
        NSString *path = [NSString stringWithUTF8String:default_location];
        NSString *dir = [path stringByDeletingLastPathComponent];
        NSString *name = [path lastPathComponent];
        if (dir.length > 0) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:dir isDirectory:YES]];
        }
        if (name.length > 0) {
            [panel setNameFieldStringValue:name];
        } else if (fallback_name && fallback_name[0]) {
            [panel setNameFieldStringValue:[NSString stringWithUTF8String:fallback_name]];
        }
    } else if (fallback_name && fallback_name[0]) {
        [panel setNameFieldStringValue:[NSString stringWithUTF8String:fallback_name]];
    }

    const NSInteger result = [panel runModal];
    if (result == NSModalResponseOK) {
        invokeCallback(callback, userdata, [[[panel URL] path] UTF8String]);
    } else {
        invokeCallback(callback, userdata, nullptr);
    }
}

void gs2_show_save_gs2_dialog(SDL_DialogFileCallback callback, void *userdata,
                              SDL_Window *window, const char *default_location) {
    gs2_show_save_file_dialog(callback, userdata, window, default_location,
                              "Save System Configuration",
                              "Choose a .gs2 file to overwrite, or enter a new name.",
                              "system.gs2");
}

#endif // __APPLE__
