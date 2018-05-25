#include "ReseptConfigManagerUi.h"
#include "LoadSettingsDialog.h"
#include "LoadSettingsBL.h"
#include "rclient/TaskUtils.h"
#include "rclient/TaskSettings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "rclient/WaitDialog.h"
#include "resept/util.h"
#include "ta/InternetExplorer.h"
#include "ta/process.h"
#include "ta/strings.h"
#include "ta/logappender.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/common.h"

#include <qformlayout.h>
#include <QFileDialog>
#include "boost/bind.hpp"
#include "boost/range/algorithm.hpp"
#include "boost/filesystem.hpp"

using std::string;
using std::vector;

// All UI changes except installing new RCCD are stored in memory until the entire dialog is closed by pressing "OK"

namespace
{
    QColor TaskDisabledColor = QColor(140, 140, 140, 255);
    QColor TaskInvalidColor = QColor(255, 0, 0, 255);

    class TaskSettingLoader
    {
    public:
        typedef boost::function<string (const string&)> StringSettingSource;
        typedef boost::function<unsigned int (const string&)> UIntSettingSource;
        typedef boost::function<bool (const string&)> BoolSettingSource;


        TaskSettingLoader(const string& aTaskName)
            : theTaskName(aTaskName)
        {}

        void loadBool(const BoolSettingSource aSourceCbk, QCheckBox* aCheckBox, const string& anInputLabel)
        {
            try
            {
                aCheckBox->setChecked(aSourceCbk(theTaskName));
            }
            catch (rclient::Settings::TaskSettingsError&)
            {
                theSettingsWithError.push_back(stripInputLabel(anInputLabel));
            }
        }

        void loadUInt(const UIntSettingSource aSourceCbk, QSpinBox* aSpinner, const string& anInputLabel)
        {
            try
            {
                aSpinner->setValue(aSourceCbk(theTaskName));
            }
            catch (rclient::Settings::TaskSettingsError&)
            {
                theSettingsWithError.push_back(stripInputLabel(anInputLabel));
            }
        }

        void loadStr(const StringSettingSource aSourceCbk, QComboBox* aComboBox, const string& anInputLabel)
        {
            try
            {
                aComboBox->setCurrentText(QString::fromStdString(aSourceCbk(theTaskName)));
            }
            catch (rclient::Settings::TaskSettingsError&)
            {
                theSettingsWithError.push_back(stripInputLabel(anInputLabel));
            }
        }


        void loadStr(const StringSettingSource aSourceCbk, QLineEdit* aLineEdit, const string& anInputLabel)
        {
            try
            {
                aLineEdit->setText(QString::fromStdString(aSourceCbk(theTaskName)));
            }
            catch (rclient::Settings::TaskSettingsError&)
            {
                theSettingsWithError.push_back(stripInputLabel(anInputLabel));
            }
        }

        vector<string> settingsWithError()
        {
            return theSettingsWithError;
        }
    private:
        string stripInputLabel(const string& anInputLabel)
        {
            string myStrippedLabel = boost::trim_copy(anInputLabel);
            boost::erase_all(myStrippedLabel, ":");
            return myStrippedLabel;
        }

        vector<string> theSettingsWithError;
        string theTaskName;
    };
}

ReseptConfigManagerUi::ReseptConfigManagerUi(QWidget* parent)
    : QDialog(parent)
{
    try
    {
        theOpenSSLAppPtr.reset(new ta::OpenSSLApp());

        theTabWidget = new QTabWidget;

        theTabWidget->addTab(new GeneralTab(this), "General");

        if (rclient::Settings::isCustomized())
        {
            theTabWidget->addTab(new ProviderSettingsTab,"Provider Settings");
            theTabWidget->addTab(new ServiceSettingsTab, "Service Settings");
            if (rclient::Settings::isScheduledTaskFeatureInstalled())
            {
                theTabWidget->addTab(new TaskSettingsTab, "Task Settings");
            }
        }

        theButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok
                                            | QDialogButtonBox::Cancel);

        setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
        setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);

        connect(theButtonBox, SIGNAL(accepted()), this, SLOT(onOkClicked()));
        connect(theButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

        QVBoxLayout* mainLayout = new QVBoxLayout;
        mainLayout->addWidget(theTabWidget);
        mainLayout->addWidget(theButtonBox);
        setLayout(mainLayout);

        setWindowTitle("Configuration Manager");
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Error initializing RESEPT Configuration Manager", e.what());
        throw;
    }
}

ReseptConfigManagerUi::~ReseptConfigManagerUi()
{}

QTabWidget* ReseptConfigManagerUi::getTabWidget() const
{
    return theTabWidget;
}

void ReseptConfigManagerUi::onOkClicked()
{
    ProviderSettingsTab* myProviderSettingsTab = getProviderSettingsTab();
    if (myProviderSettingsTab)
        myProviderSettingsTab->save();
    ServiceSettingsTab* myServiceSettingsTab = getServiceSettingsTab();
    if (myServiceSettingsTab)
        myServiceSettingsTab->save();
    accept();
}

ProviderSettingsTab* ReseptConfigManagerUi::getProviderSettingsTab() const
{
    ProviderSettingsTab* myRetVal = NULL;
    for (int index=0; index<theTabWidget->count() && theTabWidget->widget(index); ++index)
    {
        myRetVal = dynamic_cast<ProviderSettingsTab*>(theTabWidget->widget(index));
        if (myRetVal)
            break;
    }
    return myRetVal;
}

ServiceSettingsTab* ReseptConfigManagerUi::getServiceSettingsTab() const
{
    ServiceSettingsTab* myRetVal = NULL;
    for (int index=0; index<theTabWidget->count() && theTabWidget->widget(index); ++index)
    {
        myRetVal = dynamic_cast<ServiceSettingsTab*>(theTabWidget->widget(index));
        if (myRetVal)
            break;
    }
    return myRetVal;
}

TaskSettingsTab* ReseptConfigManagerUi::getTaskSettingsTab() const
{
    TaskSettingsTab* myRetVal = NULL;
    for (int index = 0; index < theTabWidget->count() && theTabWidget->widget(index); ++index)
    {
        myRetVal = dynamic_cast<TaskSettingsTab*>(theTabWidget->widget(index));
        if (myRetVal)
            break;
    }
    return myRetVal;
}

/////////////////////////

class InstalledSettingsTable: public QTreeWidget
{
public:
    InstalledSettingsTable()
        : QTreeWidget()
    {
        setColumnCount(3);
        setAlternatingRowColors(true);
        setRootIsDecorated(false);
        setSelectionMode(QAbstractItemView::SingleSelection);

        QStringList myHeader;
        myHeader << "Provider" << "User Settings" << "Master Settings";
        setHeaderLabels(myHeader);
        header()->setStretchLastSection(false);
    }
};// InstalledSettingsTable

GeneralTab::GeneralTab(ReseptConfigManagerUi* parent)
    : QWidget(parent)
    , theParent(parent)
{
    if (!parent)
        TA_THROW_MSG(std::runtime_error, "Parent cannot be NULL");

    theInstalledSettingsTable = new InstalledSettingsTable();

    connect(theInstalledSettingsTable, SIGNAL(itemSelectionChanged()), this, SLOT(onProviderSelectionChanged()));

    theLoadBtn = new QPushButton("Load...");
    theLoadBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    theRemoveBtn = new QPushButton("Remove");
    theRemoveBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    theRemoveBtn->setEnabled(false);

    QHBoxLayout* myBtnsLayout =  new QHBoxLayout;
    myBtnsLayout->addWidget(theLoadBtn);
    myBtnsLayout->addWidget(theRemoveBtn);
    myBtnsLayout->setAlignment(Qt::AlignLeft);

    // layout the widgets vertically
    QGroupBox* settingsGroup = new QGroupBox("Installed Settings");
    QVBoxLayout* myLayout = new QVBoxLayout(settingsGroup);
    myLayout->setAlignment(Qt::AlignTop);
    myLayout->addWidget(theInstalledSettingsTable);
    myLayout->addLayout(myBtnsLayout);
    settingsGroup->setLayout(myLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(settingsGroup);
    setLayout(mainLayout);

    load();

    connect(theLoadBtn, SIGNAL(clicked()), this, SLOT(onLoadClicked()));
    connect(theRemoveBtn, SIGNAL(clicked()), this, SLOT(onRemoveClicked()));
}

void GeneralTab::load()
{
    theInstalledSettingsTable->clear();
    if (!rclient::Settings::isCustomized())
        return;

    foreach (const string& provider, rclient::Settings::getProviders())
    {
        QTreeWidgetItem* row = new QTreeWidgetItem();
        row->setText(0, provider.c_str());
        bool myFromMasterConfig;
        const string myContentVersion = ta::Strings::toString(rclient::Settings::getProviderContentVersion(provider, myFromMasterConfig));
        row->setText(1, myContentVersion.c_str());
        if (myFromMasterConfig)
            row->setText(2, myContentVersion.c_str());
        else
            row->setText(2, "Not Installed");
        theInstalledSettingsTable->addTopLevelItem(row);
    }
    theInstalledSettingsTable->sortByColumn(0, Qt::AscendingOrder);
}

void GeneralTab::reLoadAllTabs()
{
    load();

    QTabWidget* myTabHolder = theParent->getTabWidget();

    if (rclient::Settings::isCustomized())
    {
        if (theParent->getProviderSettingsTab())
            theParent->getProviderSettingsTab()->load();
        else
            myTabHolder->addTab(new ProviderSettingsTab, "Provider Settings");

        if (theParent->getServiceSettingsTab())
            theParent->getServiceSettingsTab()->load();
        else
            myTabHolder->addTab(new ServiceSettingsTab, "Service Settings");

        if (rclient::Settings::isScheduledTaskFeatureInstalled())
        {
            if (theParent->getTaskSettingsTab())
                theParent->getTaskSettingsTab()->load();
            else
                myTabHolder->addTab(new TaskSettingsTab, "Task Settings");
        }
    }
    else
    {
        if (theParent->getProviderSettingsTab())
            myTabHolder->removeTab(myTabHolder->indexOf(theParent->getProviderSettingsTab()));
        if (theParent->getServiceSettingsTab())
            myTabHolder->removeTab(myTabHolder->indexOf(theParent->getServiceSettingsTab()));
        if (theParent->getTaskSettingsTab())
            myTabHolder->removeTab(myTabHolder->indexOf(theParent->getTaskSettingsTab()));
    }
}

void GeneralTab::onLoadClicked()
{
    LoadSettingsDialog myDlg;
    if (myDlg.exec() == QDialog::Accepted)
    {
        reLoadAllTabs();
    }
}

void GeneralTab::onRemoveClicked()
{
    QList <QTreeWidgetItem*> mySelectedItems = theInstalledSettingsTable->selectedItems();
    if (mySelectedItems.size() > 0)
    {
        string myErrorMsg;
        const string mySelectedProvider = mySelectedItems.at(0)->text(0).toStdString();

        if (LoadSettingsBL::uninstallUserSettings(mySelectedProvider, myErrorMsg))
        {
            QMessageBox::information(this, "Provider settings removed", ("Successfully removed settings for provider " + mySelectedProvider).c_str());
        }
        else
        {
            QMessageBox::warning(this, "Unable to remove settings", myErrorMsg.c_str());
        }

        foreach(const string& taskName, rclient::Settings::getTaskNames(rclient::Settings::IISTask))
        {
            const string myTaskProvider(rclient::Settings::IISTaskParameters::getKeyTalkProvider(taskName));
            if (myTaskProvider == mySelectedProvider)
            {
                rclient::Settings::removeTask(rclient::Settings::IISTask, taskName);
            }
        }
        reLoadAllTabs();
    }
    else
    {
        WARNLOG("Remove settings button should have been disabled when no provider selected");
    }
}

void GeneralTab::onProviderSelectionChanged()
{
    QList <QTreeWidgetItem*> mySelectedItems = theInstalledSettingsTable->selectedItems();
    if (mySelectedItems.size() > 0)
    {
        const string mySelectedProvider = mySelectedItems.at(0)->text(0).toStdString();
        bool myFromMasterConfig;
        rclient::Settings::getProviderContentVersion(mySelectedProvider, myFromMasterConfig);

        // allow only KT user settings to be removed
        theRemoveBtn->setEnabled(!myFromMasterConfig);
    }
    else
    {
        theRemoveBtn->setEnabled(false);
    }
}



/////////////////////////
ProviderSettingsTab::ProviderSettingsTab(QWidget* parent)
    : QWidget(parent)
{
    QLabel* providerLabel = new QLabel("Provider:");
    theProvidersCombo = new QComboBox;

    QGroupBox* settingsGroup = new QGroupBox("Settings");
    theLogLevelsCombo = new QComboBox;
    QLabel* serverLabel = new QLabel("Server:");
    theServerEdit = new QLineEdit;
    QLabel* logLevelsLabel = new QLabel("Log Level:");
    QFormLayout* settingsLayout = new QFormLayout;
    settingsLayout->addRow(serverLabel, theServerEdit);
    settingsLayout->addRow(logLevelsLabel, theLogLevelsCombo);
    settingsGroup->setLayout(settingsLayout);

    QFormLayout* mainLayout = new QFormLayout;
    mainLayout->addRow(providerLabel, theProvidersCombo);
    mainLayout->addRow(settingsGroup);
    setLayout(mainLayout);

    load();

    connect(theProvidersCombo, SIGNAL(activated(const QString&)), this, SLOT(onProviderSelected(const QString&)) );
}

void ProviderSettingsTab::load()
{
    theProvidersSettings.clear();
    BOOST_FOREACH (const string& providerName, rclient::Settings::getProviders())// full macro name because Qt defines its own foreach
    {
        bool myReseptSvrAddressFromMasterConfig = true;
        const ta::NetUtils::RemoteAddress myServerAddress = rclient::Settings::getReseptSvrAddress(providerName, myReseptSvrAddressFromMasterConfig);
        bool myLogLevelFromMasterConfig = true;
        const string myLogLevelStr = rclient::Settings::getLogLevel(providerName, myLogLevelFromMasterConfig);
        theProvidersSettings.push_back(ProviderSettings(providerName, myServerAddress, myReseptSvrAddressFromMasterConfig, myLogLevelStr, myLogLevelFromMasterConfig));
    }
    theSelectedProvider = rclient::Settings::getLatestProvider();
    updateUiFromProviderSettings(theSelectedProvider);
}

void ProviderSettingsTab::save()
{
    updateProviderSettingsFromUi(theProvidersCombo->currentText().toStdString());

    BOOST_FOREACH (const ProviderSettings& provider, theProvidersSettings)// because Qt defines its own foreach
    {
        if (!provider.svr_addr_ronly)
            rclient::Settings::setReseptSvrAddress(provider.name, provider.svr_addr);
        if (!provider.log_level_ronly)
            rclient::Settings::setLogLevel(provider.name, provider.log_level);
    }
}

ProviderSettingsTab::ProviderSettings ProviderSettingsTab::getProviderSettings(const string& aProviderName) const
{
    std::vector<ProviderSettings>::const_iterator myIt = boost::find_if(theProvidersSettings, boost::bind(&ProviderSettings::name, _1) == aProviderName);
    if (myIt == theProvidersSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Provider %s does not exist") % aProviderName);
    return *myIt;
}

void ProviderSettingsTab::setProviderSettings(const string& aProviderName, const ProviderSettingsTab::ProviderSettings& aProviderSettings)
{
    std::vector<ProviderSettings>::iterator myProviderIt = boost::find_if(theProvidersSettings, boost::bind(&ProviderSettings::name, _1) == aProviderName);
    if (myProviderIt == theProvidersSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Provider %s does not exist") % aProviderName);
    *myProviderIt = aProviderSettings;
}

void ProviderSettingsTab::updateUiFromProviderSettings(const string& aSelectedProvider)
{
    // Fill in providers combo
    theProvidersCombo->clear();
    std::list<string> myProviderNames;
    BOOST_FOREACH (const ProviderSettings& provider, theProvidersSettings)// because Qt defines its own foreach
    {
        myProviderNames.push_back(provider.name);
    }
    myProviderNames.sort();
    BOOST_FOREACH (const string& providerName, myProviderNames)// because Qt defines its own foreach
    {
        theProvidersCombo->addItem(providerName.c_str());
    }
    theProvidersCombo->setCurrentIndex(theProvidersCombo->findText(aSelectedProvider.c_str()));

    const ProviderSettings myProviderSettings = getProviderSettings(aSelectedProvider);

    // RESEPT Server address
    const string myServerAddressStr = toString(myProviderSettings.svr_addr, rclient::Settings::DefRcdpV2Port);
    theServerEdit->setText(myServerAddressStr.c_str());
    theServerEdit->setEnabled(!myProviderSettings.svr_addr_ronly);

    // Log Level
    theLogLevelsCombo->clear();
    unsigned int myLogLevelIndex = 0, mySelectedLogLevelIndex = (unsigned int)-1;
    BOOST_FOREACH (const string& logLevelStr, ta::LogLevel::strs) // because Qt defines its own foreach
    {
        theLogLevelsCombo->addItem(logLevelStr.c_str());
        if (logLevelStr == myProviderSettings.log_level)
            mySelectedLogLevelIndex = myLogLevelIndex;
        ++myLogLevelIndex;
    }
    if (mySelectedLogLevelIndex == -1)
        TA_THROW_MSG(std::logic_error, boost::format("Log Level %s not found") % myProviderSettings.log_level);
    theLogLevelsCombo->setCurrentIndex(mySelectedLogLevelIndex);
    theLogLevelsCombo->setEnabled(!myProviderSettings.log_level_ronly);
}

bool ProviderSettingsTab::updateProviderSettingsFromUi(const string& aSelectedProvider)
{
    ProviderSettings myProviderSettings = getProviderSettings(aSelectedProvider);

    const string myReseptSvrAddrStr = boost::trim_copy(theServerEdit->text().toStdString());
    try
    {
        myProviderSettings.svr_addr = ta::NetUtils::parseHost(myReseptSvrAddrStr, rclient::Settings::DefRcdpV2Port);
    }
    catch (std::exception& e)
    {
        WARNLOG(boost::format("Invalid RESEPT server address \"%s\". %s") % myReseptSvrAddrStr % e.what());
        QMessageBox::warning(this, "Invalid server address", str(boost::format("Invalid %s server address \"%s\"") % resept::ProductName % myReseptSvrAddrStr).c_str());
        return false;
    }
    myProviderSettings.log_level = theLogLevelsCombo->currentText().toStdString();
    setProviderSettings(aSelectedProvider, myProviderSettings);
    return true;
}

void ProviderSettingsTab::onProviderSelected(const QString& aText)
{
    // First save the previous provider
    const string myPrevSelectedProvider = theSelectedProvider;
    if (!updateProviderSettingsFromUi(myPrevSelectedProvider))
        return;

    // Now load the newly selected provider
    theSelectedProvider = aText.toStdString();
    updateUiFromProviderSettings(theSelectedProvider);
}


//////////

ServiceSettingsTab::ServiceSettingsTab(QWidget* parent)
    : QWidget(parent)
{
    QLabel* providerLabel = new QLabel("Provider:");
    theProvidersCombo = new QComboBox;

    QLabel* serviceLabel = new QLabel("Service:");
    theServicesCombo = new QComboBox;

    QGroupBox* settingsGroup = new QGroupBox("Settings");
    theServiceUriLabel = new QLabel;
    theCertValidPercentageLabel = new QLabel;
    QFormLayout* settingsLayout = new QFormLayout;
    settingsLayout->addRow(new QLabel("Service URI:"), theServiceUriLabel);
    settingsLayout->addRow(new QLabel("Certificate Validity:"), theCertValidPercentageLabel);
    settingsGroup->setLayout(settingsLayout);

    QFormLayout* mainLayout = new QFormLayout;
    mainLayout->addRow(providerLabel, theProvidersCombo);
    mainLayout->addRow(serviceLabel, theServicesCombo);
    mainLayout->addRow(settingsGroup);
    setLayout(mainLayout);

    load();

    connect(theProvidersCombo, SIGNAL(activated(const QString&)), this, SLOT(onProviderSelected(const QString&)) );
    connect(theServicesCombo, SIGNAL(activated(const QString&)), this, SLOT(onServiceSelected(const QString&)) );
}

ServiceSettingsTab::ProviderSettings ServiceSettingsTab::getProviderSettings(const string& aProviderName) const
{
    std::vector<ProviderSettings>::const_iterator myIt = boost::find_if(theProvidersSettings, boost::bind(&ProviderSettings::name, _1) == aProviderName);
    if (myIt == theProvidersSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Provider %s does not exist") % aProviderName);
    return *myIt;
}

ServiceSettingsTab::ServiceSettings ServiceSettingsTab::getServiceSettings(const string& aProviderName, const string& aServiceName) const
{
    std::vector<ProviderSettings>::const_iterator myProviderIt = boost::find_if(theProvidersSettings, boost::bind(&ProviderSettings::name, _1) == aProviderName);
    if (myProviderIt == theProvidersSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Provider %s does not exist") % aProviderName);

    const vector<ServiceSettings> myServicesSettings = myProviderIt->services_settings;
    std::vector<ServiceSettings>::const_iterator myServiceIt = boost::find_if(myServicesSettings, boost::bind(&ServiceSettings::name, _1) == aServiceName);
    if (myServiceIt == myServicesSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Service %s does not exist") % aServiceName);

    return *myServiceIt;
}

void ServiceSettingsTab::setServiceSettings(const string& aProviderName, const string& aServiceName, const ServiceSettingsTab::ServiceSettings& aServiceSettings)
{
    std::vector<ProviderSettings>::iterator myProviderIt = boost::find_if(theProvidersSettings, boost::bind(&ProviderSettings::name, _1) == aProviderName);
    if (myProviderIt == theProvidersSettings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Provider %s does not exist") % aProviderName);

    std::vector<ServiceSettings>::iterator myServiceIt = boost::find_if(myProviderIt->services_settings, boost::bind(&ServiceSettings::name, _1) == aServiceName);
    if (myServiceIt == myProviderIt->services_settings.end())
        TA_THROW_MSG(std::logic_error, boost::format("Service %s does not exist") % aServiceName);

    *myServiceIt = aServiceSettings;
}

void ServiceSettingsTab::load()
{
    theProvidersSettings.clear();
    BOOST_FOREACH (const string& providerName, rclient::Settings::getProviders())// full macro name because Qt defines its own foreach
    {
        vector<ServiceSettings> myServicesSettings;
        BOOST_FOREACH (const string& serviceName, rclient::Settings::getServices(providerName))// full macro name because Qt defines its own foreach
        {
            const string myServiceUri = rclient::Settings::getServiceUri(providerName, serviceName);
            const unsigned int myCertValidityPercentage = rclient::Settings::getCertValidPercentage(providerName, serviceName);
            myServicesSettings.push_back(ServiceSettings(serviceName, myServiceUri, myCertValidityPercentage));
        }
        theProvidersSettings.push_back(ProviderSettings(providerName, myServicesSettings));
    }
    updateUiFromServiceSettings(rclient::Settings::getLatestProvider(), rclient::Settings::getLatestService());
}

void ServiceSettingsTab::save()
{
    //nothing to save for service configuration
}

void ServiceSettingsTab::updateUiFromServiceSettings(const string& aSelectedProvider, const string& aSelectedService)
{
    // Fill in providers combo
    theProvidersCombo->clear();
    std::list<string> myProviderNames;
    BOOST_FOREACH (const ProviderSettings& provider, theProvidersSettings)// because Qt defines its own foreach
    {
        myProviderNames.push_back(provider.name);
    }
    myProviderNames.sort();
    BOOST_FOREACH (const string&  providerName, myProviderNames)// because Qt defines its own foreach
    {
        theProvidersCombo->addItem(providerName.c_str());
    }
    theProvidersCombo->setCurrentIndex(theProvidersCombo->findText(aSelectedProvider.c_str()));

    // Fill in services combo
    theServicesCombo->clear();
    std::list<string> myServiceNames;
    BOOST_FOREACH (const ServiceSettings& service, getProviderSettings(aSelectedProvider).services_settings)// because Qt defines its own foreach
    {
        myServiceNames.push_back(service.name);
    }
    myServiceNames.sort();
    BOOST_FOREACH (const string&  serviceName, myServiceNames)// because Qt defines its own foreach
    {
        theServicesCombo->addItem(serviceName.c_str());
    }
    theServicesCombo->setCurrentIndex(theServicesCombo->findText(aSelectedService.c_str()));

    // Fill in service settings
    const ServiceSettings myServiceSettings = getServiceSettings(aSelectedProvider, aSelectedService);
    theServiceUriLabel->setText(myServiceSettings.service_uri.c_str());
    theCertValidPercentageLabel->setText(str(boost::format("%u%%") % myServiceSettings.cert_valid_percentage).c_str());
}


void ServiceSettingsTab::onProviderSelected(const QString& aSelectedProvider)
{
    const string mySelectedProvider = aSelectedProvider.toStdString();
    const string mySelectedService = rclient::Settings::getServices(theProvidersCombo->currentText().toStdString()).at(0);
    updateUiFromServiceSettings(mySelectedProvider, mySelectedService);
}

void ServiceSettingsTab::onServiceSelected(const QString& aSelectedService)
{
    updateUiFromServiceSettings(theProvidersCombo->currentText().toStdString(), aSelectedService.toStdString());
}


//////////
IISUpdateTaskSettingDialog::IISUpdateTaskSettingDialog(const string& aTask)
    : theTask(aTask)
    , theUi(NULL)
{
    Qt::WindowFlags flags = windowFlags();
    flags = flags & (~Qt::WindowContextHelpButtonHint);
    setWindowFlags(flags);

    theUi = new Ui::IISUpdateTaskSettingDialog;
    theUi->setupUi(this);

    setWindowTitle(QString::fromStdString("IIS HTTPS Binding Certificate Update Task: " + theTask));

    connect(theUi->keyTalkProvider, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(selectProvider(const QString&)));
    connect(theUi->keyTalkService, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(selectService(const QString&)));
    connect(theUi->sendTestMailButton, SIGNAL(clicked()), this, SLOT(onTestMailClicked()));
    connect(theUi->saveButton, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
    connect(theUi->cancelButton, SIGNAL(clicked()), this, SLOT(accept()));

    foreach(const string& provider, rclient::Settings::getProviders())
    {
        theUi->keyTalkProvider->addItem(QString::fromStdString(provider));
    }
    // Notice that combos with KT services and KT users are implicitly populated from selectProvider() and selectService() slots whenever a provider/service is selected

    foreach(const string& rootStore, rclient::NativeCertStore::getStoreNames())
    {
        theUi->certificateStore->addItem(QString::fromStdString(rootStore));
    }

    theUi->httpsBindingIp->addItem(QString::fromStdString("*"));
    theUi->httpsBindingIp->addItem(QString::fromStdString("0.0.0.0"));
    foreach(const string& ipAddress, ta::NetUtils::getMyIpv4())
    {
        theUi->httpsBindingIp->addItem(QString::fromStdString(ipAddress));
    }
}

void IISUpdateTaskSettingDialog::onTestMailClicked()
{
    string myErrorMsg;
    if (!hasValidMailNotificationValues(myErrorMsg))
    {
        QMessageBox::warning(this, "Incorrect settings", QString::fromStdString(myErrorMsg));
        ERRORLOG("Could not send test mail due to invalid form values: " + myErrorMsg);
        return;
    }

    try
    {
        string myPowershellCmd = boost::str(
                                     boost::format("if (SendTestMail \"%s\" \"%s\" \"%s\" \"%s\") {Exit 0} else {Exit 1}") %
                                     theUi->emailFrom->text().toStdString() %
                                     theUi->emailTo->text().toStdString() %
                                     theUi->emailSubject->text().toStdString() %
                                     theUi->smtpServer->text().toStdString()
                                 );

        string myStdOut;
        string myStdErr;
        int ret;

        {
            WaitDialog waitDialog("Sending test mail...", this);
            ret = rclient::executePowerShellCode(rclient::getKeyTalkUtilsScriptPath(), myPowershellCmd, myStdOut, myStdErr);
        }

        if (ret == 0) {
            QMessageBox::information(this, "Test e-mail", QString::fromStdString(myStdOut));
        }
        else {
            WARNLOG("Could not send test e-mail: " + myStdOut);
            QMessageBox::warning(this, "Test e-mail", QString::fromStdString(myStdOut));
        }
    }
    catch (std::exception& ex)
    {
        QMessageBox::warning(this, "Test e-mail", "An internal error occurred while sending the test e-mail.");
        ERRORLOG2("An internal error occurred while sending the test e-mail.", ex.what());
    }
}

void IISUpdateTaskSettingDialog::onSaveClicked()
{
    try
    {
        saveValues();
        close();
    }
    catch (rclient::Settings::TaskSettingsError& e)
    {
        ERRORLOG2(boost::format("Cannot save settings for task '%s'. %s") % theTask % e.friendlyMessage(), e.what());
        QMessageBox::warning(this, "Cannot save task settings", QString::fromStdString(e.friendlyMessage()));
        // Do not close the dialog: the user should first fix the errors
    }
}

void IISUpdateTaskSettingDialog::checkMailNotificationValues(vector<string>& anErrors)
{
    string myStringValue = theUi->emailFrom->text().toStdString();
    if (!ta::isValidEmail(myStringValue))
    {
        if (boost::trim_copy(myStringValue) == "")
        {
            anErrors.push_back("From Email address may not be empty.");
        }
        else
        {
            anErrors.push_back("From Email address invalid.");
        }
    }

    myStringValue = theUi->emailTo->text().toStdString();
    if (!ta::isValidEmail(myStringValue))
    {
        if (boost::trim_copy(myStringValue) == "")
        {
            anErrors.push_back("To Email address may not be empty.");
        }
        else
        {
            anErrors.push_back("To Email address invalid.");
        }
    }

    myStringValue = theUi->smtpServer->text().toStdString();
    ta::NetUtils::DomainNameValidationResult myDNResult;
    if (!ta::NetUtils::isValidDomainName(myStringValue, myDNResult, ta::NetUtils::dnsName))
    {
        if (boost::trim_copy(myStringValue) == "")
        {
            anErrors.push_back("SMTP server name may not be empty.");
        }
        else
        {
            anErrors.push_back("SMTP server is not a valid DNS name. Currently only plain DNS names without port numbers are supported.");
        }
    }
}

bool IISUpdateTaskSettingDialog::hasValidMailNotificationValues(string& anErrorMsg)
{
    vector<string> myErrors;

    checkMailNotificationValues(myErrors);

    if (myErrors.empty())
    {
        return true;
    }
    else
    {
        anErrorMsg = ta::Strings::join(myErrors, "\n");
        return false;
    }
}

bool IISUpdateTaskSettingDialog::hasValidValues(string& anErrorMsg)
{
    vector<string> myErrors;
    string myError;
    if (!resept::isValidProviderName(theUi->keyTalkProvider->currentText().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    if (!resept::isValidServiceName(theUi->keyTalkService->currentText().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    if (!resept::isValidUserName(theUi->keyTalkUser->currentText().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    if (!resept::isValidPassword(theUi->keyTalkPassword->text().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    if (!rclient::NativeCertStore::isStoreExists(theUi->certificateStore->currentText().toStdString()))
    {
        myErrors.push_back("Certificate store cannot be found.");
    }

    if (!rclient::Settings::IISTaskParameters::isValidScriptLogFilePath(theUi->scriptLogFilePath->text().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    if (!rclient::Settings::IISTaskParameters::isValidHttpsBindingIp(theUi->httpsBindingIp->currentText().toStdString(), myError))
    {
        myErrors.push_back(myError);
    }

    checkMailNotificationValues(myErrors);

    if (myErrors.empty())
    {
        return true;
    }
    else
    {
        anErrorMsg = ta::Strings::join(myErrors, "\n");
        return false;
    }
}

void IISUpdateTaskSettingDialog::saveValues()
{
    string myErrorMsg;
    if (!hasValidValues(myErrorMsg))
    {
        TA_THROW_MSG2(rclient::Settings::TaskSettingsError, myErrorMsg, myErrorMsg);
    }

    if (!rclient::Settings::isTaskExists(rclient::Settings::IISTask, theTask))
    {
        rclient::Settings::addTask(rclient::Settings::IISTask, theTask);
    }

    using namespace rclient::Settings::IISTaskParameters;
    setKeyTalkProvider(theTask, theUi->keyTalkProvider->currentText().toStdString());

    setKeyTalkService(theTask, theUi->keyTalkService->currentText().toStdString());

    setKeyTalkUser(theTask, theUi->keyTalkUser->currentText().toStdString());

    setKeyTalkPassword(theTask, theUi->keyTalkPassword->text().toStdString());

    setCertificateStore(theTask, theUi->certificateStore->currentText().toStdString());

    setHttpsBindingIp(theTask, theUi->httpsBindingIp->currentText().toStdString());

    setHttpsBindingPort(theTask, theUi->httpsBindingPort->value());

    const string myLogFilePath = boost::trim_copy(theUi->scriptLogFilePath->text().toStdString());
    setScriptLogFilePath(theTask, myLogFilePath);

    const boost::filesystem::path myPath(myLogFilePath);
    if (!boost::filesystem::exists(myPath))
    {
        ta::writeData(myLogFilePath, string(""));
    }

    const bool myEmailReportingEnabled = theUi->emailReporting->isChecked();
    setEmailReporting(theTask, myEmailReportingEnabled);

    if (myEmailReportingEnabled)
    {
        setSendEmailOnSuccess(theTask, theUi->sendEmailOnSuccess->isChecked());

        setEmailFrom(theTask, theUi->emailFrom->text().toStdString());

        setEmailTo(theTask, theUi->emailTo->text().toStdString());

        setEmailSubject(theTask, theUi->emailSubject->text().toStdString());

        setSmtpServer(theTask, theUi->smtpServer->text().toStdString());
    }
}

void IISUpdateTaskSettingDialog::loadValues(ValidationPolicy aValidate)
{
    using namespace rclient::Settings::IISTaskParameters;

    TaskSettingLoader settings(theTask);
    settings.loadStr(&getKeyTalkProvider,
                     theUi->keyTalkProvider,
                     theUi->keyTalkProviderLabel->text().toStdString());

    settings.loadStr(&getKeyTalkService,
                     theUi->keyTalkService,
                     theUi->keyTalkServiceLabel->text().toStdString());

    settings.loadStr(&getKeyTalkUser,
                     theUi->keyTalkUser,
                     theUi->keyTalkUserLabel->text().toStdString());

    settings.loadStr(&getKeyTalkPassword,
                     theUi->keyTalkPassword,
                     theUi->keyTalkPasswordLabel->text().toStdString());

    settings.loadStr(&getCertificateStore,
                     theUi->certificateStore,
                     theUi->certificateStoreLabel->text().toStdString());

    settings.loadStr(&getHttpsBindingIp,
                     theUi->httpsBindingIp,
                     theUi->httpsBindingIpLabel->text().toStdString());

    settings.loadUInt(&getHttpsBindingPort,
                      theUi->httpsBindingPort,
                      theUi->httpsBindingPortLabel->text().toStdString());

    settings.loadStr(&getScriptLogFilePath,
                     theUi->scriptLogFilePath,
                     theUi->scriptLogFilePathLabel->text().toStdString());

    settings.loadBool(&getEmailReporting,
                      theUi->emailReporting,
                      theUi->emailReporting->text().toStdString());

    bool myEmailReporting = false;
    try
    {
        myEmailReporting = getEmailReporting(theTask);
    }
    catch (rclient::Settings::TaskSettingsError&)
    {
        // If this gives an error, it is already reported while loading the EmailReporting setting into the UI
    }

    if (myEmailReporting)
    {
        settings.loadBool(&getSendEmailOnSuccess,
                          theUi->sendEmailOnSuccess,
                          theUi->sendEmailOnSuccessLabel->text().toStdString());

        settings.loadStr(&getEmailFrom,
                         theUi->emailFrom,
                         theUi->emailFromLabel->text().toStdString());

        settings.loadStr(&getEmailTo,
                         theUi->emailTo,
                         theUi->emailToLabel->text().toStdString());

        settings.loadStr(&getEmailSubject,
                         theUi->emailSubject,
                         theUi->emailSubjectLabel->text().toStdString());

        settings.loadStr(&getSmtpServer,
                         theUi->smtpServer,
                         theUi->smtpServerLabel->text().toStdString());
    }

    if (aValidate == validateYes)
    {
        vector<string> myErrors = settings.settingsWithError();
        if (!myErrors.empty())
        {
            string myMessage = "The following settings in the selected task '" + theTask + "' need to be configured: <p>";
            string myLogMessage = "Found unconfigured settings while loading task '" + theTask + "':\n";

            myMessage += ta::Strings::join(settings.settingsWithError(), "<br />");
            myLogMessage += ta::Strings::join(settings.settingsWithError(), "\n");
            myMessage += "</p> <p>Please configure the settings and save the configuration.</p>";

            WARNLOG(myLogMessage);
            QMessageBox::warning(this, "Please update task configuration", myMessage.c_str());
        }
    }
}

void IISUpdateTaskSettingDialog::selectProvider(const QString& aProviderName)
{
    const string myProviderName = aProviderName.toStdString();

    theUi->keyTalkService->clear();
    if (!aProviderName.isEmpty())
    {
        foreach(const string& service, rclient::Settings::getServices(myProviderName))
        {
            theUi->keyTalkService->addItem(QString::fromStdString(service));
        }

    }
}

void IISUpdateTaskSettingDialog::selectService(const QString& aServiceName)
{
    const string myProviderName = theUi->keyTalkProvider->currentText().toStdString();
    const string myServiceName = aServiceName.toStdString();

    theUi->keyTalkUser->clear();
    if (!aServiceName.isEmpty())
    {
        rclient::Settings::Users myUsers = rclient::Settings::getUsers(myProviderName, myServiceName);

        // add FQDN to the list of users to make admin's life somewhat easier
        try
        {
            const string myFQDN = ta::NetUtils::getSelfFqdn();
            if (!ta::isElemExist(myFQDN, myUsers))
            {
                myUsers.insert(myUsers.begin(), myFQDN);  // insert at the beginning to make it selected by default
            }
        }
        catch (std::exception& e)
        {
            WARNDEVLOG(e.what());
        }

        foreach(const string& user, myUsers)
        {
            theUi->keyTalkUser->addItem(QString::fromStdString(user));
        }
    }
}

//////////

TaskScheduleCredentialDialog::TaskScheduleCredentialDialog(QWidget *parent)
    : QDialog(parent)
    , theTaskUserName(rclient::getScheduledTaskUserName())
{
    Qt::WindowFlags flags = windowFlags();
    flags = flags & (~Qt::WindowContextHelpButtonHint);
    setWindowFlags(flags);

    theUi = new Ui::TaskScheduleCredentialDialog;
    theUi->setupUi(this);

    theUi->userName->setText(QString::fromStdString(theTaskUserName));
    theUi->userName->setEnabled(false);

    connect(theUi->okButton, SIGNAL(clicked()), this, SLOT(onOk()));
    connect(theUi->skipButton, SIGNAL(clicked()), this, SLOT(onSkip()));
}

string TaskScheduleCredentialDialog::getUserName() const
{
    return theTaskUserName;
}

string TaskScheduleCredentialDialog::getPassword() const
{
    return theUi->password->text().toStdString();
}

void TaskScheduleCredentialDialog::onOk()
{
    accept();
}

void TaskScheduleCredentialDialog::onSkip()
{
    reject();
}
//////////

TaskSettingsTab::TaskSettingsTab(QWidget *parent)
    : QWidget(parent)
    , theLastSelectedTask(NULL)
{
    // Task list controls
    QLabel* myTaskLabel = new QLabel("Task:");
    theTaskList = new QListWidget;
    theTaskList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    theTaskList->setSelectionMode(QAbstractItemView::SingleSelection);
    theAddTaskButton = new QPushButton("Add", this);
    theAddTaskButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    theRemoveTaskButton = new QPushButton("Remove", this);
    theRemoveTaskButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    theEditTaskButton = new QPushButton("Edit", this);
    theEditTaskButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    theTaskToggleEnabledButton = new QPushButton("Enable/Disable", this);
    theTaskToggleEnabledButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QHBoxLayout* myTaskListLayout = new QHBoxLayout;
    QVBoxLayout* myTaskListButtonsLayout = new QVBoxLayout;
    myTaskListLayout->addWidget(theTaskList);
    myTaskListButtonsLayout->addWidget(theAddTaskButton);
    myTaskListButtonsLayout->addItem(new QSpacerItem(10,10));
    myTaskListButtonsLayout->addWidget(theEditTaskButton);
    myTaskListButtonsLayout->addItem(new QSpacerItem(10, 10));
    myTaskListButtonsLayout->addWidget(theTaskToggleEnabledButton);
    myTaskListButtonsLayout->addItem(new QSpacerItem(10, 10));
    myTaskListButtonsLayout->addWidget(theRemoveTaskButton);
    myTaskListButtonsLayout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
    myTaskListLayout->addItem(myTaskListButtonsLayout);

    // Signals
    connect(theAddTaskButton, SIGNAL(clicked()), this, SLOT(addTask()));
    connect(theTaskList, SIGNAL(itemSelectionChanged()), this, SLOT(selectTask()));
    connect(theRemoveTaskButton, SIGNAL(clicked()), this, SLOT(removeSelectedTask()));
    connect(theEditTaskButton, SIGNAL(clicked()), this, SLOT(editSelectedTask()));
    connect(theTaskToggleEnabledButton, SIGNAL(clicked()), this, SLOT(toggleSelectedTaskEnabled()));
    connect(theTaskList, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(editTask(QListWidgetItem*)));

    // Layout
    QLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(myTaskLabel);
    mainLayout->addItem(myTaskListLayout);
    setLayout(mainLayout);

    load();
}

void TaskSettingsTab::updateTaskList()
{
    theTaskList->clear();
    foreach(const string& taskName, rclient::Settings::getTaskNames(rclient::Settings::IISTask))
    {
        const bool myIsTaskValid = rclient::Settings::IISTaskParameters::isValidIISTask(taskName);
        const bool myIsTaskEnabled = rclient::Settings::getTaskEnabled(taskName);

        string myTaskDisplayName = taskName;
        if (!myIsTaskEnabled)
        {
            myTaskDisplayName += " (DISABLED)";
        }
        if (!myIsTaskValid)
        {
            myTaskDisplayName += " (INVALID)";
        }

        QListWidgetItem* myItem = new QListWidgetItem(QString::fromStdString(myTaskDisplayName));
        myItem->setData(Qt::ItemDataRole::UserRole, QString::fromStdString(taskName));

        if (!myIsTaskEnabled)
        {
            myItem->setForeground(TaskDisabledColor); // Color the item text grey
        }
        else if (!myIsTaskValid)
        {
            myItem->setForeground(TaskInvalidColor); // Color the item text red
        }
        theTaskList->addItem(myItem);
    }
}

void TaskSettingsTab::setTaskListControlsEnabled(const bool aEnabled)
{
    theRemoveTaskButton->setEnabled(aEnabled);
    theEditTaskButton->setEnabled(aEnabled);
    theTaskToggleEnabledButton->setEnabled(aEnabled);
}

void TaskSettingsTab::load()
{
    updateTaskList();
    setTaskListControlsEnabled(!theTaskList->selectedItems().empty());
}

bool TaskSettingsTab::warnDependenciesNotFulfilled()
{
    string myErrorMsg;
    bool myCheckResult;
    {
        WaitDialog waitDialog("Checking IIS...", this);
        myCheckResult = rclient::isIISInstalled(myErrorMsg);
    }
    if (!myCheckResult)
    {
        WARNLOG(myErrorMsg);
        QMessageBox::warning(this, "Missing dependency", QString::fromStdString(myErrorMsg));
        return true;
    }

    {
        WaitDialog waitDialog("Checking Powershell...", this);
        myCheckResult = rclient::isPowerShellInstalled(myErrorMsg);
    }
    if (!myCheckResult)
    {
        WARNLOG(myErrorMsg);
        QMessageBox::warning(this, "Missing dependency", QString::fromStdString(myErrorMsg + "\nPlease make sure to install Windows Management Framework version 3 or newer before enabling KeyTalk client scheduled task"));
        return true;
    }

    {
        WaitDialog waitDialog("Checking WebAdministration Module...", this);
        myCheckResult = rclient::isPowerShellWebAdministrationModuleAvailable(myErrorMsg);
    }
    if (!myCheckResult)
    {
        WARNLOG(myErrorMsg);
        QMessageBox::warning(this, "Missing dependency", QString::fromStdString(myErrorMsg + "\nPlease make sure to install Windows Management Framework version 3 or newer before enabling KeyTalk client scheduled task."));
        return true;
    }
    return false;
}

void TaskSettingsTab::addTask()
{
    if (warnDependenciesNotFulfilled())
    {
        return;
    }

    while (!rclient::isScheduledTaskRunsOnStartup())
    {
        TaskScheduleCredentialDialog myDialog;
        myDialog.exec();
        const int myResult = myDialog.result();

        if (myResult == QDialog::Accepted)
        {
            string myErrorMsg;
            bool mySuccess = rclient::enableScheduledTaskAtSystemStartup(myDialog.getUserName(), myDialog.getPassword(), myErrorMsg);
            if (!mySuccess)
            {
                ERRORLOG("Error while changing scheduled task: " + myErrorMsg);
                QMessageBox::warning(this, "Error while changing scheduled task", QString::fromStdString(myErrorMsg));
                continue; // retry
            }
            break;
        }
        else if (myResult == QDialog::Rejected)
        {
            break;
        }

        ERRORLOG2("Error while changing scheduled task", boost::format("Result from dialog '%d' unexpected.") % myResult);
        QMessageBox::warning(this, "Error while changing scheduled task", "Internal Error");
        break;
    }

    bool myOkPressed;
    const string myNewTaskName = boost::trim_copy(QInputDialog::getText(this, "New task", "Task name:", QLineEdit::Normal, QString(), &myOkPressed).toStdString());
    if (myOkPressed)
    {
        string myErrorMsg;
        if (!rclient::Settings::isValidTaskName(myNewTaskName, myErrorMsg))
        {
            ERRORLOG("Cannot add task: " + myErrorMsg);
            QMessageBox::warning(this, "Cannot add task", QString::fromStdString(myErrorMsg));
            return;
        }

        if (rclient::Settings::isTaskExists(rclient::Settings::IISTask, myNewTaskName))
        {
            ERRORLOG("Cannot add task: Task already exists.");
            QMessageBox::warning(this, "Cannot add task", "Task already exists.");
            return;
        }

        editTask(myNewTaskName, validateNo);
        load(); // Refreshes task list
    }
}

void TaskSettingsTab::removeSelectedTask()
{
    int mySelection = theTaskList->currentRow();
    foreach(QListWidgetItem* item, theTaskList->selectedItems())
    {
        const string myTaskName = item->data(Qt::ItemDataRole::UserRole).toString().toStdString();

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this,
                                      "Remove task",
                                      QString::fromStdString(boost::str(boost::format("Remove task '%s'?") % myTaskName)),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
            try
            {
                rclient::Settings::removeTask(rclient::Settings::IISTask, myTaskName);
            }
            catch (rclient::Settings::TaskSettingsError& e)
            {
                ERRORLOG2(boost::format("Cannot remove task \"%s\". %s") % myTaskName % e.friendlyMessage(), e.what());
                QMessageBox::warning(this, "Cannot remove task", QString::fromStdString(e.friendlyMessage()));
            }
        }
    }
    load();

    mySelection = mySelection < theTaskList->count() ? mySelection : theTaskList->count() - 1;
    theTaskList->setCurrentRow(mySelection);
}

void TaskSettingsTab::editTask(const string& aTaskName, const ValidationPolicy aValidate)
{
    IISUpdateTaskSettingDialog mySettingsDialog(aTaskName);

    try
    {
        mySettingsDialog.loadValues(aValidate);
    }
    catch (rclient::Settings::TaskSettingsError& e)
    {
        ERRORLOG2(boost::format("Error loading task settings '%s'. %s") % aTaskName % e.friendlyMessage(), e.what());
        QMessageBox::warning(this, "Cannot load task settings", QString::fromStdString(e.friendlyMessage()));
    }

    mySettingsDialog.exec();
}

void TaskSettingsTab::editTask(QListWidgetItem* anItem)
{
    if (warnDependenciesNotFulfilled())
    {
        return;
    }

    const string myTaskName = anItem->data(Qt::ItemDataRole::UserRole).toString().toStdString();
    editTask(myTaskName, validateYes);
    load();
}

void TaskSettingsTab::editSelectedTask()
{
    foreach(QListWidgetItem* item, theTaskList->selectedItems())
    {
        editTask(item);
    }
    load();
}

void TaskSettingsTab::toggleSelectedTaskEnabled()
{
    int mySelection = theTaskList->currentRow();
    foreach(QListWidgetItem* item, theTaskList->selectedItems())
    {
        const string myTaskName = item->data(Qt::ItemDataRole::UserRole).toString().toStdString();
        const bool myEnabled = rclient::Settings::getTaskEnabled(myTaskName);
        rclient::Settings::setTaskEnabled(myTaskName, !myEnabled);
    }

    load();
    theTaskList->setCurrentRow(mySelection);
}

void TaskSettingsTab::selectTask()
{
    const bool myIsSingleSelection = theTaskList->selectedItems().size() == 1;

    setTaskListControlsEnabled(myIsSingleSelection);

    if (myIsSingleSelection)
    {
        const string myTaskName = theTaskList->selectedItems()[0]->data(Qt::ItemDataRole::UserRole).toString().toStdString();
        const string myButtonLabel = rclient::Settings::getTaskEnabled(myTaskName) ? "Disable" : "Enable";
        theTaskToggleEnabledButton->setText(QString::fromStdString(myButtonLabel));
    }
}
