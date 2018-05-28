#pragma once

#include "ta/common.h"

#include "boost/foreach.hpp"
#include "boost/cstdint.hpp"
#include <string>
#include <vector>

namespace ta
{
    struct RetType;
    struct RetTailType;

    /**
      Read contents of a file to a memory buffer. Supported memory buffers: std::vector, std::basic_string

      @param[in] aFileName String of filename to read contents from
      @return Data read from file
      @throw std::runtime_error
     */
    RetType readData(const std::string& aFileName);


    /**
     Fetch data from the given http(s) URL
    */
    std::vector<unsigned char> fetchHttpUrl(const std::string& anUrl);

    /**
      Read at most last aMaxLines to a memory buffer. Supported memory buffers: std::vector, std::basic_string

      @param[in] aFileName String of filename to read contents from
      @param[in] aMaxLines desired number of lines to read
      @return Data read from file
      @throw std::runtime_error
     */
    RetTailType readTail(const std::string& aFileName, unsigned long aMaxLines);

    /**
      Write the contents of the memory buffer to the file specified. If the file already exists, its contents is destroyed.

      @param[in] aFileName filename to write to, intermediate directories are NOT created
      @param[in] aData Memory buffer as vector
      @throw std::runtime_error
     */
    template <class T> void writeData(const std::string& aFileName, const std::vector<T>& aData);

    /**
      Write the contents of the memory buffer to the file specified. If the file already exists, its contents is destroyed.

      @param[in] aFileName filename to write to, intermediate directories are NOT created
      @param[in] aData Memory buffer in string
      @throw std::runtime_error
     */
    template <class T> void writeData(const std::string& aFileName, const std::basic_string<T>& aData);


    /**
      Validate email address

      @param[in] aEmail email address to be checked
     */
    bool isValidEmail(const std::string& aEmail);

    /**
      Validate if file exist

      @param[in] aFileName path/filename to be checked
      @nothrow
     */
    bool isFileExist(const std::string& aFileName);

    /**
      Validate if directory exist

      @param[in] aDirName path to be checked
      @nothrow
     */
    bool isDirExist(const std::string& aDirName);

    // Creates parent directory for the given file if it does not yet exists
    // E.g. given the path is /foo/bar/baz.txt the function will (create /foo/bar directory provided it does not exist
    void createParentDir(const std::string& aFilePath);

    // Return parent directory for the given file (without trailing slash)
    std::string getParentDir(const std::string& aFilePath);

    /**
      Retrieve platform-dependent directory separator

      @return Platform-dependent directory separator
     */
    std::string getDirSep();

    /*
    	Recursively copy the contents of source directory to another directory
    **/
    void copyDir(const std::string& aSrcDir, const std::string& aDestDir);

    /**
      Escape all non-alphanumeric characters in the string to let the string to be used in regex

      @param[in] anStr String to be escaped
      @return Escaped string
     */
    std::string regexEscapeStr(const std::string& anStr);

    /**
        Return lower-case hex representation of 16-byte UUID
    */
    std::string genUuid();

    /**
        Return 16-byte UUID
    */
    std::vector<unsigned char> genRawUuid();

    /**
        Return random number in the interval [0, anUpperBound)
    */
    boost::uint32_t genRand(unsigned int anUpperBound);

    /**
        Return random data buffer
    */
    std::vector<unsigned char> genRandBuf(size_t aBufSize);

    /**
        Retrieve name of the real user
        @nothrow
    */
    std::string getUserName();

#if defined(__linux__)

    // Check if the real user is root
    //@nothrow
    bool isUserRoot();

    //@nothrow
    bool isRebootRequired();

    enum Recursive
    {
        recursiveYes, recursiveNo
    };
    // Set owner of the directory to the real user
    void chownDir(const std::string& aDir, Recursive aRecursive = recursiveYes);
#endif
}// namespace ta

#include "utilsimpl.hpp"
