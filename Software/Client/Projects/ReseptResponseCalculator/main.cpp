#include "ReseptResponseCalculator.h"
#include "resept/common.h"
#include "resept/util.h"
#include "ta/utils.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <string>
#include <iostream>

//@note see http://doc.qt.nokia.com/4.6/appicon.html how to setup app icon of different platforms

// Usage:
// ReseptResponseCalculator - run in GUI mode
// or
// ReseptResponseCalculator username challenge response-out-file-path - run in GUI-less mode and writes the results to response-out-file-path
// (ReseptResponseCalculator username challenge response-out-file-path) 2>stderr-file - run in GUI-less mode, writes the results to response-out-file-path and errors to stderr-file
// @return 0 on success, <>0 otherwise
// @note because the app is not attached to the console one cannot check error codes with %errorlevel%
int main(int argc, char* argv[])
{
    // GUI-less version
    // @note the reason stderr should be explicitly redirected to a file is because the app is linked with SYBSYSTEML:WINDOWS and thus does not have a console
    if (argc == 4)
    {
        try
        {
            const std::string myUserId = argv[1];
            const std::string myChallenge = argv[2];
            const std::string myResponse = resept::calcResponse(myUserId, myChallenge);
            ta::writeData(argv[3], myResponse);
            return 0;
        }
        catch (std::exception& e)
        {
            std::cerr << "Error occurred. Please contact " << resept::ProductName << " administrator. " << e.what() << "\n";
            return 1;
        }
        catch (...)
        {
            std::cerr << "Unexpected error occurred. Please contact " << resept::ProductName << " administrator.\n";
            return 2;
        }
    }

    // GUI version
    QApplication a(argc, argv);

    try
    {
        ResponseCalculator w;
        w.show();
        return a.exec();
    }
    catch (std::exception&)
    {
        QMessageBox::warning(NULL, "Error", ("Error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
        return 1;
    }
    catch (...)
    {
        QMessageBox::warning(NULL, "Error", ("Unexpected error occurred. Please contact " + resept::ProductName + " administrator.").c_str());
        return 2;
    }
}
