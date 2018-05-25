#pragma once

#include <QDialog>

namespace Ui { class LoadSettingsDialogClass; }

class LoadSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    LoadSettingsDialog(QWidget* parent = 0);
    ~LoadSettingsDialog();

private slots:
    void on_UrlRadioBtn_clicked();
    void on_FileRadioBtn_clicked();
    void on_FileBrowseBtn_clicked();
    void on_OkBtn_clicked();
    void on_CancelBtn_clicked();
private:
    static bool confirmationPrompt(const std::string& aMsgText, void* aCookie);
private:
    Ui::LoadSettingsDialogClass* theLoadSettingsDialogPtr;
};
