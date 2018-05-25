#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace rclient
{
    class ReseptClientAppInitError : public std::runtime_error
    {
    public:
        ReseptClientAppInitError(const std::string& aUserError, const std::string& aDeveloperError) :
            std::runtime_error(aDeveloperError), theUserError(aUserError)
        {}
        virtual ~ReseptClientAppInitError() throw() {}

        inline std::string userError() const { return theUserError; }
        inline std::string developerError() const { return what(); }
    private:
        std::string theUserError;
    };


    class ReseptClientApp
    {
    public:
        enum ExitCode
        {
            _FirstExitCode = 0,
            exitSuccess = _FirstExitCode,
            exitError,
            exitAuthDelay,
            exitUserLocked,
            exitUserPasswdExpired,
            _LastExitCode = exitUserPasswdExpired
        };

        struct Options
        {
            Options()
                : is_interactive(false), provider_supplied(false), service_supplied(false), userid_supplied(false),
                  password_supplied(false), new_password_supplied(false), pincode_supplied(false), cr_file_supplied(false)
            {}

            inline void setProvider(const std::string& aProvider) { provider = aProvider; provider_supplied = true; }
            inline void setService(const std::string& aService) { service = aService; service_supplied = true; }
            inline void setUserid(const std::string& aUserid) { userid = aUserid; userid_supplied = true; }
            inline void setPassword(const std::string& aPassword) { password = aPassword; password_supplied = true; }
            inline void setNewPassword(const std::string& aPassword) { new_password = aPassword; new_password_supplied = true; }
            inline void setPincode(const std::string& aPincode) { pincode = aPincode; pincode_supplied = true; }
            inline void setCrFile(const std::string& aCrFile) { cr_file = aCrFile; cr_file_supplied = true; }
            inline void setInteractive() { is_interactive = true; }

            bool is_interactive;
            std::string provider;
            bool provider_supplied;
            std::string service;
            bool service_supplied;
            std::string userid;
            bool userid_supplied;
            std::string password;
            bool password_supplied;
            std::string new_password;
            bool new_password_supplied;
            std::string pincode;
            bool pincode_supplied;
            std::string cr_file;
            bool cr_file_supplied;
        }; // Options

        struct UserMessage
        {
            UserMessage(time_t aUtc, const std::string& aText) : utc(aUtc), text(aText) {}
            time_t utc;
            std::string text;
        };
        typedef std::map<std::string, std::string> StringMap;

        //
        // Callback definitions
        //
        // Errors (e.g. if password cannot by supplied) are indicated by throwing C++ exceptions
        //
        typedef std::string (*OnPasswordPromptCb)(const StringMap& aChallenges, const std::string& aUserId, void* aCookie);
        typedef std::string (*OnPincodePromptCb)(const std::string& aUserId, void* aCookie);
        typedef StringMap (*OnResponsePromptCb)(const StringMap& aChallenges, const std::vector<std::string>& aResponseNames, const std::string& aUserId, void* aCookie);
        // @return whether a new password has been supplied via aNewPassword argument
        typedef bool (*OnChangePasswordPromptCb)(const std::string& aMsg, const std::string& aUserId, bool aReasonPasswordExpired, std::string& aNewPassword, void* aCookie);
        typedef void (*OnUserMessagesCb)(const std::vector<UserMessage>& aMessages, void* aCookie);
        typedef void (*OnAuthenticationDelayedCb)(size_t aDelaySecs, void* aCookie);
        typedef void (*OnAuthenticationUserLockedCb)(void* aCookie);
        typedef void (*OnPfxCb)(const std::vector<unsigned char>& aPfx, const std::string& aPassword, void* aCookie);
        typedef void (*OnPemCb)(const std::vector<unsigned char>& aCert, const std::string& aPassword, void* aCookie);
        typedef void (*OnNotifyCb)(const std::string& aMsg, void* aCookie);
        typedef void (*OnErrorCb)(const std::string& anError, void* aCookie);

        //@throw ReseptClientAppInitError
        ReseptClientApp(const Options& anOptions, void* aCookie = NULL);

        //@nothrow
        ~ReseptClientApp();

        //
        // Request certificate from KeyTalk server
        //
        //@param aOnPasswordPrompt Delegate called to prompt user to enter password
        //@param aOnPincodePromptCb Delegate called to prompt user to enter pincode
        //@param aOnResponsePrompt Delegate called to prompt user to enter responses
        //@param aOnChangePasswordPrompt Delegate called to prompt user to change password
        //@param aOnUserMessages Delegate called after user messages are received and processed by the client
        //@param aOnAuthenticationDelayed Delegate called when the authentication is delayed
        //@param aOnAuthenticationUserLocked Delegate called when the user is locked on the server
        //@param aOnPfx Delegate which is called after Pfx is received from the server and is imported to the cert store
        //@param aOnPem Delegate which is called after PEM is received from the server
        //@param OnNotify Delegate which is called for misc. informational notifications
        //@param aOnError Delegate which called when the error occurred
        //@return exit code
        //@nothrow
        ExitCode requestCertificate(
            OnPasswordPromptCb aOnPasswordPrompt,
            OnPincodePromptCb aOnPincodePrompt,
            OnResponsePromptCb aOnResponsePrompt,
            OnChangePasswordPromptCb aOnChangePasswordPrompt,
            OnUserMessagesCb aOnUserMessages,
            OnAuthenticationDelayedCb aOnAuthenticationDelayed,
            OnAuthenticationUserLockedCb aOnAuthenticationUserLocked,
            OnPfxCb aOnPfx,
            OnPemCb aOnPem,
            OnNotifyCb aOnNotify,
            OnErrorCb aOnError
        );
    private:
        struct ReseptClientAppImpl;
        ReseptClientAppImpl* pImpl;
    };
}
