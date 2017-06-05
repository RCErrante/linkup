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
#ifndef ARQPITCHER_H
#define ARQPITCHER_H

#include <QMainWindow>

namespace Ui {
class ARQPitcher;
}

class ARQPitcher : public QMainWindow
{
    Q_OBJECT

public:
    explicit ARQPitcher(QWidget *parent = 0);
    ~ARQPitcher();

public slots:
    void processIncoming(QByteArray inbytes);
    void on_modemReceiving();
    void on_modemReceivingData();
    void on_modemTransmitting();
signals:
    void sendBytes(QByteArray data);
    void sessionFinished();

private slots:
    void on_findButton_clicked();

    void on_startButton_clicked();

    bool frameUpTheFile(const QString fileName);
    //void on_readyRead();
    void on_responseTimeout();
    void sendFillQuery();
    void on_stopButton_clicked();

private:
    Ui::ARQPitcher *ui;
    bool b_modemTransmitting = false;
    bool b_incomingData = false;
    void sendTheWholeBlob();
    QString createFillQuery();
};

#endif // ARQPITCHER_H
