#ifndef ZipTest_H
#define ZipTest_H

#include "ta/Zip.h"
#include "ta/utils.h"

#include "cxxtest/TestSuite.h"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/assign/list_of.hpp"
#include <string>
#include <vector>

using std::vector;
using std::string;

class ZipTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        boost::filesystem::remove_all(NewArchiveFileName);
        foreach (const string& f, NewArchiveContents)
            ta::writeData(f, "Filename:"+f);
    }
    void tearDown()
    {
        foreach (const string& f, NewArchiveContents)
            boost::filesystem::remove_all(f);
        boost::filesystem::remove_all(NewArchiveFileName);
    }

    void testExtractExisting()
    {
        try
        {
            TS_ASSERT_THROWS(ta::Zip::extract("__nonexisting_file__",""), ta::ZipExtractError);

            TS_ASSERT_EQUALS(ta::Zip::extract("test.zip",""), "test");
            TS_ASSERT(ta::isDirExist("test"));
            boost::filesystem::remove_all("test");

            const string myOutExtractDir = "extracted"+ta::getDirSep()+"test";
            TS_ASSERT_EQUALS(ta::Zip::extract("test.zip","extracted"), myOutExtractDir);
            TS_ASSERT(ta::isDirExist(myOutExtractDir));
            TS_ASSERT(ta::isDirExist(myOutExtractDir+ta::getDirSep()+"dir1"));
            TS_ASSERT(ta::isDirExist(myOutExtractDir+ta::getDirSep()+"dir2"));
            TS_ASSERT(ta::isFileExist(myOutExtractDir+ta::getDirSep()+"file1"));
            TS_ASSERT(ta::isFileExist(myOutExtractDir+ta::getDirSep()+"file2.txt"));
            boost::filesystem::remove_all("extracted");
        }
        catch (ta::ZipExtractError & e)
        {
            TS_ASSERT(false);
            TS_TRACE(e.what());
        }
        catch (std::exception& e)
        {
            TS_ASSERT(!"Unexpected exception");
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }
    }

    void testArchiveExtract()
    {
        try
        {
            TS_ASSERT_THROWS(ta::Zip::archive("",vector<string>()), ta::ZipArchiveError);
            TS_ASSERT_THROWS(ta::Zip::archive(NewArchiveFileName, vector<string>()), ta::ZipArchiveError);
            TS_ASSERT_THROWS(ta::Zip::archive("", NewArchiveContents), ta::ZipArchiveError);

            ta::Zip::archive(NewArchiveFileName, NewArchiveContents);

            boost::filesystem::remove_all("extracted.new");
            const string myOutExtractDir = "extracted.new" + ta::getDirSep() + boost::replace_last_copy(NewArchiveFileName, ".zip", "");
            TS_ASSERT_EQUALS(ta::Zip::extract(NewArchiveFileName,"extracted.new"), myOutExtractDir);
            foreach (const string& f, NewArchiveContents)
            {
                TS_ASSERT(ta::isFileExist(myOutExtractDir+ta::getDirSep()+f));
                const string myContent = ta::readData(myOutExtractDir+ta::getDirSep()+f);
                TS_ASSERT_EQUALS(myContent, "Filename:"+f);
            }
        }
        catch (ta::ZipArchiveError & e)
        {
            TS_ASSERT(false);
            TS_TRACE(e.what());
        }
        catch (std::exception& e)
        {
            TS_ASSERT(!"Unexpected exception");
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }
    }

    //@todo add test for ta::Zip::archive when input files live in subdirectories and for various dir mapping functions

private:
    static const std::string ExistingArchiveFileName;
    static const std::string NewArchiveFileName;
    static const std::vector<std::string> NewArchiveContents;
};

const std::string ZipTest::NewArchiveFileName = "test.new.zip";
const std::vector<std::string> ZipTest::NewArchiveContents = boost::assign::list_of("file1.ziptest")("file2.ziptest");

#endif
