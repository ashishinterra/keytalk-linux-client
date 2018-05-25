#ifdef _WIN32
#include "TaskSettings.h"
#include "TaskUtils.h"
#include "SettingsImpl.hpp"
#include "Settings.h"
#include "NativeCertStore.h"
#include "resept/util.h"
#include "ta/process.h"
#include "ta/logger.h"
#include "ta/common.h"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/format.hpp"
#include <fstream>
#include <string>

using std::string;
using rclient::Settings::TaskNames;
using libconfig::Setting;
using boost::str;
using boost::format;
using rclient::Settings::TaskSettingsError;
using namespace rclient::Settings::SettingsImpl;

namespace rclient
{
    namespace Settings
    {
        // Used for test purposes only!
        static string ReseptTaskSettingsPath = "";

        static const string TaskName = "Name";

        static const string IISUpdateSSLCertificateTaskList = "IISUpdateSSLCertificate";

        static const string TaskLookupUserError = "Internal error while looking up the task in the configuration.";

        namespace SettingsImpl
        {
            namespace
            {
                /**
                @brief Wrapper class for task configuration file.

                Lazily (upon getPtr) Creates an empty config file.
                */
                class TaskConfig
                {
                public:
                    TaskConfig()
                    {
                        theUserCount++;
                    }

                    ~TaskConfig()
                    {
                        theUserCount--;
                        if (theConfigPtr)
                        {
                            if (theUserCount == 0)
                            {
                                SettingsImpl::save(*theConfigPtr, getTaskConfigPath());
                                delete theConfigPtr;
                                theConfigPtr = NULL;
                            }
                        }
                    }

                    libconfig::Config* getPtr()
                    {
                        if (!ta::isFileExist(getTaskConfigPath()))
                        {
                            create_file(getTaskConfigPath());
                        }

                        if (!theConfigPtr)
                        {
                            theConfigPtr = SettingsImpl::load(getTaskConfigPath()).release();
                        }

                        return theConfigPtr;
                    }
                private:
                    static libconfig::Config* theConfigPtr;
                    static unsigned int theUserCount;
                    void create_file(const string aFilePath)
                    {
                        std::ofstream os(aFilePath);
                        os << "ConfigVersion = \"1.0\";" << endl;
                        os << IISUpdateSSLCertificateTaskList << " = ();" << endl;
                        os.close();
                    }
                };
                libconfig::Config* TaskConfig::theConfigPtr = NULL; // Initialized lazily by getPtr method
                unsigned int TaskConfig::theUserCount = 0;


                void getTaskListPath(const TaskType aTaskType, string& aPath)
                {
                    string myTasksPath;
                    if (aTaskType == IISTask)
                    {
                        myTasksPath = IISUpdateSSLCertificateTaskList;
                    }
                    else
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      TaskLookupUserError,
                                      format("Unexpected task type '%s'") % toStr(aTaskType));
                    }

                    aPath = myTasksPath;
                }


                unsigned int getTaskListSize(const TaskType aTaskType)
                {
                    string myTasksPath;
                    getTaskListPath(aTaskType, myTasksPath);

                    TaskConfig taskConfig;
                    const libconfig::Config* conf = taskConfig.getPtr();
                    return getListSize(*conf, myTasksPath);
                }


                void getTaskPath(const TaskType aTaskType, unsigned int anIndex, string& aPath)
                {
                    if (anIndex >= getTaskListSize(aTaskType))
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      TaskLookupUserError,
                                      format("Cannot request task index '%d' of task type '%s'. Index out of bounds.") % anIndex % toStr(aTaskType));
                    }

                    string myTasksPath;
                    getTaskListPath(aTaskType, myTasksPath);
                    const string myTaskPath = str(format("%s.[%s]") % myTasksPath % anIndex);

                    aPath = myTaskPath;
                }

                /*
                @return true if task parameter value can be retrieved from configuration, false otherwise
                */
                template <typename T>
                void getTaskParameter(const TaskType aTaskType, unsigned int aTaskIndex, const string& aParameterName, T& aParameterValue)
                {
                    TaskConfig taskConfig;
                    const libconfig::Config* conf = taskConfig.getPtr();

                    std::string myTaskParameterPath;
                    if (!getTaskParameterPath(aTaskType, aTaskIndex, aParameterName, myTaskParameterPath))
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      format("Cannot find task parameter '%s'.") % aParameterName,
                                      format("Cannot find task parameter '%s' (at '%s')") % aParameterName % myTaskParameterPath);
                    }

                    T myValue;
                    if (conf->lookupValue(myTaskParameterPath, myValue))
                    {
                        aParameterValue = myValue;
                    }
                    else
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      format("Invalid value for task parameter '%s'.") % aParameterName,
                                      format("Invalid value for task parameter '%s' (at '%s')") % aParameterName % myTaskParameterPath);
                    }
                }


                /*
                @return true if a task with the specified name has been found, false otherwise
                */
                bool getTaskIndex(const TaskType aTaskType, const string& aTaskName, unsigned int& anIndex)
                {
                    try
                    {
                        string myTasksPath;
                        getTaskListPath(aTaskType, myTasksPath);
                        for (unsigned int i = 0; i < getTaskListSize(aTaskType); ++i)
                        {
                            string myTaskName;
                            getTaskParameter<string>(aTaskType, i, TaskName, myTaskName);
                            if (myTaskName == aTaskName)
                            {
                                anIndex = i;
                                return true;
                            }
                        }
                        return false;
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      TaskLookupUserError,
                                      boost::format("Failed to get task index for task '%s' of type '%s'. %s") % aTaskName % toStr(aTaskType) % e.what());
                    }
                }


                /*
                @return true if a task with the specified name has been found, false otherwise
                */
                bool getTaskPath(const TaskType aTaskType, const string& aTaskName, string& aPath)
                {
                    try
                    {
                        unsigned int myTaskIndex;
                        if (!getTaskIndex(aTaskType, aTaskName, myTaskIndex))
                        {
                            return false;
                        }

                        getTaskPath(aTaskType, myTaskIndex, aPath);
                        return true;
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      TaskLookupUserError,
                                      boost::format("Failed to get task path for task '%s' of type '%s'. %s") % aTaskName % toStr(aTaskType) % e.what());
                    }
                }


                /*
                @return true if the task parameter exists, false otherwise
                */
                bool getTaskParameterPath(const TaskType aTaskType, const string& aTaskName, const string& aParameterName, string& aPath)
                {
                    string myTaskPath;
                    if (!getTaskPath(aTaskType, aTaskName, myTaskPath))
                    {
                        TA_THROW_MSG2(TaskSettingsError,
                                      TaskLookupUserError,
                                      boost::format("Failed to get task path for task '%s' of type '%s' while retrieving task parameter.") % aTaskName % toStr(aTaskType));
                    }

                    aPath = myTaskPath + "." + aParameterName;

                    TaskConfig taskConfig;
                    const libconfig::Config* conf = taskConfig.getPtr();
                    return conf->exists(aPath);
                }


                /*
                @return true if the task parameter exists, false otherwise
                */
                bool getTaskParameterPath(const TaskType aTaskType, unsigned int anIndex, const string& aParameterName, string& aPath)
                {
                    string myTaskPath;
                    getTaskPath(aTaskType, anIndex, myTaskPath);

                    aPath = myTaskPath + "." + aParameterName;
                    TaskConfig taskConfig;
                    const libconfig::Config* conf = taskConfig.getPtr();
                    return conf->exists(aPath);
                }


                template <typename T>
                void getTaskParameter(const TaskType aTaskType, const std::string& aTaskName, const string& aParameterName, T& aParameterValue)
                {
                    unsigned int myTaskIndex;
                    if (!getTaskIndex(aTaskType, aTaskName, myTaskIndex))
                    {
                        const string myErrorMsg(str(format("Task '%s' of type '%s' does not exist.") % aTaskName % toStr(aTaskType)));
                        TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                    }

                    return getTaskParameter<T>(aTaskType, myTaskIndex, aParameterName, aParameterValue);
                }


                template <typename T>
                bool getTaskParameterWithDefault(const TaskType aTaskType, const std::string& aTaskName, const string& aParameterName, const T& aDefaultValue, T& aParameterValue)
                {
                    try
                    {
                        getTaskParameter(aTaskType, aTaskName, aParameterName, aParameterValue);
                        return true;
                    }
                    catch (TaskSettingsError&)
                    {
                        aParameterValue = aDefaultValue;
                        return false;
                    }
                }


                template <typename T>
                void setTaskParameter(const TaskType aTaskType, const string& aTaskName, const string& aParameterName, const T& aParameterValue)
                {
                    TaskConfig taskConfig;
                    const libconfig::Config* conf = taskConfig.getPtr();

                    std::string myTaskParameterPath;
                    getTaskParameterPath(aTaskType, aTaskName, aParameterName, myTaskParameterPath);

                    try
                    {
                        conf->lookup(myTaskParameterPath) = aParameterValue;
                    }
                    catch (libconfig::SettingNotFoundException&)
                    {
                        std::string myTaskPath;
                        getTaskPath(aTaskType, aTaskName, myTaskPath);
                        libconfig::Setting& task = conf->lookup(myTaskPath);

                        string myTypeName;
                        task.add(aParameterName, getLibconfigType(aParameterValue, myTypeName)) = aParameterValue;
                    }
                }
            } // namespace
        } // namespace SettingsImpl

        void resetTaskConfigPath()
        {
            ReseptTaskSettingsPath = "";
        }


        void setTaskConfigPath(const string& aPath)
        {
            ReseptTaskSettingsPath = aPath;
        }

        bool isScheduledTaskFeatureInstalled()
        {
            return boost::filesystem::exists(getScriptsDirectoryPath());
        }

        string getTaskConfigPath()
        {
            if (!ReseptTaskSettingsPath.empty())
            {
                return ReseptTaskSettingsPath;
            }
            return getReseptConfigDir() + ta::getDirSep() + TaskConfigFileName;
        }


        bool isValidTaskName(const std::string& aTaskName,  std::string& anErrorMsg)
        {
            if (boost::trim_copy(aTaskName).empty())
            {
                anErrorMsg = "Task name cannot be empty";
                return false;
            }
            static const std::locale& loc = std::locale::classic();
            foreach (char ch, aTaskName)
            {
                if (!std::isalnum(ch, loc) && ch != '_' && ch != '-')
                {
                    anErrorMsg = "Task may only contain alphanumeric characters, '_' or '-'";
                    return false;
                }
            }
            return true;
        }

        void addTask(const TaskType aTaskType, const string& aTaskName)
        {
            if (boost::trim_copy(aTaskName) == "")
            {
                const string myErrorMsg("Cannot add task with empty name");
                TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
            }

            std::string myTaskName = boost::trim_copy(aTaskName);

            string myTasksPath;
            if (aTaskType == IISTask)
            {
                myTasksPath = IISUpdateSSLCertificateTaskList;
            }
            else
            {
                const string myErrorMsg(str(format("Unexpected task type '%s'") % toStr(aTaskType)));
                TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
            }

            if (isTaskExists(aTaskType, myTaskName))
            {
                const string myErrorMsg(str(format("Cannot add task '%s' of type '%s'. Task already exists.") % myTaskName % toStr(aTaskType)));
                TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
            }

            TaskConfig taskConfig;
            const libconfig::Config* conf = taskConfig.getPtr();
            libconfig::Setting& myTask = conf->lookup(myTasksPath).add(libconfig::Setting::TypeGroup);
            myTask.add(TaskName, Setting::TypeString) = myTaskName;
        }


        void removeTask(const TaskType aTaskType, const string& aTaskName)
        {
            if (!isTaskExists(aTaskType, aTaskName))
            {
                const string myErrorMsg(str(format("Cannot remove nonexisting task '%s' of type '%s'.") % aTaskName % toStr(aTaskType)));
                TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
            }

            unsigned int myTaskIndex;
            if (!getTaskIndex(aTaskType, aTaskName, myTaskIndex))
            {
                const string myErrorMsg(str(format("Cannot add task '%s' of type '%s'. Task already exists.") % aTaskName % toStr(aTaskType)));
                TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
            }

            std::string myTasksPath;
            getTaskListPath(aTaskType, myTasksPath);

            TaskConfig taskConfig;
            const libconfig::Config* conf = taskConfig.getPtr();
            conf->lookup(myTasksPath).remove(myTaskIndex);
        }


        bool isTaskExists(const TaskType aTaskType, const string& aTaskName)
        {
            if (!ta::isFileExist(getTaskConfigPath()))
            {
                return false;
            }

            std::string myTaskPath;
            return getTaskPath(aTaskType, aTaskName, myTaskPath);
        }

        TaskNames getTaskNames(const TaskType aTaskType)
        {
            TaskNames result;

            string myTasksPath;
            getTaskListPath(aTaskType, myTasksPath);
            for (unsigned int i = 0; i < getTaskListSize(aTaskType); ++i)
            {
                std::string myTaskName;
                getTaskParameter<string>(aTaskType, i, TaskName, myTaskName);
                result.push_back(myTaskName);
            }
            return result;
        }


        bool isAllTasksValid()
        {
            foreach (const string& taskName, getTaskNames(IISTask))
            {
                if (!IISTaskParameters::isValidIISTask(taskName))
                {
                    return false;
                }
            }
            return true;
        }

        bool isGenericTaskParametersValid(const std::string& aTaskName)
        {
            bool result = true;
            try
            {
                string myError;
                if (!isValidTaskName(aTaskName, myError))
                {
                    ERRORLOG(myError);
                    result = false;
                }
                getTaskEnabled(aTaskName);
            }
            catch (...)
            {
                ERRORLOG("Error while validating generic task parameters.");
                result = false;
            }
            return result;
        }

        bool getTaskEnabled(const string& aTaskName)
        {
            bool myValue;
            const bool myDefaultValue = true;
            getTaskParameterWithDefault<bool>(IISTask, aTaskName, TaskEnabled, myDefaultValue, myValue);
            return myValue;
        }

        void setTaskEnabled(const string& aTaskName, const bool& aValue)
        {
            setTaskParameter<bool>(IISTask, aTaskName, TaskEnabled, aValue);
        }

        namespace IISTaskParameters
        {
            bool isValidIISTask(const std::string& aTaskName)
            {
                try
                {
                    INFOLOG(format("Validating task '%s'.") % aTaskName);
                    TaskConfig taskConfig;
                    // For performance reasons: keep the config file open during the many task config operations
                    taskConfig.getPtr();

                    bool result = true;
                    std::string myError;

                    if (!isGenericTaskParametersValid(aTaskName))
                    {
                        ERRORLOG("Error in generic task parameters for task " + aTaskName);
                        result = false;
                    }

                    if (!rclient::Settings::IISTaskParameters::isValidScriptLogFilePath(getScriptLogFilePath(aTaskName), myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }

                    if (!rclient::Settings::IISTaskParameters::isValidHttpsBindingIp(getHttpsBindingIp(aTaskName), myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }

                    if (!NetUtils::isValidPort(getHttpsBindingPort(aTaskName)))
                    {
                        ERRORLOG("IIS Binding port invalid.");
                        result = false;
                    }

                    const string myKeyTalkProvider = getKeyTalkProvider(aTaskName);
                    if (!resept::isValidProviderName(myKeyTalkProvider, myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }
                    if (!ta::isElemExist(myKeyTalkProvider, rclient::Settings::getProviders()))
                    {
                        ERRORLOG("Provider " + myKeyTalkProvider + "does not exist.");
                        result = false;
                    }

                    const string myServiceName = getKeyTalkService(aTaskName);
                    if (!resept::isValidServiceName(myServiceName, myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }
                    if (!ta::isElemExist(myServiceName, rclient::Settings::getServices(myKeyTalkProvider)))
                    {
                        ERRORLOG("Service " + myServiceName + " does not exist for provider " + myKeyTalkProvider);
                        result = false;
                    }

                    if (!resept::isValidUserName(getKeyTalkUser(aTaskName), myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }

                    if (!resept::isValidPassword(getKeyTalkPassword(aTaskName), myError))
                    {
                        ERRORLOG(myError);
                        result = false;
                    }

                    if (!rclient::NativeCertStore::isStoreExists(getCertificateStore(aTaskName)))
                    {
                        ERRORLOG("Certificate store does not exist.");
                        result = false;
                    }

                    getShouldRemoveOldCertificate(aTaskName);

                    const bool myEmailReporting = getEmailReporting(aTaskName);

                    if (myEmailReporting)
                    {
                        getSendEmailOnSuccess(aTaskName);

                        getSendEmailIfApplyNotRequired(aTaskName);

                        if (!ta::isValidEmail(getEmailFrom(aTaskName)))
                        {
                            ERRORLOG("From email invalid.");
                            result = false;
                        }

                        if (!ta::isValidEmail(getEmailTo(aTaskName)))
                        {
                            ERRORLOG("To email invalid.");
                            result = false;
                        }

                        ta::NetUtils::DomainNameValidationResult myDNResult;
                        if (!ta::NetUtils::isValidDomainName(getSmtpServer(aTaskName), myDNResult, NetUtils::dnsName))
                        {
                            ERRORLOG("Smtp server invalid.");
                            result = false;
                        }

                        getEmailSubject(aTaskName);
                    }
                    return result;
                }
                catch (std::exception& e)
                {
                    ERRORLOG2(format("Error while validating definition of task '%s'.") % aTaskName, e.what());
                }
                catch (...)
                {
                    ERRORLOG(format("Error while validating definition of task '%s'.") % aTaskName);
                }
                return false;
            }

            void getDefaultScriptLogFileName(const std::string& aTaskName, std::string& aValue)
            {
                aValue =  DefaultScriptLogFilePrefix + aTaskName + ".log";
            }

            void getFallbackScriptLogFilePath(const std::string& aTaskName, std::string& aValue)
            {
                std::string defaultScriptFileName;
                getDefaultScriptLogFileName(aTaskName, defaultScriptFileName);
                aValue = ta::Process::getTempDir() + defaultScriptFileName;
            }

            string getScriptLogFilePath(const string& aTaskName)
            {
                string myValue;
                string myDefaultLogFileName;
                getDefaultScriptLogFileName(aTaskName, myDefaultLogFileName);
                const string myDefaultValue(ta::Process::getTempDir() + myDefaultLogFileName);
                getTaskParameterWithDefault<string>(IISTask, aTaskName, ScriptLogFilePath, myDefaultValue, myValue);
                return myValue;
            }

            bool isValidScriptLogFilePath(const std::string& aValue, std::string& anErrorMsg)
            {
                boost::filesystem::path myPath(boost::trim_copy(aValue));
                if (boost::trim_copy(aValue) == "")
                {
                    anErrorMsg = "Log file path may not be empty.";
                    return false;
                }

                if (!myPath.is_complete())
                {
                    anErrorMsg = "Log file path should be an absolute path.";
                    return false;
                }

                if (!myPath.has_parent_path())
                {
                    anErrorMsg = "Log file path has no parent directory.";
                    return false;
                }

                if (!myPath.has_filename())
                {
                    anErrorMsg = "Log file path has no filename.";
                    return false;
                }

                if (boost::filesystem::exists(myPath) && boost::filesystem::is_directory(myPath))
                {
                    anErrorMsg = "Log file path may not be a directory.";
                    return false;
                }

                if (!boost::filesystem::exists(myPath.parent_path()))
                {
                    anErrorMsg = str(format("Log file parent directory '%s' does not exist.") % myPath.parent_path());
                    return false;
                }
                return true;
            }

            void setScriptLogFilePath(const string& aTaskName, const string& aValue)
            {
                string myErrorMsg;
                if (!isValidScriptLogFilePath(aValue, myErrorMsg))
                {
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }

                setTaskParameter<string>(IISTask, aTaskName, ScriptLogFilePath, boost::trim_copy(aValue));
            }

            string getEmailFrom(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, EmailFrom, myValue);
                return myValue;
            }

            void setEmailFrom(const string& aTaskName, const string& aValue)
            {
                if (!ta::isValidEmail(aValue))
                {
                    const string myErrorMsg(str(format("'%s' is not a valid 'From' e-mail address") % aValue));
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, EmailFrom, boost::trim_copy(aValue));
            }


            string getEmailTo(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, EmailTo, myValue);
                return myValue;
            }

            void setEmailTo(const string& aTaskName, const string& aValue)
            {
                if (!ta::isValidEmail(aValue))
                {
                    const string myErrorMsg(str(format("'%s' is not a valid 'To' e-mail address") % aValue));
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, EmailTo, boost::trim_copy(aValue));
            }


            string getSmtpServer(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, SmtpServer, myValue);
                return myValue;
            }

            void setSmtpServer(const string& aTaskName, const string& aValue)
            {
                NetUtils::DomainNameValidationResult myDNResult;
                if (!NetUtils::isValidDomainName(aValue, myDNResult, NetUtils::dnsName))
                {
                    const string myErrorMsg(str(format("'%s' is not a valid SMTP server name. Valid DNS name required.") % aValue));
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, SmtpServer, boost::trim_copy(aValue));
            }


            string getEmailSubject(const string& aTaskName)
            {
                string myValue;
                const string myDefaultValue(str(format("%s IIS certificate update") % resept::ProductName));
                getTaskParameterWithDefault<string>(IISTask, aTaskName, EmailSubject, myDefaultValue, myValue);
                return myValue;
            }

            void setEmailSubject(const string& aTaskName, const string& aValue)
            {
                setTaskParameter<string>(IISTask, aTaskName, EmailSubject, boost::trim_copy(aValue));
            }


            bool getEmailReporting(const string& aTaskName)
            {
                bool myValue;
                const bool myDefaultValue = false;
                getTaskParameterWithDefault<bool>(IISTask, aTaskName, EmailReporting, myDefaultValue, myValue);
                return myValue;
            }

            void setEmailReporting(const string& aTaskName, const bool& aValue)
            {
                setTaskParameter<bool>(IISTask, aTaskName, EmailReporting, aValue);
            }


            bool getSendEmailOnSuccess(const string& aTaskName)
            {
                bool myValue;
                const bool myDefaultValue = true;
                getTaskParameterWithDefault<bool>(IISTask, aTaskName, SendEmailOnSuccess, myDefaultValue, myValue);
                return myValue;
            }

            void setSendEmailOnSuccess(const string& aTaskName, const bool& aValue)
            {
                setTaskParameter<bool>(IISTask, aTaskName, SendEmailOnSuccess, aValue);
            }


            bool getSendEmailIfApplyNotRequired(const string& aTaskName)
            {
                bool myValue;
                const bool myDefaultValue = false;
                getTaskParameterWithDefault<bool>(IISTask, aTaskName, SendEmailIfApplyNotRequired, myDefaultValue, myValue);
                return myValue;
            }

            void setSendEmailIfApplyNotRequired(const string& aTaskName, const bool& aValue)
            {
                setTaskParameter<bool>(IISTask, aTaskName, SendEmailIfApplyNotRequired, aValue);
            }


            string getHttpsBindingIp(const string& aTaskName)
            {
                string myValue;
                const string myDefaultValue("0.0.0.0");
                getTaskParameterWithDefault<string>(IISTask, aTaskName, HttpsBindingIp, myDefaultValue, myValue);
                return myValue;
            }

            bool isValidHttpsBindingIp(const std::string& aValue, std::string& anErrorMsg)
            {
                string myValue = boost::trim_copy(boost::trim_copy(aValue));

                if (myValue == "")
                {
                    anErrorMsg = "IIS HTTPS binding IP may not be empty.";
                    return false;
                }

                if (myValue == "*" ||
                        myValue == "0.0.0.0" ||
                        NetUtils::isValidIpv4(myValue) ||
                        NetUtils::isValidIpv6(myValue) )
                {
                    return true;
                }

                anErrorMsg = "IIS HTTPS binding IP should be '*', '0.0.0.0', valid IPv4/IPv6 Address";
                return false;
            }

            void setHttpsBindingIp(const string& aTaskName, const string& aValue)
            {
                if (boost::trim_copy(aValue) == "*")
                {
                    setTaskParameter<string>(IISTask, aTaskName, HttpsBindingIp, "0.0.0.0");
                    return;
                }

                string myErrorMsg;
                if (!isValidHttpsBindingIp(aValue, myErrorMsg))
                {
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }

                // Strip the brackets (in case of IPv6), to accomodate the "IIS:\" virtual filesystem in powershell.
                string myStrippedValue = boost::trim_copy(aValue);
                if (NetUtils::isValidIpv6(myStrippedValue))
                {
                    boost::erase_all(myStrippedValue, "[");
                    boost::erase_all(myStrippedValue, "]");
                }

                setTaskParameter<string>(IISTask, aTaskName, HttpsBindingIp, myStrippedValue);
            }


            unsigned int getHttpsBindingPort(const string& aTaskName)
            {
                unsigned int myValue;
                const int myDefaultValue = 443;
                getTaskParameterWithDefault<unsigned int>(IISTask, aTaskName, HttpsBindingPort, myDefaultValue, myValue);
                return myValue;
            }

            void setHttpsBindingPort(const string& aTaskName, const unsigned int& aValue)
            {
                if (!NetUtils::isValidPort(aValue))
                {
                    const string myErrorMsg(str(format("'%d' is not a valid IIS HTTPS binding port.") % aValue));
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }
                setTaskParameter<int>(IISTask, aTaskName, HttpsBindingPort, aValue);
            }


            string getKeyTalkProvider(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, KeyTalkProvider, myValue);
                return myValue;
            }

            void setKeyTalkProvider(const string& aTaskName, const string& aValue)
            {
                string myErrorMsg;
                if (!resept::isValidProviderName(aValue, myErrorMsg))
                {
                    const string myExceptionMsg(str(format("'%s' is not a valid %s provider name. %s") % aValue % resept::ProductName % myErrorMsg));
                    TA_THROW_MSG2(TaskSettingsError, myExceptionMsg, myExceptionMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, KeyTalkProvider, boost::trim_copy(aValue));
            }


            string getKeyTalkService(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, KeyTalkService, myValue);
                return myValue;
            }

            void setKeyTalkService(const string& aTaskName, const string& aValue)
            {
                string myErrorMsg;
                if (!resept::isValidServiceName(aValue, myErrorMsg))
                {
                    const string myExceptionMsg(str(format("'%s' is not a valid %s service name. %s") % aValue % resept::ProductName % myErrorMsg));
                    TA_THROW_MSG2(TaskSettingsError, myExceptionMsg, myExceptionMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, KeyTalkService, boost::trim_copy(aValue));
            }


            string getKeyTalkUser(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, KeyTalkUser, myValue);
                return myValue;
            }

            void setKeyTalkUser(const string& aTaskName, const string& aValue)
            {
                string myErrorMsg;
                if (!resept::isValidUserName(aValue, myErrorMsg))
                {
                    const string myExceptionMsg(str(format("Invalid %s user name. %s") % resept::ProductName % myErrorMsg));
                    TA_THROW_MSG2(TaskSettingsError, myExceptionMsg, myExceptionMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, KeyTalkUser, boost::trim_copy(aValue));
            }


            string getKeyTalkPassword(const string& aTaskName)
            {
                string myValue;
                getTaskParameter<string>(IISTask, aTaskName, KeyTalkPassword, myValue);
                return myValue;
            }

            void setKeyTalkPassword(const string& aTaskName, const string& aValue)
            {
                string myErrorMsg;
                if (!resept::isValidPassword(aValue, myErrorMsg))
                {
                    const string myExceptionMsg(str(format("Invalid %s password. %s") % resept::ProductName % myErrorMsg));
                    TA_THROW_MSG2(TaskSettingsError, myExceptionMsg, myExceptionMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, KeyTalkPassword, aValue);
            }


            string getCertificateStore(const string& aTaskName)
            {
                string myValue;
                const string myDefaultValue = "My";
                getTaskParameterWithDefault<string>(IISTask, aTaskName, CertificateStore, myDefaultValue, myValue);
                return myValue;
            }

            void setCertificateStore(const string& aTaskName, const string& aValue)
            {
                std::vector<std::string> myStorenames = NativeCertStore::getStoreNames();
                if (!NativeCertStore::isStoreExists(aValue))
                {
                    const string myErrorMsg(str(format("Certificate store '%s' does not exist.") % aValue));
                    TA_THROW_MSG2(TaskSettingsError, myErrorMsg, myErrorMsg);
                }
                setTaskParameter<string>(IISTask, aTaskName, CertificateStore, boost::trim_copy(aValue));
            }


            bool getShouldRemoveOldCertificate(const string& aTaskName)
            {
                bool myValue;
                const bool myDefaultValue = true;
                getTaskParameterWithDefault<bool>(IISTask, aTaskName, ShouldRemoveOldCertificate, myDefaultValue, myValue);
                return myValue;
            }

            void setShouldRemoveOldCertificate(const string& aTaskName, const bool& aValue)
            {
                setTaskParameter<bool>(IISTask, aTaskName, ShouldRemoveOldCertificate, aValue);
            }
        } // namespace IISTaskParameters

    } // namespace Settings
} // namespace rclient
#endif // _WIN32