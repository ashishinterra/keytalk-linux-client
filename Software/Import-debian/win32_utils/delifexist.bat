@REM this script remove the file is it exists
@echo off
SET file2Del="%1%"
if exist %file2Del% del %file2Del%
