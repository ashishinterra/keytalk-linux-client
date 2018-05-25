taskkill /F /IM iexplore.exe >NUL 2>&1
..\..\..\Import\win32_utils\sleep.exe 1
tasklist | find /c /i "iexplore.exe" > NUL
if %ERRORLEVEL% == 0 ( 
    ..\..\..\Import\win32_utils\sleep.exe 1
    tasklist | find /c /i "iexplore.exe" > NUL
    if %ERRORLEVEL% == 0  exit /b 1
)
exit /b 0

