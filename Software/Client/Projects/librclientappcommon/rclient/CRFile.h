#ifndef RCLIENT_CRFILE_H
#define RCLIENT_CRFILE_H


#include "CommonUtils.h"
#include "resept/common.h"
#include "ta/libconfigwrapper.h"

#include <string>

namespace rclient
{
    /**
        Config file storing client-side credentials per service/user.

        The primary usage of this file is to fed it to the client to allow non-interactive
        authentication sessions comprising multiple requests for credentials (such as RADIUS Securid)
        and/or credentials based on challenge sent by the server (CR authentications such as RADIUS AKA/SIM).

        Sample CR file:

    Challenges = (
    {
        User =  "UMTS_2_354162120787078";
        Challenge = (
        {
            Name  = "UMTS AUTN";
            Value  = "01010101010101010101010101010101";
        },
        {
            Name  = "UMTS RANDOM";
            Value  = "101112131415161718191a1b1c1d1e1f";
        }
        );
        Response = (
        {
            Name  = "CK";
            Value  = "04040404040404040404040404040404";
        },
        {
            Name  = "IK";
            Value  = "03030303030303030303030303030303";
        },
        {
            Name  = "RES";
            Value  = "02020202020202020202020202020202";
        }
        );
    },
    {
        User =  "DemoUser";
        Challenge = (
        {
            Name  = "Challenge";
            Value  = "a43bf18c";
        }
        );
        Response = (
        {
            Name  = "Response";
            Value  = "FAB60E96";
        }
        );
     },
     {
        User =  "SecuridNewSystemPinUser";
        InitialToken =  "444444";
        Challenge = (
        {
            Name  = "Password challenge";
            Value  = "Are you prepared to accept a new system-generated PIN [y/n]?";
        }
        );
        Response = (
        {
            Name  = "Response";
            Value  = "y";
        }
        );
    },
    {
        User =  "SecuridNewSystemPinUser";
        InitialToken =  "444444";
        Challenge = (
        {
            Name  = "Password challenge";
            Value  = "Your new PIN is: 123456 Do you accept this [y/n]?";
        }
        );
        Response = (
        {
            Name  = "Response";
            Value  = "y";
        }
        );
    },
    {
        User =  "SecuridNewSystemPinUser";
        InitialToken =  "444444";
        Challenge = (
        {
            Name  = "Password challenge";
            Value  = "Pin Accepted. Wait for the code on your card to change, then enter new PIN and TokenCode Enter PASSCODE:";
        }
        );
        Response = (
        {
            Name  = "Response";
            Value  = "555555";
        }
        );
    }
    );
    */

    namespace crfile
    {
        static const std::string UserKey         = "User";
        static const std::string InitialTokenKey = "InitialToken";
        static const std::string ResponseKey     = "Response";

        static const std::string challengesList = "Challenges";
        static const std::string challengeList  = "Challenge";
        static const std::string responseList   = "Response";
        static const std::string keyname        = "Name";
        static const std::string keyvalue       = "Value";
    }

    class CRFile
    {
    public:
        CRFile(const std::string& aFileName);
        virtual ~CRFile();

        std::string getKey(const std::string& aKeyName, const ta::StringDict& aFilter) const;
        std::string getResponse(const std::string& aKey, const std::string& aUser, const ta::StringDict& aFilter) const;

    private:
        ta::LibConfigWrapper theConfig;

    };
}

#endif // RCLIENT_CRFILE_H
