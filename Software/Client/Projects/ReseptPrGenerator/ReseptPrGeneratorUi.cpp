#include "ReseptPrGeneratorUi.h"
#include "ReseptPrGenerator.h"
#include "LogInitializer.h"
#include "rclient/CommonUtils.h"
#include "rclient/Common.h"
#include "ta/Zip.h"
#include "ta/process.h"
#include "ta/utils.h"

#include <QFileDialog>

using std::string;

PrGeneratorUi::PrGeneratorUi(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setWindowTitle("PR Generator");
    ui.textLabel->setText(("Click \"Generate...\" to generate the recent " + resept::ProductName + " Client activity report and save it to a file. ").c_str());
}

PrGeneratorUi::~PrGeneratorUi()
{}

void PrGeneratorUi::on_generateButton_clicked()
{
    try
    {
        LogInitializer myLogInitializer;
        const string myTempDirPath = ta::Process::getTempDir() + "ktprgenerator";
        const std::string myPrFilePath = PrGenerator::getSavePath();

        try
        {
            // Prepare files
            const ta::StringArray myFileList = PrGenerator::preparePrFiles(myTempDirPath);
            QString myOutDilePath = QFileDialog::getSaveFileName(this,
                                    ("Save " + resept::ProductName + " Client Problem Report File").c_str(),
                                    myPrFilePath.c_str(),
                                    (resept::ProductName + " Client Problem Report Files").c_str());
            if (myOutDilePath.size())
            {
                ta::Zip::archive(QDir::toNativeSeparators(myOutDilePath).toStdString(), myFileList, ta::Zip::makeStem);
                QMessageBox::information(this, "Problem report saved", ("Problem report has been successfully saved. Please email this report along with your problem description to " + resept::SupportEmail).c_str());
            }
        }
        catch (...)
        {
            PrGenerator::safeRemoveDir(myTempDirPath);
            throw;
        }
        PrGenerator::safeRemoveDir(myTempDirPath);
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(NULL, "Error", ("Error occurred. Please contact " + resept::ProductName + " administrator. " + std::string(e.what())).c_str());
    }
    catch (...)
    {
        QMessageBox::warning(NULL, "Error", ("Unexpected error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
    }
}
