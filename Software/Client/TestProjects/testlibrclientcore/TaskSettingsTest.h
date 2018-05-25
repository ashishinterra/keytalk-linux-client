#ifndef TaskSettingsTest_H
#define TaskSettingsTest_H

#include "rclient/TaskSettings.h"
#include "rclient/Common.h"
#include "ta/utils.h"
#include "cxxtest/TestSuite.h"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/assign/std/vector.hpp"
#include <string>
#include <vector>

using namespace std;
using namespace ta;
using rclient::Settings::TaskNames;
using rclient::Settings::TaskSettingsError;

namespace TaskSettingsTestUtils
{
    static void removeFile(const std::string& aFile)
    {
        remove(aFile.c_str());
        if (ta::isFileExist(aFile))
            TA_THROW_MSG(std::runtime_error, "Failed to remove file " + aFile);
    }

} // TaskSettingsTestUtils

class TaskSettingsTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
        namespace fs = boost::filesystem;
        fs::copy_file("tasks.ini.orig", "tasks.ini", fs::copy_option::overwrite_if_exists);
        rclient::Settings::setTaskConfigPath("tasks.ini");
        CxxTest::setAbortTestOnFail(false);
    }
    void tearDown()
    {
        try
        {
            rclient::Settings::resetTaskConfigPath();
            TaskSettingsTestUtils::removeFile("tasks.ini");
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
        }
        catch(...)
        {
            TS_TRACE("tearDown() failed with unknown error");
        }
    }

    void test_file_automatically_created_when_task_created()
    {
        // Given no task.ini
        TaskSettingsTestUtils::removeFile("tasks.ini");
        TS_ASSERT(!ta::isFileExist("tasks.ini"));

        // When adding a task
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");

        // Then the task file exists
        TS_ASSERT(ta::isFileExist("tasks.ini"));
    }

    void test_iis_task_creation()
    {
        // Given no IIS tasks
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));

        // When creating tasks
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask2");

        // Then the tasks exist
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));
    }

    void test_adding_an_existing_task_fails()
    {
        // Given a task
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));

        // When/Then
        TS_ASSERT_THROWS(rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask"), TaskSettingsError);
    }

    void test_adding_a_task_with_empty_name_fails()
    {
        // Given/When/Then
        TS_ASSERT_THROWS(rclient::Settings::addTask(rclient::Settings::IISTask, ""), TaskSettingsError);
        TS_ASSERT_THROWS(rclient::Settings::addTask(rclient::Settings::IISTask, "   "), TaskSettingsError);
        TS_ASSERT_THROWS(rclient::Settings::addTask(rclient::Settings::IISTask, "  \t  \n "), TaskSettingsError);
    }

    void test_whitespace_is_stripped_from_taskname()
    {
        // Given
        const string myNameWithSpaces("  TestTask  ");
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, myNameWithSpaces));
        const string myNameWithoutSpaces("TestTask");
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, myNameWithoutSpaces));

        // When
        rclient::Settings::addTask(rclient::Settings::IISTask, myNameWithSpaces);

        // Then
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, myNameWithSpaces));
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, myNameWithoutSpaces));
    }

    void test_removing_a_nonexisting_task_fails()
    {
        // Given a taskname which does not exist
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));

        // When/Then
        TS_ASSERT_THROWS(rclient::Settings::removeTask(rclient::Settings::IISTask, "TestTask"), TaskSettingsError);
    }

    void test_iis_task_removal()
    {
        // Given two IIS tasks
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask2");

        // When removing one of the tasks
        rclient::Settings::removeTask(rclient::Settings::IISTask, "TestTask");

        // Then only the removed task is gone
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));
    }

    void test_task_queries()
    {
        // Initially the task list is empty
        TaskNames expectedTaskNames = boost::assign::list_of("ValidTask")("InvalidTask");
        TS_ASSERT_EQUALS(rclient::Settings::getTaskNames(rclient::Settings::IISTask), expectedTaskNames);
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));

        // Adding one task gives a list of one task
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");
        expectedTaskNames = boost::assign::list_of("ValidTask")("InvalidTask")("TestTask");
        TS_ASSERT_EQUALS(rclient::Settings::getTaskNames(rclient::Settings::IISTask), expectedTaskNames);
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));

        // Adding another task gives a list of two tasks
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask2");
        expectedTaskNames = boost::assign::list_of("ValidTask")("InvalidTask")("TestTask")("TestTask2");
        TS_ASSERT_EQUALS(rclient::Settings::getTaskNames(rclient::Settings::IISTask), expectedTaskNames);
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));

        // Removing one task again gives a list of one task
        rclient::Settings::removeTask(rclient::Settings::IISTask, "TestTask");
        expectedTaskNames = boost::assign::list_of("ValidTask")("InvalidTask")("TestTask2");
        TS_ASSERT_EQUALS(rclient::Settings::getTaskNames(rclient::Settings::IISTask), expectedTaskNames);
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        TS_ASSERT(rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask2"));
    }

    void test_getting_iis_task_parameters_without_default_values_throws_exception()
    {
        using namespace rclient::Settings::IISTaskParameters;
        // Given a freshly created task "TestTask"
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");

        // When retrieving non-defaulted values, then an exception is thrown
        TS_ASSERT_THROWS(getKeyTalkProvider("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getKeyTalkService("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getKeyTalkUser("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getKeyTalkPassword("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getEmailFrom("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getEmailTo("TestTask"), rclient::Settings::TaskSettingsError);
        TS_ASSERT_THROWS(getSmtpServer("TestTask"), rclient::Settings::TaskSettingsError);
    }

    void test_generic_task_parameter_default_values()
    {
        using namespace rclient::Settings;
        TS_ASSERT_EQUALS(getTaskEnabled("TestTask"), true);
    }

    void test_set_generic_task_parameter_values()
    {
        using namespace rclient::Settings;

        // Given an IIS task without specific parameters set
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");

        // When/Then
        // We test the following for each task parameter:
        // Attempt to set the parameter with valid  (non-default) data
        // Expect: current value equals the set value
        // If validation is possible, attempt to set the parameter with invalid data
        // Expect: exception is thrown and current value is the last "good" value
        bool myValidBool;

        myValidBool = false;
        setTaskEnabled("TestTask", myValidBool);
        TS_ASSERT_EQUALS(getTaskEnabled("TestTask"), myValidBool);
    }

    void test_iis_parameter_default_values()
    {
        using namespace rclient::Settings::IISTaskParameters;
        // Given a freshly created task "TestTask"
        TS_ASSERT(!rclient::Settings::isTaskExists(rclient::Settings::IISTask, "TestTask"));
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");

        // When retrieving defaulted value, the default is returned
        TS_ASSERT_EQUALS(getScriptLogFilePath("TestTask"), ta::Process::getTempDir() + "keytalk_task_TestTask.log");

        TS_ASSERT_EQUALS(getEmailReporting("TestTask"), false);

        TS_ASSERT_EQUALS(getSendEmailOnSuccess("TestTask"), true);

        TS_ASSERT_EQUALS(getSendEmailIfApplyNotRequired("TestTask"), false);

        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), "0.0.0.0");

        TS_ASSERT_EQUALS(getHttpsBindingPort("TestTask"), 443);

        TS_ASSERT_EQUALS(getCertificateStore("TestTask"), "My");

        TS_ASSERT_EQUALS(getShouldRemoveOldCertificate("TestTask"), true);

        TS_ASSERT_EQUALS(getEmailSubject("TestTask"), "KeyTalk IIS certificate update");
    }

    void test_task_validity_check()
    {
        using namespace rclient::Settings::IISTaskParameters;
        TS_ASSERT(isValidIISTask("ValidTask"));
        TS_ASSERT(!isValidIISTask("InvalidTask"));

        TS_ASSERT(!rclient::Settings::isAllTasksValid());
    }

    void test_validator_functions()
    {
        using namespace rclient::Settings;
        using namespace rclient::Settings::IISTaskParameters;
        string myErrorMsg;

        myErrorMsg.clear();
        TS_ASSERT(isValidTaskName("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789_-",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(!isValidTaskName("invalid.task",  myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");


        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp("*",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp(" * ",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp("0.0.0.0",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp(" 0.0.0.0 ",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp("192.168.0.1",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp(" 192.168.0.1 ",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(!isValidHttpsBindingIp("312.168.0.1",  myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp(" [fe80::2550:bc94:d9e2:2153] ", myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(isValidHttpsBindingIp(" fe80::2550:bc94:d9e2:2153 ", myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(!isValidHttpsBindingIp(" ze80::2550:bc94:d9e2:2153 ", myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");

        myErrorMsg.clear();
        TS_ASSERT(!isValidHttpsBindingIp("thisIsNoIp",  myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");


        myErrorMsg.clear();
        TS_ASSERT(isValidScriptLogFilePath(ta::Process::getTempDir() + "mylogfile.txt",  myErrorMsg));
        TS_ASSERT_EQUALS(myErrorMsg, "");

        // log file may not be a directory
        myErrorMsg.clear();
        TS_ASSERT(!isValidScriptLogFilePath(ta::Process::getTempDir(),  myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");

        // parent directory must exist
        myErrorMsg.clear();
        TS_ASSERT(!isValidScriptLogFilePath(ta::Process::getTempDir() + "nonexistingdirectory" + ta::getDirSep() + "mylogfile.txt", myErrorMsg));
        TS_ASSERT_DIFFERS(myErrorMsg, "");
    }

    void test_set_iis_task_parameter()
    {
        using namespace rclient::Settings::IISTaskParameters;

        // Given an IIS task without specific parameters set
        rclient::Settings::addTask(rclient::Settings::IISTask, "TestTask");

        // When/Then
        // We test the following for each task parameter:
        // Attempt to set the parameter with valid  (non-default) data
        // Expect: current value equals the set value
        // If validation is possible, attempt to set the parameter with invalid data
        // Expect: exception is thrown and current value is the last "good" value

        string myValidString;
        string myInvalidString;
        unsigned int myValidUnsignedInt;
        unsigned int myInvalidUnsignedInt;
        bool myValidBool;

        myValidString = ta::Process::getTempDir() + "keytalk_task_TestTask.log";
        setScriptLogFilePath("TestTask", myValidString);
        TS_ASSERT_EQUALS(getScriptLogFilePath("TestTask"), myValidString);
        TS_ASSERT_THROWS(setScriptLogFilePath("TestTask", ta::Process::getTempDir() + "nonexistingdirectory\\keytalk_task_TestTask.log"), TaskSettingsError);
        TS_ASSERT_EQUALS(getScriptLogFilePath("TestTask"), myValidString);

        myValidString = "me@example.com";
        setEmailFrom("TestTask", myValidString);
        TS_ASSERT_EQUALS(getEmailFrom("TestTask"), myValidString);
        TS_ASSERT_THROWS(setEmailFrom("TestTask", "#@%^%#$@#$@#.com"), TaskSettingsError);
        TS_ASSERT_EQUALS(getEmailFrom("TestTask"), myValidString);

        myValidString = "me@example.com";
        setEmailTo("TestTask", myValidString);
        TS_ASSERT_EQUALS(getEmailTo("TestTask"), myValidString);
        TS_ASSERT_THROWS(setEmailTo("TestTask", "#@%^%#$@#$@#.com"), TaskSettingsError);
        TS_ASSERT_EQUALS(getEmailTo("TestTask"), myValidString);

        myValidString = "smtp.example.com";
        setSmtpServer("TestTask", myValidString);
        TS_ASSERT_EQUALS(getSmtpServer("TestTask"), myValidString);
        TS_ASSERT_THROWS(setSmtpServer("TestTask", "example..com"), TaskSettingsError);
        TS_ASSERT_EQUALS(getSmtpServer("TestTask"), myValidString);

        myValidString = "[Test] E-mail subject";
        setEmailSubject("TestTask", myValidString);
        TS_ASSERT_EQUALS(getEmailSubject("TestTask"), myValidString);

        setEmailReporting("TestTask", true);
        TS_ASSERT_EQUALS(getEmailReporting("TestTask"), true);

        setSendEmailOnSuccess("TestTask", false);
        TS_ASSERT_EQUALS(getSendEmailOnSuccess("TestTask"), false);

        setSendEmailIfApplyNotRequired("TestTask", true);
        TS_ASSERT_EQUALS(getSendEmailIfApplyNotRequired("TestTask"), true);

        myValidString = "0.0.0.0";
        setHttpsBindingIp("TestTask", myValidString);
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);

        myValidString = "192.168.0.1";
        setHttpsBindingIp("TestTask", myValidString);
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);

        myValidString = "::1";
        setHttpsBindingIp("TestTask", myValidString);
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);

        myValidString = "[fe80::2550:bc94:d9e2:2153]";
        setHttpsBindingIp("TestTask", myValidString);
        myValidString = "fe80::2550:bc94:d9e2:2153"; // Brackets should be stripped (required for IIS:// filesystem in powershell)
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);

        myValidString = "*";
        setHttpsBindingIp("TestTask", myValidString);
        myValidString = "0.0.0.0"; // * should be converted to 0.0.0.0
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);
        TS_ASSERT_THROWS(setHttpsBindingIp("TestTask", "invalid.ip"), TaskSettingsError);
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);
        TS_ASSERT_THROWS(setHttpsBindingIp("TestTask", "192.168.0.312"), TaskSettingsError);
        TS_ASSERT_EQUALS(getHttpsBindingIp("TestTask"), myValidString);

        myValidUnsignedInt = 65535;
        myInvalidUnsignedInt = 65536;
        setHttpsBindingPort("TestTask", myValidUnsignedInt);
        TS_ASSERT_EQUALS(getHttpsBindingPort("TestTask"), myValidUnsignedInt);
        TS_ASSERT_THROWS(setHttpsBindingPort("TestTask", myInvalidUnsignedInt), TaskSettingsError);
        TS_ASSERT_EQUALS(getHttpsBindingPort("TestTask"), myValidUnsignedInt);

        myValidString = "TestProvider";
        setKeyTalkProvider("TestTask", myValidString);
        TS_ASSERT_EQUALS(getKeyTalkProvider("TestTask"), myValidString);
        TS_ASSERT_THROWS(setKeyTalkProvider("TestTask", "Invalid+Provider"), TaskSettingsError);
        TS_ASSERT_EQUALS(getKeyTalkProvider("TestTask"), myValidString);

        myValidString = "TEST_SERVICE";
        setKeyTalkService("TestTask", myValidString);
        TS_ASSERT_EQUALS(getKeyTalkService("TestTask"), myValidString);
        TS_ASSERT_THROWS(setKeyTalkService("TestTask", "invalid$service"), TaskSettingsError);
        TS_ASSERT_EQUALS(getKeyTalkService("TestTask"), myValidString);

        myValidString = "TestUser";
        setKeyTalkUser("TestTask", myValidString);
        TS_ASSERT_EQUALS(getKeyTalkUser("TestTask"), myValidString);
        TS_ASSERT_THROWS(setKeyTalkUser("TestTask", ""), TaskSettingsError);
        TS_ASSERT_EQUALS(getKeyTalkUser("TestTask"), myValidString);

        myValidString = "password!@#$$^%&*()_+[]'.,";
        setKeyTalkPassword("TestTask", myValidString);
        TS_ASSERT_EQUALS(getKeyTalkPassword("TestTask"), myValidString);
        TS_ASSERT_THROWS(setKeyTalkPassword("TestTask", "toolongpasswordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpassword"), TaskSettingsError);
        TS_ASSERT_EQUALS(getKeyTalkPassword("TestTask"), myValidString);

        myValidString = "My";
        setCertificateStore("TestTask", myValidString);
        TS_ASSERT_EQUALS(getCertificateStore("TestTask"), myValidString);
        TS_ASSERT_THROWS(setCertificateStore("TestTask", "SomeNonExistingStore"), TaskSettingsError);
        TS_ASSERT_EQUALS(getCertificateStore("TestTask"), myValidString);

        myValidBool = false;
        setShouldRemoveOldCertificate("TestTask", myValidBool);
        TS_ASSERT_EQUALS(getShouldRemoveOldCertificate("TestTask"), myValidBool);
    }
};

#endif
