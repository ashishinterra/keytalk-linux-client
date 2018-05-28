#pragma once

#include "ta/common.h"

#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/type_traits/is_unsigned.hpp"
#include <string>
#include <stdexcept>

namespace ta
{
    namespace Strings
    {
        template <bool is_unsigned>
        struct unsigned_checker
        {
            static inline void do_check(const std::string& UNUSED(str)) { }
        };

        template <>
        struct unsigned_checker<true>
        {
            static inline void do_check(const std::string& str)
            {
                const std::string myTrimmedStr = boost::trim_copy(str);
                if (myTrimmedStr.empty())
                {
                    throw std::invalid_argument("Cannot interpret empty string as unsigned integer");
                }
                if (myTrimmedStr[0] == '-')
                {
                    throw std::invalid_argument("Cannot interpret value as unsigned integer");
                }
            }
        };

        template<typename Target>
        inline Target parse(const std::string& aVal)
        {
            // fix misleading behavior of lexical_cast which successfully casts negative number to unsigned int i.e. boost::lexical_cast<unsigned int>("-1") gives 4294967295 i.o. throwing excepion
            // see https://svn.boost.org/trac/boost/ticket/5494 for more info
            unsigned_checker< boost::is_unsigned<Target>::value >::do_check(aVal);

            const std::string myTrimmedVal = boost::trim_copy(aVal);
            try
            {
                return boost::lexical_cast<Target>(myTrimmedVal);
            }
            catch (...)
            {
                throw std::invalid_argument("Cannot interpret value as a target type");
            }
        }

    }// namespace Strings
} // namespace ta
