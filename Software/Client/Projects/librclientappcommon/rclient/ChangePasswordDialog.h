//----------------------------------------------------------------------------
//
//  Description : Password dialog for user authentication
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_CHANGEPASSWORDDIALOG_H
#define RCLIENT_CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <string>

namespace Ui { class ChangePasswordDialogClass; }

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(const std::string& aUserId, const std::string& anOldPassword, QWidget* parent);
    ~ChangePasswordDialog();
    std::string getEnteredPassword() const;
    std::string getEnteredPasswordNew() const;
private slots:
    void on_OkBtn_clicked();
    void on_CancelBtn_clicked();
private:
    void postSetupUi(const std::string& aUserId, const std::string& anOldPassword);
private:
    Ui::ChangePasswordDialogClass* theChangePasswordDialogPtr;
    std::string theEnteredUsername;
    std::string theEnteredPassword;
    std::string theEnteredPasswordNew;
    std::string theEnteredPasswordConfirm;
};

#endif
