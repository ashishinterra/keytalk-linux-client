#include "WaitDialog.h"
#include "resept/common.h"

#include <QCursor>
#include "boost/format.hpp"
#include <qdialog.h>
#include <qlabel.h>
#include <qboxlayout.h>
#include <qdesktopwidget.h>
#include <qapplication.h>

const std::string myTextTempl =
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
    "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
    "p, li { white-space: pre-wrap; }"
    "</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">"
    "<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt; font-weight:600;\">%s </span></p>"
    "<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">%s</span></p></body></html>";

WaitDialog::WaitDialog(const std::string& aText,  QWidget* parent, bool aCenterOnDesktop)
    : theParent(parent)
    , theCenterOnDesktop(aCenterOnDesktop)
    , theDialog(new QDialog(parent))
    , theText(new QLabel(theDialog))
{
    // Set to modal to disallow events to reach the parent otherwise the user may be allowed e.g. to press
    // "Next" or "Connect" button of the RESEPT Wizard before the previous handler of this button completes.
    theDialog->setWindowTitle("WaitDialog");
    theDialog->setModal(true);
    theDialog->setWindowFlags(theDialog->windowFlags() | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    theDialog->setAttribute(Qt::WA_ShowWithoutActivating);

    theText->setWordWrap(true);
    theText->setTextFormat(Qt::RichText);
    theText->setAlignment(Qt::AlignCenter | Qt::AlignHCenter | Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);

    QVBoxLayout* layout = new QVBoxLayout(theDialog);
    layout->addWidget(theText);
    theDialog->setLayout(layout);

    setText(str(boost::format(myTextTempl) % resept::ProductName % aText));

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (theCenterOnDesktop)
        centerOnDesktop();

    theDialog->show();
}

WaitDialog::~WaitDialog()
{
    delete theDialog;
    QApplication::restoreOverrideCursor();
}

void WaitDialog::setText(const std::string& aText)
{
    theText->setText(aText.c_str());
}

void WaitDialog::centerOnDesktop()
{
    QDesktopWidget* myDefDesktop = QApplication::desktop();
    QRect myScreenRect = myDefDesktop->screenGeometry();
    QPoint myCenterPos = myScreenRect.center();
    QRect myThisRect = theDialog->frameGeometry();
    myCenterPos.setX( myCenterPos.x() - myThisRect.width()/2 );
    myCenterPos.setY( myCenterPos.y() - myThisRect.height()/2 );
    theDialog->move(myCenterPos);
}
