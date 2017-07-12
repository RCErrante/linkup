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
#include "arqpitcher.h"
#include "ui_arqpitcher.h"

#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QStringBuilder>
#include <QTimer>

QByteArray bytesToSend;
QString nwFrame; // the leader frame NW which starts the process
QVector<QByteArray> framesToSend; // vector holds only FR frames of the message
bool b_nwSent = false;
bool b_sendingFrames = false;
bool b_framesSent = false;
bool b_cQsl = false;
bool b_sendingFills = false;
const QString s_NWTemplate = "NW_%1_%2_%3_%4_%5"; // QString::arg template for building a NW frame
const QString s_FRTemplate = "FR_%1_%2_%3_%4";  // template for a data frame
const QString s_FITemplate = "FI_%1_%2_%3";
const QString DELIM("_");
int i_msgNumber = 1; // starting message number
QTimer *responseTimer = 0;

ARQPitcher::ARQPitcher(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ARQPitcher)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    responseTimer = new QTimer(this);
    responseTimer->setInterval(10000); // this will change based on frame list and data rate and interleave
    connect(responseTimer, &QTimer::timeout, this, &ARQPitcher::on_responseTimeout);
}

ARQPitcher::~ARQPitcher()
{
    delete ui;
}

void ARQPitcher::setDestCall(QString dest)
{
    s_destCall = dest;
}

void ARQPitcher::setSourceCall(QString source)
{
    s_sourceCall = source;
}

void ARQPitcher::on_findButton_clicked()
{
    QString msgText = "";
    // open a file chooser to open a text file
    const QString fileName = QFileDialog::getOpenFileName(this, "Open Text File", ".", "Text Files (*.txt *.TXT);;All Files (*)");
    //const QString fileName = "XXXXXX_AAM_0001_0110001_PASS.txt";
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
            return;
//    // read the text in line by line and replensish the
//    // line ends for the GUI QTextArea
//    QTextStream in(&file);
//    while (!in.atEnd()) {
//        QString line = in.readLine();
//        msgText += (line + "\r\n");
//    }
    // COMPRESS the file before doing anything else
    // which allows all frame count calcs to work properly
    bytesToSend = qCompress(file.readAll());
    ui->FileLineEdit->setText(file.fileName());
    file.close();
    ui->messagesPlainTextEdit->appendHtml("<pre>File to send loaded...</pre>");
    //qDebug()<<"bytesToSend"<<bytesToSend;
}

void ARQPitcher::on_startButton_clicked()
{
    if (bytesToSend == "")
    {
        ui->messagesPlainTextEdit->appendHtml("<p style=color:red;><pre>*****<br>You must choose a file to send.<br>*****</pre></p>");
        return;
    }
    if (frameUpTheFile(ui->FileLineEdit->text().trimmed()))
    {
        // start the SMARQ session with an NW frame
        emit(sendBytes(nwFrame.toLatin1())); // send the leader and wait for a response
        b_nwSent = true;
        ui->messagesPlainTextEdit->appendHtml("<p style=color:blue;><pre>TX: " + nwFrame.mid(2, nwFrame.length() - 4) + "</pre></p>");
    } else
    {
        QMessageBox::information(this, "Unable to Frame File", "He's Dead Jim!");
        return;
    }
}

bool ARQPitcher::frameUpTheFile(const QString fileName)
{
    framesToSend.clear();
    // bytesToSend already holds the bytes to send
    const QString sendFile = fileName.mid(fileName.lastIndexOf("/") + 1);
    int msgLength = bytesToSend.length();
    int frameSize = 256; // trying to get the sweet spot for zlib compression + Base64 encoding
    int numFrames = msgLength/frameSize;
    if (msgLength % frameSize) // if a remainder exists add one more frame
        numFrames++;
    //qDebug()<<"numFrames"<<numFrames;
    // the Leader frame to set up the ARQ session containing message number, number of frames and the original filename
    nwFrame = s_NWTemplate.arg(s_destCall).arg(s_sourceCall).arg(i_msgNumber).arg(numFrames).arg(sendFile);
    //nwFrame.append(QString::number(numFrames)).append(DELIM).append(fileName.mid(fileName.lastIndexOf("/") + 1)).append(DELIM);
    nwFrame.append(QString::number(qChecksum(nwFrame.toLatin1(), nwFrame.length())));
    nwFrame.prepend("~1").append("~4");
    //QByteArray work(bytesToSend); // we don't change the bytes so it can be re-used
    int j = 0;
    for (int i = 0; i < numFrames; i++)
    {
        QByteArray fr;
        if (i == (numFrames - 1))
        {
            //QString rest(bytesToSend.mid(j)); // the final frame
            //fr = s_FRTemplate.arg(i_msgNumber).arg(i).arg(QString(qCompress(rest.toLatin1()).toBase64()));
            fr = s_FRTemplate.arg(s_destCall).arg(s_sourceCall).arg(i_msgNumber).arg(i).toLatin1();
            fr.append(bytesToSend.mid(j).toBase64()).append(DELIM);
            //fr.append(QString::number(i)).append(DELIM).append(DELIM).append(rest).append(DELIM);
            fr.append(QString::number(qChecksum(fr, fr.length())).toLatin1());
            fr.prepend("~1").append("~4");
            framesToSend.append(fr);
        }
        else
        {
            // ~1FR_1_3_text_CRC~4
            fr = s_FRTemplate.arg(s_destCall).arg(s_sourceCall).arg(i_msgNumber).arg(i).toLatin1();
            fr.append(bytesToSend.mid(j, frameSize).toBase64()).append(DELIM);
            //fr.append(QString::number(i)).append(DELIM).append(bytesToSend.mid(j, frameSize)).append(DELIM);
            fr.append(QString::number(qChecksum(fr, fr.length())).toLatin1());
            fr.prepend("~1").append("~4");
            framesToSend.append(fr);
            j += frameSize; // move the index to the next frame's worth of bytes
        }
        qDebug()<<"fr"<<fr;
    }
    // vector now holds all the bytes for each frame.
    // Iterate through each frame and wrap with FR_<msg num>_<frame num>_<frame size>_<data>_<checksum>
    //qDebug()<<"framesToSend length"<<framesToSend.length();
//    qDebug()<<"Frame List";
//    qDebug()<<nwFrame;
//    foreach(const QString s, framesToSend)
//        qDebug()<<s;
    return true;
}

void ARQPitcher::processIncoming(QByteArray inbytes)
{
    // slot to receive incoming responses from the smarq catcher
    // process at the other end.
    //sTimer->stop();
    // based on frame received, process accordingly
    QString cmd(inbytes);
    inbytes.clear(); // clear the incoming bytes
    ui->messagesPlainTextEdit->appendHtml("<pre>RX: " + cmd + "</pre>");
    if (cmd.contains("~1GO_" % s_sourceCall))
    {
        b_sendingFrames = true;
        // ready to receive frames after NW
        sendTheWholeBlob();
    } else if(cmd.contains("~1FL"))
    {
        if (cmd.contains("_RO_") && cmd.contains(s_sourceCall)) // got all frames ok
        {
            qDebug()<<"rec'd RO frame";
            b_cQsl = true;
            responseTimer->stop();
            emit(sessionFinished());
            ui->messagesPlainTextEdit->appendHtml("<pre>MESSAGE SENT SUCCESSFULLY</pre>");
        } else
        {
            b_sendingFills = true;
            // FILL request so send the requested frames again
            QStringList frameList;
            // ~1FL_<source>_<dest>_<msgnum>_<fill list>_<checksum>~4
            int index = cmd.indexOf(DELIM, 5) + 1; // start of list
            int endex = cmd.indexOf(DELIM, index); // end of list
            QString fl = cmd.mid(index, endex - index);
            //qDebug()<<"fill list"<<fl;
            frameList = fl.split(",", QString::SkipEmptyParts);
            QString s_out;
            foreach(QString s, frameList)
            {
                s_out.append(framesToSend.at(s.toInt()));
                emit(sendBytes(s_out.toLatin1()));
            }
            emit(sendBytes(createFillQuery().toLatin1()));
            // fill response and start a timer
            responseTimer->start(15000);
            ui->messagesPlainTextEdit->appendHtml("<p><pre>TX: SENDING FILLS: " % fl % "<br>" % s_out % "</pre></p>");
        }
    } else if (cmd.startsWith("~1NA_" % s_sourceCall))
    {
        // NACK arrives after a NW or FI frame is recognized
        // but does not pass checksum.
        // Could contain SNR value in case we wish to re-calculate
        // frame size or data rate
        /*
         * if SNR < 6 drop to 300L
         * if SNR < 4 drop to 150L
         * if SNR < 2 drop to 75L
         */
    }
}

void ARQPitcher::sendTheWholeBlob()
{
    b_cQsl = false;
    // send all frames
    foreach(QByteArray s, framesToSend)
    {
        ui->messagesPlainTextEdit->appendHtml("<p><pre>TX: " + s + "</pre><p>");
        //qDebug()<<"s"<<s;
        emit(sendBytes(s));
        qApp->processEvents();
    }
    emit(sendBytes(createFillQuery().toLatin1()));
    // starts the process of sending a fill query based on whether we
    // are still transmitting or not.  most any message can wait at least
    // 15 seconds before the first try at polling for fills
    responseTimer->start(15000);
}

void ARQPitcher::sendFillQuery()
{
    responseTimer->stop();
    if(!b_modemTransmitting)
    {
        if(b_cQsl)
        {
            return; // don't restart the response timer
        }
        emit(sendBytes(createFillQuery().toLatin1()));
        ui->messagesPlainTextEdit->appendHtml("<p><pre>TX: Fill Query</pre></p>");
    }
    responseTimer->start(15000);
}

QString ARQPitcher::createFillQuery()
{
    QString fillQuery = s_FITemplate.arg(s_destCall).arg(s_sourceCall).arg(i_msgNumber);
    fillQuery.append(QString::number(qChecksum(fillQuery.toLatin1(), fillQuery.length()))).prepend("~1").append("~4");
    return fillQuery;
}

void ARQPitcher::on_responseTimeout()
{
    // figure out what our state is and what to do next if we
    // haven't heard from the receiver in a while...usually just
    // another NW if no GO received, or another FI after initial FI
    // has been sent.
    responseTimer->stop();
    if(b_cQsl || b_modemTransmitting)
    {
        // either I alrady have the qsl message or
        // I am currently transmitting, so wait some more
        responseTimer->start(15000);
        return;
    }
    if(b_nwSent && !b_sendingFrames)
        emit(sendBytes(nwFrame.toLatin1()));
    else if(b_sendingFrames || b_sendingFills)
        sendFillQuery();
}

void ARQPitcher::on_stopButton_clicked()
{
    responseTimer->stop();
    framesToSend.clear();
    b_cQsl = false;
    b_sendingFills = false;
    b_sendingFrames = false;
    //ui->messagesPlainTextEdit->clear();
    ui->FileLineEdit->clear();
}

void ARQPitcher::on_modemReceivingData()
{
    b_modemTransmitting = false;
    b_incomingData = true;
    if(responseTimer->isActive())
        responseTimer->start(15000); // extend the timer because we are receiving data
}

void ARQPitcher::on_modemReceiving()
{
    b_modemTransmitting = false;
}

void ARQPitcher::on_modemTransmitting()
{
    b_modemTransmitting = true;
}
