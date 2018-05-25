#include "ReseptConsoleClientCommon.h"
#include "ReseptConsoleClientDelegates.h"
#include "rclient/ReseptClientApp.h"
#include "rclient/Common.h"
#include "ta/process.h"
#include "ta/encodingutils.h"
#include "ta/common.h"

#include "boost/algorithm/string.hpp"
#include "boost/program_options.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

using std::string;


static void showHelp(po::options_description desc)
{
    std::cout << "Usage: " << rclient::ReseptConsoleClient << " [options]" << "\n" << desc << "\n";
}

static void showVersion()
{
    std::cout << std::endl << resept::ProductName << " client " << toStr(rclient::ClientVersion) << std::endl << "(C) " << resept::CompanyName << std::endl;
}

static string ubase64(const string& aB64Str)
{
    try
    {
        return ta::vec2Str(ta::EncodingUtils::fromBase64(aB64Str, true));
    }
    catch (std::exception&)
    {
        std::cerr << "'" << aB64Str << "' is not a valid base64 singleline-encoded string" << std::endl;
        throw;
    }
}

int main(int argc, char* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");

    try
    {
        // Init options
        desc.add_options()
        (rclient::HelpOpt, "produce this help message")
        (rclient::BatchModeOpt, "batch mode (default). In batch mode no interaction with a user is possible")
        (rclient::InteractiveModeOpt, "interactive mode. In interactive mode the app will prompt a user for actions when needed. For example during CR authentication or when AD password change prompt is received from the server")
        (rclient::ProviderOpt, po::value<string>(), ("provider name required if more than one provider exists in " + resept::ProductName + " settings").c_str())
        (rclient::ServiceOpt, po::value<string>(), ("service name required if more than one service for the given provider exists in " + resept::ProductName + " settings").c_str())
        (rclient::UserOpt, po::value<string>(), "")
        (rclient::B64UserOpt, po::value<string>(), str(boost::format("userid required if more than one user or no users for the given provider and service exists in %s settings. The alternative '%s' form expects singleline base64-encoded UTF-8 userid which is useful when userid contains non-ASCII symbols.") % resept::ProductName % rclient::B64UserOpt).c_str())
        (rclient::PasswordOpt, po::value<string>(), "")
        (rclient::B64PasswordOpt, po::value<string>(), str(boost::format("password if required by the service. The alternative '%s' form expects singleline base64-encoded UTF-8 password which is useful when password contains non-ASCII symbols.") % rclient::B64PasswordOpt).c_str())
        (rclient::NewPasswordOpt, po::value<string>(), "")
        (rclient::B64NewPasswordOpt, po::value<string>(), str(boost::format("New password if required by the service. The alternative '%s' form expects singleline base64-encoded UTF-8 password which is useful when password contains non-ASCII symbols.") % rclient::B64NewPasswordOpt).c_str())
        (rclient::PincodeOpt, po::value<string>(), "")
        (rclient::B64PincodeOpt, po::value<string>(), str(boost::format("pincode if required by the service. The alternative '%s' form expects singleline base64-encoded UTF-8 pincode which is useful when pincode contains non-ASCII symbols.") % rclient::B64PincodeOpt).c_str())
        (rclient::SavePfxOpt, str(boost::format("when the certificate is requested as pfx package, save received pfx and pfx password to %s%s and %s%s besides importing it to the personal store") % ta::Process::getTempDir() % rclient::PfxFileName % ta::Process::getTempDir() % rclient::PfxPassFileName).c_str())
        (rclient::ShowVersionOpt, "show the client version.")
        (rclient::CrFileOpt, po::value<string>(), "Path to the file containing answers to the subsequent prompts from the client otherwise handled interactively. Such prompts can be for example new SecurID tokencode or responses for GSM/UMTS authentication.")
        ;

        // Parse the command line
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count(rclient::HelpOpt))
        {
            showHelp(desc);
            return rclient::ReseptClientApp::exitSuccess;
        }

        if (vm.count(rclient::ShowVersionOpt))
        {
            showVersion();
            return rclient::ReseptClientApp::exitSuccess;
        }

        rclient::ReseptClientApp::Options myOptions;

        if (vm.count(rclient::ProviderOpt))
            myOptions.setProvider(vm[rclient::ProviderOpt].as<string>());

        myOptions.is_interactive = vm.count(rclient::InteractiveModeOpt) ? true: false;

        if (vm.count(rclient::ServiceOpt))
            myOptions.setService(vm[rclient::ServiceOpt].as<string>());

        if (vm.count(rclient::CrFileOpt))
            myOptions.setCrFile(vm[rclient::CrFileOpt].as<string>());

        if (vm.count(rclient::UserOpt) && vm.count(rclient::B64UserOpt))
        {
            std::cerr << "--" << rclient::UserOpt << " and --" << rclient::B64UserOpt << " cannot be specified both at the same time.\n";
            return rclient::ReseptClientApp::exitError;
        }
        if (vm.count(rclient::UserOpt))
            myOptions.setUserid(vm[rclient::UserOpt].as<string>());
        else if (vm.count(rclient::B64UserOpt))
            myOptions.setUserid(ubase64(vm[rclient::B64UserOpt].as<string>()));

        if (vm.count(rclient::PasswordOpt) && vm.count(rclient::B64PasswordOpt))
        {
            std::cerr << "--" << rclient::PasswordOpt << " and --" << rclient::B64PasswordOpt << " cannot be specified both at the same time.\n";
            return rclient::ReseptClientApp::exitError;
        }
        if (vm.count(rclient::PasswordOpt))
            myOptions.setPassword(vm[rclient::PasswordOpt].as<string>());
        else if (vm.count(rclient::B64PasswordOpt))
            myOptions.setPassword(ubase64(vm[rclient::B64PasswordOpt].as<string>()));


        if (vm.count(rclient::NewPasswordOpt))
            myOptions.setNewPassword(vm[rclient::NewPasswordOpt].as<string>());
        else if (vm.count(rclient::B64NewPasswordOpt))
            myOptions.setNewPassword(ubase64(vm[rclient::B64NewPasswordOpt].as<string>()));

        if (vm.count(rclient::PincodeOpt))
            myOptions.setPincode(vm[rclient::PincodeOpt].as<string>());
        else if (vm.count(rclient::B64PincodeOpt))
            myOptions.setPincode(ubase64(vm[rclient::B64PincodeOpt].as<string>()));


        // here we go!
        rclient::ReseptClientApp myApp(myOptions, &myOptions);
        return myApp.requestCertificate(
                   onPasswordPrompt,
                   onPincodePrompt,
                   onResponsePrompt,
                   onChangePasswordPrompt,
                   onUserMessages,
                   onAuthenticationDelayed,
                   onAuthenticationUserLocked,
                   vm.count(rclient::SavePfxOpt) ? onSavePfx : NULL,
                   onSavePem,
                   onNotify,
                   onError
               );

    }
    catch (rclient::ReseptClientAppInitError& e)
    {
        std::cerr << e.userError() << std::endl;
        return rclient::ReseptClientApp::exitError;
    }
    catch (boost::program_options::error&)
    {
        showHelp(desc);
        return rclient::ReseptClientApp::exitError;
    }
    catch (...)
    {
        return rclient::ReseptClientApp::exitError;
    }
}

