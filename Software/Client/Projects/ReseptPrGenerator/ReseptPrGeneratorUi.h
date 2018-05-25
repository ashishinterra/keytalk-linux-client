#pragma once

#include <QtWidgets>
#include "ui_ReseptPrGenerator.h"
#include <string>
#include <vector>

class PrGeneratorUi : public QDialog
{
    Q_OBJECT

public:
    PrGeneratorUi(QWidget* parent = 0);
    ~PrGeneratorUi();
public slots:
    void on_generateButton_clicked();

private:
    Ui::PrGeneratorClass ui;
};
