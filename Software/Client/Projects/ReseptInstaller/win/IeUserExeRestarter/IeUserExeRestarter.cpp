#include "ta/InternetExplorer.h"
#include <windows.h>

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR,  int)
{
    try
    {
        ta::InternetExplorer::restartIeUser();
        return 0;
    }
    catch (...)
    {
        return 1;
    }
}

