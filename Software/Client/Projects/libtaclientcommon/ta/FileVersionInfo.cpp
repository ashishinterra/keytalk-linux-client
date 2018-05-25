#include "FileVersionInfo.h"
#include "ta/common.h"
#include <stdexcept>
#include <memory>

#ifndef _WIN32
# error "Only Win32 platform is supported"
#endif

#include <windows.h>
#pragma message("automatically link to Version.lib")
#pragma comment(lib, "Version.lib")

using std::string;

namespace ta
{
    static bool getFixedInfo (const LPVOID pData, string& aFileVersion, string& aProductVersion);
    static bool getStringInfo(const LPVOID pData, const string& aKey, string& aVal);

    FileVersionInfo::FileVersionInfo(const string& aPath)
        : thePath(aPath)
    {
        init();
    }

    // throw std::exception on error
    void FileVersionInfo::init()
    {
        DWORD myHandle;
        DWORD mySize = ::GetFileVersionInfoSize(thePath.c_str(), &myHandle);
        if (mySize == 0)
            TA_THROW_MSG(std::exception, boost::format("::GetFileVersionInfoSize failed for file '%s'") % thePath);
        std::auto_ptr<unsigned char> myData(static_cast<unsigned char*>(::operator new (mySize + 1)));
        ::ZeroMemory(myData.get(), mySize+1);
        if (!::GetFileVersionInfo(thePath.c_str(), myHandle, mySize, myData.get()))
            TA_THROW_MSG(std::exception, boost::format("::GetFileVersionInfo failed for file '%s'") % thePath);

        if (!getFixedInfo(myData.get(), theFileVersion, theProductVersion))
            TA_THROW_MSG(std::exception, boost::format("getFixedInfo failed for file '%s'") % thePath);
        if (!getStringInfo(myData.get(), "CompanyName", theCompanyName))
            TA_THROW_MSG(std::exception, boost::format("getFileVersionInfo for CompanyName failed for file '%s'") % thePath);
        if (!getStringInfo(myData.get(),"FileDescription", theFileDescription))
            TA_THROW_MSG(std::exception, boost::format("getFileVersionInfo for FileDescription failed for file '%s'") % thePath);
        if (!getStringInfo(myData.get(),"ProductName", theProductName))
            TA_THROW_MSG(std::exception, boost::format("getFileVersionInfo for ProductName failed for file '%s'") % thePath);
    }

    string FileVersionInfo::getFileVersion() const
    {
        return theFileVersion;
    }

    string FileVersionInfo::getProductVersion() const
    {
        return theProductVersion;
    }

    string FileVersionInfo::getCompanyName() const
    {
        return theCompanyName;
    }

    string FileVersionInfo::getFileDescription() const
    {
        return theFileDescription;
    }

    string FileVersionInfo::getProductName() const
    {
        return theProductName;
    }

    bool getFixedInfo(const LPVOID aData, string& aFileVersion, string& aProductVersion)
    {
        if (!aData)
            return false;

        UINT nLength;
        VS_FIXEDFILEINFO* pFixedInfo = NULL;
        if (!::VerQueryValue(aData, "\\", (void**) &pFixedInfo, &nLength))
            return false;

        aFileVersion = str(boost::format("%d.%d.%d.%d") %
                           HIWORD(pFixedInfo->dwFileVersionMS) %
                           LOWORD(pFixedInfo->dwFileVersionMS) %
                           HIWORD(pFixedInfo->dwFileVersionLS) %
                           LOWORD(pFixedInfo->dwFileVersionLS));
        aProductVersion = str(boost::format("%d.%d.%d.%d")  %
                              HIWORD(pFixedInfo->dwProductVersionMS) %
                              LOWORD(pFixedInfo->dwProductVersionMS) %
                              HIWORD(pFixedInfo->dwProductVersionLS) %
                              LOWORD(pFixedInfo->dwProductVersionLS));
        return true;
    }

    bool getStringInfo(const LPVOID aData, const string& aKey, string& aVal)
    {
        if (!aData)
            return false;

        DWORD* pdwTranslation;
        UINT nLength;
        if (!::VerQueryValue(aData, "\\VarFileInfo\\Translation", (void**) &pdwTranslation, &nLength))
            return false;

        LPTSTR lpszValue;
        char myKey[2000];
        sprintf(myKey, "\\StringFileInfo\\%04x%04x\\%s", LOWORD (*pdwTranslation), HIWORD (*pdwTranslation), aKey.c_str());
        if (!::VerQueryValue(aData, myKey, (void**) &lpszValue, &nLength))
            return false;

        aVal = lpszValue;
        return true;
    }
}
