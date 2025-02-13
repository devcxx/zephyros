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


#include <iomanip>
#include <tchar.h>

#include <Shlwapi.h>
#include <ShlObj.h>
#include <Uxtheme.h>
#include <winhttp.h>
#include <vsstyle.h>

#include "zephyros_strings.h"

#include "base/app.h"
#include "base/licensing.h"
#include "base/cef/client_handler.h"

#include "util/string_util.h"
#include "components/dialog_win.h"
#include "native_extensions/file_util.h"

#include "res/windows/resource.h"


#define BUF_SIZE 512

extern HINSTANCE g_hInst;
extern CefRefPtr<Zephyros::ClientHandler> g_handler;


namespace Zephyros {


//////////////////////////////////////////////////////////////////////////
// LicenseData Implementation

LicenseData::LicenseData(const TCHAR* szLicenseInformationFilename)
    : m_szLicenseInfoFilename(szLicenseInformationFilename)
{
    // read the license data from the file

    TCHAR szFilename[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szFilename);

    const TCHAR* szCompanyName = Zephyros::GetCompanyName();
    if (szCompanyName != NULL && szCompanyName[0] != TCHAR('\0'))
    {
        PathAddBackslash(szFilename);
        PathAppend(szFilename, szCompanyName);
        CreateDirectory(szFilename, NULL);
    }

    PathAddBackslash(szFilename);
    PathAppend(szFilename, Zephyros::GetAppID());
    CreateDirectory(szFilename, NULL);

    PathAppend(szFilename, TEXT("\\"));
    PathAppend(szFilename, m_szLicenseInfoFilename);

    HANDLE hFile = CreateFile(szFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // if the file doesn't exist in the default location, try the /res folder within the program directory
    if (hFile == INVALID_HANDLE_VALUE)
    {
        Path path;
        FileUtil::GetApplicationResourcesPath(path);
        _tcscpy(szFilename, path.GetPath().c_str());
        PathAppend(szFilename, TEXT("\\"));
        PathAppend(szFilename, m_szLicenseInfoFilename);

        hFile = CreateFile(szFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD numBytesRead = 0;
        DWORD len = 0;
        TCHAR* buf = NULL;

        // read demo tokens
        DWORD numDemoTokens = 0;
        ReadFile(hFile, &numDemoTokens, sizeof(DWORD), &numBytesRead, NULL);
        for (int i = 0; i < (int) numDemoTokens; ++i)
        {
            ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL);
            if (len <= 0 || len > 500)
                break;

            buf = new TCHAR[len];
            ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
            m_demoTokens.push_back(String(buf, (String::size_type) len));
            delete[] buf;
        }

        // read time stamp
        ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL);
        if (0 < len && len < 500)
        {
            buf = new TCHAR[len];
            ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
            m_timestampLastDemoTokenUsed = String(buf, (String::size_type) len);
            delete[] buf;
        }

        // read activation cookie
        ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL);
        if (0 < len && len < 500)
        {
            buf = new TCHAR[len];
            ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
            m_activationCookie = String(buf, (String::size_type) len);
            delete[] buf;
        }

        // read license key
        if (ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL))
        {
            if (numBytesRead > 0 && 0 < len && len < 500)
            {
                buf = new TCHAR[len];
                ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
                m_licenseKey = LicenseData::Decrypt(String(buf, (String::size_type) len));
                delete[] buf;
            }
        }

        // read name
        if (ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL))
        {
            if (numBytesRead > 0 && 0 < len && len < 500)
            {
                buf = new TCHAR[len];
                ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
                m_name = LicenseData::Decrypt(String(buf, (String::size_type) len));
                delete[] buf;
            }
        }

        // read company
        if (ReadFile(hFile, &len, sizeof(DWORD), &numBytesRead, NULL))
        {
            if (numBytesRead > 0 && 0 < len && len < 500)
            {
                buf = new TCHAR[len];
                ReadFile(hFile, buf, len * sizeof(TCHAR), &numBytesRead, NULL);
                m_company = LicenseData::Decrypt(String(buf, (String::size_type) len));
                delete[] buf;
            }
        }

        CloseHandle(hFile);
    }
}

void LicenseData::Save()
{
    // save the license data to the file

    TCHAR szFilename[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szFilename);

    const TCHAR* szCompanyName = Zephyros::GetCompanyName();
    if (szCompanyName != NULL && szCompanyName[0] != TCHAR('\0'))
    {
        PathAddBackslash(szFilename);
        PathAppend(szFilename, szCompanyName);
        CreateDirectory(szFilename, NULL);
    }

    PathAddBackslash(szFilename);
    PathAppend(szFilename, Zephyros::GetAppID());
    CreateDirectory(szFilename, NULL);

    PathAppend(szFilename, TEXT("\\"));
    PathAppend(szFilename, m_szLicenseInfoFilename);

    HANDLE hFile = CreateFile(szFilename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        hFile = CreateFile(szFilename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD numBytesWritten;
        DWORD len;

        // write demo tokens
        DWORD numDemoTokens = (DWORD) m_demoTokens.size();
        WriteFile(hFile, &numDemoTokens, sizeof(DWORD), &numBytesWritten, NULL);
        for (String token : m_demoTokens)
        {
            len = (DWORD) token.length();
            WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
            WriteFile(hFile, token.c_str(), (DWORD) (token.length() * sizeof(TCHAR)), &numBytesWritten, NULL);
        }

        // write time stamp
        len = (DWORD) m_timestampLastDemoTokenUsed.length();
        WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
        WriteFile(hFile, m_timestampLastDemoTokenUsed.c_str(), (DWORD) (m_timestampLastDemoTokenUsed.length() * sizeof(TCHAR)), &numBytesWritten, NULL);

        // write activation cookie
        len = (DWORD) m_activationCookie.length();
        WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
        WriteFile(hFile, m_activationCookie.c_str(), (DWORD) (m_activationCookie.length() * sizeof(TCHAR)), &numBytesWritten, NULL);

        // write license key
        String licenseKey = LicenseData::Encrypt(m_licenseKey);
        len = (DWORD) licenseKey.length();
        WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
        WriteFile(hFile, licenseKey.c_str(), (DWORD) (licenseKey.length() * sizeof(TCHAR)), &numBytesWritten, NULL);

        // write name
        String name = LicenseData::Encrypt(m_name);
        len = (DWORD) name.length();
        WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
        WriteFile(hFile, name.c_str(), (DWORD) (name.length() * sizeof(TCHAR)), &numBytesWritten, NULL);

        // write company
        String company = LicenseData::Encrypt(m_company);
        len = (DWORD) company.length();
        WriteFile(hFile, &len, sizeof(DWORD), &numBytesWritten, NULL);
        WriteFile(hFile, company.c_str(), (DWORD) (company.length() * sizeof(TCHAR)), &numBytesWritten, NULL);

        CloseHandle(hFile);
    }
}

String LicenseData::Now()
{
    String result = TEXT("");

    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_DSS, CRYPT_VERIFYCONTEXT))
        return TEXT("");

    HCRYPTHASH hHash;
    if (CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hHash))
    {
        // get the current time
        SYSTEMTIME time;
        GetLocalTime(&time);

        std::stringstream ss;
        ss << time.wYear << "-" << time.wMonth << "-" << time.wDay << "-" << time.wDayOfWeek;
        std::string strTimestamp = ss.str();

        // add the hash data
        if (!CryptHashData(hHash, (BYTE*) strTimestamp.c_str(), (DWORD) strTimestamp.length(), 0))
            App::ShowErrorMessage();

        // extract the hash data and construct the result string
        DWORD dwHashLen = 0;
        if (!CryptGetHashParam(hHash, HP_HASHVAL, NULL, &dwHashLen, 0))
            App::ShowErrorMessage();
        BYTE* pData = new BYTE[dwHashLen];
        if (!CryptGetHashParam(hHash, HP_HASHVAL, pData, &dwHashLen, 0))
            App::ShowErrorMessage();

        StringStream ssResult;
        ssResult << std::hex << std::setfill(TEXT('0'));
        for (int i = 0; i < (int) dwHashLen; ++i)
            ssResult << std::setw(2) << pData[i];
        result = ssResult.str();

        delete[] pData;
        CryptDestroyHash(hHash);
    }

    CryptReleaseContext(hCryptProv, 0);
    return result;
}


//////////////////////////////////////////////////////////////////////////
// LicenseManager Implementation

LicenseManagerImpl::LicenseManagerImpl()
    : m_timerId((UINT_PTR) -1), m_pDemoDlg(NULL)
{
    InitConfig();
}

LicenseManagerImpl::~LicenseManagerImpl()
{
    KillTimer();
}

bool LicenseManagerImpl::VerifyKey(String key, String info, const TCHAR* szPubkey)
{
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_DSS, CRYPT_VERIFYCONTEXT))
        return false;

    bool result = false;

    BYTE szDERPubKey[2048];
    DWORD dwDERPubKeyLen = 2048;
    CERT_PUBLIC_KEY_INFO* publicKeyInfo = NULL;
    DWORD dwPublicKeyInfoLen = 0;

    if (CryptStringToBinary(szPubkey, (DWORD) _tcslen(szPubkey), CRYPT_STRING_BASE64, szDERPubKey, &dwDERPubKeyLen, NULL, NULL) &&
        CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, szDERPubKey, dwDERPubKeyLen, CRYPT_ENCODE_ALLOC_FLAG, NULL, &publicKeyInfo, &dwPublicKeyInfoLen))
    {
        HCRYPTKEY hPubKey;
        if (CryptImportPublicKeyInfo(hCryptProv, X509_ASN_ENCODING, publicKeyInfo, &hPubKey))
        {
            HCRYPTHASH hHash;
            if (CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hHash))
            {
                const TCHAR* szInfo = info.c_str();
                int infoLen = (int) info.length();

                int mbLen = WideCharToMultiByte(CP_UTF8, 0, szInfo, infoLen, NULL, 0, NULL, NULL);
                if (mbLen > 0)
                {
                    BYTE* buf = new BYTE[mbLen];
                    mbLen = WideCharToMultiByte(CP_UTF8, 0, szInfo, infoLen, (LPSTR) buf, mbLen, NULL, NULL);
                    if (mbLen > 0)
                    {
                        CryptHashData(hHash, buf, mbLen, 0);

                        BYTE* pKey = NULL;
                        size_t keyLen = 0;
                        if (DecodeKey(key, &pKey, &keyLen))
                        {
                            BYTE* pDSASignature = NULL;
                            DWORD dwDSASignatureLen = 0;
                            if (CryptDecodeObjectEx(X509_ASN_ENCODING, X509_DSS_SIGNATURE, pKey, (DWORD) keyLen, CRYPT_ENCODE_ALLOC_FLAG, NULL, &pDSASignature, &dwDSASignatureLen))
                            {
                                if (CryptVerifySignature(hHash, pDSASignature, dwDSASignatureLen, hPubKey, NULL, 0))
                                    result = true;

                                LocalFree(pDSASignature);
                            }

                            delete[] pKey;
                        }
                    }

                    delete[] buf;
                }

                CryptDestroyHash(hHash);
            }

            CryptDestroyKey(hPubKey);
        }

        LocalFree(publicKeyInfo);
    }

    CryptReleaseContext(hCryptProv, 0);

    return result;
}


class UpgradeDialog : public Dialog
{
public:
    UpgradeDialog(HWND hwndParent = NULL)
      : Dialog(Zephyros::GetString(ZS_PREVVERSIONDLG_WNDTITLE), hwndParent),
        m_hFontBold(NULL),
        m_hIcon(NULL)
    {
        ON_COMMAND(IDOK, UpgradeDialog::OnOK);
        ON_COMMAND(IDCANCEL, UpgradeDialog::OnCancel);
        ON_MESSAGE(WM_PAINT, UpgradeDialog::OnPaint);
        ON_MESSAGE(WM_INITDIALOG, UpgradeDialog::OnInitDialog);

        AddStatic(Zephyros::GetString(ZS_PREVVERSIONDLG_TITLE), 105, 19, 185, 8, IDC_UPGRADETITLE);
        AddStatic(Zephyros::GetString(ZS_PREVVERSIONDLG_DESCRIPTION), 105, 40, 185, 30);

        const TCHAR* szUpgradeURL = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetUpgradeURL();
        if (szUpgradeURL != NULL && szUpgradeURL[0] != TCHAR('\0'))
        {
            AddDefaultButton(Zephyros::GetString(ZS_PREVVERSIONDLG_UPGRADE), 160, 85, 130, 14, IDOK);
            AddButton(Zephyros::GetString(ZS_PREVVERSIONDLG_BACK), 105, 85, 50, 14, IDCANCEL);
        }
        else
            AddDefaultButton(Zephyros::GetString(ZS_PREVVERSIONDLG_BACK), 105, 155, 50, 14, IDCANCEL);
    }

    ~UpgradeDialog()
    {
        if (m_hFontBold != NULL)
            DeleteObject(m_hFontBold);
        if (m_hIcon != NULL)
            DestroyIcon(m_hIcon);
    }

protected:
    void OnOK(WORD w, LPARAM l)
    {
        // open URL
        static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->OpenUpgradeLicenseURL();
    }

    void OnCancel(WORD w, LPARAM l)
    {
        EndDialog(m_hwnd, IDCANCEL);
    }

    void OnInitDialog(WPARAM wParam, LPARAM lParam)
    {
        CenterDialog();

        // make the title bold
        HFONT hFont = (HFONT) SendDlgItemMessage(m_hwnd, IDC_UPGRADETITLE, WM_GETFONT, 0, 0);
        LOGFONT font;
        GetObject(hFont, sizeof(LOGFONT), &font);
        font.lfWeight = FW_BOLD;
        m_hFontBold = CreateFontIndirect(&font);
        SendDlgItemMessage(m_hwnd, IDC_UPGRADETITLE, WM_SETFONT, (WPARAM) m_hFontBold, TRUE);
    }

    void OnPaint(WPARAM wParam, LPARAM lParam)
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(m_hwnd, &ps);

        // paint the application icon
        if (m_hIcon == NULL)
            m_hIcon = (HICON) LoadImage(g_hInst, Zephyros::GetWindowsInfo().szIcon, IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR | LR_LOADFROMFILE);
        if (m_hIcon != NULL)
            DrawIconEx(hDC, 16, 10, m_hIcon, 128, 128, 0, NULL, DI_NORMAL);

        // clean up
        EndPaint(m_hwnd, &ps);
    }

private:
    HICON m_hIcon;
    HFONT m_hFontBold;
};

class LicenseKeyDialog : public Dialog
{
public:
    LicenseKeyDialog(HWND hwndParent = NULL)
        : Dialog(Zephyros::GetString(ZS_ENTERLICKEYDLG_WNDTITLE), hwndParent)
    {
        ON_COMMAND(IDOK, LicenseKeyDialog::OnOK);
        ON_COMMAND(IDCANCEL, LicenseKeyDialog::OnCancel);
        ON_MESSAGE(WM_INITDIALOG, LicenseKeyDialog::OnInitDialog);

        AddGroupBox(Zephyros::GetString(ZS_ENTERLICKEYDLG_TITLE), 7, 7, 295, 117, -1, WS_CHILD | WS_VISIBLE | BS_CENTER);
        AddStatic(Zephyros::GetString(ZS_ENTERLICKEYDLG_FULL_NAME), 37, 33, 34, 8, -1, WS_CHILD | WS_VISIBLE | SS_RIGHT);
        AddTextBox(TEXT(""), 87, 30, 195, 14, IDC_EDIT_FULLNAME, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
        AddStatic(Zephyros::GetString(ZS_ENTERLICKEYDLG_ORGANIZATION), 27, 55, 44, 8, -1, WS_CHILD | WS_VISIBLE | SS_RIGHT);
        AddTextBox(TEXT(""), 87, 52, 195, 14, IDC_EDIT_ORGANIZATION, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
        AddStatic(Zephyros::GetString(ZS_ENTERLICKEYDLG_LICKEY), 30, 77, 41, 8, -1, WS_CHILD | WS_VISIBLE | SS_RIGHT);
        AddTextBox(TEXT(""), 87, 74, 195, 33, IDC_EDIT_LICENSEKEY, WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, WS_EX_CLIENTEDGE);
        AddDefaultButton(Zephyros::GetString(ZS_ENTERLICKEYDLG_ACTIVATE), 198, 140, 50, 14, IDOK);
        AddButton(Zephyros::GetString(ZS_ENTERLICKEYDLG_CANCEL), 252, 140, 50, 14, IDCANCEL);
    }

protected:
    void OnInitDialog(WPARAM wParam, LPARAM lParam)
    {
        CenterDialog();
    }

    void OnOK(WORD w, LPARAM l)
    {
        TCHAR buf[BUF_SIZE + 1];

        GetDlgItemText(m_hwnd, IDC_EDIT_FULLNAME, buf, BUF_SIZE);
        String fullName = buf;
        GetDlgItemText(m_hwnd, IDC_EDIT_ORGANIZATION, buf, BUF_SIZE);
        String organization = buf;
        GetDlgItemText(m_hwnd, IDC_EDIT_LICENSEKEY, buf, BUF_SIZE);
        String key = buf;

        int ret = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->Activate(fullName, organization, key);

        switch (ret)
        {
        case ACTIVATION_SUCCEEDED:
            EndDialog(m_hwnd, IDOK);
            break;

        case ACTIVATION_OBSOLETELICENSE:
            UpgradeDialog dlg(m_hwnd);
            dlg.DoModal();
            break;
        }
    }

    void OnCancel(WORD w, LPARAM l)
    {
        EndDialog(m_hwnd, IDCANCEL);
    }
};


class DemoDialog : public Dialog
{
public:
    DemoDialog(int numDaysLeft)
      : Dialog(Zephyros::GetString(ZS_DEMODLG_WNDTITLE)),
        m_numDaysLeft(numDaysLeft),
        m_hFontBold(NULL),
        m_hIcon(NULL)
    {
        ON_MESSAGE(WM_INITDIALOG, DemoDialog::OnInitDialog);
        ON_MESSAGE(WM_PAINT, DemoDialog::OnPaint);

        ON_COMMAND(IDOK, DemoDialog::OnContinueDemo);
        ON_COMMAND(IDC_PURCHASELICENSE, DemoDialog::OnPurchaseLicense);
        ON_COMMAND(IDC_ENTERLICENSEKEY, DemoDialog::OnEnterLicenseKey);
        ON_COMMAND(IDCANCEL, DemoDialog::OnCancel);

        AddStatic(Zephyros::GetString(ZS_DEMODLG_TITLE), 105, 19, 185, 8, IDC_DEMOTITLE);
        AddStatic(Zephyros::GetString(ZS_DEMODLG_DESCRIPTION), 105, 40, 185, 30);
        AddGroupBox(Zephyros::GetString(ZS_DEMODLG_REMAINING_TIME), 18, 92, 272, 59, -1, WS_CHILD | WS_VISIBLE | BS_CENTER);
        AddStatic(Zephyros::GetString(ZS_DEMODLG_REMAINING_TIME_DESC), 33, 107, 210, 8);
        AddStatic(TEXT(""), 33, 132, 243, 8, IDC_TEXT_DAYS);

        AddDefaultButton(TEXT(""), 18, 172, 80, 14, IDOK);

        const TCHAR* szShopURL = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetShopURL();
        if (szShopURL != NULL && szShopURL[0] != TCHAR('\0'))
            AddButton(Zephyros::GetString(ZS_DEMODLG_PURCHASE_LICENSE), 114, 172, 80, 14, IDC_PURCHASELICENSE);

        AddButton(Zephyros::GetString(ZS_DEMODLG_ENTER_LICKEY), 210, 172, 80, 14, IDC_ENTERLICENSEKEY);
    }

    ~DemoDialog()
    {
        if (m_hFontBold != NULL)
            DeleteObject(m_hFontBold);
        if (m_hIcon != NULL)
            DestroyIcon(m_hIcon);
    }

    void Dismiss(INT_PTR result)
    {
        // TODO: use PostMessage() if in Worker thread
        EndDialog(m_hwnd, result);
    }

protected:
    void OnInitDialog(WPARAM wParam, LPARAM lParam)
    {
        CenterDialog();

        // set the text of the number of days display
        String captionNumDays = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->GetDaysCountLabelText();
        SetDlgItemText(m_hwnd, IDC_TEXT_DAYS, captionNumDays.c_str());

        // set the caption of the demo button
        String captionDemoButton = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->GetDemoButtonCaption();
        SetDlgItemText(m_hwnd, IDOK, captionDemoButton.c_str());

        // make the title bold
        HFONT hFont = (HFONT) SendDlgItemMessage(m_hwnd, IDC_DEMOTITLE, WM_GETFONT, 0, 0);
        LOGFONT font;
        GetObject(hFont, sizeof(LOGFONT), &font);
        font.lfWeight = FW_BOLD;
        m_hFontBold = CreateFontIndirect(&font);
        SendDlgItemMessage(m_hwnd, IDC_DEMOTITLE, WM_SETFONT, (WPARAM) m_hFontBold, TRUE);
    }

    void OnPaint(WPARAM wParam, LPARAM lParam)
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(m_hwnd, &ps);

        // paint the application icon
        if (m_hIcon == NULL)
            m_hIcon = (HICON) LoadImage(g_hInst, Zephyros::GetWindowsInfo().szIcon, IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR | LR_LOADFROMFILE);
        if (m_hIcon != NULL)
            DrawIconEx(hDC, 16, 10, m_hIcon, 128, 128, 0, NULL, DI_NORMAL);

        // paint the meter bar
        HTHEME theme = OpenThemeData(m_hwnd, TEXT("PROGRESS"));

        // set the fill state of the meter
        WPARAM fillstate = PBFS_PARTIAL;
        if (m_numDaysLeft <= 3)
            fillstate = PBFS_PAUSED;
        if (m_numDaysLeft <= 1)
            fillstate = PBFS_ERROR;

        // find the rect where to paint the meter
        RECT r;
        GetWindowRect(GetDlgItem(m_hwnd, IDC_TEXT_DAYS), &r);
        ScreenToClient(m_hwnd, (POINT*) &(r.left));
        ScreenToClient(m_hwnd, (POINT*) &(r.right));
        LONG offs = r.bottom - r.top + 2;
        r.top -= offs;
        r.bottom -= offs;

        // draw the background
        DrawThemeBackground(theme, hDC, PP_TRANSPARENTBAR, (int) PBBS_PARTIAL, &r, NULL);

        // draw the filling
        int nNumDemoDays = static_cast<Zephyros::LicenseManager*> (Zephyros::GetLicenseManager())->GetNumberOfDemoDays();
        r.right = r.left + ((r.right - r.left) * (nNumDemoDays - m_numDaysLeft)) / nNumDemoDays;
        DrawThemeBackground(theme, hDC, PP_FILL, (int) fillstate, &r, NULL);

        // clean up
        CloseThemeData(theme);
        EndPaint(m_hwnd, &ps);
    }

    void OnContinueDemo(WORD w, LPARAM l)
    {
        bool canContinue = m_numDaysLeft > 0;
        if (canContinue)
            canContinue = static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->ContinueDemo();

        Dismiss(canContinue ? IDOK : IDCANCEL);
    }

    void OnPurchaseLicense(WORD w, LPARAM l)
    {
        Zephyros::GetLicenseManager()->OpenPurchaseLicenseURL();
    }

    void OnEnterLicenseKey(WORD w, LPARAM l)
    {
        LicenseKeyDialog dlg(m_hwnd);
        dlg.DoModal();
    }

    void OnCancel(WORD w, LPARAM l)
    {
        Dismiss(IDCANCEL);
    }

private:
    int m_numDaysLeft;

    HICON m_hIcon;
    HFONT m_hFontBold;
};


void CALLBACK DemoTimeout(HWND hwnd, UINT msg, UINT_PTR event, DWORD time)
{
    if (Zephyros::GetLicenseManager()->CheckDemoValidity())
        static_cast<Zephyros::LicenseManager*>(Zephyros::GetLicenseManager())->GetLicenseManagerImpl()->KillTimer();
}

void LicenseManagerImpl::KillTimer()
{
    if (m_timerId == -1)
        return;

    ::KillTimer(NULL, m_timerId);
    m_timerId = (UINT_PTR) -1;
}

void LicenseManagerImpl::ShowDemoDialog()
{
    // cancel any previous timer
    KillTimer();

    // show the demo dialog
    m_pDemoDlg = new DemoDialog(GetNumDaysLeft());
    INT_PTR nResult = m_pDemoDlg->DoModal();
    delete m_pDemoDlg;
    m_pDemoDlg = NULL;
    m_canStartApp = nResult == IDOK;

    // check the validity of the demo after 6 hours
    if (m_canStartApp && !IsActivated())
        m_timerId = SetTimer(NULL, 0, 6 * 3600 * 1000, DemoTimeout);

    // if the message loop is already running, post a quit message
    if (!m_canStartApp)
        App::QuitMessageLoop();
}

void LicenseManagerImpl::ShowEnterLicenseDialog()
{
    LicenseKeyDialog dlg(g_handler->GetMainHwnd());
    dlg.DoModal();
}

void LicenseManagerImpl::OpenPurchaseLicenseURL()
{
    if (!m_config.shopURL)
        return;

    SHELLEXECUTEINFO execInfo = { 0 };

    execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    execInfo.fMask = SEE_MASK_DEFAULT;
    execInfo.lpVerb = TEXT("open");
    execInfo.lpFile = m_config.shopURL;
    execInfo.nShow = SW_SHOW;

    ShellExecuteEx(&execInfo);
}

void LicenseManagerImpl::OpenUpgradeLicenseURL()
{
    if (!m_config.upgradeURL)
        return;

    SHELLEXECUTEINFO execInfo = { 0 };

    execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    execInfo.fMask = SEE_MASK_DEFAULT;
    execInfo.lpVerb = TEXT("open");
    execInfo.lpFile = m_config.upgradeURL;
    execInfo.nShow = SW_SHOW;

    ShellExecuteEx(&execInfo);
}

bool LicenseManagerImpl::SendRequest(String url, std::string strPostData, std::stringstream& out)
{
    bool ret = false;

    // parse the URL
    URL_COMPONENTS urlComponents;
    ZeroMemory(&urlComponents, sizeof(URL_COMPONENTS));
    urlComponents.dwStructSize = sizeof(URL_COMPONENTS);

    // set required component lengths to non-zero  so that they are cracked.
    urlComponents.dwSchemeLength = (DWORD) -1;
    urlComponents.dwHostNameLength = (DWORD) -1;
    urlComponents.dwUrlPathLength = (DWORD) -1;
    urlComponents.dwExtraInfoLength = (DWORD) -1;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComponents))
        return ret;

    HINTERNET hSession = WinHttpOpen(Zephyros::GetAppID(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession != NULL)
    {
        String strHostName(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
        HINTERNET hHttp = WinHttpConnect(hSession, strHostName.c_str(), urlComponents.nPort, 0);
        if (hHttp != NULL)
        {
            String strPath(urlComponents.lpszUrlPath, urlComponents.dwUrlPathLength);
            bool isHTTPS = urlComponents.nScheme == INTERNET_SCHEME_HTTPS;
            HINTERNET hRequest = WinHttpOpenRequest(hHttp, TEXT("POST"), strPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_REFRESH | (isHTTPS ? WINHTTP_FLAG_SECURE : 0));
            if (hRequest != NULL)
            {
                // check for proxy
                bool setProxySucceeded = false;

                WINHTTP_PROXY_INFO proxyInfo = { 0 };
                WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyConfig = { 0 };

                if (WinHttpGetIEProxyConfigForCurrentUser(&ieProxyConfig))
                {
                    if (ieProxyConfig.fAutoDetect || ieProxyConfig.lpszAutoConfigUrl != NULL)
                    {
                        // auto detect proxy options
                        WINHTTP_AUTOPROXY_OPTIONS autoproxyOption = { 0 };
                        autoproxyOption.dwFlags = 0;
                        autoproxyOption.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
                        autoproxyOption.fAutoLogonIfChallenged = TRUE;

                        if (ieProxyConfig.fAutoDetect)
                            autoproxyOption.dwFlags |= WINHTTP_AUTOPROXY_AUTO_DETECT;

                        // fallback to a autoconfig URL if one has been set
                        if (ieProxyConfig.lpszAutoConfigUrl != NULL)
                        {
                            autoproxyOption.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
                            autoproxyOption.lpszAutoConfigUrl = ieProxyConfig.lpszAutoConfigUrl;
                        }

                        // get the proxy for the URL we want to access and set the option
                        if (WinHttpGetProxyForUrl(hSession, url.c_str(), &autoproxyOption, &proxyInfo))
                        {
                            if (WinHttpSetOption(hRequest, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo)))
                                setProxySucceeded = true;
                        }
                        else
                        {
                            // couldn't get autoproxy; assume we can use direct connection
                            setProxySucceeded = true;
                        }
                    }
                    else if (ieProxyConfig.lpszProxy != NULL)
                    {
                        // use fixed proxy options
                        WINHTTP_PROXY_INFO p = { 0 };
                        p.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
                        p.lpszProxy = ieProxyConfig.lpszProxy;
                        p.lpszProxyBypass = ieProxyConfig.lpszProxyBypass;

                        if (WinHttpSetOption(hRequest, WINHTTP_OPTION_PROXY, &p, sizeof(p)))
                            setProxySucceeded = true;
                    }
                    else
                        setProxySucceeded = true;
                }
                else
                {
                    // ignore the error and try to request directly
                    setProxySucceeded = true;
                }

                // prepare and make the request
                if (setProxySucceeded)
                {
                    StringStream ssHeaders;
                    ssHeaders << TEXT("Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ");
                    ssHeaders << strPostData.length();

                    String strHeaders = ssHeaders.str();
                    char* szPostData = new char[strPostData.length() + 1];
                    strcpy(szPostData, strPostData.c_str());

                    if (WinHttpSendRequest(hRequest, strHeaders.c_str(), (DWORD) strHeaders.length(), szPostData, (DWORD) strPostData.length(), (DWORD) strPostData.length(), NULL))
                    {
                        if (WinHttpReceiveResponse(hRequest, NULL))
                        {
                            char szBuf[BUF_SIZE + 1];
                            DWORD dwBytesRead = 0;

                            while (WinHttpReadData(hRequest, szBuf, BUF_SIZE, &dwBytesRead))
                            {
                                if (dwBytesRead == 0)
                                    break;

                                out << std::string(szBuf, szBuf + dwBytesRead);
                                dwBytesRead = 0;
                                ret = true;
                            }
                        }
                        else
                            App::ShowErrorMessage();
                    }
                    else
                        App::ShowErrorMessage();

                    delete[] szPostData;
                }

                if (ieProxyConfig.lpszProxy != NULL)
                    GlobalFree(ieProxyConfig.lpszProxy);
                if (ieProxyConfig.lpszProxyBypass != NULL)
                    GlobalFree(ieProxyConfig.lpszProxyBypass);
                if (ieProxyConfig.lpszAutoConfigUrl != NULL)
                    GlobalFree(ieProxyConfig.lpszAutoConfigUrl);
                if (proxyInfo.lpszProxy != NULL)
                    GlobalFree(proxyInfo.lpszProxy);
                if (proxyInfo.lpszProxyBypass != NULL)
                    GlobalFree(proxyInfo.lpszProxyBypass);

                WinHttpCloseHandle(hRequest);
            }
            else
                App::ShowErrorMessage();

            WinHttpCloseHandle(hHttp);
        }
        else
            App::ShowErrorMessage();

        WinHttpCloseHandle(hSession);
    }
    else
        App::ShowErrorMessage();

    return ret;
}

void LicenseManagerImpl::OnActivate(bool isSuccess)
{
    if (m_pDemoDlg != NULL)
        m_pDemoDlg->Dismiss(isSuccess ? IDOK : IDCANCEL);
}

void LicenseManagerImpl::OnReceiveDemoTokens(bool isSuccess)
{
    // nothing needs to be done here...
}

String LicenseManagerImpl::DecodeURI(String uri)
{
    DWORD dwSize = (DWORD) uri.length() + 1;
    TCHAR* decodedUri = new TCHAR[dwSize];

    // decode the URI component
    String uriNoPluses = StringReplace(uri, TEXT("+"), TEXT(" "));
    if (UrlUnescape((PTSTR) uriNoPluses.c_str(), decodedUri, &dwSize, 0) == E_POINTER)
    {
        delete[] decodedUri;
        decodedUri = new TCHAR[dwSize + 1];
        UrlUnescape((PTSTR) uriNoPluses.c_str(), decodedUri, &dwSize, 0);
    }

    // convert from UTF8 (we first have to convert to a non-wide string for MultiByteToWideChar to work)
    int nUriLen = (int) _tcslen(decodedUri);
    std::string strDecodedUri(decodedUri, decodedUri + nUriLen);
    int nWcLen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH) strDecodedUri.c_str(), nUriLen, NULL, 0);
    TCHAR* buf = new TCHAR[nWcLen + 1];
    MultiByteToWideChar(CP_UTF8, 0, (LPCCH) strDecodedUri.c_str(), nUriLen, buf, nWcLen);
    buf[nWcLen] = 0;
    String result = String(buf, buf + nWcLen);

    delete[] decodedUri;
    delete[] buf;

    return result;
}

} // namespace Zephyros
