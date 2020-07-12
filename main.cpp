#include <QCoreApplication>
#include "server.h"
#include <QString>
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QStringList lst = QString(argv[0]).split("/");
    Server obj(lst[lst.size()-1]);
    return a.exec();
}
