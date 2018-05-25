#ifndef RCLIENT_TASKSETTINGS_H
#define RCLIENT_TASKSETTINGS_H

#ifdef _WIN32

#include "ta/common.h"
#include <vector>
#include <string>

namespace rclient
{
    namespace Settings
    {
        class TaskSettingsError : public std::runtime_error
        {
        public:
            explicit TaskSettingsError(const std::string& aMessage) :
                std::runtime_error(aMessage), theFriendlyMessage(aMessage)
            {}

            TaskSettingsError(const std::string& aFriendlyMessage, const std::string& aDeveloperMessage) :
                std::runtime_error(aDeveloperMessage), theFriendlyMessage(aFriendlyMessage)
            {}

            ~TaskSettingsError() throw() {}

            std::string friendlyMessage() const
            {
                return theFriendlyMessage;
            }
        private:
            std::string theFriendlyMessage;
        };

        typedef std::vector<std::string> TaskNames;

        static const std::string TaskConfigFileName = "tasks.ini";
        void setTaskConfigPath(const std::string& aPath);
        void resetTaskConfigPath();
        std::string getTaskConfigPath();

        class ScopedTaskConfiguration
        {
        public:
            ScopedTaskConfiguration(const std::string& aTaskConfigurationFile)
            {
                setTaskConfigPath(aTaskConfigurationFile);
            }

            ~ScopedTaskConfiguration()
            {
                resetTaskConfigPath();
            }
        };

        bool isScheduledTaskFeatureInstalled();

        enum TaskType
        {
            _firstTaskType,
            IISTask = _firstTaskType,
            _lastTaskType = IISTask
        };
        static const char* TaskTypeString[] = {"IIS HTTPS Binding Certificate Update Task"};
        BOOST_STATIC_ASSERT((sizeof(TaskTypeString) / sizeof(TaskTypeString[0])) == (_lastTaskType - _firstTaskType + 1));
        inline std::string toStr(TaskType aTaskType)
        {
            if (aTaskType > _lastTaskType || aTaskType < _firstTaskType)
                throw std::invalid_argument(str(boost::format("Cannot retrieve string value for non-existing task type %d.") % aTaskType));
            return TaskTypeString[aTaskType - _firstTaskType];
        }

        TaskNames getTaskNames(TaskType aTaskType);

        bool isAllTasksValid();

        bool isValidTaskName(const std::string& aTaskName,  std::string& anErrorMsg);
        void addTask(TaskType aTaskType, const std::string& aTaskName);
        void removeTask(TaskType aTaskType, const std::string& aTaskName);
        bool isTaskExists(TaskType aTaskType, const std::string& aTaskName);

        // Generic task parameters
        const std::string TaskEnabled = "Enabled";

        bool getTaskEnabled(const std::string& aTaskName);
        void setTaskEnabled(const std::string& aTaskName, const bool& aValue);

        const std::string DefaultScriptLogFilePrefix = "keytalk_task_";
        namespace IISTaskParameters
        {
            const std::string ScriptLogFilePath           = "ScriptLogFilePath";
            const std::string EmailFrom                   = "EmailFrom";
            const std::string EmailTo                     = "EmailTo";
            const std::string SmtpServer                  = "SmtpServer";
            const std::string EmailSubject                = "EmailSubject";
            const std::string EmailReporting              = "EmailReporting";
            const std::string SendEmailOnSuccess          = "SendEmailOnSuccess";
            const std::string SendEmailIfApplyNotRequired = "SendEmailIfApplyNotRequired";
            const std::string HttpsBindingIp              = "HttpsBindingIp";
            const std::string HttpsBindingPort            = "HttpsBindingPort";
            const std::string KeyTalkProvider             = "KeyTalkProvider";
            const std::string KeyTalkService              = "KeyTalkService";
            const std::string KeyTalkUser                 = "KeyTalkUser";
            const std::string KeyTalkPassword             = "KeyTalkPassword";
            const std::string CertificateStore            = "CertificateStore";
            const std::string ShouldRemoveOldCertificate  = "ShouldRemoveOldCertificate"; // Setting defaults to true in production. Only used for debugging purposes.

            bool isValidIISTask(const std::string& aTaskName);

            void getDefaultScriptLogFileName(const std::string& aTaskName, std::string& aValue); // does not depend on configuration
            void getFallbackScriptLogFilePath(const std::string& aTaskName, std::string& aValue); // does not depend on configuration
            std::string getScriptLogFilePath(const std::string& aTaskName);
            bool isValidScriptLogFilePath(const std::string& aValue, std::string& anErrorMsg);
            void setScriptLogFilePath(const std::string& aTaskName, const std::string& aValue);

            std::string getEmailFrom(const std::string& aTaskName);
            void setEmailFrom(const std::string& aTaskName, const std::string& aValue);

            std::string getEmailTo(const std::string& aTaskName);
            void setEmailTo(const std::string& aTaskName, const std::string& aValue);

            std::string getSmtpServer(const std::string& aTaskName);
            void setSmtpServer(const std::string& aTaskName, const std::string& aValue);

            std::string getEmailSubject(const std::string& aTaskName);
            void setEmailSubject(const std::string& aTaskName, const std::string& aValue);

            bool getEmailReporting(const std::string& aTaskName);
            void setEmailReporting(const std::string& aTaskName, const bool& aValue);

            bool getSendEmailOnSuccess(const std::string& aTaskName);
            void setSendEmailOnSuccess(const std::string& aTaskName, const bool& aValue);

            bool getSendEmailIfApplyNotRequired(const std::string& aTaskName);
            void setSendEmailIfApplyNotRequired(const std::string& aTaskName, const bool& aValue);

            std::string getHttpsBindingIp(const std::string& aTaskName);
            bool isValidHttpsBindingIp(const std::string& aValue, std::string& anErrorMsg);
            void setHttpsBindingIp(const std::string& aTaskName, const std::string& aValue);

            unsigned int getHttpsBindingPort(const std::string& aTaskName);
            void setHttpsBindingPort(const std::string& aTaskName, const unsigned int& aValue);

            std::string getKeyTalkProvider(const std::string& aTaskName);
            void setKeyTalkProvider(const std::string& aTaskName, const std::string& aValue);

            std::string getKeyTalkService(const std::string& aTaskName);
            void setKeyTalkService(const std::string& aTaskName, const std::string& aValue);

            std::string getKeyTalkUser(const std::string& aTaskName);
            void setKeyTalkUser(const std::string& aTaskName, const std::string& aValue);

            std::string getKeyTalkPassword(const std::string& aTaskName);
            void setKeyTalkPassword(const std::string& aTaskName, const std::string& aValue);

            std::string getCertificateStore(const std::string& aTaskName);
            void setCertificateStore(const std::string& aTaskName, const std::string& aValue);

            bool getShouldRemoveOldCertificate(const std::string& aTaskName);
            void setShouldRemoveOldCertificate(const std::string& aTaskName, const bool& aValue);
        } // namespace IISTaskParameters
    }
}

#endif // _WIN32

#endif //RCLIENT_TASKSETTINGS_H
