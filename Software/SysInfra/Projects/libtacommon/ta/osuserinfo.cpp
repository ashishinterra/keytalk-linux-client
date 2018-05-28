#ifdef _WIN32
#include "osuserinfo.h"
#include "ta/common.h"

#include <vector>
#include <stdexcept>
#include <windows.h>

namespace ta
{
    namespace OsUserInfo
    {

        using std::string;
        using std::vector;

        //
        // Internal API
        //
        namespace
        {
            // Exceptions: throw std::runtime_error on error
            static void getTokenInfo(HANDLE aTokenHandle, TOKEN_INFORMATION_CLASS aTokenInfoClass, vector<unsigned char>& aTokenInfo)
            {
                DWORD myLen = 0;
                ::GetTokenInformation(aTokenHandle, aTokenInfoClass,  NULL, 0, &myLen ) ;
                if (myLen == 0)
                    TA_THROW_MSG(std::runtime_error, "GetTokenInformation returned zero token length");
                aTokenInfo.resize(myLen);
                ::GetTokenInformation(aTokenHandle, aTokenInfoClass, ta::getSafeBuf(aTokenInfo), myLen, &myLen );
            }

            // Exceptions: throw std::runtime_error on error
            static string getSid(vector<unsigned char>& aTokenInfo)
            {
                if (aTokenInfo.empty())
                    TA_THROW_MSG(std::runtime_error, "aTokenInfo is empty");
                PSID mySidPtr = ((TOKEN_USER*)&aTokenInfo[0])->User.Sid;
                if (!mySidPtr )
                    TA_THROW_MSG(std::runtime_error, "SID is NULL");
                const int mySubAuthCount = *::GetSidSubAuthorityCount(mySidPtr) ;
                string mySid = str(boost::format("S-1-%u") % (unsigned int)(::GetSidIdentifierAuthority(mySidPtr)->Value[5]));
                for (int i = 0; i < mySubAuthCount; ++i )
                    mySid += str(boost::format("-%u") % (*::GetSidSubAuthority(mySidPtr, i)));
                return mySid;
            }
        }


        //
        // Public API
        //

        string getCurentUserSID()
        {
            HANDLE myTokenHandle;
            if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,&myTokenHandle))
                TA_THROW_MSG(std::runtime_error, "::OpenProcessToken");
            vector<unsigned char> myTokenInfo;
            getTokenInfo(myTokenHandle, TokenUser, myTokenInfo);
            string mySid = getSid(myTokenInfo);
            return mySid;
        }
    }
}
#endif
