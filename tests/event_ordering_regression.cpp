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
#include <array>
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
static void checkImplicitReleaseVelocity()
{
    DocRoot doc; EditorState state;
    load(doc,state,smf(QByteArray::fromHex("00903c5a0a903c0000ff2f00")));
    CHECK(doc.trackList[0]->firstEvent->noteEventData.releaseVelocity==64);
    const auto output=messages(save(doc,state)); CHECK(output.size()==2);
    CHECK(output[1].tick==10 && output[1].data==QByteArray::fromHex("803c40"));
    DocRoot explicitDoc; EditorState explicitState;
    load(explicitDoc,explicitState,smf(QByteArray::fromHex("00903c5a0a803c0000ff2f00")));
    CHECK(explicitDoc.trackList[0]->firstEvent->noteEventData.releaseVelocity==0);
    CHECK(messages(save(explicitDoc,explicitState))[1].data==QByteArray::fromHex("803c00"));
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
class FullExporterComparator : public SmfExporter
{
public:
    using SmfExporter::eventOrderingLessThan;
};
static void checkMixedComparator()
{
    std::array<SmfExporterMidiEvent,9> midi;
    std::array<SmfSysExEvent,4> packets;
    std::array<SmfMetaEvent,2> meta;
    QVector<SmfEvent*> events;
    for(int i=0;i<int(midi.size());++i)
    {
        midi[i].tickPosition=10;
        midi[i].midiCommand[0]=i%3==0 ? 0x80 : i%3==1 ? 0x90 : 0xb0;
        midi[i].midiCommand[1]=60; midi[i].midiCommand[2]=90;
        midi[i].index=i; midi[i].beforeNoteEvents=i%2==0;
        if(i<3)midi[i].importOrder=2-i;
        events.append(&midi[i]);
    }
    for(int i=0;i<int(packets.size());++i)
    {
        packets[i].tickPosition=10;
        packets[i].sysExType=i%2 ? 0xf0 : 0xf7;
        packets[i].importOrder=i==0 ? -1 : i==1 ? 0 : 2;
        events.append(&packets[i]);
    }
    for(int i=0;i<int(meta.size());++i)
    {
        meta[i].tickPosition=10; meta[i].metaEventType=i ? SMF_META_EVENT_TYPE_TEXT : SMF_META_EVENT_TYPE_TEMPO;
        events.append(&meta[i]);
    }
    const auto less=FullExporterComparator::eventOrderingLessThan;
    for(auto* a : events)
    {
        CHECK(!less(a,a));
        for(auto* b : events)
        {
            CHECK(!(less(a,b) && less(b,a)));
            for(auto* c : events)
            {
                if(less(a,b) && less(b,c))CHECK(less(a,c));
                if(!less(a,b) && !less(b,a) && !less(b,c) && !less(c,b))
                    CHECK(!less(a,c) && !less(c,a));
            }
        }
    }
}
static QVector<Message> messagesWithPackets(QByteArray bytes)
{
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    QVector<Message> result;
    for(auto* track : source.trackList)for(auto* event : track->eventList)
    {
        if(auto* packet=event->isSysExEvent())
        {
            QByteArray data(1,char(packet->sysExType));
            data.append(reinterpret_cast<const char*>(packet->data),packet->dataLength);
            result.append({packet->tickPosition,data});
        }
        else if(auto* midi=event->isMidiEvent())
        {
            const int family=midi->midiCommand[0]&0xf0;
            if(family==0x80 || family==0x90 || (family==0xb0 && midi->midiCommand[1]==64))
                result.append({event->tickPosition,QByteArray(reinterpret_cast<const char*>(midi->midiCommand),3)});
        }
    }
    return result;
}
static void checkMixedRoundtripPermutations()
{
    // All 120 source permutations of sustain, two opaque fragments, a new
    // note-on and an old note-off at one tick must survive both roundtrips.
    const std::array<QByteArray,5> eventBytes={QByteArray::fromHex("b0407f"),QByteArray::fromHex("f0027d01"),
        QByteArray::fromHex("f70202f7"),QByteArray::fromHex("903d60"),QByteArray::fromHex("803c11")};
    std::array<int,5> order={0,1,2,3,4};
    int permutations=0;
    do
    {
        QByteArray track=QByteArray::fromHex("00903c5a");
        for(int i=0;i<int(order.size());++i)
        { track.append(char(i==0 ? 10 : 0)); track.append(eventBytes[order[i]]); }
        track.append(QByteArray::fromHex("0a803d220aff2f00"));
        QByteArray input=QByteArray::fromHex("4d546864000000060001000201e04d54726b0000000400ff2f004d54726b");
        QDataStream writer(&input,QIODevice::Append); writer << quint32(track.size()); input.append(track);
        const auto expected=messagesWithPackets(input);
        DocRoot doc; EditorState state; load(doc,state,input);
        QByteArray saved=save(doc,state); CHECK(messagesWithPackets(saved)==expected);
        DocRoot copy; EditorState copyState; load(copy,copyState,saved);
        CHECK(messagesWithPackets(save(copy,copyState))==expected);
        ++permutations;
    } while(std::next_permutation(order.begin(),order.end()));
    CHECK(permutations==120);
}
static QVector<Message> setupAndPackets(QByteArray bytes) {
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    QVector<Message> result;
    for(auto* track : source.trackList)for(auto* event : track->eventList) {
        if(auto* packet=event->isSysExEvent()) {
            QByteArray data(1,char(packet->sysExType));
            data.append(reinterpret_cast<const char*>(packet->data),packet->dataLength);
            result.append({event->tickPosition,data});
        } else if(auto* midi=event->isMidiEvent()) {
            const int command=midi->midiCommand[0]&0xf0;
            if(command==0xc0 || (command==0xb0 && (midi->midiCommand[1]==7 || midi->midiCommand[1]==10)))
                result.append({event->tickPosition,QByteArray(reinterpret_cast<const char*>(midi->midiCommand),3)});
        }
    }
    return result;
}
static void checkInitialResetSetupOrder() {
    // Keep reset/setup in a normal format-1 track, rather than letting format-0
    // conductor extraction accidentally hide a change to their source order.
    for(const QByteArray& reset : {QByteArray::fromHex("00f00a4110421240007f0041f7"),
                                  QByteArray::fromHex("00f0057e7f0901f7")})
    for(bool bank : {false,true})for(bool saveState : {false,true}) {
        QByteArray track=reset;
        if(bank)track+=QByteArray::fromHex("00b0000200b02003");
        track+=QByteArray::fromHex("00b0071400b00a7f00c02800903c640a803c4000ff2f00");
        QByteArray bytes=QByteArray::fromHex("4d546864000000060001000201e04d54726b0000000400ff2f004d54726b");
        QDataStream length(&bytes,QIODevice::Append); length << quint32(track.size()); bytes+=track;
        const auto expected=setupAndPackets(bytes);
        for(int cycle=0;cycle<3;++cycle) {
            DocRoot doc; EditorState state; load(doc,state,bytes);
            CHECK(doc.trackList[0]->midiPatch==41 && doc.trackList[0]->midiVolume==20 && doc.trackList[0]->midiPanorama==127);
            QByteArray output; QBuffer buffer(&output); CHECK(buffer.open(QIODevice::WriteOnly));
            CHECK(doc.save(&buffer,state,saveState));
            CHECK(setupAndPackets(output)==expected);
            // Editing track settings changes their retained final commands in
            // place, rather than injecting replacements before the reset.
            doc.trackList[0]->midiPatch=73; doc.trackList[0]->midiVolume=55; doc.trackList[0]->midiPanorama=32;
            QByteArray edited; QBuffer editedBuffer(&edited); CHECK(editedBuffer.open(QIODevice::WriteOnly));
            CHECK(doc.save(&editedBuffer,state,saveState));
            auto editedExpected=expected;
            for(auto& message : editedExpected) {
                const int status=quint8(message.data[0])&0xf0;
                if(status==0xc0)message.data[1]=char(72);
                else if(status==0xb0 && quint8(message.data[1])==7)message.data[2]=char(55);
                else if(status==0xb0 && quint8(message.data[1])==10)message.data[2]=char(32);
            }
            CHECK(setupAndPackets(edited)==editedExpected);
            bytes=output;
        }
    }
}
static QByteArray resetLayout(const QByteArray& track,int layout) {
    if(layout==0)return smf(track);
    if(layout==1) {
        QByteArray bytes=QByteArray::fromHex("4d546864000000060001000201e04d54726b");
        QDataStream length(&bytes,QIODevice::Append); length << quint32(track.size());
        return bytes+track+QByteArray::fromHex("4d54726b0000000400ff2f00");
    }
    QByteArray bytes=QByteArray::fromHex("4d546864000000060001000201e04d54726b0000000400ff2f004d54726b");
    QDataStream length(&bytes,QIODevice::Append); length << quint32(track.size()); return bytes+track;
}
static void checkResetMissingSetupAndMixedConductor() {
    for(int layout=0;layout<3;++layout)for(int mask=0;mask<8;++mask)
    for(bool bank : {false,true})for(bool saveState : {false,true}) {
        QByteArray track=QByteArray::fromHex("00f0057e7f0901f7");
        if(bank)track+=QByteArray::fromHex("00b0000200b02003");
        if(mask&1)track+=QByteArray::fromHex("00b00714");
        if(mask&2)track+=QByteArray::fromHex("00b00a7f");
        if(mask&4)track+=QByteArray::fromHex("00c028");
        track+=QByteArray::fromHex("00903c640a803c4000ff2f00");
        QByteArray bytes=resetLayout(track,layout);
        for(int cycle=0;cycle<3;++cycle) {
            DocRoot doc; EditorState state; load(doc,state,bytes);
            doc.trackList[0]->midiPatch=73; doc.trackList[0]->midiVolume=55; doc.trackList[0]->midiPanorama=32;
            QByteArray output; QBuffer buffer(&output); CHECK(buffer.open(QIODevice::WriteOnly));
            CHECK(doc.save(&buffer,state,saveState));
            QBuffer input(&output); CHECK(input.open(QIODevice::ReadOnly));
            SmfDocument saved(&input); CHECK(saved.load());
            int volume=-1,pan=-1,patch=-1,resets=0,volumes=0,pans=0,patches=0;
            bool heard=false;
            for(auto* savedTrack : saved.trackList)for(auto* event : savedTrack->eventList) {
                if(event->isSysExEvent()) { ++resets; volume=100; pan=64; patch=0; }
                if(auto* midi=event->isMidiEvent()) {
                    if((midi->midiCommand[0]&0x0f)!=0)continue;
                    const int command=midi->midiCommand[0]&0xf0;
                    if(command==0xc0) { patch=midi->midiCommand[1]; ++patches; }
                    if(command==0xb0 && midi->midiCommand[1]==7) { volume=midi->midiCommand[2]; ++volumes; }
                    if(command==0xb0 && midi->midiCommand[1]==10) { pan=midi->midiCommand[2]; ++pans; }
                    if(command==0x90 && midi->midiCommand[2]) {
                        CHECK(volume==55 && pan==32 && patch==72); heard=true;
                    }
                }
            }
            CHECK(heard && resets==1 && volumes==1 && pans==1 && patches==1);
            bytes=output;
        }
    }
}
static void checkSetupBeforeLaterReset() {
    for(bool repeatedReset : {false,true}) {
        QByteArray track=QByteArray::fromHex("00b0071400b00a7f00c02800f0057e7f0901f7");
        if(repeatedReset)track+=QByteArray::fromHex("00b0075000f0057e7f0901f7");
        track+=QByteArray::fromHex("00903c640a803c4000ff2f00");
        const QByteArray original=resetLayout(track,2);
        DocRoot doc; EditorState state; load(doc,state,original);
        CHECK(setupAndPackets(save(doc,state))==setupAndPackets(original));
        doc.trackList[0]->midiPatch=73; doc.trackList[0]->midiVolume=55; doc.trackList[0]->midiPanorama=32;
        const auto edited=setupAndPackets(save(doc,state));
        // The effective settings follow the last reset and precede the note.
        CHECK(edited.size()>=4);
        CHECK(edited[edited.size()-4].data==QByteArray::fromHex("f07e7f0901f7"));
        CHECK(edited[edited.size()-3].data==QByteArray::fromHex("b00737"));
        CHECK(edited[edited.size()-2].data==QByteArray::fromHex("b00a20"));
        CHECK(edited.last().data==QByteArray::fromHex("c04800"));
    }
    // A reset after the first note must not move its initial setup past that
    // note. The later reset still has its original effect on the second note.
    DocRoot doc; EditorState state;
    load(doc,state,resetLayout(QByteArray::fromHex(
        "00903c6400f0057e7f0901f70a903d640a803c4000803d4000ff2f00"),2));
    doc.trackList[0]->midiPatch=73; doc.trackList[0]->midiVolume=55; doc.trackList[0]->midiPanorama=32;
    QByteArray output=save(doc,state); QBuffer input(&output); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument saved(&input); CHECK(saved.load());
    int patch=-1,notes=0;
    for(auto* track : saved.trackList)for(auto* event : track->eventList) {
        if(event->isSysExEvent())patch=0;
        if(auto* midi=event->isMidiEvent()) {
            const int command=midi->midiCommand[0]&0xf0;
            if(command==0xc0)patch=midi->midiCommand[1];
            if(command==0x90 && midi->midiCommand[2]) { CHECK(patch==(notes==0 ? 72 : 0)); ++notes; }
        }
    }
    CHECK(notes==2);
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    checkRoundtrip(); checkImplicitReleaseVelocity(); checkClipboard(); checkComparator(); checkMixedComparator(); checkMixedRoundtripPermutations();
    checkInitialResetSetupOrder();
    checkResetMissingSetupAndMixedConductor();
    checkSetupBeforeLaterReset();
    std::puts("Source event order, repeated-note releases and legacy clipboard passed");
}
