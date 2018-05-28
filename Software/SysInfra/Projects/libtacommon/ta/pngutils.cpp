//----------------------------------------------------------------------------
//
//  Description : PNG images utilites implementation
//
//----------------------------------------------------------------------------
#include "pngutils.h"
#include "utils.h"
#include "common.h"
#include "boost/algorithm/string.hpp"

// include for ntohl
#ifdef _WIN32
# include <Winsock2.h>
#else
# include <arpa/inet.h>
#endif

namespace ta
{
    namespace PngUtils
    {
        //
        // Private API
        //
        namespace
        {
            //@note aFilePathHint, when non-empty is only used for improve error reporting
            PngInfo getPngInfoImpl(const std::vector<unsigned char>& aPngMemBuf, const std::string& aFilePathHint = "")
            {
                static const std::string PngHeader = "\x89PNG\r\n\x1A\n";
                using boost::uint32_t;

                struct IHDR
                {
                    uint32_t header_size;
                    char header_name[4];
                    uint32_t width;
                    uint32_t height;
                    // and more.. but we don't care
                };

                if ( (aPngMemBuf.size() < PngHeader.size() + sizeof(IHDR)/*ignore alignment*/) ||
                        !boost::starts_with(vec2Str(aPngMemBuf), PngHeader))
                {
                    if (!aFilePathHint.empty())
                        TA_THROW_MSG(std::runtime_error, boost::format("File %s is not a valid PNG file.") % aFilePathHint);
                    TA_THROW_MSG(std::runtime_error, "Not a valid PNG buffer");
                }

                // First cast to void* in order to silence the alignment warnings.
                const IHDR* myIhdr = (const IHDR*)(const void*)(&aPngMemBuf[0] + PngHeader.size());
                const std::string myIhdrName(myIhdr->header_name, sizeof(myIhdr->header_name));
                if (myIhdrName != "IHDR")
                {
                    if (!aFilePathHint.empty())
                        TA_THROW_MSG(std::runtime_error, boost::format("File %s is not a valid PNG file (no IHDR header found)") % aFilePathHint);
                    TA_THROW_MSG(std::runtime_error, "Not a valid PNG buffer (no IHDR header found)");
                }

                PngInfo myRetVal(ntohl(myIhdr->width), ntohl(myIhdr->height));
                return myRetVal;
            }
        }

        //
        // Public API
        //
        PngInfo getPngInfo(const std::string& aPngFilePath)
        {
            const std::vector<unsigned char> myPngMemBuf = ta::readData(aPngFilePath);
            return getPngInfoImpl(myPngMemBuf, aPngFilePath);
        }

        PngInfo getPngInfo(const std::vector<unsigned char>& aPngMemBuf)
        {
            return getPngInfoImpl(aPngMemBuf);
        }


    }
}


