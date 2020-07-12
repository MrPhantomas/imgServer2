#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>

#include <QDebug>
#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <QSettings>
#include <QTimer>
#include "log.h"

//Порт udp сервера по умолчанию
#define defUdpServerPort 50002
//Порт tcp сервера по умолчанию
#define defTcpServerPort 50003

//#define portDef 6666
#define imagePathDef "Images"
#define logPathDef "log"
#define udpDef 0
#define iniPath ".ini"
#define ENDDATA "EOC"
#define packetSize 10000

//Структура комманд для tcp сервера
struct tcpStruct
{
    tcpStruct(tcpStruct& strct)
    {
        command = strct.command;
        dataSize = strct.dataSize;
        data = strct.data;
        end = strct.end;
    }
    tcpStruct(){};
    //номер комнады
    qint8 command;
    //размер блока данных
    quint64 dataSize;
    //Блок данных
    QByteArray data;
    //Окончание блока данных
    QString end = ENDDATA;
};
//Структура комманд для udp сервера
struct udpStruct
{
    udpStruct(udpStruct& strct)
    {
        command = strct.command;
        filename = strct.filename;
        numPacket = strct.numPacket;
        packets = strct.packets;
        dataSize = strct.dataSize;
        data = strct.data;
        end = strct.end;
    }
    udpStruct(){};
    //Номер команды
    qint8 command;
    //Название файла
    QString filename;
    //Номер пакета
    quint32 numPacket;
    //Количество пакетов
    quint32 packets;
    //Размер блока данных
    quint64 dataSize;
    //БЛок данных
    QByteArray data;
    //Окончание блока данных
    QString end = ENDDATA;
};

void operator>> (QDataStream& obj, udpStruct& strct);
void operator<< (QDataStream& obj, udpStruct& strct);
void operator>> (QDataStream& obj, tcpStruct& strct);
void operator<< (QDataStream& obj, tcpStruct& strct);

struct connections
{
   QHostAddress adress;
   quint16 port = 0;
   QTimer *timer = 0;
   udpStruct* lastDatagramm = 0;
   bool ok = false;
};
void operator>> (QDataStream& obj, udpStruct* strct);
void operator<< (QDataStream& obj, udpStruct* strct);

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QString projectName, QObject *parent = nullptr);
signals:

public slots:

    void incommingConnection(); // обработчик входящего подключения TCP протокола
    void onReadyReadUdp(); // обработчик сообщений UDP протокола
    void stateChanged(QAbstractSocket::SocketState stat);//ОБработчик смены состояния подключения TCP
    void onReadyReadTcp();// обработчик сообщений TCP протокола

private:
    QTcpServer *server; // указатель на TCP сервер
    QUdpSocket *udpSocket; // указатель на UDP сокет
    QString initPath = ".ini";
    quint16 port; // порт
    QString imagePath; // адрес папки с картинками
    QString logPath; // адрес папки с картинками
    bool udp = 0;//флаг использования udp  true - UDP  false - TCP
    QSettings* settings;
    Log *lg;
    QString projectName;
    void initialization(); //инициализация
    void initAsDefault(); //инициализация в случае отсутствия или повреждения ini файла
    void createInit(); //Создание ini файла в случае его отсутствия или повреждения
    /*QByteArray createData();//Функция сбора данных файлов в массив*/
    QByteArray createDataNew();
    QList<QTcpSocket *> sockets;
    QList<connections*>* lst;
signals:

};

#endif // SERVER_H
