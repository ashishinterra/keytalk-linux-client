#pragma once

#include <string>
#include "ta/common.h"

namespace PrGenerator
{
    // Prepares KeyTalk files ready to be archived in the specified directory.
    // The function tolerates all errors related to file non-accessibility
    // @return list of full paths to the prepared files
    ta::StringArray preparePrFiles(const std::string& aDir);

    void generate(const std::string& aPrFilePath);

    //@nothrow
    std::string getSavePath();

    //@nothrow
    void safeRemoveDir(const std::string& aDir);
}
