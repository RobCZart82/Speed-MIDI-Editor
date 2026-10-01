#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "controller.h"
#include "commands.h"
#include "smfdocument.h"
#include "smfimporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include <QBuffer>
#include <QDataStream>
#include <QTemporaryDir>
#include <QUndoStack>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)

class LosslessTestApp : public SpeedyMidiApp
{
public:
    using SpeedyMidiApp::SpeedyMidiApp;
    void initialize()
    {
        settings=new Settings;
        settings->mainWindowGeometry.clear();
        settings->mainWindowState.clear();
        settings->mousePiano.visible=false;
        setupAppGlobalUi(); // Never initialize MIDI hardware.
    }
    ~LosslessTestApp() override { aboutToQuitCleanup(); }
};
class LosslessTestWindow : public MainWindow
{
public:
    Controller* editor() { return controller; }
};
static QByteArray smfBytes(const QList<QByteArray>& tracks,int ppqn=480)
{
    QByteArray bytes;
    QDataStream stream(&bytes,QIODevice::WriteOnly);
    stream.writeRawData("MThd",4);
    stream << quint32(6) << quint16(tracks.size()>1 ? 1:0) << quint16(tracks.size()) << quint16(ppqn);
    for(const QByteArray& track : tracks)
    {
        stream.writeRawData("MTrk",4);
        stream << quint32(track.size());
        stream.writeRawData(track.constData(),track.size());
    }
    return bytes;
}
static QByteArray saveDoc(const DocRoot& doc,const EditorState& editor)
{
    QByteArray bytes; QBuffer buffer(&bytes); CHECK(buffer.open(QIODevice::WriteOnly));
    CHECK(doc.save(&buffer,editor,false)); return bytes;
}
static void importBytes(QByteArray bytes,DocRoot& doc,EditorState& state)
{
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument smf(&input); CHECK(smf.load());
    SmfImporter importer(&doc,&smf,&state); CHECK(importer.doImport());
}
struct Packet { quint32 tick; quint8 type; QByteArray bytes; };
static QList<Packet> packets(const SmfTrack* track)
{
    QList<Packet> result;
    for(const SmfEvent* event : track->eventList)
        if(const SmfSysExEvent* packet=event->isSysExEvent())
            result.append({packet->tickPosition,packet->sysExType,
                           QByteArray(reinterpret_cast<const char*>(packet->data),packet->dataLength)});
    return result;
}
static void checkPacket(const Packet& packet,int tick,int type,const char* payload)
{
    CHECK(packet.tick==quint32(tick)); CHECK(packet.type==type);
    CHECK(packet.bytes==QByteArray::fromHex(payload));
}
static bool endpoint(const DocEvent* event)
{
    return event->type==DocEvent::E_Meta && event->metaEventData.metaEvent &&
            event->metaEventData.metaEvent->metaEventType==SMF_META_EVENT_TYPE_END_OF_TRACK;
}
static void checkPacketAndDurationRoundtrip()
{
    // Conductor packets and local packets remain on their original tracks.
    // Same-tick F0/F7 fragments must retain order and must not acquire extra F7 bytes.
    QByteArray bytes=smfBytes({QByteArray::fromHex("00f00243011ef70202f700ff2f00"),
                             QByteArray::fromHex("00f0027d0100f70202f700903c6450803c008310ff2f00")});
    for(int cycle=0;cycle<3;++cycle)
    {
        DocRoot doc; EditorState state; importBytes(bytes,doc,state);
        CHECK(doc.sysExEventList.size()==2); CHECK(doc.conductorEndTick==30);
        CHECK(doc.trackList.size()==1);
        int localPackets=0,endpoints=0;
        for(DocEvent* event=doc.trackList[0]->firstEvent;event;event=event->nextEvent)
        {
            if(event->type==DocEvent::E_SysEx)++localPackets;
            if(endpoint(event)) { ++endpoints; CHECK(event->tickPositionEnd()==480); }
        }
        CHECK(localPackets==2 && endpoints==1);
        CHECK(doc.getMaxTicks()==479);
        DocRoot copy; copy.makeDeepCopy(doc);
        CHECK(copy.sysExEventList[0]!=doc.sysExEventList[0]);
        CHECK(copy.sysExEventList[0]->data!=doc.sysExEventList[0]->data);
        bytes=saveDoc(copy,state);
        CHECK(bytes.count(QByteArray::fromHex("ff2f00"))==2);
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument saved(&input); CHECK(saved.load()); CHECK(saved.trackList.size()==2);
        CHECK(saved.trackList[0]->endTick==30 && saved.trackList[1]->endTick==480);
        auto global=packets(saved.trackList[0]),local=packets(saved.trackList[1]);
        CHECK(global.size()==2 && local.size()==2);
        checkPacket(global[0],0,0xf0,"4301"); checkPacket(global[1],30,0xf7,"02f7");
        checkPacket(local[0],0,0xf0,"7d01"); checkPacket(local[1],0,0xf7,"02f7");
    }
    // Format-0 conversion keeps packets in the conductor and its trailing endpoint.
    DocRoot doc; EditorState state;
    importBytes(smfBytes({QByteArray::fromHex("00f0017d14903c6450803c00822cff2f00")}),doc,state);
    CHECK(doc.sysExEventList.size()==1 && doc.conductorEndTick==400);
    CHECK(doc.trackList.size()==1);
    QByteArray saved=saveDoc(doc,state); QBuffer input(&saved); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument roundtrip(&input); CHECK(roundtrip.load());
    CHECK(roundtrip.trackList[0]->endTick==400 && roundtrip.trackList[1]->endTick==400);
    checkPacket(packets(roundtrip.trackList[0])[0],0,0xf0,"7d");
    // Resolution changes scale global packets and duration just like local events.
    doc.scaleTickResolution(3840);
    CHECK(doc.conductorEndTick==800);
    CHECK(doc.trackList[0]->firstEvent->tickPosition>=0);
    saved=saveDoc(doc,state); QBuffer scaled(&saved); CHECK(scaled.open(QIODevice::ReadOnly));
    SmfDocument scaledDoc(&scaled); CHECK(scaledDoc.load());
    CHECK(scaledDoc.trackList[0]->endTick==800 && scaledDoc.trackList[1]->endTick==800);
}
static void checkUnmatchedAndEmptyTracks()
{
    DocRoot doc; EditorState state;
    importBytes(smfBytes({QByteArray::fromHex("00ff2f00"),
                         QByteArray::fromHex("0a903c6440903d5028ff2f00"),
                         QByteArray::fromHex("8360ff2f00")}),doc,state);
    CHECK(doc.trackList.size()==2);
    int notes=0;
    for(DocEvent* event=doc.trackList[0]->firstEvent;event;event=event->nextEvent)
        if(event->type==DocEvent::E_Note)
        { ++notes; CHECK(event->tickPositionEnd()==114); }
    CHECK(notes==2);
    CHECK(endpoint(doc.trackList[1]->firstEvent));
    CHECK(doc.trackList[1]->firstEvent->tickPositionEnd()==480);
    QByteArray saved=saveDoc(doc,state); QBuffer input(&saved); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument roundtrip(&input); CHECK(roundtrip.load());
    CHECK(roundtrip.trackList[1]->endTick==114 && roundtrip.trackList[2]->endTick==480);
    // A note at EOT still has a supported positive duration.
    DocRoot atEnd; EditorState atEndState;
    importBytes(smfBytes({QByteArray::fromHex("00903c6400ff2f00")}),atEnd,atEndState);
    CHECK(atEnd.trackList[0]->firstEvent->tickLength==1);
    // Low-PPQN normalization also scales the original endpoint.
    DocRoot low; EditorState lowState;
    importBytes(smfBytes({QByteArray::fromHex("00903c6401803c0009ff2f00")},1),low,lowState);
    CHECK(low.conductorEndTick==4800);
    int lowNotes=0;
    for(DocEvent* event=low.trackList[0]->firstEvent;event;event=event->nextEvent)
        if(event->type==DocEvent::E_Note) { ++lowNotes; CHECK(event->tickPositionEnd()==480); }
    CHECK(lowNotes==1);
}
static void checkRealtimeRoundtrip()
{
    QByteArray bytes=smfBytes({QByteArray::fromHex("00903c640af80a3d500a803c00003d0000ff2f00")});
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument doc(&input); CHECK(doc.load()); CHECK(doc.trackList[0]->eventList.size()==5);
    QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
    SmfDocument writer(&output); writer.setMidiTicksPerWholeNote(1920);
    for(const SmfEvent* event : doc.trackList[0]->eventList)
    {
        CHECK(event->isMidiEvent());
        if(writer.trackList.isEmpty())writer.trackList.append(new SmfTrack);
        writer.trackList[0]->eventList.append(new SmfMidiEvent(*event->isMidiEvent()));
    }
    writer.trackList[0]->endTick=30;
    CHECK(writer.save()); CHECK(output.seek(0));
    SmfDocument reloaded(&output); CHECK(reloaded.load());
    CHECK(reloaded.trackList[0]->eventList.size()==5);
}
static QByteArray eventHeader(int type,int start,int end)
{
    QByteArray bytes; QDataStream writer(&bytes,QIODevice::WriteOnly);
    writer << type << start << end; return bytes;
}
static void rejectClipboard(const QByteArray& bytes)
{
    QDataStream stream(bytes); DocEvent event; event.deserialize(stream);
    CHECK(stream.status()!=QDataStream::Ok); CHECK(event.type==DocEvent::E_Invalid);
}
static void checkClipboardValidation()
{
    for(int type : {std::numeric_limits<int>::min(),-1,0,5,std::numeric_limits<int>::max()})
        rejectClipboard(eventHeader(type,0,1));
    for(auto interval : {qMakePair(-1,1),qMakePair(1,1),qMakePair(10,-10),qMakePair(0,std::numeric_limits<int>::min())})
        rejectClipboard(eventHeader(DocEvent::E_Note,interval.first,interval.second));
    for(int value : {-1,128})
    {
        QByteArray bytes=eventHeader(DocEvent::E_Note,0,1); QDataStream w(&bytes,QIODevice::Append);
        w << value << 64; rejectClipboard(bytes);
        QByteArray velocity=eventHeader(DocEvent::E_Note,0,1); QDataStream v(&velocity,QIODevice::Append);
        v << 60 << value; rejectClipboard(velocity);
    }
    QByteArray other=eventHeader(DocEvent::E_OtherMidi,0,1); QDataStream ow(&other,QIODevice::Append);
    ow << quint8(0xf0) << quint8(0) << quint8(0) << false << 0; rejectClipboard(other);
    for(int index : {1,2})
    {
        QByteArray bytes=eventHeader(DocEvent::E_OtherMidi,0,1); QDataStream w(&bytes,QIODevice::Append);
        w << quint8(0xb0) << quint8(index==1 ? 128:64) << quint8(index==2 ? 128:64) << false << 0;
        rejectClipboard(bytes);
    }
    for(int length : {-1,1000000000,2})
    {
        QByteArray bytes=eventHeader(DocEvent::E_SysEx,0,1); QDataStream w(&bytes,QIODevice::Append);
        w << true << quint8(0xf0) << length << qint64(0); w.writeRawData("x",1); rejectClipboard(bytes);
    }
    QByteArray nullPacket=eventHeader(DocEvent::E_SysEx,0,1); QDataStream n(&nullPacket,QIODevice::Append);
    n << false; rejectClipboard(nullPacket);
    QByteArray badType=eventHeader(DocEvent::E_SysEx,0,1); QDataStream b(&badType,QIODevice::Append);
    b << true << quint8(0x90) << 0 << qint64(0); rejectClipboard(badType);
    for(int size=0;size<12;++size)rejectClipboard(eventHeader(DocEvent::E_Note,0,1).left(size));
    // Empty continuation packets are valid opaque SMF data, including deep copy.
    QByteArray emptyPacket=eventHeader(DocEvent::E_SysEx,0,1); QDataStream e(&emptyPacket,QIODevice::Append);
    e << true << quint8(0xf7) << 0 << qint64(-1);
    QDataStream emptyReader(emptyPacket); DocEvent empty; empty.deserialize(emptyReader);
    CHECK(emptyReader.status()==QDataStream::Ok && empty.type==DocEvent::E_SysEx);
    DocEvent copy(empty); CHECK(copy.sysExEventData.sysExEvent->dataLength==0);
}
static void checkNativeUndoAndClipboard()
{
    DocRoot doc; EditorState state;
    importBytes(smfBytes({QByteArray::fromHex("00ff2f00"),
                         QByteArray::fromHex("0af0027d018356ff2f00")}),doc,state);
    DocEvent* packet=nullptr; DocEvent* end=nullptr;
    for(DocEvent* event=doc.trackList[0]->firstEvent;event;event=event->nextEvent)
    {
        if(event->type==DocEvent::E_SysEx)packet=event;
        if(endpoint(event))end=event;
    }
    CHECK(packet && end);
    CHECK(end->tickPositionEnd()==480);
    QByteArray clipboard; QDataStream writer(&clipboard,QIODevice::WriteOnly);
    packet->serialize(writer,0,480); QDataStream reader(clipboard); DocEvent pasted; pasted.deserialize(reader);
    CHECK(reader.status()==QDataStream::Ok && pasted.type==DocEvent::E_SysEx);
    CHECK(pasted.sysExEventData.sysExEvent!=packet->sysExEventData.sysExEvent);
    CHECK(!(*pasted.sysExEventData.sysExEvent!=*packet->sysExEventData.sysExEvent));
    // A selection ending exactly at EOT retains its hidden duration marker.
    QByteArray durationClipboard; QDataStream durationWriter(&durationClipboard,QIODevice::WriteOnly);
    end->serialize(durationWriter,0,480);
    QDataStream durationReader(durationClipboard); DocEvent pastedEnd; pastedEnd.deserialize(durationReader);
    CHECK(durationReader.status()==QDataStream::Ok && endpoint(&pastedEnd));
    CHECK(pastedEnd.tickPositionEnd()==480);
    CHECK(pastedEnd.metaEventData.metaEvent!=end->metaEventData.metaEvent);
    LosslessTestWindow window;
    QUndoStack undo;
    auto* shift=new Command_ShiftEvents(0,0,120); shift->connectToController(window.editor(),&doc);
    undo.push(shift); CHECK(packet->tickPosition==130 && end->tickPositionEnd()==600);
    undo.undo(); CHECK(packet->tickPosition==10 && end->tickPositionEnd()==480);
    undo.redo(); CHECK(end->tickPositionEnd()==600);
    DocEvent changed(*packet); changed.sysExEventData.sysExEvent->data[1]=0x42;
    auto* edit=new Command_EventProperties(0,packet,changed); edit->connectToController(window.editor(),&doc);
    undo.push(edit); CHECK(packet->sysExEventData.sysExEvent->data[1]==0x42);
    undo.undo(); CHECK(packet->sysExEventData.sysExEvent->data[1]==1);
    undo.redo(); CHECK(packet->sysExEventData.sysExEvent->data[1]==0x42);
    auto* remove=new Command_DeleteEvent(0,packet); remove->connectToController(window.editor(),&doc);
    undo.push(remove); CHECK(doc.trackList[0]->firstEvent==end);
    undo.undo(); CHECK(doc.trackList[0]->firstEvent==packet);
    undo.redo(); CHECK(doc.trackList[0]->firstEvent==end);
    undo.clear(); // Deleted opaque payload is released by the owning command.
    auto* insert=new Command_InsertEvent(0,new DocEvent(pasted)); insert->connectToController(window.editor(),&doc);
    undo.push(insert); CHECK(doc.trackList[0]->firstEvent->type==DocEvent::E_SysEx);
    undo.undo(); CHECK(doc.trackList[0]->firstEvent==end);
    undo.redo(); CHECK(doc.trackList[0]->firstEvent->type==DocEvent::E_SysEx);
    QByteArray saved=saveDoc(doc,state); QBuffer input(&saved); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument reloaded(&input); CHECK(reloaded.load()); CHECK(reloaded.trackList[1]->endTick==600);
}
int main(int argc,char** argv)
{
    QTemporaryDir temporary; CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temporary.path());
    LosslessTestApp app(argc,argv); app.initialize();
    checkPacketAndDurationRoundtrip(); checkUnmatchedAndEmptyTracks();
    checkRealtimeRoundtrip(); checkClipboardValidation(); checkNativeUndoAndClipboard();
    std::puts("Lossless packets, endpoints, unmatched notes, realtime, clipboard and native undo passed");
}
