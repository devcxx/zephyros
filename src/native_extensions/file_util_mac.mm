/*******************************************************************************
 * Copyright (c) 2015-2016 Vanamco AG, http://www.vanamco.com
 *
 * The MIT License (MIT)
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * Matthias Christen, Vanamco AG
 * Florian Müller, Vanamco AG
 *******************************************************************************/


#import <Cocoa/Cocoa.h>

#import "zephyros_strings.h"

#import "native_extensions/file_util.h"
#import "native_extensions/image_util_mac.h"


#ifdef USE_CEF
extern CefRefPtr<Zephyros::ClientHandler> g_handler;
#endif

#ifdef USE_WEBVIEW
extern JSContextRef g_ctx;
#endif


namespace Zephyros {
namespace FileUtil {

void GetPathFromNSURL(NSURL* url, Path& path, BOOL useParentDirForSecurityBookmark)
{
    NSURL* urlDir = useParentDirForSecurityBookmark && ![url.absoluteString hasSuffix: @"/"] ? [url URLByDeletingLastPathComponent] : url;
    
    NSError *error = nil;
    NSData *bookmarkData = [urlDir bookmarkDataWithOptions: NSURLBookmarkCreationWithSecurityScope
                            includingResourceValuesForKeys: nil
                                             relativeToURL: nil
                                                     error: &error];
    NSURL *urlWithBookmark = nil;
    if (bookmarkData != nil)
    {
        BOOL bookmarkDataIsStale;
        urlWithBookmark = [NSURL URLByResolvingBookmarkData: bookmarkData
                                                    options: NSURLBookmarkResolutionWithSecurityScope
                                              relativeToURL: nil
                                        bookmarkDataIsStale: &bookmarkDataIsStale
                                                      error: &error];
    }
    else
        urlWithBookmark = url;

    path = Path([url.path UTF8String], [[urlWithBookmark absoluteString] UTF8String], bookmarkData != nil);
}


void ShowOpenDialog(CallbackId callback, BOOL canChooseFiles, BOOL useParentDirForSecurityBookmark)
{
#ifdef USE_WEBVIEW
    JSValueProtect(g_ctx, callback);
#endif

    // create and configure an open panel
    NSOpenPanel* browsePanel = [NSOpenPanel openPanel];
    browsePanel.canChooseFiles = canChooseFiles;
    browsePanel.canChooseDirectories = YES;
    browsePanel.allowsMultipleSelection = NO;

    [browsePanel beginSheetModalForWindow: [NSApp mainWindow] completionHandler: ^(NSInteger result)
    {
#ifdef USE_CEF
        JavaScript::Array args = JavaScript::CreateArray();
#endif
#ifdef USE_WEBVIEW
        JSValueRef arg;
#endif

        if (result == NSFileHandlingPanelOKButton)
        {
            Path path;
            GetPathFromNSURL(browsePanel.URL, path, useParentDirForSecurityBookmark);
            
#ifdef USE_CEF
            args->SetDictionary(0, path.CreateJSRepresentation());
#endif
#ifdef USE_WEBVIEW
            arg = path.CreateJSRepresentation()->AsJS();
#endif
        }
        else
        {
#ifdef USE_CEF
            args->SetNull(0);
#endif
#ifdef USE_WEBVIEW
            arg = JSValueMakeNull(g_ctx);
#endif
        }
        
#ifdef USE_CEF
        g_handler->GetClientExtensionHandler()->InvokeCallback(callback, args);
#endif
#ifdef USE_WEBVIEW
        JSObjectCallAsFunction(g_ctx, callback, NULL, 1, &arg, NULL);
        JSValueUnprotect(g_ctx, callback);
#endif
    }];
}

void ShowSaveFileDialog(CallbackId callback)
{
    // create and configure a save panel
    NSSavePanel* savePanel = [NSSavePanel savePanel];
    savePanel.canCreateDirectories = YES;
        
    // show the save panel
    [savePanel beginSheetModalForWindow: [NSApp mainWindow] completionHandler: ^(NSInteger result)
    {
#ifdef USE_CEF
        JavaScript::Array args = JavaScript::CreateArray();
#endif
#ifdef USE_WEBVIEW
        JSValueRef arg;
#endif
        
        if (result == NSFileHandlingPanelOKButton)
        {
            Path path;
            GetPathFromNSURL(savePanel.URL, path, NO);
            
#ifdef USE_CEF
            args->SetDictionary(0, path.CreateJSRepresentation());
#endif
#ifdef USE_WEBVIEW
            arg = path.CreateJSRepresentation()->AsJS();
#endif
        }
        else
        {
#ifdef USE_CEF
            args->SetNull(0);
#endif
#ifdef USE_WEBVIEW
            arg = JSValueMakeNull(g_ctx);
#endif
        }
      
#ifdef USE_CEF
        g_handler->GetClientExtensionHandler()->InvokeCallback(callback, args);
#endif
#ifdef USE_WEBVIEW
        JSObjectCallAsFunction(g_ctx, callback, NULL, 1, &arg, NULL);
        JSValueUnprotect(g_ctx, callback);
#endif
    }];
}

void ShowOpenFileDialog(CallbackId callback)
{
    ShowOpenDialog(callback, YES, NO);
}

void ShowOpenDirectoryDialog(CallbackId callback)
{
    ShowOpenDialog(callback, NO, NO);
}

void ShowOpenFileOrDirectoryDialog(CallbackId callback)
{
    ShowOpenDialog(callback, YES, YES);
}

void ShowInFileManager(String path)
{
    // get the actual directory (the directory itself or the directory containing the file)
    NSString *dir = [NSString stringWithUTF8String: path.c_str()];
    
    NSURL *url = [NSURL URLWithString: dir];
    if (url.isFileURL)
        dir = url.path;
    else
        url = [NSURL fileURLWithPath: dir];
    
    BOOL isDir;
    if ([[NSFileManager defaultManager] fileExistsAtPath: dir isDirectory: &isDir])
    {
        if (!isDir)
            url = [url URLByDeletingLastPathComponent];
    }
    else
    {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = [NSString stringWithUTF8String: Zephyros::GetString(ZS_OPEN_FINDER).c_str()];
        alert.informativeText = [NSString stringWithFormat: [NSString stringWithUTF8String: Zephyros::GetString(ZS_OPEN_PATH_DOESNT_EXIST).c_str()], [url.path UTF8String]];
        
        [alert beginSheetModalForWindow: [NSApp mainWindow] completionHandler: nil];
    }
    
    // open in Finder
    if (url != nil)
        [[NSWorkspace sharedWorkspace] openURL: url];
}

bool ExistsFile(String filename)
{
    return [[NSFileManager defaultManager] fileExistsAtPath: [NSString stringWithUTF8String: filename.c_str()]] == YES;
}

bool IsDirectory(String path)
{
    NSString *filename = [NSString stringWithUTF8String: path.c_str()];
    
    BOOL isDirectory = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath: filename isDirectory: &isDirectory])
        return false;
    
    return isDirectory == YES;
}

bool MakeDirectory(String path, bool recursive)
{
    if (![[NSFileManager defaultManager] createDirectoryAtPath: [NSString stringWithUTF8String: path.c_str()] withIntermediateDirectories:recursive attributes:nil error:nil] )
        return false;
    return true;
}

bool ReadFile(String filename, JavaScript::Object options, String& result)
{
    NSData *data = [[NSData alloc] initWithContentsOfFile: [NSString stringWithUTF8String: filename.c_str()]];
    bool ret = false;
    if (data != nil)
    {
        std::string encoding = "";
        if (options->HasKey("encoding"))
            encoding = options->GetString("encoding");
        
        if (encoding == "" || encoding == "utf-8" || encoding == "text/plain;utf-8")
        {
            // plain text
            NSString *text = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
            if (text == nil)
                text = [[NSString alloc] initWithData: data encoding: NSASCIIStringEncoding];

            if (text != nil)
            {
                result = [text UTF8String];
                ret = true;
            }
        }
        else if (encoding == "image/png;base64")
        {
            // base64-encoded PNG image
            NSImage *image = [[NSImage alloc] initWithData: data];
            if (image)
            {
                result = ImageUtil::NSImageToBase64EncodedPNG(image);
                ret = true;
            }
        }
    }

    return ret;
}

bool WriteFile(String filename, String contents)
{
    return [[NSString stringWithUTF8String: contents.c_str()] writeToFile: [NSString stringWithUTF8String: filename.c_str()]
                                                               atomically: NO
                                                                 encoding: NSUTF8StringEncoding
                                                                    error: NULL] == YES;
}
    
bool MoveFile(String oldFilename, String newFilename)
{
    return [[NSFileManager defaultManager] moveItemAtPath: [NSString stringWithUTF8String: oldFilename.c_str()]
                                                   toPath: [NSString stringWithUTF8String: newFilename.c_str()]
                                                    error: nil] == YES;
}

bool DeleteFiles(String filenames)
{
    NSFileManager *mgr = [NSFileManager defaultManager];
        
    if (filenames.find_first_of(TEXT("*?")) == String::npos)
        return [mgr removeItemAtPath: [NSString stringWithUTF8String: filenames.c_str()] error: nil] == YES;
        
    NSString *strFilenames = [NSString stringWithUTF8String: filenames.c_str()];
    NSString *path = [strFilenames stringByDeletingLastPathComponent];
        
    // list all the files in the directory
    NSError *err;
    NSArray *files = [mgr contentsOfDirectoryAtPath: path error: &err];
        
    if (files && !err)
    {
        NSPredicate *predicate = [NSPredicate predicateWithFormat: @"SELF LIKE %@", [strFilenames lastPathComponent]];
        for (NSString *filename in files)
        {
            if ([predicate evaluateWithObject: filename])
            {
                // the filename matches the predicate; try to delete the file
                if ([mgr removeItemAtPath: [path stringByAppendingPathComponent: filename] error: nil] == NO)
                    return false;
            }
        }
            
        return true;
    }
        
    return false;
}

bool GetDirectory(String& path)
{
    NSString *filename = [NSString stringWithUTF8String: path.c_str()];
    
    BOOL isDirectory = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath: filename isDirectory: &isDirectory])
        return false;
    
    if (!isDirectory)
    {
        filename = [filename stringByDeletingLastPathComponent];
        path = String([filename UTF8String]);
    }
    
    return true;
}

void LoadPreferences(String key, String& data)
{
    NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
    NSData *dataSettings = [settings objectForKey: [NSString stringWithUTF8String: key.c_str()]];
    NSString *res = @"";
    if (dataSettings != nil)
        res = [NSKeyedUnarchiver unarchiveObjectWithData: dataSettings];

    data = [res UTF8String];
}

void StorePreferences(String key, String data)
{
    NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
    [settings setObject: [NSKeyedArchiver archivedDataWithRootObject: [NSString stringWithUTF8String: data.c_str()]]
                 forKey: [NSString stringWithUTF8String: key.c_str()]];
    [settings synchronize];
}

bool StartAccessingPath(Path& path)
{
    if (path.HasSecurityAccessData())
        return [[NSURL URLWithString: [NSString stringWithUTF8String: path.GetURLWithSecurityAccessData().c_str()]] startAccessingSecurityScopedResource];
    return true;
}

void StopAccessingPath(Path& path)
{
    if (path.HasSecurityAccessData())
        [[NSURL URLWithString: [NSString stringWithUTF8String: path.GetURLWithSecurityAccessData().c_str()]] stopAccessingSecurityScopedResource];
}
 
void GetTempDir(Path& path)
{
    NSString *tempDir = NSTemporaryDirectory();
    if (tempDir == nil)
        tempDir = @"/tmp";

    path = Path(String([tempDir UTF8String]), TEXT(""), false);
}

String GetApplicationPath()
{
    return "";
}

void GetApplicationResourcesPath(Path& path)
{
    path = Path(String([[[NSBundle mainBundle] resourcePath] UTF8String]), TEXT(""), false);
}

} // namespace FileUtil
} // namespace Zephyros
