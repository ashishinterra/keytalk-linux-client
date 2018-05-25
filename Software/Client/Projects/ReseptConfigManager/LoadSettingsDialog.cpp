#include "LoadSettingsDialog.h"
#include "LoadSettingsBL.h"
#include "ui_LoadSettingsDialog.h"
#include "resept/common.h"
#include "rclient/WaitDialog.h"
#include "ta/logger.h"
#include "ta/common.h"

#include <QtWidgets>
#include <string>
#include <vector>

using std::string;

LoadSettingsDialog::LoadSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , theLoadSettingsDialogPtr (new Ui::LoadSettingsDialogClass)
{
    theLoadSettingsDialogPtr->setupUi(this);

    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

    theLoadSettingsDialogPtr->UrlRadioBtn  -> setChecked(true);
    theLoadSettingsDialogPtr->UrlEdit      -> setEnabled(true);
    theLoadSettingsDialogPtr->FileRadioBtn -> setChecked(false);
    theLoadSettingsDialogPtr->FilePathEdit -> setEnabled(false);
    theLoadSettingsDialogPtr->FileBrowseBtn-> setEnabled(false);
}

LoadSettingsDialog::~LoadSettingsDialog()
{
    delete theLoadSettingsDialogPtr;
}

void LoadSettingsDialog::on_UrlRadioBtn_clicked()
{
    theLoadSettingsDialogPtr->UrlRadioBtn  -> setChecked(true);
    theLoadSettingsDialogPtr->UrlEdit      -> setEnabled(true);
    theLoadSettingsDialogPtr->FileRadioBtn -> setChecked(false);
    theLoadSettingsDialogPtr->FilePathEdit -> setEnabled(false);
    theLoadSettingsDialogPtr->FileBrowseBtn-> setEnabled(false);
}

void LoadSettingsDialog::on_FileRadioBtn_clicked()
{
    theLoadSettingsDialogPtr->UrlRadioBtn  -> setChecked(false);
    theLoadSettingsDialogPtr->UrlEdit      -> setEnabled(false);
    theLoadSettingsDialogPtr->FileRadioBtn -> setChecked(true);
    theLoadSettingsDialogPtr->FilePathEdit -> setEnabled(true);
    theLoadSettingsDialogPtr->FileBrowseBtn-> setEnabled(true);
}

void LoadSettingsDialog::on_FileBrowseBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, ("Open " + resept::ProductName + " Client Customization File").c_str(), "", (resept::ProductName + " Client Customization Files (*.rccd)").c_str());
    theLoadSettingsDialogPtr->FilePathEdit->setText(QDir::toNativeSeparators(filePath));
}


void LoadSettingsDialog::on_OkBtn_clicked()
{
    std::vector<unsigned char> myRccdBlob;
    string myRccdUrl, myErrorMsg;

    if (theLoadSettingsDialogPtr->UrlEdit->isEnabled())
    {
        myRccdUrl = theLoadSettingsDialogPtr->UrlEdit->text().toStdString();
        WaitDialog myWaitDialog("Downloading settings...", this);
        if (!LoadSettingsBL::loadRccdFromUrl(myRccdUrl, myRccdBlob, myErrorMsg))
        {
            QMessageBox::warning(this, resept::ProductName.c_str(), myErrorMsg.c_str());
            return;
        }
    }
    else
    {
        myRccdUrl = theLoadSettingsDialogPtr->FilePathEdit->text().toStdString();
        if (!LoadSettingsBL::loadRccdFromFile(myRccdUrl, myRccdBlob, myErrorMsg))
        {
            QMessageBox::warning(this, resept::ProductName.c_str(), myErrorMsg.c_str());
            return;
        }
    }
    if (myRccdBlob.empty())
    {
        QMessageBox::warning(this, resept::ProductName.c_str(), ("Empty RCCD file received from " + myRccdUrl).c_str());
        return;
    }

    if (LoadSettingsBL::installRccd(myRccdBlob, myRccdUrl, confirmationPrompt, this, myErrorMsg))
    {
        QMessageBox::information(this, resept::ProductName.c_str(), "Customization settings have been successfully applied");
        accept();
        return;
    }
    QMessageBox::warning(this, resept::ProductName.c_str(), myErrorMsg.c_str());
}

void LoadSettingsDialog::on_CancelBtn_clicked()
{
    reject();
}

bool LoadSettingsDialog::confirmationPrompt(const std::string& aMsgText, void* aCookie)
{
    QWidget* myParent = (QWidget*)aCookie;
    return QMessageBox::warning (myParent, resept::ProductName.c_str(), aMsgText.c_str(), QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes;
}
