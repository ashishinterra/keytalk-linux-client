#ifdef _WIN32
#include "ReseptConfigManagerUi.h"
#endif
#include "ReseptConfigManagerNoUi.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "rclient/CommonUtils.h"
#include "resept/common.h"
#include "ta/process.h"
#include "ta/strings.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/common.h"

#ifdef _WIN32
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#endif
#include "boost/program_options.hpp"
#include <string>
#include <sstream>
#include <iostream>

using std::string;
namespace po = boost::program_options;


//@note see http://doc.qt.nokia.com/4.6/appicon.html how to setup app icon of different platforms
//@note because the app is not attached to the console one cannot check error codes with %errorlevel%
//@note we do not support base64 username/password because these arguments go from vbs script (not from console) so thay can be properly encoded

namespace
{
    enum ExitCode
    {
        exitSuccess = 0,
        exitError,
        exitLoggerInitError,
        exitKeyTalkNotInstalledError,
        exitUnknownError
    };

    // just quick&dirty check
    //@nothrow
    bool isReseptInsalled()
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

    class LogInitializer
    {
    public:
        enum AppType {appWithUi, appNoUi};

        LogInitializer(AppType anAppType)
            : theAppType(anAppType)
        {
            try
            {
                string myLogDir;
                try  { myLogDir = ta::Process::getTempDir();}
                catch (std::runtime_error&) {}
                string myLogFileName = myLogDir + rclient::ConfigManagerLogFileName;
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

                PROLOG(boost::format("%s Client-%s Configuration Manager (%s)") % resept::ProductName % toStr(rclient::ClientVersion) % (theAppType==appWithUi?"UI":"no UI"));
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
            EPILOG(boost::format("%s Client-%s Configuration Manager (%s)") % resept::ProductName % toStr(rclient::ClientVersion) % (theAppType==appWithUi?"UI":"no UI"));
        }
    private:
        AppType theAppType;
    };

#ifdef _WIN32
    int execUi(int argc, char* argv[])
    {
        LogInitializer myLogInitializer(LogInitializer::appWithUi);
        QApplication app(argc, argv);
        try
        {
            ReseptConfigManagerUi myReseptConfigManager;
            return myReseptConfigManager.exec() == 0 ? exitSuccess : exitError;
        }
        catch (std::exception& e)
        {
            ERRORLOG2("Error occurred", e.what());
            QMessageBox::warning(NULL, "Error", ("Error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
            return exitError;
        }
        catch (...)
        {
            ERRORLOG("Unknown error occurred");
            QMessageBox::warning(NULL, "Error", ("Unexpected error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
            return exitUnknownError;
        }
    }
#endif

    void logErrorUsage(const po::options_description& aDesc)
    {
        std::stringstream ss;
        ss << aDesc;
        ERRORLOG("\nUsage:\n" + rclient::ReseptConfigManager + " - run in GUI mode\n" +
                 rclient::ReseptConfigManager + " options\n" + ss.str());
    }

    int execNoUi(int argc, char* argv[])
    {
        LogInitializer myLogInitializer(LogInitializer::appNoUi);

        po::variables_map vm;
        po::options_description desc("Allowed options");

        try
        {
            // Init options
            desc.add_options()
            (RccdPathOpt.c_str(), po::value<string>(), "location of RCCD file specified as URL or as an absolute file path")
#ifdef _WIN32
            (TasksIniPathOpt.c_str(), po::value<string>(), "location of tasks.ini file specified as an absolute file path. Only usable after Resept customization.")
#endif
            (AllowDowngradeOpt.c_str(), "allow installing RCCDs containing older version of the settings than already installed")
            (InteractiveModeOpt.c_str(), str(boost::format("allow interactive mode when the app will prompt a user for actions when needed. For example confirmation for settings downgrade unless --%s is supplied.") % AllowDowngradeOpt).c_str())
            ;

            // Parse the args
            po::store(po::parse_command_line(argc, argv, desc), vm);
            po::notify(vm);

            if (vm.count(RccdPathOpt))
            {
                const bool myAllowDowngrade = vm.count(AllowDowngradeOpt) ? true : false;
                const bool myInteractiveMode = vm.count(InteractiveModeOpt) ? true : false;
                const string myRccdPath = vm[RccdPathOpt].as<string>();
                if (ReseptConfigManagerNoUi().installRccd(myRccdPath, myAllowDowngrade, myInteractiveMode))
                {
                    return exitSuccess;
                }
                else
                {
                    return exitError;
                }
            }

#ifdef _WIN32
            if (vm.count(TasksIniPathOpt))
            {
                if (!rclient::Settings::isCustomized())
                {
                    ERRORLOG("Cannot install tasks ini file when KeyTalk is not customized.");
                    return exitError;
                }

                const string myTasksIniPath = vm[TasksIniPathOpt].as<string>();
                return ReseptConfigManagerNoUi().installTasksIni(myTasksIniPath) ? exitSuccess : exitError;
            }
#endif

            logErrorUsage(desc);
            return exitError;
        }
        catch (boost::program_options::error&)
        {
            logErrorUsage(desc);
            return exitError;
        }
        catch (std::exception& e)
        {
            ERRORLOG2("Error occurred. Please contact RESEPT administrator.", e.what());
            return exitError;
        }
        catch (...)
        {
            ERRORLOG("Unexpected error occurred. Please contact RESEPT administrator.");
            return exitUnknownError;
        }
    }
}

int main(int argc, char* argv[])
{
    if (!isReseptInsalled())
    {
        std::cerr << resept::ProductName + " is not installed. Please install " + resept::ProductName + " before configuring it" << std::endl;
        return exitKeyTalkNotInstalledError;
    }

    try
    {
#ifdef _WIN32
        //@notice Windows users will neither see console output nor proper error code because this app is built
        // with /SUBSYSTEM:WINDOWS VC linker setting. If having a proper output is needed on Windows, a separate
        // console version of this app should be built with SUBSYSTEM:CONSOLE linker setting
        return (argc <= 1) ? execUi(argc, argv) : execNoUi(argc, argv);
#else
        return execNoUi(argc, argv);
#endif
    }
    catch (rclient::LoggerInitError& e)
    {
        std::cerr << str(boost::format("Failed to initialize logger. %s. Please contact %s administrator.") % e.what() % resept::ProductName) << std::endl;
        return exitLoggerInitError;
    }
    catch (...)
    {
        std::cerr << str(boost::format("Unknon error occurred. Please contact %s administrator.") % resept::ProductName) << std::endl;
        return exitUnknownError;
    }
}
