#include "smfdocument.h"
#include "smfimporter.h"
#include "smfexporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QVector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

struct Message {
    quint32 tick;
    QByteArray data;
    bool operator==(const Message& other) const { return tick==other.tick && data==other.data; }
};
static QByteArray smf(const QByteArray& events) {
    QByteArray bytes=QByteArray::fromHex("4d546864000000060000000101e04d54726b");
    QDataStream stream(&bytes,QIODevice::Append); stream << quint32(events.size());
    return bytes+events;
}
static void load(DocRoot& doc,EditorState& state,QByteArray bytes) {
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    SmfImporter importer(&doc,&source,&state); CHECK(importer.doImport());
}
static QVector<Message> messages(QByteArray bytes) {
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    QVector<Message> result;
    for(auto* track : source.trackList)for(auto* event : track->eventList)
        if(auto* midi=event->isMidiEvent()) {
            const int family=midi->midiCommand[0]&0xf0;
            // Export adds initial track volume, pan and patch. Compare every
            // original note/sustain event, including the exact same-tick order.
            if(family==0x80 || family==0x90 || (family==0xb0 && midi->midiCommand[1]==64))
                result.append({event->tickPosition,QByteArray(reinterpret_cast<const char*>(midi->midiCommand),3)});
        }
    return result;
}
static QByteArray save(const DocRoot& doc,const EditorState& state) {
    QByteArray bytes; QBuffer output(&bytes); CHECK(output.open(QIODevice::WriteOnly));
    CHECK(doc.save(&output,state,false)); return bytes;
}
static void checkRoundtrip() {
    // New note-on precedes the old note-off, with sustain changes interleaved.
    // A blanket "all offs first" changes the meaning of this sequence.
    const QByteArray input=smf(QByteArray::fromHex(
        "00903c5a0a903c5000b0407f00803c0c00903d4600b040000a803c2200803d2000ff2f00"));
    const auto expected=messages(input);
    DocRoot doc; EditorState state; load(doc,state,input);
    CHECK(doc.trackList.size()==1);
    const QByteArray saved=save(doc,state);
    CHECK(messages(saved)==expected);
    DocRoot restored; EditorState restoredState; load(restored,restoredState,saved);
    CHECK(messages(save(restored,restoredState))==expected);

    // Identical pitch and start, different velocity and release time. Source
    // order must survive the linked model's reverse insertion order so FIFO
    // pairing on the next import keeps each velocity attached to its duration.
    const QByteArray repeated=smf(QByteArray::fromHex("00903c5a00903c460a803c080a803c1100ff2f00"));
    DocRoot repeatedDoc; EditorState repeatedState; load(repeatedDoc,repeatedState,repeated);
    CHECK(messages(save(repeatedDoc,repeatedState))==messages(repeated));
    DocRoot repeatedCopy; EditorState copyState;
    load(repeatedCopy,copyState,save(repeatedDoc,repeatedState));
    int notes=0;
    for(auto* event=repeatedCopy.trackList[0]->firstEvent; event; event=event->nextEvent) {
        if(event->type != DocEvent::E_Note)continue;
        ++notes;
        CHECK(event->tickPosition==0);
        if(event->noteEventData.velocity==90) {
            CHECK(event->tickLength==10 && event->noteEventData.releaseVelocity==8);
        } else {
            CHECK(event->noteEventData.velocity==70 && event->tickLength==20 && event->noteEventData.releaseVelocity==17);
        }
    }
    CHECK(notes==2);
}
static DocEvent makeNote() {
    DocEvent event; event.type=DocEvent::E_Note; event.tickPosition=5; event.tickLength=15;
    event.noteEventData.noteNumber=60; event.noteEventData.velocity=90;
    event.noteEventData.importOnOrder=3; event.noteEventData.importOffOrder=11;
    event.noteEventData.releaseVelocity=23;
    return event;
}
static void checkClipboard() {
    DocEvent note=makeNote(), controller;
    controller.type=DocEvent::E_OtherMidi; controller.tickPosition=5; controller.tickLength=1;
    controller.otherMidiEventData.midiCommand[0]=0xb0;
    controller.otherMidiEventData.midiCommand[1]=64;
    controller.otherMidiEventData.midiCommand[2]=127;
    controller.otherMidiEventData.importOrder=4;
    controller.otherMidiEventData.sameTickSubOrdering.beforeNoteEvents=false;
    controller.otherMidiEventData.sameTickSubOrdering.index=0;
    QByteArray payload; QDataStream writer(&payload,QIODevice::WriteOnly);
    note.serialize(writer,0,100); controller.serialize(writer,0,100);
    note.serialize(writer,0,100,false); controller.serialize(writer,0,100,false);
    writer << qint32(123456);
    QDataStream reader(payload); DocEvent copiedNote,copiedController,legacyNote,legacyController;
    copiedNote.deserialize(reader); copiedController.deserialize(reader);
    legacyNote.deserialize(reader); legacyController.deserialize(reader);
    qint32 sentinel=0; reader >> sentinel;
    CHECK(reader.status()==QDataStream::Ok && sentinel==123456);
    CHECK(!(copiedNote != note)); CHECK(!(copiedController != controller));
    CHECK(legacyNote.noteEventData.importOnOrder==-1 && legacyNote.noteEventData.importOffOrder==-1);
    CHECK(legacyNote.noteEventData.releaseVelocity==64 && legacyController.otherMidiEventData.importOrder==-1);
    CHECK(legacyNote.noteEventData.noteNumber==60 && legacyController.otherMidiEventData.midiCommand[1]==64);
    // The fallback retains the exact old byte layout and positive type tag.
    QByteArray legacy; QDataStream oldWriter(&legacy,QIODevice::WriteOnly);
    note.serialize(oldWriter,0,100,false);
    QDataStream oldReader(legacy); qint32 type,start,end,key,velocity;
    oldReader >> type >> start >> end >> key >> velocity;
    CHECK(type==DocEvent::E_Note && start==5 && end==20 && key==60 && velocity==90);
    CHECK(oldReader.atEnd());

    // Corrupt extension fields, truncated extensions and extreme negative tags
    // must be rejected while still in the staging document.
    for(qint64 order : {-2LL,3LL})for(int release : {-1,23,128}) {
        QByteArray bad; QDataStream out(&bad,QIODevice::WriteOnly);
        out << qint32(-2) << qint32(0) << qint32(1) << qint32(60) << qint32(90);
        out << order << qint64(4) << qint32(release);
        QDataStream in(bad); DocEvent copy; copy.deserialize(in);
        CHECK((in.status()==QDataStream::Ok)==(order>=-1 && release>=0 && release<=127));
    }
    QByteArray extended; QDataStream out(&extended,QIODevice::WriteOnly); note.serialize(out,0,100);
    for(qsizetype length=20; length<extended.size(); ++length) {
        QDataStream in(extended.left(length)); DocEvent copy; copy.deserialize(in);
        CHECK(in.status()!=QDataStream::Ok && copy.type==DocEvent::E_Invalid);
    }
    QByteArray invalid; QDataStream invalidOut(&invalid,QIODevice::WriteOnly);
    invalidOut << qint32(-2147483647-1) << qint32(0) << qint32(1);
    QDataStream invalidIn(invalid); DocEvent copy; copy.deserialize(invalidIn);
    CHECK(invalidIn.status()!=QDataStream::Ok && copy.type==DocEvent::E_Invalid);
}
static void checkComparator() {
    QVector<SmfExporterMidiEvent> events(9);
    for(int i=0;i<events.size();++i) {
        events[i].midiCommand[0]=i%3==0 ? 0x80 : i%3==1 ? 0x90 : 0xb0;
        events[i].midiCommand[1]=60; events[i].midiCommand[2]=90;
        events[i].index=i; events[i].beforeNoteEvents=i%2==0;
        if(i<3)events[i].importOrder=2-i;
    }
    const auto less=SmfExporterMidiEvent::sameTickLessThan;
    for(const auto& a : events) {
        CHECK(!less(&a,&a));
        for(const auto& b : events) {
            CHECK(!(less(&a,&b) && less(&b,&a)));
            for(const auto& c : events)
                if(less(&a,&b) && less(&b,&c))CHECK(less(&a,&c));
        }
    }
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    checkRoundtrip(); checkClipboard(); checkComparator();
    std::puts("Source event order, repeated-note releases and legacy clipboard passed");
}
