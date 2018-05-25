#include "AboutDialog.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "ta/sysinfo.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/logger.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QMessageBox(parent)
{
    const std::string clientDesc = str(boost::format("%s %s © %s\n\n%s") %
                                       resept::ProductName %
                                       toStr(rclient::ClientVersion) %
                                       resept::CompanyName %
                                       ta::SysInfo::getHardwareDescription());
    init(clientDesc);
}

AboutDialog::AboutDialog(const std::string& aClientServiceUri, QWidget* parent)
    : QMessageBox(parent)
{
    const std::string clientDesc = str(boost::format("%s %s © %s\n\n%s\n\nService URI: %s") %
                                       resept::ProductName %
                                       toStr(rclient::ClientVersion) %
                                       resept::CompanyName %
                                       ta::SysInfo::getHardwareDescription() %
                                       aClientServiceUri);
    init(clientDesc);
}

AboutDialog::~AboutDialog()
{}

void AboutDialog::init(const std::string& clientDesc)
{
    setText(clientDesc.c_str());
    addButton(tr("OK"), QMessageBox::AcceptRole);
    reportProblemButton = addButton(tr("Report Problem..."), QMessageBox::ActionRole);
}

void AboutDialog::reportProblem()
{
    unsigned int prGeneratorExitCode;
    std::string prGeneratorExecutablePath(boost::str(boost::format("\"%s%s%s\"") % rclient::Settings::getReseptInstallDir() % ta::getDirSep() % rclient::ReseptPrGenerator));
    DEBUGLOG(boost::str(boost::format("Starting PR Generator from \"%s\"") % prGeneratorExecutablePath));
    bool prGeneratorFinished = ta::Process::shellExecAsync(prGeneratorExecutablePath, prGeneratorExitCode);
    if (prGeneratorFinished && prGeneratorExitCode != 0)
    {
        std::string errorMessage("Problem Report Generator tool could not be started.");
        ERRORLOG(boost::str(boost::format("%s PR generator path: \"%s\". Return code: %d") % errorMessage  % prGeneratorExecutablePath % prGeneratorExitCode));
        QMessageBox::warning(this, "Error starting PR tool", errorMessage.c_str());
    }
}
