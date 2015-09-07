#include <string>
#include <sstream>

#ifdef USE_CEF
#include "lib/cef/include/cef_base.h"
#include "lib/cef/include/cef_values.h"
#endif


#ifdef OS_WIN

#ifdef _UNICODE
typedef std::wstring String;
typedef std::wstringstream StringStream;
#define TO_STRING std::to_wstring
#else
typedef std::string String;
typedef std::stringstream StringStream;
#define TO_STRING std::to_string
#endif

#else

#define _tprintf     printf
#define _tcscpy      strcpy
#define _tcscat      strcat
#define _tcslen      strlen
#define _ttoi		 atoi
#define _tcsnccmp    strncmp

typedef std::string String;
typedef std::stringstream StringStream;

#endif  // OS_WIN



#ifdef USE_CEF

#include "base/cef/jsbridge_v8.h"

namespace Zephyros {
class ClientHandler;
}

class CefBrowser;

typedef CefRefPtr<Zephyros::ClientHandler> ClientHandlerPtr;
typedef CefRefPtr<CefBrowser> BrowserPtr;

#if defined(OS_LINUX)
// The Linux client uses GTK instead of the underlying platform type (X11).
#include <gtk/gtk.h>
#define ClientWindowHandle GtkWidget*
#else
#define ClientWindowHandle CefWindowHandle
#endif

typedef CefWindowHandle WindowHandle;

typedef int CallbackId;

#endif  // USE_CEF


#ifdef USE_WEBVIEW

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
typedef NSView* WindowHandle;
#else
#include <objc/objc.h>
typedef id WindowHandle;
#endif

#include "jsbridge_webview.h"

#endif  // USE_WEBVIEW


#include "logging.h"
