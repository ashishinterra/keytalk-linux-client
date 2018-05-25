#include "rclient/Settings.h"
#ifdef _WIN32
#include "rclient/TaskSettings.h"
#endif
#include "rclient/CommonUtils.h"
#include "rclient/Common.h"
#include "ta/process.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/logger.h"
#include "ta/common.h"

#include <string>
#include <iostream>
#include <vector>
#include <string>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using boost::format;
using boost::str;
using std::string;

namespace
{
    enum ExitCode
    {
        exitSuccess = 0,
        exitError,
        exitIllegalCommand,
        exitLoggerInitError,
        exitKeyTalkNotInstalledError,
        exitUnknownError,
        exitInvalidTask
    };

    // just quick&dirty check
    //@nothrow
    bool isReseptInstalled()
    {
        try
        {
            rclient::Settings::getReseptInstallDir();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

class LogInitializer
{
public:
    LogInitializer()
    {
        try
        {
            string myLogDir;
            try  { myLogDir = ta::Process::getTempDir(); }
            catch (std::runtime_error&) {}
            string myLogFileName = myLogDir + rclient::ConfigToolLogFileName;
            string myAppName;
            try { myAppName = ta::Process::getSelfShortName(); }
            catch (ta::ProcessGetNameError&) {}

            ta::LogConfiguration::Config myMemConfig;
            myMemConfig.fileAppender = true;
            myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
            myMemConfig.fileAppenderLogFileName = myLogFileName;
            myMemConfig.consoleAppender = true;
            myMemConfig.consoleAppenderLogThreshold = ta::LogLevel::Warn;
            myMemConfig.consoleAppenderOutDev = ta::LogConfiguration::conDevStdErr;
            ta::LogConfiguration::instance().load(myMemConfig);

            PROLOG(boost::format("%s Client-%s Configuration Tool") % resept::ProductName % toStr(rclient::ClientVersion));
        }
        catch (rclient::LoggerInitError&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(rclient::LoggerInitError, e.what());
        }
    }
    ~LogInitializer()
    {
        EPILOG(boost::format("%s Client-%s Configuration Tool") % resept::ProductName % toStr(rclient::ClientVersion));
    }
};

enum ShouldExit
{
    doNotExit = 0,
    doExit
};
void printUsage(const string& aProgramName)
{
    // The main users of this configuration tool are scripts.
    // Some commands in this tool do not have a 1 to 1 mapping to settings found in rclient::Settings.
    // The main consideration in this is that scripts can easier recombine information (e.g. append host:port)
    // then to split information up (e.g. parsing a host and port from a string)

    cout << "Usage:" << endl;
#ifdef _WIN32
    cout << aProgramName << " task list" << endl;
    cout << aProgramName << " task getparam <taskname> <parametername>" << endl;
    cout << aProgramName << " task validate <taskname>" << endl;
#endif
    cout << aProgramName << " provider list" << endl;
    cout << aProgramName << " provider getparam <providername> <parametername>" << endl;
    cout << aProgramName << " service list <providername>" << endl;
    cout << aProgramName << " service getparam <providername> <servicename> <parametername>" << endl;
    cout << aProgramName << " cert is-revoked <cert-path>" << endl;
}

bool equal(const string& paramValue, const string& expectedParamvalue)
{
    return boost::trim_copy(paramValue) == boost::trim_copy(expectedParamvalue);
}

template<typename T>
void printValue(const T& aValue)
{
    cout << aValue;
}

void printLn()
{
    cout << endl;
}

int main(int argc, char *argv[])
{
    LogInitializer myLogInitializer;
    using namespace rclient::Settings;
#ifdef _WIN32
    using namespace rclient::Settings::IISTaskParameters;
#endif
    if (!isReseptInstalled())
    {
        std::cerr << resept::ProductName + " is not installed. Please install " + resept::ProductName + " before configuring it" << std::endl;
        return exitKeyTalkNotInstalledError;
    }

    try
    {
        const vector<string> argvec(argv, argv + argc); // First argument program name, parameters are 1-based
        const unsigned int myNumArgs = argvec.size() - 1;

        if (false) // Added to have consistent structure (only "else if") in #ifdefs below
        {
        }
#ifdef _WIN32
        else if (myNumArgs == 2 && equal(argvec[1], "task") && equal(argvec[2], "list"))
        {
            // task list
            foreach(const string& task, getTaskNames(IISTask))
            {
                printValue(task);
                printLn();
            }
        }
        else if (myNumArgs == 4 && equal(argvec[1], "task") && equal(argvec[2], "getparam"))
        {
            const string myTaskName = boost::trim_copy(argvec[3]);
            const string myParamName = boost::trim_copy(argvec[4]);

            if (!isTaskExists(IISTask, myTaskName))
            {
                ERRORLOG(format("Cannot find task '%s'") % myTaskName);
                return exitError;
            }

            if (myParamName == rclient::Settings::TaskEnabled)
            {
                printValue(getTaskEnabled(myTaskName));
            }
            else if (myParamName == ScriptLogFilePath)
            {
                printValue(getScriptLogFilePath(myTaskName));
            }
            else if (myParamName == EmailFrom)
            {
                printValue(getEmailFrom(myTaskName));
            }
            else if (myParamName == EmailTo)
            {
                printValue(getEmailTo(myTaskName));
            }
            else if (myParamName == SmtpServer)
            {
                printValue(getSmtpServer(myTaskName));
            }
            else if (myParamName == EmailSubject)
            {
                printValue(getEmailSubject(myTaskName));
            }
            else if (myParamName == EmailReporting)
            {
                printValue(getEmailReporting(myTaskName));
            }
            else if (myParamName == SendEmailOnSuccess)
            {
                printValue(getSendEmailOnSuccess(myTaskName));
            }
            else if (myParamName == SendEmailIfApplyNotRequired)
            {
                printValue(getSendEmailIfApplyNotRequired(myTaskName));
            }
            else if (myParamName == HttpsBindingIp)
            {
                printValue(getHttpsBindingIp(myTaskName));
            }
            else if (myParamName == HttpsBindingPort)
            {
                printValue(getHttpsBindingPort(myTaskName));
            }
            else if (myParamName == KeyTalkProvider)
            {
                printValue(getKeyTalkProvider(myTaskName));
            }
            else if (myParamName == KeyTalkService)
            {
                printValue(getKeyTalkService(myTaskName));
            }
            else if (myParamName == KeyTalkUser)
            {
                printValue(getKeyTalkUser(myTaskName));
            }
            else if (myParamName == KeyTalkPassword)
            {
                printValue(getKeyTalkPassword(myTaskName));
            }
            else if (myParamName == CertificateStore)
            {
                printValue(getCertificateStore(myTaskName));
            }
            else if (myParamName == ShouldRemoveOldCertificate)
            {
                printValue(getShouldRemoveOldCertificate(myTaskName));
            }
            else
            {
                std::string myErrorMsg = str(format("Unknown task parameter '%s' for task '%s'") % myParamName % myTaskName);
                ERRORLOG2(myErrorMsg, myErrorMsg);
                return exitError;
            }
        }
        else if (myNumArgs == 3 && equal(argvec[1], "task") && equal(argvec[2], "validate"))
        {
            const string myTaskName = argvec[3];
            if(isValidIISTask(myTaskName)) {
                cout << "valid";
            }
            else {
                cout << "invalid";
                return exitInvalidTask;
            }
        }
#endif
        else if (myNumArgs == 2 && equal(argvec[1], "provider") && equal(argvec[2], "list"))
        {
            foreach (const string& providerName, rclient::Settings::getInstalledProviders())
            {
                printValue(providerName);
                printLn();
            }
        }
        else if (myNumArgs == 4 && equal(argvec[1], "provider") && equal(argvec[2], "getparam"))
        {
            const string myProviderName = boost::trim_copy(argvec[3]);
            const string myParamName = boost::trim_copy(argvec[4]);

            if (!ta::isElemExist(myProviderName, rclient::Settings::getInstalledProviders()))
            {
                ERRORLOG(format("Cannot find provider '%s'") % myProviderName);
                return exitError;
            }

            if (myParamName == "ServerHost")
            {
                ta::NetUtils::RemoteAddress myAddress = rclient::Settings::getReseptSvrAddress(myProviderName);
                printValue(myAddress.host);
            }
            else if (myParamName == "ServerPort")
            {
                ta::NetUtils::RemoteAddress myAddress = rclient::Settings::getReseptSvrAddress(myProviderName);
                printValue(myAddress.port);
            }
            else
            {
                const std::string myErrorMsg(str(format("Unknown task parameter '%s' for provider '%s'") % myParamName % myProviderName));
                ERRORLOG2(myErrorMsg, myErrorMsg);
                return exitError;
            }
        }
        else if (myNumArgs == 3 && equal(argvec[1], "service") && equal(argvec[2], "list"))
        {
            const string myProviderName = boost::trim_copy(argvec[3]);
            if (!ta::isElemExist(myProviderName, rclient::Settings::getInstalledProviders()))
            {
                ERRORLOG(format("Cannot find provider '%s'") % myProviderName);
                return exitError;
            }

            foreach (const string& serviceName, rclient::Settings::getServices(myProviderName))
            {
                printValue(serviceName);
                printLn();
            }
        }
        else if (myNumArgs == 5 && equal(argvec[1], "service") && equal(argvec[2], "getparam"))
        {
            const string myProviderName = boost::trim_copy(argvec[3]);
            const string myServiceName = boost::trim_copy(argvec[4]);
            const string myParamName = boost::trim_copy(argvec[5]);

            if (!ta::isElemExist(myProviderName, rclient::Settings::getInstalledProviders()))
            {
                ERRORLOG(format("Cannot find provider '%s'") % myProviderName);
                return exitError;
            }

            if (!ta::isElemExist(myServiceName, rclient::Settings::getServices(myProviderName)))
            {
                ERRORLOG(format("Cannot find service '%s'") % myServiceName);
                return exitError;
            }

            if (myParamName == rclient::Settings::getCertValidPercentParamName())
            {
                const unsigned int myCertValidPercent = getCertValidPercentage(myProviderName, myServiceName);
                printValue(myCertValidPercent);
            }
            else
            {
                const std::string myErrorMsg(str(format("Unknown task parameter '%s' for service '%s' of provider '%s'") % myParamName % myServiceName % myProviderName));
                ERRORLOG2(myErrorMsg, myErrorMsg);
                return exitError;
            }
        }
        else if (myNumArgs == 3 && equal(argvec[1], "cert") && equal(argvec[2], "is-revoked"))
        {
            const string myCertPath = boost::trim_copy(argvec[3]);

            string myWarnings;
            if (ta::CertUtils::isCertFileRevoked(myCertPath, &myWarnings))
            {
                printValue("revoked");
            }
            else
            {
                printValue("valid");
            }
            if (!myWarnings.empty())
            {
                WARNDEVLOG(myWarnings);
            }
        }
        else
        {
            printUsage(argvec[0]);
            return exitIllegalCommand;
        }
    }
#ifdef _WIN32
    catch (rclient::Settings::TaskSettingsError& e)
    {
        ERRORLOG2(format("Error while executing %s configuration tool. '%s'") % resept::ProductName % e.friendlyMessage(), e.what());
        return exitError;
    }
#endif
    catch (std::exception& e)
    {
        ERRORLOG2(format("Internal error while executing %s configuration tool.") % resept::ProductName, e.what());
        return exitError;
    }
    return exitSuccess;
}
