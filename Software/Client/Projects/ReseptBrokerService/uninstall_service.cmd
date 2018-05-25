@echo off
@echo "Uninstalling ReseptBrokerService..."
sc stop ReseptBrokerService
sc delete ReseptBrokerService
