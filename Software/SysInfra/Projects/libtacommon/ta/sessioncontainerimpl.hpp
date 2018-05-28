#pragma once

#include "ta/strings.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/assert.h"
#include "ta/common.h"

using std::string;
using std::vector;
using namespace ta;

namespace ta
{
    template <typename Session>
    SessionContainer<Session>::SessionContainer(const size_t aMaxSize, const unsigned long aSessionTtlSec)
        : theMaxSize(aMaxSize), theSessionTtlSec(aSessionTtlSec)
    {}

    template <typename Session>
    SessionContainer<Session>::~SessionContainer()
    {
        ScopedLock lock(theContainerMutex);
        foreach (typename ContainerType::value_type& session, theContainer)
        {
            session.second.destroy();
        }
    }

    template <typename Session>
    bool SessionContainer<Session>::fetch(const SidType& anSid, Session& aSession)
    {
        ScopedLock lock(theContainerMutex);

        typename ContainerType::const_iterator myFoundIt = theContainer.find(anSid);
        if (myFoundIt == theContainer.end())
        {
            return false;
        }

        if (isExpired(myFoundIt->second.create_time, time(NULL)))
        {
            return false;
        }

        if (!myFoundIt->second.isValid())
        {
            TA_THROW_MSG(SessionInvalidError, boost::format("Cannot fetch session data with ID '%s' because it is invalid") % anSid);
        }

        aSession = myFoundIt->second;

        theContainer.erase(anSid);

        return true;
    }

    template <typename Session>
    typename SessionContainer<Session>::SidType SessionContainer<Session>::add(const Session& aSession)
    {
        ScopedLock lock(theContainerMutex);
        TA_ASSERT(theContainer.size() <= theMaxSize);

        if (theContainer.size() == theMaxSize)
        {
            cleanUp();
            if (theContainer.size() == theMaxSize)
            {
                TA_THROW_MSG(SessionContainerFullError, boost::format("Max size is %1%") % theContainer.size());
            }
            TA_ASSERT(theContainer.size() < theMaxSize);
        }

        if (!aSession.isValid())
        {
            TA_THROW_MSG(SessionInvalidError, "Cannot add session because it contains invalid data");
        }

        const SidType myNewSid = generateUniqueSid();
        TA_ASSERT(theContainer.find(myNewSid) == theContainer.end());
        theContainer[myNewSid] = aSession;

        return myNewSid;
    }

    template <typename Session>
    size_t SessionContainer<Session>::size() const
    {
        const time_t myNow = time(NULL);
        size_t myNumOfValid = 0;

        ScopedLock lock(theContainerMutex);
        foreach (const typename ContainerType::value_type& session, theContainer)
        {
            if (!isExpired(session.second.create_time, myNow))
            {
                ++myNumOfValid;
            }
        }
        return myNumOfValid;
    }


    //
    // Private stuff
    //

    /**
    * Cleans up expired sessions
    */
    template <typename Session>
    void SessionContainer<Session>::cleanUp()
    {
        const time_t myNow = time(NULL);
        for (typename ContainerType::iterator it = theContainer.begin(), end = theContainer.end(); it != end; )
        {
            if (isExpired(it->second.create_time, myNow))
            {
                it->second.destroy();
                theContainer.erase(it++);
            }
            else
            {
                ++it;
            }
        }
    }

    template <typename Session>
    bool SessionContainer<Session>::isExpired(const time_t aCreateTime, const time_t aNow) const
    {
        return (aNow > static_cast<time_t>(aCreateTime+theSessionTtlSec));
    }

    /**
    * Generates SID unique for the session container.
    */
    template <typename Session>
    typename SessionContainer<Session>::SidType SessionContainer<Session>::generateUniqueSid() const
    {
        while (true)
        {
            SidType mySid = ta::genUuid();
            if (theContainer.find(mySid) == theContainer.end())
            {
                return mySid;
            }
        }
    }

} // ta
