#include "midiinterface.h"
#include <QCoreApplication>
#include <QPointer>
#include <QMap>
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
        clearThruState();
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
        CHECK(submittedEvents.size()==2); // paced state precedes the audible note
        clockMs=951; processStreamOutput();
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
    static MidiShortMsg startup(MidiShortMsg msg) { msg.stateRestoration=true; return msg; }
    void checkPacedRestoration() {
        QList<MidiShortMsg> history={MidiShortMsg(0,0xb0,64,127),MidiShortMsg(0,0xb0,0,2),MidiShortMsg(0,0xb0,32,5),MidiShortMsg(0,0xc0,40),
            MidiShortMsg(0,0xb0,101,0),MidiShortMsg(0,0xb0,100,0),MidiShortMsg(0,0xb0,6,12),MidiShortMsg(0,0xb0,38,34),
            MidiShortMsg(0,0xb0,99,1),MidiShortMsg(0,0xb0,98,3),MidiShortMsg(0,0xb0,6,64),MidiShortMsg(0,0xb0,38,11),
            MidiShortMsg(0,0xb0,96,0),MidiShortMsg(0,0xb0,97,0)};
        for(int i=0;i<5000;++i)history.append(MidiShortMsg(0,0xb0,1,i%128));
        history.append({MidiShortMsg(0,0xb0,0,3),MidiShortMsg(0,0xb0,32,4),MidiShortMsg(0,0xc0,12),
            MidiShortMsg(0,0xb0,0,6),MidiShortMsg(0,0xb0,32,7),MidiShortMsg(0,0xb0,101,0),MidiShortMsg(0,0xb0,100,1),
            MidiShortMsg(0,0xb0,6,23),MidiShortMsg(0,0xb0,38,45),MidiShortMsg(0,0xb0,96,0),
            MidiShortMsg(0,0xb0,99,2),MidiShortMsg(0,0xb0,98,9),MidiShortMsg(0,0xb0,6,34),MidiShortMsg(0,0xb0,38,12),
            MidiShortMsg(0,0xb0,97,0),MidiShortMsg(0,0xd0,13),MidiShortMsg(0,0xe0,11,64)});
        // Preserve every controller transaction, not just each controller's
        // last value: bank/program and selected RPN/NRPN parameters depend on it.
        for(bool taggedSeek : {false,true}) {
            resetPlaybackFixture();
            QList<MidiShortMsg> messages=history;
            if(taggedSeek)for(auto& msg : messages)msg.stateRestoration=true;
            if(!taggedSeek) {
                messages.insert(100,MidiShortMsg(0,0xf0,1,2));
                messages.insert(101,MidiShortMsg(0,0xf7)); // unsupported opaque packets never replayed
            }
            QList<MidiShortMsg> expected=history;
            if(!taggedSeek) {
                messages.append(MidiShortMsg(1000,0x90,59,100));
                messages.append(MidiShortMsg(1100,0x80,59,64)); // elapsed during loading, remains silent
                const QList<MidiShortMsg> laterState={MidiShortMsg(3000,0xb0,1,77),MidiShortMsg(3000,0xb0,6,50),
                                                     MidiShortMsg(3000,0xb0,38,99)};
                messages.append(laterState); expected.append(laterState);
            }
            messages.append(MidiShortMsg(20000,0x90,60,100));
            messages.append(MidiShortMsg(20100,0x80,60,64));
            addStreamOutputTrack(messages);
            if(!taggedSeek)CHECK(setMute(0,true));
            CHECK(play(0));
            if(!taggedSeek) {
                processStreamOutput(); CHECK(submittedEvents.isEmpty()); clockMs=950;
                CHECK(setMute(0,false));
                CHECK(outputImmediateMsgList.isEmpty()); // UI call did not materialize the history
            }
            const int heldPosition=getCurrentPlayTimestamp();
            int passes=0;
            while(stateRestorationActive || submittedEvents.isEmpty()) {
                const int before=submittedEvents.size();
                processStreamOutput();
                const int count=submittedEvents.size()-before;
                CHECK(count<=MIDI_INTERFACE_STATE_SUBMIT_BUDGET);
                CHECK(getCurrentPlayTimestamp()==(taggedSeek ? heldPosition : clockMs));
                for(int i=before;i<submittedEvents.size();++i) {
                    CHECK(i<expected.size());
                    CHECK(Pm_MessageStatus(submittedEvents[i].message)==expected[i].data[0]);
                    CHECK(Pm_MessageData1(submittedEvents[i].message)==expected[i].data[1]);
                    CHECK(Pm_MessageData2(submittedEvents[i].message)==expected[i].data[2]);
                    CHECK(submittedEvents[i].timestamp<=clockMs+MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE);
                    if(i>0)CHECK(submittedEvents[i].timestamp>submittedEvents[i-1].timestamp);
                }
                CHECK(++passes<250);
                if(stateRestorationActive)clockMs+=50;
            }
            CHECK(submittedEvents.size()==expected.size());
            CHECK(passes>70); // thousands of state changes were spread across bounded passes
            // Independently interpret bank/program and RPN/NRPN commands to
            // verify the final device state as well as the exact trace.
            int bankMsb=-1,bankLsb=-1,program=-1,programBankMsb=-1,programBankLsb=-1,modulation=-1;
            int rpnMsb=127,rpnLsb=127,nrpnMsb=127,nrpnLsb=127;
            bool selectedNrpn=false;
            QMap<int,int> parameterValues;
            for(const auto& event : submittedEvents) {
                const int status=Pm_MessageStatus(event.message)&0xf0;
                const int key=Pm_MessageData1(event.message),value=Pm_MessageData2(event.message);
                if(status==0xb0 && key==0)bankMsb=value;
                if(status==0xb0 && key==32)bankLsb=value;
                if(status==0xb0 && key==1)modulation=value;
                if(status==0xc0) { program=key; programBankMsb=bankMsb; programBankLsb=bankLsb; }
                if(status==0xb0) {
                    if(key==101) { rpnMsb=value; selectedNrpn=false; }
                    if(key==100) { rpnLsb=value; selectedNrpn=false; }
                    if(key==99) { nrpnMsb=value; selectedNrpn=true; }
                    if(key==98) { nrpnLsb=value; selectedNrpn=true; }
                    const int parameter=selectedNrpn ? 16384+nrpnMsb*128+nrpnLsb : rpnMsb*128+rpnLsb;
                    if(key==6)parameterValues[parameter]=(parameterValues.value(parameter)&127)+(value<<7);
                    if(key==38)parameterValues[parameter]=(parameterValues.value(parameter)&~127)+value;
                    if(key==96)parameterValues[parameter]=qMin(16383,parameterValues.value(parameter)+1);
                    if(key==97)parameterValues[parameter]=qMax(0,parameterValues.value(parameter)-1);
                }
            }
            CHECK(program==12 && programBankMsb==3 && programBankLsb==4);
            CHECK(bankMsb==6 && bankLsb==7 && modulation==(taggedSeek ? 4999%128 : 77));
            CHECK(parameterValues.value(0)==12*128+34);
            CHECK(parameterValues.value(1)==23*128+45+1);
            CHECK(parameterValues.value(16384+1*128+3)==64*128+11);
            CHECK(parameterValues.value(16384+2*128+9)==(taggedSeek ? 34*128+12-1 : 50*128+99));
            CHECK(getCurrentPlayTimestamp()==(taggedSeek ? heldPosition : clockMs));
            // The musical clock resumes continuously; notes never precede the
            // final restoration event and are not burst out as overdue notes.
            clockMs+=20000-getCurrentPlayTimestamp(); processStreamOutput();
            CHECK(submittedEvents.size()==expected.size()+1);
            CHECK(Pm_MessageStatus(submittedEvents.last().message)==0x90);
            CHECK(submittedEvents.last().timestamp>=submittedEvents[expected.size()-1].timestamp);
        }

        // Histories on a shared channel are merged by original time, not UI
        // unmute order; each bank/parameter transaction keeps its chronology.
        resetPlaybackFixture();
        addStreamOutputTrack({MidiShortMsg(0,0xc0,10),MidiShortMsg(20,0xc0,11),MidiShortMsg(1000,0x90,60,100)});
        addStreamOutputTrack({MidiShortMsg(10,0xc0,12),MidiShortMsg(30,0xc0,13),MidiShortMsg(1000,0x90,61,100)});
        setMute(0,true); setMute(1,true); play(0); clockMs=100; processStreamOutput();
        setMute(0,false); setMute(1,false); processStreamOutput();
        CHECK(submittedEvents.size()==4);
        const int programs[]={10,12,11,13};
        for(int i=0;i<4;++i)CHECK(Pm_MessageData1(submittedEvents[i].message)==programs[i]);

        // Long live unmute must not freeze or stretch another audible track.
        // Its note-off and sustain release stay within the backend horizon.
        resetPlaybackFixture(); QList<MidiShortMsg> liveHistory=history;
        liveHistory.append(MidiShortMsg(20000,0x90,60,100));
        addStreamOutputTrack(liveHistory);
        addStreamOutputTrack({MidiShortMsg(0,0x91,64,100),MidiShortMsg(0,0xb1,64,127),
                              MidiShortMsg(100,0x81,64,64),MidiShortMsg(100,0xb1,64,0),
                              MidiShortMsg(1000,0x91,65,100),MidiShortMsg(1100,0x81,65,64)});
        setMute(0,true); play(0); processStreamOutput();
        CHECK(submittedEvents.size()==2);
        clockMs=10; setMute(0,false);
        bool released=false,sustainReleased=false,nextNote=false;
        for(int pass=0;pass<25;++pass) {
            processStreamOutput();
            CHECK(getCurrentPlayTimestamp()==clockMs);
            for(const auto& event : submittedEvents) {
                if(Pm_MessageStatus(event.message)==0x81 && Pm_MessageData1(event.message)==64) {
                    CHECK(event.timestamp<=100+MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE); released=true;
                }
                if(Pm_MessageStatus(event.message)==0xb1 && Pm_MessageData1(event.message)==64 && Pm_MessageData2(event.message)==0) {
                    CHECK(event.timestamp<=100+MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE); sustainReleased=true;
                }
                if(Pm_MessageStatus(event.message)==0x91 && Pm_MessageData1(event.message)==65)nextNote=true;
            }
            clockMs+=50;
        }
        CHECK(released && sustainReleased && nextNote && stateRestorationActive);
        for(int i=1;i<submittedEvents.size();++i)
            CHECK(submittedEvents[i].timestamp>=submittedEvents[i-1].timestamp);

        // Even note-heavy history has a bounded scan budget, not an O(history)
        // UI or worker loop before finding the first controller.
        resetPlaybackFixture(); QList<MidiShortMsg> sparse;
        for(int i=0;i<10000;++i)sparse.append(MidiShortMsg(0,0x80,60,64));
        sparse.append(MidiShortMsg(0,0xc0,23)); sparse.append(MidiShortMsg(1000,0x90,60,100));
        addStreamOutputTrack(sparse); setMute(0,true); play(0); processStreamOutput(); clockMs=950;
        setMute(0,false); processStreamOutput();
        CHECK(submittedEvents.isEmpty());
        CHECK(outputStreamTrackList[0]->restoreNextMsgIndex==MIDI_INTERFACE_STATE_SCAN_BUDGET);

        resetPlaybackFixture();
        addStreamOutputTrack({MidiShortMsg(0,0xb0,64,127),MidiShortMsg(0,0x90,60,100),
                              MidiShortMsg(10,0x80,60,64),MidiShortMsg(1000,0x90,61,100)});
        play(0); processStreamOutput(); CHECK(outputStreamTrackList[0]->playingNoteList.isEmpty());
        setMute(0,true); processImmediateOutput();
        CHECK(submittedEvents.size()==4 && Pm_MessageData1(submittedEvents.last().message)==64 &&
              Pm_MessageData2(submittedEvents.last().message)==0);

        // Stop, pause, re-mute and fatal output errors discard unsent history.
        // Cleanup goes after the bounded queued horizon and before a restart.
        QList<MidiShortMsg> prefix;
        for(const auto& msg : history)prefix.append(startup(msg));
        prefix.append(MidiShortMsg(1000,0x90,60,100));
        for(int action=0;action<4;++action) {
            resetPlaybackFixture(); addStreamOutputTrack(prefix); play(0); processStreamOutput();
            CHECK(submittedEvents.size()==MIDI_INTERFACE_STATE_SUBMIT_BUDGET);
            const int before=submittedEvents.size();
            if(action==0) {
                CHECK(stop()); CHECK(!stateRestorationActive);
                resetStreamOutputTracks(); addStreamOutputTrack({MidiShortMsg(0,0x91,64,100)}); CHECK(play(0));
                processStreamOutput(); CHECK(submittedEvents.size()==before+33);
                CHECK(Pm_MessageData1(submittedEvents[before].message)==123);
                CHECK(Pm_MessageStatus(submittedEvents.last().message)==0x91);
            } else if(action==1) {
                CHECK(pause()); CHECK(!stateRestorationActive && getCurrentPlayTimestamp()==0);
                processImmediateOutput(); processStreamOutput();
                CHECK(submittedEvents.size()==before+32);
                clockMs=100; processStreamOutput(); CHECK(submittedEvents.size()==before+32);
                CHECK(play(0)); CHECK(getPlayMode()==PM_Play); // responsive resume creates a fresh cursor
            } else if(action==2) {
                CHECK(setMute(0,true)); clockMs=100; processStreamOutput();
                CHECK(submittedEvents.size()==before+1 && !stateRestorationActive);
                CHECK(Pm_MessageData1(submittedEvents.last().message)==64 && Pm_MessageData2(submittedEvents.last().message)==0);
                CHECK(!outputStreamTrackList[0]->restoringState);
            } else {
                writeResult=pmHostError; clockMs=50; processStreamOutput(); writeResult=pmNoError;
                CHECK(submittedEvents.size()==before+1 && !stateRestorationActive && outputFailed);
                processStreamOutput(); CHECK(submittedEvents.size()==before+1);
            }
            for(int i=1;i<submittedEvents.size();++i)
                CHECK(submittedEvents[i].timestamp>=submittedEvents[i-1].timestamp);
        }
        // A speed change during initial restoration preserves the held seek
        // position and takes effect when the freshly audible stream begins.
        resetPlaybackFixture(); addStreamOutputTrack(prefix); play(0); processStreamOutput();
        setRelativePlaybackSpeed(200);
        for(int pass=0;stateRestorationActive;++pass) {
            CHECK(pass<250); clockMs+=50; processStreamOutput();
            CHECK(getCurrentPlayTimestamp()==0);
        }
        CHECK(relativePlaybackSpeedInPercent==200);
        const int restoredCount=submittedEvents.size();
        clockMs+=500; processStreamOutput();
        CHECK(submittedEvents.size()==restoredCount+1 && Pm_MessageStatus(submittedEvents.last().message)==0x90);

        outputDeviceOpened=false; outputStream=nullptr;
    }
    void checkBoundedQueuedStopMute() {
        // Known backend scheduling limit: already submitted short pairs cannot
        // be removed per track. They remain within the lookahead, and no later
        // note may be newly submitted after stop/pause/mute.
        for(int action=0;action<3;++action) {
            resetPlaybackFixture();
            const int due=MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE-5;
            addStreamOutputTrack({MidiShortMsg(due,0x90,60,100),MidiShortMsg(due+1,0x80,60,64),
                                  MidiShortMsg(1000,0x90,61,100),MidiShortMsg(1100,0x80,61,64)});
            CHECK(play(0)); processStreamOutput(); CHECK(submittedEvents.size()==2);
            clockMs=10;
            if(action==0)CHECK(stop());
            else if(action==1)CHECK(pause());
            else CHECK(setMute(0,true));
            processImmediateOutput();
            const int count=submittedEvents.size();
            CHECK(count==(action==2 ? 2 : 34));
            for(const auto& event : submittedEvents)
                CHECK(event.timestamp<=MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE+1);
            clockMs=1000; processStreamOutput(); processImmediateOutput();
            CHECK(submittedEvents.size()==count);
        }
        outputDeviceOpened=false; outputStream=nullptr;
    }
    void checkThruToggle() {
        resetPlaybackFixture();
        inputDeviceOpened=true; inputStream=reinterpret_cast<PortMidiStream*>(1);
        setMidiThru(true);
        // Repeated notes, multiple channels, and sustain with the physical key
        // already released. An unrelated playback note must not be reset.
        playMode=PM_Play;
        inputEvents={PmEvent{Pm_Message(0x90,60,100),0},PmEvent{Pm_Message(0x90,60,90),0},
                     PmEvent{Pm_Message(0x91,64,100),0},PmEvent{Pm_Message(0xb2,64,127),0},
                     PmEvent{Pm_Message(0x92,67,100),0},PmEvent{Pm_Message(0x82,67,64),0}};
        pollInput(); CHECK(submittedEvents.size()==6);
        clockMs=10; setMidiThru(false); processImmediateOutput();
        CHECK(submittedEvents.size()==10);
        int off60=0,off64=0,sustain=0;
        for(int i=6;i<submittedEvents.size();++i) {
            const auto& event=submittedEvents[i];
            CHECK(event.timestamp>=10);
            const int status=Pm_MessageStatus(event.message), key=Pm_MessageData1(event.message);
            if(status==0x80 && key==60)++off60;
            else if(status==0x81 && key==64)++off64;
            else { CHECK(status==0xb2 && key==64 && Pm_MessageData2(event.message)==0); ++sustain; }
        }
        CHECK(off60==2 && off64==1 && sustain==1);
        CHECK(playMode==PM_Play);
        setMidiThru(false); processImmediateOutput(); CHECK(submittedEvents.size()==10);
        inputEvents={PmEvent{Pm_Message(0x80,60,64),0},PmEvent{Pm_Message(0x80,60,64),0},
                     PmEvent{Pm_Message(0x81,64,64),0}};
        pollInput(); CHECK(!isKeyDown(60) && !isKeyDown(64) && submittedEvents.size()==10);
        // Keys observed while disabled never become cleanup targets.
        inputEvents={PmEvent{Pm_Message(0x93,70,100),0}}; pollInput();
        setMidiThru(true); setMidiThru(false); processImmediateOutput(); CHECK(submittedEvents.size()==10);
        // A fresh enable/disable cycle releases only the new forwarded note.
        setMidiThru(true); inputEvents={PmEvent{Pm_Message(0x94,72,100),0}}; pollInput();
        setMidiThru(false); processImmediateOutput(); CHECK(submittedEvents.size()==12);
        CHECK(Pm_MessageStatus(submittedEvents.last().message)==0x84 && Pm_MessageData1(submittedEvents.last().message)==72);
        // Dense repeated-note input must not allocate an unbounded cleanup
        // list or block a UI toggle while every release is sent synchronously.
        resetPlaybackFixture(); setMidiThru(true);
        for(int i=0;i<200;++i)inputEvents.append(PmEvent{Pm_Message(0x95,75,100),0});
        pollInput(); CHECK(submittedEvents.size()==200);
        setMidiThru(false); CHECK(outputImmediateMsgList.isEmpty());
        processImmediateOutput(); CHECK(submittedEvents.size()==200+MIDI_INTERFACE_STATE_SUBMIT_BUDGET);
        setMidiThru(true); inputEvents={PmEvent{Pm_Message(0x96,76,100),0}};
        pollInput(); CHECK(inputEvents.size()==1); // Old releases still pending.
        processImmediateOutput(); processImmediateOutput(); pollInput();
        CHECK(inputEvents.isEmpty() && submittedEvents.size()==401);
        for(int i=200;i<400;++i)
            CHECK(Pm_MessageStatus(submittedEvents[i].message)==0x85 && Pm_MessageData1(submittedEvents[i].message)==75);
        CHECK(Pm_MessageStatus(submittedEvents.last().message)==0x96);
        setMidiThru(false); processImmediateOutput(); CHECK(submittedEvents.size()==402);
        inputDeviceOpened=false; inputStream=nullptr; outputDeviceOpened=false; outputStream=nullptr;
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
        midi.checkPacedRestoration();
        midi.checkBoundedQueuedStopMute();
        midi.checkThruToggle();
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
