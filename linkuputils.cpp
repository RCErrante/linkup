#include "linkuputils.h"
#include "logger.h"

#include <QDebug>

LINKUpUtils::LINKUpUtils()
{

}
QByteArray LINKUpUtils::buildHeader(QByteArray msgText, QString to, QString from, QString fileName, int protocolVer)
{
    QByteArray sb;
    switch(protocolVer) {
    case 1:
        sb.append("1");
        break;
    case 2:
        sb.append("2");
        break;
    default:
        sb.append("1");
    }
    sb.append(to);
    sb.append(" ");
    sb.append(from);
//qDebug() << sb;
    if (protocolVer == 2) {
        sb.append(" ");
        sb.append(fileName);
    }
    //qDebug() << sb;
    sb.append(" ");
    sb.append(msgText);
    int crc16 = -1;
    crc16 = LINKUpUtils::checkSum(sb);
    //byte first = (byte) (crc16 >>> 8);
    //System.out.printf("\nCRC16: %d %04x: " , crc16, crc16);
    QString crcString = QString::number(crc16, 16);
    //System.out.println("CRC STRING: " + crcString);
    //byte last = (byte) (crc16 & 0x000000FF);
    if (crcString.length() < 4) {
        for (int i = 0; i < (4 - crcString.length()); i++) {
            crcString = "0" + crcString;
        }
    }
    sb.insert(0, "~1");
    sb.append(crcString);
    sb.append("~4");
    return sb;
}

unsigned short LINKUpUtils::checkSum(QByteArray message) {
//        unsigned short residual = 55665;
//        unsigned char update = 0;
//        unsigned short const1 = 52845;
//        unsigned short const2 = 22719;
//        unsigned int sum = 0;
//        //System.out.println("CS MSG LENGTH: " + message.length);
//        for (int i = 0; i < message.length(); i++) {
//            // use unsigned right shift to pull in zeros from the left for residual
//            update = message.at(i) ^ (residual >> 8);
//            //System.out.println("residual>>>8 = " + String.format("%08x", residual>>>8));
//            residual += update;
//            residual *= const1;
//            residual += const2;
//            // add the update to the rolling checksum total
//            sum += update;
//            //System.out.printf("Checksum %3d 0x%02x: 0x%08x\t \"%c\"\t%08x\n", i, message[i], sum, message[i], residual);
//        }
//        //System.out.printf("Calculated checksum:\t0x%x\n", sum);
//        //System.out.printf("Received checksum:\t0x%08x\n", sum);
//        return sum;
    return qChecksum(message, message.length());
}

