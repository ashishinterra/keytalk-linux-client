#pragma once

#include "ta/common.h"
#include <string>

#ifdef _WIN32
# ifdef TA_LOGGER_EXPORTS
#  define TA_ASSERT_API __declspec(dllexport)
# else
#  define TA_ASSERT_API __declspec(dllimport)
# endif
#else
#  define TA_ASSERT_API
#endif

namespace ta
{
    /**
      Function implementation for TA_ASSERT

      @param[in] anExpr Expression to be displayed in message
      @param[in] aFunc Function name to be displayed in message
      @param[in] aFile Filename to be displayed in message
      @param[in] aLine Line number to be displayed in message
     */
    TA_ASSERT_API void assertion_failed(const std::string& anExpr, const std::string& aFunc, const std::string& aFile, unsigned int aLine);
}

/**
  Assert macro.
  The macro has the following useful features compared to the standard C++ assert macro:
  It has nearly the same behavior in different configurations (aka debug and release):
  - the expression is evaluated always (all configurations).
  - error location is written to the log (all configurations).
  - an error message is presented (all configurations, however in release builds, as intended for end users, the error message might be less detailed).
  - user-oriented assert information is sent to the stderr for console applications and to the window for UI applications
  - developer-oriented assert information is put into the log.
  - a possibility to stop on the assert statement if the program is being debugged (Win32 only).
  - zero overhead when the expression evaluates to true (same as in standard assert)

  @param[in] expr expression to evaluate
 */
#define TA_ASSERT(expr) ((expr)? ((void)0): ::ta::assertion_failed(#expr, TA_BARE_FUNC, __FILE__, __LINE__))
