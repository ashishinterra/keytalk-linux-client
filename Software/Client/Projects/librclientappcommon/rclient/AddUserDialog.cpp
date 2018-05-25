#include "AddUserDialog.h"
#include "ui_AddUserDialog.h"
#include "resept/util.h"
#include "resept/common.h"
#include "ta/common.h"
#include <QtWidgets>
#include <algorithm>

using std::string;
using std::list;

namespace rclient
{
    AddUserDialog::AddUserDialog(const QIcon& anIcon, const list<string>& aParentUtf8Users, QWidget* parent)
        : QDialog(parent)
        , theAddUserDialogPtr(new Ui::AddUserDialogClass)
        , theParentUtf8Users(aParentUtf8Users)
    {
        theAddUserDialogPtr->setupUi(this);

        setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
        setWindowIcon(anIcon);
        theAddUserDialogPtr->UserNameEdit->setFocus(Qt::ActiveWindowFocusReason);
    }

    AddUserDialog::~AddUserDialog()
    {
        delete theAddUserDialogPtr;
    }

    void AddUserDialog::on_OkBtn_clicked()
    {
        QByteArray myUserNameUtf8Bytes = theAddUserDialogPtr->UserNameEdit->text().toUtf8();
        string myUtf8UserName(myUserNameUtf8Bytes.data(), myUserNameUtf8Bytes.size());
        if (!validateUserName(myUtf8UserName))
            return;
        theUtf8UserName = myUtf8UserName;
        accept();
    }

    bool AddUserDialog::validateUserName(const string& anUtf8UserName)
    {
        std::string myErrMsg;
        if (!resept::isValidUserName(anUtf8UserName, myErrMsg))
            return QMessageBox::warning(this, "Invalid user name", myErrMsg.c_str()), false;

        if (ta::isElemExist(anUtf8UserName, theParentUtf8Users))
            return QMessageBox::warning(this, "The specified user is already in the list", "The specified user is already in the list"), false;
        return true;
    }

    std::string AddUserDialog::getUtf8UserName() const
    {
        return theUtf8UserName;
    }
}
