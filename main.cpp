#include "linkup.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LINKUp w;
    w.show();

    return a.exec();
}
