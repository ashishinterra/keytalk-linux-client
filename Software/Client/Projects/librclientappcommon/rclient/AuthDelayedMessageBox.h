#ifndef RCLIENT_AUTHDELAYEDMESSAGEBOX_H
#define RCLIENT_AUTHDELAYEDMESSAGEBOX_H

#include <string>

class QWidget;


namespace AuthDelayedMessageBox
{
    //@return whether another authentication attempt is requested
    bool show (QWidget* aParent, size_t aDelay);

    //@return whether another authentication attempt is requested
    bool show (QWidget* aParent, std::string msgText, bool retryButton, size_t aDelay);

}

#endif
