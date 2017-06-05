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
#ifndef ARQCATCHER_H
#define ARQCATCHER_H

#include <QMainWindow>

namespace Ui {
class ARQCatcher;
}

class ARQCatcher : public QMainWindow
{
    Q_OBJECT

public:
    explicit ARQCatcher(QWidget *parent = 0);
    ~ARQCatcher();
    void setInitialData(QByteArray data);
public slots:


    void processInputFrame(QByteArray data);
    void on_modemReceiving();
    void on_modemTransmitting();
signals:
    void sendBytes(QByteArray data);
    void sessionFinished();
private slots:
    //void on_readyRead();
    void processDataFrame(const QString inString);
    void processNewFrame(const QString inString);
    void processFillFrame(const QString inString);
    void processAbortFrame(const QString inString);
    void on_sendCmdButton_clicked();
    void sendGOCommand();
    void on_sendGOCommandButton_clicked();
    void sendROCommand();
    void sendNACommand();
    void on_clearButton_clicked();
    void processFrame();
private:
    Ui::ARQCatcher *ui;
    bool b_modemTransmitting = false;
    const QString SPACE = " ";
    void writeMessage();
};

#endif // ARQCATCHER_H
