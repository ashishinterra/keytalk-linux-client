//----------------------------------------------------------------------------
//
//  Description : FileVersionInfo class declaration  providing version onfo about file on Windows
//
//----------------------------------------------------------------------------
#ifndef TA_FILEVERSIONINFO_H
#define TA_FILEVERSIONINFO_H

#ifndef _WIN32
# error "Only Win32 platform is supported"
#endif

#include <string>

namespace ta
{
    class FileVersionInfo
    {
    public:
        // throw std::exception on error
        explicit FileVersionInfo(const std::string& aPath);

        std::string getFileVersion() const;
        std::string getProductVersion() const;
        std::string getCompanyName() const;
        std::string getFileDescription() const;
        std::string getProductName() const;

    private:
        void init();
    private:
        std::string	 thePath;
        std::string	 theFileVersion;
        std::string	 theProductVersion;
        std::string	 theCompanyName;
        std::string	 theProductName;
        std::string	 theFileDescription;
    };

}

#endif
