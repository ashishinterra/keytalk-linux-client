#ifndef TaskUtilsTest_H
#define TaskUtilsTest_H

#include "rclient/TaskUtils.h"
#include "rclient/Common.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "cxxtest/TestSuite.h"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/format.hpp"
#include "boost/assign/std/vector.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/regex.hpp"
#include "boost/foreach.hpp"
#include <string>
#include <vector>
#include <iostream>

using namespace std;
using namespace ta;
using boost::format;
using boost::str;

class TaskUtilsTest : public CxxTest::TestSuite
{
private:
    string getTestKtUtilsModulePath()
    {
        return ".\\KeyTalkUtils.psm1";
    }

public:
    void test_execute_powershell_code_captures_stdout()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        TS_ASSERT_EQUALS(rclient::executePowerShellCode("Write-Host 'Standard Output'", myStdOut, myStdErr), 0);

        // Then
        TS_ASSERT_EQUALS(myStdOut, "Standard Output");
        TS_ASSERT_EQUALS(myStdErr, "");
    }

    void test_execute_powershell_code_captures_stderr()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        // Not using Write-Error for writing to stderr, because this adds the error linkage to stderr
        TS_ASSERT_EQUALS(rclient::executePowerShellCode("$host.ui.WriteErrorLine('Standard Error')", myStdOut, myStdErr), 0);

        // Then
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_EQUALS(myStdErr, "Standard Error");
    }

    void test_execute_powershell_code_captures_return_code()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        TS_ASSERT_EQUALS(rclient::executePowerShellCode("throw 'some error'", myStdOut, myStdErr), 1);

        // Then
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_DIFFERS(myStdErr, "");
    }

    void test_execute_existing_powershell_function_in_script_succeeds()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        TS_ASSERT_EQUALS(rclient::executePowerShellCode(boost::filesystem::path("test.psm1").string(), "PrintHello 'TaskUtilsTest'", myStdOut, myStdErr), 0);

        // Then
        TS_ASSERT_EQUALS(myStdOut, "Hello TaskUtilsTest");
        TS_ASSERT_EQUALS(myStdErr, "");
    }

    void test_import_keytalk_task_utils_module_succeeds_without_warnings()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        const int myRetVal = rclient::executePowerShellCode(getTestKtUtilsModulePath(), "", myStdOut, myStdErr);

        // Then
        if (myStdOut != "")
        {
            // Extra diagnostics to clarify test failure reason
            string myStd;
            string myErr;
            TS_ASSERT_EQUALS(rclient::executePowerShellCode(str(format("Import-Module -name \"%s\" -verbose") % getTestKtUtilsModulePath()), myStd, myErr), 0);

            vector<string> myWarnings;
            myStd = boost::replace_all_copy(myStd, "\r", "");
            cout << endl << myStd << endl;

        }
        TS_ASSERT_EQUALS(myRetVal, 0);
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_EQUALS(myStdErr, "");
    }

    void test_execute_nonexisting_powershell_function_in_script_fails()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When
        TS_ASSERT_EQUALS(rclient::executePowerShellCode(boost::filesystem::path("test.psm1").string(), "Nonexisting-Function 'TaskUtilsTest'", myStdOut, myStdErr), 1);

        // Then
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_DIFFERS(myStdErr, "");
    }

    void test_using_function_from_nonexisting_script_throws()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When, Then
        TS_ASSERT_THROWS(rclient::executePowerShellCode(boost::filesystem::path("nonexisting.psm1").string(), "Write-Host 'Hello'", myStdOut, myStdErr), std::invalid_argument);
    }

    void test_get_from_nonexisting_ini_file_fails()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When, Then
        TS_ASSERT_DIFFERS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "GetStringFromIniFile 'nonexisting.ini' 'SomeValue'", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_DIFFERS(myStdErr, "");
    }

    void test_get_nonexisting_value_from_ini_file_fails()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When, Then
        TS_ASSERT_DIFFERS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "GetStringFromIniFile 'resept.ini' 'SomeValue'", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_DIFFERS(myStdErr, "");
    }

    void test_get_string_value_from_ini_file_succeeds()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When, Then
        TS_ASSERT_EQUALS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "GetStringFromIniFile 'resept.ini' 'Install'", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "C:\\\\Program Files\\\\keytalk");
        TS_ASSERT_EQUALS(myStdErr, "");
    }

    void test_string_to_bool()
    {
        // Given
        string myStdOut;
        string myStdErr;

        // When, Then
        TS_ASSERT_EQUALS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "StringToBool '0'", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "False");
        TS_ASSERT_EQUALS(myStdErr, "");

        TS_ASSERT_EQUALS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "StringToBool '1'", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "True");
        TS_ASSERT_EQUALS(myStdErr, "");


        TS_ASSERT_DIFFERS(rclient::executePowerShellCode(getTestKtUtilsModulePath(), "try { StringToBool '0notanumber0'} catch { Write-Error $error }", myStdOut, myStdErr), 0);
        TS_ASSERT_EQUALS(myStdOut, "");
        TS_ASSERT_DIFFERS(myStdErr, "");
    }
};

#endif
