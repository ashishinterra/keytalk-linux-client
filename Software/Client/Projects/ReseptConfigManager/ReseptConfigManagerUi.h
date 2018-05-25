#pragma once

#include "ui_IISCertificateUpdateTaskSettings.h"
#include "ui_TaskScheduleCredentialDialog.h"

#include "ta/opensslapp.h"
#include "ta/netutils.h"
#include "rclient/Settings.h"

#include <QtWidgets>
#include <boost/function.hpp>
#include <string>
#include <vector>
#include <memory>

class ReseptConfigManagerUi;

class GeneralTab : public QWidget
{
    Q_OBJECT
public:
    GeneralTab(ReseptConfigManagerUi* parent);
private slots:
    void onLoadClicked();
    void onRemoveClicked();
    void onProviderSelectionChanged();
private:
    void load();
    void reLoadAllTabs();
private:
    QTreeWidget* theInstalledSettingsTable;
    QPushButton* theLoadBtn;
    QPushButton* theRemoveBtn;
    ReseptConfigManagerUi* theParent;
};

class ProviderSettingsTab : public QWidget
{
    Q_OBJECT
public:
    ProviderSettingsTab(QWidget* parent = 0);
    void load();
    void save();
protected:
    void updateUiFromProviderSettings(const std::string& aSelectedProvider);
    bool updateProviderSettingsFromUi(const std::string& aSelectedProvider);
private slots:
    void onProviderSelected(const QString& aText);
private:
    struct ProviderSettings
    {
        ProviderSettings(const std::string& aName, const ta::NetUtils::RemoteAddress& aSvrAddr, bool aSvrAddrRonly, const std::string& aLogLevel, bool aLogLevelRonly)
            : name(aName), svr_addr(aSvrAddr), svr_addr_ronly(aSvrAddrRonly), log_level(aLogLevel), log_level_ronly(aLogLevelRonly) {}
        std::string name;
        ta::NetUtils::RemoteAddress svr_addr;
        bool svr_addr_ronly;
        std::string log_level;
        bool log_level_ronly;
    };
    ProviderSettings getProviderSettings(const std::string& aProviderName) const;
    void setProviderSettings(const std::string& aProviderName, const ProviderSettings& aProviderSettings);
    std::vector<ProviderSettings> theProvidersSettings;
    QComboBox* theProvidersCombo;
    QComboBox* theLogLevelsCombo;
    QLineEdit* theServerEdit;
    std::string theSelectedProvider;
};

class ServiceSettingsTab : public QWidget
{
    Q_OBJECT
public:
    ServiceSettingsTab(QWidget* parent = 0);
    void load();
    void save();
protected:
    void updateUiFromServiceSettings(const std::string& aSelectedProvider, const std::string& aSelectedService);
private slots:
    void onProviderSelected(const QString& aText);
    void onServiceSelected(const QString& aText);
private:
    struct ServiceSettings
    {
        ServiceSettings(const std::string& aName, const std::string& aServiceUri, const unsigned int aCertValidPercentage)
            : name(aName), service_uri(aServiceUri), cert_valid_percentage(aCertValidPercentage) {}
        std::string name;
        std::string service_uri;
        unsigned int cert_valid_percentage;
    };
    struct ProviderSettings
    {
        ProviderSettings(const std::string& aName, const std::vector<ServiceSettings>& aServiceSettings)
            : name(aName), services_settings(aServiceSettings) {}
        std::string name;
        std::vector<ServiceSettings> services_settings;
    };
    ProviderSettings getProviderSettings(const std::string& aProviderName) const;
    ServiceSettings getServiceSettings(const std::string& aProviderName, const std::string& aServiceName) const;
    void setServiceSettings(const std::string& aProviderName, const std::string& aServiceName, const ServiceSettingsTab::ServiceSettings& aServiceSettings);
    std::vector<ProviderSettings> theProvidersSettings;
    QComboBox* theProvidersCombo;
    QComboBox* theServicesCombo;
    QLabel* theServiceUriLabel;
    QLabel* theCertValidPercentageLabel;
};

enum ValidationPolicy { validateYes, validateNo };

class IISUpdateTaskSettingDialog : public QDialog
{
    Q_OBJECT
public slots:
    void onSaveClicked();
    void onTestMailClicked();
    void selectProvider(const QString& aProviderName);
    void selectService(const QString& aServiceName);
public:
    IISUpdateTaskSettingDialog(const std::string& aTask);
    void saveValues();
    void loadValues(const ValidationPolicy aValidate);
    bool hasValidValues(std::string& anErrorMsg);
    bool hasValidMailNotificationValues(std::string& anErrorMsg);
private:
    void checkMailNotificationValues(std::vector<std::string>& anErrors);
    const std::string theTask;
    Ui::IISUpdateTaskSettingDialog* theUi;
};

class TaskSettingsTab : public QWidget
{
    Q_OBJECT
public:
    TaskSettingsTab(QWidget *parent = 0);
    void load();
    /**
    @return true if a warning has been issued because of unmet dependencies, false otherwise
    **/
    bool warnDependenciesNotFulfilled();
protected:
private slots:
    void selectTask();
    void addTask();
    void removeSelectedTask();
    void editSelectedTask();
    void editTask(QListWidgetItem* anItem);
    void toggleSelectedTaskEnabled();
private:
    void editTask(const std::string& aTaskName, const ValidationPolicy aValidate);
    void updateTaskList();
    void setTaskListControlsEnabled(const bool aEnabled);
    QListWidget *theTaskList;
    QPushButton* theAddTaskButton;
    QPushButton* theRemoveTaskButton;
    QPushButton* theEditTaskButton;
    QPushButton* theTaskToggleEnabledButton;
    QListWidgetItem* theLastSelectedTask;
    std::string theTask;
    std::vector<QWidget*> theTaskSettings; // All task settings widgets
};

class TaskScheduleCredentialDialog : public QDialog
{
    Q_OBJECT
public:
    TaskScheduleCredentialDialog(QWidget *parent = 0);
    std::string getUserName() const;
    std::string getPassword() const;
protected:
private slots:
    void onOk();
    void onSkip();
private:
    std::string theTaskUserName;
    Ui::TaskScheduleCredentialDialog* theUi;
};

class ReseptConfigManagerUi : public QDialog
{
    Q_OBJECT
public:
    ReseptConfigManagerUi(QWidget* parent = 0);
    ~ReseptConfigManagerUi();

    QTabWidget* getTabWidget() const;
    ProviderSettingsTab* getProviderSettingsTab() const;
    ServiceSettingsTab* getServiceSettingsTab() const;
    TaskSettingsTab* getTaskSettingsTab() const;
private slots:
    void onOkClicked();
private:
    QTabWidget* theTabWidget;
    QDialogButtonBox* theButtonBox;
    std::auto_ptr<ta::OpenSSLApp> theOpenSSLAppPtr;
};
