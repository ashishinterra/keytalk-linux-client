#include "ConfigUsersDialog.h"
#include "ui_ConfigUsersDialog.h"
#include "AddUserDialog.h"
#include "AuthenticationWizard.h"
#include "rclient/Common.h"
#include "rclient/Settings.h"
#include "ta/assert.h"
#include "ta/netutils.h"
#include "ta/utils.h"
#include "ta/strings.h"
#include "ta/logger.h"

#include <QtWidgets>

using std::string;

namespace rclient
{
    ConfigUsersDialog::ConfigUsersDialog(const QIcon& anIcon,
                                         const std::string& aProviderName,
                                         const std::string& aServiceName,
                                         QWidget* parent)
        : QDialog(parent)
        , theConfigUsersDialogPtr (new Ui::ConfigUsersDialogClass)
        , theIcon(anIcon)
        , theProviderName(aProviderName)
        , theServiceName(aServiceName)
        , theLastAcceptCode(acceptOk)
    {
        theConfigUsersDialogPtr->setupUi(this);

        setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

        if (!parent)
        {
            // no parent, care to be on top ourselves
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        }

        setWindowIcon(anIcon);

        setWindowTitle("Configure users");
        loadReseptSettingsToUi();

        connect(theConfigUsersDialogPtr->UserList, SIGNAL(itemDoubleClicked( QListWidgetItem*)), this, SLOT(on_OkBtn_clicked()));
    }

    ConfigUsersDialog::~ConfigUsersDialog()
    {
        delete theConfigUsersDialogPtr;
    }

    // @throw SettingsError
    void ConfigUsersDialog::loadReseptSettingsToUi()
    {
        bool myFromMasterConfig;
        const Settings::Users myUsers = Settings::getUsers(theProviderName, theServiceName, myFromMasterConfig);

        theConfigUsersDialogPtr->UserList->clear();
        foreach (const string& user, myUsers)
        {
            theConfigUsersDialogPtr->UserList->addItem(QString::fromUtf8(user.c_str()));
        }
        if (theConfigUsersDialogPtr->UserList->count())
            theConfigUsersDialogPtr->UserList->setCurrentRow(0);

        theCanAddRemoveUsers = !myFromMasterConfig;
        theConfigUsersDialogPtr->AddBtn->setEnabled(theCanAddRemoveUsers);
        theConfigUsersDialogPtr->RemoveBtn->setEnabled(theCanAddRemoveUsers && theConfigUsersDialogPtr->UserList->count());
    }

    void ConfigUsersDialog::on_OkBtn_clicked()
    {
        theLastAcceptCode = acceptOk;
        try
        {
            saveReseptSettingsFromUi();
        }
        catch (SettingsError& e)
        {
            ERRORLOG2("Error saving RESEPT settings", e.what());
            theLastAcceptCode = acceptSettingsError;
        }
        catch (std::exception& e)
        {
            ERRORLOG2("Error saving RESEPT settings", e.what());
            theLastAcceptCode = acceptUnknownError;
        }
        catch (...)
        {
            ERRORLOG2("Error saving RESEPT settings", "Unknown error");
            theLastAcceptCode = acceptUnknownError;
        }
        accept();
    }

    void ConfigUsersDialog::on_CancelBtn_clicked()
    {
        reject();
    }

    ConfigUsersDialog::LastAcceptCode ConfigUsersDialog::getLastAcceptCode() const
    {
        return theLastAcceptCode;
    }

    bool ConfigUsersDialog::getSelectedUser(string& aSelectedUser) const
    {
        if (!theConfigUsersDialogPtr->UserList->selectedItems().size())
            return false;
        aSelectedUser = theConfigUsersDialogPtr->UserList->currentItem()->text().toUtf8();
        return true;
    }


    void ConfigUsersDialog::on_AddBtn_clicked()
    {
        addUser();
    }

    void ConfigUsersDialog::on_RemoveBtn_clicked()
    {
        removeCurrentUser();
    }


    // Exceptions: throw SettingsError on error
    void ConfigUsersDialog::saveReseptSettingsFromUi() const
    {
        Settings::removeUsers(theProviderName, theServiceName);
        const int mySize = theConfigUsersDialogPtr->UserList->count();
        for (int row = 0; row < mySize; ++row)
        {
            QByteArray myUserNameUtf8Bytes = theConfigUsersDialogPtr->UserList->item(row)->text().toUtf8();
            string myUtf8UserName(myUserNameUtf8Bytes.data(), myUserNameUtf8Bytes.size());
            Settings::addUser(theProviderName, theServiceName, myUtf8UserName);
        }
    }

    void ConfigUsersDialog::addUser()
    {
        std::list<string> myUtf8Users;
        const int mySize = theConfigUsersDialogPtr->UserList->count();
        for (int row = 0; row < mySize; ++row)
        {
            QByteArray myUserNameUtf8Bytes = theConfigUsersDialogPtr->UserList->item(row)->text().toUtf8();
            string myUtf8UserName(myUserNameUtf8Bytes.data(), myUserNameUtf8Bytes.size());
            myUtf8Users.push_back(myUtf8UserName);
        }

        AddUserDialog myAddUserDialog(theIcon, myUtf8Users, this);
        if (myAddUserDialog.exec() == QDialog::Rejected)
            return;
        string myUtf8UserName = myAddUserDialog.getUtf8UserName();
        TA_ASSERT(theConfigUsersDialogPtr->UserList->findItems(QString::fromUtf8(myUtf8UserName.c_str()), Qt::MatchFixedString|Qt::MatchCaseSensitive).empty());
        theConfigUsersDialogPtr->UserList->addItem(QString::fromUtf8(myUtf8UserName.c_str()));
        QList<QListWidgetItem*> myItems = theConfigUsersDialogPtr->UserList->findItems(QString::fromUtf8(myUtf8UserName.c_str()), Qt::MatchFixedString|Qt::MatchCaseSensitive);
        TA_ASSERT(myItems.size()  == 1);
        theConfigUsersDialogPtr->UserList->setCurrentItem(myItems.first());
    }

    void ConfigUsersDialog::removeCurrentUser()
    {
        int myCurRow = theConfigUsersDialogPtr->UserList->currentRow();
        if (myCurRow < 0)
            return;
        if (QMessageBox::question(this, "Remove user confirmation", "Are you sure?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
            return;
        QListWidgetItem* myItem = theConfigUsersDialogPtr->UserList->takeItem (myCurRow);
        delete myItem;
    }

    void ConfigUsersDialog::on_UserList_currentItemChanged(QListWidgetItem* UNUSED(current), QListWidgetItem* UNUSED(previous))
    {
        if (theConfigUsersDialogPtr->UserList->currentRow() >= 0 && theCanAddRemoveUsers)
            theConfigUsersDialogPtr->RemoveBtn->setEnabled(true);
        else
            theConfigUsersDialogPtr->RemoveBtn->setEnabled(false);
    }


    ConfigUserDialogResult showConfigUserDialog(const string& aProvider,
            const string& aService,
            string& aSelectedUser,
            QWidget* parent)
    {
        using rclient::Settings::getProviderInstallDir;

        QIcon myProviderIcon(":/RClientAppCommon/logo.png"); // default
        string myProviderIconPath = getProviderInstallDir(aProvider) + ta::getDirSep() + rclient::LogoV20ImageName;
        if (!ta::isFileExist(myProviderIconPath))
        {
            myProviderIconPath = getProviderInstallDir(aProvider) + ta::getDirSep() + rclient::LogoV11ImageName;
            if (!ta::isFileExist(myProviderIconPath))
            {
                myProviderIconPath = getProviderInstallDir(aProvider) + ta::getDirSep() + rclient::IconV10ImageName;
            }
        }
        if (ta::isFileExist(myProviderIconPath))
        {
            myProviderIcon = QIcon(myProviderIconPath.c_str());
        }

        ConfigUsersDialog myDlg(myProviderIcon, aProvider, aService, parent);
        if (myDlg.exec() != QDialog::Accepted)
            return dlgCancelled;

        ConfigUsersDialog::LastAcceptCode myLastAcceptCode = myDlg.getLastAcceptCode();
        if (myLastAcceptCode == ConfigUsersDialog::acceptOk)
            return myDlg.getSelectedUser(aSelectedUser) ? dlgAcceptedUserSelected : dlgAcceptedNoUserSelected;

        if (myLastAcceptCode == ConfigUsersDialog::acceptSettingsError)
            TA_THROW_MSG(std::runtime_error, resept::ProductName + " installation is misconfigured. Please contact " + resept::ProductName + " administrator.");
        TA_THROW_MSG(std::runtime_error, resept::ProductName + " error. Please contact " + resept::ProductName + " administrator.");
    }
}
