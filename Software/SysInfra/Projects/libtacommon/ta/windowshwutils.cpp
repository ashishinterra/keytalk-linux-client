#ifdef _WIN32

//
// The implementation is based on devcon utility (http://support.microsoft.com/kb/311272)
//

#include "windowshwutils.h"
#include "windowshddutils.h"
#include "scopedresource.hpp"
#include "common.h"

#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <cfgmgr32.h>

using namespace boost;
using std::string;
using std::vector;

#define INSTANCEID_PREFIX_CHAR '@' // character used to prefix instance ID's
#define CLASS_PREFIX_CHAR      '=' // character used to prefix class name
#define WILD_CHAR              '*' // wild character
#define QUOTE_PREFIX_CHAR      '\'' // prefix character to ignore wild characters


namespace ta
{
    namespace windowshwutils
    {
        //
        // Private API
        //
        namespace
        {
            struct IdEntry
            {
                LPCTSTR String;     // string looking for
                LPCTSTR Wild;       // first wild character if any
                BOOL    InstanceId;
            };
            typedef bool (*FindCallbackFunc)(HDEVINFO Devs,PSP_DEVINFO_DATA DevInfo,LPVOID Context);



            LPTSTR GetDeviceStringProperty(HDEVINFO Devs,PSP_DEVINFO_DATA DevInfo,DWORD Prop)
            /*++

            Routine Description:

            Return a string property for a device, otherwise NULL

            Arguments:

            Devs    )_ uniquely identify device
            DevInfo )
            Prop     - string property to obtain

            Return Value:

            string containing description

            --*/
            {
                LPTSTR buffer;
                DWORD size;
                DWORD reqSize;
                DWORD dataType;
                DWORD szChars;

                size = 1024; // initial guess
                buffer = new TCHAR[(size/sizeof(TCHAR))+1];
                if(!buffer) {
                    return NULL;
                }
                while(!SetupDiGetDeviceRegistryProperty(Devs,DevInfo,Prop,&dataType,(LPBYTE)buffer,size,&reqSize)) {
                    if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                        goto failed;
                    }
                    if(dataType != REG_SZ) {
                        goto failed;
                    }
                    size = reqSize;
                    delete [] buffer;
                    buffer = new TCHAR[(size/sizeof(TCHAR))+1];
                    if(!buffer) {
                        goto failed;
                    }
                }
                szChars = reqSize/sizeof(TCHAR);
                buffer[szChars] = TEXT('\0');
                return buffer;

failed:
                if(buffer) {
                    delete [] buffer;
                }
                return NULL;
            }

            LPTSTR GetDeviceDescription(HDEVINFO Devs,PSP_DEVINFO_DATA DevInfo)
            /*++

            Routine Description:

            Return a string containing a description of the device, otherwise NULL
            Always try friendly name first

            Arguments:

            Devs    )_ uniquely identify device
            DevInfo )

            Return Value:

            string containing description

            --*/
            {
                LPTSTR desc;
                desc = GetDeviceStringProperty(Devs,DevInfo,SPDRP_FRIENDLYNAME);
                if(!desc)
                    desc = GetDeviceStringProperty(Devs,DevInfo,SPDRP_DEVICEDESC);
                return desc;
            }

            IdEntry GetIdType(LPCTSTR Id)
            /*++

            Routine Description:

            Determine if this is instance id or hardware id and if there's any wildcards
            instance ID is prefixed by '@'
            wildcards are '*'


            Arguments:

            Id - ptr to string to check

            Return Value:

            IdEntry

            --*/
            {
                IdEntry Entry;

                Entry.InstanceId = FALSE;
                Entry.Wild = NULL;
                Entry.String = Id;

                if(Entry.String[0] == INSTANCEID_PREFIX_CHAR) {
                    Entry.InstanceId = TRUE;
                    Entry.String = CharNext(Entry.String);
                }
                if(Entry.String[0] == QUOTE_PREFIX_CHAR) {
                    //
                    // prefix to treat rest of string literally
                    //
                    Entry.String = CharNext(Entry.String);
                } else {
                    //
                    // see if any wild characters exist
                    //
                    Entry.Wild = _tcschr(Entry.String,WILD_CHAR);
                }
                return Entry;
            }

            LPTSTR* GetMultiSzIndexArray(LPTSTR MultiSz)
            /*++

            Routine Description:

            Get an index array pointing to the MultiSz passed in

            Arguments:

            MultiSz - well formed multi-sz string

            Return Value: array of strings.
              each element point to the parsed parts of MultiSz
              last element contains NULL to allow identifying array size
              returns NULL on failure

            --*/
            {
                int num_elements = 0;

                // determine the number of elements
                for (LPTSTR scan = MultiSz; scan[0] ; ++num_elements)
                {
                    scan += lstrlen(scan)+1;
                }

                LPTSTR* retval = new LPTSTR[num_elements+1];
                if (!retval)
                {
                    return NULL;
                }

                if (num_elements > 0)
                {
                    LPTSTR scan = MultiSz;
                    for (int i = 0; i < num_elements; ++i)
                    {
                        retval[i] = scan;
                        scan += lstrlen(scan)+1;
                    }
                }
                retval[num_elements] = NULL;

                return retval;
            }


            LPTSTR* GetDevMultiSz(HDEVINFO Devs,PSP_DEVINFO_DATA DevInfo,DWORD Prop)
            /*++

            Routine Description:

            Get a multi-sz device property
            and return as an array of strings

            Arguments:

            Devs    - HDEVINFO containing DevInfo
            DevInfo - Specific device
            Prop    - SPDRP_HARDWAREID or SPDRP_COMPATIBLEIDS

            Return Value:

            array of strings. last entry+1 of array contains NULL
            returns NULL on failure

            --*/
            {
                DWORD reqSize;
                DWORD dataType;

                DWORD size = 8192; // initial guess, nothing magic about this
                LPTSTR buffer = new TCHAR[(size/sizeof(TCHAR))+2];
                if (!buffer)
                {
                    return NULL;
                }

                while (!SetupDiGetDeviceRegistryProperty(Devs,DevInfo,Prop,&dataType,(LPBYTE)buffer,size,&reqSize))
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || dataType != REG_MULTI_SZ)
                    {
                        delete [] buffer;
                        return NULL;
                    }

                    size = reqSize;
                    delete [] buffer;
                    buffer = new TCHAR[(size/sizeof(TCHAR))+2];
                    if (!buffer)
                    {
                        return NULL;
                    }
                }
                DWORD szChars = reqSize/sizeof(TCHAR);
                buffer[szChars] = TEXT('\0');
                buffer[szChars+1] = TEXT('\0');

                LPTSTR* retval = GetMultiSzIndexArray(buffer);
                delete [] buffer;
                return retval;
            }



            BOOL WildCardMatch(LPCTSTR Item,const IdEntry& MatchEntry)
            /*++

            Routine Description:

            Compare a single item against wildcard
            I'm sure there's better ways of implementing this
            Other than a command-line management tools
            it's a bad idea to use wildcards as it implies
            assumptions about the hardware/instance ID
            eg, it might be tempting to enumerate root\* to
            find all root devices, however there is a CfgMgr
            API to query status and determine if a device is
            root enumerated, which doesn't rely on implementation
            details.

            Arguments:

            Item - item to find match for eg a\abcd\c
            MatchEntry - eg *\*bc*\*

            Return Value:

            TRUE if any match, otherwise FALSE

            --*/
            {
                LPCTSTR scanItem;
                LPCTSTR wildMark;
                LPCTSTR nextWild;
                size_t matchlen;

                //
                // before attempting anything else
                // try and compare everything up to first wild
                //
                if(!MatchEntry.Wild) {
                    return _tcsicmp(Item,MatchEntry.String) ? FALSE : TRUE;
                }
                if(_tcsnicmp(Item,MatchEntry.String,MatchEntry.Wild-MatchEntry.String) != 0) {
                    return FALSE;
                }
                wildMark = MatchEntry.Wild;
                scanItem = Item + (MatchEntry.Wild-MatchEntry.String);

                for(; wildMark[0];) {
                    //
                    // if we get here, we're either at or past a wildcard
                    //
                    if(wildMark[0] == WILD_CHAR) {
                        //
                        // so skip wild chars
                        //
                        wildMark = CharNext(wildMark);
                        continue;
                    }
                    //
                    // find next wild-card
                    //
                    nextWild = _tcschr(wildMark,WILD_CHAR);
                    if(nextWild) {
                        //
                        // substring
                        //
                        matchlen = nextWild-wildMark;
                    } else {
                        //
                        // last portion of match
                        //
                        size_t scanlen = lstrlen(scanItem);
                        matchlen = lstrlen(wildMark);
                        if(scanlen < matchlen) {
                            return FALSE;
                        }
                        return _tcsicmp(scanItem+scanlen-matchlen,wildMark) ? FALSE : TRUE;
                    }
                    if(_istalpha(wildMark[0])) {
                        //
                        // scan for either lower or uppercase version of first character
                        //
                        TCHAR u = (TCHAR)_totupper(wildMark[0]);
                        TCHAR l = (TCHAR)_totlower(wildMark[0]);
                        while(scanItem[0] && scanItem[0]!=u && scanItem[0]!=l) {
                            scanItem = CharNext(scanItem);
                        }
                        if(!scanItem[0]) {
                            //
                            // ran out of string
                            //
                            return FALSE;
                        }
                    } else {
                        //
                        // scan for first character (no case)
                        //
                        scanItem = _tcschr(scanItem,wildMark[0]);
                        if(!scanItem) {
                            //
                            // ran out of string
                            //
                            return FALSE;
                        }
                    }
                    //
                    // try and match the sub-string at wildMark against scanItem
                    //
                    if(_tcsnicmp(scanItem,wildMark,matchlen)!=0) {
                        //
                        // nope, try again
                        //
                        scanItem = CharNext(scanItem);
                        continue;
                    }
                    //
                    // substring matched
                    //
                    scanItem += matchlen;
                    wildMark += matchlen;
                }
                return (wildMark[0] ? FALSE : TRUE);
            }

            BOOL WildCompareHwIds(LPTSTR* Array,const IdEntry& MatchEntry)
            /*++

            Routine Description:

            Compares all strings in Array against Id
            Use WildCardMatch to do real compare

            Arguments:

            Array - pointer returned by GetDevMultiSz
            MatchEntry - string to compare against

            Return Value:

            TRUE if any match, otherwise FALSE

            --*/
            {
                if(Array) {
                    while(Array[0]) {
                        if(WildCardMatch(Array[0],MatchEntry)) {
                            return TRUE;
                        }
                        Array++;
                    }
                }
                return FALSE;
            }




            /*
            Generic enumerator for devices that will be passed the following arguments:
            <id> [<id>...]
            =<class> [<id>...]
            where <id> can either be @instance-id, or hardware-id and may contain wildcards
            <class> is a class name

            Arguments:

            Flags    - extra enumeration flags (eg DIGCF_PRESENT)
            argc/argv - remaining arguments on command line
            Callback - function to call for each hit
            Context  - data to pass function for each hit


            throw std::runtime_error, std::invalid_argument

            --*/
            void EnumerateDevices(DWORD Flags,int argc,const char* argv[], FindCallbackFunc aFindCallback, LPVOID Context)
            {
                if(!argc || !argv || !Context || !aFindCallback)
                    TA_THROW(std::invalid_argument);

                std::vector<IdEntry> templ(argc);

                int argIndex;
                DWORD devIndex;
                SP_DEVINFO_DATA devInfo;
                SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
                BOOL doSearch = FALSE;
                BOOL match;
                BOOL all = FALSE;
                GUID cls;
                DWORD numClass = 0;
                int skip = 0;

                //
                // determine if a class is specified
                //
                if(argc>skip && argv[skip][0]==CLASS_PREFIX_CHAR && argv[skip][1])
                {
                    if(!SetupDiClassGuidsFromNameEx(argv[skip]+1,&cls,1,&numClass,NULL,NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                        TA_THROW_MSG(std::runtime_error, boost::format("SetupDiClassGuidsFromNameEx failed. Last error %d") % GetLastError());
                    if (!numClass)
                        return;
                    skip++;
                }
                if(argc>skip && argv[skip][0]==WILD_CHAR && !argv[skip][1])
                {
                    //
                    // catch convinient case of specifying a single argument '*'
                    //
                    all = TRUE;
                    skip++;
                }
                else if(argc<=skip)
                {
                    //
                    // at least one parameter, but no <id>'s
                    //
                    all = TRUE;
                }

                //
                // determine if any instance id's were specified
                //
                // note, if =<class> was specified with no id's
                // we'll mark it as not doSearch
                // but will go ahead and add them all
                //
                for(argIndex=skip; argIndex<argc; argIndex++)
                {
                    templ[argIndex] = GetIdType(argv[argIndex]);
                    if(templ[argIndex].Wild || !templ[argIndex].InstanceId)
                    {
                        //
                        // anything other than simple InstanceId's require a search
                        //
                        doSearch = TRUE;
                    }
                }


                ScopedResource<HDEVINFO> devs((doSearch||all) ? SetupDiGetClassDevsEx(numClass?&cls:NULL,NULL,NULL,(numClass ? 0 : DIGCF_ALLCLASSES) | Flags, NULL, NULL, NULL)
                                              : SetupDiCreateDeviceInfoListEx(numClass?&cls:NULL,NULL,NULL,NULL),
                                              SetupDiDestroyDeviceInfoList,
                                              INVALID_HANDLE_VALUE);

                if (devs == INVALID_HANDLE_VALUE)
                    TA_THROW_MSG(std::runtime_error,  boost::format("SetupDiGetClassDevsEx or SetupDiCreateDeviceInfoListEx failed. Last error: %d") % ::GetLastError());
                for(argIndex=skip; argIndex<argc; argIndex++)
                {
                    //
                    // add explicit instances to list (even if enumerated all,
                    // this gets around DIGCF_PRESENT)
                    // do this even if wildcards appear to be detected since they
                    // might actually be part of the instance ID of a non-present device
                    //
                    if(templ[argIndex].InstanceId)
                        SetupDiOpenDeviceInfo(devs,templ[argIndex].String,NULL,0,NULL);
                }

                devInfoListDetail.cbSize = sizeof(devInfoListDetail);
                if (!SetupDiGetDeviceInfoListDetail(devs,&devInfoListDetail))
                    TA_THROW_MSG(std::runtime_error,  boost::format("SetupDiGetDeviceInfoListDetail failed. Last error: %d") % ::GetLastError());

                //
                // now enumerate them
                //
                if (all)
                    doSearch = FALSE;

                devInfo.cbSize = sizeof(devInfo);
                for (devIndex=0; SetupDiEnumDeviceInfo(devs,devIndex,&devInfo); devIndex++)
                {
                    if(doSearch)
                    {
                        for(argIndex=skip,match=FALSE; (argIndex<argc) && !match; argIndex++)
                        {
                            TCHAR devID[MAX_DEVICE_ID_LEN];
                            //
                            // determine instance ID
                            //
                            if(CM_Get_Device_ID_Ex(devInfo.DevInst,devID,MAX_DEVICE_ID_LEN,0,devInfoListDetail.RemoteMachineHandle)!=CR_SUCCESS)
                                devID[0] = TEXT('\0');

                            if(templ[argIndex].InstanceId)
                            {
                                //
                                // match on the instance ID
                                //
                                if(WildCardMatch(devID,templ[argIndex]))
                                    match = TRUE;
                            }
                            else
                            {
                                //
                                // determine hardware ID's
                                // and search for matches
                                //
                                LPTSTR* hwIds = GetDevMultiSz(devs,&devInfo,SPDRP_HARDWAREID);
                                LPTSTR* compatIds = GetDevMultiSz(devs,&devInfo,SPDRP_COMPATIBLEIDS);

                                if(WildCompareHwIds(hwIds, templ[argIndex]) || WildCompareHwIds(compatIds, templ[argIndex]))
                                {
                                    match = TRUE;
                                }
                                delete [] compatIds;
                                delete [] hwIds;
                            }
                        }
                    }
                    else
                    {
                        match = TRUE;
                    }
                    if (match)
                    {
                        if (!aFindCallback(devs,&devInfo,Context))
                            TA_THROW_MSG(std::runtime_error, "aFindCallback failed.");
                    }
                } // for

                return;
            }

            BOOL getDevInfo(HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, std::string& aDevInstId, std::string& aDevDescr, bool& anIsDevRunning, std::string& aParentDevInstId)
            {
                TCHAR devID[MAX_DEVICE_ID_LEN] = {};
                TCHAR parent_devID[MAX_DEVICE_ID_LEN] = {};
                SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
                ULONG status = 0;
                ULONG problem = 0;

                devInfoListDetail.cbSize = sizeof(devInfoListDetail);
                if (
                    !SetupDiGetDeviceInfoListDetail(Devs,&devInfoListDetail) ||
                    CM_Get_Device_ID(DevInfo->DevInst,devID, sizeof(devID), 0)!=CR_SUCCESS ||
                    CM_Get_DevNode_Status(&status,&problem,DevInfo->DevInst,0)!=CR_SUCCESS
                )
                {
                    return FALSE;
                }


                DEVINST myParentDevInst;
                if (CM_Get_Parent(&myParentDevInst,DevInfo->DevInst, 0) == CR_SUCCESS &&
                        CM_Get_Device_ID(myParentDevInst,parent_devID, sizeof(parent_devID), 0) == CR_SUCCESS)
                {
                    aParentDevInstId  = parent_devID;
                }
                else
                {
                    // do not fail if patent device id cannot be retieved, just return empty id
                    aParentDevInstId = "";
                }


                LPTSTR desc = GetDeviceDescription(Devs,DevInfo);
                if(!desc)
                    return FALSE;

                aDevInstId = devID;
                aDevDescr = desc;
                delete [] desc;

                anIsDevRunning = false;
                if((status & DN_HAS_PROBLEM) || (status & DN_PRIVATE_PROBLEM))
                    return TRUE;
                if(status & DN_STARTED)
                {
                    anIsDevRunning = true;
                    return TRUE;
                }
                return TRUE;
            }


            bool FindCallback(HDEVINFO Devs,PSP_DEVINFO_DATA DevInfo,LPVOID Context)
            {
                if (!Context)
                    return false;
                std::vector<DeviceInfo>* pDevices = (std::vector<DeviceInfo>*)Context;
                DeviceInfo myDeviceInfo;
                if(!getDevInfo(Devs,DevInfo, myDeviceInfo.instId, myDeviceInfo.descr, myDeviceInfo.isRunning, myDeviceInfo.parent_instId))
                    return false;
                pDevices->push_back(myDeviceInfo);
                return true;
            }

        }


        //
        // Public API
        //

        std::vector<DeviceInfo> getDevices(int argc,const char* argv[])
        {
            std::vector<DeviceInfo> myDevices;
            EnumerateDevices(DIGCF_PRESENT,argc,argv,FindCallback,&myDevices);
            return myDevices;
        }

        std::vector<DeviceClass> getDeviceClasses()
        {
            DWORD myReqGuids;
            std::vector<GUID> myDevGuids(128);
            if (!SetupDiBuildClassInfoListEx(0,ta::getSafeBuf(myDevGuids), (DWORD)myDevGuids.size(),&myReqGuids,NULL,NULL))
            {
                do
                {
                    if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                        TA_THROW_MSG(std::runtime_error,  boost::format("SetupDiBuildClassInfoListEx failed, Last error %d") % GetLastError());
                    myDevGuids.resize(myReqGuids);
                }
                while (!SetupDiBuildClassInfoListEx(0,ta::getSafeBuf(myDevGuids), (DWORD)myDevGuids.size(),&myReqGuids,NULL,NULL));
            }
            myDevGuids.resize(myReqGuids);
            std::vector<DeviceClass> myDeviceClasses;
            foreach (GUID guid, myDevGuids)
            {
                TCHAR className[MAX_CLASS_NAME_LEN] = {};
                TCHAR classDesc[LINE_LEN] = {};
                if (!SetupDiClassNameFromGuidEx(&guid,className,sizeof(className),NULL,NULL,NULL))
                {
                    lstrcpyn(className,TEXT("?"),MAX_CLASS_NAME_LEN);
                }
                if (!SetupDiGetClassDescriptionEx(&guid,classDesc,sizeof(classDesc),NULL,NULL,NULL))
                {
                    lstrcpyn(classDesc,className,LINE_LEN);
                }
                DeviceClass myDevClass;
                myDevClass.name = className;
                myDevClass.descr = classDesc;
                myDeviceClasses.push_back(myDevClass);
            }
            return myDeviceClasses;
        }

        bool getPrimaryHardDriveSerial(string& aSerial)
        {
            return ta::windhowshddutils::getPrimaryHardDriveSerial(aSerial);
        }

    }
} // ta

#endif