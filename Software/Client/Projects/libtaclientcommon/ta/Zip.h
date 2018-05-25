//----------------------------------------------------------------------------
//
//  Description : Utility to work with zip archives
//
//----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "boost/function.hpp"

namespace ta
{
    struct ZipArchiveError : std::runtime_error
    {
        explicit ZipArchiveError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };
    struct ZipExtractError : std::runtime_error
    {
        explicit ZipExtractError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    namespace Zip
    {
        std::string doNotChange(const std::string& aPath);
        std::string makeStem(const std::string& aPath);

        /**
        Archives the given list of files to the specified archive file overwriting any existing archive file if any.
        @param anOutArchivePath zip archive to create
        @param aFileList list of files
        @param anActual2ArchivePathMapper function that maps the real paths passed in aFileList to the path appear in the resulted archive
        @throw ZipArchiveError
        */
        typedef boost::function<std::string (const std::string&)> Actual2ArchivePathMapper;
        void archive(const std::string& anOutArchivePath,
                     const std::vector<std::string>& aFileList,
                     Actual2ArchivePathMapper anActual2ArchivePathMapper = doNotChange);

        /**
        Extracts the given zip archive to the supplied directory.
        @param anArchivePath zip archive to extract from
        @param anOutDir output directory. The contents of the archive is extracted to anOutDir/stem(anArchivePath)
        which is (re)created and cleaned if needed before extraction
        @return on success returns the directory with extracted archive contents
        @throw ZipExtractError
        */
        std::string extract(const std::string& anArchivePath,
                            const std::string& anOutDir);
    }
}
