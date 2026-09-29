#include "midiinterface.h"
#include <QCoreApplication>
#include <QPointer>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static int closeCalls=0;
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
    QThread* worker() const { return midiInterfaceThread; }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QPointer<QThread> worker;
    {
        TestInterface midi;
        worker=midi.worker();
        midi.checkCloseFailures();
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
