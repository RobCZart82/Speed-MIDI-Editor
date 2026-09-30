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
extern "C" PtTimestamp test_Pt_Time() { return clockMs; }
extern "C" PmError test_Pm_WriteShort(PortMidiStream*, PmTimestamp when, int32_t) {
    ++writeCalls; submittedTime=when; return writeResult;
}
extern "C" PmError test_Pm_Close(PortMidiStream*) {
    ++closeCalls;
    return pmInternalError;
}
class TestInterface : public MidiInterface {
public:
    TestInterface() : MidiInterface(nullptr) {}
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
            resetStreamOutputTracks();
            outputFailed=false; clockInitialized=false; clockMs=0; writeCalls=0;
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
            resetStreamOutputTracks(); outputFailed=false;
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
