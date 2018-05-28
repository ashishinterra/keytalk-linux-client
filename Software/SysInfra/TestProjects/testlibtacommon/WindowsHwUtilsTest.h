#pragma once

#include "ta/windowshwutils.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <iostream>

class WindowsHtUtilsTest : public CxxTest::TestSuite
{
public:
    void testDevInfo4AllSupportedClasses()
    {
        // Retrieve devices for all device classes which exist in the system
        using namespace ta::windowshwutils;

        const std::vector<DeviceClass> myDevClasses = getDeviceClasses();
        TS_ASSERT(!myDevClasses.empty());
        std::cout << "\n" << myDevClasses.size() << " device classes found\n";
        foreach (const DeviceClass& devClass,  myDevClasses)
        {
            const string myArg = ("=" + devClass.name);
            const char* mySzArg = myArg.c_str();
            std::vector<DeviceInfo> myDevs = getDevices( 1,  &mySzArg);
            std::cout << myDevs.size() << " devices in class " << devClass.name << " (" << devClass.descr << ")\n";
            foreach (const DeviceInfo& dev, myDevs)
            {
                TS_ASSERT(!dev.instId.empty());
                TS_ASSERT(!dev.parent_instId.empty());
                std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
             }
        }
    }
    void testDevInfo4PredefinedArgs()
    {
        using namespace ta::windowshwutils;

        std::vector<DeviceInfo> myDevs  = getDevices(sizeof(HddArgs)/sizeof(HddArgs[0]), HddArgs);
        std::cout << "\n" << myDevs.size() << " HDDs\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(NicArgs)/sizeof(NicArgs[0]), NicArgs);
        std::cout << "\n" << myDevs.size() << " NICs\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(HdcArgs)/sizeof(HdcArgs[0]), HdcArgs);
        std::cout << "\n" << myDevs.size() << " IDE ATA/ATAPI controllers\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(UsbHubArgs)/sizeof(UsbHubArgs[0]), UsbHubArgs);
        std::cout << "\n" << myDevs.size() << " USB Hubs\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(DisplayAdapterArgs)/sizeof(DisplayAdapterArgs[0]), DisplayAdapterArgs);
        std::cout << "\n" << myDevs.size() << " Display adapters\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(CpuArgs)/sizeof(CpuArgs[0]), CpuArgs);
        std::cout << "\n" << myDevs.size() << " CPUs\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(IcArgs)/sizeof(IcArgs[0]), IcArgs);
        std::cout << "\n" << myDevs.size() << " Interrupt controllers\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }

        myDevs  = getDevices(sizeof(SysTimerArgs)/sizeof(SysTimerArgs[0]), SysTimerArgs);
        std::cout << "\n" << myDevs.size() << " System timer devices\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }


        myDevs  = getDevices(sizeof(DmaArgs)/sizeof(DmaArgs[0]), DmaArgs);
        std::cout << "\n" << myDevs.size() << " DMA controllers\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }


        myDevs  = getDevices(sizeof(SysSpeakerArgs)/sizeof(SysSpeakerArgs[0]), SysSpeakerArgs);
        std::cout << "\n" << myDevs.size() << " System speakers\n";
        foreach (const DeviceInfo& dev, myDevs)
        {
            TS_ASSERT(!dev.instId.empty());
            TS_ASSERT(!dev.parent_instId.empty());
            std::cout << "\t" << dev.instId << " (" << dev.descr << "). Status: "  << (dev.isRunning? "running":"not running" ) << ". Parent: " << dev.parent_instId <<"\n";
         }
    }

    void test_that_hdd_serial_can_be_retrieved()
    {
        std::string myHddSerial;
        if (ta::windowshwutils::getPrimaryHardDriveSerial(myHddSerial))
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
