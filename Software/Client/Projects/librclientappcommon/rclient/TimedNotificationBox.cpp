#include "TimedNotificationBox.h"
#include <QtWidgets>
#include "resept/common.h"
#include "ta/assert.h"
#include "ta/common.h"

#include <QString>
#include <QApplication>

namespace TimedNotificationBox
{
    class QTimedNotificationBox: public QMessageBox
    {
    public:
        QTimedNotificationBox(QWidget* aParent, const size_t aDelaySec, const QString& aTitle, const QString& aMessage): QMessageBox(aParent)
        {
            QTimer* timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(update()));
            timer->start(1000);

            theTime = time(NULL) + aDelaySec;

            setWindowTitle(aTitle);
            setIcon(Information);
            setText(aMessage);

            // optional to allow the user to quickly close the dialog
            setStandardButtons(QMessageBox::Ok);
            setDefaultButton(QMessageBox::Ok);
        }
    protected:
        virtual void paintEvent(QPaintEvent*)
        {
            QPushButton* myOkBtn = defaultButton();
            TA_ASSERT(myOkBtn);

            int mySecRemain = (int)theTime - time(NULL);
            if (mySecRemain <= 0)
                this->done(QMessageBox::Ok);
            else
                myOkBtn->setText(str(boost::format("OK (closes in %d seconds)") % mySecRemain).c_str());
            QMessageBox::update();
        }
    private:
        time_t theTime;
    };

    bool show(QWidget* aParent, const size_t aDelaySec, const QString& aTitle, const QString& aMessage)
    {
        if (aDelaySec > 0)
        {
            QApplication::setOverrideCursor(Qt::ArrowCursor);
            const bool result = QTimedNotificationBox(aParent, aDelaySec, aTitle, aMessage).exec() == QMessageBox::Ok;
            QApplication::restoreOverrideCursor();
            return result;
        }
        return false;
    }
}
