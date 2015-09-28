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


#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <direct.h>
#include <Shlwapi.h>
#include <ShlObj.h>

#include "lib/cef/include/cef_app.h"

#include "zephyros.h"
#include "base/app.h"
#include "base/cef/client_handler.h"

#include "res/windows/resource.h"


extern CefRefPtr<Zephyros::ClientHandler> g_handler;
extern HINSTANCE g_hInst;
extern bool g_isMessageLoopRunning;
extern bool g_isMultithreadedMessageLoop;
extern HWND g_hMessageWnd;

HANDLE g_hndLogFile;


namespace Zephyros {
namespace App {

void Quit()
{
	// close the app's main window
	DestroyWindow(App::GetMainHwnd());
	CloseHandle(g_hndLogFile);
}

void QuitMessageLoop()
{
	if (!g_isMessageLoopRunning)
		return;

	if (g_isMultithreadedMessageLoop)
	{
		// running in multi-threaded message loop mode
		// need to execute PostQuitMessage on the main application thread
		DCHECK(g_hMessageWnd);
		PostMessage(g_hMessageWnd, WM_COMMAND, ID_QUIT, 0);
	}
	else
		CefQuitMessageLoop();

	CloseHandle(g_hndLogFile);
}

void BeginWait()
{
	HWND hwnd = GetActiveWindow();
	HCURSOR hCursor = LoadCursor(NULL, IDC_WAIT);
	SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) hCursor);
}

void EndWait()
{
	HWND hwnd = GetActiveWindow();
	HCURSOR hCursor = LoadCursor(NULL, IDC_ARROW);
	SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) hCursor);
}

void Alert(String title, String msg, AlertStyle style)
{
	HWND hWnd = NULL;
	if (g_handler.get())
		hWnd = g_handler->GetMainHwnd();

	UINT type = MB_ICONINFORMATION;
	switch (style)
	{
	case AlertWarning:
		type = MB_ICONWARNING;
		break;
	case AlertError:
		type = MB_ICONERROR;
		break;
	}

	MessageBox(hWnd, msg.c_str(), title.c_str(), type);
}

HANDLE OpenLogFile()
{
    TCHAR szLogFileName[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szLogFileName);

	const TCHAR* szCompanyName = Zephyros::GetCompanyName();
	if (szCompanyName != NULL && szCompanyName[0] != TCHAR('\0'))
	{
		PathAddBackslash(szLogFileName);
		PathAppend(szLogFileName, szCompanyName);
		CreateDirectory(szLogFileName, NULL);
	}

	PathAddBackslash(szLogFileName);
	PathAppend(szLogFileName, Zephyros::GetAppName());
	CreateDirectory(szLogFileName, NULL);

	PathAppend(szLogFileName, TEXT("\\debug.log"));
	HANDLE hndLogFile = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hndLogFile == INVALID_HANDLE_VALUE)
	{
		//AppShowErrorMessage();
		hndLogFile = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	// write marker bytes to mark the file as UTF-8 encoded
	BYTE marker[] = { 0xef, 0xbb, 0xbf };
	DWORD dwBytesWritten = 0;
	WriteFile(hndLogFile, marker, 3, &dwBytesWritten, NULL);

	return hndLogFile;
}

void Log(String msg)
{
	if (msg.length() == 0)
		return;

	int mbLen = WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), (int) msg.length(), NULL, 0, NULL, NULL);
	if (mbLen == 0)
	{
		App::ShowErrorMessage();
		return;
	}

	char bufTime[30];
	SYSTEMTIME time;
	GetSystemTime(&time);
	sprintf(bufTime, "%d-%d-%d %d:%d:%d.%d ", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
	DWORD dwBytesWritten = 0;
	WriteFile(g_hndLogFile, bufTime, (DWORD) strlen(bufTime), &dwBytesWritten, NULL);

	BYTE* buf = new BYTE[mbLen + 3];
	WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), (int) msg.length(), (LPSTR) buf, mbLen, NULL, NULL);
	buf[mbLen] = '\r';
	buf[mbLen + 1] = '\n';
	buf[mbLen + 2] = 0;
	dwBytesWritten = 0;
	WriteFile(g_hndLogFile, buf, mbLen + 2, &dwBytesWritten, NULL);
	FlushFileBuffers(g_hndLogFile);
	delete[] buf;
}

String ShowErrorMessage()
{
	TCHAR* msg = NULL;
	String strMsg;

	DWORD errCode = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, errCode, 0, (LPWSTR) &msg, 0, NULL);

	if (msg != NULL)
	{
		MessageBox(NULL, msg, TEXT("Error"), MB_ICONERROR);
		String strMsg = String(msg);
		LocalFree(msg);
	}
	else
	{
		msg = new TCHAR[100];

#ifdef _UNICODE
		wsprintf(msg, L"Error code %ld", errCode);
#else
		sprintf(msg, "Error code %ld", errCode);
#endif

		MessageBox(NULL, msg, TEXT("Error"), MB_ICONERROR);
		strMsg = String(msg);
		delete[] msg;
	}

	return strMsg;
}

void SetMenuItemStatuses(JavaScript::Object items)
{
	if (!g_handler.get())
		return;

	HMENU hMenu = GetMenu(g_handler->GetMainHwnd());
	if (!hMenu)
		return;

	JavaScript::KeyList keys;
	items->GetKeys(keys);
	for (JavaScript::KeyType commandId : keys)
	{
		String strCmdId = commandId;
		int nID = Zephyros::GetMenuIDForCommand(strCmdId.c_str());
		if (nID)
		{
			int status = items->GetInt(commandId);
			EnableMenuItem(hMenu, nID, MF_BYCOMMAND | ((status & 0x01) ? MF_ENABLED : MF_DISABLED));
			CheckMenuItem(hMenu, nID, MF_BYCOMMAND | ((status & 0x02) ? MF_CHECKED : MF_UNCHECKED));
		}
	}
}

} // namespace App
} // namespace Zephyros
