#include "AuthDelayedMessageBox.h"
#include <QtWidgets>
#include "resept/common.h"
#include "ta/assert.h"
#include "ta/common.h"

namespace AuthDelayedMessageBox
{
    class QAuthDelayedMessageBox: public QMessageBox
    {
    public:
        QAuthDelayedMessageBox(QWidget* aParent, size_t aDelay)
            : QMessageBox(aParent)
        {
            QTimer* timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(update()));
            timer->start(1000);

            theAuthRetryAllowedTime = time(NULL) + aDelay;

            setWindowTitle("Unable to authenticate");
            setIcon(Warning);
            setText("Invalid user credentials. Please wait and press 'Retry' to try again or press 'Cancel' to cancel authentication");
            setStandardButtons(QMessageBox::Retry|QMessageBox::Cancel);
            setDefaultButton(QMessageBox::Retry);
            retryButtonEnabled = true;
        }

        QAuthDelayedMessageBox(QWidget* aParent, std::string msgText, bool retryButton, size_t aDelay)
            : QMessageBox(aParent)
        {
            QTimer* timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(update()));
            timer->start(1000);

            theAuthRetryAllowedTime = time(NULL) + aDelay;

            setWindowTitle("Unable to authenticate");
            setIcon(Warning);
            setText(msgText.c_str());
            retryButtonEnabled = retryButton;
            if (retryButton)
            {
                setStandardButtons(QMessageBox::Retry|QMessageBox::Cancel);
                setDefaultButton(QMessageBox::Retry);
            }
            else
            {
                setStandardButtons(QMessageBox::Cancel);
                setDefaultButton(QMessageBox::Cancel);
            }
        }
    protected:
        virtual void paintEvent(QPaintEvent*)
        {
            QPushButton* myRetryBtn = defaultButton();
            TA_ASSERT(myRetryBtn);

            int mySecRemain = (int)theAuthRetryAllowedTime - time(NULL);
            if (mySecRemain <= 0)
            {
                myRetryBtn->setEnabled(true);
                if (retryButtonEnabled)
                {
                    myRetryBtn->setText("Retry");
                }
                else
                {
                    myRetryBtn->setText("Cancel");
                }
            }
            else
            {
                myRetryBtn->setEnabled(false);
                if (retryButtonEnabled)
                {
                    myRetryBtn->setText(str(boost::format("Retry (wait %d seconds)") % mySecRemain).c_str());
                }
                else
                {
                    myRetryBtn->setText(str(boost::format("wait %d seconds") % mySecRemain).c_str());
                }
            }

            QMessageBox::update();
        }
    private:
        time_t theAuthRetryAllowedTime;
        bool retryButtonEnabled;
    };

    class QAuthFailedMessageBox: public QMessageBox
    {
    public:
        QAuthFailedMessageBox(QWidget* aParent)
            : QMessageBox(aParent)
        {
            setWindowTitle("Unable to authenticate");
            setIcon(Warning);
            setText("Invalid user credentials. Please wait and press 'Retry' to try again or press 'Cancel' to cancel authentication");
            setStandardButtons(QMessageBox::Retry|QMessageBox::Cancel);
            setDefaultButton(QMessageBox::Retry);
        }
    };

    bool show(QWidget* aParent, size_t aDelay)
    {
        if (aDelay > 0)
            return QAuthDelayedMessageBox(aParent, aDelay).exec() == QMessageBox::Retry;
        return QAuthFailedMessageBox(aParent).exec() == QMessageBox::Retry;
    }

    bool show(QWidget* aParent, std::string msgText, bool retryButton, size_t aDelay)
    {
        if (aDelay > 0)
            return QAuthDelayedMessageBox(aParent, msgText, retryButton, aDelay).exec() == QMessageBox::Retry;
        return QAuthFailedMessageBox(aParent).exec() == QMessageBox::Retry;
    }
}
