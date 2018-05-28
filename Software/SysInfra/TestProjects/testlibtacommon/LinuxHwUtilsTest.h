#pragma once

#include "ta/linuxhwutils.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <string>


class LinuxHwUtilsTest : public CxxTest::TestSuite
{
public:
    void test_that_cpu_arch_can_be_read()
    {
        const std::string test_str = ta::linuxhwutils::getCpuArch();
        TS_TRACE("CPU Architecture is " + test_str);
        TS_ASSERT(!test_str.empty());
    }

    void test_that_cpu_model_can_be_read()
    {
        const std::string test_str = ta::linuxhwutils::getCpuModel();
        TS_TRACE("CPU model is " + test_str);
        TS_ASSERT(!test_str.empty());
    }

    void test_that_serial_num_can_be_read()
    {
        const std::string test_str = ta::linuxhwutils::getSerialNum();
        TS_TRACE("Serial number is " + test_str);
        TS_ASSERT(!test_str.empty());
    }

    void test_that_sshd_host_keys_can_be_retrieved()
    {
        std::vector<ta::linuxhwutils::SshPubKey> myKeys = ta::linuxhwutils::getSsh2HostPubKeys();
        TS_ASSERT(!myKeys.empty());
        foreach (const ta::linuxhwutils::SshPubKey& key, myKeys)
        {
            TS_TRACE(str(boost::format("SShd host key type: %s, value: %s") % key.type % key.val).c_str());
            TS_ASSERT(!key.type.empty());
            TS_ASSERT(!key.val.empty());
        }
    }

    void test_that_hdd_serial_can_be_retrieved()
    {
        std::string myHddSerial;
        if (ta::linuxhwutils::getPrimaryHardDriveSerial(myHddSerial))
        {
            if (myHddSerial.empty())
            {
                TS_WARN("Primary HDD serial number is empty");
            }
            else
            {
                TS_TRACE(("Primary HDD serial: " + myHddSerial).c_str());
            }
        }
        else
        {
            // can happen on VMs
            TS_WARN("Failed to retrieve primary HDD serial.");
        }
    }
};
