#include "resept/common.h"
#include "rclient/Common.h"
#include "ta/process.h"
#include "ta/version.h"
#include "ta/common.h"
#include <windows.h>
#include <cstdio>
#include <tchar.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "boost/tokenizer.hpp"
#include "boost/algorithm/string.hpp"

using std::string;
using std::vector;

static const char ArgSep[]     =  "\"";
static const string InstallerWindowTitle = str(boost::format("%s %s Setup") % resept::ProductName % ta::version::toStr(rclient::ClientVersion, ta::version::fmtMajorMinor));

// Display a message box with a given text/title if at least one instance of the given program is running in the system
//
// Usage: MessageBoxManager <ProgName> <Title> <Text>
//
// Return values:
// 0 if succeeded, <>0 otherwise



vector<string> parseArgs(const string& aCmdLine)
{
    vector<string> myRetVal;
    typedef boost::tokenizer<boost::char_separator<char> > Tokenizer;
    boost::char_separator<char> mySep(ArgSep);
    Tokenizer myTokens(aCmdLine, mySep);

    for (Tokenizer::const_iterator it = myTokens.begin(), end = myTokens.end(); it != end; ++it)
    {
        if (boost::trim_copy(*it).empty())
            continue;
        myRetVal.push_back(*it);
    }
    return myRetVal;
}

int APIENTRY WinMain(HINSTANCE UNUSED(hInstance), HINSTANCE UNUSED(hPrevInstance), LPSTR  lpCmdLine,  int  UNUSED(nCmdShow))
{
    if (!lpCmdLine || !(*lpCmdLine))
        return -1;
    vector<string>  myArgs = parseArgs(lpCmdLine);
    if (myArgs.size() != 3)
        return -1;
    string myProgName = myArgs[0];
    if (myProgName.empty())
        return -1;
    string myTitle    = myArgs[1];
    string myText     = myArgs[2];
    try
    {
        if (ta::Process::isRunning(myProgName))
        {
            ::MessageBox(::FindWindow(NULL, InstallerWindowTitle.c_str()),
                         myText.c_str(),
                         myTitle.c_str(),
                         MB_ICONINFORMATION);
        }
        return 0;
    }
    catch (std::exception&)
    {}
    return 1;
}

