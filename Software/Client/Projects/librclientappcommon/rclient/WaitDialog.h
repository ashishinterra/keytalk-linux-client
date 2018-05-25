//----------------------------------------------------------------------------
//
//  Name          WaitDialog.h
//  Description : WaitDialog class declaration
//                WaitDialog class displays an splash-type wait dialog
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_WAITDIALOG_H
#define RCLIENT_WAITDIALOG_H

#include <string>

class QDialog;
class QLabel;
class QWidget;

class WaitDialog
{
public:
    explicit WaitDialog(const std::string& aText, QWidget* parent = NULL, bool aCenterOnDesktop = false);
    ~WaitDialog();
    void setText(const std::string& aText);
private:
    void centerOnDesktop();
private:
    const bool theCenterOnDesktop;
    static const std::string theTextTempl;
    QDialog* theDialog;
    QLabel* theText;
    QWidget* theParent;
};

#endif // WAITDIALOG_H
