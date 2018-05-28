#pragma once

#if defined(__linux__)

#include <string>
#include <vector>

namespace ta
{
    namespace linuxhwutils
    {
        std::string getCpuArch();
        std::string getCpuModel();
        std::string getSerialNum();

        struct SshPubKey
        {
            SshPubKey() {}
            SshPubKey(const std::string& aType, const std::string& aVal): type(aType), val(aVal) {}
            std::string type;
            std::string val;
        };
        std::vector<SshPubKey> getSsh2HostPubKeys();

        //
        // Retrieve serial number for the "primary" HDD i.e.
        // the first of /dev/sda or /dev/hda that is accessible
        // @return on success return true and anHddSerialis filled with HDD serial value.
        //         on error return false
        //
        bool getPrimaryHardDriveSerial(std::string& aSerial);
    }
}

#endif
