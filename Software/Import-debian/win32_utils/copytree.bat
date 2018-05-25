@REM This script copies file specified in the first argument to the directory specified in the second argument, creating intermediate directories if required
@echo off
SET fileName="%1%"
SET dirName="%2%"
IF NOT EXIST "%dirName%" MD "%dirName%"
@echo Copying %fileName% to %dirName%\
XCOPY "%fileName%" "%dirName%\" /Y
