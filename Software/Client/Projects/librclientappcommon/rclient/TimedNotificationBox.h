#pragma once

#include <QString>

class QWidget;

namespace TimedNotificationBox
{
    bool show (QWidget* aParent, const size_t aDelaySec, const QString& aTitle, const QString& aMessage);
}
