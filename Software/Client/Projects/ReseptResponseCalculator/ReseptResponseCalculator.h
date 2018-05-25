#pragma once

#include <QtWidgets>
#include "ui_ReseptResponseCalculator.h"
#include <string>


class ResponseCalculator : public QDialog
{
    Q_OBJECT

public:
    ResponseCalculator(QWidget* parent = 0);
    ~ResponseCalculator();
public slots:
    void on_calcButton_clicked();
    void on_gsmCalcButton_clicked();
    void on_umtsCalcButton_clicked();
    void on_otpCalcButton_clicked();
private:
    static std::string getUiValue(const QLineEdit* aCtrl);
    static void setUiValue(QLineEdit* aCtrl, const std::string& aVal);
private:
    Ui::ResponseCalculatorClass ui;
};
