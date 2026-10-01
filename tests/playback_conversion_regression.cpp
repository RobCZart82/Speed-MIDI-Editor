#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "controller.h"
#include "cs_playback.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

class PlaybackTestApp : public SpeedyMidiApp
{
public:
    using SpeedyMidiApp::SpeedyMidiApp;
    void initialize()
    {
        settings=new Settings;
        settings->mainWindowGeometry.clear();
        settings->mainWindowState.clear();
        settings->mousePiano.visible=false;
        setupAppGlobalUi();
    }
    ~PlaybackTestApp() override { aboutToQuitCleanup(); }
};

class PlaybackTestWindow : public MainWindow
{
public:
    DocRoot* document() { return docRoot; }
    Controller* editor() { return controller; }
};

class PlaybackTest : public CS_Playback
{
public:
    using CS_Playback::CS_Playback;
    ~PlaybackTest() override { playbackMode=PBM_None; }
    void prepare(int start=0)
    {
        playbackMode=PBM_Stream;
        playbackStartTicks=start;
        rebuildTimestampTranslationTable();
    }
    int timestamp(int ticks) { return ticksToTimestamp(ticks); }
    int ticks(int timestamp) { return timestampToTicks(timestamp); }
    QList<MidiShortMsg> messages()
    {
        QList<MidiShortMsg> result;
        convertTrackToShortMessages(0,result);
        return result;
    }
};

static void appendTempo(DocRoot* doc,int tick,int microseconds)
{
    auto* item=new DocMeasureItem;
    item->tickPosition=tick;
    item->setTempo=true;
    item->BPM=120;
    item->microsecondsPerQuarter=microseconds;
    doc->measureItemList.append(item);
}

static void appendNote(DocTrack* track,int tick,int length,int pitch)
{
    auto* event=new DocEvent;
    event->type=DocEvent::E_Note;
    event->tickPosition=tick;
    event->tickLength=length;
    event->noteEventData.noteNumber=pitch;
    event->noteEventData.velocity=80;
    event->noteEventData.midiKeypressSerialNo=0;
    track->insertEvent(event);
}

static int noteTimestamp(const QList<MidiShortMsg>& messages,int command,int pitch)
{
    int found=-1;
    for(const auto& message : messages)
        if((message.data[0]&0xf0)==command && message.data[1]==pitch)
        {
            CHECK(found==-1);
            found=message.timestamp;
        }
    CHECK(found>=0);
    return found;
}

static void checkProductionTempoMap()
{
    PlaybackTestWindow window;
    DocRoot* doc=window.document();
    doc->measureItemList[0]->microsecondsPerQuarter=500001;
    // Earlier same-tick messages do not alter the effective last tempo.
    doc->measureItemList[0]->precedingTempoValues={800001,700001};
    appendTempo(doc,240,400003);
    auto* key=new DocMeasureItem;
    key->tickPosition=480;
    key->setKeySignature=true;
    key->keySignature=1;
    key->keySignatureScale=DocMeasureItem::KSS_Major;
    doc->measureItemList.append(key);
    appendTempo(doc,720,600007);
    auto* meter=new DocMeasureItem;
    meter->tickPosition=1920;
    meter->setTimeSignature=true;
    meter->timeSignatureNominator=8;
    meter->timeSignatureDenominator=8;
    doc->measureItemList.append(meter);
    doc->trackList[0]->midiChannel=1;
    appendNote(doc->trackList[0],0,960,60);
    appendNote(doc->trackList[0],480,480,64);

    PlaybackTest playback(window.editor());
    playback.prepare();
    CHECK(playback.timestamp(0)==0);
    CHECK(playback.timestamp(240)==250);
    CHECK(playback.timestamp(480)==450);
    CHECK(playback.timestamp(720)==650);
    CHECK(playback.timestamp(960)==950);
    CHECK(playback.timestamp(2160)==2450);
    // Exercise cache movement backwards across multiple tempo changes.
    CHECK(playback.timestamp(240)==250);
    CHECK(playback.timestamp(0)==0);
    CHECK(playback.ticks(950)==959);
    CHECK(playback.ticks(450)==479);
    CHECK(playback.ticks(0)==0);

    const auto messages=playback.messages();
    CHECK(noteTimestamp(messages,0x90,60)==0);
    CHECK(noteTimestamp(messages,0x80,60)==950);
    CHECK(noteTimestamp(messages,0x90,64)==450);
    CHECK(noteTimestamp(messages,0x80,64)==950);

    playback.prepare(480);
    const auto sought=playback.messages();
    CHECK(noteTimestamp(sought,0x90,60)==450);
    CHECK(noteTimestamp(sought,0x90,64)==450);
    CHECK(noteTimestamp(sought,0x80,60)==950);
    for(const auto& message : sought)
        if(message.stateRestoration)CHECK(message.timestamp==450);
    // Rebuilding starts accumulation from zero rather than appending stale entries.
    playback.prepare();
    CHECK(playback.timestamp(960)==950);

    doc->scaleTickResolution(3840);
    playback.prepare();
    CHECK(playback.timestamp(480)==250);
    CHECK(playback.timestamp(1920)==950);
}

static void checkShortSegmentAccumulation()
{
    PlaybackTestWindow window;
    DocRoot* doc=window.document();
    doc->measureItemList[0]->microsecondsPerQuarter=500001;
    for(int tick=1;tick<=480;++tick)appendTempo(doc,tick,500001+(tick&1));
    doc->trackList[0]->midiChannel=1;
    appendNote(doc->trackList[0],0,480,60);
    PlaybackTest playback(window.editor());
    playback.prepare();
    // Summing 480 individually truncated one-tick segments would give 480 ms.
    CHECK(playback.timestamp(480)==500);
    CHECK(playback.timestamp(240)==250);
    CHECK(playback.timestamp(512)==533);
    CHECK(noteTimestamp(playback.messages(),0x80,60)==500);
    CHECK(playback.ticks(500)==479);
}

int main(int argc,char** argv)
{
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temporary.path());
    PlaybackTestApp app(argc,argv);
    app.initialize();
    checkProductionTempoMap();
    checkShortSegmentAccumulation();
    std::puts("Exact arbitrary-tick playback, seeking and tempo accumulation passed");
}
