#ifndef LINKUP_H
#define LINKUP_H

#include "logger.h"

#include <QMainWindow>
#include <QObject>
#include <QSettings>
#include <QtNetwork/QTcpSocket>
#include <QDateTime>

namespace Ui {
class LINKUp;
}

class LINKUp : public QMainWindow
{
    Q_OBJECT

public:
    explicit LINKUp(QWidget *parent = 0);
    ~LINKUp();

    bool eventFilter(QObject *obj, QEvent *event);
public slots:
    void on_sessionFinished();
    void sendRawBytesToModem(QByteArray toSend);
    void reportFLDIGIFinished(int code, int exitStatus);
signals:
    void modemTransmitting();
    void modemReceiving();
    void modemReceivingData();
    void smarqReply(QByteArray data);
private slots:

    void on_EMCONCheckbox_toggled(bool checked);

    void on_modemComboBox_currentIndexChanged(const QString &arg1);

    void on_heardList_clicked(const QModelIndex &index);

    void processIncomingData();

    void on_FileXferButton_clicked();

    void on_orderWireLineEdit_returnPressed();

    void on_LINKUpButton_clicked();

    void on_abortButton_clicked();

    void on_resetButton_clicked();

    void on_delCallButton_clicked();

    void on_centerTimer();

    void on_QRUButton_clicked();

    void on_QRVButton_clicked();

    void on_sPClearButton_clicked();

    void on_SPOpenFileButton_clicked();

    void on_QSLButton_clicked();

    void on_logButton_clicked();

    void on_SPSaveAsButton_clicked();

    void on_SPSendAllButton_clicked();

    void on_SPSendSeletctedButton_clicked();

    void on_SPSaveButton_clicked();

    void on_cfgCancelButton_clicked();

    void on_cfgSaveButton_clicked();

    void on_beaconTimer();

    void on_useZULUCheckbox_toggled(bool checked);

    void on_SPFORMSButton_clicked();

    void on_modemsSelectAllCheckbox_clicked(bool checked);

    void on_modemsSaveButton_clicked();

    void on_findFLDIGIButton_clicked();

    void on_fldigiLockCheckBox_clicked(bool checked);

    void setupFLDIGI();

    void on_fldigiUseCurrentModemButton_clicked();

    void updateClock();
    void on_smarqButton_clicked();

    void launchSMARQCatcher(QByteArray data);
    void sendBytesToModem(QByteArray data);
    void closeEvent(QCloseEvent *event);
    void on_actionExit_triggered();

    void on_chooseBeaconFileButton_clicked();

private:
    bool b_closingDown = false;
    bool b_EMCON = false;
    // bools for rx data processing
    bool b_soh = false;
    bool b_eot = false;
    bool b_useZULUTime = false;
    bool b_xmitInhibit = false;
    bool b_inSMARQSession = false;
    //unsigned short proto = 0;
    //const QString version = "20160826";
    const QString version = __DATE__;
    const QByteArray qbaSOH = "~1";
    const QByteArray qbaEOT = "~4";
    Ui::LINKUp *ui;
    QString lastOrderWire;
    QString mycall;
    QString currModem;
    QString currentSPFileName;
    QString s_beaconFile;
    const QString NEWLINE = "\r\n";
    const QString STYLE_RED = "<p style=color:red><pre>";
    const QString END_HTML = "</pre></p>";
    QByteArray inbytes;
    QByteArray PROTO1 = QByteArrayLiteral("~11");
    QByteArray PROTO2 = QByteArrayLiteral("~12");
    QByteArray SMARQ = QByteArrayLiteral("~1NW_");
    QByteArray SPACE = QByteArrayLiteral(" ");
    QTcpSocket *modem;
    QSettings *settings = new QSettings("LINKUp.ini", QSettings::IniFormat);
    Logger log;
    bool loadSettings();
    void writeHexDump();
    void reportModemConnected();
    bool writeBinaryFile(QString fileName, QByteArray msgBytes);
    void moveFileToSent();
    void clearInputBuffer(int before);
    void updateHeardList();
    void processFrame(QByteArray inbytes);
    void updateHeardList(QString sourceCall);
    void sendText(QString toSend, QString destCall);
    QByteArray handleQRU(QString sourceCall);
    QString handleQRV(QString sourceCall);
    QString handleQRVQSL(QString fileName);
    void sendTextFile(QString fileName, QString destCall);
    void loadModems();
    void autoStartFLDIGI(QString fldigiPath, QString args);

};

#endif // LINKUP_H
