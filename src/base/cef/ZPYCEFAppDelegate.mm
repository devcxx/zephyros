//
//  ZephyrosAppDelegate.m
//  Zephyros
//
//  Created by Matthias Christen on 08.07.15.
//  Copyright (c) 2015 Vanamco AG. All rights reserved.
//

#import <objc/runtime.h>

#import <JavaScriptCore/JavaScriptCore.h>
#import <SystemConfiguration/SystemConfiguration.h>

//#include "include/cef_app.h"
#include "lib/cef/include/cef_application_mac.h"
#include "lib/cef/include/cef_browser.h"
#include "lib/cef/include/cef_frame.h"
//#include "include/cef_runnable.h"

#import "base/zephyros_impl.h"
#import "base/types.h"

#import "base/cef/client_handler.h"
#import "base/cef/ZPYCEFAppDelegate.h"
#import "base/cef/ZPYWindowDelegate.h"

#import "components/ZPYMenuItem.h"

#import "native_extensions/custom_url_manager.h"
#import "native_extensions/path.h"

#import "native_extensions/native_extensions.h"


// default content area size for newly created windows
const int kDefaultWindowWidth = 800;
const int kDefaultWindowHeight = 600;

extern CefRefPtr<Zephyros::ClientHandler> g_handler;
extern bool g_isWindowBeingLoaded;


@implementation ZPYCEFAppDelegate

- (id) init
{
    self = [super init];
    
    if (_tcslen(Zephyros::GetUpdaterURL()) > 0)
    {
        self.updater = [[SUUpdater alloc] init];
        self.updater.feedURL = [NSURL URLWithString: [NSString stringWithUTF8String: Zephyros::GetUpdaterURL()]];
    }
    
    // TODO: check if this works (not too early)
    // register ourselves as delegate for the notification center
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate: self];
    
    return self;
}

- (void) createApp: (id) object
{
    NSApplication* app = [NSApplication sharedApplication];
    NSArray* tl;
    [[NSBundle mainBundle] loadNibNamed: @"MainMenu" owner: app topLevelObjects: &tl];

    // set the delegate for application events
    [app setDelegate: self];
    
    [self createMenuItems];
    [self applicationWillFinishLaunching: nil];

    Zephyros::AbstractLicenseManager* pMgr = Zephyros::GetLicenseManager();
    if (pMgr == NULL || pMgr->CanStartApp())
        [self createMainWindow];
}

- (void) createMainWindow
{
    // create the handler
    if (g_handler != NULL)
    {
        g_handler->ReleaseCefObjects();
        g_handler = NULL;
    }
    g_handler = new Zephyros::ClientHandler();
    
    // Create the delegate for control and browser window events.
    ZPYWindowDelegate* delegate = [[ZPYWindowDelegate alloc] init];
    
    // Create the main application window.
    NSRect rectScreen = [[NSScreen mainScreen] visibleFrame];
    NSRect rectWindow;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSData *dataDefaults = [defaults objectForKey: @"wnd2"];
    if (dataDefaults != nil)
    {
        NSDictionary *dictWndRect = [NSKeyedUnarchiver unarchiveObjectWithData: dataDefaults];
        rectWindow = NSMakeRect(
            [(NSNumber*)[dictWndRect objectForKey: @"x"] doubleValue],
            [(NSNumber*)[dictWndRect objectForKey: @"y"] doubleValue],
            [(NSNumber*)[dictWndRect objectForKey: @"w"] doubleValue],
            [(NSNumber*)[dictWndRect objectForKey: @"h"] doubleValue]
        );
        
        if (rectWindow.origin.x + rectWindow.size.width > rectScreen.size.width)
            rectWindow.origin.x = rectScreen.size.width - rectWindow.size.width;
        if (rectWindow.origin.x < 0)
            rectWindow.origin.x = 0;
        
        if (rectWindow.origin.y + rectWindow.size.height > rectScreen.size.height)
            rectWindow.origin.y = rectScreen.size.height - rectWindow.size.height;
        if (rectWindow.origin.y < 0)
            rectWindow.origin.y = 0;
    }
    else
        rectWindow = NSMakeRect(0, rectScreen.size.height - kDefaultWindowHeight, kDefaultWindowWidth, kDefaultWindowHeight);
    
    self.window = [[UnderlayOpenGLHostingWindow alloc] initWithContentRect: rectWindow
                                                                 styleMask: (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask )
                                                                   backing: NSBackingStoreBuffered
                                                                     defer: NO];
    
    self.window.title = [NSString stringWithUTF8String: Zephyros::GetAppName()];
    self.window.delegate = delegate;
    self.window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    
    // Rely on the window delegate to clean us up rather than immediately
    // releasing when the window gets closed. We use the delegate to do
    // everything from the autorelease pool so the window isn't on the stack
    // during cleanup (ie, a window close from javascript).
    [self.window setReleasedWhenClosed: NO];
    
    NSView* contentView = self.window.contentView;
    g_handler->SetMainHwnd(contentView);
    
    // create the browser view
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;
    settings.web_security = STATE_DISABLED;
    
    // initialize window info to the defaults for a child window
    windowInfo.SetAsChild(contentView, 0, 0, rectWindow.size.width, rectWindow.size.height);
    
    CefBrowserHost::CreateBrowser(windowInfo, g_handler.get(), Zephyros::GetAppURL(), settings, NULL);
    
    // size the window
    NSRect r = [self.window contentRectForFrameRect: self.window.frame];
    r.size.width = rectWindow.size.width;
    r.size.height = rectWindow.size.height;
    [self.window setFrame: [self.window frameRectForContentRect: r] display: YES];
}

- (void) tryToTerminateApplication: (NSApplication*) app
{
    if (g_handler.get() && !g_handler->IsClosing())
    {
        g_handler->CloseAllBrowsers(false);
        g_handler->GetClientExtensionHandler()->InvokeCallbacks("onAppTerminating", CefListValue::Create());
    }
}

//
// Restore the window when the dock icon is clicked.
//
- (BOOL) applicationShouldHandleReopen: (NSApplication*) sender hasVisibleWindows: (BOOL) flag
{
    if ([super applicationShouldHandleReopen: sender hasVisibleWindows: flag] == NO)
        return NO;
    
    // TODO: check if super class implementation works with this
    if (!flag)
    {
        g_isWindowBeingLoaded = true;
        [self createMainWindow];
    }
    
    return YES;
}

@end
