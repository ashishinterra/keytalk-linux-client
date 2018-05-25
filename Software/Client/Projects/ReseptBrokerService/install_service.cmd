@echo off
@echo "Installing ReseptBrokerService..."
sc create ReseptBrokerService binPath= "%CD%\..\export\ReseptBrokerService.exe" type= "own" start= "auto" error= "normal" tag= "no" obj= "LocalSystem" DisplayName= "KeyTalk Broker Service"
sc start ReseptBrokerService
