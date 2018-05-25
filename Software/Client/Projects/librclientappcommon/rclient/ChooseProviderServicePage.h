#ifndef RCLIENT_CHOOSEPROVIDERSERVICEPAGE_H
#define RCLIENT_CHOOSEPROVIDERSERVICEPAGE_H

#include "rclient/AuthenticationWizard.h"

#include <QWizard>
#include <string>


class QComboBox;

namespace rclient
{
    class ChooseProviderServicePage : public QWizardPage
    {
        Q_OBJECT
    public:
        ChooseProviderServicePage(CurrentUser* aCurrentUser, ClientType aClientType, AuthenticationWizard* anAuthenticationWizard);
    private:
        // Overriden QWiazardPage methods
        virtual int nextId() const;
        virtual void initializePage();// wizard is started
        virtual bool validatePage(); // user clicks 'next'
        virtual void setVisible(bool visible);

        void updateUi(const std::string& aSelectedProvider, const std::string& aSelectedService);

    private slots:
        void onProviderSelected(const QString& aText);
        void onServiceSelected(const QString& aText);
        void customButtonClicked(int button);

    private:
        CurrentUser* theCurrentUser;
        const ClientType theClientType;
        AuthenticationWizard* theAuthenticationWizard;
        QComboBox* theProvidersCombo;
        QComboBox* theServicesCombo;
    };
}

#endif
