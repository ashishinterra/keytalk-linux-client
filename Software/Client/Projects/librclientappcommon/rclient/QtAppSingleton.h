//----------------------------------------------------------------------------
//
//  Name          QtAppSingleton.h
//  Description : QtAppSingleton class declaration
//
//                QtAppSingleton initializes Qt library by creating a process-singleton QApplication instance and initializing RClientAppCommon resources.
//                A typical usage of this class is to initialize Qt library in single-threaded applications
//                or in applications which perform Qt GUI operations only in the same thread the library Qt was initialized (singleton was created)
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_QTAPPSINGLETON_H
#define RCLIENT_QTAPPSINGLETON_H

#include "ta/common.h"
#include "ta/singletonholder.hpp"

class QApplication;

namespace rclient
{
    class QtAppSingleton: public ta::SingletonHolder<QtAppSingleton>
    {
        friend class ta::SingletonHolder<QtAppSingleton>;
        friend class ta::DefaultCreationPolicy<QtAppSingleton>;
    private:
        QtAppSingleton();
        ~QtAppSingleton();
    private:
        QApplication* theQtAppPtr;
    };
}

#endif