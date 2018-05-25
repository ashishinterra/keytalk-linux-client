#include "ReseptConsoleClientDelegates.h"
#include "ReseptConsoleClientCommon.h"
#include "rclient/CRFile.h"
#include "rclient/Common.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "ta/logger.h"

#include <iostream>
#include <stdexcept>
#include "boost/algorithm/string.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/regex.hpp"

#ifndef WIN32
#include <unistd.h>   //getch
#include <termios.h>  //getch
#else
#include <conio.h>  //getch
#endif


using std::string;
using std::vector;

//
// Private helpers
//
namespace
{
    unsigned int sec2Msec(size_t aSecs)
    {
        return static_cast<unsigned int>(1000 * aSecs);
    }

    enum EchoMask
    {
        maskEchoNo, maskEchoYes
    };

#ifndef WIN32
    int getch()
    {
        static int ch = -1, fd = 0;
        struct termios myNew, myOld;
        fd = fileno(stdin);
        tcgetattr(fd, &myOld);
        myNew = myOld;
        myNew.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(fd, TCSANOW, &myNew);
        ch = getchar();
        tcsetattr(fd, TCSANOW, &myOld);
        return ch;
    }
#endif

    string readStdin(EchoMask mask)
    {
        string input;
        char key;

        if(mask==maskEchoNo)
        {
            std::cin >> input;
#ifndef WIN32
            key = getch(); // get cr
#endif
        }
        else
        {
            do
            {
#ifdef WIN32
                key = (char)_getch();
#else
                key = getch();
#endif

                switch (key)
                {
                case 127:
                {
                    if(input.length() > 0)
                    {
                        input.erase(input.length() - 1, 1);
                        //erase the last character in password
                        std::cout << key << " " << key;
                    }
                    break;
                }

                default:
                {
                    if(key > 31 && key < 127)
                    {
                        input.push_back(key);
                        std::cout << "*";
                    }
                    break;
                }
                }// switch

            } while((key != 13) && (key != 10) ); //quit if enter is pressed ( '\r' or eol )

            std::cout << std::endl;
        }
        return input;
    }

    // strip leading and trailing whitespace and replace internal adjucent whitespace with a single space character
    string stripWs(const string& aStr)
    {
        static const boost::regex ex("\\s+");
        return boost::regex_replace(boost::trim_copy(aStr), ex, " ");
    }

} // private helpers

//
// Implementation of callbacks
//

string onPasswordPrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const string& aUserId, void* aCookie)
{
    if (!aCookie)
    {
        TA_THROW_MSG(std::invalid_argument, "Password is required but options cookie is not initialized");
    }

    rclient::ReseptClientApp::Options* options = (rclient::ReseptClientApp::Options*)aCookie;

    if (options->is_interactive)
    {
        if (!aChallenges.empty() )
        {
            foreach (const rclient::ReseptClientApp::StringMap::value_type& challNameVal, aChallenges)
            {
                std::cout << challNameVal.second << " ";
            }
        }
        else
        {
            std::cout <<  "Enter password: " ;
        }

        return readStdin(maskEchoYes);
    }
    else  // non-interactive
    {
        if (options->password_supplied)
        {
            return options->password;
        }
        else if (options->cr_file_supplied)
        {
            rclient::CRFile myCRFile(options->cr_file);
            if (aChallenges.empty()) // initially password challenges list is empty
            {
                const rclient::ReseptClientApp::StringMap myFilter = boost::assign::map_list_of(rclient::crfile::UserKey, aUserId);
                return myCRFile.getKey(rclient::crfile::InitialTokenKey, myFilter );
            }
            else
            {
                const rclient::ReseptClientApp::StringMap myFilter = boost::assign::map_list_of(aChallenges.begin()->first, stripWs(aChallenges.begin()->second));
                return myCRFile.getResponse(rclient::crfile::ResponseKey, aUserId, myFilter );
            }
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, boost::format("Password is required for provider %s, service %s, user %s but it cannot be supplied") % options->provider % options->service % aUserId);
        }
    }
}

string onPincodePrompt(const string& aUserId, void* aCookie)
{
    if (!aCookie)
    {
        TA_THROW_MSG(std::invalid_argument, "Pincode is required for but options cookie is not initialized");
    }

    rclient::ReseptClientApp::Options* options = (rclient::ReseptClientApp::Options*)aCookie;

    if (options->is_interactive)
    {

        std::cout <<  "Enter pincode: " ;
        return readStdin(maskEchoYes);
    }
    else
    {
        if (options->pincode_supplied)
        {
            return options->pincode;
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, boost::format("Pincode is required for provider %s, service %s, user %s but it cannot be supplied") % options->provider % options->service % aUserId);
        }
    }
}

rclient::ReseptClientApp::StringMap onResponsePrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const vector<string>& aResponseNames, const string& aUserId, void* aCookie)
{
    if (!aCookie)
    {
        TA_THROW_MSG(std::invalid_argument, "Response is required for but options cookie is not initialized");
    }
    rclient::ReseptClientApp::Options* options = (rclient::ReseptClientApp::Options*)aCookie;

    if (options->is_interactive)
    {
        rclient::ReseptClientApp::StringMap myResponses;
        foreach (const rclient::ReseptClientApp::StringMap::value_type& challNameVal, aChallenges)
        {
            string myChallengePrompt = challNameVal.first;
            if (!boost::ends_with(myChallengePrompt, ":"))
            {
                myChallengePrompt += ":";
            }
            std::cout << myChallengePrompt << " " << challNameVal.second << std::endl;
        }

        foreach (const string& responseName, aResponseNames)
        {
            std::cout << responseName << ": " ;
            string myAnswer = readStdin(maskEchoYes);
            myResponses[responseName] = myAnswer;
        }

        return myResponses;
    }
    else // non-interactive
    {
        if (options->cr_file_supplied)
        {
            rclient::CRFile myCRFile(options->cr_file);
            rclient::ReseptClientApp::StringMap myResponses;

            foreach (const string& responseName, aResponseNames)
            {
                rclient::ReseptClientApp::StringMap myFilter;
                foreach (const rclient::ReseptClientApp::StringMap::value_type& challNameVal, aChallenges)
                {
                    myFilter [challNameVal.first] = stripWs(challNameVal.second);
                }

                myResponses[responseName] = myCRFile.getResponse(responseName, aUserId, myFilter );
            }
            return myResponses;
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, boost::format("Response is required in non-iteractive mode for provider %s, service %s, user %s but no CR file supplied") % options->provider % options->service % aUserId);
        }
    }
}

bool onChangePasswordPrompt(const string& aMsg, const string& aUserId, bool aReasonPasswordExpired, string& aNewPassword, void* aCookie)
{
    if (!aCookie)
    {
        std::cerr << "Cannot change password for user " << aUserId << " because cookie is not initialized" << std::endl;
        return false;
    }

    rclient::ReseptClientApp::Options* options = (rclient::ReseptClientApp::Options*)aCookie;
    if (options->is_interactive)
    {
        std::cout << aMsg << std::endl;

        if (aReasonPasswordExpired)
        {
            std::cout << "Enter new password: ";
            aNewPassword = readStdin(maskEchoYes);
            return true;
        }

        std::cout << "Do you want to change it? [Y/N]" << std::endl;
        const string myConfirmation = readStdin(maskEchoNo);

        if (boost::iequals(myConfirmation, "y"))
        {
            std::cout << "Enter new password: ";
            aNewPassword = readStdin(maskEchoYes);
            return true;
        }

        return false;
    }
    else // non-interactive
    {
        if (options->new_password_supplied)
        {
            aNewPassword = options->new_password;

            // supplied new password is for single use only
            options->password = options->new_password;
            options->new_password_supplied = false;

            return true;
        }
        return false;
    }
}

void onUserMessages(const std::vector<rclient::ReseptClientApp::UserMessage>& aMessages, void* UNUSED(aCookie))
{
    foreach (const rclient::ReseptClientApp::UserMessage& msg, aMessages)
    {
        std::cout << msg.text << std::endl;
        DEBUGLOG(boost::format("%s Server message from %s: %s") % resept::ProductName % ta::TimeUtils::timestampToLocalStr(msg.utc) % msg.text);
    }
}

void onAuthenticationDelayed(size_t aDelaySecs, void* aCookie)
{
    if (!aCookie)
    {
        std::cerr << "Cookie is not initialized!" << std::endl;
        return;
    }
    rclient::ReseptClientApp::Options* options = (rclient::ReseptClientApp::Options*)aCookie;
    if (options->is_interactive)
    {
        std::cerr << "Invalid credentials. Please wait for " << aDelaySecs << " seconds and try again." << std::endl;
        ta::TimeUtils::sleep(sec2Msec(aDelaySecs));
    }
    else
    {
        std::cerr << rclient::DelayPrefix << aDelaySecs << std::endl;
    }
}

void onAuthenticationUserLocked(void* UNUSED(aCookie))
{
    std::cerr << "User is locked in the server." << std::endl;
}

void onSavePfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword, void* UNUSED(aCookie))
{
    const string myPfxSavePath = ta::Process::getTempDir() + rclient::PfxFileName;
    const string myPfxPassSavePath = ta::Process::getTempDir() + rclient::PfxPassFileName;
    ta::writeData(myPfxSavePath, aPfx);
    ta::writeData(myPfxPassSavePath, aPassword);
    DEBUGLOG(boost::format("Pfx has been saved to %s, private key password has been saved to %s") % myPfxSavePath % myPfxPassSavePath);
}

void onSavePem(const vector<unsigned char>& aCert, const string& aPassword, void* UNUSED(aCookie))
{
    const string myPemSavePath = ta::Process::getTempDir() + rclient::SavedPemName;
    const string myPemPassSavePath = ta::Process::getTempDir() + rclient::SavedPemKeyPasswdName;
    ta::writeData(myPemSavePath, aCert);
    ta::writeData(myPemPassSavePath, aPassword);
    DEBUGLOG(boost::format("PEM has been saved to %s, private key password has been saved to %s") % myPemSavePath % myPemPassSavePath);
}

void onError(const string& anUserErrorMsg, void* UNUSED(aCookie))
{
    if (!anUserErrorMsg.empty())
    {
        std::cerr << anUserErrorMsg << std::endl;
    }
    else
    {
        std::cerr << "Error occurred. See log for more info." << std::endl;
    }
}

void onNotify(const string& aMsg, void* UNUSED(aCookie))
{
    std::cout << aMsg << std::endl;
}

