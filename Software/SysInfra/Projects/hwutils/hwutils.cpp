/*
 * Some implementation details are borrowed from dmidecode project (http://www.nongnu.org/dmidecode/).
 */

#if defined(__linux__)

#include "hwutils.h"
#include "ta/scopedresource.hpp"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/strings.h"
#include "ta/utils.h"
#include "ta/process.h"
#include "ta/common.h"

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif /* !MAP_FAILED */

#include <stdexcept>
#include <string>

#include "boost/regex.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/filesystem/operations.hpp"

using std::string;
using std::vector;


enum Command { commandGetSystemSerial, commandGetPrimaryHddSerial};

static const vector<string> HDDs = boost::assign::list_of("/dev/sda")("/dev/hda");

static bool isBlockDevice(const string& aFilePath)
{
    namespace fs = boost::filesystem;
    fs::path myPath(aFilePath);
    return (fs::exists(myPath) && fs::status(myPath).type() == fs::block_file);
}

static string getPrimaryHDDSerial()
{
    foreach (const string& hdd, HDDs)
    {
        if (isBlockDevice(hdd))
        {
            try
            {
                const string myCmd = "/sbin/hdparm -i " + hdd;
                string myStdOut, myStdErr;
                const int myErrorCode = ta::Process::shellExecSync(myCmd,  myStdOut, myStdErr);
                if (myErrorCode != 0)
                {
                    // mostly fails on VMs
                    WARNLOG2("Failed to retrieve HDD serial for " + hdd + ". Trying another disk",
                             str(boost::format("Command %s finished with code %s. Stderr: %s. Stdout: %s") % myCmd % myErrorCode % myStdErr % myStdOut));
                    continue;
                }

                // read serial
                static const boost::regex myRegEx("\\s+SerialNo\\s*\\=\\s*(?<serial>[\\w\\-]+)");
                boost::cmatch myMatch;
                if (!regex_search(myStdOut.c_str(), myMatch, myRegEx))
                {
                    WARNLOG2("Failed to retrieve HDD serial for " + hdd + ". Trying another disk",
                             str(boost::format("Cannot parse HDD serial from %s (command %s)") % myStdOut % myCmd));
                    continue;
                }
                return myMatch["serial"];
            }
            catch (std::exception& e)
            {
                WARNLOG2("Failed to retrieve HDD serial for " + hdd + ". Trying another disk",
                         e.what());
            }
        }
        else
        {
            WARNLOG("Block device " + hdd + " does not exist. Trying another disk.");
        }
    }

    TA_THROW_MSG(std::runtime_error, "Failed to retrieve HDD serial from any of block devices: " + ta::Strings::join(HDDs, ", "));
}


typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;


#ifdef __ia64__
#define ALIGNMENT_WORKAROUND
#endif

#ifdef ALIGNMENT_WORKAROUND
#   ifdef BIGENDIAN
#   define WORD(x) (u16)((x)[1] + ((x)[0] << 8))
#   define DWORD(x) (u32)((x)[3] + ((x)[2] << 8) + ((x)[1] << 16) + ((x)[0] << 24))
#   else /* BIGENDIAN */
#   define WORD(x) (u16)((x)[0] + ((x)[1] << 8))
#   define DWORD(x) (u32)((x)[0] + ((x)[1] << 8) + ((x)[2] << 16) + ((x)[3] << 24))
#   endif /* BIGENDIAN */
#else /* ALIGNMENT_WORKAROUND */
#define WORD(x) (u16)(*(const u16 *)(x))
#define DWORD(x) (u32)(*(const u32 *)(x))
#endif /* ALIGNMENT_WORKAROUND */

struct string_keyword
{
    u8 type;
    u8 offset;
};

struct opt
{
    const char* devmem;
    unsigned int flags;
    const struct string_keyword* string;
};

#define FLAG_DUMP               (1 << 2)
#define FLAG_QUIET              (1 << 3)
#define DEFAULT_MEM_DEV "/dev/mem"
#define out_of_spec "<OUT OF SPEC>"
static const char* bad_index = "<BAD INDEX>";

struct dmi_header
{
    u8 type;
    u8 length;
    u16 handle;
    u8* data;
};

struct opt opt;

static const struct string_keyword opt_system_serial_nr       = { 1, 0x07 }; //system-serial-number

#define SUPPORTED_SMBIOS_VER 0x0207

using std::string;
using std::vector;


int read_fd(int fd, u8* buf, size_t count, const char* filename_hint)
{
    ssize_t r = 1;
    size_t r2 = 0;

    while (r2 != count && r != 0)
    {
        r = read(fd, buf + r2, count - r2);
        if (r == -1)
        {
            if (errno != EINTR)
            {
                ERRORLOG(boost::format("Failed to read from %s: %s") % filename_hint % strerror(errno));
                return -1;
            }
        }
        else
            r2 += r;
    }

    if (r2 != count)
    {
        ERRORLOG(boost::format("Failed to read from %s: Unexpected end of file") % filename_hint);
        return -1;
    }

    return 0;
}

/*
 * Copy a physical memory chunk into a memory buffer.
 * This function allocates memory.
 * @note the function requires root privileges
 */
void* mem_chunk(size_t base, size_t len, const char* devmem)
{
    ta::ScopedResource<int> fd(open(devmem, O_RDONLY), close, -1);

    if (fd == -1)
    {
        ERRORLOG(boost::format("Failed to open %s: %s") % devmem % strerror(errno));
        return NULL;
    }

    void* p = malloc(len);
    if (!p)
    {
        ERRORLOG(boost::format("malloc: %s") % strerror(errno));
        return NULL;
    }

#ifdef _SC_PAGESIZE
    size_t mmoffset = base % sysconf(_SC_PAGESIZE);
#else
    size_t mmoffset = base % getpagesize();
#endif /* _SC_PAGESIZE */
    void* mmp = mmap(0, mmoffset + len, PROT_READ, MAP_SHARED, fd, base - mmoffset);
    if (mmp == MAP_FAILED)
    {
        if (lseek(fd, base, SEEK_SET) == -1)
        {
            ERRORLOG(boost::format("%s: lseek: %s") % devmem % strerror(errno));
            free(p);
            return NULL;
        }

        if (read_fd(fd, (u8*)p, len, devmem) == -1)
        {
            free(p);
            return NULL;
        }
        return p;
    }

    memcpy(p, (u8*)mmp + mmoffset, len);

    if (munmap(mmp, mmoffset + len) == -1)
    {
        WARNLOG(boost::format("Failed to unmap %s: %s") % devmem % strerror(errno));
    }
    return p;
}

int checksum(const u8* buf, size_t len)
{
    u8 sum = 0;
    size_t a;

    for (a = 0; a < len; a++)
        sum += buf[a];
    return (sum == 0);
}

const char* dmi_string(const struct dmi_header* dm, u8 s)
{
    char* bp = (char*)dm->data;
    size_t i, len;

    if (s == 0)
        return "Not Specified";

    bp += dm->length;
    while (s > 1 && *bp)
    {
        bp += strlen(bp);
        bp++;
        s--;
    }

    if (!*bp)
        return bad_index;

    if (!(opt.flags & FLAG_DUMP))
    {
        /* ASCII filtering */
        len = strlen(bp);
        for (i = 0; i < len; i++)
            if (bp[i] < 32 || bp[i] == 127)
                bp[i] = '.';
    }

    return bp;
}


void dmi_system_uuid(const u8* p, u16 ver)
{
    int only0xFF = 1, only0x00 = 1;
    int i;
    char str[50];

    for (i = 0; i < 16 && (only0x00 || only0xFF); i++)
    {
        if (p[i] != 0x00) only0x00 = 0;
        if (p[i] != 0xFF) only0xFF = 0;
    }

    if (only0xFF)
    {
        sprintf(str, "Not Present");
        return;
    }
    if (only0x00)
    {
        sprintf(str, "Not Settable");
        return;
    }

    /*
     * As of version 2.6 of the SMBIOS specification, the first 3
     * fields of the UUID are supposed to be encoded on little-endian.
     * The specification says that this is the defacto standard,
     * however I've seen systems following RFC 4122 instead and use
     * network byte order, so I am reluctant to apply the byte-swapping
     * for older versions.
     */
    if (ver >= 0x0206)
        sprintf(str, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                p[3], p[2], p[1], p[0], p[5], p[4], p[7], p[6],
                p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    else
        sprintf(str, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}


const char* dmi_processor_frequency(const u8* p)
{
    // First cast to void* in order to silence the alignment warnings.
    u16 code = WORD((void*)p);
    static char str[30];

    if (code)
        sprintf(str, "%u MHz", code);
    else
        sprintf(str, "Unknown");

    return str;
}


void to_dmi_header(struct dmi_header* h, u8* data)
{
    h->type = data[0];
    h->length = data[1];
    // First cast to void* in order to silence the alignment warnings.
    h->handle = WORD((void*)(data + 2));
    h->data = data;
}

const char* dmi_table_string(const struct dmi_header* h, const u8* data, u16 ver)
{
    int key;
    u8 offset = opt.string->offset;

    if (offset >= h->length)
        return "";

    key = (opt.string->type << 8) | offset;
    switch (key)
    {
    case 0x108:
        dmi_system_uuid(data + offset, ver);
        break;
    case 0x416:
        return dmi_processor_frequency(data + offset);
        break;
    default:
        return dmi_string(h, data[offset]);
    }

    return "";
}

const char* dmi_table(u32 base, u16 len, u16 num, u16 ver, const char* devmem)
{
    u8* buf;
    u8* data;
    int i = 0;

    if (ver > SUPPORTED_SMBIOS_VER)
    {
        ERRORLOG(boost::format("# SMBIOS implementations newer than version %u.%u")
                 % (SUPPORTED_SMBIOS_VER >> 8) % (SUPPORTED_SMBIOS_VER & 0xFF));
        return "";
    }

    if ((buf =(u8*) mem_chunk(base, len, devmem)) == NULL)
    {
        ERRORLOG( "The table is not reachable.");
        return "";
    }

    data = buf;
    while (i < num && data+4 <= buf + len) /* 4 is the length of an SMBIOS structure header */
    {
        u8* next;
        struct dmi_header h;

        to_dmi_header(&h, data);

        /*
         * If a short entry is found (less than 4 bytes), not only it
         * is invalid, but we cannot reliably locate the next entry.
         * Better stop at this point, and let the user know his/her
         * table is broken.
         */
        if (h.length < 4)
        {
            ERRORLOG(boost::format("Invalid length (%u)") % (unsigned int)h.length);
            opt.flags |= FLAG_QUIET;
            break;
        }

        /* In quiet mode, stop decoding at end of table marker */
        if ((opt.flags & FLAG_QUIET) && h.type == 127)
            break;

        /* look for the next handle */
        next = data + h.length;
        while (next - buf + 1 < len && (next[0] != 0 || next[1] != 0))
            next++;
        next += 2;

        if (opt.string != NULL
                && opt.string->type == h.type)
        {
            free(buf);
            return dmi_table_string(&h, data, ver);
        }

        data = next;
        i++;
    }

    free(buf);
    return "";
}


int smbios_decode(u8* buf, const char* devmem, char* str)
{
    u16 ver;

    if (!checksum(buf, buf[0x05])
            || memcmp(buf + 0x10, "_DMI_", 5) != 0
            || !checksum(buf + 0x10, 0x0F))
        return 0;

    ver = (buf[0x06] << 8) + buf[0x07];
    /* Some BIOS report weird SMBIOS version, fix that up */
    switch (ver)
    {
    case 0x021F:
    case 0x0221:
        ver = 0x0203;
        break;
    case 0x0233:
        ver = 0x0206;
        break;
    }

    // First cast to void* in order to silence the alignment warnings.
    strcpy (str, dmi_table(DWORD((void*)(buf + 0x18)), WORD((void*)(buf + 0x16)), WORD((void*)(buf + 0x1C)),
                           ver, devmem));

    return 1;
}

int legacy_decode(u8* buf, const char* devmem)
{
    if (!checksum(buf, 0x0F))
        return 0;

    // First cast to void* in order to silence the alignment warnings.
    dmi_table(DWORD((void*)(buf + 0x08)), WORD((void*)(buf + 0x06)), WORD((void*)(buf + 0x0C)),
              ((buf[0x0E] & 0xF0) << 4) + (buf[0x0E] & 0x0F), devmem);

    return 1;
}

/*
 * Probe for EFI interface
 */
#define EFI_NOT_FOUND   (-1)
#define EFI_NO_SMBIOS   (-2)
int address_from_efi(size_t* address)
{
    FILE* efi_systab;
    const char* filename;
    char linebuf[64];

    *address = 0; /* Prevent compiler warning */

    /*
     * Linux up to 2.6.6: /proc/efi/systab
     * Linux 2.6.7 and up: /sys/firmware/efi/systab
     */
    if ((efi_systab = fopen(filename = "/sys/firmware/efi/systab", "r")) == NULL
            && (efi_systab = fopen(filename = "/proc/efi/systab", "r")) == NULL)
    {
        /* No EFI interface, fallback to memory scan */
        return EFI_NOT_FOUND;
    }
    int ret = EFI_NO_SMBIOS;
    while ((fgets(linebuf, sizeof(linebuf) - 1, efi_systab)) != NULL)
    {
        char* addrp = strchr(linebuf, '=');
        *(addrp++) = '\0';
        if (strcmp(linebuf, "SMBIOS") == 0)
        {
            *address = strtoul(addrp, NULL, 0);
            ret = 0;
            break;
        }
    }
    if (fclose(efi_systab) != 0)
        ERRORLOG(boost::format("%s: %s") % filename % strerror(errno));

    if (ret == EFI_NO_SMBIOS)
        ERRORLOG(boost::format("%s: SMBIOS entry point missing") % filename);
    return ret;
}

string getHwSystemSerial()
{
    opt.devmem = DEFAULT_MEM_DEV;
    opt.flags = FLAG_QUIET;
    opt.string = &opt_system_serial_nr;

    size_t fp;

    /* First try EFI (ia64, Intel-based Mac) */
    const int efi = address_from_efi(&fp);
    if (efi == EFI_NO_SMBIOS)
    {
        TA_THROW_MSG(std::runtime_error, "Failed to retrieve BIOS serial number using EFI (got EFI_NO_SMBIOS)");
    }

    char retVal[80] = {};
    if (efi != EFI_NOT_FOUND)
    {
        ta::ScopedResource<u8*> buf ((u8*)mem_chunk(fp, 0x20, opt.devmem), free);
        if (!buf)
        {
            TA_THROW_MSG(std::runtime_error, "Failed to retrieve HBIOS serial number using EFI");
        }

        smbios_decode(buf, opt.devmem, retVal);
        return retVal;
    }

    /* Fallback to memory scan (x86, x86_64) */
    ta::ScopedResource<u8*> buf ((u8*)mem_chunk(0xF0000, 0x10000, opt.devmem), free);
    if (!buf)
    {
        TA_THROW_MSG(std::runtime_error, "Failed to retrieve BIOS serial number using memory scan");
    }

    for (fp = 0; fp <= 0xFFF0; fp += 16)
    {
        if (memcmp(buf + fp, "_SM_", 4) == 0 && fp <= 0xFFE0)
        {
            if (smbios_decode(buf+fp, opt.devmem, retVal))
            {
                fp += 16;
            }
        }
        else if (memcmp(buf + fp, "_DMI_", 5) == 0)
        {
            legacy_decode(buf + fp, opt.devmem);
        }
    }
    return retVal;
}


bool parseCmdLineArgs(int argc, char* argv[], Command& aParsedCommand)
{
    DEBUGLOG(boost::format("Started hwutils with %d args") % (argc-1));
    if (argc == 2)
    {
        const string myOpt = argv[1];
        if (myOpt == HwUtils_GetSystemSerialArg)
        {
            aParsedCommand = commandGetSystemSerial;
            return true;
        }
        else if (myOpt == HwUtils_GetHddPrimarySerialArg)
        {
            aParsedCommand = commandGetPrimaryHddSerial;
            return true;
        }
    }
    ERRORLOG("Invalid arguments");
    return false;
}

void initLogger()
{
    const string myEnvInfo = str(boost::format("HW Utils tool (user: %s)") % ta::getUserName());
    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.consoleAppender = true;
    myMemConfig.consoleAppenderLogThreshold = ta::LogLevel::Debug;
    myMemConfig.consoleAppenderOutDev = ta::LogConfiguration::conDevStdErr;
    ta::LogConfiguration::instance().load(myMemConfig);
    PROLOG(myEnvInfo);
}

void deInitLogger()
{
    EPILOG("HW Utils tool");
}



int main(int argc, char* argv[])
{
    initLogger();
    int myRetVal = HwUtilsRetError;

    try
    {
        Command myParsedCommand;
        if (!parseCmdLineArgs(argc, argv, myParsedCommand))
        {
            return HwUtilsRetBadArgs;
        }

        switch (myParsedCommand)
        {
        case commandGetSystemSerial:
        {
            DEBUGLOG("Retrieving system serial");
            const string mySerial = getHwSystemSerial();
            std::cout << mySerial << std::endl;
            myRetVal = HwUtilsRetOk;
            break;
        }
        case commandGetPrimaryHddSerial:
        {
            DEBUGLOG("Retrieving primary HDD serial");
            const string mySerial = getPrimaryHDDSerial();
            std::cout << mySerial << std::endl;
            myRetVal = HwUtilsRetOk;
            break;
        }
        default:
        {
            ERRORLOG(boost::format("Unsupported command %d") % myParsedCommand);
            myRetVal = HwUtilsRetBadArgs;
            break;
        }
        }
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Error", e.what());
    }
    catch (...)
    {
        ERRORLOG("Unknown error");
    }

    INFOLOG(boost::format("Exiting with retval %d") % myRetVal);
    deInitLogger();

    return myRetVal;
}



#endif
