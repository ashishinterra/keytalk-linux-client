#include "ReseptPrGenerator.h"
#ifdef _WIN32
#include "ReseptPrGeneratorUi.h"
#endif
#include "resept/common.h"
#include "ta/utils.h"
#include "ta/logconfiguration.h"
#include "ta/utils.h"

#ifdef _WIN32
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#endif
#include <string>
#include <iostream>

// Usage on Windows:
//     ReseptPrGenerator - run in GUI mode
//     ReseptPrGenerator out-pr-path - run in GUI-less mode and writes the result to out-pr-path
//     (ReseptPrGenerator out-pr-path) 2>stderr-file - run in GUI-less mode, writes the result to out-pr-path and errors to stderr-file
//     @note because the app is not attached to the console one cannot check error codes with %errorlevel%
// Usage on Linux:
//     ktprgen - run in GUI-less mode and writes the result to ~/keytalk.clnt.pr.dat

#ifdef _WIN32
int main_win32(int argc, char* argv[])
{
    // GUI-less version
    // @note the reason stderr should be explicitly redirected to a file is because the app is linked with SYBSYSTEML:WINDOWS and thus does not have a console
    if (argc == 2)
    {
        try
        {
            const std::string myOutPrPath = argv[1];
            PrGenerator::generate(myOutPrPath);
            std::cout << "PR successfully saved to " << myOutPrPath
                      << "\nPlease email this report along with your problem description to " + resept::SupportEmail << "\n";
            return 0;
        }
        catch (std::exception& e)
        {
            std::cerr << "Error occurred. Please contact "<< resept::ProductName << " administrator. " << e.what() << "\n";
            return 1;
        }
        catch (...)
        {
            std::cerr << "Unexpected error occurred. Please contact " << resept::ProductName << " administrator.\n";
            return 2;
        }
    }

    // GUI mode

    QApplication a(argc, argv);

    try
    {
        PrGeneratorUi w;
        w.show();
        return a.exec();
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(NULL, "Error", ("Error occurred. Please contact " + resept::ProductName + "administrator. " + std::string(e.what())).c_str());
        return 1;
    }
    catch (...)
    {
        QMessageBox::warning(NULL, "Error", ("Unexpected error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
        return 2;
    }
}
#else
int main_posix(int UNUSED(argc), char** UNUSED(argv))
{
    try
    {
        const std::string myOutPrPath = PrGenerator::getSavePath();
        PrGenerator::generate(myOutPrPath);
        std::cout << "PR successfully saved to " << myOutPrPath
                  << "\nPlease email this report along with your problem description to " + resept::SupportEmail << "\n";
        return 0;
    }
    catch (std::exception& e)
    {
        std::cerr << "Error occurred. Please contact "<< resept::ProductName << " administrator. " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unexpected error occurred. Please contact " << resept::ProductName << " administrator.\n";
        return 2;
    }
}
#endif


int main(int argc, char* argv[])
{
#ifdef _WIN32
    return main_win32(argc, argv);
#else
    return main_posix(argc, argv);
#endif
}

