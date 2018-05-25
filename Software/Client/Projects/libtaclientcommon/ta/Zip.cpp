//----------------------------------------------------------------------------
//
//  Description : Zlib front-end utility to work with zip archives
// The most code is adopted from  miniunz.c and miniunz.c front-end under contrib/minizip/
// See zlib source code and Sandbox/zlibtest in KeyTalk source tree

//@todo improve error handling
//
//----------------------------------------------------------------------------
#include "Zip.h"
#include "ta/scopedresource.hpp"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/algorithm/string.hpp"

#if (!defined(_WIN32)) && (!defined(WIN32))
#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BIT
#define _FILE_OFFSET_BIT 64
#endif
#endif

#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define ZLIB_WINAPI

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
#endif


#include "zlib/unzip.h"
#include "zlib/zip.h"

#define EXTRACT_WRITEBUFFERSIZE (8192)
#define ARCHIVE_WRITEBUFFERSIZE (16384)
#ifdef __unix__
#define MAXFILENAME (256)
#endif

#ifdef _WIN32
#define USEWIN32IOAPI
#include "zlib/iowin32.h"
#endif

using std::string;

namespace ta
{
    namespace Zip
    {
        namespace
        {
            /* change_file_date : change the date/time of a file
            filename : the filename of the file where date/time must be modified
            dosdate : the new date at the MSDos format (4 bytes)
            tmu_date : the SAME new date at the tm_unz format */
#ifdef _WIN32
            void change_file_date(const char* filename, uLong dosdate, tm_unz UNUSED(tmu_date))
            {
                FILETIME ftm,ftLocal,ftCreate,ftLastAcc,ftLastWrite;
                HANDLE hFile = CreateFileA(filename,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
                GetFileTime(hFile,&ftCreate,&ftLastAcc,&ftLastWrite);
                DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate,&ftLocal);
                LocalFileTimeToFileTime(&ftLocal,&ftm);
                SetFileTime(hFile,&ftm,&ftLastAcc,&ftm);
                CloseHandle(hFile);
            }
#elif defined(__unix__)
            void change_file_date(const char* filename, uLong UNUSED(dosdate), tm_unz tmu_date)
            {
                struct utimbuf ut;
                struct tm newdate;
                newdate.tm_sec = tmu_date.tm_sec;
                newdate.tm_min=tmu_date.tm_min;
                newdate.tm_hour=tmu_date.tm_hour;
                newdate.tm_mday=tmu_date.tm_mday;
                newdate.tm_mon=tmu_date.tm_mon;
                if (tmu_date.tm_year > 1900)
                    newdate.tm_year=tmu_date.tm_year - 1900;
                else
                    newdate.tm_year=tmu_date.tm_year ;
                newdate.tm_isdst=-1;

                ut.actime=ut.modtime=mktime(&newdate);
                utime(filename,&ut);
            }
#else
#error Unsupported platform
#endif

            int do_extract_currentfile(unzFile uf, const string& anExtractDir)
            {
                char filename_inzip[256];
                char* filename_withoutpath;

                unz_file_info64 file_info;
                int err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
                if (err!=UNZ_OK)
                    return err;

                uInt size_buf = EXTRACT_WRITEBUFFERSIZE;
                void* buf = (void*)malloc(size_buf);
                if (buf==NULL)
                    return UNZ_INTERNALERROR;

                char* p = filename_withoutpath = filename_inzip;
                while ((*p) != '\0')
                {
                    if (((*p)=='/') || ((*p)=='\\'))
                        filename_withoutpath = p+1;
                    p++;
                }

                if ((*filename_withoutpath)=='\0')
                {
                    boost::filesystem::create_directories(anExtractDir + ta::getDirSep() + filename_inzip);
                }
                else
                {
                    FILE* fout=NULL;
                    const char* write_filename = filename_inzip;
                    err = unzOpenCurrentFile(uf);
                    if (err==UNZ_OK)
                    {
                        fout=FOPEN_FUNC((anExtractDir + ta::getDirSep() + write_filename).c_str(),"wb");

                        /* some zipfile don't contain directory alone before file */
                        if ((fout==NULL) && (filename_withoutpath!=(char*)filename_inzip))
                        {
                            char c=*(filename_withoutpath-1);
                            *(filename_withoutpath-1)='\0';
                            boost::filesystem::create_directories(anExtractDir + ta::getDirSep() + write_filename);
                            *(filename_withoutpath-1)=c;
                            fout=FOPEN_FUNC((anExtractDir + ta::getDirSep() + write_filename).c_str(),"wb");
                        }
                    }

                    if (fout!=NULL)
                    {
                        do
                        {
                            err = unzReadCurrentFile(uf,buf,size_buf);
                            if (err<0)
                            {
                                break;
                            }
                            if (err>0)
                            {
                                if (fwrite(buf,err,1,fout)!=1)
                                {
                                    err=UNZ_ERRNO;
                                    break;
                                }
                            }
                        }
                        while (err>0);

                        if (fout)
                            fclose(fout);

                        if (err==0)
                            change_file_date((anExtractDir + ta::getDirSep() + write_filename).c_str(),file_info.dosDate,
                                             file_info.tmu_date);
                    }

                    if (err==UNZ_OK)
                        err = unzCloseCurrentFile (uf);
                    else
                        unzCloseCurrentFile(uf); /* don't lose the error */
                }

                free(buf);
                return err;
            }

            int do_extract(unzFile uf, const string& anExtractDir)
            {
                uLong i;
                unz_global_info64 gi;

                int err = unzGetGlobalInfo64(uf,&gi);
                if (err!=UNZ_OK)
                    return err;

                for (i=0; i<gi.number_entry; i++)
                {
                    if (do_extract_currentfile(uf, anExtractDir) != UNZ_OK)
                        break;

                    if ((i+1)<gi.number_entry)
                    {
                        err = unzGoToNextFile(uf);
                        if (err!=UNZ_OK)
                            break;
                    }
                }

                return err;
            }


            /* f - name of file to get info on */
            /* tm_zip - return value: access, modific. and creation times */
            /* dt - dostime */
#ifdef _WIN32
            uLong getFiletime(const char* f, tm_zip* UNUSED(tmzip), uLong* dt)
            {
                int ret = 0;

                FILETIME ftLocal;
                HANDLE hFind;
                WIN32_FIND_DATAA ff32;

                hFind = FindFirstFileA(f,&ff32);
                if (hFind != INVALID_HANDLE_VALUE)
                {
                    FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
                    FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
                    FindClose(hFind);
                    ret = 1;
                }
                return ret;
            }
#elif defined(__unix__)
            uLong getFiletime(const char* f, tm_zip* tmzip, uLong* UNUSED(dt))
            {
                int ret=0;
                struct stat s;        /* results of stat() */
                struct tm* filedate;
                time_t tm_t=0;

                if (strcmp(f,"-")!=0)
                {
                    char name[MAXFILENAME+1];
                    int len = strlen(f);
                    if (len > MAXFILENAME)
                        len = MAXFILENAME;

                    strncpy(name, f,MAXFILENAME-1);
                    /* strncpy doesnt append the trailing NULL, of the string is too long. */
                    name[ MAXFILENAME ] = '\0';

                    if (name[len - 1] == '/')
                        name[len - 1] = '\0';
                    /* not all systems allow stat'ing a file with / appended */
                    if (stat(name,&s)==0)
                    {
                        tm_t = s.st_mtime;
                        ret = 1;
                    }
                }
                filedate = localtime(&tm_t);

                tmzip->tm_sec  = filedate->tm_sec;
                tmzip->tm_min  = filedate->tm_min;
                tmzip->tm_hour = filedate->tm_hour;
                tmzip->tm_mday = filedate->tm_mday;
                tmzip->tm_mon  = filedate->tm_mon ;
                tmzip->tm_year = filedate->tm_year;
                return ret;
            }
#endif


            bool isLargeFile(const char* filename)
            {
                bool largeFile = false;
                FILE* pFile = FOPEN_FUNC(filename, "rb");
                if (pFile)
                {
                    fseeko64(pFile, 0, SEEK_END);
                    ZPOS64_T pos = ftello64(pFile);
                    if(pos >= 0xffffffff)
                        largeFile = true;
                    fclose(pFile);
                }
                return largeFile;
            }


            void zipCloseWrapper(zipFile zf)
            {
                zipClose(zf, NULL);
            }

            void doAddFileToArchive(const string& anInFilePath, zipFile aZipFile, Actual2ArchivePathMapper anActual2ArchivePathMapper)
            {
                int opt_compress_level=Z_DEFAULT_COMPRESSION;
                unsigned long crcFile=0;

                zip_fileinfo zi;
                zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
                                         zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
                zi.dosDate = 0;
                zi.internal_fa = 0;
                zi.external_fa = 0;
                getFiletime(anInFilePath.c_str(), &zi.tmz_date, &zi.dosDate);

                const int zip64 = isLargeFile(anInFilePath.c_str()) ? 1 : 0;

                string myArchiveFilePath = boost::trim_left_copy_if(anInFilePath,boost::is_any_of(ta::getDirSep()));
                myArchiveFilePath = anActual2ArchivePathMapper(myArchiveFilePath);

                int err = zipOpenNewFileInZip3_64(aZipFile, myArchiveFilePath.c_str(), &zi,
                                                  NULL,0,NULL,0,NULL /* comment*/,
                                                  (opt_compress_level != 0) ? Z_DEFLATED : 0,
                                                  opt_compress_level,0,
                                                  -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                                  NULL, crcFile, zip64);
                if (err != ZIP_OK)
                    TA_THROW_MSG(ZipArchiveError, "Error opening " + anInFilePath + " in zipfile");

                ta::ScopedResource<FILE*> fin(FOPEN_FUNC(anInFilePath.c_str(),"rb"), fclose);
                if (!fin)
                    //@todo no need for zipCloseFileInZip ?
                    TA_THROW_MSG(ZipArchiveError, "Error opening " + anInFilePath + " for reading");

                size_t size_read = 0;
                do
                {
                    char buf[ARCHIVE_WRITEBUFFERSIZE];
                    size_read = fread(buf,1,sizeof(buf),fin);
                    if (size_read < sizeof(buf) && feof(fin)==0)
                        TA_THROW_MSG(ZipArchiveError, "Error reading " + anInFilePath);
                    if (size_read > 0)
                    {
                        if (zipWriteInFileInZip (aZipFile, buf, (unsigned int)size_read) < 0)
                            TA_THROW_MSG(ZipArchiveError, "Error writing " + anInFilePath + " in the zipfile.");
                    }
                } while (size_read>0);

                if (zipCloseFileInZip(aZipFile) != ZIP_OK)
                    TA_THROW_MSG(ZipArchiveError, "Error closing " + anInFilePath + " in the zipfile.");
            }

        } // end-of-unnamed ns


        //
        // Public API
        //
        string doNotChange(const string& aPath)
        {
            return aPath;
        }
        string makeStem(const string& aPath)
        {
            return aPath.substr(aPath.find_last_of(ta::getDirSep())+1);
        }

        void archive(const string& anOutArchivePath, const std::vector<string>& aFileList, Actual2ArchivePathMapper anActual2ArchivePathMapper)
        {
            if (anOutArchivePath.empty())
                TA_THROW_MSG(ZipArchiveError, "No archive file name specified");
            if (aFileList.empty())
                TA_THROW_MSG(ZipArchiveError, "Nothing to archive");

            try
            {
                boost::filesystem::remove_all(anOutArchivePath);

                ta::ScopedResource<zipFile> zf(zipOpen64(anOutArchivePath.c_str(), 0), zipCloseWrapper);
                if (!zf)
                    TA_THROW_MSG(ZipArchiveError, boost::format("Cannot open archive file %s") % anOutArchivePath);

                foreach (const string& inFile, aFileList)
                {
                    doAddFileToArchive(inFile, zf, anActual2ArchivePathMapper);
                }
            }
            catch (ZipArchiveError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(ZipArchiveError, e.what());
            }
        }

        string extract(const string& anArchivePath, const string& anOutDir)
        {
            try
            {
                ta::ScopedResource<unzFile> uf(unzOpen64(anArchivePath.c_str()), unzClose);
                if (!uf)
                    TA_THROW_MSG(ZipExtractError, boost::format("Cannot open %s or it is not a zip archive") % anArchivePath);

                string myExtractDir = boost::filesystem::path(anArchivePath).stem().string();
                if (!anOutDir.empty())
                {
                    if (!boost::ends_with(anOutDir, ta::getDirSep()))
                        myExtractDir = anOutDir + ta::getDirSep() + myExtractDir;
                    else
                        myExtractDir = anOutDir + myExtractDir;
                }
                boost::filesystem::remove_all(myExtractDir);
                boost::filesystem::create_directories(myExtractDir);

                int ret_value = do_extract(uf, myExtractDir);
                if  (ret_value != UNZ_OK)
                    TA_THROW_MSG(ZipExtractError, boost::format("Failed to extract %s to %s. do_extract returned %d") % anArchivePath % myExtractDir % ret_value);

                return myExtractDir;
            }
            catch (ZipExtractError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(ZipExtractError, e.what());
            }
        }


    }
}
