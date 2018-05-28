#ifndef _WIN32
#error "Only Windows platform is supported"
#endif
#include "registry.h"
#include "common.h"
#include <vector>

namespace ta
{
    namespace Registry
    {
        using std::string;
        using std::vector;

        static string toString(HKEY aBaseKey)
        {
            if (aBaseKey == HKEY_CLASSES_ROOT)
                return "HKEY_CLASSES_ROOT";
            if (aBaseKey == HKEY_CURRENT_USER)
                return "HKEY_CURRENT_USER";
            if (aBaseKey == HKEY_LOCAL_MACHINE)
                return "HKEY_LOCAL_MACHINE";
            if (aBaseKey == HKEY_USERS)
                return "HKEY_USERS";
            if (aBaseKey == HKEY_DYN_DATA)
                return "HKEY_DYN_DATA";
            return "<UNSUPPORTED KEY TYPE>";
        }

        // Exceptions: throw RegistryError on error
        static vector<char> doRead(HKEY aBaseKey, const string& aKey, const string& aValName, DWORD& aValType, bool anIsKey64bit);

        bool isExist(HKEY aBaseKey, const string& aKey, const string& aValName, bool anIsKey64bit)
        {
            HKEY myKeyHandle;
            REGSAM myAccess = KEY_READ;
            if (anIsKey64bit)
                myAccess |= KEY_WOW64_64KEY;
            LONG myErrorCode = ::RegOpenKeyEx(aBaseKey, aKey.c_str(), 0, myAccess, &myKeyHandle);
            if (myErrorCode != ERROR_SUCCESS)
                return false;
            int mySize = 0;
            DWORD myType;
            myErrorCode = ::RegQueryValueEx(myKeyHandle, aValName.c_str(), 0, &myType, 0, (LPDWORD)&mySize);
            ::RegCloseKey(myKeyHandle);
            if (myErrorCode != ERROR_SUCCESS)
                return false;
            return true;
        }

        void read(HKEY aBaseKey, const string& aKey, const string& aValName, string& aValVal, bool anIsKey64bit)
        {
            DWORD myValType;
            vector<char> myRetValVec = doRead(aBaseKey, aKey, aValName, myValType, anIsKey64bit);
            if (myValType != REG_SZ && myValType != REG_EXPAND_SZ)
                TA_THROW_MSG(RegistryError, boost::format("Value type mismatch for %s. Actual type: %d. Expected type: %d or %d (REG_SZ or REG_EXPAND_SZ)") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % myValType % REG_SZ % REG_EXPAND_SZ);
            aValVal = myRetValVec.empty() ? "" : &myRetValVec[0];
        }

        void read(HKEY aBaseKey, const string& aKey, const string& aValName, DWORD& aValVal, bool anIsKey64bit)
        {
            DWORD myValType;
            vector<char> myRetVal = doRead(aBaseKey, aKey, aValName, myValType, anIsKey64bit);
            if (myValType != REG_DWORD)
                TA_THROW_MSG(RegistryError, boost::format("Value type mismatch for %s. Actual type: %d. Expected type: %d (REG_DWORD)") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % myValType % REG_DWORD);
            if (myRetVal.empty())
                TA_THROW_MSG(RegistryError, boost::format("Empty REG_DWORD value for %s ?!") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName));
            aValVal = *((DWORD*)&myRetVal[0]);
        }

        void write(HKEY aBaseKey, const string& aKey, const string& aValName, const string& aValVal, bool anIsKey64bit)
        {
            HKEY myKeyHandle;
            REGSAM myAccess = KEY_QUERY_VALUE | KEY_SET_VALUE;
            if (anIsKey64bit)
                myAccess |= KEY_WOW64_64KEY;
            LONG myErrorCode = ::RegOpenKeyEx(aBaseKey, aKey.c_str(), 0, myAccess, &myKeyHandle);
            if (myErrorCode != ERROR_SUCCESS)
                TA_THROW_MSG(RegistryError, boost::format("Error opening key %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey) % myErrorCode);
            DWORD myValType;
            myErrorCode = ::RegQueryValueEx(myKeyHandle, aValName.c_str(), NULL, &myValType, NULL, NULL);
            if (myErrorCode != ERROR_SUCCESS)
            {
                string myError = str(boost::format("Error querying value type for %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % myErrorCode);
                ::RegCloseKey(myKeyHandle);
                TA_THROW_MSG(RegistryError, myError);
            }
            if (myValType != REG_SZ && myValType != REG_EXPAND_SZ)
            {
                string myError = str(boost::format("Value type for %s is not REG_SZ or REG_EXPAND_SZ") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName));
                ::RegCloseKey(myKeyHandle);
                TA_THROW_MSG(RegistryError, myError);
            }
            myErrorCode = ::RegSetValueEx(myKeyHandle, aValName.c_str(), 0, myValType, (const BYTE*)aValVal.c_str(), (DWORD)aValVal.size());
            if (myErrorCode != ERROR_SUCCESS)
            {
                string myError = str(boost::format("Error setting value of %s to %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % aValVal % myErrorCode);
                ::RegCloseKey(myKeyHandle);
                TA_THROW_MSG(RegistryError, myError);
            }
            ::RegCloseKey(myKeyHandle);
        }

        vector<char> doRead(HKEY aBaseKey, const string& aKey, const string& aValName, DWORD& aValType, bool anIsKey64bit)
        {
            HKEY myKeyHandle;
            REGSAM myAccess = KEY_READ;
            if (anIsKey64bit)
                myAccess |= KEY_WOW64_64KEY;
            LONG myErrorCode = ::RegOpenKeyEx(aBaseKey, aKey.c_str(), 0, myAccess, &myKeyHandle);
            if (myErrorCode != ERROR_SUCCESS)
                TA_THROW_MSG(RegistryError, boost::format("Error opening key %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey) % myErrorCode);
            int mySize = 0;
            myErrorCode = ::RegQueryValueEx(myKeyHandle, aValName.c_str(), NULL, &aValType, NULL, (LPDWORD)&mySize);
            if (myErrorCode != ERROR_SUCCESS)
            {
                string myError = str(boost::format("Error querying value type for %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % myErrorCode);
                ::RegCloseKey(myKeyHandle);
                TA_THROW_MSG(RegistryError, myError);
            }
            std::auto_ptr<char> myVal(static_cast<char*>(::operator new (mySize)));
            myErrorCode = ::RegQueryValueEx(myKeyHandle, aValName.c_str(), NULL, &aValType, (LPBYTE) myVal.get(),(LPDWORD) &mySize);
            if (myErrorCode != ERROR_SUCCESS)
            {
                string myError = str(boost::format("Error querying value %s. Last error is %d") % (toString(aBaseKey)+"\\"+aKey+"\\"+aValName) % myErrorCode);
                ::RegCloseKey(myKeyHandle);
                TA_THROW_MSG(RegistryError, myError);
            }
            vector<char> myRetVal(myVal.get(), myVal.get() + mySize);
            ::RegCloseKey(myKeyHandle);
            return myRetVal;
        }
    }
}
