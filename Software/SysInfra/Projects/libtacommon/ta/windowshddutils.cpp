//----------------------------------------------------------------------------
//
//  Description : HDD Utility for Win32. Adapted from http://www.winsim.com/diskid32/diskid32.cpp from 01/29/08
//
//----------------------------------------------------------------------------

#ifndef _WIN32
# error "Windows platform required!"
#endif

#include "ta/scopedresource.hpp"
#include "ta/logger.h"
#include "ta/common.h"
#include <windows.h>
#include <winioctl.h>
#include "boost/algorithm/string.hpp"
#include <cstdio>
#include <string>

#pragma warning (disable: 4996) // suppress deprecation warning for GetVersionEx()

//  Required to ensure correct PhysicalDrive IOCTL structure setup
#pragma pack(1)

#define  IDENTIFY_BUFFER_SIZE  512
//  IOCTL commands
#define  DFP_RECEIVE_DRIVE_DATA   0x0007c088

#define  FILE_DEVICE_SCSI              0x0000001b
#define  IOCTL_SCSI_MINIPORT_IDENTIFY  ((FILE_DEVICE_SCSI << 16) + 0x0501)
#define  IOCTL_SCSI_MINIPORT 0x0004D008  //  see NTDDSCSI.H for definition


#define SMART_GET_VERSION               CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)
#define SMART_RCV_DRIVE_DATA            CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//  GETVERSIONOUTPARAMS contains the data returned from the
//  Get Driver Version function.
typedef struct _GETVERSIONOUTPARAMS
{
    BYTE bVersion;      // Binary driver version.
    BYTE bRevision;     // Binary driver revision.
    BYTE bReserved;     // Not used.
    BYTE bIDEDeviceMap; // Bit map of IDE devices.
    DWORD fCapabilities; // Bit mask of driver capabilities.
    DWORD dwReserved[4]; // For future use.
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;



//  Bits returned in the fCapabilities member of GETVERSIONOUTPARAMS
#define  CAP_IDE_ID_FUNCTION             1  // ATA ID command supported
#define  CAP_IDE_ATAPI_ID                2  // ATAPI ID command supported
#define  CAP_IDE_EXECUTE_SMART_FUNCTION  4  // SMART commannds supported


//  Valid values for the bCommandReg member of IDEREGS.
#define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
#define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.


// The following struct defines the interesting part of the IDENTIFY
// buffer:
typedef struct _IDSECTOR
{
    USHORT  wGenConfig;
    USHORT  wNumCyls;
    USHORT  wReserved;
    USHORT  wNumHeads;
    USHORT  wBytesPerTrack;
    USHORT  wBytesPerSector;
    USHORT  wSectorsPerTrack;
    USHORT  wVendorUnique[3];
    CHAR    sSerialNumber[20];
    USHORT  wBufferType;
    USHORT  wBufferSize;
    USHORT  wECCSize;
    CHAR    sFirmwareRev[8];
    CHAR    sModelNumber[40];
    USHORT  wMoreVendorUnique;
    USHORT  wDoubleWordIO;
    USHORT  wCapabilities;
    USHORT  wReserved1;
    USHORT  wPIOTiming;
    USHORT  wDMATiming;
    USHORT  wBS;
    USHORT  wNumCurrentCyls;
    USHORT  wNumCurrentHeads;
    USHORT  wNumCurrentSectorsPerTrack;
    ULONG   ulCurrentSectorCapacity;
    USHORT  wMultSectorStuff;
    ULONG   ulTotalAddressableSectors;
    USHORT  wSingleWordDMA;
    USHORT  wMultiWordDMA;
    BYTE    bReserved[128];
} IDSECTOR, *PIDSECTOR;


typedef struct _SRB_IO_CONTROL
{
    ULONG HeaderLength;
    UCHAR Signature[8];
    ULONG Timeout;
    ULONG ControlCode;
    ULONG ReturnCode;
    ULONG Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;


// Define global buffers.
BYTE IdOutCmd [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];

namespace ta
{
    namespace windhowshddutils
    {
        using std::string;

        char* ConvertToString (DWORD diskdata [256], int firstIndex, int lastIndex);
        void setHddInfo (int drive, DWORD diskdata [256], string& anHddSerial, string& anHddModel);
        BOOL DoIDENTIFY (HANDLE, PSENDCMDINPARAMS, PSENDCMDOUTPARAMS, BYTE, BYTE, PDWORD);
        bool getPrimaryHardDriveInfo(string& anHddSerial, string& anHddModel, string* anErrorInfoPtr);

        //
        // IDENTIFY data (from ATAPI driver source)
        //

#pragma pack(1)

        typedef struct _IDENTIFY_DATA {
            USHORT GeneralConfiguration;            // 00 00
            USHORT NumberOfCylinders;               // 02  1
            USHORT Reserved1;                       // 04  2
            USHORT NumberOfHeads;                   // 06  3
            USHORT UnformattedBytesPerTrack;        // 08  4
            USHORT UnformattedBytesPerSector;       // 0A  5
            USHORT SectorsPerTrack;                 // 0C  6
            USHORT VendorUnique1[3];                // 0E  7-9
            USHORT SerialNumber[10];                // 14  10-19
            USHORT BufferType;                      // 28  20
            USHORT BufferSectorSize;                // 2A  21
            USHORT NumberOfEccBytes;                // 2C  22
            USHORT FirmwareRevision[4];             // 2E  23-26
            USHORT ModelNumber[20];                 // 36  27-46
            UCHAR  MaximumBlockTransfer;            // 5E  47
            UCHAR  VendorUnique2;                   // 5F
            USHORT DoubleWordIo;                    // 60  48
            USHORT Capabilities;                    // 62  49
            USHORT Reserved2;                       // 64  50
            UCHAR  VendorUnique3;                   // 66  51
            UCHAR  PioCycleTimingMode;              // 67
            UCHAR  VendorUnique4;                   // 68  52
            UCHAR  DmaCycleTimingMode;              // 69
            USHORT TranslationFieldsValid:1;        // 6A  53
            USHORT Reserved3:15;
            USHORT NumberOfCurrentCylinders;        // 6C  54
            USHORT NumberOfCurrentHeads;            // 6E  55
            USHORT CurrentSectorsPerTrack;          // 70  56
            ULONG  CurrentSectorCapacity;           // 72  57-58
            USHORT CurrentMultiSectorSetting;       //     59
            ULONG  UserAddressableSectors;          //     60-61
            USHORT SingleWordDMASupport : 8;        //     62
            USHORT SingleWordDMAActive : 8;
            USHORT MultiWordDMASupport : 8;         //     63
            USHORT MultiWordDMAActive : 8;
            USHORT AdvancedPIOModes : 8;            //     64
            USHORT Reserved4 : 8;
            USHORT MinimumMWXferCycleTime;          //     65
            USHORT RecommendedMWXferCycleTime;      //     66
            USHORT MinimumPIOCycleTime;             //     67
            USHORT MinimumPIOCycleTimeIORDY;        //     68
            USHORT Reserved5[2];                    //     69-70
            USHORT ReleaseTimeOverlapped;           //     71
            USHORT ReleaseTimeServiceCommand;       //     72
            USHORT MajorRevision;                   //     73
            USHORT MinorRevision;                   //     74
            USHORT Reserved6[50];                   //     75-126
            USHORT SpecialFunctionsEnabled;         //     127
            USHORT Reserved7[128];                  //     128-255
        } IDENTIFY_DATA, *PIDENTIFY_DATA;

#pragma pack()

        // throw std::runtime_error on error
        void ReadPhysicalDriveInNTUsingSmart (string& anHddSerial, string& anHddModel)
        {
            int drive = 0;
            HANDLE hPhysicalDriveIOCTL = 0;

            //  Try to get a handle to PhysicalDrive IOCTL
            char driveName [256] = {};
            sprintf (driveName, "\\\\.\\PhysicalDrive%d", drive);

            //  Windows NT, Windows 2000, Windows Server 2003, Vista
            hPhysicalDriveIOCTL = ::CreateFile (driveName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
                TA_THROW_MSG(std::runtime_error, boost::format("Unable to open physical drive %s, error code: 0x%lX") % driveName % ::GetLastError());
            GETVERSIONINPARAMS GetVersionParams = {0};
            DWORD cbBytesReturned = 0;
            if (!::DeviceIoControl (hPhysicalDriveIOCTL, SMART_GET_VERSION, NULL, 0, &GetVersionParams, sizeof (GETVERSIONINPARAMS), &cbBytesReturned, NULL))
            {
                int myError = ::GetLastError();
                ::CloseHandle(hPhysicalDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, boost::format("DeviceIoControl(SMART_GET_VERSION) failed for physical drive %s, error code: 0x%lX") % driveName % myError);
            }
            ULONG CommandSize = sizeof(SENDCMDINPARAMS) + IDENTIFY_BUFFER_SIZE;
            ta::ScopedResource<PSENDCMDINPARAMS> Command((PSENDCMDINPARAMS)malloc(CommandSize), free);
            // Retrieve the IDENTIFY data
#define ID_CMD          0xEC            // Returns ID sector for ATA
            Command -> irDriveRegs.bCommandReg = ID_CMD;
            DWORD BytesReturned = 0;
            if (!::DeviceIoControl (hPhysicalDriveIOCTL, SMART_RCV_DRIVE_DATA, Command, sizeof(SENDCMDINPARAMS), Command, CommandSize, &BytesReturned, NULL) )
            {
                int myError = ::GetLastError();
                ::CloseHandle(hPhysicalDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, boost::format("DeviceIoControl(SMART_RCV_DRIVE_DATA) failed for physical drive %s, error code: 0x%lX") % driveName % myError);
            }
            DWORD diskdata [256] = {};
            USHORT* pIdSector = (USHORT*)(PIDENTIFY_DATA) (Command -> bBuffer);
            for (int ijk = 0; ijk < 256; ijk++)
                diskdata [ijk] = pIdSector [ijk];

            setHddInfo (drive, diskdata, anHddSerial, anHddModel);
            ::CloseHandle (hPhysicalDriveIOCTL);
        }


        //  Required to ensure correct PhysicalDrive IOCTL structure setup
#pragma pack(4)


        //
        // IOCTL_STORAGE_QUERY_PROPERTY
        //
        // Input Buffer:
        //      a STORAGE_PROPERTY_QUERY structure which describes what type of query
        //      is being done, what property is being queried for, and any additional
        //      parameters which a particular property query requires.
        //
        //  Output Buffer:
        //      Contains a buffer to place the results of the query into.  Since all
        //      property descriptors can be cast into a STORAGE_DESCRIPTOR_HEADER,
        //      the IOCTL can be called once with a small buffer then again using
        //      a buffer as large as the header reports is necessary.
        //


        //
        // Types of queries
        //

        typedef enum _STORAGE_QUERY_TYPE {
            PropertyStandardQuery = 0,          // Retrieves the descriptor
            PropertyExistsQuery,                // Used to test whether the descriptor is supported
            PropertyMaskQuery,                  // Used to retrieve a mask of writeable fields in the descriptor
            PropertyQueryMaxDefined     // use to validate the value
        } STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

        //
        // define some initial property id's
        //

        typedef enum _STORAGE_PROPERTY_ID {
            StorageDeviceProperty = 0,
            StorageAdapterProperty
        } STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

        //
        // Query structure - additional parameters for specific queries can follow
        // the header
        //

        typedef struct _STORAGE_PROPERTY_QUERY {

            //
            // ID of the property being retrieved
            //

            STORAGE_PROPERTY_ID PropertyId;

            //
            // Flags indicating the type of query being performed
            //

            STORAGE_QUERY_TYPE QueryType;

            //
            // Space for additional parameters if necessary
            //

            UCHAR AdditionalParameters[1];

        } STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;


#define IOCTL_STORAGE_QUERY_PROPERTY   CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)


        //
        // Device property descriptor - this is really just a rehash of the inquiry
        // data retrieved from a scsi device
        //
        // This may only be retrieved from a target device.  Sending this to the bus
        // will result in an error
        //

#pragma pack(4)

        typedef struct _STORAGE_DEVICE_DESCRIPTOR {

            //
            // Sizeof(STORAGE_DEVICE_DESCRIPTOR)
            //
            ULONG Version;

            //
            // Total size of the descriptor, including the space for additional
            // data and id strings
            //
            ULONG Size;

            //
            // The SCSI-2 device type
            //
            UCHAR DeviceType;

            //
            // The SCSI-2 device type modifier (if any) - this may be zero
            //
            UCHAR DeviceTypeModifier;

            //
            // Flag indicating whether the device's media (if any) is removable.  This
            // field should be ignored for media-less devices
            //
            BOOLEAN RemovableMedia;

            //
            // Flag indicating whether the device can support mulitple outstanding
            // commands.  The actual synchronization in this case is the responsibility
            // of the port driver.
            //
            BOOLEAN CommandQueueing;

            //
            // Byte offset to the zero-terminated ascii string containing the device's
            // vendor id string.  For devices with no such ID this will be zero
            //
            ULONG VendorIdOffset;

            //
            // Byte offset to the zero-terminated ascii string containing the device's
            // product id string.  For devices with no such ID this will be zero
            //
            ULONG ProductIdOffset;

            //
            // Byte offset to the zero-terminated ascii string containing the device's
            // product revision string.  For devices with no such string this will be
            // zero
            //
            ULONG ProductRevisionOffset;

            //
            // Byte offset to the zero-terminated ascii string containing the device's
            // serial number.  For devices with no serial number this will be zero
            //
            ULONG SerialNumberOffset;

            //
            // Contains the bus type (as defined above) of the device.  It should be
            // used to interpret the raw device properties at the end of this structure
            // (if any)
            //
            STORAGE_BUS_TYPE BusType;

            //
            // The number of bytes of bus-specific data which have been appended to
            // this descriptor
            //
            ULONG RawPropertiesLength;

            //
            // Place holder for the first byte of the bus specific property data
            //
            UCHAR RawDeviceProperties[1];

        } STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;


        //  function to decode the serial numbers of IDE hard drives
        //  using the IOCTL_STORAGE_QUERY_PROPERTY command
        char* flipAndCodeBytes (const char* str,
                                int pos,
                                int flip,
                                char* buf)
        {
            int i;
            int j = 0;
            int k = 0;

            buf [0] = '\0';
            if (pos <= 0)
                return buf;

            if ( ! j)
            {
                char p = 0;

                // First try to gather all characters representing hex digits only.
                j = 1;
                k = 0;
                buf[k] = 0;
                for (i = pos; j && str[i] != '\0'; ++i)
                {
                    int c = tolower(str[i]);

                    if (isspace(c))
                        c = '0';

                    ++p;
                    buf[k] <<= 4;

                    if (c >= '0' && c <= '9')
                        buf[k] |= (unsigned char) (c - '0');
                    else if (c >= 'a' && c <= 'f')
                        buf[k] |= (unsigned char) (c - 'a' + 10);
                    else
                    {
                        j = 0;
                        break;
                    }

                    if (p == 2)
                    {
                        if (buf[k] != '\0' && ! isprint(buf[k]))
                        {
                            j = 0;
                            break;
                        }
                        ++k;
                        p = 0;
                        buf[k] = 0;
                    }

                }
            }

            if ( ! j)
            {
                // There are non-digit characters, gather them as is.
                j = 1;
                k = 0;
                for (i = pos; j && str[i] != '\0'; ++i)
                {
                    char c = str[i];

                    if ( ! isprint(c))
                    {
                        j = 0;
                        break;
                    }

                    buf[k++] = c;
                }
            }

            if ( ! j)
            {
                // The characters are not there or are not printable.
                k = 0;
            }

            buf[k] = '\0';

            if (flip)
                // Flip adjacent characters
                for (j = 0; j < k; j += 2)
                {
                    char t = buf[j];
                    buf[j] = buf[j + 1];
                    buf[j + 1] = t;
                }

            // Trim any beginning and end space
            i = j = -1;
            for (k = 0; buf[k] != '\0'; ++k)
            {
                if (! isspace(buf[k]))
                {
                    if (i < 0)
                        i = k;
                    j = k;
                }
            }

            if ((i >= 0) && (j >= 0))
            {
                for (k = i; (k <= j) && (buf[k] != '\0'); ++k)
                    buf[k - i] = buf[k];
                buf[k - i] = '\0';
            }

            return buf;
        }

        // throw std::runtime_error on error
        void ReadPhysicalDriveInNTWithZeroRights (string& anHddSerial, string& anHddModel)
        {
            int drive = 0;
            //  Try to get a handle to PhysicalDrive IOCTL
            char driveName [256] = {};
            sprintf (driveName, "\\\\.\\PhysicalDrive%d", drive);

            //  Windows NT, Windows 2000, Windows XP - admin rights not required
            HANDLE hPhysicalDriveIOCTL = ::CreateFile (driveName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
                TA_THROW_MSG(std::runtime_error, boost::format("Unable to open physical drive %s, error code: 0x%lX") % driveName % ::GetLastError());

            STORAGE_PROPERTY_QUERY query;
            memset(&query, 0, sizeof(query));
            DWORD cbBytesReturned = 0;
            char buffer [10000] = {};

            query.PropertyId = StorageDeviceProperty;
            query.QueryType = PropertyStandardQuery;

            if (!:: DeviceIoControl (hPhysicalDriveIOCTL, IOCTL_STORAGE_QUERY_PROPERTY,
                                     &query, sizeof(query), &buffer, sizeof(buffer), &cbBytesReturned, NULL) )
            {
                int myError = ::GetLastError();
                ::CloseHandle(hPhysicalDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, boost::format("DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY) failed for physical drive %s, error code: 0x%lX") % driveName % myError);
            }

            STORAGE_DEVICE_DESCRIPTOR* descrip = (STORAGE_DEVICE_DESCRIPTOR*) & buffer;
            char serialNumber [1000] = {};
            char modelNumber [1000] = {};

            flipAndCodeBytes (buffer, descrip -> ProductIdOffset, 0, modelNumber );
            flipAndCodeBytes (buffer, descrip -> SerialNumberOffset, 1, serialNumber );

            if (!strlen(serialNumber) ||  !isalnum (serialNumber [0]) || !isalnum (serialNumber [19]))
            {
                ::CloseHandle(hPhysicalDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, "Invalid serial number");
            }
            anHddSerial = serialNumber;
            anHddModel = modelNumber;
            ::CloseHandle (hPhysicalDriveIOCTL);
        }


        // DoIDENTIFY
        // FUNCTION: Send an IDENTIFY command to the drive
        // bDriveNum = 0-3
        // bIDCmd = IDE_ATA_IDENTIFY or IDE_ATAPI_IDENTIFY
        BOOL DoIDENTIFY (HANDLE hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
                         PSENDCMDOUTPARAMS pSCOP, BYTE bIDCmd, BYTE bDriveNum,
                         PDWORD lpcbBytesReturned)
        {
            // Set up data structures for IDENTIFY command.
            pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;
            pSCIP -> irDriveRegs.bFeaturesReg = 0;
            pSCIP -> irDriveRegs.bSectorCountReg = 1;
            //pSCIP -> irDriveRegs.bSectorNumberReg = 1;
            pSCIP -> irDriveRegs.bCylLowReg = 0;
            pSCIP -> irDriveRegs.bCylHighReg = 0;

            // Compute the drive number.
            pSCIP -> irDriveRegs.bDriveHeadReg = 0xA0 | ((bDriveNum & 1) << 4);

            // The command can either be IDE identify or ATAPI identify.
            pSCIP -> irDriveRegs.bCommandReg = bIDCmd;
            pSCIP -> bDriveNumber = bDriveNum;
            pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;

            return ( DeviceIoControl (hPhysicalDriveIOCTL, DFP_RECEIVE_DRIVE_DATA,
                                      (LPVOID) pSCIP,
                                      sizeof(SENDCMDINPARAMS) - 1,
                                      (LPVOID) pSCOP,
                                      sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
                                      lpcbBytesReturned, NULL) );
        }

#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE

        // throw std::runtime_error on error
        void ReadIdeDriveAsScsiDriveInNT (string& anHddSerial, string& anHddModel)
        {
            int controller = 0;
            HANDLE hScsiDriveIOCTL = 0;
            char   driveName [256] = {};
            //  Try to get a handle to PhysicalDrive IOCTL
            sprintf (driveName, "\\\\.\\Scsi%d:", controller);

            //  Windows NT, Windows 2000, any rights should do
            hScsiDriveIOCTL = ::CreateFile (driveName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hScsiDriveIOCTL == INVALID_HANDLE_VALUE)
                TA_THROW_MSG(std::runtime_error, boost::format("Unable to open physical drive %s, error code: 0x%lX\n") % driveName % ::GetLastError());
            BYTE drive = 0;
            char buffer [sizeof (SRB_IO_CONTROL) + SENDIDLENGTH] = {};
            SRB_IO_CONTROL* p = (SRB_IO_CONTROL*) buffer;
            SENDCMDINPARAMS* pin = (SENDCMDINPARAMS*) (buffer + sizeof (SRB_IO_CONTROL));
            DWORD dummy;

            p -> HeaderLength = sizeof (SRB_IO_CONTROL);
            p -> Timeout = 10000;
            p -> Length = SENDIDLENGTH;
            p -> ControlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
            strncpy ((char*) p -> Signature, "SCSIDISK", 8);
            pin -> irDriveRegs.bCommandReg = IDE_ATA_IDENTIFY;
            pin -> bDriveNumber = drive;

            if (!::DeviceIoControl (hScsiDriveIOCTL, IOCTL_SCSI_MINIPORT, buffer, sizeof (SRB_IO_CONTROL) + sizeof (SENDCMDINPARAMS) - 1, buffer, sizeof (SRB_IO_CONTROL) + SENDIDLENGTH, &dummy, NULL))
            {
                int myError = ::GetLastError();
                ::CloseHandle (hScsiDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, boost::format("DeviceIoControl(IOCTL_SCSI_MINIPORT) failed for physical drive %s, error code: 0x%lX\n") % driveName % myError);
            }
            SENDCMDOUTPARAMS* pOut = (SENDCMDOUTPARAMS*) (buffer + sizeof (SRB_IO_CONTROL));
            IDSECTOR* pId = (IDSECTOR*) (pOut -> bBuffer);
            if (!pId -> sModelNumber [0])
            {
                ::CloseHandle (hScsiDriveIOCTL);
                TA_THROW_MSG(std::runtime_error, "pId -> sModelNumber [0] is NULL");
            }
            DWORD diskdata [256] = {};
            USHORT* pIdSector = (USHORT*) pId;
            for (int ijk = 0; ijk < 256; ijk++)
                diskdata [ijk] = pIdSector [ijk];

            setHddInfo (controller * 2 + drive, diskdata, anHddSerial, anHddModel);
            ::CloseHandle (hScsiDriveIOCTL);
        }


        void setHddInfo (int UNUSED(drive), DWORD diskdata [256], string& anHddSerial, string& anHddModel)
        {
            char mySerial [1024] = {};
            strcpy (mySerial, ConvertToString (diskdata, 10, 19));
            if (anHddSerial.empty() &&
                    //  serial number must be alphanumeric (but there can be leading spaces on IBM drives)
                    (isalnum (mySerial [0]) || isalnum (mySerial [19])))
            {
                anHddSerial = mySerial;
                anHddModel = ConvertToString (diskdata, 27, 46);
                boost::trim(anHddSerial);
                boost::trim(anHddModel);
            }
        }


        char* ConvertToString (DWORD diskdata [256], int firstIndex, int lastIndex)
        {
            static char s [1024];
            int index = 0;
            int position = 0;

            //  each integer has two characters stored in it backwards
            for (index = firstIndex; index <= lastIndex; index++)
            {
                //  get high byte for 1st character
                s [position] = (char) (diskdata [index] / 256);
                position++;
                //  get low byte for 2nd character
                s [position] = (char) (diskdata [index] % 256);
                position++;
            }
            s [position] = '\0';
            //  cut off the trailing blanks
            for (index = position - 1; index > 0 && isspace(s [index]); index--)
                s [index] = '\0';
            return s;
        }

        bool getPrimaryHardDriveSerial(string& aSerial)
        {
            aSerial = "";
            string myHddModelDummy; // HDD model is not needed, get it just to do not change interface

            OSVERSIONINFO version = {0};
            version.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
            ::GetVersionEx (&version);
            if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
            {
                try
                {
                    //  this works under WinNT4 or Win2K or WinXP or Windows Server 2003 or Vista if you have any rights
                    ReadPhysicalDriveInNTWithZeroRights (aSerial, myHddModelDummy);
                    return true;
                }
                catch (std::exception& e)
                {
                    WARNDEVLOG(e.what());
                }
                try
                {
                    //  this should work in WinNT or Win2K if previous did not work
                    //  this is kind of a backdoor via the SCSI mini port driver into the IDE drives
                    ReadIdeDriveAsScsiDriveInNT (aSerial, myHddModelDummy);
                    return true;
                }
                catch (std::exception& e)
                {
                    WARNDEVLOG(e.what());
                }

                try
                {
                    //  this works under WinNT4 or Win2K or WinXP or Windows Server 2003 or Vista if you have any rights
                    ReadPhysicalDriveInNTUsingSmart(aSerial, myHddModelDummy);
                    return true;
                }
                catch (std::exception& e)
                {
                    WARNDEVLOG(e.what());
                }
            }
            else
            {
                WARNLOG("Cannot retrieve HDD serial. Only NT-based Windows platrofms supported");
            }
            return false;
        }

    }
}
