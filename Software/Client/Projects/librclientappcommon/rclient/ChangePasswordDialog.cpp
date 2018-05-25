#include "ChangePasswordDialog.h"
#include "ui_ChangePasswordDialog.h"
#include "ta/common.h"
#include "rclient/Common.h"
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QtWidgets/QMessageBox>

ChangePasswordDialog::ChangePasswordDialog(const std::string& aUserId, const std::string& anOldPassword, QWidget* parent)
    : QDialog(parent)
    , theChangePasswordDialogPtr(new Ui::ChangePasswordDialogClass)
{
    theChangePasswordDialogPtr->setupUi(this);
    postSetupUi(aUserId, anOldPassword);
    QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
}

ChangePasswordDialog::~ChangePasswordDialog()
{
    delete theChangePasswordDialogPtr;
    QApplication::restoreOverrideCursor();
}

void ChangePasswordDialog::postSetupUi(const std::string& aUserId, const std::string& anOldPassword)
{
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

    theChangePasswordDialogPtr->UserIdLabel->setText(aUserId.c_str());
    theChangePasswordDialogPtr->PasswordEditOld->setText(anOldPassword.c_str());

    theChangePasswordDialogPtr->PasswordEditNew->setFocus(Qt::ActiveWindowFocusReason);
}


void ChangePasswordDialog::on_OkBtn_clicked()
{
    theEnteredUsername = theChangePasswordDialogPtr->UserIdLabel->text().toStdString();
    theEnteredPassword = theChangePasswordDialogPtr->PasswordEditOld->text().toStdString();
    theEnteredPasswordNew = theChangePasswordDialogPtr->PasswordEditNew->text().toStdString();
    theEnteredPasswordConfirm = theChangePasswordDialogPtr->PasswordEditConfirm->text().toStdString();

    if (theEnteredPasswordNew != theEnteredPasswordConfirm)
    {
        QMessageBox::warning(this, "Password change failed", "New password and confirmation are not equal!");
    }
    else
    {
        accept();
    }
}

void ChangePasswordDialog::on_CancelBtn_clicked()
{
    theEnteredUsername.clear();
    theEnteredPassword.clear();
    reject();
}

std::string ChangePasswordDialog::getEnteredPassword() const
{
    return theEnteredPassword;
}

std::string ChangePasswordDialog::getEnteredPasswordNew() const
{
    return theEnteredPasswordNew;
}
