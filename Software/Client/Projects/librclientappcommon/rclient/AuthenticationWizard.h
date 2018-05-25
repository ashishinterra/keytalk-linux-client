//----------------------------------------------------------------------------
//
//  Name          AuthenticationWizard.h
//  Description : Provider/Service selection wizard for a user
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_AUTHENTICATIONWIZARD_H
#define RCLIENT_AUTHENTICATIONWIZARD_H

#include <string>
#include <stdexcept>
#include <vector>
#include <QWizard>

namespace rclient
{
    class CurrentUser
    {
    public:
        CurrentUser() : selected(false) {}
        inline std::string name() const  { return user;}
        inline bool isSelected() const  { return selected;}
        inline void select(const std::string& aUser) { user = aUser; selected = true;}
        inline void unselect() { user = ""; selected = false;}
    private:
        std::string user;
        bool selected;
    };

    enum ClientType
    {
        clientStandalone,
        clientBrowser
    };

    class AuthenticatePage;

    class AuthenticationWizard : public QWizard
    {
        Q_OBJECT
    public:
        enum Page { pageSelectProviderService, pageAuthenticate};

        // c'tor for standalone client
        AuthenticationWizard(QWidget* parent = NULL);
        // c'tor for browser client
        AuthenticationWizard(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, QWidget* parent = NULL);

        std::string getServiceUri() const;
        bool getExecuteSync() const;
    private:
        void init(Page aStartPage);
        bool selectUser(const std::string& aProvider, const std::string& aService, QWidget* parent);
    private:
        CurrentUser theCurrentUser;
        const ClientType theClientType;
        AuthenticatePage* theAuthenticatePage;
        static const QSize theDefaultSize;
    };
}

#endif
