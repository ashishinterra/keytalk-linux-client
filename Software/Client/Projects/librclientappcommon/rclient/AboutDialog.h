#ifndef ABOUT_DIALOG_H
#define ABOUT_DIALOG_H

#include <QtWidgets/QMessageBox>
#include <QPushButton>
#include <string>

class AboutDialog : public QMessageBox
{
public:
    AboutDialog(QWidget* parent);
    AboutDialog(const std::string& aClientServiceUri, QWidget* parent);
    ~AboutDialog();
    void reportProblem();
    QPushButton* reportProblemButton;
private:
    void init(const std::string& clientDesc);
};

#endif

