#pragma once

#include "ta/InternetExplorer.h"
#include "cxxtest/TestSuite.h"

class InternetExplorerTest : public CxxTest::TestSuite
{
public:
    void testInstallDir()
    {
        if (ta::InternetExplorer::isInstalled())
        {
            const std::string myInstallDir = ta::InternetExplorer::getInstallDir();
            TS_ASSERT(!myInstallDir.empty());
        }
        else
        {
            TS_ASSERT_THROWS(ta::InternetExplorer::getInstallDir(), std::exception);
        }
    }

    void testVersion()
    {
        if (ta::InternetExplorer::isInstalled())
        {
            ta::InternetExplorer::Version myVersion = ta::InternetExplorer::getVersion();
        }
        else
        {
            TS_ASSERT_THROWS(ta::InternetExplorer::getVersion(), std::exception);
        }
    }

    void testProtectedMode()
    {
        TS_ASSERT_EQUALS(ta::InternetExplorer::getProtectedMode(), ta::InternetExplorer::protectedModeNotIeProcess);
        TS_ASSERT_THROWS(ta::InternetExplorer::getProtectedModeTempDir(), std::exception);
    }

};
