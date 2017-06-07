#include "arqcatcher.h"
#include "arqpitcher.h"
#include "ui_arqcatcher.h"
#include "ui_arqpitcher.h"
#include "linkup.h"
#include "logger.h"
#include "ui_linkup.h"
#include "linkuputils.h"
#include "XmlRpcException.h"
#include "XmlRpcClient.h"
#include "XmlRpcValue.h"
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QStringBuilder>
#include <QTextStream>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QRegularExpression>
#include <QCheckBox>
#include <QWidget>
#include <QList>
#include <QProcess>

// xml rpc client objuect
XmlRpc::XmlRpcClient *xc;
// xml rpc result object
XmlRpc::XmlRpcValue rv;
QString DIR_PATH;
//QTimer portTimer;
// file object to pass around
QFile *loadedFile;
QTimer centerTimer;
QTimer *beaconTimer;
QTimer * rxTimer;
QProcess *fldigiProcess;
LINKUp::LINKUp(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LINKUp)
{

    ui->setupUi(this);
    // set the base path
    DIR_PATH = QApplication::applicationDirPath() + "/";
    //qDebug() << "DIR_PATH = " << DIR_PATH;

    // load the settings or inform the user that we cannot
    // and use defaults
    if (!loadSettings()) {
        QMessageBox::information(this, "UNABLE TO LOAD SETTINGS","Unable to load settings.\nLINKUp will use defaults.");
    }


    // connect to the XMLRPC host in the FLDIGI instance
    xc = new XmlRpc::XmlRpcClient(settings->value(tr("tcp_address"), "127.0.0.1").toByteArray().data(), settings->value(tr("tcp_port"), "7362").toInt());
    if(xc)
        reportModemConnected();
    loadModems();
    // trap the up arrow for retrieving prev order wire
    ui->orderWireLineEdit->installEventFilter(this);

    connect(&centerTimer, &QTimer::timeout, this, &LINKUp::on_centerTimer);
    connect(&centerTimer, &QTimer::timeout, this, &LINKUp::updateClock);
    centerTimer.start(4000);

    // QTimer for handling receive monitoring
    rxTimer = new QTimer(this);
    connect(rxTimer, &QTimer::timeout, this, &LINKUp::processIncomingData);
    rxTimer->start(250);

    // if beacon > 0 start the beacon timer and connect the
    beaconTimer = new QTimer(this);
    connect(beaconTimer, &QTimer::timeout, this, &LINKUp::on_beaconTimer);
    int interval = settings->value(tr("beaconInterval"), "0").toInt();
    if ( interval > 0) {
        beaconTimer->start(interval * 60000);
        QTimer::singleShot(5000, this, SLOT(on_beaconTimer));
        //QTimer::singleShot(5000, this, &LINKUp::on_beaconTimer);
    }
    QString id(settings->value(tr("mycall"), "MYCALL").toString());
    if (id == "MYCALL" || id == "") {
        ui->tabbedWidget->setCurrentIndex(2); // the Config tab
        qApp->processEvents();
        QMessageBox::information(this, "SET CALL SIGN","Please set the call sign on the CONF tab, then Save and RESTART LINKUp.");
    } else {
        log.setStationID(id);
        log.createLogFile();
        log.logEntry("Opened log file: ");
        // do the initial setu of FLDIGI controls
        QTimer::singleShot(1500, this, SLOT(setupFLDIGI));
        //QTimer::singleShot(1500, this, &LINKUp::setupFLDIGI);
    }
    QWidget::setWindowTitle(tr("LINKUp ver. ") + version);

    // A place to test out some of the XMLRPC functions
//    try {
//        XmlRpc::XmlRpcValue testrv;
//        bool b_res = xc->execute("modem.get_names", 0, testrv, 1);
//        if (b_res) {
//            qDebug() << "positive result";
//        }
//        XmlRpc::XmlRpcValue::ValueArray ans = testrv;
//        qDebug() << "created vector";
//        for (uint i = 0; i < ans.size(); i++) {
//            XmlRpc::XmlRpcValue val = ans.at(i);
//            ui->receiveTextArea->appendPlainText(QString::fromStdString(val));
//        }
//    }
//    catch (const XmlRpc::XmlRpcException &id) {
//        qDebug() << "handle exception: " << QString::fromStdString(id.getMessage());
//    }
}

LINKUp::~LINKUp()
{
    on_cfgSaveButton_clicked();
    if (xc)
        delete xc;
    if (fldigiProcess)
    {
        fldigiProcess->close();
        delete fldigiProcess;
    }
    delete ui;
}

void LINKUp::on_beaconTimer() {
    // skip this beacon if we are currently
    // receiving
    if (b_xmitInhibit)
        return;
    // otherwise also make sure we aren't
    // transmitting already!
    xc->execute("main.get_trx_status", 0, rv, 1);
    std::string ans = rv;
    if (ans != "rx")
        return;
    //qDebug() << "Beaconing...";
    if (b_useZULUTime)
        sendText(tr("http://linkup.sf.net ") + QDateTime::currentDateTimeUtc().toString(tr("HH:mm"))+ "Z", tr("ALL"));
    else
        sendText(tr("http://linkup.sf.net ") + QDateTime::currentDateTime().toString(tr("HH:mm")), tr("ALL"));
}

void LINKUp::updateClock()
{
    ui->lcdClock->display(QDateTime::currentDateTimeUtc().toString("HH:mm"));
}

void LINKUp::on_centerTimer()
{
    xc->execute("modem.get_carrier", 0, rv, 1);
    int i_center = rv;
    QString center = QString::number(i_center, 10);
    //qDebug() << "center : " << center;
    statusBar()->showMessage(center);
    xc->execute("modem.get_name", 0, rv, 1);
    QString modemName = QString::fromStdString(rv);
    //qDebug() << "fldigi modem: " << modemName << "   curr: " << currModem;
    if (ui->modemComboBox->count() > 0 && currModem != modemName) {
        ui->modemComboBox->setCurrentText(modemName);
        currModem = modemName;
    }
}

void LINKUp::reportModemConnected() {
    //qDebug() << "modem connected";
    ui->receiveTextArea->appendHtml(tr("<p style=color:red;><pre>Modem Connected...</pre></p>"));
}

bool LINKUp::loadSettings() {
    //ui->hexDumpCheckbox->setChecked(settings->value(tr("hexDump"), "false").toString() == "true");
    //b_hexDump = ui->hexDumpCheckbox->isChecked();
    ui->CONFXMLRPCTCPPort->setText(settings->value(tr("tcp_port"),"7362").toString());
    ui->CONFXMLRPCIP->setText(settings->value(tr("tcp_address"),"127.0.0.1").toString());
    ui->CONFCallSign->setText(settings->value(tr("mycall"),"MYCALL").toString());
    mycall = ui->CONFCallSign->text();
    ui->CONFName->setText(settings->value(tr("tactical"),"").toString());
    ui->CONFGrid->setText(settings->value(tr("mygrid"),"").toString());
    ui->beaconIntervalLineEdit->setText(settings->value(tr("beaconInterval"), "0").toString());
    ui->useZULUCheckbox->setChecked(settings->value(tr("useZULUTime")).toBool());
    ui->autoStartFLDIGICheckBox->setChecked(settings->value(tr("startFLDIGI"), "false").toBool());
    ui->fldigiPathLineEdit->setText(settings->value(tr("fldigiPath"), "").toString());
    ui->fldigiCmdsLineEdit->setText(settings->value(tr("fldigiCmdLine"), "").toString());
    ui->fldigiLockCheckBox->setChecked(settings->value(tr("fldigiLock"), "false").toBool());
    ui->fldigiCenterLineEdit->setText(settings->value(tr("fldigiCenter"), "1500").toString());
    ui->fldigiUsePSMCheckBox->setChecked(settings->value(tr("fldigiUsePSM"), "false").toBool());
    ui->fldigiDefModemLineEdit->setText(settings->value(tr("fldigiDefModem"), "").toString());
    ui->modemComboBox->setCurrentText(ui->fldigiDefModemLineEdit->text());
    this->restoreGeometry(settings->value("geometry").toByteArray());
    this->restoreState(settings->value("windowState").toByteArray());
    if (settings->value(tr("startFLDIGI"), "false").toString() == "true")
        autoStartFLDIGI(settings->value(tr("fldigiPath"), "fldigi").toString(), settings->value(tr("fldigiCmdLine"), "").toString());
    return true;
}

void LINKUp::loadModems()
{
    // Load the list of modems from the settings string list
    // modem checkboxes on the Modems tab are also checked if used.
    // This function should also be called whenever the user saves
    // a new list of modems from the Modems tab.
    QString currModem = ui->modemComboBox->currentText();
    ui->modemComboBox->clear();
    qApp->processEvents();
    QList<QVariant> sl = settings->value(tr("modems")).toList();
    for (int i = 0; i < sl.length(); i++) {
            //qDebug() << sl.at(i).toString();
            QCheckBox *cb = ui->modemsTab->findChild<QCheckBox *>(sl.at(i).toString());
            cb->setChecked(true);
            ui->modemComboBox->addItem(cb->text());
    }
    if (ui->modemComboBox->count() > 0)
        ui->modemComboBox->model()->sort(0);
    ui->modemComboBox->setCurrentText(currModem);
}


void LINKUp::on_cfgSaveButton_clicked()
{
    // Cancel changes to config parameters, so
    // re-read stored parameters or drop to defaults
    settings->setValue(tr("mycall"), ui->CONFCallSign->text().trimmed().toUpper());
    settings->setValue(tr("tactical"), ui->CONFName->text().trimmed().toUpper());
    settings->setValue(tr("tcp_address"), ui->CONFXMLRPCIP->text());
    settings->setValue(tr("tcp_port"), ui->CONFXMLRPCTCPPort->text());
    settings->setValue(tr("mygrid"), ui->CONFGrid->text());
    settings->setValue(tr("beaconInterval"), ui->beaconIntervalLineEdit->text());
    int beaconInterval = settings->value(tr("beaconInterval"), "0").toInt();
    if(!b_closingDown)
    {
        if (beaconTimer) {
            if (beaconInterval < 1) {
                beaconTimer->stop();
            } else {
                beaconTimer->setInterval(beaconInterval * 60000);
                beaconTimer->start();
                on_beaconTimer();
            }
        } else {
            beaconTimer = new QTimer(this);
            connect(beaconTimer, &QTimer::timeout, this, &LINKUp::on_beaconTimer);
            if (beaconInterval > 0) {
                beaconTimer->start(beaconInterval * 60000);
                on_beaconTimer();
            }
        }
    }
    settings->setValue(tr("useZULUTime"), b_useZULUTime);
    settings->setValue(tr("fldigiPath"), ui->fldigiPathLineEdit->text().trimmed());
    settings->setValue(tr("startFLDIGI"), ui->autoStartFLDIGICheckBox->isChecked());
    settings->setValue(tr("fldigiCmdLine"), ui->fldigiCmdsLineEdit->text().trimmed());
    settings->setValue(tr("fldigiLock"), ui->fldigiLockCheckBox->isChecked());
    settings->setValue(tr("fldigiCenter"), ui->fldigiCenterLineEdit->text().trimmed());
    settings->setValue(tr("fldigiUsePSM"), ui->fldigiUsePSMCheckBox->isChecked());
    if(!b_closingDown)
        setupFLDIGI();
    //settings->setValue(tr("hexDump"), ui->hexDumpCheckbox->isChecked());
    settings->setValue("geometry", saveGeometry());
    settings->setValue("windowState", saveState());
}

void LINKUp::setupFLDIGI()
{
    ui->modemComboBox->setCurrentText(ui->fldigiDefModemLineEdit->text());
    // ensure we are in FLDIGI ARQ mode
    xc->execute("io.enable_arq", 0, rv, 1);
    // use settings to set the FLDIGI controls
    if (settings->value(tr("fldigiLock"), "false").toBool())
    {
        xc->execute("modem.set_carrier", settings->value(tr("fldigiCenter"), "1500").toInt(), rv, 1);
        xc->execute("main.set_lock", true, rv, 1);
    }
    if (settings->value(tr("fldigiUsePSM"), "false").toBool())
        xc->execute("modem.set_carrier", settings->value(tr("fldigiCenter"), "1500").toInt(), rv, 1);
}

void LINKUp::on_modemComboBox_currentIndexChanged(const QString &arg1)
{
    // send XMLRPC to FLDIGI to change the modem in use
    xc->execute("modem.set_by_name",arg1.toStdString(), rv, 1);
    currModem = arg1;
    //qDebug() << QString::fromStdString(rv);
}


void LINKUp::on_EMCONCheckbox_toggled(bool checked)
{
    b_EMCON = checked;
    //qDebug() << b_EMCON;
}

void LINKUp::on_heardList_clicked(const QModelIndex &index)
{
    ui->destCallLineEdit->setText(ui->heardList->currentItem()->text());
    ui->orderWireLineEdit->setFocus();
}

void LINKUp::processIncomingData() {
    // thanks to Dave w1hkj for the suggestion to:
    xc->execute("main.get_trx_status", 0, rv, 1);
    std::string ans = rv;
    if (ans != "rx")
    {
        emit(modemTransmitting()); // tell the SMARQ process that we are transmitting
        return;
    }
    else
    {
        emit(modemReceiving()); // tell the SMARQ process that we are receiving
    }
    // get all data from FLDIGI since last poll
    // and check for frame markers.  If both
    // start and end are found, send bytes for
    // processing which clears the input buffer and flags.
    //qDebug() << "in processIncomingData function";
    xc->execute("rx.get_data", 0 , rv, 1);
    std::vector<unsigned char> inb = rv;
    foreach (unsigned char inbb, inb) {
        inbytes.append(inbb);
    }
    int soh = inbytes.indexOf(qbaSOH);
    int eot = inbytes.indexOf(qbaEOT);
    b_soh = soh > -1;
    b_eot = eot > -1;

    if (b_soh && b_eot) {
        //qDebug() << "INBYTES: " << inbytes;
        // reset the transmit inhibit flag
        b_xmitInhibit = false;
        processFrame(inbytes.mid(soh, eot - soh + 2));
        // we may have back to back frames, so only
        // clear out the part of the buffer that held
        // the first frame
        clearInputBuffer(eot + 2);

    } else if (b_soh && !b_eot) {
        if (!b_xmitInhibit) {
            // don't beacon if in the middle of an rx cycle
            // this is reset by the if clause above or in
            // the clearInputBuffer functions
            b_xmitInhibit = true;
            // shed the junk ahead of the SOH but only once
            inbytes = inbytes.mid(inbytes.indexOf(qbaSOH));
        }
        // let the GUI have a go and give
        // a little more time for bytes to
        // accumulate
        qApp->processEvents();
    } else if (!b_soh && b_eot) {
        // there's an end but no beginning or it's all garbage, so flush all
        clearInputBuffer(eot + 2);
    }
}

void LINKUp::processFrame(QByteArray inputBuffer) {
    // TODO: need to add handlers for SMARQ frames
    //qDebug()<<"processFrame:"<<inputBuffer;
    bool proto1 = false;
    bool proto2 = false;
    int index = -1;
    QString sourceCall;
    QString destCall;
    QString filename;
//    QByteArray PROTO1 = tr("~11").toLatin1();
//    QByteArray PROTO2 = tr("~12").toLatin1();
//    QByteArray SMARQ = tr("~1NW_").toLatin1();
//    QByteArray SPACE = tr(" ").toLatin1();
    // the incoming frame has already been trimmed of fat front and back
    if(inputBuffer.indexOf(SMARQ) > -1) {
            index = inputBuffer.indexOf(SMARQ);
            b_inSMARQSession = true;
    } else if (inputBuffer.lastIndexOf(PROTO1) > -1) {
        index = inputBuffer.indexOf(PROTO1);
        proto1 = true;
    } else if (inputBuffer.lastIndexOf(PROTO2) > -1) {
        index = inputBuffer.lastIndexOf(PROTO2);
        proto2 = true;
    }
    if(b_inSMARQSession)
    {
        if(index < 0) // no match from above so inside a SMARQ session
        {
            index = inputBuffer.indexOf("~1");
            int endex = inputBuffer.indexOf("~4", index) + 2;
            emit(smarqReply(inputBuffer.mid(index, endex - index))); // send the clean frame
        }
        else {
            // spin up a SMARQ catcher session
            int endex = inputBuffer.indexOf("~4", index) + 2;
            launchSMARQCatcher(inputBuffer.mid(index, endex - index));
        }
    }
    else if (proto1 || proto2) {
        // since the input buffer is trimmed to it's start
        // position this means that the index of the
        // start of header is 0
        // grab the call signs and checksum and filename if proto2
        // move the "cursor" to the dest call sign
        index = 3;
        int indexEnd = inputBuffer.indexOf(SPACE) - index;
        // special case, indexEnd is the length of the destCall
        destCall = inputBuffer.mid(index, indexEnd);
        // source call is immediately after dest call
        index = (index + indexEnd + 1);
        indexEnd = inputBuffer.indexOf(SPACE, index) - index;
        sourceCall = inputBuffer.mid(index, indexEnd).toUpper();
        // if we are dealing with a binary file, collect the filename also
        if (proto2) {
            index = (index + indexEnd + 1);
            indexEnd = inputBuffer.indexOf(SPACE, index) - index;
            filename = inputBuffer.mid(index, indexEnd);
        }
        // head of message text block
        index = (index + indexEnd + 1);
        // move indexEnd to the beginning of the 4 checksum chars
        indexEnd = inputBuffer.lastIndexOf(qbaEOT) - 4 - index;
        // this data is collected as a String because even if it
        // is a file send, it is Base64 encoded text
        QString msgText = inputBuffer.mid(index, indexEnd);
        // head of 4 byte checksum chars
        index = index + indexEnd;
        // 4 bytes of hex char that we'll wrap into an int
        int checksum = inputBuffer.mid(index, 4).toInt(0, 16);
        // generate a count to use for calculating checksum later
        // protocol + call + space + call + space + <optional filename + space>  + msgtext
        int count = 1 + destCall.length() + 1 + sourceCall.length() + 1 + msgText.length();
        if (proto2) {
            count += filename.length() + 1;
        }
        //qDebug() << "DECODED CALLS AND MSG TEXT";
        //qDebug() << "msgText: " << msgText;
        //qDebug() << "sourceCall: " << sourceCall;
        //qDebug() << "destCall: " << destCall;
        //if (filename.length() > 0)
            //qDebug() << "filename: " << filename;
        //qDebug() << "count: " << count;
        //qDebug() << "checksum: " << checksum;
        // process proto1 version messages
        QString sb;
        bool checksumPass = false;
        if (proto1) {
            sb.append("&lt;&lt; ");
            sb.append(destCall);
            sb.append(" DE ");
            sb.append(sourceCall);
            sb.append(": ");
            sb.append(msgText);
            //qDebug() << msgText;
            // hit the checksum process
            //index = inputBuffer.lastIndexOf(PROTO1) + 1;
            //qDebug() << inputBuffer.mid(2, inputBuffer.lastIndexOf(EOT) - 4 - 2);
            if (checksum == LINKUpUtils::checkSum(inputBuffer.mid(2, inputBuffer.lastIndexOf(qbaEOT) - 4 - 2))) {
                checksumPass = true;
                ui->destCallLineEdit->setText(sourceCall);
                // update the heard list with this call : function not yet written
                updateHeardList(sourceCall);
            }
            //qDebug() << "checksum: " << checksum << "  checksumPass: " << checksumPass;
            if (checksumPass && (destCall.toUpper() == mycall || destCall == "ALL")) {
                if (destCall == "ALL") {
                    ui->receiveTextArea->appendHtml("<p><pre>" + sb + "</pre></p>");
                }
                if (destCall.toUpper() == mycall) {
                    // That is where we will find any commands to evaluate and handle
                    QString keyword;
                    // the first 4 chars might be a command or a response
                    keyword = msgText.mid(0, 4);
                    if (keyword == "QRK:")
                    {
                        ui->receiveTextArea->appendHtml("<p><pre>&lt;&lt; " + destCall + " DE " + sourceCall + " QRK:</pre></p>");
                        qApp->processEvents();
                        //TODO: make call to FLDIGI for quality of last rx
                        xc->execute("modem.get_quality",0,rv,1);
                        double d_quality = rv;
                        QString quality("%1");
                        quality = quality.arg(d_quality, 1, 'f', 1, '0');
                        //qDebug() << tr("OUT QUALITY: %1").arg(d_quality, 1, 'f', 1, '0');
                        QThread::msleep(2500);
                        // TODO:  send reply : function not yet created
                        sendText("QRK=" + quality, sourceCall);
                    } else if(keyword == "QRK=")\
                    {
                        QString inQuality = msgText.mid(msgText.indexOf("QRK="));
                        //qDebug() << "IN QUALITY: " + inQuality + SPACE + myGrid);
                        ui->receiveTextArea->appendHtml("<p><pre>&lt;&lt; " + destCall + " DE " + sourceCall + " " + inQuality + "</pre></p>");
                    } else if (keyword == "QRU:")
                    {
                        ui->receiveTextArea->appendHtml("<p><pre>&lt;&lt; " + destCall + " DE " + sourceCall + " QRU?</pre></p>");
                        qApp->processEvents();
                        QByteArray msgs = handleQRU(sourceCall);
                        QThread::msleep(2000);
                        sendText("QTC:\r\n" + msgs, sourceCall);
                    } else if (keyword == "QRV:")
                    {
                        // TODO: not yet created handleQRV function
                        // send first message by source call sign in natural order by filename
                        ui->receiveTextArea->appendHtml("<p><pre>&lt;&lt; QRV from " + sourceCall + "</pre></p>");
                        qApp->processEvents();
                        QString fileName = handleQRV(sourceCall);
                        if (fileName != "") {
                            QThread::msleep(2000);
                            sendTextFile(DIR_PATH + "QRU/" +fileName, sourceCall);
                        }
                    } else if (keyword == "QSL:")
                    {
                        ui->receiveTextArea->appendHtml("<p><pre>" + sb + "</pre></p>");
                        qApp->processEvents();
                        // do the archive operations on a successfully transmitted message
                        QString fileName = handleQRVQSL(msgText.mid(5));
                        //qDebug() << fileName;
                        if (fileName != "") {
                            QThread::msleep(2000);
                            sendText(fileName, sourceCall);
                        }
                    }
                    else {
                        ui->receiveTextArea->appendHtml("<p><pre>&lt;&lt; " + destCall + " DE " + sourceCall + " " + msgText + "</pre></p>");
                    }
                }

            } else if (((destCall.toUpper() == mycall) || (destCall == "ALL")) && !checksumPass) {
                // ERR denotes checksum fail
                ui->receiveTextArea->appendHtml("<p><pre>ERR "  + sb + "</pre></p>");
            }

        }
        // handle proto2 type messages
        if (proto2) {
            // We think there is an file in the frame
            // which is contained in this input buffer.
            // we also have a filename in the static "filename" field
            // Now we check the checksum against the message
            // content and alert the user if it fails otherwise
            // process the incoming file, after Base64.Decoder, and ACK if SELCAL
            //index = inputBuffer.lastIndexOf(PROTO2) + 1;
            //qDebug() << inputBuffer.mid(index, inputBuffer.lastIndexOf(EOT) -2));
            if (checksum == LINKUpUtils::checkSum(inputBuffer.mid(2, inputBuffer.lastIndexOf(qbaEOT) - 4 - 2))) {
                checksumPass = true;
                updateHeardList(sourceCall);
            }
            if (checksumPass) {
                // If message directed to me or ALL
                if ((destCall.toUpper() == mycall) || (destCall.toUpper() == "ALL")) {
                    ui->receiveTextArea->appendHtml("<p><pre>Incoming File Xfer: " + filename + "</pre></p>");
                    qApp->processEvents();
                    // Process the messsage to a file
                    QByteArray decoded = QByteArray::fromBase64(msgText.toLatin1());
                    QByteArray uncompressed = qUncompress(decoded);
                    writeBinaryFile(filename, uncompressed);
                    // decode from Base64 and use the filename to write to disk
                    ui->receiveTextArea->appendHtml("<p><pre>" + filename + " written to received folder..." + "</pre></p>");
                    // If the message was directed to me, also QSL the message
                    if ((destCall.toUpper() == mycall)) {
                        QString response = "&gt;&gt; " + sourceCall + " DE " + mycall + " QSL: " + filename;
                        ui->receiveTextArea->appendHtml("<p><pre>" + response + "</pre></p>");

                        sendText(response.mid(response.indexOf("QSL:")), sourceCall);
                    }
                }
            } else {
                // Otherwise, if the checksum failed, inform the user
                ui->receiveTextArea->appendHtml("<p><pre>FRAME FAILED_CHECKSUM</pre></p>");
            }

        }
    }
}

QByteArray LINKUp::handleQRU(QString sourceCall) {
    QStringList fnf;
    fnf << sourceCall + "*";
    QDir* QRUDir = new QDir(DIR_PATH + "QRU");
    QRUDir->setNameFilters(fnf);
    QStringList fileList = QRUDir->entryList();
    QByteArray out;
    foreach(QString s, fileList)
    {
        out.append(s).append("\r\n");
    }
    delete QRUDir;
    return out;
}

QString LINKUp::handleQRV(QString sourceCall) {
    // return the first filename in the matchin
    // list of QRU staged files by call sign
    // at the front of the filename
    QStringList fnf;
    fnf << sourceCall + "*";
    QDir* QRUDir = new QDir(DIR_PATH + "QRU");
    QRUDir->setNameFilters(fnf);
    QStringList fileList = QRUDir->entryList();
    delete QRUDir;
    if(fileList.length() > 0)
        return fileList.at(0);
    else
        return "";
}

QString LINKUp::handleQRVQSL(QString fileName) {
    // move the file to the QRV folder
    QFile oldFile(DIR_PATH + tr("QRU/") + fileName);
    //qDebug() << "handleQRVQSL: " << oldFile.fileName();
    if (oldFile.rename(DIR_PATH + tr("QRV/") + fileName))
        return fileName + tr(" archived");
    else
        return "";
}

void LINKUp::updateHeardList(QString sourceCall) {
    if (ui->heardList->findItems(sourceCall, Qt::MatchFixedString).length() > 0)
            return;
    ui->heardList->insertItem(0, sourceCall);
    ui->heardList->sortItems();
    ui->destCallLineEdit->setText(sourceCall.trimmed().toUpper());
}

void LINKUp::writeHexDump() {
    // create a filename built with UNKNOWN_<dtg>.txt
    QDateTime stamp = QDateTime::currentDateTimeUtc();
    QString fileName = "HEXDUMP_" + stamp.toString("yyyyMMdd_HHmmss") + ".hex";
    QFile f(fileName);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(inbytes);
        f.flush();
        f.close();
    }
}


bool LINKUp::writeBinaryFile(QString fileName, QByteArray msgBytes) {
    // check for a "valid" passed in filename first and if none
    // create one
    if (fileName == "") {
        QDateTime stamp = QDateTime::currentDateTimeUtc();
        fileName = "UNKNOWN_" + stamp.toString("yyyyMMdd_HHmmss") + ".dat";
    }
    QFile file(DIR_PATH + tr("received/") + fileName);
    // if this base message file already exists, write to a timestamped
    // version of the same file name
    if (file.exists()) {
        QDateTime stamp = QDateTime::currentDateTimeUtc();
        // chop off the .txt
        int index = fileName.lastIndexOf('.');
        QString extension = fileName.mid(index);
        fileName = fileName.mid(0, index) + "_" + stamp.toString("yyyyMMdd_HHmmss") + extension;
        //qDebug() << fileName;
        file.setFileName(DIR_PATH + tr("RECEIVED/") + fileName);
    }

    // open the text file
    if (!file.open(QIODevice::WriteOnly))
        // error handled by returning false to caller
        return false;
    // write the bytes
    file.write(msgBytes);
    // close the text file
    file.close();
    // report success to caller
    return true;
}

void LINKUp::on_FileXferButton_clicked()
{
    // put the terminal into binary send mode which means a difference in how we
    // write bytes to the modem...mainly by how we put bytes into a
    // QByteArray directly from the file instead of through a QString
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    ui->destCallLineEdit->setText(ui->destCallLineEdit->text().trimmed().toUpper());
    updateHeardList();
    // kick a file dialog so the user may select the file
    // ending the file dialog should send the file immediately inside a
    // header which includes the original filename only (no path).
    const QString fileName = QFileDialog::getOpenFileName(this, "Open Binary File",".", "All Files (*)");
    loadedFile = new QFile(fileName);
    //qDebug() << "SBF: loadedFile : " << loadedFile->fileName();

    if (!loadedFile->open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "UNABLE TO OPEN FILE","Unable to open file or no file selected.");
        return;
    }
    // read the bytes in the file
    QByteArray fileBytes = loadedFile->readAll();

    //qDebug() << "file contents: " << fileBytes;
    // build header and send to FLDIGI
    QByteArray compressed = qCompress(fileBytes);
    //qDebug() << "compressed: " << compressed.toHex();
    QByteArray encoded = compressed.toBase64(QByteArray::Base64Encoding);
    loadedFile->close();
    //qDebug() << "FILEBYTES: " << fileBytes;
    QByteArray msgBytes = LINKUpUtils::buildHeader(encoded, ui->destCallLineEdit->text().trimmed().toUpper(), mycall, fileName.mid(fileName.lastIndexOf("/") + 1), 2);
    //qDebug() << "MSGBYTES: " << msgBytes;
    xc->execute("text.add_tx_queu",QString(msgBytes).toStdString(),rv,1);
    xc->execute("main.tx", 0, rv, 1);
    ui->receiveTextArea->appendHtml(tr("<p><pre>&gt;&gt; Send File TO: ") + ui->destCallLineEdit->text().trimmed().toUpper() + tr(" ") + fileName.mid(fileName.lastIndexOf("/") + 1));
    log.logEntry(Logger::FT_TX, "BINARY FILE SEND TO: " + ui->destCallLineEdit->text() + " FR: " + mycall + " : " + loadedFile->fileName());
    //moveFileToSent();
}

void LINKUp::moveFileToSent() {
    // rename the file into the SENT directory
    QString fileName = DIR_PATH + tr("sent/") + loadedFile->fileName().mid(loadedFile->fileName().lastIndexOf("/") + 1);
    //qDebug() << "new fileName : " << fileName;
    // if this file already exists, overwrite..
    if (QFile::exists(fileName)) {
        QFile::remove(fileName);
    }
    loadedFile->rename(fileName);
}



bool LINKUp::eventFilter(QObject* obj, QEvent *event)
{
    //qDebug() << "trapped key press";
    if (obj == ui->orderWireLineEdit)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Up)
            {
                //qDebug() << "lineEdit -> Qt::Key_Up";
                ui->orderWireLineEdit->setText(lastOrderWire);
                return true;
            }
        }
        return false;
    }
    return QMainWindow::eventFilter(obj, event);
}

void LINKUp::updateHeardList()
{
    QString call(ui->destCallLineEdit->text().trimmed().toUpper());
    if (ui->heardList->findItems(call,Qt::MatchFixedString).length() > 0)
        return;
    // otherwise add it to the list and sort the list
    ui->heardList->addItem(call);
    ui->heardList->sortItems();
    ui->destCallLineEdit->setText(ui->destCallLineEdit->text().trimmed().toUpper());
}

void LINKUp::sendText(QString toSend, QString destCall)
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON ENABLED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text() == "") {
        QMessageBox::information(this, "SELECT CALL SIGN","You must select a station from the heard list\nor select the SELCAL radio button and enter\none in the field below it.");
        return;
    }
    // format and send the order wire text to the modem
    ui->receiveTextArea->appendHtml(tr("<p style=color:black;><pre>>> ") + destCall.trimmed().toUpper() + " DE " + mycall + " " + toSend + tr("</pre></p>"));
    // build header and send to FLDIGI
    QByteArray msgBytes = LINKUpUtils::buildHeader(toSend.toLatin1(), destCall, mycall, "" , 1);
    //qDebug() << "MSGBYTES: " << msgBytes;
    xc->execute("text.add_tx_queu",QString(msgBytes).toStdString(),rv,1);
    xc->execute("main.tx", 0, rv, 1);
    // take this opportunity while transmitting
    // to clear the RX decks
    xc->execute("text.get_data", 0, rv, 1);


    log.logEntry(Logger::SEL_TX, " Sent plain text: TO: " + destCall + " FR: " + mycall + " -- " + toSend);
    if (!(ui->destCallLineEdit->text().trimmed().toUpper() == "ALL")) {
        LINKUp::updateHeardList();
    }
}

void LINKUp::sendTextFile(QString fileName, QString destCall)
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text() == "") {
        QMessageBox::information(this, "SELECT CALL SIGN","You must select a station from the heard list\nor select the SELCAL radio button and enter\none in the field below it.");
        return;
    }
    // upper case it
    ui->destCallLineEdit->setText(destCall.trimmed().toUpper());
    // grab the text in the supplied filename
    QFile fileToSend(fileName);
    if (!fileToSend.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "UNABLE TO OPEN FILE","Unable to open file or no file selected.");
        return;
    }
    QByteArray msgText = fileToSend.readAll();
    fileToSend.close();
    //qDebug() << "file contents: " << msgText;
    // build header and send to FLDIGI
    QByteArray compressed = qCompress(msgText);
    //qDebug() << "compressed: " << compressed.toHex();
    QByteArray encoded = compressed.toBase64(QByteArray::Base64Encoding);
    //qDebug() << "encoded: " << encoded;
    QByteArray msgBytes = LINKUpUtils::buildHeader(encoded, destCall, mycall, fileName.mid(fileName.lastIndexOf("/") + 1) , 2);
    //qDebug() << "MSGBYTES: " << msgBytes;
    // clear the input buffer
    xc->execute("text.get_data",0,rv,1);
    xc->execute("text.add_tx_queu",QString(msgBytes).toStdString(),rv,1);
    xc->execute("main.tx", 0, rv, 1);

    log.logEntry(Logger::SEL_TX, " Sent plain text: TO: " + destCall + " FR: " + mycall + " -- " + fileName);
    if (!(ui->destCallLineEdit->text().trimmed().toUpper() == "ALL")) {
        LINKUp::updateHeardList();
    }
}

void LINKUp::on_orderWireLineEdit_returnPressed()
{

    if (b_xmitInhibit) {
        QMessageBox::information(this, "CURRENTLY RECEIVING","LINKUp is currently receiving a message.\n\nWait for the message to finish and then send again.");
        return;
    }
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text() == "") {
        QMessageBox::information(this, "SELECT A CALL SIGN","You must select a station from the heard list\nor select the SELCAL radio button and enter\none in the field below it.");
        return;
    }

    // upper case it
    ui->destCallLineEdit->setText(ui->destCallLineEdit->text().trimmed().toUpper());

    // format and send the order wire text to the modem
    lastOrderWire = ui->orderWireLineEdit->text().toLatin1();
    ui->receiveTextArea->appendHtml(tr("<p style=color:black;><pre>>> TO: ") + ui->destCallLineEdit->text().trimmed().toUpper() + " " + lastOrderWire + tr("</pre></p>"));
    // build header and send to FLDIGI
    QByteArray msgBytes = LINKUpUtils::buildHeader(lastOrderWire.toLatin1(), ui->destCallLineEdit->text().trimmed().toUpper(), mycall, "" , 1);
    //qDebug() << "OW MSGBYTES: " << msgBytes;

    xc->execute("text.add_tx_queu",QString(msgBytes).toStdString(),rv,1);
    xc->execute("main.get_trx_status", 0, rv, 1);
    std::string ans = rv;
    if (ans == "rx") {
        // FLDIGI is receiving so transmit
        xc->execute("main.tx", 0, rv, 1);
    } else {
        // take the opportunity while xmitting to clear the input buffer
        xc->execute("text.get_data",0,rv,1);
    }
    log.logEntry(Logger::SEL_TX, " Sent plain text order wire: TO: " + ui->destCallLineEdit->text().trimmed() + " FR: " + mycall + " -- " + lastOrderWire);
    if (!(ui->destCallLineEdit->text().trimmed().toUpper() == "ALL")) {
        LINKUp::updateHeardList();
    }
    ui->orderWireLineEdit->clear();
}

void LINKUp::on_LINKUpButton_clicked()
{
    if ((ui->heardList->selectedItems().count() == 0 && ui->destCallLineEdit->text().trimmed() == "") || ui->destCallLineEdit->text().trimmed().toUpper() == tr("ALL")){
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    ui->receiveTextArea->appendHtml(tr("<p style=color:black;><pre>>> LINKUp TO: ") + ui->destCallLineEdit->text().trimmed().toUpper() + tr("</pre></p>"));
    // build header and send to FLDIGI
    QString fileName = "";
    QString msg("QRK:");
    QByteArray msgBytes = LINKUpUtils::buildHeader(msg.toLatin1(), ui->destCallLineEdit->text().trimmed().toUpper(), mycall, fileName , 1);
    //qDebug() << "MSGBYTES: " << msgBytes;
    // clear the input buffer
    //xc->execute("text.get_data",0, rv,1);
    // load the output buffer
    xc->execute("text.add_tx_queu",QString(msgBytes).toStdString(),rv,1);
    // transmit it
    xc->execute("main.tx", 0, rv, 1);

    log.logEntry(Logger::SEL_TX, " Sent LINKUp: TO: " + ui->destCallLineEdit->text().trimmed() + " FR: " + mycall);
    if (!(ui->destCallLineEdit->text().trimmed().toUpper() == "ALL")) {
        LINKUp::updateHeardList();
    }
}

void LINKUp::on_abortButton_clicked()
{
    xc->execute("main.abort",0,rv,1);
}

void LINKUp::on_resetButton_clicked()
{
    // clear FLDIGI rx buffer
    xc->execute("text.clear_rx", 0, rv, 1);
    // clear the FLDIGI xmlrpc data buffer
    xc->execute("text.get_data", 0 , rv, 1);
    // clear everything locally too
    LINKUp::clearInputBuffer(inbytes.length() - 1);
}



void LINKUp::clearInputBuffer(int before)
{
    b_soh = false;
    b_eot = false;
    b_xmitInhibit = false;
    inbytes = inbytes.mid(before);
}

void LINKUp::on_delCallButton_clicked()
{
    if(ui->heardList->selectedItems().count() == 0) {
        // must select a call to delete first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.");
        return;
    }
    QListWidgetItem *gonner = ui->heardList->takeItem(ui->heardList->currentRow());
    delete gonner;
    if(ui->heardList->selectedItems().count() == 0) {
        ui->destCallLineEdit->setText("ALL");
    }
    return;
}

void LINKUp::on_QRUButton_clicked()
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text().trimmed() == "" || ui->destCallLineEdit->text().trimmed().toUpper() == tr("ALL")){
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    QString dest = ui->destCallLineEdit->text().trimmed().toUpper();
    //ui->receiveTextArea->appendHtml(tr("<p style=color:black;><pre>>> Sending QRU TO: ") + dest + tr("</pre></p>"));
    // build header and send to FLDIGI
    sendText("QRU:", dest);
    log.logEntry(Logger::SEL_TX, " Sent QRU: TO: " + dest + " FR: " + mycall);
    LINKUp::updateHeardList();
}

void LINKUp::on_QRVButton_clicked()
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text().trimmed() == "" || ui->destCallLineEdit->text().trimmed().toUpper() == tr("ALL")){
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    QString dest = ui->destCallLineEdit->text().trimmed().toUpper();
    //ui->receiveTextArea->appendHtml(tr("<p style=color:black;><pre>&gt;&gt; Sending QRV TO: ") + dest + tr("</pre></p>"));
    // build header and send to FLDIGI
    sendText("QRV:", dest);

    log.logEntry(Logger::SEL_TX, " Sent QRV: TO: " + dest + " FR: " + mycall);
    LINKUp::updateHeardList();
}


void LINKUp::on_sPClearButton_clicked()
{
    ui->SPTextArea->clear();
    currentSPFileName = "";
}

void LINKUp::on_SPOpenFileButton_clicked()
{
    QString msgText = "";
    // open a file chooser to open a text file
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Text File"), ".", tr("Text Files (*.txt);;All Files (*)"));
    currentSPFileName = fileName;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
            return;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            msgText += (line + "\n");
        }
        file.close();
        ui->SPTextArea->setPlainText(msgText);
}

void LINKUp::on_QSLButton_clicked()
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if (ui->destCallLineEdit->text().trimmed() == "" || ui->destCallLineEdit->text().trimmed().toUpper() == tr("ALL")){
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    QString dest = ui->destCallLineEdit->text().trimmed().toUpper();
    // open a file chooser to open a text file
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Text File"), ".", tr("Text Files (*.txt);;All Files (*)"));
    //qDebug() << fileName;
    if (fileName == "") // the user cancelled
        return;
    sendText("QSL: " + fileName.mid(fileName.lastIndexOf("/") + 1), dest);
    log.logEntry(Logger::SEL_TX, " Sent Manual QSL for :" + fileName.mid(fileName.lastIndexOf("/") + 1) + " TO: " + dest + " FR: " + mycall);
    LINKUp::updateHeardList();
}

void LINKUp::on_logButton_clicked()
{
    log.logEntry(99, ui->receiveTextArea->toPlainText());
    ui->receiveTextArea->clear();
}

void LINKUp::on_SPSaveAsButton_clicked()
{
    // open a file chooser to open a text file
    const QString fileName = QFileDialog::getSaveFileName(this, tr("Save Text File As"), ".", tr("Text Files (*.txt);;All Files (*)"));
    QFile file(fileName);
    if (!file.open(QIODevice::ReadWrite | QFile::Truncate))
            return;
    QTextStream out(&file);
    out << ui->SPTextArea->toPlainText();
    file.close();
}

void LINKUp::on_SPSendAllButton_clicked()
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if(ui->destCallLineEdit->text().trimmed() == "") {
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    QString dest = ui->destCallLineEdit->text().trimmed().toUpper();
    QString toSend = ui->SPTextArea->toPlainText();
    sendText(toSend, dest);
    log.logEntry(Logger::SEL_TX, " TO: " + dest + " FR: " + mycall + " -- " + toSend);
    if (dest != "ALL")
        LINKUp::updateHeardList();
}

void LINKUp::on_SPSendSeletctedButton_clicked()
{
    if (b_EMCON) {
        QMessageBox::information(this, "EMCON MODE SELECTED","LINKUp is in EMCON mode.\n\nClear the EMCON check box to transmit.");
        return;
    }
    if(ui->destCallLineEdit->text().trimmed() == "") {
        // must select a call first, so yell at the user
        QMessageBox::information(this, "SELECT A CALL SIGN","Please select a call sign from the Heard List first.\n\nOr type a call sign into the box below the list.");
        return;
    }
    QString dest = ui->destCallLineEdit->text().trimmed().toUpper();
    QString toSend = ui->SPTextArea->textCursor().selectedText();
    sendText(toSend, dest);
    log.logEntry(Logger::SEL_TX, " TO: " + dest + " FR: " + mycall + " -- " + toSend);
    if (dest != "ALL")
        LINKUp::updateHeardList();
}

void LINKUp::on_SPSaveButton_clicked()
{
    QFile file;
    if (currentSPFileName == "") {
    // open a file chooser to open a text file
        const QString fileName = QFileDialog::getSaveFileName(this, tr("Save Text File As"), ".", tr("Text Files (*.txt);;All Files (*)"));
        file.setFileName(fileName);
    } else {
        file.setFileName(currentSPFileName);
    }

    if (!file.open(QIODevice::ReadWrite | QFile::Truncate))
            return;
    QTextStream out(&file);
    out << ui->SPTextArea->toPlainText();
    file.close();
}


void LINKUp::on_cfgCancelButton_clicked()
{
    loadSettings();
    loadModems();
}


void LINKUp::on_useZULUCheckbox_toggled(bool checked)
{
    b_useZULUTime = checked;

}

void LINKUp::on_SPFORMSButton_clicked()
{
    // open file dialog... needs to be something else.
    QString msgText;
    // open a file chooser to open a text file
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Text File"), "./forms", tr("Text Files (*.txt);;All Files (*)"));
    currentSPFileName = fileName;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
            return;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            msgText += (line + "\n");
        }
        file.close();
        ui->SPTextArea->setPlainText(msgText);
}

void LINKUp::on_modemsSelectAllCheckbox_clicked(bool checked)
{
    QList<QCheckBox *> l = ui->modemsTab->findChildren<QCheckBox *>();
    for (int i  = 0; i < l.length(); i++) {
        l.at(i)->setChecked(checked);
    }
}

void LINKUp::on_modemsSaveButton_clicked()
{
    QStringList sl;
    QList<QCheckBox *> l = ui->modemsTab->findChildren<QCheckBox *>(QRegExp("checkBox*"));
    for (int i  = 0; i < l.length(); i++) {
        if (l.at(i)->isChecked() )
        {
            sl.append(l.at(i)->objectName());
        }
    }
    //qDebug() << sl;
    QVariant v;
    //if (v.canConvert<QStringList>())
        //qDebug() << "can convert QStringList";
    settings->setValue(tr("modems"), sl);
    qApp->processEvents();
    loadModems();
}

void LINKUp::autoStartFLDIGI(QString fldigiPath, QString args) {

    fldigiProcess = new QProcess(this);
    //if the user closes the auto-started instance of FLDIGI, report this to the text area
    connect(fldigiProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
          [=](int exitCode, QProcess::ExitStatus exitStatus){ reportFLDIGIFinished(exitCode, exitStatus); });
    if(args != "")
    {
        QStringList arguments;
        arguments << args;
        fldigiProcess->start(fldigiPath, arguments);
    } else {
        fldigiProcess->start(fldigiPath);
    }
    if (!fldigiProcess->waitForStarted(3000))
        fldigiProcess->deleteLater();
}

void LINKUp::reportFLDIGIFinished(int code, int exitStatus)
{
    ui->receiveTextArea->appendHtml("<p><pre>FLDIGI has exited with exit code: " % QString::number(code) % "\nRestart LINKUp to recover.");
    if(xc)
        xc->close();
    rxTimer->stop();
    beaconTimer->stop();
    centerTimer.stop();

}

void LINKUp::on_findFLDIGIButton_clicked()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "Locate fldigi Executable",".", "All Files (*)");
    ui->fldigiPathLineEdit->setText("\""+ fileName + "\"");
}



void LINKUp::on_fldigiLockCheckBox_clicked(bool checked)
{

    xc->execute("main.set_lock", checked, rv, 1);

}

void LINKUp::on_fldigiUseCurrentModemButton_clicked()
{
    QString dm = ui->modemComboBox->currentText();
    ui->fldigiDefModemLineEdit->setText(dm);
    settings->setValue(tr("fldigiDefModem"), dm);
}

void LINKUp::on_smarqButton_clicked()
{
    b_inSMARQSession = true;
    ARQPitcher * p = new ARQPitcher(this);
    connect(p, &ARQPitcher::sendBytes, this, &LINKUp::sendRawBytesToModem);
    connect(this, &LINKUp::smarqReply, p, &ARQPitcher::processIncoming);
    connect(p, &ARQPitcher::sessionFinished, this, &LINKUp::on_sessionFinished);
    connect(this, &LINKUp::modemReceiving, p, &ARQPitcher::on_modemReceiving);
    connect(this, &LINKUp::modemTransmitting, p, &ARQPitcher::on_modemTransmitting);
    p->setVisible(true);
}

void LINKUp::launchSMARQCatcher(QByteArray data)
{
    b_inSMARQSession = true;
    ARQCatcher *c = new ARQCatcher(this);
    connect(c, &ARQCatcher::sendBytes, this, &LINKUp::sendRawBytesToModem);
    connect(this, &LINKUp::smarqReply, c, &ARQCatcher::processInputFrame);
    connect(c, &ARQCatcher::sessionFinished, this, &LINKUp::on_sessionFinished);
    connect(this, &LINKUp::modemReceiving, c, &ARQCatcher::on_modemReceiving);
    connect(this, &LINKUp::modemTransmitting, c, &ARQCatcher::on_modemTransmitting);
    c->setInitialData(data.mid(0, data.lastIndexOf("~4") + 2));
    c->setVisible(true);
}

void LINKUp::sendRawBytesToModem(QByteArray toSend)
{
    //qDebug()<<"sendRawBytesToModem reached";
    // slot for sending bytes from the ARQ pitcher/catcher GUI linked by singals
    // so we simply use the native sendBytesToModem process. Generic, so this can carry
    // from modem type to modem type
    sendBytesToModem(toSend);
}


void LINKUp::sendBytesToModem(QByteArray data)
{
    // This is for the SMARQ process to send it's pre-formatted bytes
    //qDebug() << "MSGBYTES: " << msgBytes;
    xc->execute("text.add_tx_queu",QString(data).toStdString(),rv,1);
    xc->execute("main.tx", 0, rv, 1);
    // take this opportunity while transmitting
    // to clear the RX decks
    xc->execute("text.get_data", 0, rv, 1);
}

void LINKUp::on_sessionFinished()
{
    b_inSMARQSession = false;
    ui->receiveTextArea->appendHtml("<p><pre>SMARQ Session Finished</pre></p>");
}

void LINKUp::closeEvent(QCloseEvent *event)
{

    b_closingDown = true;
    //qDebug()<<"closing down"<<b_closingDown;
    on_cfgSaveButton_clicked();
    if(fldigiProcess)
    {
        fldigiProcess->close();
        fldigiProcess->deleteLater();
    }
    if(beaconTimer)
    {
        beaconTimer->stop();
        beaconTimer->deleteLater();
    }
    if(rxTimer)
    {
        rxTimer->stop();
        rxTimer->deleteLater();
    }
    if(loadedFile)
        loadedFile->deleteLater();
    if(xc)
        xc->close();
    event->accept();
}

void LINKUp::on_actionExit_triggered()
{
    this->close();
}
