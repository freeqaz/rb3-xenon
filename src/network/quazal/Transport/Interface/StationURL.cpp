// Quazal NetZ - .\Transport\Interface\StationURL.cpp
// Retail TU 0x82AA1658..0x82AA6270, built /Od /Ob1 /Oi-.
#include "Plugins/StationURL.h"
#include "Platform/InetAddress.h"
#include "Platform/StringStream.h"

// The CRT headers are not included: the TU uses the C++ char* overload of
// strstr (inline, so its argument is spilled to a temporary at /Ob1).
extern "C" {
unsigned int strlen(const char *);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
void *memcpy(void *, const void *, unsigned int);
char *strstr(const char *, const char *);
int _snprintf(char *, unsigned int, const char *, ...);
int atoi(const char *);
}
extern "C++" inline char *strstr(char *str, const char *substr) {
    return strstr((const char *)str, substr);
}

namespace Quazal {
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);

    namespace StringConversion {
        bool BufferToHexString(const unsigned char *, unsigned int, char *, unsigned int);
        bool HexStringToBuffer(const char *, unsigned char *, unsigned int);
    }

    String s_strStream("stream");
    String s_strStreamID("sid");
    String s_strPrincipalID("PID");
    String s_strConnectionID("CID");
    String s_strType("type");
    String s_strXLSPID("XLSPID");
    String s_strXNAddr("xnaddr");
    String s_strXNKey("xnkey");
    String s_strXNKid("xnkid");
    String s_strRVConnectionID("RVCID");

    StationURL::StationURL() {
        m_szURL = NULL;
        m_bParsed = true;
        m_bFormatted = false;
        m_bValid = true;
        m_pInetAddress = new (__FILE__, 0x7A) InetAddress();
        SetURLType(prudp);
    }

    StationURL::StationURL(const String &url) {
        m_szURL = NULL;
        m_bParsed = false;
        m_bFormatted = true;
        m_bValid = false;
        m_pInetAddress = new (__FILE__, 0x84) InetAddress();
        SetURL(url);
    }

    StationURL::StationURL(const char *url) {
        m_szURL = NULL;
        m_bParsed = false;
        m_bFormatted = true;
        m_bValid = false;
        m_pInetAddress = new (__FILE__, 0x8E) InetAddress();
        SetURL(url);
    }

    StationURL::StationURL(const StationURL &url) {
        m_szURL = NULL;
        m_bParsed = true;
        m_bFormatted = false;
        m_bValid = false;
        m_pInetAddress = new (__FILE__, 0x98) InetAddress();
        Copy(url);
    }

    StationURL &StationURL::operator=(const StationURL &url) {
        Copy(url);
        return *this;
    }

    StationURL &StationURL::operator=(const char *url) {
        SetURL(url);
        return *this;
    }

    void StationURL::Copy(const StationURL &url) {
        if (url.m_bFormatted) {
            SetURL(url.GetURL());
            m_bFormatted = true;
        } else {
            SetURL(NULL);
        }
        if (url.m_bParsed) {
            *m_pInetAddress = *url.m_pInetAddress;
            m_eURLType = url.m_eURLType;
            m_mapUIntParams = url.m_mapUIntParams;
            m_mapParams = url.m_mapParams;
            m_mapOptions = url.m_mapOptions;
            m_bParsed = true;
        }
        m_bValid = url.m_bValid;
    }

    StationURL::~StationURL() {
        SetURL(NULL);
        delete m_pInetAddress;
    }

    void StationURL::ParseIfNeeded() const {
        if (!m_bParsed) {
            const_cast<StationURL *>(this)->Parse();
        }
    }

    void StationURL::FormatIfNeeded() const {
        if (!m_bFormatted) {
            const_cast<StationURL *>(this)->Format();
        }
    }

    void StationURL::Format() {
        const unsigned int uiSize = 0x400;
        int iPos = 0;
        char szURL[uiSize];
        switch (m_eURLType) {
        case prudp:
            iPos = _snprintf(szURL, uiSize, "prudp:/");
            break;
        case prudps:
            iPos = _snprintf(szURL, uiSize, "prudps:/");
            break;
        case udp:
            iPos = _snprintf(szURL, uiSize, "udp:/");
            break;
        }
        bool bSep = false;
        if (m_pInetAddress->GetAddress() != 0) {
            iPos += _snprintf(szURL + iPos, uiSize, "%s%s", "address", "=");
            m_pInetAddress->GetAddress(szURL + iPos, uiSize - iPos);
            iPos += strlen(szURL + iPos);
            bSep = true;
        }
        if (m_pInetAddress->GetPortNumber() != 0) {
            if (bSep) {
                iPos += _snprintf(szURL + iPos, uiSize, ";");
            }
            iPos += _snprintf(
                szURL + iPos, uiSize, "%s%s%d", "port", "=", m_pInetAddress->GetPortNumber()
            );
            bSep = true;
        }
        for (qMap<String, String>::iterator it = m_mapParams.begin(); it != m_mapParams.end();
             ++it) {
            if (bSep) {
                iPos += _snprintf(szURL + iPos, uiSize, ";");
            }
            iPos += _snprintf(
                szURL + iPos,
                uiSize,
                "%s%s%s",
                (const char *)it->first,
                "=",
                (const char *)it->second
            );
            bSep = true;
        }
        for (qMap<String, unsigned int>::iterator it = m_mapUIntParams.begin();
             it != m_mapUIntParams.end();
             ++it) {
            if (bSep) {
                iPos += _snprintf(szURL + iPos, uiSize, ";");
            }
            iPos += _snprintf(
                szURL + iPos, uiSize, "%s%s%d", (const char *)it->first, "=", it->second
            );
            bSep = true;
        }
        if (!m_mapOptions.empty()) {
            iPos += _snprintf(szURL + iPos, uiSize, "#");
            bSep = false;
            for (qMap<String, String>::iterator it = m_mapOptions.begin();
                 it != m_mapOptions.end();
                 ++it) {
                if (bSep) {
                    iPos += _snprintf(szURL + iPos, uiSize, ";");
                }
                iPos += _snprintf(
                    szURL + iPos,
                    uiSize,
                    "%s%s%s",
                    (const char *)it->first,
                    "=",
                    (const char *)it->second
                );
                bSep = true;
            }
        }
        SetURL(szURL);
        m_bValid = true;
        m_bParsed = true;
    }

    void StationURL::SetURLType(_URLType type) {
        ParseIfNeeded();
        m_eURLType = type;
        m_bFormatted = false;
    }

    StationURL::_URLType StationURL::GetURLType() const {
        ParseIfNeeded();
        return m_eURLType;
    }

    bool StationURL::SetAddress(const char *address) {
        ParseIfNeeded();
        m_bFormatted = false;
        return m_pInetAddress->SetAddress(address);
    }

    String StationURL::GetAddress() const {
        char szAddress[0x80];
        szAddress[0] = 0;
        ParseIfNeeded();
        if (m_pInetAddress->GetAddress() != 0) {
            m_pInetAddress->GetAddress(szAddress, 0x80);
        }
        return String(szAddress);
    }

    bool StationURL::SetInetAddress(const InetAddress *address) {
        ParseIfNeeded();
        *m_pInetAddress = *address;
        m_bFormatted = false;
        return true;
    }

    InetAddress *StationURL::GetInetAddress() const {
        ParseIfNeeded();
        return m_pInetAddress;
    }

    void StationURL::SetXboxAddress(const XNADDR *addr, const XNKID *kid, const XNKEY *key) {
        SetXNAddr(addr);
        SetXNKid(kid);
        SetXNKey(key);
    }

    void StationURL::SetXNAddr(const XNADDR *addr) {
        SetBinaryParam(s_strXNAddr, addr, 0x24);
    }

    void StationURL::SetXNKid(const XNKID *kid) { SetBinaryParam(s_strXNKid, kid, 8); }

    void StationURL::SetXNKey(const XNKEY *key) { SetBinaryParam(s_strXNKey, key, 0x10); }

    bool StationURL::GetXNAddr(XNADDR *addr) const {
        return GetBinaryParam(s_strXNAddr, addr, 0x24);
    }

    bool StationURL::GetXNKid(XNKID *kid) const { return GetBinaryParam(s_strXNKid, kid, 8); }

    bool StationURL::GetXNKey(XNKEY *key) const {
        return GetBinaryParam(s_strXNKey, key, 0x10);
    }

    void StationURL::SetPortNumber(unsigned short port) {
        ParseIfNeeded();
        m_pInetAddress->SetPortNumber(port);
        m_bFormatted = false;
    }

    unsigned short StationURL::GetPortNumber() const {
        ParseIfNeeded();
        return m_pInetAddress->GetPortNumber();
    }

    void StationURL::SetStreamType(Stream::Type type) { SetUIntParam(s_strStream, type); }

    Stream::Type StationURL::GetStreamType() const {
        return (Stream::Type)GetUIntParam(s_strStream, 0);
    }

    void StationURL::SetStreamID(unsigned char id) { SetUIntParam(s_strStreamID, id); }

    unsigned char StationURL::GetStreamID() const { return GetUIntParam(s_strStreamID, 0); }

    void StationURL::SetPrincipalID(unsigned int id) { SetUIntParam(s_strPrincipalID, id); }

    unsigned int StationURL::GetPrincipalID() const {
        return GetUIntParam(s_strPrincipalID, 0);
    }

    void StationURL::SetConnectionID(unsigned int id) { SetUIntParam(s_strConnectionID, id); }

    unsigned int StationURL::GetConnectionID() const {
        return GetUIntParam(s_strConnectionID, 0);
    }

    void StationURL::SetRVConnectionID(unsigned int id) {
        SetUIntParam(s_strRVConnectionID, id);
    }

    unsigned int StationURL::GetRVConnectionID() const {
        return GetUIntParam(s_strRVConnectionID, 0);
    }

    void StationURL::SetType(unsigned int type) { SetUIntParam(s_strType, type); }

    unsigned int StationURL::GetType() const { return GetUIntParam(s_strType, 0); }

    void StationURL::SetXLSPID(unsigned int id) { SetUIntParam(s_strXLSPID, id); }

    unsigned int StationURL::GetXLSPID() const { return GetUIntParam(s_strXLSPID, 0); }

    StationURL &StationURL::operator=(const String &url) {
        SetURL(url);
        return *this;
    }

    bool StationURL::operator==(const StationURL &url) const {
        const char *szOther = url.GetURL();
        const char *szThis = GetURL();
        return strcmp(szThis, szOther) == 0;
    }

    bool StationURL::operator!=(const StationURL &url) const {
        const char *szOther = url.GetURL();
        const char *szThis = GetURL();
        return strcmp(szThis, szOther) != 0;
    }

    bool StationURL::SetParam(const String &key, const String &value, bool bOption) {
        ParseIfNeeded();
        if (!bOption) {
            m_mapParams[key] = value;
        } else {
            m_mapOptions[key] = value;
        }
        m_bFormatted = false;
        return true;
    }

    bool StationURL::RemoveParam(const String &key, bool bOption) {
        ParseIfNeeded();
        bool bRemoved;
        if (!bOption) {
            bRemoved = m_mapParams.erase(key) == 1 || m_mapUIntParams.erase(key) == 1;
        } else {
            bRemoved = m_mapOptions.erase(key) == 1;
        }
        if (bRemoved) {
            m_bFormatted = false;
        }
        return bRemoved;
    }

    bool StationURL::GetParam(const String &key, String *value, bool bOption) const {
        bool bFound = false;
        ParseIfNeeded();
        if (!bOption) {
            qMap<String, String>::const_iterator it;
            ParseIfNeeded();
            it = m_mapParams.find(key);
            if (it != m_mapParams.end()) {
                if (value) {
                    *value = it->second;
                }
                bFound = true;
            }
        } else {
            qMap<String, String>::const_iterator it = m_mapOptions.find(key);
            if (it != m_mapOptions.end()) {
                if (value) {
                    *value = it->second;
                }
                bFound = true;
            }
        }
        return bFound;
    }

    void StationURL::SetURL(const char *url) {
        if (m_szURL) {
            qDeleteArray(m_szURL);
            m_szURL = NULL;
            m_bFormatted = false;
        }
        if (url) {
            m_szURL = qNewArray<char>(strlen(url) + 1, __FILE__, 0x2C9);
            strcpy(m_szURL, url);
            m_bFormatted = true;
            m_bParsed = false;
            m_bValid = false;
        }
    }

    const char *StationURL::GetURL() const {
        FormatIfNeeded();
        return m_szURL;
    }

    static qResult CopySubString(
        char *dest, unsigned int destSize, const char *src, unsigned int len
    );

    qResult StationURL::GetURLType(char *szType, unsigned int uiSize) const {
        if (m_szURL == NULL) {
            return qResult(0x8001000C);
        }
        if (szType == NULL) {
            return qResult(0x8001000A);
        }
        char *pSep = strstr(m_szURL, ":/");
        if (pSep == NULL) {
            return qResult(0x80050003);
        }
        if (pSep == m_szURL) {
            return qResult(0x80050003);
        }
        unsigned int i;
        unsigned int uiLength = pSep - m_szURL;
        for (i = 0; i < uiLength; i++) {
            if ((m_szURL[i] < 'A' || m_szURL[i] > 'Z') && (m_szURL[i] < 'a' || m_szURL[i] > 'z')
                && (m_szURL[i] < '0' || m_szURL[i] > '9') && m_szURL[i] != '-'
                && m_szURL[i] != '_') {
                return qResult(0x80050003);
            }
        }
        return CopySubString(szType, uiSize, m_szURL, uiLength);
    }

    qResult StationURL::SetUIntParam(const String &key, unsigned int value) {
        ParseIfNeeded();
        m_mapParams.erase(key);
        m_mapUIntParams[key] = value;
        m_bFormatted = false;
        return qResult(0x10001);
    }

    unsigned int StationURL::GetUIntParam(const String &key, unsigned int uiDefault) const {
        unsigned int uiValue = uiDefault;
        ParseIfNeeded();
        qMap<String, unsigned int>::const_iterator it = m_mapUIntParams.find(key);
        if (it != m_mapUIntParams.end()) {
            uiValue = (*it).second;
        } else {
            qMap<String, String>::const_iterator itStr = m_mapParams.find(key);
            if (itStr != m_mapParams.end()) {
                uiValue = atoi(itStr->second);
                const_cast<StationURL *>(this)->SetUIntParam(key, uiValue);
            }
        }
        return uiValue;
    }

    qResult
    StationURL::SetBinaryParam(const String &key, const void *data, unsigned int size) {
        char szHex[0x100];
        StringConversion::BufferToHexString((const unsigned char *)data, size, szHex, 0x100);
        SetParam(key, szHex, false);
        return qResult(0x10001);
    }

    bool StationURL::GetBinaryParam(const String &key, void *data, unsigned int size) const {
        bool bFound = false;
        qMap<String, String>::const_iterator it;
        ParseIfNeeded();
        it = m_mapParams.find(key);
        if (it != m_mapParams.end()) {
            StringConversion::HexStringToBuffer(it->second, (unsigned char *)data, size);
            bFound = true;
        }
        return bFound;
    }

    unsigned int StationURL::GetNbParams(bool bOption) const {
        if (!IsValid()) {
            return 0;
        }
        unsigned int uiCount;
        if (!bOption) {
            uiCount = m_mapParams.size() + m_mapUIntParams.size();
        } else {
            uiCount = m_mapOptions.size();
        }
        return uiCount;
    }

    bool StationURL::IsValid() const {
        ParseIfNeeded();
        return m_bValid;
    }

    static qResult CopySubString(
        char *dest, unsigned int destSize, const char *src, unsigned int len
    ) {
        if (dest == NULL) {
            return qResult(0x8001000A);
        }
        if (len + 1 > destSize) {
            return qResult(0x8001000F);
        }
        memcpy(dest, src, len);
        dest[len] = 0;
        return qResult(0x10001);
    }

    StationURL::_URLType StationURL::ExtractURLType() const {
        char szType[0x40];
        qResult r = GetURLType(szType, 0x40);
        if (r.Equals(true)) {
            if (strcmp(szType, "prudp") == 0) {
                return prudp;
            } else if (strcmp(szType, "prudps") == 0) {
                return prudps;
            } else if (strcmp(szType, "udp") == 0) {
                return udp;
            }
        }
        return Unknown;
    }

    bool StationURL::ParseParams(const char *szParams, bool bOption) {
        bool bLast = false;
        char *pSep;
        do {
            pSep = strstr(szParams, ";");
            if (pSep == NULL) {
                bLast = true;
                pSep = (char *)szParams + strlen(szParams);
                if (szParams == pSep) {
                    return true;
                }
            }
            if (szParams == pSep) {
                return false;
            }
            *pSep = 0;
            bool bOk = ParseParam(szParams, bOption);
            if (!bLast) {
                *pSep = ';';
            }
            if (!bOk) {
                return false;
            }
            szParams = pSep + 1;
        } while (!bLast);
        return true;
    }

    bool StationURL::ParseParam(const char *szParam, bool bOption) {
        char *pEqual = strstr(szParam, "=");
        if (pEqual == NULL || szParam == pEqual) {
            return false;
        }
        const char *szVal = pEqual + 1;
        if (*szVal == 0 || strstr(szVal, "=") != NULL) {
            return false;
        }
        *pEqual = 0;
        bool bOk = true;
        if (!bOption) {
            if (strcmp(szParam, "address") == 0) {
                bOk = SetAddress(szVal);
            } else if (strcmp(szParam, "port") == 0) {
                SetPortNumber(atoi(szVal));
            } else {
                SetParam(szParam, szVal, bOption);
            }
        } else {
            SetParam(szParam, szVal, bOption);
        }
        *pEqual = '=';
        return bOk;
    }

    void StationURL::Parse() {
        m_bParsed = true;
        m_pInetAddress->Init();
        m_mapUIntParams.clear();
        m_mapParams.clear();
        m_mapOptions.clear();
        m_eURLType = ExtractURLType();
        if (m_eURLType == Unknown) {
            return;
        }
        const char *szParams = strstr(m_szURL, ":/") + strlen(":/");
        char *pHash = strstr(szParams, "#");
        if (pHash) {
            if (strstr(pHash + 1, "#")) {
                return;
            }
            *pHash = 0;
            bool bOk = ParseParams(szParams, false);
            *pHash = '#';
            if (!bOk) {
                return;
            }
            szParams = pHash + 1;
            if (!ParseParams(szParams, true)) {
                return;
            }
        } else if (!ParseParams(szParams, false)) {
            return;
        }
        m_bValid = true;
        m_bFormatted = true;
    }

    void StationURL::Trace(unsigned int) const {}

    bool StationURL::IsSameHost(const StationURL &url) const {
        return GetAddress() == url.GetAddress() && GetPortNumber() == url.GetPortNumber();
    }

    StringStream &operator<<(StringStream &stream, const StationURL &url) {
        return stream << url.GetURL();
    }
}
