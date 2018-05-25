#include "AuthenticationWizard.h"
#include "ChooseProviderServicePage.h"
#include "ConfigUsersDialog.h"
#include "AuthenticatePage.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/CommonUtils.h"
#include "ta/common.h"
#include "ta/logger.h"

#include <QtWidgets>
#include <vector>

using namespace ta;
using std::string;
using std::vector;

namespace
{
    /**
      Checks client settings that at least one provider exists and for each provider at least one service exist
    */
    void verifyExistProviderService()
    {
        std::vector<string> myProviders = rclient::Settings::getProviders();
        if (myProviders.empty())
            TA_THROW_MSG(std::runtime_error, "No providers exist in the client settings");
        foreach (string provider, myProviders)
        {
            if (rclient::Settings::getServices(provider).empty())
                TA_THROW_MSG(std::runtime_error, boost::format("No services exist in the client settings for provider %s") % provider);
        }
    }
}


namespace rclient
{
    AuthenticationWizard::AuthenticationWizard(QWidget* parent)
        : QWizard(parent)
        , theClientType(clientStandalone)
        , theAuthenticatePage(NULL)
    {
        verifyExistProviderService();

        Page myStartPage = pageAuthenticate;
        if (rclient::Settings::getProviders().size() == 1 && rclient::Settings::getServices().size() == 1)
        {
            const string mySelectedProvider = rclient::Settings::getProviders()[0];
            const string mySelectedService = rclient::Settings::getServices()[0];

            rclient::Settings::setLatestProviderService(mySelectedProvider, mySelectedService);
            rclient::NativeCertStore::deleteAllReseptUserCerts();
            myStartPage = pageAuthenticate;

            if (!selectUser(mySelectedProvider, mySelectedService, NULL /* NULL because we call from c'tor */))
            {
                WARNLOG(boost::format("No user selected for provider %s, service %s, cancelling authentication") % mySelectedProvider % mySelectedService);
                throw AuthCancelledException();
            }
        }
        else
        {
            myStartPage = pageSelectProviderService;
        }

        init(myStartPage);
    }


    AuthenticationWizard::AuthenticationWizard(const vector<std::pair<string, string> >& aProviderServicePairs, QWidget* parent)
        : QWizard(parent)
        , theClientType(clientBrowser)
        , theAuthenticatePage(NULL)
    {
        verifyExistProviderService();

        Page myStartPage = pageAuthenticate;

        const string myLatestProvider = rclient::Settings::getLatestProvider();
        const string myLatestService = rclient::Settings::getLatestService();

        if (ta::isElemExist(std::make_pair(myLatestProvider, myLatestService), aProviderServicePairs))
        {
            // the latest provider/service is in aProviderServicePairs, skip provider/service selection UI
            rclient::NativeCertStore::deleteInvalidReseptUserCerts();
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                DEBUGLOG("Certificate is still valid.");
                throw CertStillValidException();
            }
            DEBUGLOG("No valid certificate found (browser), proceeding with authentication");
            myStartPage = pageAuthenticate;

            if (!selectUser(myLatestProvider, myLatestService, NULL /* NULL because we call from c'tor */))
            {
                WARNLOG(boost::format("No user selected for provider %s, service %s, cancelling authentication") % myLatestProvider % myLatestService);
                throw AuthCancelledException();
            }
        }
        else if (aProviderServicePairs.size() == 1)
        {
            // the only provider/service pair, skip provider/service selection UI
            const string mySelectedProvider = aProviderServicePairs.front().first;
            const string mySelectedService = aProviderServicePairs.front().second;

            rclient::Settings::setLatestProviderService(mySelectedProvider, mySelectedService);
            rclient::NativeCertStore::deleteInvalidReseptUserCerts();
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                DEBUGLOG("Certificate is still valid.");
                throw CertStillValidException();
            }
            DEBUGLOG("No valid certificate found (browser), proceeding with authentication");
            myStartPage = pageAuthenticate;

            if (!selectUser(mySelectedProvider, mySelectedService, NULL /* NULL because we call from c'tor */))
            {
                WARNLOG(boost::format("No user selected for provider %s, service %s, cancelling authentication") % mySelectedProvider % mySelectedService);
                throw AuthCancelledException();
            }
        }
        else if (rclient::Settings::getProviders().size() == 1 && rclient::Settings::getServices().size() == 1)
        {
            // only one provider and service
            const string mySelectedProvider = rclient::Settings::getProviders()[0];
            const string mySelectedService = rclient::Settings::getServices()[0];

            rclient::Settings::setLatestProviderService(mySelectedProvider, mySelectedService);
            rclient::NativeCertStore::deleteInvalidReseptUserCerts();
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                DEBUGLOG("Certificate is still valid.");
                throw CertStillValidException();
            }
            DEBUGLOG("No valid certificate found (browser), proceeding with authentication");
            myStartPage = pageAuthenticate;

            if (!selectUser(mySelectedProvider, mySelectedService, NULL /* NULL because we call from c'tor */))
            {
                WARNLOG(boost::format("No user selected for provider %s, service %s, cancelling authentication") % mySelectedProvider % mySelectedService);
                throw AuthCancelledException();
            }
        }
        else
        {
            myStartPage = pageSelectProviderService;
        }

        init(myStartPage);
    }

    void AuthenticationWizard::init(Page aStartPage)
    {
        setPage(pageSelectProviderService, new rclient::ChooseProviderServicePage(&theCurrentUser, theClientType, this));
        theAuthenticatePage = new rclient::AuthenticatePage(&theCurrentUser, this);
        setPage(pageAuthenticate, theAuthenticatePage);
        setStartId(aStartPage);

        setWizardStyle(ModernStyle);
        setWindowIcon(QIcon(":/RClientAppCommon/logo.png"));
        setPixmap(QWizard::LogoPixmap, QPixmap(":/RClientAppCommon/logo.png"));
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setWindowTitle("Authentication Agent");

        setButtonText(FinishButton, "Connect");
    }

    string AuthenticationWizard::getServiceUri() const
    {
        if (!theAuthenticatePage)
            TA_THROW_MSG(std::logic_error, "Authentication page has not been initialized yet");
        return theAuthenticatePage->getServiceUri();
    }

    bool AuthenticationWizard::getExecuteSync() const
    {
        if (!theAuthenticatePage)
            TA_THROW_MSG(std::logic_error, "Authentication page has not been initialized yet");
        return theAuthenticatePage->getExecuteSync();
    }

    bool AuthenticationWizard::selectUser(const string& aProvider, const string& aService, QWidget* aParent)
    {
        if (rclient::Settings::getUsers().size() == 1)
        {
            theCurrentUser.select(rclient::Settings::getUsers()[0]);
            return true;
        }

        string mySelectedUser;
        if (showConfigUserDialog(aProvider, aService, mySelectedUser, aParent) == dlgAcceptedUserSelected)
        {
            theCurrentUser.select(mySelectedUser);
            return true;
        }
        theCurrentUser.unselect();
        return false;
    }
}
