#include "server.h"
#include <QByteArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QThread>
#include <QDataStream>
#define NEWDATA 1
#define RESENDDATA 2
#define CONNECTIONRESPONSE 3
#define ASK 4
#define CONNECTIONASK 5

Server::Server(QString projectName,QObject *parent) :
    QObject(parent), projectName(projectName)
{
    settings = new QSettings(initPath, QSettings::IniFormat);
    initialization();
    lg->createLog("daemon server start");

    lst = new QList<connections*>;

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, defUdpServerPort);
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(onReadyReadUdp()));

    if(!udp)
    {
        //если сокет TCP
        server = new QTcpServer(this);
        qDebug() << "server listen = " << server->listen(QHostAddress::Any, defTcpServerPort);
        connect(server, SIGNAL(newConnection()), this, SLOT(incommingConnection()));
    }
}

void operator>> (QDataStream& obj, tcpStruct& strct) //Перегрузка для удобства
{
    obj>>strct.command>>strct.dataSize>>strct.data>>strct.end;
}

void operator<< (QDataStream& obj, tcpStruct& strct) //Перегрузка для удобства
{
    obj<<strct.command<<strct.dataSize<<strct.data<<strct.end;
}

void operator>> (QDataStream& obj, udpStruct& strct)
{
    obj>>strct.command>>strct.filename>>strct.numPacket>>strct.packets>>strct.dataSize>>strct.data>>strct.end;
}

void operator<< (QDataStream& obj, udpStruct& strct)
{
    obj<<strct.command<<strct.filename<<strct.numPacket<<strct.packets<<strct.dataSize<<strct.data<<strct.end;
}

void operator>> (QDataStream& obj, udpStruct* strct)
{
    obj>>strct->command>>strct->filename>>strct->numPacket>>strct->packets>>strct->dataSize>>strct->data>>strct->end;
}

void operator<< (QDataStream& obj, udpStruct* strct)
{
    obj<<strct->command<<strct->filename<<strct->numPacket<<strct->packets<<strct->dataSize<<strct->data<<strct->end;
}

void Server::initialization() // получение настроек
{
    //Создание папки etc
    if(!QDir("etc").exists())
    {
        QDir().mkdir("etc");
    }
    //Проверка наличия .ini файла
    if(!QFile(initPath).exists())
    {
        createInit();
        initAsDefault();
        return;
    }

    bool ok_2, ok_3, ok_4;
    QString imagePath_ = settings->value("imagePath", "").toString();// папка с картинками
    QString logPath_ = settings->value("logPath", "").toString();// папка для логирования
    int upd_ = settings->value("udp", -1).toInt(&ok_2);// флаг использования UDP
    int maxLogSize_ = settings->value("maxLogSize", -1).toInt(&ok_3);
    int maxFiles_ = settings->value("maxFiles", -1).toInt(&ok_4);

    if( !ok_2 || !ok_3 || !ok_3 || imagePath_ == ""
            || logPath_ == "" || maxLogSize_ == -1 || maxFiles_ == -1 )
    {
        //если какая то переменная в файле не верна
        qDebug() << ".ini file corrupted... initializate as default";
        createInit();
        qDebug() << "maxLogSize = "<<maxLogSizeDef;
        qDebug() << "maxFiles = "<<maxFilesDef;
        initAsDefault();
    }
    else
    {
        imagePath = imagePath_;
        logPath = logPath_;
        udp = upd_;
        lg = new Log(logPath, projectName, maxLogSize_, maxFiles_);
        qDebug() << "initialization succes";
        qDebug() << "maxLogSize = "<<maxLogSize_;
        qDebug() << "maxFiles = "<<maxFiles_;
        qDebug() << "image path = "<<imagePath;
        qDebug() << "log path = "<<logPath;
        qDebug() << "use "<<(udp?"UPD":"TCP")<<" socket";
    }

}

void Server::createInit() // создание файла настроек
{
    settings->setValue("imagePath", QString(imagePathDef));
    settings->setValue("logPath", QString(logPathDef));
    settings->setValue("udp", QString::number(udpDef));
    settings->setValue("maxLogSize", QString::number(maxLogSizeDef));
    settings->setValue("maxFiles", QString::number(maxFilesDef));
}

void Server::initAsDefault() // инициализация по умолчанию
{
    qDebug() << "initialization with default configuration";
    imagePath = imagePathDef;
    if(!QDir(imagePath).exists())
        QDir().mkdir(imagePath);
    logPath = logPathDef;
    udp = udpDef;
    lg = new Log(logPath, projectName);
    qDebug() << "path = "<<imagePath;
    qDebug() << "log path = "<<logPath;
    qDebug() << "use "<<(udp?"UPD":"TCP")<<" socket";
}


void Server::incommingConnection() // обработчик подключений TCP
{
    //получение сокета отправителя
    QTcpSocket * socket = server->nextPendingConnection();
    //подключение к сигналам изменения состояния и чения
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(stateChanged(QAbstractSocket::SocketState)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyReadTcp()));
    //добавление сокета в список сокетов
    sockets << socket;
    //вызов сигнала изменения состояния на успешное подключение
    socket->stateChanged(QAbstractSocket::ConnectedState);
}

void Server::onReadyReadUdp() // обработчик UDP
{
    //qDebug() << "начало считывания udp данных";
    QByteArray tmpMessage;
    QHostAddress senderHost;
    quint16 senderPort;
    tmpMessage.resize(udpSocket->pendingDatagramSize());
    udpSocket->readDatagram(tmpMessage.data(), tmpMessage.size(), &senderHost, &senderPort);

    quint8 command;
    QDataStream obj1(&tmpMessage, QIODevice::ReadOnly);
    obj1>>command;

    switch(command)
    {
        case NEWDATA:
        {
            udpStruct strct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;

            int i;
            for(i = 0;i<lst->size();i++)
            {
                if(lst->at(i)->adress == senderHost && lst->at(i)->port == senderPort)
                    break;
            }
            if(strct.filename=="ask")
            {
                lst->at(i)->timer->stop();
                lst->at(i)->ok = true;
            }
            QByteArray tmp;
            tmpMessage.clear();
            udpStruct outStrct;
            QDataStream out(&tmpMessage, QIODevice::WriteOnly);
            strct.command = quint8(1);
            strct.numPacket = quint64(1);
            strct.packets = quint64(1);
            strct.dataSize = quint64(strct.data.size());
            out<<outStrct;
            udpSocket->writeDatagram(tmpMessage, tmpMessage.size(), senderHost, senderPort);
            udpSocket->waitForBytesWritten();
            break;
        }

        case ASK:
        {


            break;
        }

        case CONNECTIONRESPONSE:
        {
            qDebug() << "Запрос на подключение получен";
            tmpMessage.clear();
            udpStruct strct;
            QDataStream obj(&tmpMessage, QIODevice::WriteOnly);
            strct.command = quint8(CONNECTIONASK);
            strct.numPacket = quint64(1);
            strct.packets = quint64(1);
            strct.dataSize = quint64(strct.data.size());
            strct.end = QString(ENDDATA);
            obj<<strct;
            udpSocket->writeDatagram(tmpMessage, tmpMessage.size(), senderHost, senderPort);
            udpSocket->waitForBytesWritten();
            break;
        }

        default:
        {
            break;
        }
    }
}

QByteArray Server::createDataNew()
{
    QByteArray bmpArray;
    QDataStream out(&bmpArray, QIODevice::Append);
    QDirIterator itr(imagePath, QStringList()<<"*.bmp", QDir::Files); //ищем только bmp файлы

    while(itr.hasNext())
    {
        QFile img(itr.next());
        QFileInfo fInfo(img.fileName());
        QString fileName = fInfo.completeBaseName()+"."+fInfo.completeSuffix(); //Получение имени файла

        if(img.open(QIODevice::ReadOnly))
        {
            quint64 filesize = img.size();//Получение размера файла
            qDebug() << "img - "<<fileName<<" open size("<<filesize<<")";
            out<<fileName<<filesize<<img.readAll();
            lg->createLog("add " + fileName + " size " + QString::number(filesize) + " into packet");
        }
        else
        {
            lg->createLog("can't open " + fileName);
            qDebug() << "img - "<<fileName<<" cant open size("<<img.size()<<")";
        }
        img.close();
        QThread::msleep(5);
    }
    return bmpArray;
}


void Server::stateChanged(QAbstractSocket::SocketState stat)
{
   QObject * object = QObject::sender();
   if (!object)
       return;
   QTcpSocket * socket = static_cast<QTcpSocket *>(object);

    if (stat == QAbstractSocket::UnconnectedState)
    {
        qDebug() << "disconnected";
        lg->createLog(socket, QString("disconnected"));
    }
    else if(stat == QAbstractSocket::ConnectedState)
    {
        qDebug() << "connected";
        lg->createLog(socket, QString("connected"));
    }
}

void Server::onReadyReadTcp()
{
    qDebug()<<"начало считывания данных";
    //получение сокета отпрвиеля сигнала
    QTcpSocket * socket = static_cast<QTcpSocket *>(QObject::sender());

    QByteArray tmpMessage;
    //получение данных

    while(socket->bytesAvailable() || socket->waitForReadyRead(1000))
    {
        tmpMessage.append(socket->readAll());
        QThread::msleep(5);
    }

    QDataStream in(&tmpMessage, QIODevice::ReadOnly);
    quint8 command;
    in>>command;

    switch(command)
    {
        case NEWDATA:// запрос на отпраку данных
        {
            QDir dir(imagePath);
            if(!dir.exists())//Проверка наличия папки с картинками
            {
                //если папки нет то закрываем соединение
                qDebug() << "Директория "<<this->imagePath<<" не существует.";
                socket->close();
                lg->createLog(socket, + " closed, not exists path with images ");
                return;
            }
            //Получение данных картинок
            QByteArray message;
            tcpStruct strct;
            strct.command = NEWDATA;
            strct.data = createDataNew();
            strct.dataSize = quint64(strct.data.size());
            strct.end = QString("EOC");
            QDataStream obj(&message, QIODevice::WriteOnly);
            obj<<strct;
            socket->write(message);
            socket->waitForBytesWritten();
            lg->createLog(socket, "packet send");
            break;
        }

        case RESENDDATA://запрос на отправку данных в случае их повреждения
        {
            quint64 totalSize;
            quint64 reciveDataSize;
            in>>totalSize>>reciveDataSize;

            //Получение данных картинок

            QByteArray message;
            tcpStruct strct;
            strct.command = RESENDDATA;
            strct.data = createDataNew();
            strct.dataSize = quint64(strct.data.size());
            strct.end = QString("EOC");

            //Если размер данных не изменился
            if(!(totalSize!=strct.dataSize+reciveDataSize))
            {
                strct.command = NEWDATA;
                strct.data = strct.data.mid(reciveDataSize);
                strct.dataSize = quint64(strct.data.size());
            }
            QDataStream obj(&message, QIODevice::WriteOnly);
            obj<<strct;
            socket->write(message);
            socket->waitForBytesWritten();
            break;
        }

        default:
        {
            break;
        }

    }
    qDebug()<<"конец считывания данных";
}
