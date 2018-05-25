#include "ChooseProviderServicePage.h"
#include "AuthenticationWizard.h"
#include "AboutDialog.h"
#include "ConfigUsersDialog.h"
#include "CommonUtils.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "ta/logger.h"
#include "ta/common.h"

#include <QtWidgets>

using std::string;

namespace rclient
{
    ChooseProviderServicePage::ChooseProviderServicePage(CurrentUser* aCurrentUser, ClientType aClientType, AuthenticationWizard* anAuthenticationWizard)
        : QWizardPage(NULL)
        , theCurrentUser(aCurrentUser)
        , theClientType(aClientType)
        , theAuthenticationWizard(anAuthenticationWizard)
    {
        if (!aCurrentUser)
            TA_THROW_MSG(std::invalid_argument, "CurrentUser is NULL");

        QLabel* providerLabel = new QLabel("Provider:");
        theProvidersCombo = new QComboBox;

        QLabel* serviceLabel = new QLabel("Service:");
        theServicesCombo = new QComboBox;

        QFormLayout* mainLayout = new QFormLayout;
        mainLayout->addRow(providerLabel, theProvidersCombo);
        mainLayout->addRow(serviceLabel, theServicesCombo);
        setLayout(mainLayout);

        connect(theProvidersCombo, SIGNAL(activated(const QString&)), this, SLOT(onProviderSelected(const QString&)) );
        connect(theServicesCombo, SIGNAL(activated(const QString&)), this, SLOT(onServiceSelected(const QString&)) );
    }


    bool ChooseProviderServicePage::validatePage()
    {
        const string mySelectedProvider = theProvidersCombo->currentText().toUtf8();
        const string mySelectedService = theServicesCombo->currentText().toUtf8();
        rclient::Settings::setLatestProviderService(mySelectedProvider, mySelectedService);

        if (theClientType == clientStandalone)
        {
            rclient::NativeCertStore::deleteAllReseptUserCerts();
        }
        else if (theClientType == clientBrowser)
        {
            rclient::NativeCertStore::deleteInvalidReseptUserCerts();
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                DEBUGLOG("Certificate is still valid.");
                throw CertStillValidException();
            }
            DEBUGLOG("No valid certificate found (browser), proceeding with authentication");
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported client type %d") % theClientType);
        }

        //select user
        const rclient::Settings::Users myUsers = rclient::Settings::getUsers(mySelectedProvider, mySelectedService);
        if (myUsers.size() == 1)
        {
            theCurrentUser->select(myUsers[0]);
            return true;
        }

        string mySelectedUser;
        if (showConfigUserDialog(mySelectedProvider, mySelectedService, mySelectedUser, this) == dlgAcceptedUserSelected)
        {
            theCurrentUser->select(mySelectedUser);
            return true;
        }

        theCurrentUser->unselect();
        return false;
    }

    int ChooseProviderServicePage::nextId() const
    {
        return AuthenticationWizard::pageAuthenticate;
    }

    void ChooseProviderServicePage::initializePage()
    {
        setTitle("Select Provider and Service");
        setSubTitle("Please select an Application Service Provider and a Service");

        const string mySelectedProvider = rclient::Settings::getLatestProvider();
        const string mySelectedService = rclient::Settings::getLatestService();
        updateUi(mySelectedProvider, mySelectedService);
    }

    void ChooseProviderServicePage::onProviderSelected(const QString& aSelectedProvider)
    {
        const string mySelectedProvider = aSelectedProvider.toStdString();
        const string mySelectedService = rclient::Settings::getServices(theProvidersCombo->currentText().toStdString()).at(0);
        updateUi(mySelectedProvider, mySelectedService);
    }

    void ChooseProviderServicePage::onServiceSelected(const QString& aSelectedService)
    {
        updateUi(theProvidersCombo->currentText().toStdString(), aSelectedService.toStdString());
    }

    void ChooseProviderServicePage::updateUi(const string& aSelectedProvider, const string& aSelectedService)
    {
        // Fill in providers
        theProvidersCombo->clear();
        std::list<string> mySortedProviders;
        foreach (const string& provider, rclient::Settings::getProviders())
        {
            mySortedProviders.push_back(provider);
        }
        mySortedProviders.sort();

        foreach (const string& provider, mySortedProviders)
        {
            theProvidersCombo->addItem(provider.c_str());
        }
        theProvidersCombo->setCurrentIndex(theProvidersCombo->findText(aSelectedProvider.c_str()));

        // Fill in services
        theServicesCombo->clear();
        std::list<string> mySortedServices;
        foreach (const string& service, rclient::Settings::getServices(aSelectedProvider))
        {
            mySortedServices.push_back(service);
        }
        mySortedServices.sort();

        foreach (const string& service, mySortedServices)
        {
            theServicesCombo->addItem(service.c_str());
        }
        theServicesCombo->setCurrentIndex(theServicesCombo->findText(aSelectedService.c_str()));
    }

    void ChooseProviderServicePage::setVisible(bool visible)
    {
        QWizardPage::setVisible(visible);

        if (visible)
        {
            wizard()->setButtonText(QWizard::CustomButton1, "");
            wizard()->setOption(QWizard::HaveCustomButton1, true);

            const QIcon aboutIcon(":/RClientAppCommon/info_ico.png");
            wizard()->button(QWizard::CustomButton1)->setIcon(aboutIcon);
            connect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));
        }
        else
        {
            wizard()->setOption(QWizard::HaveCustomButton1, false);
            disconnect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));
        }
    }

    void ChooseProviderServicePage::customButtonClicked(int button)
    {
        if (button == QWizard::CustomButton1)
        {
            AboutDialog about(this);
            about.exec();
            if (about.clickedButton() == about.reportProblemButton)
            {
                about.reportProblem();
            }
        }
    }

}
