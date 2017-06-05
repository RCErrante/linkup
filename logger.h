#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LOG_CLASS { ALL_RX, SEL_RX, FT_RX, SEL_TX, ALL_TX, FT_TX, DEBUG };
    explicit Logger(QObject *parent = 0);
    QString space = " ";
    QString newline = "\n";
    QString stationID;
    QString DIR_PATH;
    QString tStampFormat = "yyyy-MM-dd hh:mm:ss ";
    void setStationID(QString id);
    void setTimeStampFormat(QString tstampFormat);
signals:

public slots:
    bool logEntry(QString logEntry);
    bool logEntry(int logClass, QString logEntry);
    void createLogFile();
private:
    QFile* logfile;

};

#endif // LOGGER_H
