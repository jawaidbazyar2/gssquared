#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <SDL3/SDL.h>

@interface MyDelegate : NSObject <NSWindowDelegate>
{
	id<NSWindowDelegate> _originalDelegate;
}
- (instancetype)initWithOriginalDelegate:(id<NSWindowDelegate>)originalDelegate;
- (void)fileNew:(id)sender;
- (void)fileOpen:(id)sender;
- (void)fileClose:(id)sender;
- (bool)windowShouldClose:(NSWindow *)sender;
@end

@implementation MyDelegate
- (void)fileNew:(id)sender {
	NSLog(@"MainMenu/File/New");
	(void)sender;
}

- (void)fileOpen:(id)sender {
	NSLog(@"MainMenu/File/Open");
	(void)sender;
}

- (void)fileClose:(id)sender {
	if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
		SDL_Event event;
		event.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&event);
	}

	(void)sender;
}

- (instancetype)initWithOriginalDelegate:(id<NSWindowDelegate>)originalDelegate {
	self = [super init];
	if (self) {
		_originalDelegate = originalDelegate;
	}
	// First menu, same name
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	[[[NSApp mainMenu] itemArray][0]
	    setSubmenu:[[[NSMenu alloc] initWithTitle:[[NSProcessInfo processInfo] processName]]
			   autorelease]];
	[[[[NSApp mainMenu] itemArray][0] submenu]
	    addItem:[[[NSMenuItem alloc]
			initWithTitle:[NSString
					  stringWithFormat:NSLocalizedString(@"About %@", nil),
							   [[NSProcessInfo processInfo]
							       processName]]
			       action:@selector(orderFrontStandardAboutPanel:)
			keyEquivalent:@""] autorelease]];
#if 0
	[[[[NSApp mainMenu] itemArray][0] submenu] addItem:[NSMenuItem separatorItem]];
#ifdef __MAC_13_0
	[[[[NSApp mainMenu] itemArray][0] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Settings...", nil)
						action:@selector(settings)
					 keyEquivalent:@","] autorelease]];
#else
	[[[[NSApp mainMenu] itemArray][0] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Preferences", nil)
						action:@selector(settings:)
					 keyEquivalent:@","] autorelease]];
#endif
	[[[[NSApp mainMenu] itemArray][0] submenu] addItem:[NSMenuItem separatorItem]];
	[[[[NSApp mainMenu] itemArray][0] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Show All", nil)
						action:@selector(unhideAllApplications:)
					 keyEquivalent:@""] autorelease]];
	[[[[NSApp mainMenu] itemArray][0] submenu] addItem:[NSMenuItem separatorItem]];
#endif

	// File menu
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	[[[NSApp mainMenu] itemArray][1]
	    setSubmenu:[[[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)]
			   autorelease]];

/* 	[[[[NSApp mainMenu] itemArray][1] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"New", nil)
						action:@selector(fileNew:)
					 keyEquivalent:@"n"] autorelease]];

	[[[[NSApp mainMenu] itemArray][1] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open", nil)
						action:@selector(fileOpen:)
					 keyEquivalent:@"o"] autorelease]];

	[[[[NSApp mainMenu] itemArray][1] submenu] addItem:[NSMenuItem separatorItem]];
 */
	[[[[NSApp mainMenu] itemArray][1] submenu]
	    addItem:[[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Close", nil)
						action:@selector(fileClose:)
					 keyEquivalent:[NSString stringWithFormat:@"%C", (unichar)NSF12FunctionKey]] autorelease]];

	return self;
}

- (bool)windowShouldClose:(NSWindow *)sender {
	if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
		SDL_Event event;
		event.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&event);
	}

	return false;
}

// Forward all unhandled delegate methods to SDL's original delegate
- (BOOL)respondsToSelector:(SEL)aSelector {
	return [super respondsToSelector:aSelector] || [_originalDelegate respondsToSelector:aSelector];
}

- (id)forwardingTargetForSelector:(SEL)aSelector {
	if ([_originalDelegate respondsToSelector:aSelector]) {
		return _originalDelegate;
	}
	return [super forwardingTargetForSelector:aSelector];
}
@end

// sdl sets its own delegate; we need to save it and forward all unhandled
// delegate messages to it.
// seems like we could handle the menu shortcuts based on whether we're "keyboard captured" or not..?

void initMenu(SDL_Window *window) {
	NSWindow *nswindow = (__bridge NSWindow *)SDL_GetPointerProperty(
	    SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
	if (nswindow == NULL) {
		return;
	}
	// Save SDL's original delegate so we can forward calls to it
	id<NSWindowDelegate> originalDelegate = [nswindow delegate];
	[nswindow setDelegate:[[MyDelegate alloc] initWithOriginalDelegate:originalDelegate]];
}
