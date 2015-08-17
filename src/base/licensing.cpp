#ifdef OS_WIN
#include <tchar.h>
#endif

#include "base/zephyros_impl.h"
#include "base/zephyros_strings.h"
#include "base/app.h"
#include "base/licensing.h"

#include "native_extensions/network_util.h"
#include "native_extensions/os_util.h"


#define DIE(msg) { Zephyros::App::Log(msg); Zephyros::App::Quit(); }


namespace Zephyros {

#ifndef APPSTORE

//////////////////////////////////////////////////////////////////////////
// LicenseData Implementation

char LicenseData::gheim[] = "zM#>Vj|wx+RG>)t/a@'Y>Pg^VmPK$Tq<<RWLz8.H2~8\\#m!s+;]Pk%vQoiVH7ILTAl:h~Bi9~8%c*aHK/O2JS2X&f%Z%B~&@8-W-C.V[HrrgvdE~70l?jxv*1^')t@1\"";

String LicenseData::Encrypt(String s)
{
    StringStream ss;
    
    int j = 0;
    for (String::iterator it = s.begin(); it != s.end(); ++it, ++j)
        ss << (((int) (*it)) ^ LicenseData::gheim[j % 128]) << TEXT('.');
    
    return ss.str();
}

String LicenseData::Decrypt(String s)
{
    StringStream ss;
    StringStream ssChar;
    
    int j = 0;
    for (String::iterator it = s.begin(); it != s.end(); ++it)
    {
        if (*it == TEXT('.'))
        {
            ss << (TCHAR) (_ttoi(ssChar.str().c_str()) ^ LicenseData::gheim[j % 128]);
            ssChar.str(String());
            ++j;
        }
        else
            ssChar << *it;
        
    }
    
    return ss.str();
}


//////////////////////////////////////////////////////////////////////////
// LicenseManager Implementation
    
String LicenseManager::GetDemoButtonCaption()
{
	bool isDemoStarted = m_pLicenseData->m_timestampLastDemoTokenUsed != TEXT("");
	if (!isDemoStarted)
		return Zephyros::GetString(ZS_DEMODLG_START_DEMO);
	return GetNumDaysLeft() > 0 ? Zephyros::GetString(ZS_DEMODLG_CONTINUE_DEMO) : Zephyros::GetString(ZS_DEMODLG_CLOSE);
}

String LicenseManager::GetDaysCountLabelText()
{
	int numDaysLeft = GetNumDaysLeft();
	
	if (numDaysLeft > 1)
	{
		StringStream ss;
        ss << numDaysLeft << TEXT(" ") << Zephyros::GetString(ZS_DEMODLG_DAYS_LEFT);
		return ss.str();
	}

	if (numDaysLeft == 1)
		return Zephyros::GetString(ZS_DEMODLG_1DAY_LEFT);

	return Zephyros::GetString(ZS_DEMODLG_NO_DAYS_LEFT);
}

bool LicenseManager::ParseResponse(String url, std::string data, String errorMsg, picojson::object& result)
{
	String title = Zephyros::GetString(ZS_LIC_LICERROR);
    String msg = errorMsg;

	std::stringstream out;
	if (!SendRequest(url, data, out))
    {
        msg.append(Zephyros::GetString(ZS_LIC_CONNECTION_FAILED));
        App::Alert(title, msg, App::AlertStyle::AlertError);
        return false;
    }
    
    picojson::value value;
	std::string err = picojson::parse(value, out);
	if (err.length() > 0)
	{
		msg.append(Zephyros::GetString(ZS_LIC_PARSE_ERROR));
		msg.append(err.begin(), err.end());
		App::Alert(title, msg, App::AlertStyle::AlertError);
		return false;
	}

	if (value.is<picojson::object>())
	{
		picojson::object objResponse = value.get<picojson::object>();
    
		if (objResponse.find("status") != objResponse.end() && objResponse["status"].get<std::string>() == "success")
		{
			result = objResponse;
			return true;
		}
    
		std::string respMsg = objResponse.find("message") != objResponse.end() ? objResponse["message"].get<std::string>() : "";
		msg.append(respMsg.begin(), respMsg.end());
    
	    App::Alert(title, msg, App::AlertStyle::AlertError);
		return false;
	}

	msg.append(Zephyros::GetString(ZS_LIC_UNEXPECTED_RESPONSE));
	std::string resp = out.str();
	msg.append(resp.begin(), resp.end());
	App::Alert(title, msg, App::AlertStyle::AlertError);
	return false;
}

/**
 * Activate the application.
 * If the activation cannot proceed, ACTIVATION_FAILED is returned. If the request to the
 * server is made, the method returns ACTIVATION_SUCCEEDED.
 * If an obsolete license was entered, the method returns ACTIVATION_OBSOLETELICENSE.
 */
int LicenseManager::Activate(String name, String company, String licenseKey)
{
    String title = Zephyros::GetString(ZS_LIC_ACTIVATION);

	// verify that the entered data is OK
	StringStream ssInfo;
	ssInfo << name << TEXT("|") << company << TEXT("|") << m_config.currentLicenseInfo.productId;
	if (!VerifyKey(licenseKey, ssInfo.str(), m_config.currentLicenseInfo.pubkey))
	{
        // check if the user entered an obsolete license;
        // in case it's an obsolete license, don't alert user - notification about upgrade
        // is handled via separate dialog

        bool isObsoleteLicense = false;
        for (LicenseInfo info : m_config.prevLicenseInfo)
        {
            StringStream ssInfoPrevVersion;
            ssInfoPrevVersion << name << TEXT("|") << company << TEXT("|") << info.productId;
            if (!VerifyKey(licenseKey, ssInfoPrevVersion.str(), info.pubkey))
            {
                isObsoleteLicense = true;
                break;
            }
        }
        
        if (!isObsoleteLicense)
        {
            App::Alert(title, Zephyros::GetString(ZS_LIC_LICINVALID), App::AlertStyle::AlertError);
            return ACTIVATION_FAILED;
        }
        
		return ACTIVATION_OBSOLETELICENSE;
	}
    
	// add the POST data
	std::stringstream ssData;
	String strMACAddr = NetworkUtil::GetPrimaryMACAddress();
    String strOS = OSUtil::GetOSVersion();
	ssData <<
		"license=" << std::string(licenseKey.begin(), licenseKey.end()) <<
		"&eth0=" << std::string(strMACAddr.begin(), strMACAddr.end()) <<
		"&productcode=" << m_config.currentLicenseInfo.productId <<
        "&os=" << std::string(strOS.begin(), strOS.end());
	std::string strData = ssData.str();

	App::BeginWait();
    picojson::object result;

	String msg = Zephyros::GetString(ZS_LIC_ACTIVATION_ERROR);
	bool ret = ParseResponse(m_config.activationURL, strData, msg, result);
    App::EndWait();

    if (ret)
    {
        std::string cookie = result["activation"].get<std::string>();
		m_pLicenseData->m_activationCookie = String(cookie.begin(), cookie.end());
        m_pLicenseData->m_licenseKey = licenseKey;
        m_pLicenseData->m_name = name;
        m_pLicenseData->m_company = company;
		m_pLicenseData->Save();
        
        App::Alert(title, Zephyros::GetString(ZS_LIC_ACTIVATION_SUCCESS), App::AlertStyle::AlertInfo);
    }

    m_canStartApp = ret;
    OnActivate(ret);

	return ACTIVATION_SUCCEEDED;
}

int LicenseManager::ActivateFromURL(String url)
{
    if (!AbstractLicenseManager::IsLicensingLink(url))
		return ACTIVATION_FAILED;
        
    StringStream ssUrl(url.c_str() + _tcslen(m_config.activationLinkPrefix));
    std::vector<String> parts;
    while (!ssUrl.eof())
    {
        // split the string at slashes
        String strPart;
        std::getline(ssUrl, strPart, TEXT('/'));
        parts.push_back(strPart);
    }
    
    if (parts.size() != 3)
        return ACTIVATION_FAILED;
    
    return Activate(DecodeURI(parts[0]), DecodeURI(parts[1]), DecodeURI(parts[2]));
}


bool LicenseManager::Deactivate()
{
    if (!m_config.deactivationURL || m_pLicenseData->m_activationCookie.length() == 0 || m_pLicenseData->m_licenseKey.length() == 0)
        return false;
    
    std::stringstream ssData;
    String strMACAddr = NetworkUtil::GetPrimaryMACAddress();
    ssData <<
        "license=" << std::string(m_pLicenseData->m_licenseKey.begin(), m_pLicenseData->m_licenseKey.end()) <<
        "&eth0=" << std::string(strMACAddr.begin(), strMACAddr.end()) <<
        "&productcode=" << m_config.currentLicenseInfo.productId;
    std::string strData = ssData.str();

    picojson::object result;
    bool ret = ParseResponse(m_config.deactivationURL, strData, Zephyros::GetString(ZS_LIC_DEACTIVATION_ERROR), result);
    
    if (ret)
    {
        // write new (empty) license data
        m_pLicenseData->m_activationCookie = TEXT("");
        m_pLicenseData->m_licenseKey = TEXT("");
        m_pLicenseData->m_name = TEXT("");
        m_pLicenseData->m_company = TEXT("");
        m_pLicenseData->Save();

        // show information message
        App::Alert(Zephyros::GetString(ZS_LIC_DEACTIVATION), Zephyros::GetString(ZS_LIC_DEACTIVATION_SUCCESS), App::AlertStyle::AlertInfo);
        App::Quit();
    }
    
    return ret;
}
    
/**
 * Starts the license manager.
 */
void LicenseManager::Start()
{
    // check the configuration
    if (m_config.demoTokensURL == NULL || m_config.activationURL == NULL)
        DIE(TEXT("You must call LicenseManager::SetAPIURLs before you can use the license manager."));
    if (m_config.currentLicenseInfo.pubkey == NULL)
        DIE(TEXT("The public key must not be NULL. Please set it using LicenseManager::SetLicenseInfo."));
    if (m_config.licenseInfoFilename == NULL)
        DIE(TEXT("The license info filename must not be NULL. Please set it using LicenseManager::SetLicenseInfoFilename."));
        
    // create and read the license data
    if (m_pLicenseData == NULL)
        m_pLicenseData = new LicenseData(m_config.licenseInfoFilename);
        
    m_canStartApp = false;
        
#ifdef OS_WIN
    m_lastDemoValidityCheck = 0;
#endif
        
    if (!IsActivated())
        ShowDemoDialog();
    else
        m_canStartApp = true;
}

/**
 * Request tokens to start the demo.
 * If requesting demo tokens cannot proceed, false is returned.
 * The method returns true if the request to the server is made.
 */
bool LicenseManager::RequestDemoTokens(String strMACAddr)
{
	// add the POST data
	std::stringstream ssData;
	if  (strMACAddr.length() == 0)
		strMACAddr = NetworkUtil::GetPrimaryMACAddress();
	String strOS = OSUtil::GetOSVersion();
	ssData <<
		"eth0=" << std::string(strMACAddr.begin(), strMACAddr.end()) <<
		"&productcode=" << m_config.currentLicenseInfo.productId <<
		"&os=" << std::string(strOS.begin(), strOS.end());
	std::string strData = ssData.str();

	App::BeginWait();
    picojson::object result;
    bool ret = ParseResponse(m_config.demoTokensURL, strData, Zephyros::GetString(ZS_LIC_DEMO_ERROR), result);
    App::EndWait();
    
    if (ret)
    {
        // the response is a JSON object of the format { "status": <string>, "tokens": <array<string>> }
        if (result.find("tokens") != result.end())
		{
			picojson::array tokens = result["tokens"].get<picojson::array>();
			for (picojson::value token : tokens)
			{
				std::string strToken = token.get<std::string>();
				m_pLicenseData->m_demoTokens.push_back(String(strToken.begin(), strToken.end()));
			}
		}
        
		m_pLicenseData->Save();
        
		bool canContinueDemo = ContinueDemo();
		if (!canContinueDemo)
			m_canStartApp = false;
		OnReceiveDemoTokens(canContinueDemo);
    }
    else
        m_canStartApp = false;
    
	return ret;
}

#endif
    
} // namespace Zephyros
