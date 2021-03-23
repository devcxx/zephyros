/*******************************************************************************
 * Copyright (c) 2015-2017 Vanamco AG, http://www.vanamco.com
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


#ifndef Zephyros_App_h
#define Zephyros_App_h
#pragma once

#include <string>
#include "base/types.h"
#include "zephyros.h"


#ifdef USE_CEF
class CefApp;
class CefBrowser;
class CefCommandLine;
#endif


namespace Zephyros {
namespace App {


#ifdef USE_CEF

/**
 * Returns the main browser window instance.
 */
CefRefPtr<CefBrowser> GetBrowser();

/**
 * Returns the main application window handle.
 */
ClientWindowHandle GetMainHwnd();

/**
 * Initialize the application command line.
 */
void InitCommandLine(int argc, const char* const* argv);

/**
 * Returns the application command line object.
 */
CefRefPtr<CefCommandLine> GetCommandLine();

/**
 * Returns the application settings based on command line arguments.
 */
void GetSettings(CefSettings& settings);

void SetWindowCreatedCallback(const WindowCreatedCallback& callback);

#endif

} // namespace App
} // namespace Zephyros


#endif // Zephyros_App_h
