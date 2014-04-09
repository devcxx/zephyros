//
//  NetworkUtil.m
//  Ghostlab
//
//  Created by Matthias Christen on 06.09.13.
//
//

#import "network_util.h"
#include "NSData+Base64.h"


#ifdef USE_WEBVIEW
@interface ConnectionDelegate : NSObject <NSURLConnectionDelegate>

@property NSMutableData *data;
@property JSObjectRef callback;
@property NSString *responseDataType;
@property NSString *responseMimeType;

@end


@implementation ConnectionDelegate

- (id) init: (JSObjectRef) callback withResponseDataType: (NSString*) type
{
    self = [super init];
    
    _data = [[NSMutableData alloc] init];
    _callback = callback;
    _responseDataType = type;
    
    return self;
}

- (void) connection: (NSURLConnection*) connection didReceiveResponse: (NSURLResponse*) response
{
    _data.length = 0;
    _responseMimeType = response.MIMEType;
}

- (void) connection: (NSURLConnection*) connection didReceiveData: (NSData*) data
{
    [_data appendData: data];
}

- (void) connectionDidFinishLoading: (NSURLConnection*) connection
{
    if (JSObjectIsFunction(g_ctx, _callback))
    {
        NSString *strRetData = nil;
        if ([_responseDataType isEqualToString: @"base64"])
            strRetData = [_data base64EncodedString];
        else
            strRetData = [[NSString alloc] initWithData: _data encoding: NSUTF8StringEncoding];
    
        if (strRetData == nil)
            strRetData = [[NSString alloc] initWithData: _data encoding: NSASCIIStringEncoding];
    
        if (strRetData == nil)
            return;
    
        JSStringRef retData = JSStringCreateWithCFString((__bridge CFStringRef) strRetData);
        JSStringRef retContentType = JSStringCreateWithCFString((__bridge CFStringRef) _responseMimeType);
    
        JSValueRef args[3];
        args[0] = JSValueMakeString(g_ctx, retData);
        args[1] = JSValueMakeString(g_ctx, retContentType);
        args[2] = JSValueMakeBoolean(g_ctx, true);
    
        JSObjectCallAsFunction(g_ctx, _callback, NULL, 3, args, NULL);
    
        JSStringRelease(retData);
        JSStringRelease(retContentType);
    }
    
    JSValueUnprotect(g_ctx, _callback);
}

- (void) connection: (NSURLConnection*) connection didFailWithError: (NSError*) error
{
    JSStringRef empty = JSStringCreateWithUTF8CString("");
    
    JSValueRef args[3];
    args[0] = JSValueMakeString(g_ctx, empty);
    args[1] = JSValueMakeString(g_ctx, empty);
    args[2] = JSValueMakeBoolean(g_ctx, false);
    
    JSObjectCallAsFunction(g_ctx, _callback, NULL, 3, args, NULL);
    
    JSStringRelease(empty);
    JSValueUnprotect(g_ctx, _callback);
}

@end
#endif


namespace NetworkUtil {
    
#ifdef USE_WEBVIEW
    
void MakeRequest(JSObjectRef callback, String httpMethod, String url, String postData, String postDataContentType, String responseDataType)
{
    JSValueProtect(g_ctx, callback);
    
    NSMutableURLRequest *urlRequest = [NSMutableURLRequest requestWithURL: [NSURL URLWithString: [NSString stringWithUTF8String: url.c_str()]]];
    urlRequest.HTTPMethod = [NSString stringWithUTF8String: httpMethod.c_str()];
    
    if (postData != "")
    {
        urlRequest.HTTPBody = [[NSString stringWithUTF8String: postData.c_str()] dataUsingEncoding: NSUTF8StringEncoding];

        if (postDataContentType != "")
            [urlRequest setValue: [NSString stringWithUTF8String: postDataContentType.c_str()] forHTTPHeaderField: @"Content-Type"];
        else
            [urlRequest setValue: @"application/x-www-form-urlencoded" forHTTPHeaderField: @"Content-Type"];

        [urlRequest setValue: [NSString stringWithFormat: @"%ld", urlRequest.HTTPBody.length] forHTTPHeaderField: @"Content-Length"];
    }
    
    NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest: urlRequest
                                                            delegate: [[ConnectionDelegate alloc] init: callback withResponseDataType: [NSString stringWithUTF8String: responseDataType.c_str()]]];
    [conn start];
}
    
#endif

} // namespace NetworkUtil