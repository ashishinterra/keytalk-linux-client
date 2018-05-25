#include "ReseptResponseCalculator.h"
#include "resept/common.h"
#include "resept/util.h"
#include "ta/strings.h"
#include "ta/common.h"

#include "boost/assign/list_of.hpp"
#include <string>

using std::string;

ResponseCalculator::ResponseCalculator(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    ui.useridLineEdit->setFocus(Qt::ActiveWindowFocusReason);
    setUiValue(ui.otpInitSecretLineEdit, resept::DefOtpInitialSecret);
    setUiValue(ui.otpPinLineEdit, resept::DefOtpPincode);
    setWindowTitle(" Response/OTP Calculator");
}

ResponseCalculator::~ResponseCalculator()
{}

void ResponseCalculator::on_calcButton_clicked()
{
    try
    {
        const string myUserId = getUiValue(ui.useridLineEdit);
        const string myChallenge = getUiValue(ui.challengeLineEdit);

        const string myResponse = resept::calcResponse(myUserId, myChallenge);

        setUiValue(ui.responseLineEdit, myResponse);
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(this, "Error calculating response", e.what());
    }
}

void ResponseCalculator::on_gsmCalcButton_clicked()
{
    try
    {
        const string myUserId = getUiValue(ui.gsmUserIdLlineEdit);
        const string myRandomChallenge = getUiValue(ui.gsmRandomLineEdit);

        const ta::StringDict myChallenges = boost::assign::map_list_of(resept::GsmRandomChallengeName, myRandomChallenge);
        std::vector<std::string> myResponseNames = boost::assign::list_of("SRES")("Kc");

        const ta::StringDict myResponses = resept::calcGsmResponses(myUserId, myChallenges, myResponseNames);

        setUiValue(ui.gsmSresLineEdit, myResponses.find("SRES")->second);
        setUiValue(ui.gsmKcLineEdit, myResponses.find("Kc")->second);
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(this, "Error calculating response", e.what());
    }
}


void ResponseCalculator::on_umtsCalcButton_clicked()
{
    try
    {
        const string myUserId = getUiValue(ui.umtsUserIdLlineEdit);
        const string myRandomChallenge = getUiValue(ui.umtsRandomLineEdit);
        const string myAutnChallenge = getUiValue(ui.umtsAutnLineEdit);

        const ta::StringDict myChallenges = boost::assign::map_list_of(resept::UmtsRandomChallengeName, myRandomChallenge)
                                            (resept::UmtsAutnChallengeName, myAutnChallenge);
        const std::vector<std::string> myResponseNames = boost::assign::list_of("RES")("IK")("CK");

        const ta::StringDict myResponses = resept::calcUmtsResponses(myUserId, myChallenges, myResponseNames);

        setUiValue(ui.umtsResLineEdit, myResponses.find("RES")->second);
        setUiValue(ui.umtsIkLineEdit, myResponses.find("IK")->second);
        setUiValue(ui.umtsCkLineEdit, myResponses.find("CK")->second);
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(this, "Error calculating response", e.what());
    }
}

void ResponseCalculator::on_otpCalcButton_clicked()
{
    try
    {
        const string myInitSecret = getUiValue(ui.otpInitSecretLineEdit);
        const string myPin = getUiValue(ui.otpPinLineEdit);

        const string myResponse = resept::calcOtp(myInitSecret, myPin);

        setUiValue(ui.otpPasswdLineEdit, myResponse);
    }
    catch (std::exception& e)
    {
        QMessageBox::warning(this, "Error calculating response", e.what());
    }
}

string ResponseCalculator::getUiValue(const QLineEdit* aCtrl)
{
    QByteArray myBytes = aCtrl->text().toUtf8();
    const string myVal(myBytes.data(), myBytes.size());
    return myVal;
}

void ResponseCalculator::setUiValue(QLineEdit* aCtrl, const string& aVal)
{
    aCtrl->setText(QString::fromUtf8(aVal.c_str()));
}




