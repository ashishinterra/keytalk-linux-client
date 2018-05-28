#pragma once

#include "ta/common.h"
#include "ta/dnsutils.h"
#include "ta/netutils.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include "boost/assign/list_of.hpp"
#include "boost/filesystem.hpp"

class DnsUtilsHostsFileTest : public CxxTest::TestSuite
{
public:
    static DnsUtilsHostsFileTest* createSuite()
    {
        return new DnsUtilsHostsFileTest();
    }
    static void destroySuite( DnsUtilsHostsFileTest *suite )
    {
        delete suite;
    }

    DnsUtilsHostsFileTest()
    {
        hostsfileFilePath = "/etc/hosts";
        hostsfileBackupFilePath = "/tmp/etc.hosts";
    }

    void setUp()
    {
        try
        {
            // Make a backup of the hostname file
            namespace fs = boost::filesystem;
            fs::copy_file(hostsfileFilePath, hostsfileBackupFilePath, fs::copy_option::overwrite_if_exists);
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
            throw;
        }
        catch(...)
        {
            TS_TRACE("setUp() failed with unknown error");
            throw;
        }
    }

    void tearDown()
    {
        try
        {
            // restore /etc/hosts from backup
            namespace fs = boost::filesystem;

            if (fs::exists(hostsfileBackupFilePath))
            {
                fs::copy_file(hostsfileBackupFilePath, hostsfileFilePath, fs::copy_option::overwrite_if_exists);
                fs::remove(hostsfileBackupFilePath);
            }
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


    void testIsValidHostsEntryWithIp4()
    {
        using namespace ta::DnsUtils;

        HostsFile::Entry hostEntry = HostsFile::Entry("127.0.0.1", "testHost1", "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::ok);
    }


    void testIsValidHostsEntryWithIp6()
    {
        using namespace ta::DnsUtils;

        HostsFile::Entry hostEntry = HostsFile::Entry("::1", "testHost1", "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::ok);
    }


    void testIsValidHostsEntryWithInvalidIpAddress()
    {
        using namespace ta::DnsUtils;

        string invalidIpAddress = "0.0.1";
        HostsFile::Entry hostEntry = HostsFile::Entry(invalidIpAddress, "testHost1", "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidIpAddress);
        TS_ASSERT_EQUALS(validationMsg, invalidIpAddress);
    }


    void testIsValidHostsEntryWithInvalidHostName()
    {
        using namespace ta::DnsUtils;

        const std::string invalidHostName = "{NotGood}";
        HostsFile::Entry hostEntry = HostsFile::Entry("::1", invalidHostName, "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidCharacter);
        TS_ASSERT_EQUALS(validationMsg, invalidHostName);
    }


    void testIsValidHostsEntryWithInvalidAlias()
    {
        using namespace ta::DnsUtils;

        const std::string invalidAlias = "{NotGood}";
        HostsFile::Entry hostEntry = HostsFile::Entry("::1", "testHost1", invalidAlias);

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidCharacter);
        TS_ASSERT_EQUALS(validationMsg, invalidAlias);
    }


    void testIsValidHostsEntryWithMultipleInvalidAlias()
    {
        using namespace ta::DnsUtils;


        const std::string invalidAlias = "{NotGood2}";
        const std::string aliases = "testAlias1 "+ invalidAlias + " testAlias3";
        HostsFile::Entry hostEntry = HostsFile::Entry("::1", "testHost1", invalidAlias);

        HostsFile::Entry::ValidationResult validationResult;
        std::string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidCharacter);
        TS_ASSERT_EQUALS(validationMsg, invalidAlias);
    }


    void testIsValidHostsEntryWithEmptyIpAddress()
    {
        using namespace ta::DnsUtils;

        HostsFile::Entry hostEntry = HostsFile::Entry("", "testHost1", "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        std::string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::emptyIpAddress);
        TS_ASSERT_EQUALS(validationMsg, "");
    }


    void testIsValidHostsEntryWithEmptyHostName()
    {
        using namespace ta::DnsUtils;

        HostsFile::Entry hostEntry = HostsFile::Entry("127.0.0.1", "", "testAlias1");

        HostsFile::Entry::ValidationResult validationResult;
        std::string validationMsg;
        TS_ASSERT(!hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::hostnameEmpty);
        TS_ASSERT_EQUALS(validationMsg, "");
    }


    void testIsValidHostsEntryWithEmptyAliases()
    {
        using namespace ta::DnsUtils;

        HostsFile::Entry hostEntry = HostsFile::Entry("127.0.0.1", "testHost1", "");

        HostsFile::Entry::ValidationResult validationResult;
        std::string validationMsg;
        TS_ASSERT(hostEntry.isValid(validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::ok);
        TS_ASSERT_EQUALS(validationMsg, "");
    }


    void testIsValidHostsFile()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries hostsFile;
        for (int hostsIndex = 0; hostsIndex < 5; hostsIndex++)
        {
            hostsFile.push_back(HostsFile::Entry(
                "1.2.3." + ta::Strings::toString(hostsIndex),
                "testHost" + ta::Strings::toString(hostsIndex),
                "testAlias" + ta::Strings::toString(hostsIndex))
            );
        }

        HostsFile::Entry::ValidationResult validationResult;
        std::string validationMsg;
        TS_ASSERT(HostsFile::isValid(hostsFile, validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::ok);
        TS_ASSERT_EQUALS(validationMsg, "");
    }


    void testIsValidHostsFileWithSingleLineAndInvalidIpAddress()
    {
        using namespace ta::DnsUtils;

        std::string invalidIpAddress = "2.3.4";
        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry(invalidIpAddress, "testHost1", "testAlias1"));

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!HostsFile::isValid(hostsFile, validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidIpAddress);
        TS_ASSERT_EQUALS(validationMsg, invalidIpAddress);
    }


    void testIsValidHostsFileWithSingleLineAndInvalidHostName()
    {
        using namespace ta::DnsUtils;

        std::string invalidHostName = "{NotGood2}";
        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("::1", invalidHostName, "testAlias1"));

        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!HostsFile::isValid(hostsFile, validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidCharacter);
        TS_ASSERT_EQUALS(validationMsg, invalidHostName);
    }


    void testIsValidHostsFileWithMultipleLinesAndInvalidIpAddress()
    {
        using namespace ta::DnsUtils;

        std::string invalidIpAddress = "2.3.4";
        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("1.2.3.4", "testHost1", "testAlias1"));
        hostsFile.push_back(HostsFile::Entry("2001:0db8:85a3:0042:1000:8a2e:0370:7334", "testHost2", "testAlias2"));
        hostsFile.push_back(HostsFile::Entry("3.3.3.3", "testHost3", ""));
        hostsFile.push_back(HostsFile::Entry(invalidIpAddress, "testHost", "testAlias"));
        hostsFile.push_back(HostsFile::Entry("5.5.5.5", "testHost5", "testAlias5"));
        HostsFile::Entry::ValidationResult validationResult;
        string validationMsg;
        TS_ASSERT(!HostsFile::isValid(hostsFile, validationResult, validationMsg));
        TS_ASSERT_EQUALS(validationResult, HostsFile::Entry::invalidIpAddress);
        TS_ASSERT_EQUALS(validationMsg, invalidIpAddress);
    }


    void testSaveValidHostsfileForSingleLine()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("1.2.3.4", "testhost1", "testalias1"));
        HostsFile::save(hostsFile);

        HostsFile::HostEntries hostsFileRead;
        hostsFileRead = HostsFile::load();

        TS_ASSERT_EQUALS(hostsFile, hostsFileRead);
    }


    void testSaveValidHostsfileForSingleLineWithMixedCase()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("1.2.3.4", "testHost1", "testAlias1"));
        HostsFile::save(hostsFile);

        HostsFile::HostEntries hostsFileRead;
        hostsFileRead = HostsFile::load();

        TS_ASSERT_DIFFERS(hostsFile, hostsFileRead);
    }


    void testSaveValidHostsfileForMultipleLines()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries hostsFile;
        for (int hostsIndex = 0; hostsIndex < 5; hostsIndex++)
        {
            hostsFile.push_back(HostsFile::Entry(
                "1.2.3." + ta::Strings::toString(hostsIndex),
                "testhost" + ta::Strings::toString(hostsIndex),
                "testalias" + ta::Strings::toString(hostsIndex))
            );
        }
        HostsFile::save(hostsFile);

        HostsFile::HostEntries hostsFileRead;
        hostsFileRead = HostsFile::load();

        TS_ASSERT_EQUALS(hostsFile, hostsFileRead);
    }


    void testSaveValidHostsfileForMultipleLinesWithEmptyLines()
    {
        using namespace ta::DnsUtils;

        string hostsFileAsString = "1.1.1.1\tHost1\n\n\n2.2.2.2\tHost2\n3.3.3.3\tHost3\n";
        ta::writeData(HostsFile::getPath(), hostsFileAsString);

        HostsFile::HostEntries hostsFileRead;
        hostsFileRead = HostsFile::load();

        TS_ASSERT_EQUALS(hostsFileRead.size(), 3);
    }


    void testSaveValidHostsfileForLineWithMultipleTabsAndSpaces()
    {
        using namespace ta::DnsUtils;

        string hostsFileAsString = "  \t  \t \t  ::1  \t\t\t  \t   localhost  \t\t\t\t   \taliase1  \t\t  \t  aliase2\n";
        ta::writeData(HostsFile::getPath(), hostsFileAsString);

        HostsFile::HostEntries hostsFileRead;
        hostsFileRead = HostsFile::load();

        TS_ASSERT_EQUALS(hostsFileRead.size(), 1);
        TS_ASSERT_EQUALS(hostsFileRead.at(0).ipAddress, "::1");
        TS_ASSERT_EQUALS(hostsFileRead.at(0).hostName, "localhost");
        TS_ASSERT_EQUALS(hostsFileRead.at(0).aliases, "aliase1 aliase2");
    }


    void testSaveInvalidHostsfileWithInvalidIpAddress()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries beforeHostsFile;
        beforeHostsFile = HostsFile::load();

        std::string ipAddress = "2.3.4";
        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("1.2.3.4", "testHost1", "testAlias1"));
        hostsFile.push_back(HostsFile::Entry("2001:0db8:85a3:0042:1000:8a2e:0370:7334", "testHost2", "testAlias2"));
        hostsFile.push_back(HostsFile::Entry("3.3.3.3", "testHost3", ""));
        hostsFile.push_back(HostsFile::Entry(ipAddress, "testHost", "testAlias"));
        hostsFile.push_back(HostsFile::Entry("5.5.5.5", "testHost5", "testAlias5"));
        try
        {
            HostsFile::save(hostsFile);
        }
        catch (HostsFile::HostsFileValidationError error)
        {
            TS_ASSERT_EQUALS(error.validationResult, HostsFile::Entry::invalidIpAddress);
            TS_ASSERT_EQUALS(error.validationData, ipAddress);
        }

        HostsFile::HostEntries afterHostsFile;
        afterHostsFile = HostsFile::load();
        TS_ASSERT_EQUALS(beforeHostsFile, afterHostsFile);
    }

    void testSaveInvalidHostsfileWithEmptyHostName()
    {
        using namespace ta::DnsUtils;

        HostsFile::HostEntries beforeHostsFile;
        beforeHostsFile = HostsFile::load();

        HostsFile::HostEntries hostsFile;
        hostsFile.push_back(HostsFile::Entry("1.2.3.4", "", "testAlias1"));
        hostsFile.push_back(HostsFile::Entry("2.3.4", "testHost2", "testAlias2"));
        try
        {
            HostsFile::save(hostsFile);
        }
        catch (HostsFile::HostsFileValidationError error)
        {
            TS_ASSERT_EQUALS(error.validationResult, HostsFile::Entry::hostnameEmpty);
            TS_ASSERT_EQUALS(error.validationData, "");
        }

        HostsFile::HostEntries afterHostsFile;
        afterHostsFile = HostsFile::load();
        TS_ASSERT_EQUALS(beforeHostsFile, afterHostsFile);
    }

private:
    std::string hostsfileFilePath;
    std::string hostsfileBackupFilePath;
};


class DnsUtilsNameServersTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        try
        {
            // backup network configuration
            namespace fs = boost::filesystem;
            foreach (const ta::StringDict::value_type& orig2backup, ConfFiles)
            {
                if (fs::exists(orig2backup.first))
                {
                    fs::copy_file(orig2backup.first, orig2backup.second, fs::copy_option::overwrite_if_exists);
                }
            }
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
            throw;
        }
        catch(...)
        {
            TS_TRACE("setUp() failed with unknown error");
            throw;
        }
    }

    void tearDown()
    {
        try
        {
            // restore the original network configuration
            namespace fs = boost::filesystem;
            foreach (const ta::StringDict::value_type& orig2backup, ConfFiles)
            {
                if (fs::exists(orig2backup.second))
                {
                    fs::copy_file(orig2backup.second, orig2backup.first, fs::copy_option::overwrite_if_exists);
                }
            }
            ta::Process::checkedShellExecSync("sudo resolvconf -u");
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


    void test_that_nameservers_can_be_read()
    {
        using namespace ta::DnsUtils;

        // when
        const ta::StringArray myOrigNameServers = loadNameServers();
        const ta::StringArray myOrigUserNameServers = loadNameServers(nsUserOnly);
        // then
        foreach (const std::string& ns, myOrigUserNameServers)
        {
            TS_ASSERT(ta::isElemExist(ns, myOrigNameServers));
        }
    }

    void test_that_nameservers_can_be_changed()
    {
#ifdef RESEPT_SERVER
        using namespace ta::DnsUtils;

        // when
        const ta::StringArray myOrigNameServers = loadNameServers();
        const ta::StringArray myOrigUserNameServers = loadNameServers(nsUserOnly);
        // then
        TS_TRACE(("All nameservers (original): " + ta::Strings::join(myOrigNameServers, ", ")).c_str());
        TS_TRACE(("User-managed nameservers (original): " + ta::Strings::join(myOrigUserNameServers, ", ")).c_str());

        // given
        ta::StringArray myNewNameServers = boost::assign::list_of("8.8.4.4")("2001:4860:4860::8888")("2001:4860:4860::8844");
        // when
        applyUserNameServers(myNewNameServers);
        // then
        ta::StringArray myActualNameServers = loadNameServers();
        ta::StringArray myActualUserNameServers = loadNameServers(nsUserOnly);
        // then
        TS_TRACE(("All nameservers (after change): " + ta::Strings::join(myActualNameServers, ", ")).c_str());
        TS_TRACE(("User-managed nameservers (after change): " + ta::Strings::join(myActualUserNameServers, ", ")).c_str());
        foreach (const std::string& ns, myNewNameServers)
        {
            TS_ASSERT(ta::isElemExist(ns, myActualNameServers));
        }
        TS_ASSERT(ta::equalIgnoreOrder(myActualUserNameServers, myNewNameServers));

        // given
        myNewNameServers = boost::assign::list_of("8.8.4.4")("2001:4860:4860::8888")("8.8.4.4")("2001:4860:4860::8888");
        // when (duplicate name servers)
        applyUserNameServers(myNewNameServers);
        // then
        myActualNameServers = loadNameServers();
        myActualUserNameServers = loadNameServers(nsUserOnly);
        // then
        TS_TRACE(("All nameservers (after change): " + ta::Strings::join(myActualNameServers, ", ")).c_str());
        TS_TRACE(("User-managed nameservers (after change): " + ta::Strings::join(myActualUserNameServers, ", ")).c_str());
        foreach (const std::string& ns, myNewNameServers)
        {
            TS_ASSERT(ta::isElemExist(ns, myActualNameServers));
        }
        TS_ASSERT(ta::equalIgnoreOrder(myActualUserNameServers, ta::removeDuplicates(myNewNameServers)));

       // when
       applyUserNameServers(myOrigNameServers);
       // then
       TS_ASSERT_EQUALS(loadNameServers(), myOrigNameServers);
       TS_ASSERT(loadNameServers(nsUserOnly).empty());

       // when
       applyUserNameServers(ta::StringArray());
       // then
       TS_ASSERT_EQUALS(loadNameServers(), myOrigNameServers);
       TS_ASSERT(loadNameServers(nsUserOnly).empty());
#else
       TS_SKIP("DNS altering test is for KeyTalk server only");
#endif
    }

private:
    static const ta::StringDict ConfFiles; // orig file : backup file
};

const ta::StringDict DnsUtilsNameServersTest::ConfFiles = boost::assign::map_list_of("/etc/resolv.conf", "/tmp/resolv.conf")
                                                                                       ("/etc/resolvconf/resolv.conf.d/tail", "/tmp/resolv-conf-d-tail")
                                                                                       ;
