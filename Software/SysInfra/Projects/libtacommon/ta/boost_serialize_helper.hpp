#pragma once

#include "ta/utils.h"
#include "ta/common.h"

#include "boost/archive/text_oarchive.hpp"
#include "boost/archive/text_iarchive.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/serialization/map.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <typeinfo>

namespace ta
{
    template <class T>
    std::string boost_serialize(const T& aVal)
    {
        std::ostringstream ofs;
        boost::archive::text_oarchive oa(ofs);
        oa << aVal;
        return ofs.str();
    }

    template <class RetT>
    RetT boost_deserialize(const std::string& aVal, const bool aVerboseErrors = false)
    {
        try
        {
            std::istringstream ifs(aVal);
            boost::archive::text_iarchive ia(ifs);
            RetT myRetVal;
            ia >> myRetVal;
            return myRetVal;
        }
        catch (...)
        {
            if (aVerboseErrors)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Failed to deserialize the value of %s type from %s") % typeid(RetT).name() % aVal);
            }
            else
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Failed to deserialize the value of %s type") % typeid(RetT).name());
            }
        }
    }
}
