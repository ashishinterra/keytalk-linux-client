#include "utils.h"
#include "strings.h"
#include "scopedresource.hpp"

#include "boost/format.hpp"
#include "boost/regex.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include <locale>
#include "curl/curl.h"
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <Lmcons.h>
#else
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#endif

using std::string;

namespace ta
{
    // Private API
    namespace
    {

#if defined(__linux__)
        void chownDirContentsRecursively(const string& aDir, const uid_t anOwner)
        {
            if (!isDirExist(aDir))
            {
                return;
            }

            ta::ScopedResource<DIR*> dir(opendir(aDir.c_str()), closedir);
            if (dir)
            {
                // chown all files and subdirectories in dir recursively diving inside subdirectories
                struct dirent* dp;
                while ((dp = readdir(dir)) != NULL)
                {
                    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                    {
                        continue;
                    }
                    const string subpath = aDir + "/" + dp->d_name;
                    if (!isDirExist(subpath) && !isFileExist(subpath))
                    {
                        continue;
                    }
                    if (chown(subpath.c_str(), anOwner, -1) < 0)
                    {
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to set owner of %s to %d. %s") % subpath % anOwner % strerror(errno));
                    }

                    if (isDirExist(subpath))
                    {
                        chownDirContentsRecursively(subpath, anOwner);
                    }
                }
            }
        }
#endif

        size_t responseCallback(void* buffer, size_t size, size_t nmemb, void* aResponse)
        {
            assert(buffer && aResponse);
            string* myReponse = (string*)aResponse;
            size_t myNumBytesConsumed = nmemb*size;
            myReponse->append((char*)buffer, myNumBytesConsumed);
            return myNumBytesConsumed;
        }

        void setupSSL(CURL* aCurl)
        {
            if (!aCurl)
            {
                TA_THROW_MSG(std::invalid_argument, "NULL curl handle");
            }

#ifdef _WIN32
            curl_tlssessioninfo * myTlsSessionInfo = NULL;
            CURLcode myCurlRetCode = curl_easy_getinfo(aCurl, CURLINFO_TLS_SSL_PTR, &myTlsSessionInfo);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve TLS backend information. %s") % curl_easy_strerror(myCurlRetCode));
            }
            if (myTlsSessionInfo->backend == CURLSSLBACKEND_SCHANNEL)
            {
                // disable certificate revocation checks for curl built against WinSSL (schannel)
                // without disabling this flag WinSSL would cut TLS handshake if it does not find CLR or OSCP lists in the server's issuers CAs (which is way too strict I believe)
                myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
                if (myCurlRetCode != CURLE_OK)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to disable CLR option. %s") % curl_easy_strerror(myCurlRetCode));
                }
            }
#endif
        }

    } // unnamed ns


    //
    // Public API
    //

    bool isValidEmail(const string& aEmail)
    {
        string myEmail = boost::trim_copy(aEmail);
        if (myEmail.empty())
            return false;
        try
        {
            boost::regex myRegEx("([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})");
            if (!regex_match(myEmail, myRegEx))
                return false;
            return true;
        }
        catch (std::exception&)
        {
            return false;
        }
    }

    bool isFileExist(const string& aFileName)
    {
        try
        {
            namespace fs = boost::filesystem;
            fs::path myPath(aFileName);
            if (!fs::exists(myPath))
                return false;
            if (!fs::is_regular_file(myPath))
                return false;
        }
        catch (std::exception&)
        {
            return false;
        }
        return true;
    }

    bool isDirExist(const string& aDirName)
    {
        try
        {
            namespace fs = boost::filesystem;
            fs::path myPath(aDirName);
            if (!fs::exists(myPath))
                return false;
            if (!fs::is_directory(myPath))
                return false;
        }
        catch (std::exception&)
        {
            return false;
        }
        return true;
    }

    void createParentDir(const string& aFilePath)
    {
        namespace fs = boost::filesystem;

        const fs::path myPath(aFilePath);
        if (myPath.has_parent_path())
        {
            const fs::path myParentDir = myPath.parent_path();
            if (!fs::exists(myParentDir))
            {
                fs::create_directories(myParentDir);
            }
        }
    }

    string getParentDir(const string& aFilePath)
    {
        namespace fs = boost::filesystem;

        const fs::path myPath(aFilePath);
        if (myPath.has_parent_path())
        {
            return myPath.parent_path().string();
        }
        else
        {
            return ".";
        }
    }

    string getDirSep()
    {
#ifdef _WIN32
        return "\\";
#else
        return "/";
#endif
    }

    void copyDir(const string& aSrcDir, const string& aDestDir)
    {
        namespace fs = boost::filesystem;

        if(!fs::exists(aSrcDir) || !fs::is_directory(aSrcDir))
            TA_THROW_MSG(std::invalid_argument, "Source directory " + aSrcDir + " does not exist or is not a directory.");

        fs::create_directories(aDestDir);

        const fs::path mySourceDirPath(aSrcDir);
        for( fs::directory_iterator it(mySourceDirPath); it != fs::directory_iterator(); ++it)
        {
            fs::path current(it->path());
            if(fs::is_directory(current))
                copyDir(current.string(), aDestDir + getDirSep() + current.filename().string());
            else // file
                fs::copy_file(current.string(), aDestDir + getDirSep() + current.filename().string());
        }
    }

    string regexEscapeStr(const string& anStr)
    {
        static const std::locale& loc = std::locale::classic();
        string myEscapedStr;
        foreach (char c, anStr)
        {
            if (std::isalnum(c, loc))
                myEscapedStr += c;
            else
                myEscapedStr += str(boost::format("\\%c") % c);
        }
        return myEscapedStr;
    }

    string genUuid()
    {
        const std::vector<unsigned char> myRawUuid = genRawUuid();
        return Strings::toHex(getSafeBuf(myRawUuid), myRawUuid.size(), Strings::caseLower);
    }

    std::vector<unsigned char> genRawUuid()
    {
        static boost::uuids::random_generator gen; // seeding entropy might be expensive, so do it once only
        boost::uuids::uuid myUuid = gen();
        return std::vector<unsigned char>(myUuid.begin(), myUuid.end());
    }

    boost::uint32_t genRand(boost::uint32_t anUpperBound)
    {
        if (anUpperBound == 0)
            TA_THROW_MSG(std::invalid_argument, "Upper bound cannot be zero");

#ifdef _WIN32
        HCRYPTPROV myProv = NULL;
        if (!::CryptAcquireContext(&myProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to acquire CryptoAPI provider. Last error: %d") % ::GetLastError());

        // avoiding "modulo bias"; stolen from arc4random_uniform() of OpenBSD-4.6
        boost::uint32_t myMin;
# if (ULONG_MAX > 0xffffffffUL)
        myMin = 0x100000000UL % anUpperBound;
# else
        if (anUpperBound > 0x80000000)
            myMin = 1 + ~anUpperBound;
        else
            myMin = ((0xffffffff - (anUpperBound * 2)) + 1) % anUpperBound;
# endif

        boost::uint32_t myRandom = 0;
        while (true)
        {
            if (!::CryptGenRandom(myProv, sizeof(myRandom), (BYTE*)&myRandom))
            {
                ::CryptReleaseContext(myProv, 0);
                TA_THROW_MSG(std::runtime_error, boost::format("::CryptGenRandom failed. Last error: %d") % ::GetLastError());
            }
            if (myRandom >= myMin)
                break;
        }
        ::CryptReleaseContext(myProv, 0);

        return myRandom % anUpperBound;
#else
        static bool entropySeeded = false;
        if (!entropySeeded)
        {
            srandom((unsigned int)time(NULL));
            entropySeeded = true;
        }
        return random() % anUpperBound;
#endif
    }


    std::vector<unsigned char> genRandBuf(size_t aBufSize)
    {
        if (aBufSize == 0)
        {
            TA_THROW_MSG(std::invalid_argument, "Size of random buffer cannot be zero");
        }

        std::vector<unsigned char> randomDataBuffer(aBufSize, 0);

#ifdef _WIN32
        HCRYPTPROV myProv = NULL;
        if (!::CryptAcquireContext(&myProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT))
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to acquire CryptoAPI provider. Last error: %d") % ::GetLastError());

        if (!::CryptGenRandom(myProv, (DWORD)aBufSize, (BYTE*)getSafeBuf(randomDataBuffer)))
        {
            ::CryptReleaseContext(myProv, 0);
            TA_THROW_MSG(std::runtime_error, boost::format("::CryptGenRandom failed. Last error: %d") % ::GetLastError());
        }

        ::CryptReleaseContext(myProv, 0);

#else
        foreach (unsigned char& ch, randomDataBuffer)
        {
            ch = genRand(UCHAR_MAX);
        }
#endif
        return randomDataBuffer;
    }

    string getUserName()
    {
#ifdef _WIN32
        char myUserName[UNLEN+1] = {};
        DWORD myBufLen = sizeof(myUserName)+1;
        if (!GetUserName(myUserName, &myBufLen))
            return "<unknown>";
        return myUserName;
#else
        struct passwd pwd;
        struct passwd* result;
        char myBuf[16384];
        if (getpwuid_r(getuid(), &pwd, myBuf, sizeof(myBuf), &result) != 0 || !result || !pwd.pw_name)
            return "<unknown>";
        return pwd.pw_name;
#endif
    }

#if defined(__linux__)
    bool isUserRoot()
    {
        return getuid() == 0;
    }

    bool isRebootRequired()
    {
        return isFileExist("/var/run/reboot-required");
    }

    void chownDir(const string& aDir, Recursive aRecursive)
    {
        const uid_t owner = getuid();

        if (!isDirExist(aDir))
        {
            return;
        }
        if (chown(aDir.c_str(), owner, -1) < 0)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to set owner of %s to %d. %s") % aDir % owner % strerror(errno));
        }
        if (aRecursive == recursiveYes)
        {
            chownDirContentsRecursively(aDir, owner);
        }
    }
#endif

    std::vector<unsigned char> fetchHttpUrl(const string& anUrl)
    {
        ta::ScopedResource<CURL*> myCurl(curl_easy_init(), curl_easy_cleanup);
        if (!myCurl)
        {
            TA_THROW_MSG(std::runtime_error, "Error initializing curl");
        }
        CURLcode myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEFUNCTION, responseCallback);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup response callback. %s") % curl_easy_strerror(myCurlRetCode));
        }
        string myResponse;
        myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEDATA, &myResponse);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup cookie for response callback. %s") % curl_easy_strerror(myCurlRetCode));
        }
        myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_URL, anUrl.c_str());
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to set CURLOPTURL curl option to %s. %s") % anUrl % curl_easy_strerror(myCurlRetCode));
        }

        static const unsigned long myConnectTimeoutSeconds = 2;
        myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_CONNECTTIMEOUT, myConnectTimeoutSeconds);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to set CURLOPT_CONNECTTIMEOUT curl option. %s") % curl_easy_strerror(myCurlRetCode));
        }

        // follow HTTP redirects (3xx)
        myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_FOLLOWLOCATION, 1L);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to set CURLOPT_FOLLOWLOCATION curl option. %s") % curl_easy_strerror(myCurlRetCode));
        }


        // set buffer for error messages
        char myExtraErrorMsg[CURL_ERROR_SIZE + 1] = {};
        curl_easy_setopt(myCurl, CURLOPT_ERRORBUFFER, myExtraErrorMsg);
        // setting this is believed to prevent segfaults in curl_resolv_timeout() when DNS lookup times out
        curl_easy_setopt(myCurl, CURLOPT_NOSIGNAL, 1);

        setupSSL(myCurl);

        myCurlRetCode = curl_easy_perform(myCurl);
        if (myCurlRetCode != CURLE_OK)
        {
            string myErrorMsg = "Cannot fetch URL " + anUrl;
            if (myCurlRetCode == CURLE_PEER_FAILED_VERIFICATION || myCurlRetCode == CURLE_SSL_CACERT)
            {
                myErrorMsg += ". Your remote server cannot be trusted by known CA certificates.";
            }
            else if (myCurlRetCode == CURLE_SSL_CONNECT_ERROR)
            {
                myErrorMsg += ". Error establishing secure SSL connection.";
            }
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to fetch URL %s. %s (curl error code %d). Extra error info: %s") % anUrl % curl_easy_strerror(myCurlRetCode) % myCurlRetCode % myExtraErrorMsg);
        }
        long myHttpResponseCode = -1;
        myCurlRetCode = curl_easy_getinfo(myCurl, CURLINFO_RESPONSE_CODE, &myHttpResponseCode);
        if (myCurlRetCode != CURLE_OK)
            TA_THROW_MSG(std::runtime_error, "Cannot get HTTP response code from URL " + anUrl);
        if (myHttpResponseCode == 0)
            TA_THROW_MSG(std::runtime_error, "Cannot connect to " + anUrl);
        if (myHttpResponseCode != 200)
            TA_THROW_MSG(std::runtime_error, boost::format("HTTP %d received when fetching %s") % myHttpResponseCode % anUrl);

        return ta::str2Vec<unsigned char>(myResponse);
    }


}// namespace ta
