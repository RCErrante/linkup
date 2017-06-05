#include "logger.h"

#include <QDateTime>
#include <QFile>
#include <QApplication>
#include <QDebug>

Logger::Logger(QObject *parent) : QObject(parent)
{
    DIR_PATH = QApplication::applicationDirPath() + "/log/";
}

void Logger::createLogFile() {
    QDateTime stamp = QDateTime::currentDateTimeUtc();
    QString logfileName = stamp.toString("yyyyMMdd") + "_LOG.txt";
    logfile = new QFile(DIR_PATH + logfileName);
    //qDebug() << logfile->fileName();
}

void Logger::setStationID(QString id) {
    stationID = id.toLatin1();
}

void Logger::setTimeStampFormat(QString tstampFormat) {
    tStampFormat = tstampFormat.toLatin1();
}


bool Logger::logEntry(QString logEntry) {
    // get the current timestamp
    QDateTime stamp = QDateTime::currentDateTimeUtc();
    // construct a filename with the current timestamp
    QString newLogfileName = DIR_PATH + stamp.toString("yyyyMMdd") + "_LOG.txt";
    // if there is no logfile defined or the current logfile and
    // the newly constructed logfile names are different
    // create a new logfile.  This signifies crossing over
    // the ZULU day.
    if (logfile == 0 || logfile->fileName() != newLogfileName)
        createLogFile();
    // open the file in write only, append, and system dependent line ends
    logfile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    // append the text to the logfile
    logfile->write((stamp.toString(tStampFormat) + stationID + space + "[LOG_ENTRY] " + logEntry + newline).toLatin1());
    // and close the logfile
    logfile->close();
    return true;
}

bool Logger::logEntry(int logClass, QString logEntry) {
    // get the current timestamp
    QDateTime stamp = QDateTime::currentDateTimeUtc();
    // construct a filename with the current timestamp
    QString newLogfileName = DIR_PATH + stamp.toString("yyyyMMdd") + "_LOG.txt";
    // if there is no logfile defined or the current logfile and
    // the newly constructed logfile names are different
    // create a new logfile.  This signifies crossing over
    // the ZULU day.
    if (logfile == 0 || logfile->fileName() != newLogfileName)
        createLogFile();
    // open the file in write only, append, and system dependent line ends
    logfile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    // append the text to the logfile
    QString lClass;
    switch (logClass) {
    case ALL_RX:
        lClass = tr("[ALL_RX] ");
        break;
    case SEL_RX:
        lClass = tr("[SEL_RX] ");
        break;
    case FT_RX:
        lClass = tr("[FT_RX] ");
        break;
    case SEL_TX:
        lClass = tr("[SEL_TX] ");
        break;
    case ALL_TX:
        lClass = tr("[ALL_TX] ");
        break;
    case FT_TX:
        lClass = tr("[FT_TX] ");
        break;
    case DEBUG:
        lClass = tr("[DEBUG] ");
        break;
    default:
        lClass = tr("[LOG_ENTRY] ");
        break;
    }
    //qDebug() << "[" + lClass + "]";
    logfile->write((stamp.toString(tStampFormat) + stationID + space + lClass + logEntry + newline).toLatin1());
    // and close the logfile
    logfile->close();
    return true;
}
