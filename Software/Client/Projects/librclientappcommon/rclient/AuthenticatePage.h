#pragma once

#include "AuthenticationWizard.h"
#include "rclient/RcdpHandler.h"
#include "rclient/Common.h"
#include "resept/common.h"
#include <QWizard>
#include <map>
#include <string>
#include "boost/utility.hpp"

class QFormLayout;
class QLineEdit;

namespace rclient
{
    class AuthenticatePage : public QWizardPage, boost::noncopyable
    {
        Q_OBJECT
    public:
        AuthenticatePage(CurrentUser* aCurrentUser, AuthenticationWizard* anAuthenticationWizard);
        ~AuthenticatePage();
        std::string getServiceUri() const;
        bool getExecuteSync() const;

    private:
        enum AuthRequirementsLocation
        {
            requestAuthRequirementsFromSvr,
            continueWithKnownAuthRequirements
        };
        void requestAuthRequirements();
        void buildCredentialsPromptUi(const AuthRequirementsLocation anAuthRequirementsLocation);
        void adjustUserIdUi();
        bool selectUser();
        void resetLayout(QLayout* aLayout);
        void adjustKeyboardFocus(QFormLayout* aLayout);
        resept::Credentials readCredentialsFromUi() const;
        static bool validateSuppliedCredentials(const resept::Credentials& aCredentials, std::string& anErrorMsg);

        bool authenticate(const resept::Credentials& Creds);
        void checkForNewMessages();
        void requestCertificate();
        bool checkIfPasswordIsNearExpiration(int aRemainingPasswordValidity);

        QString getPasswordPrompt() const;

        // Overriden QWiazardPage methods
        virtual int nextId() const;
        //@throws std::exception, UserLockedError, AuthCancelledException
        virtual bool handlePasswordExpiring(const resept::Credentials& aSuppliedCredentials,
                                            const resept::PasswordValidity aPasswordValidity);
        virtual bool handlePasswordExpired(const resept::Credentials& aSuppliedCredentials);
        virtual bool handleChangePassword(const resept::Credentials& aSuppliedCredentials);
        virtual void handleCrPhase1Authentication();
        virtual void initializePage();// user comes here by pressing 'next' or the wizard is (re)started
        void initializeLogo();
        virtual bool validatePage(); // user clicks 'next'; return whether the next page is shown()
        virtual void setVisible(bool visible);
        virtual bool isComplete () const;// precondition for user to be able to click 'next'

        bool canGoNext() const;
        ta::StringArrayDict resolveURIs() const;
        ta::StringDict calcDigests() const;
    private slots:
        void customButtonClicked(int button);

    private:
        CurrentUser* theCurrentUser;
        AuthenticationWizard* theAuthenticationWizard;
        typedef std::map<resept::CredentialType, QLineEdit*> CredType2LineEditMap;
        typedef std::map<std::string, QLineEdit*> ResponseName2LineEditMap;
        CredType2LineEditMap theCredType2LineEditMap;
        ResponseName2LineEditMap theResponseName2LineEditMap;
        rclient::AuthRequirements theAuthReqs;
        rclient::AuthResponse theAuthResponse;
        std::auto_ptr<rclient::RcdpHandler> theRcdpClient;
        bool theExecuteSync;
    };
}
