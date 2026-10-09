#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controller.h"
#include "cs_playback.h"
#include "cs_localmassedit.h"
#include "measurepropertiesdialog.h"
#include "commands.h"
#include "smfdocument.h"
#include "smfimporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include "view.h"
#include <QPainter>
#include <QImage>
#include <QBuffer>
#include <QClipboard>
#include <QMimeData>
#include <QDataStream>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cmath>
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
    using MainWindow::loadFile;
    LosslessTestWindow()
    {
        setWindowState(Qt::WindowNoState); resize(1000,700);
        QMainWindow::show(); QApplication::processEvents();
    }
    DocRoot* document() { return docRoot; }
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
    return event && event->type==DocEvent::E_Meta && event->metaEventData.metaEvent &&
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
    packet->serialize(writer,0,480); writer << qint32(123456);
    QDataStream reader(clipboard); DocEvent pasted; pasted.deserialize(reader);
    qint32 sentinel=0; reader >> sentinel; CHECK(sentinel==123456 && reader.atEnd());
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
static void selectClipboardCells(LosslessTestWindow& window,int left,int right,bool wholeMeasures=false)
{
    EditorState state=window.editor()->getEditorState();
    state.selection.ticksLeft=left; state.selection.ticksRight=right;
    state.selection.trackTop=0; state.selection.trackBottom=wholeMeasures ? INT_MAX : 0;
    state.selection.anchor.setTo(left,window.document()->roundUpTicksToCellBorder(left+1,state.writeLength),0);
    state.trackStateList[0].recordingEnabled=true;
    CHECK(state.isValid(window.document())); window.editor()->csApplyStateAndUpdate(state);
}
static DocEvent* findEvent(DocTrack* track,DocEvent::EventType type,int tick)
{
    for(DocEvent* event=track->firstEvent;event;event=event->nextEvent)
        if(event->type==type && event->tickPosition==tick)return event;
    return nullptr;
}
static int totalEvents(DocTrack* track)
{
    int count=0; for(DocEvent* event=track->firstEvent;event;event=event->nextEvent)++count; return count;
}
static void inspectClipboardPayload(const QByteArray& bytes,bool modern)
{
    QDataStream reader(bytes); int mode,tracks,ticks,resolution,count;
    reader >> mode >> tracks >> ticks >> resolution;
    CHECK(mode==S_LocalCells && tracks==1 && ticks==480 && resolution==1920);
    DocTrack track; track.deserialize(reader); EditorTrackState state; state.deserialize(reader); reader >> count;
    CHECK(count==(modern ? 4:3)); int packets=0,notes=0;
    for(int i=0;i<count;++i)
    {
        const qint64 position=reader.device()->pos(); int rawType=0; reader >> rawType;
        CHECK(modern ? rawType<0 : rawType>0); CHECK(reader.device()->seek(position));
        DocEvent event; event.deserialize(reader); CHECK(reader.status()==QDataStream::Ok);
        if(event.type==DocEvent::E_SysEx)++packets;
        if(event.type==DocEvent::E_Note)
        {
            ++notes; CHECK(event.noteEventData.releaseVelocity==(modern ? 17:64));
            CHECK(event.noteEventData.importOnOrder==(modern ? 2:-1));
            CHECK(event.noteEventData.importOffOrder==(modern ? 5:-1));
        }
    }
    CHECK(packets==(modern ? 1:0) && notes==1 && reader.atEnd());
}
static void setClipboardBytes(const QByteArray& modern,const QByteArray& legacy=QByteArray())
{
    auto* mime=new QMimeData;
    if(!modern.isNull())mime->setData("application/speedymidi-v2",modern);
    if(!legacy.isNull())mime->setData("application/speedymidi",legacy);
    QApplication::clipboard()->setMimeData(mime);
}
static void checkActualClipboardActions()
{
    LosslessTestWindow window; DocRoot* doc=window.document(); DocTrack* track=doc->trackList[0];
    CHECK(!track->firstEvent); track->midiChannel=1;
    auto* note=new DocEvent; note->type=DocEvent::E_Note; note->tickPosition=10; note->tickLength=20;
    note->noteEventData.noteNumber=60; note->noteEventData.velocity=90;
    note->noteEventData.importOnOrder=2; note->noteEventData.importOffOrder=5; note->noteEventData.releaseVelocity=17;
    track->insertEvent(note);
    auto* cc=new DocEvent; cc->type=DocEvent::E_OtherMidi; cc->tickPosition=10; cc->tickLength=1;
    cc->otherMidiEventData.midiCommand[0]=0xb0; cc->otherMidiEventData.midiCommand[1]=64; cc->otherMidiEventData.midiCommand[2]=127;
    cc->otherMidiEventData.importOrder=0; cc->otherMidiEventData.sameTickSubOrdering.index=0;
    track->insertEvent(cc);
    auto* packet=new DocEvent; packet->type=DocEvent::E_SysEx; packet->tickPosition=10; packet->tickLength=1;
    packet->sysExEventData.sysExEvent=new SmfSysExEvent;
    auto* opaque=packet->sysExEventData.sysExEvent; opaque->sysExType=0xf0; opaque->importOrder=1;
    opaque->dataLength=3; opaque->data=new quint8[3]{0x7d,1,0xf7}; track->insertEvent(packet);
    auto* end=new DocEvent; end->type=DocEvent::E_Meta; end->tickPosition=479; end->tickLength=1;
    end->metaEventData.metaEvent=new SmfMetaEvent; end->metaEventData.metaEvent->tickPosition=0;
    end->metaEventData.metaEvent->metaEventType=SMF_META_EVENT_TYPE_END_OF_TRACK; track->insertEvent(end);
    selectClipboardCells(window,0,480);
    window.getUI()->actionEdit_Copy->trigger();
    const QMimeData* copied=QApplication::clipboard()->mimeData();
    CHECK(copied->hasFormat("application/speedymidi-v2") && copied->hasFormat("application/speedymidi"));
    const QByteArray modern=copied->data("application/speedymidi-v2"),legacy=copied->data("application/speedymidi");
    inspectClipboardPayload(modern,true); inspectClipboardPayload(legacy,false);
    // v2 is preferred even when a valid, deliberately simpler legacy fallback exists.
    selectClipboardCells(window,960,1440); window.getUI()->actionEdit_Paste->trigger();
    CHECK(totalEvents(track)==8);
    auto* pasted=findEvent(track,DocEvent::E_Note,970); CHECK(pasted && pasted->noteEventData.releaseVelocity==17);
    CHECK(pasted->noteEventData.importOnOrder==2 && pasted->noteEventData.importOffOrder==5);
    CHECK(findEvent(track,DocEvent::E_OtherMidi,970)->otherMidiEventData.importOrder==0);
    CHECK(findEvent(track,DocEvent::E_SysEx,970)->sysExEventData.sysExEvent->importOrder==1);
    CHECK(endpoint(findEvent(track,DocEvent::E_Meta,1439)));
    window.getUI()->actionEdit_Undo->trigger(); CHECK(totalEvents(track)==4);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(totalEvents(track)==8);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(totalEvents(track)==4);
    // A clipboard from an old application remains readable and omits opaque data safely.
    setClipboardBytes(QByteArray(),legacy); selectClipboardCells(window,1920,2400);
    window.getUI()->actionEdit_Paste->trigger(); CHECK(totalEvents(track)==7);
    pasted=findEvent(track,DocEvent::E_Note,1930); CHECK(pasted && pasted->noteEventData.releaseVelocity==64);
    CHECK(pasted->noteEventData.importOnOrder==-1 && !findEvent(track,DocEvent::E_SysEx,1930));
    // Corrupt preferred v2 data must not mutate the document or silently use fallback data.
    for(int rawMode : {-1,99,std::numeric_limits<int>::min()})
    {
        QByteArray corrupt; QDataStream out(&corrupt,QIODevice::WriteOnly); out << rawMode;
        const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
        setClipboardBytes(corrupt,legacy); window.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
    }
    // Source intervals, resolution conversion and destination addition must
    // all fit the model before a paste changes the document or undo history.
    for(int boundary=0;boundary<5;++boundary)
    {
        EditorState state=window.editor()->getEditorState();
        if(boundary==1)state.setGlobalTrackSelection(0,1,doc);
        else
        {
            const int left=960;
            state.selection.ticksLeft=left;
            state.selection.ticksRight=doc->roundUpTicksToCellBorder(left+1,state.writeLength);
            state.selection.trackTop=state.selection.trackBottom=0;
            state.selection.anchor.setTo(left,state.selection.ticksRight,0);
        }
        CHECK(state.isValid(doc)); window.editor()->csApplyStateAndUpdate(state);
        QByteArray bytes; QDataStream out(&bytes,QIODevice::WriteOnly);
        const int mode=boundary==1 ? S_GlobalTrack : S_LocalCells;
        const int range=boundary==1 ? -1 : (boundary==0 || boundary==2 ? INT_MAX : 96);
        const int resolution=boundary<2 ? 1 : doc->midiTicksPerWholeNote;
        out << mode << 1 << range << resolution;
        track->serialize(out); state.trackStateList[0].serialize(out); out << 1;
        if(boundary==3)
        {
            out << -int(DocEvent::E_OtherMidi)-1 << 0 << 96;
            DocEvent::OtherMidiEvent cc; cc.midiCommand[0]=0xb0; cc.midiCommand[1]=1; cc.midiCommand[2]=255;
            cc.serialize(out); out << qint64(-1);
        }
        else
        {
            out << -int(DocEvent::E_Note)-1 << 1 << (boundary==1 ? INT_MAX : (boundary==4 ? 120 : 2));
            out << 60 << 90 << qint64(-1) << qint64(-1) << 64;
        }
        const QByteArray before=saveDoc(*doc,state);
        const bool canUndo=window.getUI()->actionEdit_Undo->isEnabled();
        const bool canRedo=window.getUI()->actionEdit_Redo->isEnabled();
        setClipboardBytes(bytes); window.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==canUndo);
        CHECK(window.getUI()->actionEdit_Redo->isEnabled()==canRedo);
    }
    // Malformed active measure fields are rejected in staging, before division or state mutation.
    selectClipboardCells(window,0,1920,true);
    for(int badField=0;badField<5;++badField)
    {
        DocMeasureItem measure; measure.setFirstMeasureItemDefaults();
        if(badField==0)measure.timeSignatureDenominator=0;
        if(badField==1)measure.timeSignatureDenominator=3;
        if(badField==2) { measure.BPM=0; measure.microsecondsPerQuarter=0; }
        if(badField==3) { measure.setPlaybackOptions=true; measure.swingHardness=INT_MAX; }
        if(badField==4) { measure.setTimeSignature=false; measure.timeSignatureDenominator=0; }
        QByteArray corrupt; QDataStream out(&corrupt,QIODevice::WriteOnly);
        out << int(S_GlobalMeasure) << 1 << 1 << 1920 << 1920 << 1;
        measure.serialize(out,0);
        const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
        setClipboardBytes(corrupt); window.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
    }
    // Effective first properties copied away from tick zero now serialize at zero.
    selectClipboardCells(window,1920,3840,true); window.getUI()->actionEdit_Copy->trigger();
    QDataStream global(QApplication::clipboard()->mimeData()->data("application/speedymidi-v2"));
    int mode,measures,tracks,ticks,resolution,count; global >> mode >> measures >> tracks >> ticks >> resolution >> count;
    CHECK(mode==S_GlobalMeasure && count>=1); DocMeasureItem first; first.deserialize(global);
    CHECK(global.status()==QDataStream::Ok && first.tickPosition==0 && first.isValidFirstMeasureItem());
    // Legacy effective-first payloads can have unset flags and a negative tick.
    // Their valid effective values remain compatible after normalization.
    QByteArray oldGlobal=QApplication::clipboard()->mimeData()->data("application/speedymidi");
    CHECK(oldGlobal.size()>43); oldGlobal[28]=0; oldGlobal[33]=0; oldGlobal[42]=0;
    QBuffer oldBuffer(&oldGlobal); CHECK(oldBuffer.open(QIODevice::ReadWrite)); CHECK(oldBuffer.seek(24));
    QDataStream oldWriter(&oldBuffer); oldWriter << qint32(-1920);
    setClipboardBytes(QByteArray(),oldGlobal); selectClipboardCells(window,3840,5760,true);
    window.getUI()->actionEdit_Paste->trigger(); CHECK(findEvent(track,DocEvent::E_Note,3850));
    // Small source resolutions remain supported when the scaled interval fits.
    LosslessTestWindow small; auto* smallTrack=small.document()->trackList[0];
    const EditorState smallState=small.editor()->getEditorState();
    QByteArray lowResolution; QDataStream lowOut(&lowResolution,QIODevice::WriteOnly);
    lowOut << int(S_LocalCells) << 1 << 2 << 1;
    smallTrack->serialize(lowOut); smallState.trackStateList[0].serialize(lowOut); lowOut << 4;
    lowOut << -int(DocEvent::E_Note)-1 << 0 << 2 << 60 << 90 << qint64(-1) << qint64(-1) << 64;
    DocEvent ccSource; ccSource.type=DocEvent::E_OtherMidi; ccSource.tickPosition=1; ccSource.tickLength=1;
    ccSource.otherMidiEventData.midiCommand[0]=0xb0;
    ccSource.otherMidiEventData.midiCommand[1]=1; ccSource.otherMidiEventData.midiCommand[2]=2;
    ccSource.serialize(lowOut,0,2);
    DocEvent packetSource; packetSource.type=DocEvent::E_SysEx; packetSource.tickPosition=1; packetSource.tickLength=1;
    packetSource.sysExEventData.sysExEvent=new SmfSysExEvent;
    packetSource.sysExEventData.sysExEvent->sysExType=0xf0;
    packetSource.sysExEventData.sysExEvent->dataLength=2;
    packetSource.sysExEventData.sysExEvent->data=new quint8[2]{0x7d,0xf7};
    packetSource.serialize(lowOut,0,2);
    DocEvent endSource; endSource.type=DocEvent::E_Meta; endSource.tickPosition=1; endSource.tickLength=1;
    endSource.metaEventData.metaEvent=new SmfMetaEvent;
    endSource.metaEventData.metaEvent->metaEventType=SMF_META_EVENT_TYPE_END_OF_TRACK;
    endSource.serialize(lowOut,0,2);
    for(DocEvent* oldEvent : {&ccSource,&packetSource,&endSource})
    {
        oldEvent->tickLength=20;
        QByteArray oldBytes; QDataStream oldOut(&oldBytes,QIODevice::WriteOnly);
        oldEvent->serialize(oldOut,0,30);
        QDataStream oldIn(oldBytes); DocEvent restored; restored.deserialize(oldIn);
        CHECK(oldIn.status()==QDataStream::Ok && restored.tickLength==1);
        CHECK(restored.tickPosition==(oldEvent==&endSource ? 20:1));
        oldEvent->tickLength=1;
    }
    setClipboardBytes(lowResolution); small.getUI()->actionEdit_Paste->trigger();
    const int unit=small.document()->midiTicksPerWholeNote;
    CHECK(totalEvents(smallTrack)==4);
    CHECK(findEvent(smallTrack,DocEvent::E_Note,0)->tickLength==2*unit);
    CHECK(findEvent(smallTrack,DocEvent::E_OtherMidi,unit)->tickLength==1);
    CHECK(findEvent(smallTrack,DocEvent::E_SysEx,unit)->tickLength==1);
    CHECK(endpoint(findEvent(smallTrack,DocEvent::E_Meta,2*unit-1)));
    selectClipboardCells(small,0,2*unit); small.getUI()->actionEdit_Copy->trigger();
    selectClipboardCells(small,2*unit,4*unit); small.getUI()->actionEdit_Paste->trigger();
    CHECK(totalEvents(smallTrack)==8);
    CHECK(findEvent(smallTrack,DocEvent::E_OtherMidi,3*unit)->tickLength==1);
    CHECK(findEvent(smallTrack,DocEvent::E_SysEx,3*unit)->tickLength==1);
    CHECK(endpoint(findEvent(smallTrack,DocEvent::E_Meta,4*unit-1)));
    selectClipboardCells(small,4*unit,8*unit);
    small.getUI()->actionEdit_PasteScaleToSelection->trigger();
    CHECK(totalEvents(smallTrack)==12);
    CHECK(findEvent(smallTrack,DocEvent::E_Note,4*unit)->tickPositionEnd()==8*unit);
    CHECK(findEvent(smallTrack,DocEvent::E_OtherMidi,6*unit)->tickLength==1);
    CHECK(findEvent(smallTrack,DocEvent::E_SysEx,6*unit)->tickLength==1);
    CHECK(endpoint(findEvent(smallTrack,DocEvent::E_Meta,8*unit-1)));
}

using TempoEvents=QList<QPair<int,int>>;
static TempoEvents savedTempos(const DocRoot& doc,const EditorState& state)
{
    QByteArray bytes=saveDoc(doc,state);
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument smf(&input); CHECK(smf.load());
    TempoEvents result;
    for(const auto* track : smf.trackList)
        for(const auto* event : track->eventList)
            if(const auto* meta=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TEMPO))
            {
                CHECK(meta->dataLength==3);
                const int tempo=(int(meta->data[0])<<16)|(int(meta->data[1])<<8)|int(meta->data[2]);
                result.append(qMakePair(int(meta->tickPosition),tempo));
            }
    return result;
}
static TempoEvents savedTempos(LosslessTestWindow& window)
{
    return savedTempos(*window.document(),window.editor()->getEditorState());
}
static void appendExactTempo(DocRoot* doc,int tick,int tempo,QList<int> preceding=QList<int>())
{
    auto* item=new DocMeasureItem;
    item->tickPosition=tick;
    item->setTempo=true;
    item->BPM=120; // Distinct raw tempos deliberately share the old display cache.
    item->microsecondsPerQuarter=tempo;
    item->precedingTempoValues=preceding;
    doc->measureItemList.append(item);
}
static void prepareExactTempoDocument(LosslessTestWindow& window)
{
    DocRoot* doc=window.document();
    doc->measureItemList[0]->microsecondsPerQuarter=500001;
    doc->measureItemList[0]->precedingTempoValues={500009};
    appendExactTempo(doc,240,500002,{500007});
    appendExactTempo(doc,720,600007);
    appendExactTempo(doc,2160,486003);
    auto* note=new DocEvent;
    note->type=DocEvent::E_Note;
    note->tickPosition=10;
    note->tickLength=8000;
    note->noteEventData.noteNumber=60;
    note->noteEventData.velocity=80;
    note->noteEventData.midiKeypressSerialNo=0;
    doc->trackList[0]->insertEvent(note);
}
static void setExactClipboard(const QByteArray& bytes)
{
    auto* mime=new QMimeData;
    mime->setData("application/speedymidi-v3",bytes);
    QApplication::clipboard()->setMimeData(mime);
}
static void checkExactTempoClipboard()
{
    LosslessTestWindow window;
    prepareExactTempoDocument(window);
    const TempoEvents original=savedTempos(window);
    CHECK(original==TempoEvents({{0,500009},{0,500001},{240,500007},{240,500002},{720,600007},{2160,486003}}));
    selectClipboardCells(window,0,1920,true);
    window.getUI()->actionEdit_Copy->trigger();
    const QMimeData* mime=QApplication::clipboard()->mimeData();
    CHECK(mime->hasFormat("application/speedymidi-v3"));
    const QByteArray exact=mime->data("application/speedymidi-v3");
    QDataStream reader(exact);
    int version,mode,measures,tracks,ticks,resolution,count;
    reader >> version >> mode >> measures >> tracks >> ticks >> resolution >> count;
    CHECK(version==-3 && mode==S_GlobalMeasure && measures==1 && tracks==1);
    CHECK(ticks==1920 && resolution==1920 && count==3);
    DocMeasureItem first; first.deserialize(reader,true);
    CHECK(reader.status()==QDataStream::Ok && first.microsecondsPerQuarter==500001);
    CHECK(first.precedingTempoValues==QList<int>({500009}));
    setExactClipboard(exact);
    selectClipboardCells(window,3840,5760,true);
    window.getUI()->actionEdit_Paste->trigger();
    const TempoEvents pasted=savedTempos(window);
    CHECK(pasted==TempoEvents({{0,500009},{0,500001},{240,500007},{240,500002},{720,600007},
                              {2160,486003},{3840,500009},{3840,500001},{4080,500007},{4080,500002},
                              {4560,600007},{5760,486003}}));
    CHECK(window.document()->measureToTicks(3)==5760);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==original);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==pasted);
    QByteArray roundtrip=saveDoc(*window.document(),window.editor()->getEditorState());
    for(int cycle=0;cycle<3;++cycle)
    {
        DocRoot restored; EditorState state;
        importBytes(roundtrip,restored,state);
        CHECK(savedTempos(restored,state)==pasted);
        roundtrip=saveDoc(restored,state);
    }

    // Rejected exact extensions must not mutate the document or undo state.
    for(int badField=0;badField<3;++badField)
    {
        DocMeasureItem bad; bad.setFirstMeasureItemDefaults();
        if(badField==0) { bad.microsecondsPerQuarter=0; bad.BPM=0; }
        if(badField==1)bad.microsecondsPerQuarter=0x1000000;
        if(badField==2)bad.precedingTempoValues={0};
        QByteArray corrupt; QDataStream out(&corrupt,QIODevice::WriteOnly);
        out << -3 << int(S_GlobalMeasure) << 1 << 1 << 1920 << 1920 << 1;
        bad.serialize(out,0,true);
        const QByteArray before=saveDoc(*window.document(),window.editor()->getEditorState());
        const bool canUndo=window.getUI()->actionEdit_Undo->isEnabled();
        const bool canRedo=window.getUI()->actionEdit_Redo->isEnabled();
        setExactClipboard(corrupt); window.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*window.document(),window.editor()->getEditorState())==before);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==canUndo);
        CHECK(window.getUI()->actionEdit_Redo->isEnabled()==canRedo);
    }
}
static void checkExactTempoResolutionClipboard()
{
    LosslessTestWindow window;
    prepareExactTempoDocument(window);
    selectClipboardCells(window,0,1920,true);
    window.getUI()->actionEdit_Copy->trigger();
    const QByteArray exact=QApplication::clipboard()->mimeData()->data("application/speedymidi-v3");
    CHECK(!exact.isEmpty());
    window.document()->scaleTickResolution(3840);
    const TempoEvents scaled=savedTempos(window);
    CHECK(scaled==TempoEvents({{0,500009},{0,500001},{480,500007},{480,500002},{1440,600007},{4320,486003}}));
    setExactClipboard(exact);
    selectClipboardCells(window,7680,11520,true);
    window.getUI()->actionEdit_Paste->trigger();
    const TempoEvents pasted=savedTempos(window);
    CHECK(pasted.contains(qMakePair(8160,500002)) && pasted.contains(qMakePair(9120,600007)));
    CHECK(pasted.contains(qMakePair(11520,486003)));
    CHECK(window.document()->measureToTicks(3)==11520);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==scaled);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==pasted);
    window.getUI()->actionEdit_Undo->trigger();
    window.document()->scaleTickResolution(1920);
    CHECK(savedTempos(window)==TempoEvents({{0,500009},{0,500001},{240,500007},{240,500002},{720,600007},{2160,486003}}));
}
static void checkClipboardMeterAfterResolutionScaling()
{
    LosslessTestWindow window;
    selectClipboardCells(window,0,1920,true);
    QByteArray bytes; QDataStream out(&bytes,QIODevice::WriteOnly);
    out << -3 << int(S_GlobalMeasure) << 2 << 1 << 2880 << 1924 << 2;
    DocMeasureItem first; first.setFirstMeasureItemDefaults();
    first.timeSignatureNominator=4; first.timeSignatureDenominator=8;
    first.serialize(out,0,true);
    DocMeasureItem meter; meter.tickPosition=960; meter.setTimeSignature=true;
    meter.timeSignatureNominator=8; meter.timeSignatureDenominator=8;
    meter.serialize(out,0,true);
    window.document()->trackList[0]->serialize(out);
    window.editor()->getEditorState().trackStateList[0].serialize(out);
    out << 0;
    const QByteArray before=saveDoc(*window.document(),window.editor()->getEditorState());
    const bool undo=window.getUI()->actionEdit_Undo->isEnabled();
    const bool redo=window.getUI()->actionEdit_Redo->isEnabled();
    setExactClipboard(bytes);
    window.getUI()->actionEdit_Paste->trigger();
    CHECK(saveDoc(*window.document(),window.editor()->getEditorState())==before);
    CHECK(window.getUI()->actionEdit_Undo->isEnabled()==undo);
    CHECK(window.getUI()->actionEdit_Redo->isEnabled()==redo);
}

static void checkRebarRangeValidation()
{
    const auto fillLongTrackGaps=[](DocRoot* doc) {
        // SMF deltas are limited to 28 bits. Opaque metadata makes both the
        // original and transformed long fixtures serializable without adding
        // another shiftable conductor event that could mask a note-only bug.
        for(qint64 tick=200000000;tick<=2000000000;tick+=200000000) {
            auto* meta=new SmfMetaEvent;
            meta->tickPosition=tick; meta->metaEventType=SMF_META_EVENT_TYPE_SEQUENCER_SPECIFIC;
            meta->dataFromString(QStringLiteral("long-file gap"));
            doc->metaEventList.append(meta);
            doc->trackList[0]->metaEventList.append(new SmfMetaEvent(*meta));
        }
    };
    // Both conductor-only rebars and note rebars must be rejected atomically.
    // A finite rebar region can also overflow later events via its tail shift.
    for(int scenario : {0,1,2,3,4}) {
        LosslessTestWindow window;
        DocRoot* doc=window.document(); doc->midiTicksPerWholeNote=4*32767;
        const int oldLength=doc->ticksPerMeasure(doc->getFirstMeasureEffectiveProperties());
        const bool finite=scenario==2;
        const int far=finite ? 2140000000 : 1200000000;
        if(finite) {
            auto* meter=new DocMeasureItem;
            meter->tickPosition=100*oldLength; meter->setTimeSignature=true;
            meter->timeSignatureNominator=4; meter->timeSignatureDenominator=4;
            doc->measureItemList.append(meter);
        }
        if(scenario==1) {
            auto* note=new DocEvent; note->type=DocEvent::E_Note;
            note->tickPosition=far-10; note->tickLength=10;
            note->noteEventData.noteNumber=60; note->noteEventData.velocity=100;
            doc->trackList[0]->insertEvent(note);
        } else if(scenario!=4)appendExactTempo(doc,far,500001);
        fillLongTrackGaps(doc);
        selectClipboardCells(window,0,scenario==4 ? 16000*oldLength : oldLength,true);
        const EditorState state=window.editor()->getEditorState(); CHECK(state.isValid(doc));
        const QByteArray before=saveDoc(*doc,state);
        const bool undo=window.getUI()->actionEdit_Undo->isEnabled();
        const bool redo=window.getUI()->actionEdit_Redo->isEnabled();
        auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
        CHECK(editor);
        DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties();
        changed.timeSignatureNominator=8; changed.setTimeSignature=true;
        CHECK(!editor->setMeasureProperties(0,changed,oldLength,2*oldLength,scenario!=3));
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
        CHECK(window.editor()->getEditorState()==state);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==undo);
        CHECK(window.getUI()->actionEdit_Redo->isEnabled()==redo);
    }
    // A large but representable transformation must still work, including undo/redo.
    LosslessTestWindow window;
    DocRoot* doc=window.document(); doc->midiTicksPerWholeNote=4*32767;
    const int oldLength=doc->ticksPerMeasure(doc->getFirstMeasureEffectiveProperties());
    const int far=7600*oldLength; appendExactTempo(doc,far,500001);
    fillLongTrackGaps(doc);
    selectClipboardCells(window,0,oldLength,true);
    const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties();
    changed.timeSignatureNominator=8; changed.setTimeSignature=true;
    CHECK(editor->setMeasureProperties(0,changed,oldLength,2*oldLength,true));
    CHECK(savedTempos(window).contains(qMakePair(2*far,500001)));
    CHECK(window.editor()->getEditorState().isValid(doc));
    const QByteArray after=saveDoc(*doc,window.editor()->getEditorState());
    window.getUI()->actionEdit_Undo->trigger();
    CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
    window.getUI()->actionEdit_Redo->trigger();
    CHECK(saveDoc(*doc,window.editor()->getEditorState())==after);
}

static void checkExactTempoMeasureActions()
{
    LosslessTestWindow window;
    prepareExactTempoDocument(window);
    const TempoEvents original=savedTempos(window);
    selectClipboardCells(window,0,1920,true);
    window.getUI()->actionEdit_InsertSelectedRange->trigger();
    const TempoEvents inserted=savedTempos(window);
    CHECK(inserted.contains(qMakePair(2160,500007)) && inserted.contains(qMakePair(2160,500002)));
    CHECK(inserted.contains(qMakePair(2640,600007)) && inserted.contains(qMakePair(4080,486003)));
    CHECK(!inserted.contains(qMakePair(240,500002)));
    CHECK(window.document()->measureToTicks(1)==1920);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==original);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==inserted);
    window.getUI()->actionEdit_Undo->trigger();
    selectClipboardCells(window,0,1920,true);
    window.getUI()->actionEdit_Delete->trigger();
    const TempoEvents deleted=savedTempos(window);
    CHECK(deleted.contains(qMakePair(0,600007)) && deleted.contains(qMakePair(240,486003)));
    CHECK(!deleted.contains(qMakePair(240,500002)));
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==original);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==deleted);
    window.getUI()->actionEdit_Undo->trigger();

    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    CHECK(editor);
    DocMeasureItem changed=window.document()->getFirstMeasureEffectiveProperties();
    changed.timeSignatureNominator=3;
    changed.setTimeSignature=true;
    editor->setMeasureProperties(0,changed,1920,1440,true);
    const TempoEvents rebarred=savedTempos(window);
    CHECK(rebarred.contains(qMakePair(240,500007)) && rebarred.contains(qMakePair(240,500002)));
    CHECK(rebarred.contains(qMakePair(720,600007)) && rebarred.contains(qMakePair(1680,486003)));
    CHECK(window.document()->measureToTicks(1)==1440);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==original);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==rebarred);

    // Shrinking a bar can merge an off-bar tempo into the following bar's
    // tempo. Keep all source values in order and restore their ticks on undo.
    LosslessTestWindow collision;
    collision.document()->measureItemList[0]->microsecondsPerQuarter=500001;
    appendExactTempo(collision.document(),1800,500002,{500007});
    appendExactTempo(collision.document(),1920,600007);
    appendExactTempo(collision.document(),2160,486003);
    selectClipboardCells(collision,0,1920,true);
    const TempoEvents beforeCollision=savedTempos(collision);
    auto* collisionEditor=qobject_cast<CS_LocalMassEdit*>(collision.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    CHECK(collisionEditor);
    changed=collision.document()->getFirstMeasureEffectiveProperties();
    changed.timeSignatureNominator=3;
    collisionEditor->setMeasureProperties(0,changed,1920,1440,true);
    const TempoEvents collided=savedTempos(collision);
    CHECK(collided==TempoEvents({{0,500001},{1440,500007},{1440,500002},{1440,600007},{1680,486003}}));
    collision.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(collision)==beforeCollision);
    collision.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(collision)==collided);
}

class TempoPropertiesTestDialog : public MeasurePropertiesDialog
{
public:
    TempoPropertiesTestDialog(CS_LocalMassEdit* editor,int measure) : MeasurePropertiesDialog(editor,measure)
    {
        setGlobalMeasureSelection(measure);
        retrieveMeasureProperties(measure);
        CHECK(updateData(false));
        enableAndDisable();
    }
    void apply()
    {
        auto* buttons=findChild<QDialogButtonBox*>("buttonBox");
        CHECK(buttons && buttons->button(QDialogButtonBox::Apply));
        buttons->button(QDialogButtonBox::Apply)->click();
    }
};
static void checkRebarDialogRejection()
{
    LosslessTestWindow window;
    DocRoot* doc=window.document(); doc->midiTicksPerWholeNote=4*32767;
    for(int tick=200000000;tick<=1200000000;tick+=200000000)
        appendExactTempo(doc,tick,500001);
    selectClipboardCells(window,0,doc->ticksPerMeasure(doc->getFirstMeasureEffectiveProperties()),true);
    const TempoEvents before=savedTempos(window);
    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    TempoPropertiesTestDialog dialog(editor,0);
    auto* numerator=dialog.findChild<QSpinBox*>("spinBoxTimeSignatureNominator"); CHECK(numerator);
    numerator->setValue(8);
    int prompts=0,warnings=0;
    QTimer responder;
    QObject::connect(&responder,&QTimer::timeout,[&]() {
        auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if(!message)return;
        if(message->standardButtons() & QMessageBox::Yes) { ++prompts; message->done(QMessageBox::Yes); }
        else { ++warnings; message->accept(); }
    });
    responder.start(10); dialog.apply(); responder.stop();
    CHECK(prompts==1 && warnings==1);
    CHECK(savedTempos(window)==before);
    CHECK(doc->getFirstMeasureEffectiveProperties().timeSignatureNominator==4);
    CHECK(!window.getUI()->actionEdit_Undo->isEnabled());
}

static void checkExactTempoPropertiesDialog()
{
    LosslessTestWindow window;
    prepareExactTempoDocument(window);
    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    CHECK(editor);
    const TempoEvents original=savedTempos(window);
    TempoPropertiesTestDialog dialog(editor,0);
    auto* bpm=dialog.findChild<QDoubleSpinBox*>("spinBoxBPM");
    auto* key=dialog.findChild<QComboBox*>("comboBoxKeySignatureMajor");
    auto* numerator=dialog.findChild<QSpinBox*>("spinBoxTimeSignatureNominator");
    auto* denominator=dialog.findChild<QComboBox*>("comboBoxTimeSignatureDenominator");
    CHECK(bpm && key && numerator && denominator);
    CHECK(bpm->value()>119.999 && bpm->value()<120.0);
    CHECK(bpm->minimum()>=60000000.0 / 0xffffff);
    dialog.apply(); dialog.apply();
    CHECK(savedTempos(window)==original);
    key->setCurrentIndex(key->currentIndex()+1);
    dialog.apply();
    CHECK(savedTempos(window)==original);
    // 8/8 keeps the bar length unchanged and requires no rebar prompt.
    numerator->setValue(8); denominator->setCurrentIndex(3);
    CHECK(bpm->value()>239.99 && bpm->value()<240.0);
    dialog.apply();
    CHECK(savedTempos(window)==original);
    CHECK(window.document()->getFirstMeasureEffectiveProperties().timeSignatureDenominator==8);
    bpm->setValue(137.25);
    dialog.apply();
    CHECK(window.document()->measureItemList[0]->tempoMicrosecondsPerQuarter(8)==874317);
    CHECK(window.document()->measureItemList[0]->precedingTempoValues.isEmpty());
    const TempoEvents edited=savedTempos(window);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(window)==original);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedTempos(window)==edited);

    LosslessTestWindow inherit;
    inherit.document()->measureItemList[0]->microsecondsPerQuarter=500001;
    appendExactTempo(inherit.document(),720,600007);
    appendExactTempo(inherit.document(),1920,486003);
    auto* inheritEditor=qobject_cast<CS_LocalMassEdit*>(inherit.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    CHECK(inheritEditor);
    selectClipboardCells(inherit,1920,3840,true);
    const TempoEvents beforeDisable=savedTempos(inherit);
    TempoPropertiesTestDialog inheritedDialog(inheritEditor,1);
    auto* setTempo=inheritedDialog.findChild<QGroupBox*>("groupBoxSetTempo");
    CHECK(setTempo && setTempo->isChecked());
    setTempo->setChecked(false);
    inheritedDialog.apply();
    CHECK(inherit.document()->ticksToMeasure(1920).measureProperties.tempoMicrosecondsPerQuarter(4)==600007);
    CHECK(!savedTempos(inherit).contains(qMakePair(1920,486003)));
    inherit.getUI()->actionEdit_Undo->trigger(); CHECK(savedTempos(inherit)==beforeDisable);
    inherit.getUI()->actionEdit_Redo->trigger();
    CHECK(inherit.document()->ticksToMeasure(1920).measureProperties.tempoMicrosecondsPerQuarter(4)==600007);

    // Valid SMF extremes can exceed the normal editing range. Merely viewing
    // or applying their rounded display must not replace the original bytes.
    // A BPM edit made before changing its beat unit keeps the intended
    // physical tempo; the field immediately displays the new beat unit.
    LosslessTestWindow pending;
    auto* pendingEditor=qobject_cast<CS_LocalMassEdit*>(pending.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
    CHECK(pendingEditor);
    TempoPropertiesTestDialog pendingDialog(pendingEditor,0);
    auto* pendingBpm=pendingDialog.findChild<QDoubleSpinBox*>("spinBoxBPM");
    auto* pendingNumerator=pendingDialog.findChild<QSpinBox*>("spinBoxTimeSignatureNominator");
    auto* pendingDenominator=pendingDialog.findChild<QComboBox*>("comboBoxTimeSignatureDenominator");
    CHECK(pendingBpm && pendingNumerator && pendingDenominator);
    pendingBpm->setValue(100.0);
    pendingNumerator->setValue(8);
    pendingDenominator->setCurrentIndex(3);
    CHECK(pendingBpm->value()==200.0);
    pendingDialog.apply();
    CHECK(pending.document()->measureItemList[0]->microsecondsPerQuarter==600000);

    for(int tempo : {1,0xffffff})
    {
        LosslessTestWindow extreme;
        DocMeasureItem* first=extreme.document()->measureItemList[0];
        first->microsecondsPerQuarter=tempo;
        if(tempo==0xffffff)
        {
            first->timeSignatureNominator=1;
            first->timeSignatureDenominator=1;
        }
        auto* extremeEditor=qobject_cast<CS_LocalMassEdit*>(extreme.editor()->getSubsystemByClassName("CS_LocalMassEdit"));
        CHECK(extremeEditor);
        const TempoEvents before=savedTempos(extreme);
        TempoPropertiesTestDialog extremeDialog(extremeEditor,0);
        auto* spin=extremeDialog.findChild<QDoubleSpinBox*>("spinBoxBPM");
        CHECK(spin && spin->value()>0.0);
        if(tempo==1)CHECK(spin->value()==60000000.0);
        else CHECK(spin->value()<1.0);
        extremeDialog.apply(); extremeDialog.apply();
        CHECK(savedTempos(extreme)==before);
    }
}

class PlaybackConversionTest : public CS_Playback
{
public:
    using CS_Playback::CS_Playback;
    QList<MidiShortMsg> messages(int start)
    {
        playbackMode=PBM_Stream; playbackStartTicks=start;
        timestampTranslationTable.clear();
        TimestampTranslationTableEntry entry{0,INT_MAX,0,INT_MAX,1.0};
        timestampTranslationTable.append(entry); timestampTranslationTableIndexCache=0;
        QList<MidiShortMsg> result; convertTrackToShortMessages(0,result);
        playbackMode=PBM_None;
        return result;
    }
};
static void checkPlaybackConversion()
{
    LosslessTestWindow window; DocTrack* track=window.document()->trackList[0];
    track->midiChannel=1;
    auto addState=[&](int tick,int status,int key,int value,qint64 order) {
        auto* event=new DocEvent; event->type=DocEvent::E_OtherMidi;
        event->tickPosition=tick; event->tickLength=1;
        event->otherMidiEventData.midiCommand[0]=status;
        event->otherMidiEventData.midiCommand[1]=key;
        event->otherMidiEventData.midiCommand[2]=value;
        event->otherMidiEventData.importOrder=order;
        track->insertEvent(event);
    };
    addState(10,0xb0,0,3,4); addState(10,0xb0,32,4,5); addState(10,0xc0,12,0,6);
    for(int i=0;i<2;++i)
    {
        auto* event=new DocEvent; event->type=DocEvent::E_Note;
        event->tickPosition=20; event->tickLength=10;
        event->noteEventData.noteNumber=60; event->noteEventData.velocity=70+20*i;
        event->noteEventData.importOnOrder=8+i; event->noteEventData.importOffOrder=12+i;
        event->noteEventData.releaseVelocity=17+6*i; track->insertEvent(event);
    }
    addState(20,0xb0,64,127,10);
    PlaybackConversionTest playback(window.editor()); const auto messages=playback.messages(20);
    CHECK(messages.size()==11);
    for(int i=0;i<6;++i)CHECK(messages[i].stateRestoration && messages[i].timestamp==20);
    CHECK(messages[3].data[1]==0 && messages[3].data[2]==3);
    CHECK(messages[4].data[1]==32 && messages[4].data[2]==4);
    CHECK(messages[5].data[0]==0xc0 && messages[5].data[1]==12);
    for(int i=6;i<11;++i)CHECK(!messages[i].stateRestoration);
    CHECK(messages[6].data[0]==0x90 && messages[6].data[2]==70);
    CHECK(messages[7].data[0]==0x90 && messages[7].data[2]==90);
    CHECK(messages[8].data[0]==0xb0 && messages[8].data[1]==64);
    CHECK(messages[9].timestamp==30 && messages[9].data[0]==0x80 && messages[9].data[2]==17);
    CHECK(messages[10].timestamp==30 && messages[10].data[0]==0x80 && messages[10].data[2]==23);

    // Retained initial setup after a file reset still follows edited track
    // properties in live playback. SysEx itself remains file-only support.
    for(int layout=0;layout<3;++layout)for(bool bank : {false,true})for(bool sourceSetup : {false,true}) {
        LosslessTestWindow resetWindow;
        EditorState resetState;
        QByteArray trackBytes=QByteArray::fromHex("00f0057e7f0901f7");
        if(bank)trackBytes+=QByteArray::fromHex("00b0000200b02003");
        if(sourceSetup)trackBytes+=QByteArray::fromHex("00b0071400b00a7f00c028");
        trackBytes+=QByteArray::fromHex("00903c640a803c4000ff2f00");
        QList<QByteArray> tracks={trackBytes};
        if(layout==1)tracks.append(QByteArray::fromHex("00ff2f00"));
        if(layout==2)tracks.prepend(QByteArray::fromHex("00ff2f00"));
        importBytes(smfBytes(tracks),*resetWindow.document(),resetState);
        resetWindow.editor()->csApplyStateAndUpdate(resetState);
        auto* resetTrack=resetWindow.document()->trackList[0];
        resetTrack->midiPatch=73; resetTrack->midiVolume=55; resetTrack->midiPanorama=32;
        PlaybackConversionTest resetPlayback(resetWindow.editor());
        for(int start : {0,5}) {
            int patch=-1,volume=-1,pan=-1;
            for(const auto& message : resetPlayback.messages(start)) {
                const int command=message.data[0]&0xf0;
                if(command==0x90)break;
                if(command==0xc0)patch=message.data[1];
                if(command==0xb0 && message.data[1]==7)volume=message.data[2];
                if(command==0xb0 && message.data[1]==10)pan=message.data[2];
            }
            CHECK(patch==72 && volume==55 && pan==32);
        }
    }
}
class GridTestView : public View {
public:
    GridTestView() : View(nullptr) {}
    QImage grid(const QRect& cells, qreal scale) {
        cellArea=cells;
        mapper.refreshDisplayedItemLists();
        for(auto& range : outOfBoundNoteRange)range.fill(false,cells.width());
        QImage image(QSize(600*scale,600*scale),QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale); image.fill(Qt::transparent);
        QPainter painter(&image);
        paintTrackCells(painter,QRegion(cells),cells,0,SelRectXPos{0,0},QFont());
        return image;
    }
    QColor background() const { return shadedPaletteColor(0); }
};
static void checkWhiteKeyGridBoundaries() {
    LosslessTestWindow window;
    GridTestView view;
    view.setController(window.editor(),window.document());
    view.resize(600,600);
    EditorState state=window.editor()->getEditorState();
    // F/E boundaries must look like the already present C/B boundaries, even
    // without notes. Test several octaves, row heights, centers and DPR values.
    for(int zoom : {25,33,50,75})for(double center : {60.0,60.35})
    for(int height : {380,451})for(qreal scale : {1.0,1.25,1.5,2.0}) {
        state.yZoomSliderValue=zoom; state.trackStateList[0].centerMidiNote=center;
        window.editor()->csApplyStateAndUpdate(state);
        const QRect cells(20,20,550,height);
        const QImage image=view.grid(cells,scale);
        const QString capture=qEnvironmentVariable("SPEED_MIDI_GRID_CAPTURE");
        if(!capture.isEmpty() && zoom==33 && center==60.0 && height==380 && scale==1.0)
            CHECK(image.save(capture));
        const double rowHeight=state.getNoteHeightInPixels();
        for(int note : {48,53,60,65,72,77}) {
            const int rowCenter=int(cells.center().y()-(note-center)*rowHeight);
            const int bottom=int(rowCenter+rowHeight/2);
            if(bottom<=cells.top()+2 || bottom>=cells.bottom()-2)continue;
            int separatorPixels=0;
            for(int dy=0;dy<int(std::ceil(scale));++dy) {
                const int y=int(bottom*scale)+dy;
                int shaded=0;
                for(int x=100;x<400;++x)
                    if(image.pixelColor(int(x*scale),y)!=view.background())++shaded;
                separatorPixels=qMax(separatorPixels,shaded);
            }
            CHECK(separatorPixels>270);
            int plain=0;
            for(int x=100;x<400;++x)
                if(image.pixelColor(int(x*scale),int((bottom-2)*scale))==view.background())++plain;
            CHECK(plain>270);
        }
    }
}
static void checkRejectedMeterOpenKeepsDocument()
{
    for(int variant=0;variant<10;++variant) {
        const bool fractional=variant==1;
        QTemporaryDir temporary; CHECK(temporary.isValid());
        const QString path=temporary.filePath("unsupported-meter.mid");
        QByteArray source=smfBytes({fractional ?
            QByteArray::fromHex("00ff58040303180800ff2f00") :
            QByteArray::fromHex("00ff5804040218088740ff5804030218088740ff58040502180800ff2f00")},
            fractional ? 481:480);
        if(variant>=2 && variant<8) {
            const QList<QByteArray> extra={QByteArray::fromHex("00ff58042102180800ff2f00"),
                QByteArray::fromHex("00ff58040406180800ff2f00"),
                QByteArray::fromHex("00ff59027f0000ff2f00"),
                QByteArray::fromHex("00ff5902000200ff2f00"),
                QByteArray::fromHex("00ff5805040218085500ff2f00"),
                QByteArray::fromHex("00ff590300005500ff2f00")};
            source=smfBytes({extra[variant-2]});
        }
        if(variant>=8)source=smfBytes({QByteArray::fromHex("00ff58040402180800ff2f00"),
            QByteArray::fromHex("00903c648360")+
            QByteArray::fromHex(variant==8 ? "ff580406032408":"ff59020100")+
            QByteArray::fromHex("8360803c0000ff2f00")});
        QFile file(path); CHECK(file.open(QIODevice::WriteOnly)); CHECK(file.write(source)==source.size()); file.close();
        LosslessTestWindow window;
        auto* note=new DocEvent; note->type=DocEvent::E_Note;
        note->tickPosition=120; note->tickLength=240;
        note->noteEventData.noteNumber=60; note->noteEventData.velocity=100;
        window.document()->trackList[0]->insertEvent(note);
        window.setWindowModified(true);
        DocRoot* const originalDoc=window.document(); Controller* const originalEditor=window.editor();
        const EditorState state=originalEditor->getEditorState();
        const QByteArray before=saveDoc(*originalDoc,state);
        const QString directory=QDir::currentPath();
        const bool undo=window.getUI()->actionEdit_Undo->isEnabled();
        const bool redo=window.getUI()->actionEdit_Redo->isEnabled();
        QString warning;
        QTimer responder;
        QObject::connect(&responder,&QTimer::timeout,[&]() {
            auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(message) { warning=message->text(); message->accept(); }
        });
        responder.start(10); CHECK(!window.loadFile(path)); responder.stop();
        CHECK(warning.contains(variant==0 ? "inside a measure" : variant==1 ? "fractional-tick" :
                               variant<4 ? "supported range" : variant>=8 ? "outside the conductor track" :
                               variant==6 ? "invalid time signature" : "invalid key signature"));
        CHECK(warning.contains("file has not been changed"));
        CHECK(window.document()==originalDoc && window.editor()==originalEditor);
        CHECK(originalEditor->getEditorState()==state);
        CHECK(saveDoc(*originalDoc,state)==before && window.isWindowModified());
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==undo);
        CHECK(window.getUI()->actionEdit_Redo->isEnabled()==redo);
        CHECK(QDir::currentPath()==directory);
        CHECK(file.open(QIODevice::ReadOnly)); CHECK(file.readAll()==source);
    }
}

static QList<QPair<int,QByteArray>> savedMeters(LosslessTestWindow& window)
{
    QByteArray bytes=saveDoc(*window.document(),window.editor()->getEditorState());
    QBuffer buffer(&bytes); CHECK(buffer.open(QIODevice::ReadOnly));
    SmfDocument smf(&buffer); CHECK(smf.load());
    QList<QPair<int,QByteArray>> result;
    for(const auto* track : smf.trackList)for(const auto* event : track->eventList)
        if(const auto* meter=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TIME_SIGNATURE))
            result.append({int(meter->tickPosition),QByteArray(reinterpret_cast<const char*>(meter->data),int(meter->dataLength))});
    return result;
}
static void checkFractionalMeterEditRejection()
{
    for(int ppqn : {481,961,32767}) {
        LosslessTestWindow window; DocRoot* doc=window.document();
        doc->midiTicksPerWholeNote=4*ppqn;
        auto* first=doc->measureItemList[0]; first->timeSignatureNominator=4; first->timeSignatureDenominator=8;
        selectClipboardCells(window,0,2*ppqn,true);
        const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
        const EditorState beforeState=window.editor()->getEditorState();
        const bool undo=window.getUI()->actionEdit_Undo->isEnabled();
        auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit")); CHECK(editor);
        TempoPropertiesTestDialog dialog(editor,0);
        auto* numerator=dialog.findChild<QSpinBox*>("spinBoxTimeSignatureNominator"); CHECK(numerator); numerator->setValue(3);
        QString warning;
        QTimer responder;
        QObject::connect(&responder,&QTimer::timeout,[&]() {
            auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(!message)return;
            if(message->standardButtons() & QMessageBox::Yes)message->done(QMessageBox::No);
            else { warning=message->text(); message->accept(); }
        });
        responder.start(10); dialog.apply(); responder.stop();
        CHECK(warning.contains("fractional-tick"));
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
        CHECK(window.editor()->getEditorState()==beforeState);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==undo);
        DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties(); changed.timeSignatureNominator=3;
        CHECK(!editor->setMeasureProperties(0,changed,2*ppqn,3*ppqn/2,false));
        // A malformed in-memory document must also fail before producing bytes.
        first->timeSignatureNominator=3;
        QByteArray output; QBuffer buffer(&output); CHECK(buffer.open(QIODevice::WriteOnly));
        CHECK(!doc->save(&buffer,beforeState,false)); CHECK(output.isEmpty());
        first->timeSignatureNominator=4;
        DocRoot reloaded; EditorState state; importBytes(before,reloaded,state);
        CHECK(reloaded.measureToTicks(1)==2*ppqn);
        changed.timeSignatureNominator=6;
        CHECK(editor->setMeasureProperties(0,changed,2*ppqn,3*ppqn,false));
        DocRoot validChanged; EditorState validState;
        importBytes(saveDoc(*doc,window.editor()->getEditorState()),validChanged,validState);
        CHECK(validChanged.measureToTicks(1)==3*ppqn);
        window.getUI()->actionEdit_Undo->trigger();
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
    }
}
static void checkOddMeterClipboardAndExactKeys()
{
    for(int ppqn : {480,481,961,32767})for(int targetPpqn : {480,481,961,32767}) {
        LosslessTestWindow window; auto* doc=window.document(); doc->midiTicksPerWholeNote=4*ppqn;
        auto* first=doc->measureItemList[0]; first->timeSignatureNominator=4; first->timeSignatureDenominator=8;
        auto* second=new DocMeasureItem(*first); second->resetSetFlags(); second->setTimeSignature=true;
        second->timeSignatureNominator=3; second->timeSignatureDenominator=4; second->tickPosition=2*ppqn;
        doc->measureItemList.append(second);
        auto* key=new DocMeasureItem; key->tickPosition=ppqn; key->setKeySignature=true;
        key->keySignature=7; key->keySignatureScale=DocMeasureItem::KSS_Minor;
        doc->measureItemList.insert(1,key);
        auto* note=new DocEvent; note->type=DocEvent::E_Note; note->tickPosition=ppqn;
        note->tickLength=ppqn; note->noteEventData.noteNumber=60; note->noteEventData.velocity=100;
        doc->trackList[0]->insertEvent(note);
        selectClipboardCells(window,0,5*ppqn,true); window.getUI()->actionEdit_Copy->trigger();
        doc->scaleTickResolution(4*targetPpqn);
        selectClipboardCells(window,8*targetPpqn,11*targetPpqn,true);
        const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
        window.getUI()->actionEdit_Paste->trigger();
        const QByteArray after=saveDoc(*doc,window.editor()->getEditorState()); CHECK(after!=before);
        CHECK(doc->getMeasureItemAtExact(9*targetPpqn) && doc->getMeasureItemAtExact(9*targetPpqn)->keySignature==7);
        CHECK(doc->getMeasureItemAtExact(10*targetPpqn) && doc->getMeasureItemAtExact(10*targetPpqn)->timeSignatureDenominator==4);
        DocRoot reloaded; EditorState state; importBytes(after,reloaded,state);
        CHECK(reloaded.getMeasureItemAtExact(9*targetPpqn)->keySignature==7);
        window.getUI()->actionEdit_Undo->trigger(); CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
        window.getUI()->actionEdit_Redo->trigger(); CHECK(saveDoc(*doc,window.editor()->getEditorState())==after);
        auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit")); CHECK(editor);
        DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties(); changed.microsecondsPerQuarter=600001;
        CHECK(editor->setMeasureProperties(0,changed,2*targetPpqn,2*targetPpqn,false));
        CHECK(doc->getMeasureItemAtExact(9*targetPpqn)->keySignature==7);
        window.getUI()->actionEdit_Undo->trigger(); CHECK(saveDoc(*doc,window.editor()->getEditorState())==after);
    }
}
static void checkFractionalClipboardGridRejection()
{
    for(bool corruptSource : {false,true}) {
        LosslessTestWindow source;
        source.document()->midiTicksPerWholeNote=corruptSource ? 1924:1920;
        source.document()->measureItemList[0]->timeSignatureNominator=3;
        source.document()->measureItemList[0]->timeSignatureDenominator=8;
        selectClipboardCells(source,0,corruptSource ? 721:720,true);
        source.getUI()->actionEdit_Copy->trigger();
        LosslessTestWindow target; target.document()->midiTicksPerWholeNote=corruptSource ? 1920:1924;
        const int length=target.document()->midiTicksPerWholeNote;
        selectClipboardCells(target,length,2*length,true);
        const EditorState state=target.editor()->getEditorState();
        const QByteArray before=saveDoc(*target.document(),state);
        const bool undo=target.getUI()->actionEdit_Undo->isEnabled(),redo=target.getUI()->actionEdit_Redo->isEnabled();
        target.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*target.document(),target.editor()->getEditorState())==before);
        CHECK(target.editor()->getEditorState()==state);
        CHECK(target.getUI()->actionEdit_Undo->isEnabled()==undo && target.getUI()->actionEdit_Redo->isEnabled()==redo);
    }
}
static void checkStandardKeyRange()
{
    LosslessTestWindow window; auto* doc=window.document(); selectClipboardCells(window,0,1920,true);
    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit")); CHECK(editor);
    TempoPropertiesTestDialog dialog(editor,0);
    auto* major=dialog.findChild<QComboBox*>("comboBoxKeySignatureMajor");
    auto* minor=dialog.findChild<QComboBox*>("comboBoxKeySignatureMinor"); CHECK(major && minor);
    CHECK(major->count()==15 && minor->count()==15);
    const QByteArray before=saveDoc(*doc,window.editor()->getEditorState());
    for(int invalid : {-11,-8,8,11}) {
        DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties(); changed.keySignature=invalid;
        CHECK(!editor->setMeasureProperties(0,changed,1920,1920,false));
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==before);
        auto* first=doc->measureItemList[0]; int original=first->keySignature; first->keySignature=invalid;
        QByteArray output; QBuffer buffer(&output); CHECK(buffer.open(QIODevice::WriteOnly));
        CHECK(!doc->save(&buffer,window.editor()->getEditorState(),false)); CHECK(output.isEmpty());
        QByteArray payload; QDataStream stream(&payload,QIODevice::WriteOnly); first->serialize(stream,0,true,4,true);
        QDataStream reader(payload); DocMeasureItem decoded; decoded.deserialize(reader,true,true);
        CHECK(reader.status()==QDataStream::ReadCorruptData); first->keySignature=original;
    }
}
static void checkMeterMetadataClipboardAndUndo()
{
    LosslessTestWindow window; auto* doc=window.document();
    auto* first=doc->measureItemList[0];
    first->timeSignatureNominator=6; first->timeSignatureDenominator=8;
    first->midiClocksPerMetronomeClick=36; first->notated32ndNotesPerQuarter=16;
    auto* second=new DocMeasureItem(*first); second->resetSetFlags(); second->setTimeSignature=true;
    second->tickPosition=1440; second->midiClocksPerMetronomeClick=12; second->notated32ndNotesPerQuarter=4;
    doc->measureItemList.append(second);
    auto* note=new DocEvent; note->type=DocEvent::E_Note; note->tickPosition=0; note->tickLength=5000;
    note->noteEventData.noteNumber=60; note->noteEventData.velocity=100; doc->trackList[0]->insertEvent(note);
    const auto before=savedMeters(window);
    selectClipboardCells(window,0,1440,true); window.getUI()->actionEdit_Copy->trigger();
    const QByteArray native=QApplication::clipboard()->mimeData()->data("application/speedymidi-v4");
    CHECK(!native.isEmpty());
    QDataStream reader(native); int version,mode,measures,tracks,ticks,resolution,count;
    reader >> version >> mode >> measures >> tracks >> ticks >> resolution >> count;
    CHECK(version==-4 && mode==S_GlobalMeasure && count==1);
    DocMeasureItem decoded; decoded.deserialize(reader,true,true); CHECK(reader.status()==QDataStream::Ok);
    CHECK(decoded.midiClocksPerMetronomeClick==36 && decoded.notated32ndNotesPerQuarter==16);
    const qint64 metadataOffset=reader.device()->pos()-2*qint64(sizeof(qint32));
    selectClipboardCells(window,2880,4320,true); window.getUI()->actionEdit_Paste->trigger();
    const auto after=savedMeters(window);
    CHECK(after.contains(qMakePair(2880,QByteArray::fromHex("06032410"))));
    // Same numerator/denominator still needs an explicit meter to restore the
    // previous region's different metadata after the inserted range.
    CHECK(after.contains(qMakePair(4320,QByteArray::fromHex("06030c04"))));
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedMeters(window)==before);
    window.getUI()->actionEdit_Redo->trigger(); CHECK(savedMeters(window)==after);
    auto* editor=qobject_cast<CS_LocalMassEdit*>(window.editor()->getSubsystemByClassName("CS_LocalMassEdit")); CHECK(editor);
    DocMeasureItem changed=doc->getFirstMeasureEffectiveProperties(); changed.microsecondsPerQuarter=600001;
    CHECK(editor->setMeasureProperties(0,changed,1440,1440,false)); CHECK(savedMeters(window)==after);
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedMeters(window)==after);
    changed=doc->getFirstMeasureEffectiveProperties(); changed.timeSignatureNominator=3;
    CHECK(editor->setMeasureProperties(0,changed,1440,720,true));
    CHECK(savedMeters(window).first().second==QByteArray::fromHex("03032410"));
    window.getUI()->actionEdit_Undo->trigger(); CHECK(savedMeters(window)==after);
    for(int badField : {0,1,2}) {
        QByteArray corrupt=native;
        if(badField==2)corrupt.truncate(int(metadataOffset+sizeof(qint32)));
        else {
            QByteArray fields; QDataStream out(&fields,QIODevice::WriteOnly);
            out << qint32(badField==0 ? 256:36) << qint32(badField==1 ? -1:16);
            corrupt.replace(int(metadataOffset),fields.size(),fields);
        }
        auto* mime=new QMimeData; mime->setData("application/speedymidi-v4",corrupt); QApplication::clipboard()->setMimeData(mime);
        const EditorState state=window.editor()->getEditorState();
        const QByteArray bytes=saveDoc(*doc,state);
        const bool undo=window.getUI()->actionEdit_Undo->isEnabled(),redo=window.getUI()->actionEdit_Redo->isEnabled();
        window.getUI()->actionEdit_Paste->trigger();
        CHECK(saveDoc(*doc,window.editor()->getEditorState())==bytes);
        CHECK(window.editor()->getEditorState()==state);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==undo && window.getUI()->actionEdit_Redo->isEnabled()==redo);
    }
}

static void checkExactMarkerClipboard()
{
    for(int sourceTick : {0,240}) {
    LosslessTestWindow source;
    DocMeasureItem* marker=sourceTick ? new DocMeasureItem : source.document()->measureItemList[0];
    marker->tickPosition=sourceTick; marker->setRehearsalMarker=true;
    marker->rehearsalMarkerText=QStringLiteral("Last"); marker->rehearsalMarkerColor=Qt::red;
    marker->markerPackets={QByteArray("First"),QByteArray("Last")};
    if(sourceTick)source.document()->measureItemList.append(marker);
    selectClipboardCells(source,0,1920,true); source.getUI()->actionEdit_Copy->trigger();
    CHECK(QApplication::clipboard()->mimeData()->hasFormat("application/speedymidi-v5"));
    CHECK(QApplication::clipboard()->mimeData()->hasFormat("application/speedymidi-v4"));
    {
        LosslessTestWindow target;
        selectClipboardCells(target,1920,3840,true);
        const QByteArray before=saveDoc(*target.document(),target.editor()->getEditorState());
        target.getUI()->actionEdit_Paste->trigger();
        const int tick=1920+sourceTick;
        const DocMeasureItem* copied=target.document()->getMeasureItemAtExact(tick);
        CHECK(copied && copied->markerPackets==marker->markerPackets);
        const QByteArray after=saveDoc(*target.document(),target.editor()->getEditorState());
        target.getUI()->actionEdit_Undo->trigger();
        CHECK(saveDoc(*target.document(),target.editor()->getEditorState())==before);
        target.getUI()->actionEdit_Redo->trigger();
        CHECK(saveDoc(*target.document(),target.editor()->getEditorState())==after);
    }
    }
}

int main(int argc,char** argv)
{
    QTemporaryDir temporary; CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temporary.path());
    LosslessTestApp app(argc,argv); app.initialize();
    checkPacketAndDurationRoundtrip(); checkUnmatchedAndEmptyTracks();
    checkRealtimeRoundtrip(); checkClipboardValidation(); checkNativeUndoAndClipboard(); checkActualClipboardActions();
    checkExactTempoClipboard(); checkExactTempoResolutionClipboard(); checkClipboardMeterAfterResolutionScaling(); checkExactTempoMeasureActions();
    checkExactTempoPropertiesDialog();
    checkRebarRangeValidation();
    checkRebarDialogRejection();
    checkPlaybackConversion();
    checkWhiteKeyGridBoundaries();
    checkRejectedMeterOpenKeepsDocument();
    checkFractionalMeterEditRejection();
    checkOddMeterClipboardAndExactKeys();
    checkFractionalClipboardGridRejection();
    checkStandardKeyRange();
    checkMeterMetadataClipboardAndUndo();
    checkExactMarkerClipboard();
    std::puts("Lossless packets, endpoints, unmatched notes, realtime, clipboard and native undo passed");
}
