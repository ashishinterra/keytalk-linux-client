#pragma once

#include <string>
#include <vector>
#include <map>

namespace resept
{
    namespace ComputerUuid
    {
        // Components used to calculate Hardware Signature
        namespace Components
        {
            enum id
            {
                Predefined = 0,    // Predefined value
#ifdef WIN32
                First = 1,

                // Do not define component enumeration values above this line
                // ----------------------------------------------------------

                HddSerial = First, // Primary HDD Serial defined by minimal i for which \\.\PhysicalDrive<i> or \\.\Scsi<i> is accessible
                NicMac,            // Primary NIC MAC-address. Primary NIC is NIC listed first in the "Network Connections" folder-> Advanced menu -> Advanced settings list.
                Hdd,               // HDDs Device Instance IDs. Only PCI IDE and SCSI HDDs are considered skipping hot-plugguble disks attached to USB or PCMCIA.
                Nic,               // NICs Device Instance IDs. Only NICs attached to PCI are considered to avoid pluggable NICS e.g. USB.
                Hdc,               // PCI IDE ATA/ATAPI controllers Device Instance IDs excluding hot-pluggable ones like e.g. PCMCIA.
                UsbHub,            // USB Root Hubs Device Instance IDs.
                DisplayAdapter,    // Display Adapters Device Instance IDs.
                Memory,            // Amount of physical memory.
                CPU,               // CPUs device instance IDs.
                IC,                // Interrupt controller device instance ID.
                SysTimer,          // System timer device instance ID.
                DMA,               // DMA controller device instance ID.
                SysSpeaker,        // System speaker device instance ID.
                OsProductId,       // OS Product ID.
                OsRegisteredOwner, // OS registered owner.
                UserSID,           // User Security Identifier.
                Serial,            // Serial number retrieved from BIOS

                // ----------------------------------------------------------
                // Do not define component enumeration values below this line
                Last,

                End = 100         // Last element of Windows client range
#elif defined(__linux__)
                First = 601,
                // Do not define component enumeration values above this line
                // ----------------------------------------------------------

                HddSerial = First, // 601: Primary HDD Serial.
                NicMac,            // 602: Primary NIC MAC-address.
                CPUArch,           // 603: CPUs hardware architectures.
                CPUModel,          // 604: CPUs model
                OsProductId,       // 605: OS name.
                UserName,          // 606: User name.
                Serial,            // 607: Serial number read from BIOS. When BIOS is not available (e.g. on RaspberryPi) return CPU serial number
                SshPubKey,         // 608: SSH2 public keys of the host if available

                // ----------------------------------------------------------
                // Do not define component enumeration values below this line
                Last,

                End = 700         // Last element of Linux client range
#else
#error "Unsupported platform"
#endif
            };
            std::string str(Components::id aCompId);
        }

        //
        // Abstract  : calculates Hardware Computer Signature (CS).
        //
        // Computer signature is calculated from different components combined using the formula supplied by a user.
        //
        // @param [in] aFormula - comma-separated list of component ids to be used in CS calculation. The order of components matters.
        //             Format: "comp1id[,comp2id[,comp3id...]]". See validateFormula function below for more info.
        //             comp - component id, see Components::id enum above.
        //             Example "1,0,1,2" - subsequently HddSerial, Predefined, HddSerial, NicMac will be used for calculation
        //
        // @param [out] aParsedFormula if not NULL, *aParsedFormula holds the parsed formula (see discussion below)
        // @param [out] anIsFormulaOk  if not NULL and *anIsFormulaOk is set accordingly
        // @param [out] aHwIds  if not NULL and *aHwIds is set to HWIDs for individual formula components
        // @return computer signature string. The string has format 'CS-<SHA256HASH>' which gives 67 characters for the computer signature.
        //
        // The main idea of computer signature is to provide as much fault-tolerance as possible.
        // This means that the function must provide sensitive output even for ill-formed formulas.
        // The formula is considered valid if all the criteria below are met:
        // - Formula satisfies "comp1[,comp2[,comp3...]]" format (white-space is ignored)
        // - Each compN corresponds to the Computer::id value above

        // If a parsed component falls into a valid range [First..Last] but is not assigned to a certain HW component it defaults to 0 (predefined), *aParsedFormula is set accordingly and *anIsFormulaOk is set to false
        // If a parsed component does not fall into a valid range [First..Last] it is ignored, *aParsedFormula is set accordingly and *anIsFormulaOk is set to false
        //
        // Example: "1, foo, 2, 99, 3, -1, 199" will be parsed as "1, 2, 0, 3" on Windows

        std::string calcCs(const std::string& aFormula,
                           std::string* aParsedFormula = NULL,
                           bool* anIsFormulaOk = NULL,
                           std::map<Components::id, std::string>* aHwIds = NULL);

    }
}


