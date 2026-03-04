#if defined(__APPLE__) && defined(__MACH__)

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <SDL3/SDL.h>

#include "util/MenuInterface.h"

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

@interface MenuActionHandler : NSObject
- (void)fileClose:(id)sender;
- (void)machineReset:(id)sender;
- (void)machineRestart:(id)sender;
- (void)machinePauseResume:(id)sender;
- (void)machineCaptureMouse:(id)sender;
- (void)speed1_0:(id)sender;
- (void)speed2_8:(id)sender;
- (void)speed7_1:(id)sender;
- (void)speed14_3:(id)sender;
- (void)toggleSleepMode:(id)sender;
- (void)monitorComposite:(id)sender;
- (void)monitorGSRGB:(id)sender;
- (void)monitorMonoGreen:(id)sender;
- (void)monitorMonoAmber:(id)sender;
- (void)monitorMonoWhite:(id)sender;
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

- (void)machineReset:(id)sender       { getMenuInterface()->machineReset(); (void)sender; }
- (void)machineRestart:(id)sender     { getMenuInterface()->machineRestart(); (void)sender; }
- (void)machinePauseResume:(id)sender { getMenuInterface()->machinePauseResume(); (void)sender; }
- (void)machineCaptureMouse:(id)sender{ getMenuInterface()->machineCaptureMouse(); (void)sender; }

- (void)speed1_0:(id)sender  { getMenuInterface()->setSpeed(SPEED_1_0); (void)sender; }
- (void)speed2_8:(id)sender  { getMenuInterface()->setSpeed(SPEED_2_8); (void)sender; }
- (void)speed7_1:(id)sender  { getMenuInterface()->setSpeed(SPEED_7_1); (void)sender; }
- (void)speed14_3:(id)sender { getMenuInterface()->setSpeed(SPEED_14_3); (void)sender; }

- (void)toggleSleepMode:(id)sender { getMenuInterface()->toggleSleepMode(); (void)sender; }

- (void)monitorComposite:(id)sender { getMenuInterface()->setMonitor(MONITOR_COMPOSITE); (void)sender; }
- (void)monitorGSRGB:(id)sender     { getMenuInterface()->setMonitor(MONITOR_GS_RGB); (void)sender; }
- (void)monitorMonoGreen:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_GREEN); (void)sender; }
- (void)monitorMonoAmber:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_AMBER); (void)sender; }
- (void)monitorMonoWhite:(id)sender  { getMenuInterface()->setMonitor(MONITOR_MONO_WHITE); (void)sender; }
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

static MenuActionHandler *sMenuHandler = nil;
static SpeedMenuDelegate *sSpeedMenuDelegate = nil;
static SettingsMenuDelegate *sSettingsMenuDelegate = nil;
static MonitorMenuDelegate *sMonitorMenuDelegate = nil;

static void setupMenus(void) {
	sMenuHandler = [[MenuActionHandler alloc] init];

	// App menu (first menu, same name as process)
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

	NSMenuItem *closeItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Close", nil)
		       action:@selector(fileClose:)
		keyEquivalent:[NSString stringWithFormat:@"%C", (unichar)NSF12FunctionKey]] autorelease];
	[closeItem setTarget:sMenuHandler];
	[[[[NSApp mainMenu] itemArray][1] submenu] addItem:closeItem];

	// Machine menu
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	[[[NSApp mainMenu] itemArray][2]
	    setSubmenu:[[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Machine", nil)]
			   autorelease]];

	NSMenuItem *resetItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Reset", nil)
		       action:@selector(machineReset:)
		keyEquivalent:@""] autorelease];
	[resetItem setTarget:sMenuHandler];
	[[[[NSApp mainMenu] itemArray][2] submenu] addItem:resetItem];

	NSMenuItem *restartItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Restart", nil)
		       action:@selector(machineRestart:)
		keyEquivalent:@""] autorelease];
	[restartItem setTarget:sMenuHandler];
	[[[[NSApp mainMenu] itemArray][2] submenu] addItem:restartItem];

	NSMenuItem *pauseItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Pause / Resume", nil)
		       action:@selector(machinePauseResume:)
		keyEquivalent:@""] autorelease];
	[pauseItem setTarget:sMenuHandler];
	[[[[NSApp mainMenu] itemArray][2] submenu] addItem:pauseItem];

	[[[[NSApp mainMenu] itemArray][2] submenu] addItem:[NSMenuItem separatorItem]];

	NSMenuItem *captureMouseItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Capture Mouse", nil)
		       action:@selector(machineCaptureMouse:)
		keyEquivalent:@""] autorelease];
	[captureMouseItem setTarget:sMenuHandler];
	[[[[NSApp mainMenu] itemArray][2] submenu] addItem:captureMouseItem];

	// Settings menu
	sSettingsMenuDelegate = [[SettingsMenuDelegate alloc] init];
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	NSMenu *settingsMenu = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Settings", nil)] autorelease];
	[settingsMenu setDelegate:sSettingsMenuDelegate];
	[[[NSApp mainMenu] itemArray][3] setSubmenu:settingsMenu];

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
	[[[[NSApp mainMenu] itemArray][3] submenu] addItem:speedMenuItem];

	NSMenuItem *sleepItem = [[[NSMenuItem alloc]
		initWithTitle:NSLocalizedString(@"Sleep / Busy Wait", nil)
		       action:@selector(toggleSleepMode:)
		keyEquivalent:@""] autorelease];
	[sleepItem setTarget:sMenuHandler];
	[sleepItem setTag:SETTINGS_TAG_SLEEP_MODE];
	[[[[NSApp mainMenu] itemArray][3] submenu] addItem:sleepItem];

	// Display menu
	[[NSApp mainMenu] addItem:[[[NSMenuItem alloc] init] autorelease]];
	[[[NSApp mainMenu] itemArray][4]
	    setSubmenu:[[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Display", nil)]
			   autorelease]];

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
	[[[[NSApp mainMenu] itemArray][4] submenu] addItem:monitorMenuItem];
}

void initMenu(SDL_Window *window) {
	(void)window;
	setupMenus();
}
#endif
