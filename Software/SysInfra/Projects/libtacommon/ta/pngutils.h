#pragma once

#include <string>
#include <vector>
#include "boost/cstdint.hpp"

namespace ta
{
    namespace PngUtils
    {
        struct PngInfo
        {
            PngInfo()
                : width(0), height(0)
            {}
            PngInfo(boost::uint32_t aWidth, boost::uint32_t aHeight)
                : width(aWidth), height(aHeight)
            {}
            inline bool operator==(const PngInfo& rhs) const
            {
                return (width == rhs.width && height == rhs.height);
            }
            boost::uint32_t width;
            boost::uint32_t height;
        };

        PngInfo getPngInfo(const std::string& aPngFilePath);
        PngInfo getPngInfo(const std::vector<unsigned char>& aPngMemBuf);
    }
}
