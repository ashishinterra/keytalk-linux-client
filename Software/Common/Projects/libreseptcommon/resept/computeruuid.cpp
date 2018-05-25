#include "computeruuid.h"
#ifdef __linux__
#include "ta/linuxhwutils.h"
#include "ta/osinfoutils.h"
#endif
#include "ta/sysinfo.h"
#include "ta/osuserinfo.h"
#include "ta/windowshwutils.h"
#include "ta/logger.h"
#include "ta/netutils.h"
#include "ta/hashutils.h"
#include "ta/strings.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/algorithm/string.hpp"
#include "boost/static_assert.hpp"
#include "boost/bind.hpp"
#ifdef WIN32
#include <windows.h>
#endif
#include <map>
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;

namespace resept
{
    namespace ComputerUuid
    {
        namespace Components
        {
#ifdef WIN32
            const std::string names[] =
            {
                "Predefined",
                "HddSerial",
                "NicMac",
                "Hdd",
                "Nic",
                "Hdc",
                "UsbHub",
                "DisplayAdapter",
                "Memory",
                "CPU",
                "IC",
                "SysTimer",
                "DMA",
                "SysSpeaker",
                "OsProductId",
                "OsRegisteredOwner",
                "UserSID",
                "Serial"
            };
#elif defined(__linux__)
            const std::string names[] =
            {
                "Predefined",
                "HddSerial",
                "NicMac",
                "CPUArch",
                "CPUModel",
                "OsProductId",
                "UserName",
                "Serial",
                "SshPubKey"
            };

#endif
            string str(Components::id aCompId)
            {
                if (aCompId == Components::Predefined)
                {
                    return names[0];
                }
                else
                {
                    return Components::names[aCompId - Components::First+1];
                }
            }

        }
        BOOST_STATIC_ASSERT(
            (sizeof(Components::names)/sizeof(Components::names[0])) == (Components::Last - Components::First + 1) );

        //
        // Internal stuff
        //
        namespace
        {
            typedef vector<Components::id> formula_t;

            formula_t parseFormula(const string& aFormulaStr, bool& anIsFormulaOk)
            {
                anIsFormulaOk = true;
                formula_t myParsedFormula;
                foreach(const string& tok, ta::Strings::split(aFormulaStr, ','))
                {
                    try
                    {
                        int myCompIdInt = ta::Strings::parse<int>(tok);
                        if (myCompIdInt == Components::Predefined)
                        {
                            myParsedFormula.push_back(Components::Predefined);
                            continue;
                        }
                        if (myCompIdInt >= Components::First && myCompIdInt <= Components::End)
                        {
                            int myMappedCompIDInt = myCompIdInt - Components::First+1;
                            if (static_cast<size_t>(myMappedCompIDInt) <= sizeof(Components::names)/sizeof(Components::names[0])-1)
                            {
                                myParsedFormula.push_back(static_cast<Components::id>(myCompIdInt));
                                continue;
                            }
                            myParsedFormula.push_back(Components::Predefined);
                        }
                        anIsFormulaOk = false;
                    }
                    catch (std::exception&)
                    {
                        anIsFormulaOk = false;
                    }
                }
                if (myParsedFormula.empty())
                {
                    anIsFormulaOk = false;
                    myParsedFormula.push_back(Components::Predefined);
                }
                return myParsedFormula;
            }

            string formula2str(formula_t aFormula)
            {
                assert(!aFormula.empty());
                string myRetVal;
                foreach (formula_t::value_type compid, aFormula)
                {
                    myRetVal += str(boost::format("%s%d") % (myRetVal.empty() ? "" : ",") % compid);
                }
                return myRetVal;
            }
        }


        //
        // Public API
        //


        string calcCs(const string& aFormula, string* aParsedFormula, bool* anIsFormulaOk, std::map<Components::id, string>* aHwIds)
        {
            bool myIsFormulaOk;

            formula_t myParsedFormula = parseFormula(aFormula, myIsFormulaOk);
            if (!myIsFormulaOk)
            {
                INFOLOG(boost::format("Parsed HWSIG formula '%s' to '%s'") % aFormula % formula2str(myParsedFormula));
            }

            if (aParsedFormula)
                *aParsedFormula = formula2str(myParsedFormula);
            if (anIsFormulaOk)
                *anIsFormulaOk = myIsFormulaOk;

            static const string PredefinedCompId    = "000000000000";
            static const string DefDevInstId        = "000\\0000\\00000";
            static const string DefHddSerial        = "00000000000000";
            static const string DefMac              = "000000000000";
            static const string DefOsProductId      = "00000-000-0000000-00000";
            static const string DefOsProductOwner   = "Anonymous";
            static const string DefPhysMemory       = "0";
            static const string DefUserSID          = "S-0-0-00-0000000000-0000000000-0000000000-0000";
            static const string DefSerialNumber     = "0000000";
            static const string DefCpuArch          = "0000";
            static const string DefCpuModel         = "00000";
            static const string DefUserName         = "anonymous";
            static const string DefSshPubKey         ="000000";

            string myCs;
            std::map<Components::id, string> myHwIds;
            foreach (formula_t::value_type compid, myParsedFormula)
            {
                // reuse the already calcualted HWSIG is applicable
                string myHwId;
                if (ta::findValueByKey(compid, myHwIds, myHwId))
                {
                    // already calculated, skip
                    myCs += myHwId;
                    continue;
                }

                // cache miss, calculate the value
                switch (compid)
                {
                case Components::HddSerial:
                {
                    string myHddSerial;
#ifdef _WIN32
                    if (!ta::windowshwutils::getPrimaryHardDriveSerial(myHddSerial) )
#else
                    if (!ta::linuxhwutils::getPrimaryHardDriveSerial(myHddSerial) )
#endif
                    {
                        WARNLOG(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefHddSerial);
                        myHwId = DefHddSerial;
                    }
                    else
                    {
                        myHwId = myHddSerial;
                    }
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
                case Components::NicMac:
                {
                    try
                    {
                        myHwId = ta::NetUtils::getPrimaryMacAddress();
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefMac, e.what());
                        myHwId = DefMac;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#ifdef WIN32
                case Components::Hdd:
                case Components::Nic:
                case Components::Hdc:
                case Components::UsbHub:
                case Components::DisplayAdapter:
                case Components::CPU:
                case Components::IC:
                case Components::SysTimer:
                case Components::DMA:
                case Components::SysSpeaker:
                {
                    try
                    {
                        vector<ta::windowshwutils::DeviceInfo> myDevs;
                        switch (compid)
                        {
                        case Components::Hdd:
                        {
                            const vector<ta::windowshwutils::DeviceInfo> myHddDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::HddArgs) / sizeof(ta::windowshwutils::HddArgs[0]), ta::windowshwutils::HddArgs);
                            static const char* HddIdeControllerArgs[] = {"=hdc", "@PCIIDE\\*"};
                            static const char* HddScsiControllerArgs[] = {"=SCSIAdapter"};
                            vector<ta::windowshwutils::DeviceInfo> myHddParentDevs = ta::windowshwutils::getDevices(sizeof(HddIdeControllerArgs) / sizeof(HddIdeControllerArgs[0]), HddIdeControllerArgs);
                            myHddParentDevs += ta::windowshwutils::getDevices(sizeof(HddScsiControllerArgs) / sizeof(HddScsiControllerArgs[0]), HddScsiControllerArgs);
                            foreach (const ta::windowshwutils::DeviceInfo& dev, myHddDevs)
                            {
                                if (ta::isElemExistWhen(boost::bind(&ta::windowshwutils::DeviceInfo::instId, _1) == dev.parent_instId, myHddParentDevs))
                                {
                                    myDevs.push_back(dev);
                                }
                            }
                            break;
                        }
                        case Components::Nic:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::NicArgs) / sizeof(ta::windowshwutils::NicArgs[0]), ta::windowshwutils::NicArgs);
                            break;
                        case Components::Hdc:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::HdcArgs)/sizeof(ta::windowshwutils::HdcArgs[0]), ta::windowshwutils::HdcArgs);
                            break;
                        case Components::UsbHub:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::UsbHubArgs)/sizeof(ta::windowshwutils::UsbHubArgs[0]), ta::windowshwutils::UsbHubArgs);
                            break;
                        case Components::DisplayAdapter:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::DisplayAdapterArgs)/sizeof(ta::windowshwutils::DisplayAdapterArgs[0]), ta::windowshwutils::DisplayAdapterArgs);
                            break;
                        case Components::CPU:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::CpuArgs)/sizeof(ta::windowshwutils::CpuArgs[0]), ta::windowshwutils::CpuArgs);
                            break;
                        case Components::IC:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::IcArgs)/sizeof(ta::windowshwutils::IcArgs[0]), ta::windowshwutils::IcArgs);
                            break;
                        case Components::SysTimer:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::SysTimerArgs)/sizeof(ta::windowshwutils::SysTimerArgs[0]), ta::windowshwutils::SysTimerArgs);
                            break;
                        case Components::DMA:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::DmaArgs)/sizeof(ta::windowshwutils::DmaArgs[0]), ta::windowshwutils::DmaArgs);
                            break;
                        case Components::SysSpeaker:
                            myDevs = ta::windowshwutils::getDevices(sizeof(ta::windowshwutils::SysSpeakerArgs)/sizeof(ta::windowshwutils::SysSpeakerArgs[0]), ta::windowshwutils::SysSpeakerArgs);
                            break;
                        default:
                            assert(!"Bad formula component");
                        }
                        if (myDevs.empty())
                        {
                            myHwId = DefDevInstId;
                        }
                        else
                        {
                            foreach (const ta::windowshwutils::DeviceInfo& dev, myDevs)
                            {
                                myHwId += dev.instId;
                            }
                        }
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefDevInstId, e.what() );
                        myHwId = DefDevInstId;
                    }
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }

                case Components::Memory:
                {
                    MEMORYSTATUSEX statex;
                    statex.dwLength = sizeof (statex);
                    if (!::GlobalMemoryStatusEx (&statex))
                    {
                        int myLastError = ::GetLastError();
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefPhysMemory,
                                 boost::format("GlobalMemoryStatusEx failed. Last error %d.") % myLastError);
                        myHwId = DefPhysMemory;
                    }
                    else
                    {
                        myHwId = str(boost::format("%1%") % statex.ullTotalPhys);
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
                case Components::UserSID:
                {
                    try
                    {
                        myHwId = ta::OsUserInfo::getCurentUserSID();
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefUserSID, e.what() );
                        myHwId = DefUserSID;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#elif defined(__linux__)
                case Components::UserName:
                {
                    try
                    {
                        myHwId = ta::getUserName();
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefUserName, e.what() );
                        myHwId = DefUserName;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#endif
                case Components::OsProductId:
                {
                    try
                    {
#ifdef WIN32
                        myHwId = ta::SysInfo::getWinProductId();
#elif defined(__linux__)
                        myHwId = ta::OsInfoUtils::getPlatformShortName();
#endif
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefOsProductId, e.what() );
                        myHwId = DefOsProductId;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#ifdef WIN32
                case Components::OsRegisteredOwner:
                {

                    try
                    {
                        myHwId = ta::SysInfo::getWinRegisteredOwner();
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefOsProductOwner, e.what() );
                        myHwId = DefOsProductOwner;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#endif
                case Components::Serial:
                {
                    try
                    {
                        myHwId = ta::SysInfo::getSerialNumber();
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefSerialNumber, e.what() );
                        myHwId = DefSerialNumber;
                    }
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#if defined(__linux__)
                case Components::CPUArch:
                {
                    try
                    {
                        myHwId = ta::linuxhwutils::getCpuArch();
                    }
                    catch(std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefCpuArch, e.what() );
                        myHwId = DefCpuArch;
                    }
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }

                case Components::CPUModel:
                {
                    try
                    {
                        myHwId = ta::linuxhwutils::getCpuModel();
                    }
                    catch(std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefCpuModel, e.what() );
                        myHwId = DefCpuModel;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }

                case Components::SshPubKey:
                {
                    try
                    {
                        std::vector<ta::linuxhwutils::SshPubKey> myKeys = ta::linuxhwutils::getSsh2HostPubKeys();
                        if (myKeys.empty())
                        {
                            TA_THROW_MSG(std::runtime_error, "No SSH2 host keys exist");
                        }
                        myHwId = "";
                        foreach (const ta::linuxhwutils::SshPubKey& key, myKeys)
                        {
                            myHwId += key.type + ":" + key.val;
                        }
                    }
                    catch(std::exception& e)
                    {
                        WARNLOG2(boost::format("Failed to retrieve %s for HWSIG calculation. Falling back to default value %s") % str(compid) % DefSshPubKey, e.what() );
                        myHwId = DefSshPubKey;
                    }

                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
#endif
                case Components::Predefined:
                {
                    myHwId = PredefinedCompId;
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
                default:
                {
                    WARNLOG(boost::format("Failed to retrieve %s for HWSIG calculation because it is not supported. Falling back to default value %s") % str(compid) % PredefinedCompId);
                    myHwId = PredefinedCompId;
                    myHwIds[compid] = myHwId;
                    myCs += myHwId;
                    break;
                }
                }// switch

                DEBUGLOG(str(compid) + ": " + myHwId);

            }// foreach


            if (aHwIds)
            {
                *aHwIds = myHwIds;
            }

            if (myCs.empty())
            {
                myCs = PredefinedCompId;
            }
            myCs = "CS-" + ta::HashUtils::getSha256Hex(myCs);
            return myCs;
        }
    }
}
