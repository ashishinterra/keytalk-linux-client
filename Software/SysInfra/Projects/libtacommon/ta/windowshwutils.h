#pragma once

#ifdef _WIN32

#include <string>
#include <vector>

namespace ta
{
    namespace windowshwutils
    {
        struct DeviceInfo
        {
            std::string instId;
            std::string descr;
            bool isRunning;
            std::string parent_instId;
        };
        struct DeviceClass
        {
            std::string name;
            std::string descr;
        };

        static const char* HddArgs[]            = {"=DiskDrive", "@IDE\\*", "=DiskDrive", "@SCSI\\*"}; // IDE or SCSI harddisks
        static const char* NicArgs[]            = {"=net", "@PCI\\*"};                                // Network cards attached to PCI bus
        static const char* HdcArgs[]            = {"=hdc", "@PCI\\*"};                                // PCI IDE ATA/ATAPI controllers excluding e.g. hot-pluggable PCIMCIA storage devices
        static const char* UsbHubArgs[]         = {"=USB", "@USB\\ROOT_HUB*"};                        // USB Root Hubs
        static const char* DisplayAdapterArgs[] = {"=Display"};                                      // Display adapters
        static const char* CpuArgs[]            = {"=Processor"};                                    // Processors
        static const char* IcArgs[]             = {"=System", "@ACPI\\PNP0000*"};                     // Interrupt controller
        static const char* SysTimerArgs[]       = {"=System", "@ACPI\\PNP0100*"};                     // System timer device
        static const char* DmaArgs[]            = {"=System", "@ACPI\\PNP0200*"};                     // DMA controller
        static const char* SysSpeakerArgs[]     = {"=System", "@ACPI\\PNP0800*"};                     // System speaker

        //
        // Retrieve information about the device(s).
        //
        // argv contain a combination of the device class and device filter.
        // The list of device classes can be retrieved using getDeviceClasses() function below.
        // Device filter might contain '*' wildcards; '@' is used to prefix device instance id
        // For your convenience there are a number of predefined args (see HddArgs,... above) can be used.
        //
        std::vector<DeviceInfo> getDevices(int argc,const char* argv[]);

        //
        // Retrieve device classes
        //
        std::vector<DeviceClass> getDeviceClasses();


        //
        // Retrieve serial number for the "primary" HDD i.e.
        // minimal i for which \\.\PhysicalDrive<i> or \\.\Scsi<i> is accessible
        // @return on success return true and anHddSerialis filled with HDD serial value.
        //         on error return false
        //
        bool getPrimaryHardDriveSerial(std::string& aSerial);

    }
}

#endif // _WIN32
