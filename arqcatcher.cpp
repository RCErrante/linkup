// ----------------------------------------------------------------------------
// Copyright (C) 2016 Mitch Winkle
// ALL RIGHTS RESERVED
//
// This file is part of Station Manager software project.  Station Manager is
// NOT free software.  It has been released to the members of the Military
// Auxiliary Radio System organization under no-cost terms.  That release
// grant may be rescinded at any time by the author.
//
// Station Manager is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Parts of Station Manager may contain source code from Open Source projects.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------
#include "arqcatcher.h"
#include "ui_arqcatcher.h"

#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
//#include <QSerialPort>
#include <QStringBuilder>
#include <QTimer>
#include <QMap>
#include <QSaveFile>

QByteArray cinbytes; // the data buffer
int i_FrameType = -1;
QString s_msgNumber;
QString s_incomingFilename;
QMap<int, QString> framesReceived; // map holds only FR frames of the message
//bool b_nwSent = false;
//bool b_sendingFrames = false;
//bool b_framesSent = false;
bool b_qsl = false;
//bool b_sendingFills = false;
//const QString s_NWTemplate = "NW_%1_%2_%3_"; // QString::arg template for building a NW frame
//const QString s_FRTemplate = "FR_%1_%2_%3_";  // template for a data frame
const QString s_FITemplate = "FL_%1_";
const QString DELIM("_");
int i_cMsgNumber = 0; // starting message number
int i_numFrames = 0;
//QSerialPort *catcherOut = 0;
//QTimer *sCatcherTimer = 0;
QTimer *responseCatcherTimer = 0;

ARQCatcher::ARQCatcher(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ARQCatcher)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
//    connectToModem();
//    sCatcherTimer = new QTimer(this);
//    sCatcherTimer->setInterval(500);
//    connect(sCatcherTimer, &QTimer::timeout, this, &ARQCatcher::processFrame);
}

ARQCatcher::~ARQCatcher()
{
    delete ui;
}

void ARQCatcher::setInitialData(QByteArray data)
{
    qDebug()<<"setInitialData"<<data;
    cinbytes.append(data);
    processFrame();
}

//void ARQCatcher::connectToModem()
//{
//    catcherOut = new QSerialPort(this);
//    catcherOut->setPortName("COM1");
//    catcherOut->setBaudRate(QSerialPort::Baud115200);
//    catcherOut->setDataBits(QSerialPort::Data8);
//    catcherOut->setParity(QSerialPort::NoParity);
//    catcherOut->setStopBits(QSerialPort::OneStop);
//    catcherOut->setFlowControl(QSerialPort::NoFlowControl);
//    if(!catcherOut->open(QIODevice::ReadWrite))
//    {
//        QMessageBox::information(this, "NO SERIAL PORT", "Unable to connect to serial port.");
//        ui->messagesPlainTextEdit->appendHtml("<p><pre>NO SERIAL CONNECTION</pre></p>");
//    }
//    ui->messagesPlainTextEdit->appendHtml("<p><pre>SERIAL CONNECTION on " % catcherOut->portName() % "</pre></p>");
//    connect(catcherOut, &QSerialPort::readyRead, this, &ARQCatcher::on_readyRead);
//}

//void ARQCatcher::on_readyRead()
//{
//    sCatcherTimer->stop();
//    cinbytes.append(catcherOut->readAll());
//    //qDebug()<<"on_readyRead()";
//    sCatcherTimer->start();
//}

void ARQCatcher::processInputFrame(QByteArray data)
{
    // slot to catch smarq frames from the controller
    // frames are expected to be fully formed and may
    // be concantenated
    cinbytes.append(data);
    qDebug()<<"cinbytes"<<cinbytes;
    processFrame();
}

void ARQCatcher::processFrame()
{
    //sCatcherTimer->stop();
    qDebug()<<"processFrame()"<<cinbytes;
    // ~1NW_123_13_TESTFILE.TXT_11095~4
    // ~1FR_123_3_T TEST\r\nBT\r\n#0001\r\n\r\n\r\n\r\n\r\n\r\n\r\nNNNN\r\n_11095~4
    // message 123, frame 3 goes into the QMap at position 3 by the zero based frame number.
    if(!cinbytes.contains("~4"))
    {
        cinbytes.clear();
        return;
    }
    const QString inString(cinbytes);
    cinbytes.clear();
    QStringList frames = inString.split("~4", QString::SkipEmptyParts);
    qDebug()<<"frames"<<frames;
    // MUST deal with partial frames on the end or garbled frames in the middle that do
    // not have a beginning and end.
    foreach(QString f, frames)
    {
        int index = f.indexOf("~1") + 2;
        QString s_FrameType = f.mid(index, f.indexOf("_") - index);
        qDebug()<<"f"<<f<<"Frame Type"<<s_FrameType;
        if(s_FrameType == "NW")
        {
            processNewFrame(f);
        } else if(s_FrameType == "FR")
        {
            processDataFrame(f);
        } else if (s_FrameType == "FI")
        {
            processFillFrame(f);
        } else if(s_FrameType == "AB")
        {
            processAbortFrame(f);
        }
        // remove the frame from the input byte array in case we have a partial frame following
        //int len = f.length() + 2; // add 2 back for the "~4" delimiter
        //cinbytes = cinbytes.mid(len);
    }
}

void ARQCatcher::processNewFrame(const QString inString)
{
    framesReceived.clear();
    int index = inString.indexOf("_") + 1; // at the msg number field
    s_msgNumber = inString.mid(index, inString.indexOf("_", index) - index);
    //qDebug()<<"s_msgNumber"<<s_msgNumber;
    i_cMsgNumber = s_msgNumber.toInt();
    index = inString.indexOf("_", index) + 1; // at the number of frames field
    i_numFrames = inString.mid(index, inString.indexOf(DELIM, index) - index).toInt();
    //qDebug()<<"numFrames"<<i_numFrames;
    index = inString.indexOf("_", index) + 1; // at the payload field
    QString payload = inString.mid(index, inString.lastIndexOf("_") - index); // origination filename
    s_incomingFilename = payload;
    //qDebug()<<"payload"<<payload;
    index = inString.lastIndexOf("_") + 1; // at the checksum field
    QString checksum = inString.mid(index); //, inString.indexOf("~4", index) - index);
    //qDebug()<<"checksum"<<checksum;
    index = inString.indexOf("~1") + 2;
    QString checkString = inString.mid(index, (inString.lastIndexOf("_") + 1) - index);
    //qDebug()<<"checkString"<<checkString<<qChecksum(checkString.toLatin1(), checkString.length());
    bool pass = (checksum == (QString::number((qChecksum(checkString.toLatin1(), checkString.length())))));
    if(pass)
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>RX New Session: #" % s_msgNumber % SPACE % QString::number(i_numFrames) % SPACE % payload % "</pre></p>");
        sendGOCommand();
    }
    else
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>RX FAIL: " % inString % "</pre></p>");
        sendNACommand();
    }
}

void ARQCatcher::processDataFrame(const QString inString)
{
    int index = inString.indexOf("_") + 1; // at the msg number field
    QString msgNum = inString.mid(index, inString.indexOf("_", index) - index);
    index = inString.indexOf("_", index) + 1; // at the frame number field
    int frameNum = inString.mid(index, inString.indexOf("_", index) - index).toInt();
    index = inString.indexOf("_", index) + 1; // at the payload field
    QString payload = inString.mid(index, inString.lastIndexOf("_") - index);  // if the length is needed, payload.length() gives me that
    index = inString.indexOf("_", index) + 1; // at the checksum field
    QString checksum = inString.mid(index); // the rest of the frame is the checksum
    // also do error checking here to set bool errors = true/false;
    index = inString.indexOf("~1") + 2;
    QString checkString = inString.mid(index, (inString.lastIndexOf("_") + 1) - index);
    qDebug()<<"msgNum"<<msgNum<<"frameNum"<<frameNum<<"checkString"<<checkString<<"checksum"<<checksum;
    if(checksum == (QString::number((qChecksum(checkString.toLatin1(), checkString.length())))))
    {
        framesReceived.insert(frameNum, payload);
        ui->messagesPlainTextEdit->appendHtml("<p><pre>RX Frame OK: " % QString::number(frameNum) % " Msg: " % msgNum % SPACE % inString.mid(index) % "</pre></p>");
    }
    else
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>RX Frame FAIL: " % inString.mid(index) % "</pre></p>");
    }
}

void ARQCatcher::processFillFrame(const QString inString)
{
    const QString COMMA = ",";
    ui->messagesPlainTextEdit->appendHtml("<p><pre>RX Frame " % inString % "</pre></p>");
    // check to see if we got all of the frames
    qDebug()<<"frames recd count"<<framesReceived.count();
    if (i_numFrames == framesReceived.count())
    {
        QTimer::singleShot(2000, this, &ARQCatcher::sendROCommand);
    }
    else
    {
        qDebug()<<"frame count does not match"<<i_numFrames<<framesReceived.count();
        QString cmd = "FL_" % QString::number(i_cMsgNumber) % DELIM;

        // evaluate which frames need to be resent
        for(int f = 0; f < i_numFrames; f++)
        {
            if(framesReceived.value(f) == "") // default constructed value of a QString is ""
            {
                cmd.append(QString::number(f)).append(COMMA);
            }
        }
        cmd.chop(1); // truncate the trailing comma
        cmd.append(DELIM).append(QString::number(qChecksum(cmd.toLatin1(), cmd.length()))).prepend("~1").append("~4");
        ui->messagesPlainTextEdit->appendHtml("<p><pre>TX " % cmd % "</pre></p>");
        //catcherOut->write(cmd.toLatin1());
        emit(sendBytes(cmd.toLatin1()));
    }
}

void ARQCatcher::processAbortFrame(const QString inString)
{
    // reset all vars and print ABORT frame message
    if(responseCatcherTimer)
        responseCatcherTimer->stop();
    ui->messagesPlainTextEdit->appendHtml("<p><pre>RX ABORT Frame " % inString.mid(inString.indexOf("~1")) % "</pre></p>");
    i_cMsgNumber = 0;
    i_numFrames = 0;
    s_msgNumber = "";
    b_qsl = false;
    cinbytes.clear();
    // tell the modem controller we are done also
    emit(sessionFinished());
}

void ARQCatcher::on_sendCmdButton_clicked()
{
    QString cmd = ui->cmdLineEdit->text().trimmed();
    cmd.append(QString::number(qChecksum(cmd.toLatin1(), cmd.length()))).prepend("~1").append("~4");
    ui->messagesPlainTextEdit->appendHtml("<p><pre>TX " % cmd % "</pre></p>");
    //catcherOut->write(cmd.toLatin1());
    emit(sendBytes(cmd.toLatin1()));
}

void ARQCatcher::sendGOCommand()
{
    // the NW frame was recognized and passed checksum, so tell the sender to go ahead
    QString cmd = "GO_" % QString::number(i_cMsgNumber) % "_";
    cmd.append(QString::number(qChecksum(cmd.toLatin1(), cmd.length()))).prepend("~1").append("~4");
    ui->messagesPlainTextEdit->appendHtml("<p><pre>TX " % cmd % "</pre></p>");
    //catcherOut->write(cmd.toLatin1());
    //qDebug()<<"emit sendBytes";
    emit(sendBytes(cmd.toLatin1()));
}

void ARQCatcher::sendNACommand()
{
    // NACK, the NW was recognized but did not pass checksum...future may include LQA information
    // so the sending station may choose to change data rate or interleave or both.
    QString cmd = "NA_";
    cmd.append(QString::number(qChecksum(cmd.toLatin1(), cmd.length()))).prepend("~1").append("~4");
    ui->messagesPlainTextEdit->appendHtml("<p><pre>TX " % cmd % "</pre></p>");
    //catcherOut->write(cmd.toLatin1());
    emit(sendBytes(cmd.toLatin1()));
}

void ARQCatcher::sendROCommand()
{
    QString cmd = "FL_" %QString::number(i_cMsgNumber) % "_RO_";
    cmd.append(QString::number(qChecksum(cmd.toLatin1(), cmd.length()))).prepend("~1").append("~4");
    ui->messagesPlainTextEdit->appendHtml("<p><pre>TX " % cmd % "</pre></p>");
    //catcherOut->write(cmd.toLatin1());
    emit(sendBytes(cmd.toLatin1()));
    writeMessage();
}

void ARQCatcher::on_sendGOCommandButton_clicked()
{
    sendGOCommand();
}


void ARQCatcher::on_clearButton_clicked()
{
    cinbytes.clear();
    ui->messagesPlainTextEdit->clear();
    processAbortFrame("RESET");
}

void ARQCatcher::writeMessage()
{
    // now that we have the full message in the map, write it to disk with the supplied filename
    QSaveFile f(QApplication::applicationDirPath() + "/" + s_incomingFilename);
    if(!f.open(QSaveFile::WriteOnly))
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>Unable to open file for writing...</pre></p>");
        return;
    }
    ui->messagesPlainTextEdit->appendHtml("<p><pre>Writing File: " % f.fileName() % "</pre></p>");
    QString part;
    QMapIterator<int, QString> qmi(framesReceived);
    QByteArray toWrite;
    while(qmi.hasNext())
    {
        part = qmi.next().value();
        ui->messagesPlainTextEdit->appendHtml("<p><pre>SIZE: " % QString::number(part.length()));
        toWrite.append(QByteArray::fromBase64(part.toLatin1()));
    }
    //f.write(qUncompress(QByteArray::fromBase64(part.toLatin1())));
    f.write(qUncompress(toWrite));
    if(!f.commit())
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>Unable to write file..." % f.errorString()  %":" % QApplication::applicationDirPath() % "</pre></p>");
    }
    emit(sessionFinished()); // let the controller know that we are done
    ui->messagesPlainTextEdit->appendHtml("<p><pre>SMARQ Session Finished</pre></p>");
}

void ARQCatcher::on_modemReceiving()
{
    b_modemTransmitting = false;
}

void ARQCatcher::on_modemTransmitting()
{
    b_modemTransmitting = true;
}
