@echo off
rem Endurance authentication test against KeyTalk server

:Start
set now=%date% %time%
echo %now%
ReseptConsoleClient.exe --provider DemoProvider --service CUST_ANO_INTERNAL --user DemoUser
echo %ERRORLEVEL%
GOTO Start
