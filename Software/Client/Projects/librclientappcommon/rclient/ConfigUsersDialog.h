//----------------------------------------------------------------------------
//
//  Description : ConfigUsersDialog class declaration
//                ConfigUsersDialog implements a dialog for managing user accounts
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_CONFIGUSERSDIALOG_H
#define RCLIENT_CONFIGUSERSDIALOG_H

#include <string>
#include <QDialog>

class QIcon;
class QListWidgetItem;
namespace Ui { class ConfigUsersDialogClass; }

namespace rclient
{
    class ConfigUsersDialog : public QDialog
    {
        Q_OBJECT
    public:
        enum LastAcceptCode
        {
            acceptOk, acceptSettingsError, acceptUnknownError
        };
        // @throw rclient::SettingsError
        ConfigUsersDialog(const QIcon& anIcon,
                          const std::string& aProviderName,
                          const std::string& aServiceName,
                          QWidget* parent = 0);
        ~ConfigUsersDialog();

        LastAcceptCode getLastAcceptCode() const;
        //@return whether the user has been selected
        bool getSelectedUser(std::string& aUser) const;

    private:
        void loadReseptSettingsToUi();
        void saveReseptSettingsFromUi() const;
        void addUser();
        void removeCurrentUser();
    private slots:
        void on_OkBtn_clicked();
        void on_CancelBtn_clicked();
        void on_AddBtn_clicked();
        void on_RemoveBtn_clicked();
        void on_UserList_currentItemChanged (QListWidgetItem* current, QListWidgetItem* previous );
    private:
        Ui::ConfigUsersDialogClass* theConfigUsersDialogPtr;
        const QIcon& theIcon;
        std::string theProviderName;
        std::string theServiceName;
        bool theCanAddRemoveUsers;
        LastAcceptCode theLastAcceptCode;
    };

    // Displays the dialog allowing to configure (add/change/remove) and select the user.
    enum ConfigUserDialogResult
    {
        dlgCancelled, dlgAcceptedNoUserSelected, dlgAcceptedUserSelected
    };
    ConfigUserDialogResult showConfigUserDialog(const std::string& aProvider,
            const std::string& aService,
            std::string& aSelectedUser,
            QWidget* parent = 0);
}

#endif
