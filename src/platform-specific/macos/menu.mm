#if defined(__APPLE__) && defined(__MACH__)

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <SDL3/SDL.h>

#include "util/MenuInterface.h"
#include "platform-specific/menu.h"

/* ========================================================================
   Menu Tracking and Resize Tracking Helpers
   
   When macOS enters a modal tracking loop (menu pulldown or window resize),
   the normal SDL event pump is blocked. These helpers fire an NSTimer at
   60 Hz that calls SDL_AppIterate directly, keeping emulation running.
   ======================================================================== */

static MenuIterateCallback s_iterateCallback = NULL;
static void *s_iterateAppState = NULL;
static NSTimer *s_menuTrackingTimer = nil;
static NSTimer *s_resizeTrackingTimer = nil;

@interface MenuTrackingHelper : NSObject
+ (void)menuDidBeginTracking:(NSNotification *)notification;
+ (void)menuDidEndTracking:(NSNotification *)notification;
+ (void)timerFired:(NSTimer *)timer;
@end

@implementation MenuTrackingHelper
+ (void)menuDidBeginTracking:(NSNotification *)notification {
	if (s_iterateCallback && !s_menuTrackingTimer) {
		s_menuTrackingTimer = [NSTimer timerWithTimeInterval:1.0/59.9226
		                                             target:self
		                                           selector:@selector(timerFired:)
		                                           userInfo:nil
		                                            repeats:YES];
		[[NSRunLoop mainRunLoop] addTimer:s_menuTrackingTimer forMode:NSRunLoopCommonModes];
	}
}

+ (void)menuDidEndTracking:(NSNotification *)notification {
	[s_menuTrackingTimer invalidate];
	s_menuTrackingTimer = nil;
}

+ (void)timerFired:(NSTimer *)timer {
	if (s_iterateCallback) {
		s_iterateCallback(s_iterateAppState);
	}
}
@end

@interface ResizeTrackingHelper : NSObject
+ (void)windowWillStartLiveResize:(NSNotification *)notification;
+ (void)windowDidEndLiveResize:(NSNotification *)notification;
+ (void)timerFired:(NSTimer *)timer;
@end

@implementation ResizeTrackingHelper
+ (void)windowWillStartLiveResize:(NSNotification *)notification {
	if (s_iterateCallback && !s_resizeTrackingTimer) {
		s_resizeTrackingTimer = [NSTimer timerWithTimeInterval:1.0/60.0
		                                               target:self
		                                             selector:@selector(timerFired:)
		                                             userInfo:nil
		                                              repeats:YES];
		[[NSRunLoop mainRunLoop] addTimer:s_resizeTrackingTimer forMode:NSRunLoopCommonModes];
	}
}

+ (void)windowDidEndLiveResize:(NSNotification *)notification {
	[s_resizeTrackingTimer invalidate];
	s_resizeTrackingTimer = nil;
}

+ (void)timerFired:(NSTimer *)timer {
	if (s_iterateCallback) {
		s_iterateCallback(s_iterateAppState);
	}
}
@end

/*
This is where Claude fixed Claude's bad code:
Here's a summary of both changes:
The real bug — menu.mm replacing SDL's window delegate:
SDL3's internal SDL3Window class has a sendEvent: override that, after a window move completes (mouse-up), clears the isMoving flag by calling [delegate windowDidFinishMoving]. But it first checks [delegate isKindOfClass:[SDL3Cocoa_WindowListener class]]. Your MyDelegate wrapping the original delegate fails this isKindOfClass: check, so windowDidFinishMoving is never called after a window drag. The isMoving flag stays YES permanently.
This poisons both mouse grab mechanisms:
Cocoa_SetRelativeMouseMode(true) early-returns without calling CGAssociateMouseAndMouseCursorPosition(NO)
ShouldAdjustCoordinatesForGrab() returns false, so mouseConfinementRect is never set
The fix removes the delegate replacement entirely. The menu items are added to [NSApp mainMenu], which is application-level and doesn't need a window delegate. The fileClose: action now targets a standalone MenuActionHandler object instead. SDL's original delegate stays intact, preserving all of its internal window management state tracking.
The initMenu function signature is unchanged so no callers need updating. The window parameter is still accepted (for API stability) but is no longer used.
--
I don't know what all these words mean exactly. But I confirmed it does seem to be working correctly again.
*/

@interface MenuActionHandler : NSObject <NSMenuItemValidation>
- (void)fileClose:(id)sender;
- (void)appQuit:(id)sender;
- (void)machineReset:(id)sender;
- (void)machineRestart:(id)sender;
- (void)machinePauseResume:(id)sender;
- (void)machineCaptureMouse:(id)sender;
- (void)speed1_0:(id)sender;
- (void)speed2_8:(id)sender;
- (void)speed7_1:(id)sender;
- (void)speed14_3:(id)sender;
- (void)toggleSleepMode:(id)sender;
- (void)controllerMode:(id)sender;
- (void)monitorComposite:(id)sender;
- (void)monitorGSRGB:(id)sender;
- (void)monitorMonoGreen:(id)sender;
- (void)monitorMonoAmber:(id)sender;
- (void)monitorMonoWhite:(id)sender;
- (void)displayFullScreen:(id)sender;
- (void)editCopyScreen:(id)sender;
- (void)editPasteText:(id)sender;
- (void)diskToggleDrive:(id)sender;
@end

@implementation MenuActionHandler
- (void)fileClose:(id)sender {
	if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
		SDL_Event event;
		event.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&event);
	}
	(void)sender;
}

- (void)appQuit:(id)sender {
	[NSApp terminate:nil];
	(void)sender;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
	if (menuItem.action == @selector(appQuit:)) {
		return !getMenuInterface()->isEmulationRunning();
	}
	if (menuItem.action == @selector(machinePauseResume:)) {
		[menuItem setState:getMenuInterface()->isPaused() ? NSControlStateValueOn : NSControlStateValueOff];
	}
	if (menuItem.action == @selector(controllerMode:)) {
		int current = getMenuInterface()->getCurrentControllerMode();
		[menuItem setState:([menuItem tag] == current) ? NSControlStateValueOn : NSControlStateValueOff];
	}
	// All other items (File, Edit, Machine, Settings, Display) require emulation
	return getMenuInterface()->isEmulationRunning();
}

- (void)machineReset:(id)sender       { getMenuInterface()->machineReset(); (void)sender; }
- (void)machineRestart:(id)sender     { getMenuInterface()->machineRestart(); (void)sender; }
- (void)machinePauseResume:(id)sender { getMenuInterface()->machinePauseResume(); (void)sender; }
- (void)machineCaptureMouse:(id)sender{ getMenuInterface()->machineCaptureMouse(); (void)sender; }

- (void)speed1_0:(id)sender  { getMenuInterface()->setSpeed(SPEED_1_0); (void)sender; }
- (void)speed2_8:(id)sender  { getMenuInterface()->setSpeed(SPEED_2_8); (void)sender; }
- (void)speed7_1:(id)sender  { getMenuInterface()->setSpeed(SPEED_7_1); (void)sender; }
- (void)speed14_3:(id)sender { getMenuInterface()->setSpeed(SPEED_14_3); (void)sender; }

- (void)toggleSleepMode:(id)sender { getMenuInterface()->toggleSleepMode(); (void)sender; }

- (void)controllerMode:(id)sender {
	NSMenuItem *item = (NSMenuItem *)sender;
	getMenuInterface()->setControllerMode((int)[item tag]);
}

- (void)monitorComposite:(id)sender { getMenuInterface()->setMonitor(MONITOR_COMPOSITE); (void)sender; }
- (void)monitorGSRGB:(id)sender     { getMenuInterface()->setMonitor(MONITOR_GS_RGB); (void)sender; }
- (void)monitorMonoGreen:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_GREEN); (void)sender; }
- (void)monitorMonoAmber:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_AMBER); (void)sender; }
- (void)monitorMonoWhite:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_WHITE); (void)sender; }
- (void)displayFullScreen:(id)sender { getMenuInterface()->displayFullScreen(); (void)sender; }
- (void)editCopyScreen:(id)sender    { getMenuInterface()->editCopyScreen(); (void)sender; }
- (void)editPasteText:(id)sender     { getMenuInterface()->editPasteText(); (void)sender; }

- (void)diskToggleDrive:(id)sender {
	NSMenuItem *item = (NSMenuItem *)sender;
	NSNumber *keyNumber = (NSNumber *)[item representedObject];
	storage_key_t key((uint64_t)[keyNumber unsignedLongLongValue]);
	getMenuInterface()->diskToggle(key);
}
@end

@interface SpeedMenuDelegate : NSObject <NSMenuDelegate>
@end

@implementation SpeedMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
	int currentMode = getMenuInterface()->getCurrentSpeed();
	for (NSMenuItem *item in [menu itemArray]) {
		[item setState:([item tag] == currentMode) ? NSControlStateValueOn : NSControlStateValueOff];
	}
}
@end

#define SETTINGS_TAG_SLEEP_MODE 1

@interface SettingsMenuDelegate : NSObject <NSMenuDelegate>
@end

@implementation SettingsMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
	for (NSMenuItem *item in [menu itemArray]) {
		if ([item tag] == SETTINGS_TAG_SLEEP_MODE) {
			[item setState:getMenuInterface()->getSleepMode() ? NSControlStateValueOn : NSControlStateValueOff];
		}
	}
}
@end

@interface MonitorMenuDelegate : NSObject <NSMenuDelegate>
@end

@implementation MonitorMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
	int activeTag = getMenuInterface()->getCurrentMonitor();
	if (activeTag < 0) return;
	for (NSMenuItem *item in [menu itemArray]) {
		[item setState:([item tag] == activeTag) ? NSControlStateValueOn : NSControlStateValueOff];
	}
}
@end

@interface ControllerMenuDelegate : NSObject <NSMenuDelegate>
@end

@implementation ControllerMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
	int current = getMenuInterface()->getCurrentControllerMode();
	for (NSMenuItem *item in [menu itemArray]) {
		[item setState:([item tag] == current) ? NSControlStateValueOn : NSControlStateValueOff];
	}
}
@end

@class DrivesMenuDelegate;
@class ControllerMenuDelegate;

static MenuActionHandler *sMenuHandler = nil;
static SpeedMenuDelegate *sSpeedMenuDelegate = nil;
static SettingsMenuDelegate *sSettingsMenuDelegate = nil;
static MonitorMenuDelegate *sMonitorMenuDelegate = nil;
static DrivesMenuDelegate *sDrivesMenuDelegate = nil;
static ControllerMenuDelegate *sControllerMenuDelegate = nil;

@interface DrivesMenuDelegate : NSObject <NSMenuDelegate>
@end

@implementation DrivesMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
	[menu removeAllItems];

	std::vector<MenuDriveInfo> drives = getMenuInterface()->getDriveList();
	if (drives.empty()) {
		NSMenuItem *empty = [[[NSMenuItem alloc]
			initWithTitle:NSLocalizedString(@"(no drives)", nil)
			       action:nil
			keyEquivalent:@""] autorelease];
		[empty setEnabled:NO];
		[menu addItem:empty];
		return;
	}

	for (const MenuDriveInfo &info : drives) {
		std::string label = "S" + std::to_string(info.key.slot)
		                  + "D" + std::to_string(info.key.drive + 1);
		if (info.is_mounted && !info.filename.empty()) {
			// Show only the filename, not the full path
			std::string fname = info.filename;
			size_t slash = fname.rfind('/');
			if (slash != std::string::npos) fname = fname.substr(slash + 1);
			label += ": " + fname;
		} else {
			label += ": (empty)";
		}

		NSMenuItem *item = [[[NSMenuItem alloc]
			initWithTitle:[NSString stringWithUTF8String:label.c_str()]
			       action:@selector(diskToggleDrive:)
			keyEquivalent:@""] autorelease];
		[item setTarget:sMenuHandler];
		[item setRepresentedObject:[NSNumber numberWithUnsignedLongLong:info.key.key]];
		[menu addItem:item];
	}
}
@end

static NSMenu *addMenu(NSString *title) {
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	NSMenu *menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
	[[[NSApp mainMenu] itemArray].lastObject setSubmenu:menu];
	return menu;
}

static void setupMenus(void) {
	sMenuHandler = [[MenuActionHandler alloc] init];
	sDrivesMenuDelegate = [[DrivesMenuDelegate alloc] init];
	sControllerMenuDelegate = [[ControllerMenuDelegate alloc] init];

	// Remove SDL's default menus so we can replace them with our own
	NSMenu *mainMenu = [NSApp mainMenu];
	while ([mainMenu numberOfItems] > 0) {
		[mainMenu removeItemAtIndex:0];
	}

	// App menu (first menu, same name as process)
	NSMenu *appMenu = addMenu([[NSProcessInfo processInfo] processName]);
	[appMenu addItem:[[[NSMenuItem alloc]
		initWithTitle:[NSString stringWithFormat:NSLocalizedString(@"About %@", nil),
						[[NSProcessInfo processInfo] processName]]
		       action:@selector(orderFrontStandardAboutPanel:)
		keyEquivalent:@""] autorelease]];
	[appMenu addItem:[NSMenuItem separatorItem]];
	NSMenuItem *quitItem = [[[NSMenuItem alloc]
		initWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Quit %@", nil),
						[[NSProcessInfo processInfo] processName]]
		       action:@selector(appQuit:)
		keyEquivalent:@""] autorelease];
	[quitItem setTarget:sMenuHandler];
	[appMenu addItem:quitItem];
#if 0
	[appMenu addItem:[NSMenuItem separatorItem]];
#ifdef __MAC_13_0
	[appMenu addItem:[[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Settings...", nil)
		       action:@selector(settings)
		keyEquivalent:@","] autorelease]];
#else
	[appMenu addItem:[[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Preferences", nil)
		       action:@selector(settings:)
		keyEquivalent:@","] autorelease]];
#endif
	[appMenu addItem:[NSMenuItem separatorItem]];
	[appMenu addItem:[[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Show All", nil)
		       action:@selector(unhideAllApplications:)
		keyEquivalent:@""] autorelease]];
	[appMenu addItem:[NSMenuItem separatorItem]];
#endif

	// File menu
	NSMenu *fileMenu = addMenu(NSLocalizedString(@"File", nil));

	// Drives submenu — populated dynamically by DrivesMenuDelegate on each pull-down
	NSMenu *drivesMenu = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Drives", nil)] autorelease];
	[drivesMenu setDelegate:sDrivesMenuDelegate];

	NSMenuItem *drivesMenuItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Drives", nil)
		       action:nil
		keyEquivalent:@""] autorelease];
	[drivesMenuItem setSubmenu:drivesMenu];
	[fileMenu addItem:drivesMenuItem];

	[fileMenu addItem:[NSMenuItem separatorItem]];

	NSMenuItem *closeItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Close Emulation", nil)
		       action:@selector(fileClose:)
		keyEquivalent:@""] autorelease];
	[closeItem setTarget:sMenuHandler];
	[fileMenu addItem:closeItem];

	// Edit menu
	NSMenu *editMenu = addMenu(NSLocalizedString(@"Edit", nil));

	NSMenuItem *copyScreenItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Copy Screen", nil)
		       action:@selector(editCopyScreen:)
		keyEquivalent:@""] autorelease];
	[copyScreenItem setTarget:sMenuHandler];
	[editMenu addItem:copyScreenItem];

	NSMenuItem *pasteTextItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Paste Text", nil)
		       action:@selector(editPasteText:)
		keyEquivalent:@""] autorelease];
	[pasteTextItem setTarget:sMenuHandler];
	[editMenu addItem:pasteTextItem];

	// Machine menu
	NSMenu *machineMenu = addMenu(NSLocalizedString(@"Machine", nil));

	NSMenuItem *resetItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Reset", nil)
		       action:@selector(machineReset:)
		keyEquivalent:@""] autorelease];
	[resetItem setTarget:sMenuHandler];
	[machineMenu addItem:resetItem];

	NSMenuItem *restartItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Restart", nil)
		       action:@selector(machineRestart:)
		keyEquivalent:@""] autorelease];
	[restartItem setTarget:sMenuHandler];
	[machineMenu addItem:restartItem];

	NSMenuItem *pauseItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Pause / Resume", nil)
		       action:@selector(machinePauseResume:)
		keyEquivalent:@""] autorelease];
	[pauseItem setTarget:sMenuHandler];
	[machineMenu addItem:pauseItem];

	[machineMenu addItem:[NSMenuItem separatorItem]];

	NSMenuItem *captureMouseItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Capture Mouse", nil)
		       action:@selector(machineCaptureMouse:)
		keyEquivalent:@""] autorelease];
	[captureMouseItem setTarget:sMenuHandler];
	[machineMenu addItem:captureMouseItem];

	// Settings menu
	sSettingsMenuDelegate = [[SettingsMenuDelegate alloc] init];
	NSMenu *settingsMenu = addMenu(NSLocalizedString(@"Settings", nil));
	[settingsMenu setDelegate:sSettingsMenuDelegate];

	// Speed submenu
	sSpeedMenuDelegate = [[SpeedMenuDelegate alloc] init];
	NSMenu *speedMenu = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Speed", nil)] autorelease];
	[speedMenu setDelegate:sSpeedMenuDelegate];

	struct { NSString *title; SEL action; NSInteger tag; } speedItems[] = {
		{ @"1.0 MHz",  @selector(speed1_0:),  1 },
		{ @"2.8 MHz",  @selector(speed2_8:),  2 },
		{ @"7.1 MHz",  @selector(speed7_1:),  3 },
		{ @"14.3 MHz", @selector(speed14_3:), 4 },
	};
	for (auto &si : speedItems) {
		NSMenuItem *item = [[[NSMenuItem alloc]
			initWithTitle:si.title
			       action:si.action
			keyEquivalent:@""] autorelease];
		[item setTarget:sMenuHandler];
		[item setTag:si.tag];
		[speedMenu addItem:item];
	}

	NSMenuItem *speedMenuItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Speed", nil)
		       action:nil
		keyEquivalent:@""] autorelease];
	[speedMenuItem setSubmenu:speedMenu];
	[settingsMenu addItem:speedMenuItem];

	// Game Controller submenu
	NSMenu *controllerMenu = [[[NSMenu alloc]
		initWithTitle:NSLocalizedString(@"Game Controller", nil)] autorelease];
	[controllerMenu setDelegate:sControllerMenuDelegate];

	struct { NSString *title; NSInteger tag; } controllerItems[] = {
		{ @"Joystick - Gamepad",          0 },  // JOYSTICK_APPLE_GAMEPAD
		{ @"Joystick - Mouse",            1 },  // JOYSTICK_APPLE_MOUSE
		{ @"Sirius / Atari Joyport",      2 },  // JOYSTICK_ATARI_DPAD
	};
	for (auto &ci : controllerItems) {
		NSMenuItem *item = [[[NSMenuItem alloc]
			initWithTitle:ci.title
			       action:@selector(controllerMode:)
			keyEquivalent:@""] autorelease];
		[item setTarget:sMenuHandler];
		[item setTag:ci.tag];
		[controllerMenu addItem:item];
	}

	NSMenuItem *controllerMenuItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Game Controller", nil)
		       action:nil
		keyEquivalent:@""] autorelease];
	[controllerMenuItem setSubmenu:controllerMenu];
	[settingsMenu addItem:controllerMenuItem];

	NSMenuItem *sleepItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Sleep / Busy Wait", nil)
		       action:@selector(toggleSleepMode:)
		keyEquivalent:@""] autorelease];
	[sleepItem setTarget:sMenuHandler];
	[sleepItem setTag:SETTINGS_TAG_SLEEP_MODE];
	[settingsMenu addItem:sleepItem];

	// Display menu
	NSMenu *displayMenu = addMenu(NSLocalizedString(@"Display", nil));

	// Monitor submenu
	sMonitorMenuDelegate = [[MonitorMenuDelegate alloc] init];
	NSMenu *monitorMenu = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Monitor", nil)] autorelease];
	[monitorMenu setDelegate:sMonitorMenuDelegate];

	struct { NSString *title; SEL action; NSInteger tag; } monitorItems[] = {
		{ @"Composite",          @selector(monitorComposite:),  MONITOR_COMPOSITE },
		{ @"GS RGB",             @selector(monitorGSRGB:),      MONITOR_GS_RGB },
		{ @"Monochrome - Green", @selector(monitorMonoGreen:),  MONITOR_MONO_GREEN },
		{ @"Monochrome - Amber", @selector(monitorMonoAmber:),  MONITOR_MONO_AMBER },
		{ @"Monochrome - White", @selector(monitorMonoWhite:),  MONITOR_MONO_WHITE },
	};
	for (auto &mi : monitorItems) {
		NSMenuItem *item = [[[NSMenuItem alloc]
			initWithTitle:mi.title
			       action:mi.action
			keyEquivalent:@""] autorelease];
		[item setTarget:sMenuHandler];
		[item setTag:mi.tag];
		[monitorMenu addItem:item];
	}

	NSMenuItem *monitorMenuItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Monitor", nil)
		       action:nil
		keyEquivalent:@""] autorelease];
	[monitorMenuItem setSubmenu:monitorMenu];
	[displayMenu addItem:monitorMenuItem];

	NSMenuItem *fullScreenItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Full Screen", nil)
		       action:@selector(displayFullScreen:)
		keyEquivalent:@""] autorelease];
	[fullScreenItem setTarget:sMenuHandler];
	[displayMenu addItem:fullScreenItem];
}

void initMenu(SDL_Window *window) {
	(void)window;
	setupMenus();
}

void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate) {
	s_iterateCallback = callback;
	s_iterateAppState = appstate;

	// Menu tracking: fires SDL_AppIterate during menu pulldown
	[[NSNotificationCenter defaultCenter] addObserver:[MenuTrackingHelper class]
	                                         selector:@selector(menuDidBeginTracking:)
	                                             name:NSMenuDidBeginTrackingNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:[MenuTrackingHelper class]
	                                         selector:@selector(menuDidEndTracking:)
	                                             name:NSMenuDidEndTrackingNotification
	                                           object:nil];

	// Resize tracking: fires SDL_AppIterate during window resize drag
	[[NSNotificationCenter defaultCenter] addObserver:[ResizeTrackingHelper class]
	                                         selector:@selector(windowWillStartLiveResize:)
	                                             name:NSWindowWillStartLiveResizeNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:[ResizeTrackingHelper class]
	                                         selector:@selector(windowDidEndLiveResize:)
	                                             name:NSWindowDidEndLiveResizeNotification
	                                           object:nil];
}
#endif
