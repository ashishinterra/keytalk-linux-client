#pragma once

#include "strings.h"
#include "common.h"
#include "boost/numeric/conversion/cast.hpp"
#include <fstream>
#include <stdexcept>

namespace ta
{
    struct RetType
    {
        RetType(const std::string& aFileName)
            : theFileName(aFileName)
        {}

        template<typename value_t>
        operator std::vector<value_t> () const
        {
            std::ifstream myFile(theFileName.c_str(), std::ios_base::binary | std::ios_base::in);
            if (!myFile.is_open() || myFile.fail())
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for reading") % theFileName);
            myFile.seekg(0, std::ios_base::end);
            std::streamsize myFileLen = myFile.tellg();
            if (myFileLen % sizeof(value_t))
                TA_THROW_MSG(std::runtime_error, boost::format("The size of %1% should be multiple to the container element size. File size: %2%, element size: %3%") % theFileName % myFileLen % (sizeof(value_t)));
            const size_t myNumOfElems = (size_t)(myFileLen / sizeof(value_t));

            std::vector<value_t> myData(myNumOfElems);
            if (myNumOfElems > 0)
            {
                myFile.seekg(0, std::ios_base::beg);
                myFile.read((char*)getSafeBuf(myData), myFileLen);
            }
            myFile.close();
            return myData;
        }

        template<typename value_t> operator std::basic_string<value_t> () const
        {
            std::vector<value_t> myDataVec = readData(theFileName);
            std::basic_string<value_t> myDataStr;
            if (!myDataVec.empty())
                myDataStr.assign(getSafeBuf(myDataVec), myDataVec.size());
            return myDataStr;
        }

        const std::string theFileName;
    };

    struct RetTailType
    {
        RetTailType(const std::string& aFileName, unsigned long aMaxLastLines)
            : theFileName(aFileName),  theMaxLastLines(aMaxLastLines)
        {}

        template<typename value_t>
        operator std::vector<value_t> () const
        {
            std::ifstream myFile(theFileName.c_str(), std::ios_base::binary | std::ios_base::in);
            if (!myFile.is_open() || myFile.fail())
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for reading") % theFileName);
            if (theMaxLastLines == 0)
                return std::vector<value_t>();
            myFile.seekg(0, std::ios_base::end);
            std::streamsize myFileLen = myFile.tellg();
            if (myFileLen == 0)
                return std::vector<value_t>();

            // File is not empty
            unsigned long myLines = 1;
            std::streamsize pos = myFileLen-1;
            while (myLines <= theMaxLastLines)
            {
                myFile.seekg(pos, std::ios_base::beg);
                if (myFile.peek() == '\n')
                {
                    if (myLines == theMaxLastLines && pos != myFileLen-1)
                    {
                        ++pos;
                        myFile.seekg(pos, std::ios_base::beg);
                        break;
                    }
                    if (pos != myFileLen-1)
                        ++myLines;
                }
                if (pos == 0)
                    break;
                --pos;
            }
            std::streamsize myNumBytes2Read = myFileLen - pos;
            if (myNumBytes2Read % sizeof(value_t))
                TA_THROW_MSG(std::runtime_error, boost::format("The size of tail of %1% should be multiple to the container element size. Tail size: %2%, element size: %3%") % theFileName % myNumBytes2Read % (sizeof(value_t)));
            const size_t myNumOfElems = (size_t)(myNumBytes2Read / sizeof(value_t));
            std::vector<value_t> myData(myNumOfElems);
            if (myNumOfElems > 0)
                myFile.read((char*)getSafeBuf(myData), myNumBytes2Read);
            myFile.close();
            return myData;
        }

        template<typename value_t> operator std::basic_string<value_t> () const
        {
            std::vector<value_t> myDataVec = readTail(theFileName, theMaxLastLines);
            std::basic_string<value_t> myDataStr;
            if (!myDataVec.empty())
                myDataStr.assign(getSafeBuf(myDataVec), myDataVec.size());
            return myDataStr;
        }

        const std::string theFileName;
        const unsigned long  theMaxLastLines;
    };

    inline RetType readData(const std::string& aFileName)
    {
        return RetType(aFileName);
    }

    inline RetTailType readTail(const std::string& aFileName, unsigned long aMaxLines)
    {
        return RetTailType(aFileName, aMaxLines);
    }

    template <class T>
    void writeData(const std::string& aFileName, const std::vector<T>& aData)
    {
        createParentDir(aFileName);
        std::ofstream myFile(aFileName.c_str(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        if (!myFile.is_open() || myFile.fail())
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for writing") % aFileName);
        if (!aData.empty())
            myFile.write((const char*)getSafeBuf(aData), static_cast<std::streamsize>(aData.size()*sizeof(T)));
        myFile.close();
    }

    template <class T>
    void writeData(const std::string& aFileName, const std::basic_string<T>& aData)
    {
        createParentDir(aFileName);
        std::ofstream myFile(aFileName.c_str(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        if (!myFile.is_open() || myFile.fail())
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for writing") % aFileName);
        if (!aData.empty())
            myFile.write((const char*)aData.c_str(), static_cast<std::streamsize>(aData.size()*sizeof(T)));
        myFile.close();
    }
}
