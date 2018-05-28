#pragma once

#include "ta/libconfigwrapper.h"
#include "cxxtest/TestSuite.h"

class LibConfigWrapperTestBase : public CxxTest::TestSuite
{
protected:
    enum ConfigFileEncoding
    {
        encodingUtf8NoBom, encodingUtf8WithBom
    };

    void tearDown()
    {
        remove(ConfigFileName.c_str());
    }

    void createSettingsFile(ConfigFileEncoding aConfigFileEncoding)
    {
        std::ofstream mySettingsFile;

        if (aConfigFileEncoding == encodingUtf8WithBom)
        {
            static const string Utf8Bom = "\xEF\xBB\xBF";
            mySettingsFile.open(ConfigFileName.c_str(), std::ios::out | std::ios::trunc | ios::binary);
            mySettingsFile << Utf8Bom;

            mySettingsFile.close();
            mySettingsFile.open(ConfigFileName.c_str(), std::ios::out | std::ios::app);
        }
        else
        {
            mySettingsFile.open(ConfigFileName.c_str(), std::ios::out | std::ios::trunc);
        }

        CxxTest::setAbortTestOnFail(true);
        TS_ASSERT(mySettingsFile.is_open());
        CxxTest::setAbortTestOnFail(false);

        std::string myContent;

        myContent += "TestVersion = \"0.2\";\n";
        myContent += "\n";
        myContent += "TestGroup :\n";
        myContent += "{\n";
        myContent += "  TestGroupParam1 = \"TestGroupParam1ValueString для юникод теста\";\n";
        myContent += "  TestGroupParam2 = 1234;\n";
        myContent += "  TestGroupParam3 = true;\n";
        myContent += "};\n";
        myContent += "\n";
        myContent += "TestList = (\n";
        myContent += "{ TestListParam11 = \"TestListParam11ValueString для юникод теста\"; TestListParam12 = 5678; TestListParam13 = true; },\n";
        myContent += "{ TestListParam21 = \"TestListParam21ValueString для юникод теста\"; TestListParam22 = 9012; TestListParam23 = false; },\n";
        myContent += "{ TestListParam31 = \"TestListParam31ValueString для юникод теста\"; TestListParam32 = 3456; TestListParam33 = true; }\n";
        myContent += ");\n";
        myContent += "TestArray = [\"TestArrayElem1\", \"TestArrayUnicodeElem2 для юникод теста\"];\n";
        myContent += "\n";
        mySettingsFile << myContent;
    }

    void _test_that_settings_can_be_read()
    {
        ta::LibConfigWrapper myConfig(ConfigFileName);

        //
        // Test get string value
        //

        std::string myTestString;
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam1", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestGroupParam1ValueString для юникод теста");
        myTestString = "some value";

        TS_ASSERT(!myConfig.getValue("TestGroup.TestGroupParamX", myTestString, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "some value");
        TS_ASSERT_THROWS(myConfig.getValue("TestGroup.TestGroupParamX", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);

        TS_ASSERT(myConfig.getValue("TestList.[1].TestListParam21", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestListParam21ValueString для юникод теста");
        TS_ASSERT_THROWS(myConfig.getValue("TestList.[1].TestListParamX", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);

        myTestString = "";
        TS_ASSERT(myConfig.getValue("TestArray.[0]", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestArrayElem1");
        TS_ASSERT(myConfig.getValue("TestArray.[1]", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestArrayUnicodeElem2 для юникод теста");
        TS_ASSERT_THROWS(myConfig.getValue("TestArray.[2]", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);

        myTestString = "some value";
        TS_ASSERT(!myConfig.getValue("TestArray.TestListParamX", myTestString, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "some value");


        //
        // Test get integer value
        //

        int myTestInt=0;
        TS_ASSERT(!myConfig.getValue("TestGroup.TestGroupParamX", myTestInt, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 0);
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam2", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 1234);
        TS_ASSERT_THROWS(myConfig.getValue("TestGroup.TestGroupParamX", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);

        myTestInt=0;
        TS_ASSERT(!myConfig.getValue("TestList.[1].TestListParamX", myTestInt, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 0);
        TS_ASSERT(myConfig.getValue("TestList.[1].TestListParam22", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 9012);
        TS_ASSERT_THROWS(myConfig.getValue("TestList.[1].TestListParamX", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test get boolean value
        //

        bool myTestBool=false;
        TS_ASSERT(!myConfig.getValue("TestGroup.TestGroupParamX", myTestBool, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, false);
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam3", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, true);
        TS_ASSERT_THROWS(myConfig.getValue("TestGroup.TestGroupParamX", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);

        myTestBool=true;
        TS_ASSERT(!myConfig.getValue("TestList.[1].TestListParamX", myTestBool, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, true);
        TS_ASSERT(myConfig.getValue("TestList.[1].TestListParam23", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, false);
        TS_ASSERT_THROWS(myConfig.getValue("TestList.[1].TestListParamX", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test get string array value
        //
        std::vector<std::string> myTestStringArray;
        TS_ASSERT(!myConfig.getValue("UnknownSetting", myTestStringArray, ta::LibConfigWrapper::settingGetTolerateIfNotExist));
        TS_ASSERT_EQUALS(myTestStringArray, std::vector<std::string>());
        TS_ASSERT_THROWS(myConfig.getValue("UnknownSetting", myTestStringArray, ta::LibConfigWrapper::settingGetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT(myConfig.getValue("TestArray", myTestStringArray, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestStringArray, boost::assign::list_of("TestArrayElem1")("TestArrayUnicodeElem2 для юникод теста"));

        //
        // Test get list info
        //
        size_t myTestListLength = 0;
        TS_ASSERT(myConfig.getListInfo("TestList", myTestListLength));
        TS_ASSERT_EQUALS(myTestListLength, 3U);
        TS_ASSERT_THROWS(myConfig.getListInfo("TestListX", myTestListLength), ta::LibConfigWrapperError);
    }

    void _test_that_existing_setting_can_be_set()
    {
        ta::LibConfigWrapper myConfig(ConfigFileName);

        //
        // Test set string value
        //

        std::string myTestString;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam1", " TestGroupParam1NewValueString для юникод теста   ", ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam1", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, " TestGroupParam1NewValueString для юникод теста   ");
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam1", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist, ta::LibConfigWrapper::wsStripYes));
        TS_ASSERT_EQUALS(myTestString, "TestGroupParam1NewValueString для юникод теста");
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam1", 123, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam1", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParamX", "TestGroupParamXNewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam1", "TestGroupParam1NewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[2].TestListParam31", "TestListParam31NewValueString для юникод теста", ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[2].TestListParam31", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestListParam31NewValueString для юникод теста");
        TS_ASSERT_THROWS(myConfig.setValue("TestList.[2].TestListParamX", "TestListParamXNewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[2].TestListParam31", "TestListParam31NewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestArray.[0]", "TestArrayNewUnicodeElem1 для юникод теста", ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestArray.[0]", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestArrayNewUnicodeElem1 для юникод теста");
        TS_ASSERT_THROWS(myConfig.setValue("TestArray.[2]", "TestArrayNewUnicodeElem3 для юникод теста", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test set integer value
        //

        int myTestInt=0;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam2", 1122, ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam2", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 1122);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam2", "string value", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam2", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParamX", 3344, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam2", 4455, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        myTestInt=0;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[2].TestListParam32", 5566, ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[2].TestListParam32", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 5566);
        TS_ASSERT_THROWS(myConfig.setValue("TestList.[2].TestListParamX", 6677, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[2].TestListParam32", 7788, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test set boolean value
        //

        bool myTestBool=true;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam3", false, ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam3", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, false);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam3", "string value", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParam3", 123, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroup.TestGroupParamX", true, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam3", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        myTestBool=true;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[2].TestListParam33", false, ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[2].TestListParam33", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, false);
        TS_ASSERT_THROWS(myConfig.setValue("TestList.[2].TestListParamX", true, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[2].TestListParam33", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test set string array value
        //

        std::vector<std::string> myStringArray = boost::assign::list_of("TestArrayElem2")("TestArrayUnicodeElem3 для юникод теста")("");
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestArray", myStringArray, ta::LibConfigWrapper::settingSetFailIfNotExist));
        std::vector<std::string> myStringArrayActual;
        TS_ASSERT(myConfig.getValue("TestArray", myStringArrayActual, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myStringArrayActual, myStringArray);

        myStringArray.clear();
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestArray", myStringArray, ta::LibConfigWrapper::settingSetFailIfNotExist));
        TS_ASSERT(myConfig.getValue("TestArray", myStringArrayActual, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myStringArrayActual, myStringArray);

        TS_ASSERT_THROWS(myConfig.setValue("NewSetting", myStringArray, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
    }

    void _test_that_non_existing_setting_can_be_added()
    {
        ta::LibConfigWrapper myConfig(ConfigFileName);

        //
        // Test add string value
        //
        std::string myTestString;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam4", "TestGroupParam4NewValueString для юникод теста", ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam4", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestGroupParam4NewValueString для юникод теста");
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup2.TestSubGroup.TestGroupParam4", "TestGroupParam4NewValueString для юникод теста", ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup2.TestSubGroup.TestGroupParam4", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestGroupParam4NewValueString для юникод теста");
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam5", "TestGroupParam5NewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[2].TestListParam41", "TestListParam41NewValueString для юникод теста", ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[2].TestListParam41", myTestString, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestString, "TestListParam41NewValueString для юникод теста");
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[2].TestListParam51", "TestListParam51NewValueString", ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        // we do not support inserting elements into an array
        TS_ASSERT_THROWS(myConfig.setValue("TestArray.[2]", "TestArrayNewUnicodeElem3 для юникод теста", ta::LibConfigWrapper::settingSetCreateIfNotExist), ta::LibConfigWrapperError);


        //
        // Test add integer value
        //

        int myTestInt=0;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam6", 9900, ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam6", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 9900);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam7", 0011, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        myTestInt=0;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[1].TestListParam62", 2233, ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[1].TestListParam62", myTestInt, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestInt, 2233);
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[1].TestListParam72", 4455, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);


        //
        // Test add boolean value
        //
        bool myTestBool=false;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestGroup.TestGroupParam8", true, ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestGroup.TestGroupParam8", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, true);
        TS_ASSERT_THROWS(myConfig.setValue("TestGroupX.TestGroupParam9", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        myTestBool=false;
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("TestList.[0].TestListParam83", true, ta::LibConfigWrapper::settingSetCreateIfNotExist));
        TS_ASSERT(myConfig.getValue("TestList.[0].TestListParam83", myTestBool, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myTestBool, true);
        TS_ASSERT_THROWS(myConfig.setValue("TestListX.[0].TestListParam93", false, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);

        //
        // Test set string array value
        //

        std::vector<std::string> myStringArray = boost::assign::list_of("TestArrayElem2")("TestArrayUnicodeElem3 для юникод теста")("");
        TS_ASSERT_THROWS(myConfig.setValue("NewSetting", myStringArray, ta::LibConfigWrapper::settingSetFailIfNotExist), ta::LibConfigWrapperError);
        TS_ASSERT_THROWS_NOTHING(myConfig.setValue("NewSetting", myStringArray, ta::LibConfigWrapper::settingSetCreateIfNotExist));
        std::vector<std::string> myStringArrayActual;
        TS_ASSERT(myConfig.getValue("NewSetting", myStringArrayActual, ta::LibConfigWrapper::settingGetFailIfNotExist));
        TS_ASSERT_EQUALS(myStringArrayActual, myStringArray);
    }

    void _test_that_setting_can_be_removed()
    {
        ta::LibConfigWrapper myConfig(ConfigFileName);

        //
        // Test remove string setting
        //
        TS_ASSERT(myConfig.isStringSettingExist("TestGroup.TestGroupParam1"));
        myConfig.removeSetting("TestGroup.TestGroupParam1");
        TS_ASSERT(!myConfig.isStringSettingExist("TestGroup.TestGroupParam1"));

        TS_ASSERT(myConfig.isStringSettingExist("TestList.[2].TestListParam31"));
        myConfig.removeSetting("TestList.[2].TestListParam31");
        TS_ASSERT(!myConfig.isStringSettingExist("TestList.[2].TestListParam31"));


        //
        // Test remove integer setting
        //
        TS_ASSERT(myConfig.isIntSettingExist("TestGroup.TestGroupParam2"));
        myConfig.removeSetting("TestGroup.TestGroupParam2");
        TS_ASSERT(!myConfig.isIntSettingExist("TestGroup.TestGroupParam2"));

        TS_ASSERT(myConfig.isIntSettingExist("TestList.[1].TestListParam22"));
        myConfig.removeSetting("TestList.[1].TestListParam22");
        TS_ASSERT(!myConfig.isIntSettingExist("TestList.[1].TestListParam22"));


        //
        // Test remove boolean setting
        //
        TS_ASSERT(myConfig.isBoolSettingExist("TestGroup.TestGroupParam3"));
        myConfig.removeSetting("TestGroup.TestGroupParam3");
        TS_ASSERT(!myConfig.isBoolSettingExist("TestGroup.TestGroupParam3"));

        TS_ASSERT(myConfig.isBoolSettingExist("TestList.[0].TestListParam13"));
        myConfig.removeSetting("TestList.[0].TestListParam13");
        TS_ASSERT(!myConfig.isBoolSettingExist("TestList.[0].TestListParam13"));

        //
        // Test remove string array setting
        //
        TS_ASSERT(myConfig.isStringArraySettingExist("TestArray"));
        myConfig.removeSetting("TestArray");
        TS_ASSERT(!myConfig.isStringArraySettingExist("TestArray"));

        //
        // Test remove non-existing setting
        //
        TS_ASSERT(!myConfig.isStringSettingExist("NonExistingSetting"));
        myConfig.removeSetting("NonExistingSetting");
        TS_ASSERT(!myConfig.isStringSettingExist("NonExistingSetting"));

    }
private:
    static const std::string ConfigFileName;
};

const std::string LibConfigWrapperTestBase::ConfigFileName("configtestfile.conf");

//
// Test suites
//

// Test when config file is in UTF-8 without BOM marker
class LibConfigWrapperTestUtf8NoBom : public LibConfigWrapperTestBase
{
public:
    void setUp()
    {
        createSettingsFile(encodingUtf8NoBom);
    }

    void test_that_settings_can_be_read()
    {
        _test_that_settings_can_be_read();
    }

    void test_that_existing_setting_can_be_set()
    {
        _test_that_existing_setting_can_be_set();
    }

    void test_that_non_existing_setting_can_be_added()
    {
        _test_that_non_existing_setting_can_be_added();
    }

    void test_that_setting_can_be_removed()
    {
        _test_that_setting_can_be_removed();
    }
};

// Test when config file is in UTF-8 with BOM marker
class LibConfigWrapperTestUtf8WithBom : public LibConfigWrapperTestBase
{
public:
    void setUp()
    {
        createSettingsFile(encodingUtf8WithBom);
    }

    void test_that_settings_can_be_read()
    {
        _test_that_settings_can_be_read();
    }

    void test_that_existing_setting_can_be_set()
    {
        _test_that_existing_setting_can_be_set();
    }

    void test_that_non_existing_setting_can_be_added()
    {
        _test_that_non_existing_setting_can_be_added();
    }

    void test_that_setting_can_be_removed()
    {
        _test_that_setting_can_be_removed();
    }
};
