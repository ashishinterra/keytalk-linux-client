#pragma once

#include "ta/registry.h"
#include "cxxtest/TestSuite.h"

class RegistryTest : public CxxTest::TestSuite
{
public:
    void testRead()
    {
        try
        {
            string myKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
            TS_ASSERT(ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey, "ProductName"));
            string myStrVal;
            ta::Registry::read(HKEY_LOCAL_MACHINE, myKey, "ProductName", myStrVal);
            TS_ASSERT(!myStrVal.empty());
            TS_ASSERT(ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey, "InstallDate"));
            DWORD myDwVal;
            ta::Registry::read(HKEY_LOCAL_MACHINE, myKey, "InstallDate", myDwVal);

            TS_ASSERT(!ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey, "#NonExistentValName#"));
            TS_ASSERT_THROWS(ta::Registry::read(HKEY_LOCAL_MACHINE, myKey, "#NonExistentValName#", myStrVal), ta::RegistryError);
            TS_ASSERT(!ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey+"#NonExistingKeySuffix#", "ProductName"));
            TS_ASSERT_THROWS(ta::Registry::read(HKEY_LOCAL_MACHINE, myKey+"#NonExistingKeySuffix#", "ProductName", myStrVal), ta::RegistryError);
        }
        catch (ta::RegistryError& e)
        {
            TS_ASSERT(false);
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }
    }
    void testWrite()
    {
        try
        {
            std::string myOrigVal, myNewVal;
            const std::string myKeyPath = "Software\\Microsoft\\Internet Explorer\\TypedURLs";
            const std::string myKeyVal = "url1";


            if (ta::Registry::isExist(HKEY_CURRENT_USER, myKeyPath, myKeyVal))
                ta::Registry::read(HKEY_CURRENT_USER, myKeyPath, myKeyVal, myOrigVal);
            ta::Registry::write(HKEY_CURRENT_USER, myKeyPath, myKeyVal, "https://taregistrytest.com");
            ta::Registry::read(HKEY_CURRENT_USER, myKeyPath, myKeyVal, myNewVal);
            TS_ASSERT_EQUALS(myNewVal, "https://taregistrytest.com");
            ta::Registry::write(HKEY_CURRENT_USER, myKeyPath, myKeyVal, myOrigVal);
            ta::Registry::read(HKEY_CURRENT_USER, myKeyPath, myKeyVal, myNewVal);
            TS_ASSERT_EQUALS(myNewVal, myOrigVal);
        }
        catch (ta::RegistryError& e)
        {
            TS_ASSERT(false);
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }
    }
};
