#include "qtsingleapplication/qtlocalpeer.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QUuid>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
class TestPeer : public QtLocalPeer {
public:
    TestPeer() : QtLocalPeer(nullptr,"local-peer-regression-"+QUuid::createUuid().toString()) {}
    QString name() const { return socketName; }
    bool listening() const { return server->isListening(); }
};
static void pump(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms,&loop,&QEventLoop::quit);
    loop.exec();
}
static void send(QLocalSocket& socket,const QString& name,const QByteArray& bytes) {
    socket.connectToServer(name);
    CHECK(socket.waitForConnected(1000));
    CHECK(socket.write(bytes)==bytes.size());
    CHECK(socket.waitForBytesWritten(1000));
}
static QByteArray frame(const QByteArray& body) {
    QByteArray bytes;
    QDataStream stream(&bytes,QIODevice::WriteOnly);
    stream << quint32(body.size());
    return bytes+body;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    TestPeer peer; CHECK(!peer.isClient());
    if(!peer.listening())return 77; // sandbox may prohibit local sockets
    int received=0; QString last;
    QObject::connect(&peer,&QtLocalPeer::messageReceived,&app,[&](const QString& value) { ++received; last=value; });
    { QLocalSocket socket; send(socket,peer.name(),QByteArray(1,'\0')); socket.disconnectFromServer(); pump(80); CHECK(received==0); }
    { QLocalSocket socket; send(socket,peer.name(),frame("incomplete").left(6)); socket.disconnectFromServer(); pump(80); CHECK(received==0); }
    { QLocalSocket socket; send(socket,peer.name(),QByteArray::fromHex("00100001")); pump(80); CHECK(received==0); CHECK(socket.state()==QLocalSocket::UnconnectedState); }
    { QLocalSocket socket; send(socket,peer.name(),QByteArray(1,'\0')); pump(2200); CHECK(received==0); CHECK(socket.state()==QLocalSocket::UnconnectedState); }
    { QLocalSocket socket; QByteArray bytes=frame(QString::fromUtf8("Őrült Űrhajó.mid").toUtf8());
      send(socket,peer.name(),bytes.left(2)); pump(30); CHECK(received==0);
      socket.write(bytes.mid(2)); socket.flush(); pump(80);
      CHECK(received==1 && last==QString::fromUtf8("Őrült Űrhajó.mid")); CHECK(socket.readAll()=="ack"); }
    { QLocalSocket socket; send(socket,peer.name(),frame(QByteArray())); pump(80); CHECK(received==2 && last.isEmpty()); CHECK(socket.readAll()=="ack"); }
    std::puts("local peer regression passed");
}
