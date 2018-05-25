#include "AuthenticatePage.h"
#include "AboutDialog.h"
#include "AuthDelayedMessageBox.h"
#include "TimedNotificationBox.h"
#include "ChangePasswordDialog.h"
#include "WaitDialog.h"
#include "CommonUtils.h"
#include "ConfigUsersDialog.h"
#include "rclient/Settings.h"
#include "rclient/RcdpRequest.h"
#include "rclient/RcdpHandler.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "resept/common.h"
#include "resept/computeruuid.h"
#include "ta/sysinfo.h"
#include "ta/logger.h"
#include "ta/url.h"
#include "ta/process.h"
#include "ta/hashutils.h"
#include "ta/dnsutils.h"
#include "ta/utils.h"
#include "ta/timeutils.h"
#include "ta/assert.h"

#include "boost/bind.hpp"
#include <vector>
#include <QtWidgets>
#include <Qtimer>

static const int CertificateNotificationDelaySec = 3;

using std::string;
using std::vector;

namespace
{
    static const QWizard::WizardButton AboutBtn = QWizard::CustomButton1;
    static const QWizard::WizardOption HaveAboutBtn = QWizard::HaveCustomButton1;
    static const QWizard::WizardButton ServiceUriBtn = QWizard::CustomButton2;
    static const QWizard::WizardOption HaveServiceUriBtn = QWizard::HaveCustomButton2;

    //@nothrow
    string calcHwsig(const string& aFormula)
    {
        string myParsedFormula;
        const string myHwSig = resept::ComputerUuid::calcCs(aFormula, &myParsedFormula);
        DEBUGLOG(boost::format("Calculated HWSIG: %s (parsed formula: %s)") % myHwSig % myParsedFormula);
        return myHwSig;
    }

    bool promptForChangePassword(const string& aUsername, string& aPasswd, string& aNewPasswd, QWidget* parent)
    {
        FUNCLOG;

        ChangePasswordDialog myChangePasswordDialog(aUsername, aPasswd, parent);
        if (myChangePasswordDialog.exec() == QDialog::Accepted)
        {
            aPasswd = myChangePasswordDialog.getEnteredPassword();
            aNewPasswd = myChangePasswordDialog.getEnteredPasswordNew();
            return true;
        }
        DEBUGLOG("Password dialog cancelled");
        return false;
    }

}

namespace rclient
{
    AuthenticatePage::AuthenticatePage(CurrentUser* aCurrentUser, AuthenticationWizard* anAuthenticationWizard)
        : QWizardPage(anAuthenticationWizard)
        , theCurrentUser(aCurrentUser)
        , theAuthenticationWizard(anAuthenticationWizard)
        , theExecuteSync(false)
    {
        if (!aCurrentUser)
        {
            TA_THROW_MSG(std::invalid_argument, "CurrentUser is NULL");
        }
        if (!theAuthenticationWizard)
        {
            TA_THROW_MSG(std::invalid_argument, "AuthenticationWizard is NULL");
        }
    }

    AuthenticatePage::~AuthenticatePage()
    {
        try
        {
            if (theRcdpClient.get() && theRcdpClient->userSessionData().rcdpState != resept::rcdpv2::stateClosed)
            {
                theRcdpClient->eoc();
            }
        }
        catch (std::exception& e)
        {
            WARNLOG2("Failed to close RCDP connection with the server.", boost::format("Failed to close RCDP connection with the server. %s") % e.what());
        }
        catch (...)
        {
            WARNLOG("Failed to close RCDP connection with the server.");
        }
    }

    string AuthenticatePage::getServiceUri() const
    {
        return theAuthReqs.service_uris.empty() ? "" : theAuthReqs.service_uris[0];
    }
    bool AuthenticatePage::getExecuteSync() const
    {
        return theExecuteSync;
    }

    int AuthenticatePage::nextId() const
    {
        return -1;
    }

    void AuthenticatePage::initializeLogo()
    {
        static const unsigned int LogoImageWidth = 55;
        static const unsigned int LogoImageHeight = 55;

        string myProviderLogoPath = rclient::Settings::getProviderInstallDir() + ta::getDirSep() + rclient::LogoV20ImageName;
        if (!ta::isFileExist(myProviderLogoPath))
        {
            // fallback to v1.1 logo
            myProviderLogoPath = rclient::Settings::getProviderInstallDir() + ta::getDirSep() + rclient::LogoV11ImageName;
        }
        if (ta::isFileExist(myProviderLogoPath))
        {
            theAuthenticationWizard->setPixmap(QWizard::LogoPixmap, QPixmap(myProviderLogoPath.c_str()).scaled(LogoImageWidth, LogoImageHeight));
            theAuthenticationWizard->setWindowIcon(QIcon(myProviderLogoPath.c_str()));
            return;
        }

        // Fallback to legacy v1.0 bmp logo and icon
        myProviderLogoPath = rclient::Settings::getProviderInstallDir() + ta::getDirSep() + rclient::LogoV10ImageName;
        if (ta::isFileExist(myProviderLogoPath))
        {
            theAuthenticationWizard->setPixmap(QWizard::LogoPixmap, QPixmap(myProviderLogoPath.c_str()).scaled(LogoImageWidth, LogoImageHeight));
        }
        else
        {
            theAuthenticationWizard->setPixmap(QWizard::LogoPixmap, QPixmap(":/RClientAppCommon/logo.png"));// fallback to default logo
        }
        const string myProviderIconPath = rclient::Settings::getProviderInstallDir() + ta::getDirSep() + rclient::IconV10ImageName;
        if (ta::isFileExist(myProviderIconPath))
        {
            theAuthenticationWizard->setWindowIcon(QIcon(myProviderIconPath.c_str()));
        }
        else
        {
            theAuthenticationWizard->setWindowIcon(QIcon(":/RClientAppCommon/logo.png"));// fallback to default icon
        }
    }

    void AuthenticatePage::initializePage()
    {
        //@note it is not recommended to call dialogs from this method since Qt will not be able to repaint neither this page (not yet ready) nor any previous wizard page, if any (already destroyed)

        // Title
        setTitle("Authenticate");
        setSubTitle("Please supply credentials for the selected provider and service");

        initializeLogo();
        buildCredentialsPromptUi(requestAuthRequirementsFromSvr);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Fix for #278 (Client skips authentication dialog when more than one user exists)
        QTimer::singleShot(0, this, SLOT(repaint()));
        QTimer::singleShot(10, this, SLOT(repaint()));
        QTimer::singleShot(100, this, SLOT(repaint()));
        ////////////////////////////////////////////////////////////////////////////////////////////
    }


    bool AuthenticatePage::handlePasswordExpiring(const resept::Credentials& aSuppliedCredentials, const resept::PasswordValidity aPasswordValidity)
    {
        const unsigned int myDays = 1 + ((unsigned int)aPasswordValidity.validity / ta::TimeUtils::SecondsInDay);
        string passwordExpiringMsg = boost::str(boost::format("Your password expires within %u %s, do you want to change it now?") % myDays % (myDays != 1 ? "days" : "day"));

        QMessageBox::StandardButton msgResult = QMessageBox::warning(this,
                                                "Password expiration warning",
                                                passwordExpiringMsg.c_str(),
                                                QMessageBox::Yes | QMessageBox::No);

        if (msgResult == QMessageBox::Yes)
        {
            return handleChangePassword(aSuppliedCredentials);
        }
        return false;
    }


    bool AuthenticatePage::handlePasswordExpired(const resept::Credentials& aSuppliedCredentials)
    {
        QMessageBox::StandardButton msgResult = QMessageBox::warning(this,
                                                "Password expiration warning",
                                                "Your password has expired and must be changed.",
                                                QMessageBox::Ok | QMessageBox::Cancel);

        if (msgResult == QMessageBox::Ok)
        {
            return handleChangePassword(aSuppliedCredentials);
        }
        return false;
    }


    bool AuthenticatePage::handleChangePassword(const resept::Credentials& aSuppliedCredentials)
    {
        const string userId = getCredentialValue(aSuppliedCredentials, resept::credUserId, "the supplied credentials for password change");
        const string passwd = getCredentialValue(aSuppliedCredentials, resept::credPasswd, "the supplied credentials for password change");

        while (true)
        {
            string oldPassword = passwd;
            string newPassword;

            if (!promptForChangePassword(userId, oldPassword, newPassword, this))
            {
                return false;
            }

            const rclient::AuthResponse pwdChangeResult = theRcdpClient->changePassword(oldPassword, newPassword);

            if (pwdChangeResult.auth_result.type == resept::AuthResult::Ok)
            {
                QMessageBox::information(this, "Password successfully changed", "Password has been successfully changed.", QMessageBox::Ok);
                return true;
            }
            else if (pwdChangeResult.auth_result.type == resept::AuthResult::Delay)
            {
                const string msg = "Failed to change the password. \n\n" \
                                   "Please check that you typed the passwords correctly. Please also make sure the password satisfies password policy.\n\n";
                if (!AuthDelayedMessageBox::show(this, msg, true, pwdChangeResult.auth_result.delay))
                {
                    throw AuthCancelledException();
                }
                continue;
            }
            else if (pwdChangeResult.auth_result.type == resept::AuthResult::Locked)
            {
                const string msg = "This account is locked. Please contact " + resept::ProductName + " administrator.";
                QMessageBox::information(this, "Account locked", msg.c_str(), QMessageBox::Ok);
                return false;
            }
            else
            {
                WARNLOG("Got unexpected password change result " + str(pwdChangeResult.auth_result.type) + " for user " + userId);
                return false;
            }
        }
    }

    void AuthenticatePage::handleCrPhase1Authentication()
    {
        if (!ta::isElemExist(resept::credResponse, theAuthReqs.cred_types))
        {
            // not CR
            return;
        }

        while (true)
        {
            resept::Credentials myCreds;
            foreach(const resept::CredentialType cred_type, theAuthReqs.cred_types)
            {
                if (cred_type == resept::credUserId)
                {
                    myCreds.push_back(resept::Credential(resept::credUserId, theCurrentUser->name()));
                }
                if (cred_type == resept::credHwSig)
                {
                    const string myHwsig = calcHwsig(theAuthReqs.hwsig_formula);
                    myCreds.push_back(resept::Credential(resept::credHwSig, myHwsig));
                }
            }

            const string myProvider = rclient::Settings::getLatestProvider();
            const string myService = rclient::Settings::getLatestService();
            const ta::StringArrayDict myResolvedURIs = resolveURIs();
            const ta::StringDict myCalculatedDigests = calcDigests();
            string myUserId;
            if (!findCredential(myCreds, resept::credUserId, myUserId))
            {
                myUserId = "<not defined>";
            }

            theAuthResponse = theRcdpClient->authenticate(myService, myCreds, myResolvedURIs, myCalculatedDigests);

            switch (theAuthResponse.auth_result.type)
            {
            case resept::AuthResult::Challenge:
            {
                return;
            }
            case resept::AuthResult::Locked:
            {
                throw UserLockedError();
            }
            case resept::AuthResult::Delay:
            {
                WARNLOG(boost::format("User %s is delayed for %d seconds during phase 1 CR authentication for provider %s, service %s") % myUserId % theAuthResponse.auth_result.delay % myProvider % myService);
                if (AuthDelayedMessageBox::show(this, theAuthResponse.auth_result.delay))
                {
                    selectUser();
                    continue;
                }
                else
                {
                    throw AuthCancelledException();
                }
            }
            default:
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Unsupported auth result %s received for phase 1 CR authentication for provider %s, service %s and user %s") % str(theAuthResponse.auth_result.type) % myProvider % myService % myUserId);
            }
            }// switch
        }// while (true)
    }


    bool AuthenticatePage::validatePage()
    {
        FUNCLOG;

        TA_ASSERT(canGoNext());

        const resept::Credentials mySuppliedCredentials = readCredentialsFromUi();
        string myErrorMsg;
        if (!validateSuppliedCredentials(mySuppliedCredentials, myErrorMsg))
        {
            QMessageBox::warning(this, "Invalid credentials", myErrorMsg.c_str());
            buildCredentialsPromptUi(requestAuthRequirementsFromSvr);
            return false;
        }

        // Authenticating...
        if (!authenticate(mySuppliedCredentials))
        {
            if (theAuthResponse.auth_result.passwordValidity.status == resept::PasswordValidity::expired)
            {
                handlePasswordExpired(mySuppliedCredentials);
                initializePage();
            }
            return false;
        }

        // Authenticated, check for new messages and password expiration
        checkForNewMessages();
        if (theAuthResponse.auth_result.passwordValidity.status == resept::PasswordValidity::notExpired)
        {
            if (ta::SysInfo::isUserPasswordExpiring(theAuthResponse.auth_result.passwordValidity.validity))
            {
                const string passwordExpiringMsg = boost::str(boost::format("Password is expiring in %s") % ta::TimeUtils::formatTimeInterval((unsigned int)theAuthResponse.auth_result.passwordValidity.validity));
                WARNLOG(passwordExpiringMsg);

                const bool passwordChanged = handlePasswordExpiring(mySuppliedCredentials, theAuthResponse.auth_result.passwordValidity);
                if (passwordChanged)
                {
                    initializePage();
                    return false;
                }
            }
        }

        // Request cert and finish
        requestCertificate();

        TimedNotificationBox::show(this, CertificateNotificationDelaySec, "Authenticated successfully", "Authenticated successfully.");
        return true;
    }

    void AuthenticatePage::setVisible(bool visible)
    {
        QWizardPage::setVisible(visible);

        if (wizard() != NULL)
        {
            if (visible)
            {
                wizard()->setButtonText(AboutBtn, "");
                wizard()->setButtonText(ServiceUriBtn, "&Users");
                wizard()->setOption(HaveAboutBtn, true);
                wizard()->setOption(HaveServiceUriBtn, true);

                const QIcon aboutIcon(":/RClientAppCommon/info_ico.png");
                wizard()->button(AboutBtn)->setIcon(aboutIcon);

                connect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));
            }
            else
            {
                wizard()->setOption(HaveAboutBtn, false);
                wizard()->setOption(HaveServiceUriBtn, false);

                disconnect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));
            }
        }
    }


    bool AuthenticatePage::selectUser()
    {
        const string mySelectedProvider = rclient::Settings::getLatestProvider();
        const string mySelectedService = rclient::Settings::getLatestService();
        string mySelectedUser;
        bool okPressed = false;

        ConfigUserDialogResult myConfigUserDialogResult = showConfigUserDialog(mySelectedProvider,
                mySelectedService,
                mySelectedUser,
                this);
        if (myConfigUserDialogResult != dlgCancelled)
        {
            okPressed = true;
            if (myConfigUserDialogResult == dlgAcceptedUserSelected)
            {
                theCurrentUser->select(mySelectedUser);
            }
            else
            {
                theCurrentUser->unselect();
            }
        }
        else
        {
            okPressed = false;
        }

        return okPressed;
    }

    void AuthenticatePage::customButtonClicked(int button)
    {
        if (button == AboutBtn)
        {
            AboutDialog about(rclient::Settings::getServiceUri(), this);
            about.exec();
            if (about.clickedButton() == about.reportProblemButton)
            {
                about.reportProblem();
            }
        }
        if (button == ServiceUriBtn)
        {
            const bool isOkPressed = selectUser();

            emit completeChanged();
            adjustUserIdUi();
            const bool myIsCrAuthentication = ta::isElemExist(resept::credResponse, theAuthReqs.cred_types);
            // When user is selected, a new challenge needs to be generated by the keytalk server.
            if (myIsCrAuthentication && isOkPressed)
            {
                initializePage();
            }
        }
    }

    bool AuthenticatePage::isComplete() const
    {
        return canGoNext();
    }

    bool AuthenticatePage::canGoNext() const
    {
        return (theCurrentUser->isSelected() &&
                theRcdpClient.get() &&
                (theRcdpClient->userSessionData().rcdpState == resept::rcdpv2::stateConnected));
    }

    void AuthenticatePage::requestAuthRequirements()
    {
        FUNCLOG;

        WaitDialog myWaitDialog("Contacting " + resept::ProductName + " server...", this, theAuthenticationWizard->visitedPages().empty());

        if (theRcdpClient.get() && theRcdpClient->userSessionData().rcdpState != resept::rcdpv2::stateClosed)
        {
            theRcdpClient->eoc();
        }

        const ta::NetUtils::RemoteAddress mySvr = rclient::Settings::getReseptSvrAddress();

        DEBUGLOG(boost::format("Connecting to %s server at %s") % resept::ProductName % toString(mySvr));
        theRcdpClient.reset(new rclient::RcdpHandler(mySvr));
        theRcdpClient->hello();
        theRcdpClient->handshake();
        theAuthReqs = theRcdpClient->getAuthRequirements(rclient::Settings::getLatestService());
    }

    void AuthenticatePage::adjustUserIdUi()
    {
        if (!layout())
        {
            TA_THROW_MSG(std::runtime_error, "Layout is not initialized");
        }
        const string myUserId = theCurrentUser->isSelected() ? theCurrentUser->name() : "<Not Selected>";
        const unsigned int myUserIdItemPos = rclient::Settings::isDisplayServiceName() ? 5 : 3;
        QLayoutItem* myLayoutItem = layout()->itemAt(myUserIdItemPos);
        if (!myLayoutItem)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("No item found in the layout at position %d") % myUserIdItemPos);
        }
        if (!myLayoutItem->widget())
        {
            TA_THROW_MSG(std::runtime_error, "No widget found in the layout");
        }
        QLabel* myUseridLabel = dynamic_cast<QLabel*>(myLayoutItem->widget());
        if (!myUseridLabel)
        {
            TA_THROW_MSG(std::runtime_error, "No userid label found in the layout");
        }
        myUseridLabel->setText(QString::fromUtf8(myUserId.c_str()));
    }

    // userid label is always shown
    void AuthenticatePage::buildCredentialsPromptUi(const AuthRequirementsLocation anAuthRequirementsLocation)
    {
        if (anAuthRequirementsLocation == requestAuthRequirementsFromSvr)
        {
            requestAuthRequirements();
            handleCrPhase1Authentication();
        }

        const string myUserId = theCurrentUser->isSelected() ? theCurrentUser->name() : "<Not Selected>";
        CredType2LineEditMap myCredType2LineEditMap;
        ResponseName2LineEditMap myResponseName2LineEditMap;
        QFormLayout* myLayout = new QFormLayout;

        myLayout->addRow("Provider:", new QLabel(QString::fromUtf8(rclient::Settings::getLatestProvider().c_str())));
        if (rclient::Settings::isDisplayServiceName())
        {
            myLayout->addRow("Service:", new QLabel(QString::fromUtf8(rclient::Settings::getLatestService().c_str())));
        }
        myLayout->addRow("User:", new QLabel(QString::fromUtf8(myUserId.c_str())));

        // CR
        if (ta::isElemExist(resept::credResponse, theAuthReqs.cred_types))
        {
            // add challenges
            foreach(const ta::StringDict::value_type& challNameVal, theAuthResponse.challenges)
            {
                string myChallengePrompt = challNameVal.first;
                if (!boost::ends_with(myChallengePrompt, ":"))
                {
                    myChallengePrompt += ":";
                }
                const string myChallengeDisplayValue = challNameVal.second;

                QLineEdit* myChallengeLineEdit = new QLineEdit;
                myChallengeLineEdit->setReadOnly(true);
                myChallengeLineEdit->setEchoMode(QLineEdit::Normal);
                myChallengeLineEdit->setText(QString::fromUtf8(myChallengeDisplayValue.c_str()));
                myChallengeLineEdit->setFocusPolicy(Qt::NoFocus);
                myLayout->addRow(myChallengePrompt.c_str(), myChallengeLineEdit);
            }

            // add response names
            foreach(const string& respName, theAuthResponse.response_names)
            {
                string myResponsePrompt = respName;
                if (!boost::ends_with(myResponsePrompt, ":"))
                {
                    myResponsePrompt += ":";
                }

                QLineEdit* myResponseLineEdit = new QLineEdit;
                myResponseLineEdit->setEchoMode(QLineEdit::Password);
                myLayout->addRow(myResponsePrompt.c_str(), myResponseLineEdit);
                myResponseName2LineEditMap[respName] = myResponseLineEdit;
            }
        }
        else // non-CR
        {
            // password
            if (ta::isElemExist(resept::credPasswd, theAuthReqs.cred_types))
            {
                const QString myPasswordPrompt = getPasswordPrompt();
                QLabel* myPasswordPromptLabel = new QLabel(myPasswordPrompt);
                myPasswordPromptLabel->setWordWrap(true);
                myPasswordPromptLabel->setMaximumWidth(wizard()->width() / 3);

                QLineEdit* myPasswdLineEdit = new QLineEdit;
                myPasswdLineEdit->setEchoMode(QLineEdit::Password);

                myLayout->addRow(myPasswordPromptLabel, myPasswdLineEdit);
                myCredType2LineEditMap[resept::credPasswd] = myPasswdLineEdit;
            }

            // pincode
            if (ta::isElemExist(resept::credPin, theAuthReqs.cred_types))
            {
                QLineEdit* myPincodeLineEdit = new QLineEdit;
                myPincodeLineEdit->setEchoMode(QLineEdit::Password);
                myLayout->addRow("Pincode:", myPincodeLineEdit);
                myCredType2LineEditMap[resept::credPin] = myPincodeLineEdit;
            }
        }

        resetLayout(myLayout);
        adjustKeyboardFocus(myLayout);
        theCredType2LineEditMap = myCredType2LineEditMap;
        theResponseName2LineEditMap = myResponseName2LineEditMap;
    }

    QString AuthenticatePage::getPasswordPrompt() const
    {
        QString myPasswordPrompt;

        if (theAuthResponse.auth_result.type == resept::AuthResult::Challenge)
        {
            if (!theAuthResponse.challenges.empty())
            {
                myPasswordPrompt = QString::fromUtf8((theAuthResponse.challenges.begin()->second).c_str());
            }
        }
        else
        {
            myPasswordPrompt = QString::fromUtf8(theAuthReqs.password_prompt.c_str());
        }

        myPasswordPrompt = myPasswordPrompt.trimmed();

        if (myPasswordPrompt.isEmpty())
        {
            myPasswordPrompt = "Password:";
        }
        if (!myPasswordPrompt.endsWith(':'))
        {
            myPasswordPrompt += ':';
        }

        return myPasswordPrompt;
    }

    void AuthenticatePage::resetLayout(QLayout* aLayout)
    {
        if (layout())
        {
            QLayoutItem* child;
            while ((child = layout()->takeAt(0)) != 0)
            {
                delete child->widget();
                delete child;
            }
            delete layout();
        }
        setLayout(aLayout);
    }

    // Give focus to the topmost non-readonly line edit, skip userid
    void AuthenticatePage::adjustKeyboardFocus(QFormLayout* aLayout)
    {
        TA_ASSERT(aLayout);
        for (int i = 1; i < aLayout->rowCount(); ++i)
        {
            QLayoutItem* myLayoutItem = aLayout->itemAt(i, QFormLayout::FieldRole);
            if (myLayoutItem)
            {
                QLineEdit* myLineEdit = dynamic_cast<QLineEdit*>(myLayoutItem->widget());
                if (myLineEdit && !myLineEdit->isReadOnly())
                {
                    myLineEdit->setFocus(Qt::ActiveWindowFocusReason);
                    break;
                }
            }
        }
    }

    resept::Credentials AuthenticatePage::readCredentialsFromUi() const
    {
        resept::Credentials myRetVal;

        foreach(const resept::CredentialType cred_type, theAuthReqs.cred_types)
        {
            if (cred_type == resept::credUserId)
            {
                myRetVal.push_back(resept::Credential(resept::credUserId, theCurrentUser->name()));
            }
            else if (cred_type == resept::credHwSig)
            {
                const string myHwsig = calcHwsig(theAuthReqs.hwsig_formula);
                myRetVal.push_back(resept::Credential(resept::credHwSig, myHwsig));
            }
            else if (cred_type == resept::credPasswd)
            {
                CredType2LineEditMap::const_iterator myIt = theCredType2LineEditMap.find(resept::credPasswd);
                if (myIt == theCredType2LineEditMap.end())
                {
                    TA_THROW_MSG(std::runtime_error, "Password is required but it is not found in UI");
                }
                QByteArray myPasswdBytes = myIt->second->text().toUtf8();
                const string myPasswd(myPasswdBytes.data(), myPasswdBytes.size());
                myRetVal.push_back(resept::Credential(resept::credPasswd, myPasswd));
            }
            else if (cred_type == resept::credPin)
            {
                CredType2LineEditMap::const_iterator myIt = theCredType2LineEditMap.find(resept::credPin);
                if (myIt == theCredType2LineEditMap.end())
                {
                    TA_THROW_MSG(std::runtime_error, "Pincode is required but it is not found in UI");
                }
                QByteArray myPincodeBytes = myIt->second->text().toUtf8();
                const string myPincode(myPincodeBytes.data(), myPincodeBytes.size());
                myRetVal.push_back(resept::Credential(resept::credPin, myPincode));
            }
            else if (cred_type == resept::credResponse)
            {
                if (theResponseName2LineEditMap.empty())
                {
                    TA_THROW_MSG(std::runtime_error, "Response is required but it is not found in UI");
                }
                ta::StringDict myResponses;
                foreach(const ResponseName2LineEditMap::value_type& respName2lineEdit, theResponseName2LineEditMap)
                {
                    const string myResponseName = respName2lineEdit.first;
                    const QByteArray myResponseBytes = respName2lineEdit.second->text().toUtf8();
                    const string myResponseValue(myResponseBytes.data(), myResponseBytes.size());
                    myResponses[myResponseName] = myResponseValue;
                }
                myRetVal.push_back(resept::Credential(myResponses));
            }
        }

        return myRetVal;
    }

    //@return if credentials are valid, function return true and anErrorMsg is unaffected
    // otherwise anErrorMsg contains user-friendly error message and the function return false
    bool AuthenticatePage::validateSuppliedCredentials(const resept::Credentials& aCredentials, string& anErrorMsg)
    {
        foreach(const resept::Credential& cred, aCredentials)
        {
            if (cred.type == resept::credUserId && !resept::isValidUserName(cred.val, anErrorMsg))
                return false;
            if (cred.type == resept::credPasswd && !resept::isValidPassword(cred.val, anErrorMsg))
                return false;
            if (cred.type == resept::credPin && !resept::isValidPincode(cred.val, anErrorMsg))
                return false;
            if (cred.type == resept::credResponse && !resept::isValidResponse(cred.val, anErrorMsg))
                return false;
        }
        return true;
    }


    //@return whether authentication is successful so we can go further and request cert
    bool AuthenticatePage::authenticate(const resept::Credentials& aCreds)
    {
        const string myProvider = rclient::Settings::getLatestProvider();
        const string myService = rclient::Settings::getLatestService();
        string myUserId;
        if (!findCredential(aCreds, resept::credUserId, myUserId))
        {
            myUserId = "<not defined>";
        }

        {
            WaitDialog myWaitDialog("Authenticating against " + resept::ProductName + " server...", this);
            const ta::StringArrayDict myResolvedURIs = resolveURIs();
            const ta::StringDict myCalculatedDigests = calcDigests();
            theAuthResponse = theRcdpClient->authenticate(myService, aCreds, myResolvedURIs, myCalculatedDigests);
        }

        switch (theAuthResponse.auth_result.type)
        {
        case resept::AuthResult::Ok:
        {
            return true;
        }
        case resept::AuthResult::Locked:
        {
            WARNLOG(boost::format("User %s is locked trying to authenticate against provider %s, service %s") % myUserId % myProvider % myService);
            throw UserLockedError();
        }
        case resept::AuthResult::Delay:
        {
            WARNLOG(boost::format("User %s is delayed for %d seconds because of invalid credentials provided for provider %s, service %s") % myUserId % theAuthResponse.auth_result.delay % myProvider % myService);
            if (!AuthDelayedMessageBox::show(this, theAuthResponse.auth_result.delay))
            {
                throw AuthCancelledException();
            }
            // try again
            buildCredentialsPromptUi(requestAuthRequirementsFromSvr);
            return false;
        }
        case resept::AuthResult::Expired:
        {
            return false;
        }
        case resept::AuthResult::Challenge:
        {
            // prompt user for the next password in multi-round password authentication or for the the next response for CR authentication
            buildCredentialsPromptUi(continueWithKnownAuthRequirements);
            return false;
        }
        default:
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Unsupported auth result %s received for phase 1 CR authentication for provider %s, service %s and user %s") % str(theAuthResponse.auth_result.type) % myProvider % myService % myUserId);
        }
        }// switch
    }

    void AuthenticatePage::checkForNewMessages()
    {
        rclient::Messages myLastMessages;
        if (rclient::Settings::isLastUserMsgUtcExist())
        {
            const time_t myLastUserMsgFromUtc = ta::TimeUtils::parseUtcIso8601(rclient::Settings::getLastUserMsgUtc()) + 1;
            myLastMessages = theRcdpClient->getLastMessages(&myLastUserMsgFromUtc);
        }
        else
        {
            myLastMessages = theRcdpClient->getLastMessages();
        }

        foreach(const rclient::Message& msg, myLastMessages)
        {
            const string myUtf8Msg = str(boost::format("%s Server message from %s\n\n%s") % resept::ProductName % ta::TimeUtils::timestampToLocalStr(msg.utc) % msg.text);
            QMessageBox::information(this, "Server message", QString::fromUtf8(myUtf8Msg.c_str()));
        }

        if (!myLastMessages.empty())
        {
            const string myLastUserMsgUtc = ta::TimeUtils::timestampToIso8601(myLastMessages.back().utc);
            Settings::setLastUserMsgUtc(myLastUserMsgUtc);
        }
    }

    void AuthenticatePage::requestCertificate()
    {
        WaitDialog myWaitDialog("Retrieving certificate...", this);
        const resept::CertFormat myCertFormat = Settings::getCertFormat();
        const bool myWithChain = rclient::Settings::isCertChain();
        const rclient::CertResponse myCertResponse = theRcdpClient->getCert(myCertFormat, myWithChain);

        // Import/save certificate
        if (myCertFormat == resept::certformatP12)
        {
            const rclient::Pfx myPfx(myCertResponse.cert, myCertResponse.password);
            rclient::NativeCertStore::importPfx(myPfx);
        }
        else if (myCertFormat == resept::certformatPem)
        {
            const string mySavedPemPath = ta::Process::getTempDir() + rclient::SavedPemName;
            const string mySavedPemKeyPasswdPath = ta::Process::getTempDir() + rclient::SavedPemKeyPasswdName;
            ta::writeData(mySavedPemPath, myCertResponse.cert);
            ta::writeData(mySavedPemKeyPasswdPath, myCertResponse.password);
            DEBUGLOG(boost::format("PEM has been saved to %s, private key password has been saved to %s") % mySavedPemPath % mySavedPemKeyPasswdPath);
            QMessageBox::information(this, "Certificate saved", str(boost::format("User PEM certificate has been saved to:\n%s\n\nPrivate key password has been saved to:\n%s") % mySavedPemPath % mySavedPemKeyPasswdPath).c_str());
        }
        else
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Unsupported certificate format in settings: %d") % myCertFormat);
        }

        theRcdpClient->eoc();

        theExecuteSync = myCertResponse.execute_sync;
    }

    ta::StringArrayDict AuthenticatePage::resolveURIs() const
    {
        ta::StringArrayDict myResolvedURIs;
        if (theAuthReqs.resolve_service_uris)
        {
            foreach(const string& uri, theAuthReqs.service_uris)
            {
                const string myHost = ta::url::parse(uri).authority_parts.host;
                DEBUGLOG("Resolving " + myHost);
                ta::StringArray myIps;
                foreach(const ta::NetUtils::IP& ip, ta::DnsUtils::resolveIpsByName(myHost))
                {
                    if (!ip.ipv4.empty())
                    {
                        myIps.push_back(ip.ipv4);
                    }
                    if (!ip.ipv6.empty())
                    {
                        myIps.push_back(ip.ipv6);
                    }
                }
                DEBUGLOG("Resolved IPs of " + myHost + ": " + ta::Strings::join(myIps, ","));
                myResolvedURIs[uri] = myIps;
            }
        }
        return myResolvedURIs;
    }

    ta::StringDict AuthenticatePage::calcDigests() const
    {
        ta::StringDict myCalculatedDigests;
        if (theAuthReqs.calc_service_uris_digest)
        {
            foreach(const string& uri, theAuthReqs.service_uris)
            {
                const string myExecutableNativePath = ta::Process::expandEnvVars(ta::url::makeNativePath(uri));
                const string myDigest = ta::HashUtils::getSha256HexFile(myExecutableNativePath);
                DEBUGLOG("Digest of " + myExecutableNativePath + "  is " + myDigest);
                myCalculatedDigests[uri] = myDigest;
            }
        }
        return myCalculatedDigests;
    }



} // namespace rclient
