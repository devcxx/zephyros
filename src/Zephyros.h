/*******************************************************************************
 * Copyright (c) 2015 Vanamco AG, http://www.vanamco.com
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
 *******************************************************************************/


#ifndef Zephyros_h
#define Zephyros_h

#include <string>
#include <vector>


//////////////////////////////////////////////////////////////////////////
// Types

#ifdef OS_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <tchar.h>

#ifdef _UNICODE
typedef std::wstring String;
#else
typedef std::string String;
#endif

#define MAIN _tWinMain
#define MAIN_ARGS HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow
#define INIT_APPLICATION_ARGS hInstance, hPrevInstance, lpCmdLine, nCmdShow

#else

typedef std::string String;

#define TCHAR        char
#define TEXT(string) string

#define MAIN main
#define MAIN_ARGS int argc, const char* argv[]
#define INIT_APPLICATION_ARGS argc, argv

#endif  // OS_WIN


namespace Zephyros {

class NativeExtensions;

} // namespace Zephyros

    
//////////////////////////////////////////////////////////////////////////
// Zephyros Structures

namespace Zephyros {
    
typedef struct
{
    int nWidth;
    int nHeight;
} Size;

typedef struct
{
	TCHAR* szRegistryKey;
	int nIconID;
	int nMenuID;
	int nAccelID;
} WindowsInfo;

} // namespace Zephyros


#include "licensing.h"


//////////////////////////////////////////////////////////////////////////
// Zephyros Functions

namespace Zephyros {

int Run(MAIN_ARGS, const TCHAR* szAppName, const TCHAR* szAppVersion, const TCHAR* szAppURL);
int RunApplication(MAIN_ARGS);
void Shutdown();

const TCHAR* GetAppName();
const TCHAR* GetAppVersion();
const TCHAR* GetAppURL();

const TCHAR* GetCompanyName();
void SetCompanyName(const TCHAR* szCompanyName);

int GetResourceID(const TCHAR* szResourceName);
void SetResourceID(const TCHAR* szResourceName, int nID);

int GetMenuIDForCommand(const TCHAR* szCommand);
const TCHAR* GetMenuCommandForID(int nMenuID);
void SetMenuIDForCommand(const TCHAR* szCommand, int nMenuID);
    
Size GetDefaultWindowSize();
void SetDefaultWindowSize(int nWidth, int nHeight);

WindowsInfo GetWindowsInfo();
void SetWindowsInfo(const TCHAR* szRegistryKey, int nIconID, int nMenuID, int nAccelID);

const TCHAR* GetCrashReportingURL();
const TCHAR* GetCrashReportingPrivacyPolicyURL();
void SetCrashReportingURL(const TCHAR* szReportingURL, const TCHAR* szPrivacyPolicyURL);
    
const TCHAR* GetUpdaterURL();
void SetUpdaterURL(const TCHAR* szURL);
    
AbstractLicenseManager* GetLicenseManager();
void SetLicenseManager(AbstractLicenseManager* pLicenseManager);

NativeExtensions* GetNativeExtensions();
void SetNativeExtensions(NativeExtensions* pNativeExtensions);
    
String GetString(int stringId);
void SetString(int stringId, String str);
    
bool UseLogging();
void UseLogging(bool bUseLogging);

} // namespace Zephyros


#endif // Zephyros_h
