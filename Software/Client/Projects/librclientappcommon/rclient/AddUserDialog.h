//----------------------------------------------------------------------------
//
//  Description : AddUserDialog class declaration
//                AddUserDialog implements a dialog for adding a user account
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_ADDUSERDIALOG_H
#define RCLIENT_ADDUSERDIALOG_H

#include <QDialog>
#include <list>
#include <string>

class QImage;
namespace Ui { class AddUserDialogClass; }

namespace rclient
{
    class AddUserDialog : public QDialog
    {
        Q_OBJECT

    public:
        AddUserDialog(const QIcon& anIcon, const std::list<std::string>& aParentUtf8Users, QWidget* parent = 0);
        ~AddUserDialog();

        // if the dialog is accepted, the method returns the user name entered by the user which does not exist in aParentUsers list
        // if the dialog is rejected, the return value is undefined
        std::string getUtf8UserName() const;
    private:
        bool validateUserName(const std::string& aUserName);
    private slots:
        void on_OkBtn_clicked();
    private:
        Ui::AddUserDialogClass* theAddUserDialogPtr;
        std::list<std::string> theParentUtf8Users;
        std::string theUtf8UserName;
    };
}

#endif
