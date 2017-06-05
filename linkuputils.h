#ifndef LINKUPUTILS_H
#define LINKUPUTILS_H

#include <QByteArray>
#include <QString>


class LINKUpUtils
{
public:
    LINKUpUtils();

    static QByteArray buildHeader(QByteArray, QString, QString, QString, int);
    static unsigned short checkSum(QByteArray);
};

#endif // LINKUPUTILS_H
