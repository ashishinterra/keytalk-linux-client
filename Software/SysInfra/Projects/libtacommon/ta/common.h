#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include "boost/format.hpp"
#include "boost/foreach.hpp"
#include "boost/bind.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/range/algorithm.hpp"

namespace boost {
    namespace BOOST_FOREACH = foreach;
}
#undef foreach
#define foreach BOOST_FOREACH

#if defined(__GNUC__)
# define TA_PRETTY_FUNC __PRETTY_FUNCTION__
# define TA_BARE_FUNC __FUNCTION__
#elif defined(__FUNCTION__)
# define TA_PRETTY_FUNC __FUNCTION__
# define TA_BARE_FUNC __FUNCTION__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
# define TA_PRETTY_FUNC __func__
# define TA_BARE_FUNC __func__
#else
# define TA_PRETTY_FUNC "(unknown)"
# define TA_BARE_FUNC "(unknown)"
#endif

#define UNUSED(x)

namespace ta
{
    // Handy shortcuts
    typedef std::vector<std::string> StringArray;
    typedef std::set<std::string> StringSet;
    typedef std::map<std::string, std::string> StringDict;
    typedef std::pair<std::string, std::string> StringPair;
    typedef std::map<std::string, StringArray> StringArrayDict;
    typedef std::vector<StringDict> StringDictArray;

    /**
       Safe (nothrow) string formatting wrapper around boost::format.
       Because this class masks all formatting errors, use it only when non-properly formatted string is less evil than thrown exception.
    */
    class safe_format
    {
    public:
        // @nothrow
        safe_format(const char* s)
        {
            try {
                boost::format temp_fmt(s); // it seems boost::format does not support any sort of exception guarantees, so we have to construct it via temporaries
                fmt = temp_fmt;
            } catch (...) {
            }
        }
        // @nothrow
        safe_format(const std::string& s)
        {
            try {
                boost::format temp_fmt(s); // it seems boost::format does not support any sort of exception guarantees, so we have to construct it via temporaries
                fmt = temp_fmt;
            } catch (...) {
            }
        }

        /**
          When formatting error occurs, an empty string is returned.
        */
        inline std::string str() const
        {
            try  {
                return fmt.str();
            }   catch (...) {
                return "";
            }
        }

        /**
          When formatting error occurs, the function has no effect
        */
        template<class T>
        safe_format& operator%(const T& x)
        {
            try {
                fmt  = fmt % x;
            }   catch (...) {}
            return *this;
        }
    private:
        boost::format fmt;
    };

    inline std::string str(const safe_format& sfmt)
    {
        return sfmt.str();
    }

    /**
      Creates string message with empty content if content is undefined

      @param[in] aMsg String message to be validated
      @return String message with empty content if content is undefined
     */
    inline std::string safeFmtMsg(const char* aMsg) { return aMsg?aMsg:""; }

    /**
      Returns string message

      @param[in] aMsg String message
      @return String message
     */
    inline std::string safeFmtMsg(const std::string& aMsg) { return aMsg; }

    /**
      Returns string message from boost format

      @param[in] aFmt Boost format
      @return String message from boost format
     */
    inline std::string safeFmtMsg(const boost::format& aFmt)
    {
        return aFmt.str();
    }

    /**
      Returns string message from safe_format

      @param[in] aFmt the instance of safe_format
      @return String message from safe_format
     */
    inline std::string safeFmtMsg(const safe_format& aFmt)
    {
        return aFmt.str();
    }
}

// Helper macros for throwing exceptions. The macros automatically add the throwing context (exception name function name, file, line) to the exception message
#define TA_THROW(ex) throw ex( str(boost::format("%1% thrown from %2%(), file %3%:%4%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__).c_str() )
#define TA_THROW_MSG(ex, fmt) throw ex( str(boost::format("%1% thrown from %2%(), file %3%:%4%. %5%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__ % ta::safeFmtMsg(fmt)).c_str() )
#define TA_THROW_MSG2(ex, userfmt, devfmt) throw ex( ta::safeFmtMsg(userfmt).c_str(), str(boost::format("%1% thrown from %2%(), file %3%:%4%. %5%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__ % ta::safeFmtMsg(devfmt)).c_str() )
#define TA_THROW_ARG(ex, num) throw ex( num, str(boost::format("%1% thrown from %2%(), file %3%:%4%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__).c_str() )
#define TA_THROW_ARG_MSG(ex, arg, fmt) throw ex( arg, str(boost::format("%1% thrown from %2%(), file %3%:%4%. %5%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__ % ta::safeFmtMsg(fmt)).c_str() )
#define TA_THROW_ARG_STR(ex, num, s) throw ex( num, s, str(boost::format("%1% thrown from %2%(), file %3%:%4%\n") % #ex % TA_BARE_FUNC % __FILE__ % __LINE__ ).c_str() )


template<class T> std::vector<T>& operator+= (std::vector<T>& aVec, const std::vector<T>& anAdd)
{
    aVec.insert(aVec.end(), anAdd.begin(), anAdd.end());
    return aVec;
}

template<class T> std::vector<T>& operator+= (std::vector<T>& aVec, const T& anElem)
{
    aVec.push_back(anElem);
    return aVec;
}

template<class T> std::vector<T> operator+ (const std::vector<T>& aVec1, const std::vector<T>& aVec2)
{
    std::vector<T> myRetVal = aVec1;
    myRetVal += aVec2;
    return myRetVal;
}

template<class T> std::vector<T> operator+ (const std::vector<T>& aVec, const T& anElem)
{
    std::vector<T> myRetVal = aVec;
    myRetVal += anElem;
    return myRetVal;
}


namespace ta
{
    /**
      Mixin for cloning objects
     */
    template<typename T, typename BaseT>
    struct Clonable: public BaseT
    {
        virtual ~Clonable() {};
        virtual BaseT* clone() const
        {
            return new T(static_cast<const T&>(*this));
        }
    };

    inline size_t bytes2Bits(size_t aBytes)
    {
        return 8 * aBytes;
    }

    //@retrieves a pointer to the internal vector buffer or NULL when the given buffer offset is not valid (e.g. the vector is empty)
    template <class T>
    T* getSafeBuf(std::vector<T>& aVector, size_t anOffset = 0)
    {
        return (anOffset < aVector.size()) ? &aVector[anOffset] : NULL;
    }
    template <class T>
    const T* getSafeBuf(const std::vector<T>& aVector, size_t anOffset = 0)
    {
        return (anOffset < aVector.size()) ? &aVector[anOffset] : NULL;
    }

    /**
       Compare two sequences ignoring their order
    */
    template <typename Seq1Type, typename Seq2Type>
    bool equalIgnoreOrder(const Seq1Type& aSeq1, const Seq2Type& aSeq2)
    {
        if (aSeq1 == aSeq2)
            return true;
        std::list<typename Seq1Type::value_type> mySeq1List(aSeq1.begin(), aSeq1.end());
        std::list<typename Seq2Type::value_type> mySeq2List(aSeq2.begin(), aSeq2.end());
        mySeq1List.sort();
        mySeq2List.sort();
        return mySeq1List == mySeq2List;
    }

    // Check the element exists in a sequence
    template <class T>
    bool isElemExist(const typename T::value_type& anElem, const T& aSequence)
    {
        return std::find(aSequence.begin(), aSequence.end(), anElem) != aSequence.end();
    }

    // Check the element exists in a static POD array i.e. the array defined as:
    // static const string array[] = {"one", "two", three"};
    template <class elem_t, size_t size>
    bool isPodArrayElemExist(const elem_t& anElem, const elem_t (&aPodArray)[size])
    {
        return std::find(&aPodArray[0], &aPodArray[0] + size, anElem) != &aPodArray[0] + size;
    }

    template <typename Pred, class T>
    bool isElemExistWhen(Pred aPred, const T& aSequence)
    {
        return boost::find_if(aSequence, boost::bind(aPred, _1)) != aSequence.end();
    }
    template <typename Pred, class T>
    bool isElemExistWhenNot(Pred aPred, const T& aSequence)
    {
        return boost::find_if(aSequence, !boost::bind(aPred, _1)) != aSequence.end();
    }

    template <class T>
    void addElemIfNotExist(const typename T::value_type& anElem, T& aSequence)
    {
        if (!isElemExist(anElem, aSequence))
        {
            aSequence.push_back(anElem);
        }
    }

    template <class T>
    bool isKeyExist(const typename T::key_type& aKey, const T& aDict)
    {
        return aDict.find(aKey) != aDict.end();
    }

    template <class T>
    bool findValueByKey(const typename T::key_type& aKey, const T& aDict, typename T::mapped_type& aValue)
    {
        typename T::const_iterator it = aDict.find(aKey);
        if (it != aDict.end())
        {
            aValue = it->second;
            return true;
        }
        else
        {
            return false;
        }
    }

    template <class T>
    typename T::mapped_type getValueByKey(const typename T::key_type& aKey, const T& aDict)
    {
        typename T::const_iterator it = aDict.find(aKey);
        if (it != aDict.end())
        {
            return it->second;
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, boost::format("Key %1% does not exist") % aKey);
        }
    }

    template <class T>
    typename T::mapped_type getValueByKeyWithDefault(const typename T::key_type& aKey, const T& aDict, const typename T::mapped_type& aDefaultValue)
    {
        typename T::const_iterator it = aDict.find(aKey);
        if (it != aDict.end())
        {
            return it->second;
        }
        else
        {
            return aDefaultValue;
        }
    }

    // return sequence having all elements satisfying the predicate preserving elements order
    template <typename Pred, class T>
    T filterWhen(Pred aPred, const T& aSequence)
    {
        T myRetVal(aSequence);
        myRetVal.erase(std::remove_if( myRetVal.begin(),
                                       myRetVal.end(),
                                       !boost::bind(aPred, _1)),
                       myRetVal.end());
        return myRetVal;
    }

    // return sequence without elements satisfying the predicate preserving elements order
    template <typename Pred, class T>
    T filterOutWhen(Pred aPred, const T& aSequence)
    {
        T myRetVal(aSequence);
        myRetVal.erase(std::remove_if( myRetVal.begin(),
                                       myRetVal.end(),
                                       boost::bind(aPred, _1)),
                       myRetVal.end());
        return myRetVal;
    }

    // return sequence without elements equal to the given value preserving elements order
    template <class T>
    T filterOut(const typename T::value_type& aVal, const T& aSequence)
    {
        T myRetVal(aSequence);
        myRetVal.erase(std::remove( myRetVal.begin(),
                                    myRetVal.end(),
                                    aVal),
                       myRetVal.end());
        return myRetVal;
    }

    // return a subsequence of aSequence1 each element of which also exists in aSequence2, preserving elements order
    template <class T>
    T intersect(const T& aSequence1, const T& aSequence2)
    {
        return filterWhen(boost::bind(isElemExist<T>, _1, aSequence2), aSequence1);
    }
    template <class T>
    std::set<T> intersectSets(const std::set<T>& aSet1, const std::set<T>& aSet2)
    {
        std::set<T> myRetVal;
        foreach (const typename std::set<T>::value_type e, aSet1)
        {
            if (isElemExist(e, aSet2))
            {
                myRetVal.insert(e);
            }
        }
        return myRetVal;
    }

    // return a subsequence of aSequence1 each element of which does not exist in aSequence2, preserving elements order
    template <class T>
    T subtract(const T& aSequence1, const T& aSequence2)
    {
        return filterOutWhen(boost::bind(isElemExist<T>, _1, aSequence2), aSequence1);
    }
    template <class T>
    std::set<T> subtractSets(const std::set<T>& aSet1, const std::set<T>& aSet2)
    {
        std::set<T> myRetVal;
        foreach (const typename std::set<T>::value_type e, aSet1)
        {
            if (!isElemExist(e, aSet2))
            {
                myRetVal.insert(e);
            }
        }
        return myRetVal;
    }


    // return the first element in the sequence satisfying the predicate or raise exception if not found
    template <typename Pred, class T>
    typename T::value_type getFirstElem(Pred aPred, const T& aSequence, const std::string& anElementNameHint = "element")
    {
        const typename T::const_iterator it = boost::find_if(aSequence, boost::bind(aPred, _1));
        if (it == aSequence.end())
            TA_THROW_MSG(std::logic_error, "No " + anElementNameHint + " found");
        return *it;
    }

    // retrieve the first element in the sequence satisfying the predicate and return true or return false if not found
    template <typename Pred, class T>
    bool findFirstElem(Pred aPred, const T& aSequence, typename T::value_type& aVal)
    {
        const typename T::const_iterator it = boost::find_if(aSequence, boost::bind(aPred, _1));
        if (it == aSequence.end())
        {
            return false;
        }
        aVal = *it;
        return true;
    }

    // return the element in the sequence satisfying the predicate or raise exception if no or multiple elements found for the given criteria
    template <typename Pred, class T>
    typename T::value_type getUniqueElem(Pred aPred, const T& aSequence, const std::string& anElementNameHint = "element")
    {
        T myResult(aSequence);
        myResult.erase(std::remove_if(myResult.begin(), myResult.end(), !boost::bind(aPred, _1)), myResult.end());
        if (myResult.empty())
            TA_THROW_MSG(std::logic_error, "No " + anElementNameHint + " found");
        if (myResult.size() > 1)
            TA_THROW_MSG(std::logic_error, "More than one " + anElementNameHint + " found");
        return myResult.front();
    }

    template <typename Pred, class T>
    void verifyUniqueElem(Pred aPred, const T& aSequence, const std::string& anElementNameHint = "element")
    {
        getUniqueElem(aPred, aSequence, anElementNameHint);
    }

    template <class T>
    std::string vec2Str(const std::vector<T>& aVec)
    {
        return std::string(aVec.begin(), aVec.end());
    }

    template <class T>
    std::vector<T> str2Vec(const std::string& aStr)
    {
        return std::vector<T>(aStr.begin(), aStr.end());
    }

    template <class T>
    std::vector<T> set2Vec(const std::set<T>& aSet)
    {
        return std::vector<T>(aSet.begin(), aSet.end());
    }

    template <class T, class U>
    std::vector<T> vec2Vec(const std::vector<U>& aVec)
    {
        return std::vector<T>(aVec.begin(), aVec.end());
    }
    template <class T>
    std::set<T> vec2Set(const std::vector<T>& aVec)
    {
        return std::set<T>(aVec.begin(), aVec.end());
    }

    template <class T, class U>
    std::set<T> extractKeys(const std::map<T,U>& aDict)
    {
        std::set<T> mySet;
        for (typename std::map<T, U>::const_iterator it = aDict.begin(); it != aDict.end(); ++it)
        {
            mySet.insert(it->first);
        }
        return mySet;
    }

    // C-style POD array of strings {"one", "two", "three", NULL} => C++-style ["one", "two", "three"]
    inline StringArray podArray2StringArray(const char * const * anArray)
    {
        StringArray myRetVal;
        for (const char* const * myEnvp = anArray; *myEnvp; ++myEnvp)
        {
            myRetVal.push_back(*myEnvp);
        }
        return myRetVal;
    }

    template <class T>
    bool hasDuplicates(const std::vector<T>& aVec)
    {
        typename std::vector<T>::const_iterator i, j;
        typename std::vector<T>::const_iterator end = aVec.end();
        for (i = aVec.begin(); i != end; ++i)
        {
            for (j = i+1; j != end; ++j)
            {
                if (*i == *j)
                {
                    return true;
                }
            }
        }
        return false;
    }

    // remove duplicates (does not guarantee to preserve order)
    template <class T>
    std::vector<T> removeDuplicates(const std::vector<T>& aVec)
    {
        const typename std::set<T> mySet( aVec.begin(), aVec.end() );
        const typename std::vector<T> myResult(mySet.begin(), mySet.end() );
        return myResult;
    }


    struct KeyPair
    {
        KeyPair() {}
        KeyPair(const std::vector<unsigned char>& aPrivKey, const std::vector<unsigned char>& aPubKey) : privKey(aPrivKey), pubKey(aPubKey) {}
        std::vector<unsigned char> privKey; // private key content of id
        std::vector<unsigned char> pubKey;  // public key content or id
    };

} // namespace ta
