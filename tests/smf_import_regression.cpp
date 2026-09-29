#include "smfdocument.h"
#include "smfimporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include "doc_measureitem.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QMap>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    // Overlapping pitch 60 on channel 1; independent pitch 60 on channel 2.
    const QByteArray events=QByteArray::fromHex(
        "00903c640a903c5005913c4015803c0014913c0014903c0000ff2f00");
    QByteArray bytes=QByteArray::fromHex("4d546864000000060000000101e04d54726b");
    { QDataStream stream(&bytes,QIODevice::Append); stream << quint32(events.size()); }
    bytes+=events;
    QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly);
    SmfDocument smf(&buffer);
    CHECK(smf.load());
    DocRoot document;
    EditorState editor;
    SmfImporter importer(&document,&smf,&editor);
    CHECK(importer.doImport());
    CHECK(document.trackList.size()==2);
    QMap<int,int> lengths;
    for(DocEvent* note=document.trackList[0]->firstEvent; note; note=note->nextEvent) {
        CHECK(note->type==DocEvent::E_Note);
        lengths.insert(note->tickPosition,note->tickLength);
    }
    CHECK(lengths.size()==2 && lengths.value(0)==36 && lengths.value(10)==66);
    DocEvent* other=document.trackList[1]->firstEvent;
    CHECK(other && other->type==DocEvent::E_Note);
    CHECK(other->tickPosition==15 && other->tickLength==41);
    CHECK(!other->nextEvent);

    // Slow, valid SMF tempo in whole-note meter used to truncate to zero BPM.
    QByteArray slow=QByteArray::fromHex("4d546864000000060000000101e04d54726b00000013"
        "00ff58040100180800ff5103ffffff00ff2f00");
    QBuffer slowBuffer(&slow); slowBuffer.open(QIODevice::ReadOnly);
    SmfDocument slowSmf(&slowBuffer); CHECK(slowSmf.load());
    DocRoot slowDocument; EditorState slowEditor;
    SmfImporter slowImporter(&slowDocument,&slowSmf,&slowEditor);
    CHECK(slowImporter.doImport());
    CHECK(slowDocument.measureItemList[0]->BPM==1);
    QByteArray saved; QBuffer output(&saved); output.open(QIODevice::WriteOnly);
    CHECK(slowDocument.save(&output,slowEditor,false));
    // Unrepresentable tempos must fail instead of wrapping the 24-bit SMF value.
    slowDocument.measureItemList[0]->timeSignatureDenominator=4;
    CHECK(!slowDocument.save(&output,slowEditor,false));

    // Failed part export must leave a previously saved file intact.
    QTemporaryDir directory; CHECK(directory.isValid());
    const QString path=directory.filePath("part.mid");
    { QFile original(path); CHECK(original.open(QIODevice::WriteOnly)); CHECK(original.write("original")==8); }
    {
        QSaveFile staged(path); CHECK(staged.open(QIODevice::WriteOnly));
        CHECK(!slowDocument.save(&staged,slowEditor,ConversionOptions(false),QList<int>()));
        staged.cancelWriting();
    }
    { QFile original(path); CHECK(original.open(QIODevice::ReadOnly)); CHECK(original.readAll()=="original"); }
    slowDocument.measureItemList[0]->BPM=0;
    CHECK(!slowDocument.save(&output,slowEditor,false));

    // Clipboard counts are 32-bit even though QList::size() is 64-bit in Qt 6.
    DocTrack source;
    source.name=QStringLiteral("Clipboard track");
    source.midiChannel=2;
    SmfMetaEvent* text=new SmfMetaEvent;
    text->tickPosition=0;
    text->metaEventType=SMF_META_EVENT_TYPE_TEXT;
    text->dataFromString(QStringLiteral("metadata"));
    source.metaEventList.append(text);
    QByteArray clipboard;
    QDataStream writer(&clipboard,QIODevice::WriteOnly);
    source.serialize(writer);
    writer << qint32(123456); // next field must remain aligned
    DocTrack copy;
    QDataStream reader(clipboard);
    copy.deserialize(reader);
    qint32 sentinel=0;
    reader >> sentinel;
    CHECK(reader.status()==QDataStream::Ok && sentinel==123456);
    CHECK(copy.name==source.name && copy.midiChannel==2);
    CHECK(copy.metaEventList.size()==1 && copy.metaEventList[0]->dataToString()==QStringLiteral("metadata"));
    DocTrack empty;
    QByteArray header; QDataStream headerWriter(&header,QIODevice::WriteOnly);
    empty.serialize(headerWriter);
    for(qint32 count : {-1, 1000000000, 1}) {
        QByteArray corrupt=header.left(header.size()-4);
        QDataStream append(&corrupt,QIODevice::Append); append << count;
        QDataStream badReader(corrupt); DocTrack badTrack;
        badTrack.deserialize(badReader);
        CHECK(badReader.status()!=QDataStream::Ok && badTrack.metaEventList.isEmpty());
    }
    std::puts("Overlapping note FIFO, channel isolation and velocity-zero note-off passed");
    return 0;
}
