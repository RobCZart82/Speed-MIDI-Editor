#include "midiinterface.h"
#include <QCoreApplication>
#include <QPointer>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static int closeCalls=0, writeCalls=0;
static PtTimestamp clockMs=0;
static PmTimestamp submittedTime=0;
static PmError writeResult=pmNoError;
static QList<PmEvent> submittedEvents;
static QList<PmEvent> inputEvents;
static PmError inputReadError=pmNoError;
extern "C" PtTimestamp test_Pt_Time() { return clockMs; }
extern "C" PmError test_Pm_WriteShort(PortMidiStream*, PmTimestamp when, int32_t message) {
    submittedEvents.append(PmEvent{message,when});
    ++writeCalls; submittedTime=when; return writeResult;
}
extern "C" PmError test_Pm_Write(PortMidiStream* stream, PmEvent* events, int32_t count) {
    for(int i=0; i<count; ++i) {
        const PmError error=test_Pm_WriteShort(stream,events[i].timestamp,events[i].message);
        if(error != pmNoError)return error;
    }
    return pmNoError;
}
extern "C" int test_Pm_Read(PortMidiStream*, PmEvent* events, int32_t count) {
    if(inputEvents.isEmpty()) {
        const PmError result=inputReadError; inputReadError=pmNoError; return result;
    }
    int read=0;
    while(read<count && !inputEvents.isEmpty())events[read++]=inputEvents.takeFirst();
    return read;
}
extern "C" PmError test_Pm_Close(PortMidiStream*) {
    ++closeCalls;
    return pmInternalError;
}
class TestInterface : public MidiInterface {
public:
    TestInterface() : MidiInterface(nullptr) {
        midiInterfaceThread->stop(); midiInterfaceThread->wait();
    }
    void resetPlaybackFixture() {
        playMode=PM_Stop; resetStreamOutputTracks(); outputImmediateMsgList.clear();
        outputFailed=false; clockInitialized=false; clockMs=0;
        pendingPlaybackSpeedInPercent=0; relativePlaybackSpeedInPercent=100;
        writeCalls=0; submittedEvents.clear();
        outputDeviceOpened=true; outputStream=reinterpret_cast<PortMidiStream*>(1);
    }
    void checkCloseFailures() {
        midiInterfaceThread->stop();
        midiInterfaceThread->wait();
        CHECK(pmInitialized);
        outputDeviceOpened=true;
        outputDeviceID=0;
        outputStream=reinterpret_cast<PortMidiStream*>(1);
        CHECK(!closeOutput());
        CHECK(!outputDeviceOpened && outputStream==nullptr && outputDeviceID==pmNoDevice);
        CHECK(!closeOutput());
        CHECK(closeCalls==1);
        inputDeviceOpened=true;
        inputDeviceID=0;
        inputStream=reinterpret_cast<PortMidiStream*>(1);
        midiKeyDownArray[60]=true;
        CHECK(!closeInput());
        CHECK(!inputDeviceOpened && inputStream==nullptr && inputDeviceID==pmNoDevice);
        CHECK(!isKeyDown(60));
        CHECK(!closeInput());
        CHECK(closeCalls==2);
    }
    void checkTimingAndWriteErrors() {
        outputDeviceOpened=true;
        outputStream=reinterpret_cast<PortMidiStream*>(1);
        for(int speed : {1,100,200}) {
            resetPlaybackFixture();
            relativePlaybackSpeedInPercent=speed;
            timeAtTimestampZero=0; playMode=PM_Play;
            outputStreamTrackList.append(new MidiStreamOutputTrack({MidiShortMsg(21600000,0x90,60,100)}));
            processStreamOutput(); CHECK(writeCalls==0);
            const int due=21600000LL*100/speed;
            clockMs=due;
            CHECK(getCurrentPlayTimestamp()==21600000);
            processStreamOutput(); CHECK(writeCalls==1 && submittedTime==due);
        }
        // Cross the signed clock boundary and then the full 32-bit wrap.
        resetStreamOutputTracks(); clockInitialized=false;
        clockMs=2147483640; const qint64 start=currentTimeMs();
        clockMs=-2147483640; CHECK(currentTimeMs()==start+16);
        clockMs=-8; const qint64 beforeWrap=currentTimeMs();
        clockMs=8; CHECK(currentTimeMs()==beforeWrap+16);
        // Seek and live speed changes preserve positions beyond the old 6h limit.
        clockInitialized=false; clockMs=0; relativePlaybackSpeedInPercent=100;
        playMode=PM_Stop; CHECK(play(21600000));
        CHECK(getCurrentPlayTimestamp()==21600000);
        setRelativePlaybackSpeed(200); CHECK(getCurrentPlayTimestamp()==21600000);
        clockMs=1000; CHECK(getCurrentPlayTimestamp()==21602000);
        CHECK(pause()); CHECK(getCurrentPlayTimestamp()==21602000);
        int errors=0;
        QObject::connect(this,&MidiInterface::midiOutputError,this,[&](const QString& text) {
            CHECK(!text.isEmpty()); ++errors;
        });
        for(bool immediate : {false,true}) {
            resetPlaybackFixture();
            clockInitialized=false; clockMs=0; timeAtTimestampZero=0;
            relativePlaybackSpeedInPercent=100; playMode=PM_Play;
            writeCalls=0; writeResult=pmHostError;
            if(immediate) {
                outputImmediateMsgList.append(MidiShortMsg(0,0x90,60,100));
                processImmediateOutput();
            } else {
                outputStreamTrackList.append(new MidiStreamOutputTrack({MidiShortMsg(0,0x90,60,100)}));
                processStreamOutput();
            }
            CHECK(writeCalls==1 && getPlayMode()==PM_Stop && !getErrorText().isEmpty());
            CHECK(!play(0)); CHECK(!writeShortMessage(MidiShortMsg(0,0x90,60,100)));
            processImmediateOutput(); processStreamOutput(); CHECK(writeCalls==1);
        }
        CHECK(errors==2);
        QObject::disconnect(this,&MidiInterface::midiOutputError,this,nullptr);
        writeResult=pmNoError; outputFailed=false;
        outputDeviceOpened=false; outputStream=nullptr;
    }
    void checkSchedulingAndRecovery() {
        resetPlaybackFixture();
        addStreamOutputTrack({MidiShortMsg(70,0x90,60,100),MidiShortMsg(500,0x80,60,64)});
        CHECK(play(0)); processStreamOutput();
        clockMs=10; CHECK(stop()); resetStreamOutputTracks();
        addStreamOutputTrack({MidiShortMsg(0,0x91,64,100),MidiShortMsg(500,0x81,64,64)});
        CHECK(play(0)); processImmediateOutput(); processStreamOutput();
        CHECK(submittedEvents.size()==34);
        CHECK(Pm_MessageStatus(submittedEvents.first().message)==0x90);
        CHECK(Pm_MessageData1(submittedEvents[1].message)==123);
        CHECK(Pm_MessageStatus(submittedEvents.last().message)==0x91);
        for(int i=1;i<submittedEvents.size();++i)
            CHECK(submittedEvents[i].timestamp>=submittedEvents[i-1].timestamp);

        resetPlaybackFixture();
        addStreamOutputTrack({MidiShortMsg(70,0x90,60,100),MidiShortMsg(90,0x80,60,64)});
        CHECK(play(0)); processStreamOutput();
        clockMs=10; setRelativePlaybackSpeed(200); processStreamOutput();
        CHECK(submittedEvents.size()==1 && getCurrentPlayTimestamp()==10);
        clockMs=70; CHECK(getCurrentPlayTimestamp()==70); processStreamOutput();
        CHECK(submittedEvents.size()==2 && submittedEvents[1].timestamp==80);
        clockMs=80; CHECK(getCurrentPlayTimestamp()==90);

        resetPlaybackFixture();
        addStreamOutputTrack({MidiShortMsg(0,0xc1,40),MidiShortMsg(0,0xb1,7,11),
                              MidiShortMsg(1000,0x91,64,100)});
        CHECK(setMute(0,true)); CHECK(play(0)); processStreamOutput();
        CHECK(submittedEvents.isEmpty());
        clockMs=950; CHECK(setMute(0,false)); processImmediateOutput(); processStreamOutput();
        CHECK(submittedEvents.size()==3);
        CHECK(Pm_MessageStatus(submittedEvents[0].message)==0xc1 && Pm_MessageData1(submittedEvents[0].message)==40);
        CHECK(Pm_MessageData1(submittedEvents[1].message)==7 && Pm_MessageData2(submittedEvents[1].message)==11);
        CHECK(Pm_MessageStatus(submittedEvents[2].message)==0x91);

        for(PmError error : {pmBufferOverflow,pmHostError}) {
            resetPlaybackFixture(); clockMs=1000;
            inputDeviceOpened=true; inputStream=reinterpret_cast<PortMidiStream*>(1);
            setMidiThru(true);
            inputEvents.append(PmEvent{Pm_Message(0x90,60,100),7});
            inputReadError=error;
            pollInput(); processImmediateOutput();
            CHECK(!isKeyDown(60) && submittedEvents.size()==3);
            CHECK(submittedEvents[0].timestamp==1000); // Output and input epochs differ.
            CHECK(Pm_MessageData1(submittedEvents[1].message)==64 && Pm_MessageData2(submittedEvents[1].message)==0);
            CHECK(Pm_MessageData1(submittedEvents[2].message)==123);
            CHECK(inputDeviceOpened==(error==pmBufferOverflow));
            inputDeviceOpened=false; inputStream=nullptr; setMidiThru(false);
        }
        resetPlaybackFixture();
        inputDeviceOpened=true; inputStream=reinterpret_cast<PortMidiStream*>(1); setMidiThru(true);
        inputEvents={PmEvent{Pm_Message(0x90,60,100),0},PmEvent{Pm_Message(0xb0,64,127),0},
                     PmEvent{Pm_Message(0x80,60,64),0}};
        inputReadError=pmBufferOverflow;
        pollInput(); processImmediateOutput();
        CHECK(!isKeyDown(60) && submittedEvents.size()==5);
        CHECK(Pm_MessageData1(submittedEvents[3].message)==64 && Pm_MessageData2(submittedEvents[3].message)==0);
        CHECK(Pm_MessageData1(submittedEvents[4].message)==123);
        inputDeviceOpened=false; inputStream=nullptr; setMidiThru(false);
        outputDeviceOpened=false; outputStream=nullptr;
    }
    QThread* worker() const { return midiInterfaceThread; }
    void checkInitializationFailure() {
        QMutexLocker locker(&internalThreadMutex);
        pmInitialized=false;
        errorText=QStringLiteral("MIDI initialization failed");
        CHECK(!openInput(QStringLiteral("persisted input")));
        CHECK(!openOutput(QStringLiteral("persisted output")));
        CHECK(errorText==QStringLiteral("MIDI initialization failed"));
        pmInitialized=true;
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QPointer<QThread> worker;
    {
        TestInterface midi;
        worker=midi.worker();
        midi.checkCloseFailures();
        midi.checkInitializationFailure();
        midi.checkTimingAndWriteErrors();
        midi.checkSchedulingAndRecovery();
        midi.addStreamOutputTrack({MidiShortMsg(0,0x90,60,64)});
    }
    CHECK(worker.isNull());
    MidiStreamOutputTrack track({MidiShortMsg(10,0x90,60,64),MidiShortMsg(20,0x80,60,0)});
    track.setNextMsgIndexToTimestamp(30); CHECK(track.nextStreamMsgIndex==2);
    track.setNextMsgIndexToTimestamp(10); CHECK(track.nextStreamMsgIndex==0);
    track.setNextMsgIndexToTimestamp(15); CHECK(track.nextStreamMsgIndex==1);
    std::puts("MIDI close failure, worker ownership and seek boundaries passed");
    return 0;
}
