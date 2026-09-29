#include "smfdocument.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static QByteArray smf(const QByteArray& events) {
    QByteArray bytes=QByteArray::fromHex("4d546864000000060000000101e04d54726b");
    QDataStream stream(&bytes,QIODevice::Append);
    stream << quint32(events.size());
    bytes+=events;
    return bytes;
}
static bool loads(QByteArray bytes) {
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    SmfDocument document(&buffer);
    return document.load();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QByteArray events=QByteArray::fromHex("00903c64003d648360803c40003d4000ff2f00");
    QByteArray valid=smf(events);
    CHECK(loads(valid));
    for(int size=0; size<valid.size(); ++size) CHECK(!loads(valid.left(size)));
    CHECK(!loads(smf(QByteArray::fromHex("008080808000ff2f00")))); // five-byte VLQ
    CHECK(!loads(smf(QByteArray::fromHex("003c4000ff2f00")))); // missing running status
    CHECK(!loads(smf(QByteArray::fromHex("00903c8000ff2f00")))); // status in data
    CHECK(!loads(smf(QByteArray::fromHex("00ff01087a00ff2f00")))); // meta crosses chunk
    CHECK(!loads(smf(QByteArray::fromHex("00f0087a00ff2f00")))); // sysex crosses chunk
    CHECK(!loads(smf(QByteArray::fromHex("00903c4000ff010178003d4000ff2f00"))));
    QByteArray format2=valid; format2[9]=2; CHECK(!loads(format2));
    QByteArray badDivision=valid; badDivision[12]=0; badDivision[13]=0; CHECK(!loads(badDivision));

    QByteArray riff("RIFF");
    QByteArray payload("RMIDJUNK");
    payload+=QByteArray::fromHex("010000007800"); // odd chunk and padding
    payload+="data";
    { QDataStream stream(&payload,QIODevice::Append); stream.setByteOrder(QDataStream::LittleEndian); stream << quint32(valid.size()); }
    payload+=valid;
    { QDataStream stream(&riff,QIODevice::Append); stream.setByteOrder(QDataStream::LittleEndian); stream << quint32(payload.size()); }
    riff+=payload;
    CHECK(loads(riff));
    CHECK(!loads(riff.left(riff.size()-1)));

    QBuffer buffer(&valid); buffer.open(QIODevice::ReadWrite);
    SmfDocument document(&buffer);
    CHECK(document.load());
    CHECK(document.load());
    CHECK(document.trackList.size()==1); // repeated load replaces, never appends
    CHECK(document.trackList[0]->eventList.size()==4);
    valid=smf(QByteArray::fromHex("00903c4000ff0180808080"));
    CHECK(!document.load());
    CHECK(document.trackList.size()==1 && document.trackList[0]->eventList.size()==4);

    valid.clear(); buffer.seek(0);
    CHECK(document.save());
    CHECK(document.load());
    CHECK(document.trackList[0]->eventList.size()==4);
    CHECK(document.trackList[0]->eventList[2]->tickPosition==480);
    CHECK(!SmfDocument::eventTicksLessThan(document.trackList[0]->eventList[0],document.trackList[0]->eventList[1]));
    SmfDocument nullDocument(nullptr); CHECK(!nullDocument.save());

    // Generated track names must use the same encoding as dataToString().
    SmfMetaEvent* songName=new SmfMetaEvent;
    songName->tickPosition=0;
    songName->metaEventType=SMF_META_EVENT_TYPE_TRACK_NAME;
    songName->dataFromString(QString::fromLatin1("caf\xe9"));
    document.trackList[0]->eventList.prepend(songName);
    document.convertToFormat1(true);
    CHECK(document.trackList[1]->eventList[0]->isMetaEvent()->dataToString().startsWith(QString::fromLatin1("caf\xe9")));

    for(qint32 length : {-1, 1000000000, 4}) {
        QByteArray clipboard;
        QDataStream writer(&clipboard,QIODevice::WriteOnly);
        writer << quint32(0) << quint8(1) << length;
        clipboard+="x";
        QDataStream reader(clipboard);
        SmfMetaEvent meta;
        meta.deserialize(reader);
        CHECK(reader.status()!=QDataStream::Ok);
        CHECK(meta.data==nullptr && meta.dataLength==0);
    }
    std::puts("SMF/RMID truncation, running status, roundtrip, reload and clipboard tests passed");
    return 0;
}
