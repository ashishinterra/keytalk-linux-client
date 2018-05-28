#pragma once

#include "ta/thread.h"

#include "boost/noncopyable.hpp"
#include <string>
#include <map>
#include <stdexcept>
#include <time.h>

namespace ta
{
    struct DefaultSession
    {
        DefaultSession() : create_time(0)
        {}
        DefaultSession(const std::string& aData): create_time(time(NULL)), data(aData)
        {}
        inline bool isValid() const { return (create_time > 0) && (!data.empty()); }
        // container calls this method before destroying session
        inline void destroy() /*nothrow*/ {}
        inline bool operator==(const DefaultSession& rhs) const { return (create_time == rhs.create_time) && (data == rhs.data); }

        time_t create_time;
        std::string data;
    };

    /**
      Base exception type for SessionContainer errors
     */
    struct SessionContainerError : std::logic_error
    {
        explicit SessionContainerError(const std::string& aMessage = "") : std::logic_error(aMessage) {}
    };

    /**
      Raised when no SID with the valid session found
     */
    struct SidNotFoundError : SessionContainerError
    {
        explicit SidNotFoundError(const std::string& aMessage = "") : SessionContainerError(aMessage) {}
    };

    /**
      Raised when session is not valid
     */
    struct SessionInvalidError : SessionContainerError
    {
        explicit SessionInvalidError(const std::string& aMessage = "") : SessionContainerError(aMessage) {}
    };


    /**
      Raised when no more room for valid sessions left in the container
     */
    struct SessionContainerFullError : SessionContainerError
    {
        explicit SessionContainerFullError(const std::string& aMessage = "") : SessionContainerError(aMessage) {}
    };


    /**
      Session container. Thread-safe.
    */
    template <typename Session = DefaultSession>
    class SessionContainer: boost::noncopyable
    {
    public:
        typedef std::string SidType;

    public:
        /**
          C'tor

          @param[in] aMaxSize Max number of sessions
          @param[in] aSessionTtlSec Session time-to-live in seconds.
          After this TTL expires, the session is considered as invalid and becomes a subject for garbage collecting
         */
        SessionContainer(const size_t aMaxSize, const unsigned long aSessionTtlSec);
        ~SessionContainer();

        /**
          Fetches the specified valid (i.e. not expired) session from the container by removing from the container.

          @param[in] anSid Session ID
          @param[out] aSession Contains session data if the valid session was found, otherwise it stays unchanged.
          @return Whether the valid session has been found for the given SID.
         */
        bool fetch(const SidType& anSid, Session& aSession);

        /**
          Add the specified session to the container which takes ownership on the session data.
          This function also triggers garbage collector to clean up expired sessions in the container.

          @param[in] aSession session
          @return Session ID which uniquely identifies this session
         */
        SidType add(const Session& aSession);

        /**
          Return the number of valid (i.e. not expired) sessions in the container
          @return Number of valid sessions
         */
        size_t size() const;

    private:
        void cleanUp();
        bool isExpired(const time_t aCreateTime, const time_t aNow) const;
        SidType generateUniqueSid() const;

    private:
        size_t theMaxSize;
        unsigned long theSessionTtlSec;
        typedef std::map<SidType, Session> ContainerType;
        ContainerType theContainer;
        mutable ta::Mutex theContainerMutex;
    };
}

#include "sessioncontainerimpl.hpp"
