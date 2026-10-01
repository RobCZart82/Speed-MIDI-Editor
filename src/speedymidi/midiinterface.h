/***************************************************************************
 *  midiinterface.h - Interface SpeedyMidi <-> PortMidi
 *
 *  Copyright 2010-2013 Holger Hoffmann
 ***************************************************************************

   This file is part of "Speedy MIDI".

   "Speedy MIDI" is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   "Speedy MIDI" is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with "Speedy MIDI". If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MIDIINTERFACE_H
#define MIDIINTERFACE_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QRecursiveMutex>
#include <QWaitCondition>
#include <atomic>
#include <portmidi.h>
#include <porttime.h>
#if defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#endif

#define MIDI_INTERFACE_N_MIDI_CHANNELS  16
#define MIDI_INTERFACE_N_NOTE_NUMBERS   128
#define MIDI_INTERFACE_BUFFER_SIZE      1024
#define MIDI_INTERFACE_TIMER_RESOLUTION 1000    // must be 1000 ms
#define MIDI_INTERFACE_LATENCY          5

#define MIDI_INTERFACE_THREAD_SLEEP_INTERVAL            50   // ms
#define MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE    (MIDI_INTERFACE_THREAD_SLEEP_INTERVAL + 30)   // ms
#define MIDI_INTERFACE_STATE_SCAN_BUDGET                128
#define MIDI_INTERFACE_STATE_SUBMIT_BUDGET              64

class MidiInterfaceThread;

class MidiShortMsg
{
public:
    MidiShortMsg(qint64 timestamp, quint8 midiCommand)
    {
        this->timestamp=timestamp;
        this->data[0]=midiCommand;
        this->data[1]=0;
        this->data[2]=0;
    }
    MidiShortMsg(qint64 timestamp, quint8 midiCommand, quint8 midiData1)
    {
        this->timestamp=timestamp;
        this->data[0]=midiCommand;
        this->data[1]=midiData1;
        this->data[2]=0;
    }
    MidiShortMsg(qint64 timestamp, quint8 midiCommand, quint8 midiData1, quint8 midiData2)
    {
        this->timestamp=timestamp;
        this->data[0]=midiCommand;
        this->data[1]=midiData1;
        this->data[2]=midiData2;
    }
    bool operator<(const MidiShortMsg& rhs) const
    {
        return timestamp < rhs.timestamp;
    }

    int length();

    qint64 timestamp;  // ms; stream-relative for track messages, absolute for queued cleanup messages
    quint8 data[3];
    bool stateRestoration=false; // startup/seek state; paced before stream notes
};

class MidiStreamOutputTrack
{
public:
    MidiStreamOutputTrack(const QList<MidiShortMsg>& msgList)
    {
        mute=false;
        nextStreamMsgIndex=0;
        this->msgList=msgList;
    }
    void setNextMsgIndexToTimestamp(int msgTimestamp);

    class PlayingNoteType
    {
    public:
        bool operator==(const PlayingNoteType& rhs) const
        {
            return
                    noteNumber  == rhs.noteNumber &&
                    midiChannel == rhs.midiChannel;
        }

        int noteNumber;
        int midiChannel;
    };

    bool mute;
    quint16 sustainChannels=0;
    void rememberSustain(const MidiShortMsg& msg);
    QList<PlayingNoteType> playingNoteList;
    int nextStreamMsgIndex;
    QList<MidiShortMsg> msgList;
    bool restoringState=false;
    bool stateRestorationPending=false;
    bool restorationConsumesStream=false;
    int restoreNextMsgIndex=0;
    int restoreEndMsgIndex=0;
    int restoreReadyMsgIndex=-1;
};

class MidiInterface : public QObject
{
    Q_OBJECT
    friend class MidiInterfaceThread;
public:
    enum PlayMode { PM_Stop, PM_Play, PM_Pause };

    MidiInterface(QObject* parent);
    ~MidiInterface();

    void rescanDevices();

    bool openInput(const QString& deviceName);
    bool closeInput();

    bool openOutput(const QString& deviceName);
    bool closeOutput();

    bool isKeyDown(int noteNumber);                  // immediate input : noteNumber in [0;MIDI_INTERFACE_N_NOTE_NUMBERS)
    bool writeShortMessage(const MidiShortMsg& msg); // immediate output: msg is 1, 2, or 3 bytes

    void setMidiThru(bool midiThru) { QMutexLocker locker(&internalThreadMutex); this->midiThru=midiThru; }
    bool isMidiThru() { QMutexLocker locker(&internalThreadMutex); return midiThru; }

    // stream output functions
    void resetStreamOutputTracks();
    int addStreamOutputTrack(const QList<MidiShortMsg> msgList);
    bool setMute(int trackIndex, bool muteTrack);
    bool play(int fromTimestamp);
    bool pause();
    bool stop();
    void setRelativePlaybackSpeed(int percent);

    QString getErrorText() const { QMutexLocker locker(&internalThreadMutex); return errorText; }
    PlayMode getPlayMode() const { QMutexLocker locker(&internalThreadMutex); return playMode; }
    int getCurrentPlayTimestamp();

    const QStringList& getInputDeviceList() const { return inputDeviceList; }
    const QStringList& getOutputDeviceList() const { return outputDeviceList; }

signals:
    void midiKeyPressed(int noteNumber);
    void midiKeyReleased(int noteNumber);
    void midiKeyStateChanged();
    void trackMidiActivity(int trackIndex, int velocity);

    void midiStreamFinished();
    void midiInputError(const QString& message, bool fatal);
    void midiOutputError(const QString& message);

protected:
    QString errorText;
    void setErrorText(PmError pmError);

    // Device lists
    bool pmInitialized;
    QStringList inputDeviceList;
    QStringList outputDeviceList;

    // Input device
    bool inputDeviceOpened;
    PmDeviceID inputDeviceID;
    PortMidiStream* inputStream;
    bool midiKeyDownArray[MIDI_INTERFACE_N_NOTE_NUMBERS];
    quint16 midiKeyDownChannelCounts[MIDI_INTERFACE_N_MIDI_CHANNELS][MIDI_INTERFACE_N_NOTE_NUMBERS];

    // Output device
    bool outputDeviceOpened;
    PmDeviceID outputDeviceID;
    PortMidiStream* outputStream;
#if defined(Q_OS_MACOS)
    bool appleGmOutputOpened;
    AUGraph appleGmGraph;
    AUNode appleGmSynthNode;
    AudioUnit appleGmSynthUnit;
    bool openAppleGmOutput();
    void closeAppleGmOutput();
    void sendAppleGmMessage(const MidiShortMsg& msg);
#endif
    bool outputFailed=false;
    quint32 lastClock=0;
    qint64 extendedClock=0;
    bool clockInitialized=false;
    qint64 currentTimeMs();
    qint64 currentPlaybackClockTime(bool& outputBlocked);
    void failOutput(PmError error);
    PlayMode playMode;
    qint64 timeAtTimestampZero;  // ms, absolute time
    qint64 lastStreamOutputTime; // ms, latest timestamp submitted during playback
    qint64 pendingCleanupTime;
    bool streamOutputBarrierActive;
    qint64 streamOutputBarrierEndTime;
    bool streamCompletionNotified;
    int pausedAtTimestamp;    // ms, timestamp (unscaled)
    QList<MidiStreamOutputTrack*> outputStreamTrackList;
    QList<MidiShortMsg> outputImmediateMsgList;
    int relativePlaybackSpeedInPercent;
    int pendingPlaybackSpeedInPercent=0;
    qint64 pendingPlaybackSpeedTime=0;
    bool stateRestorationActive=false;
    bool stateRestorationFreezesClock=false;
    qint64 stateRestorationHoldTime=0;
    qint64 nextStateRestorationTime=0;
    qint64 lastStateRestorationTime=0;
    void beginStateRestoration(MidiStreamOutputTrack* track, int first, int end, bool consumesStream, bool freezesClock);
    void cancelStateRestoration();
    bool processStateRestoration();

    bool midiThru;
    quint16 midiThruUsedChannels=0;

    // Interface thread
    MidiInterfaceThread* midiInterfaceThread;
    mutable QRecursiveMutex internalThreadMutex; // Protects data structures and device states

    void pollInput();               // called from interface thread
    void releaseThruInputNotes();   // caller holds internalThreadMutex
    void processImmediateOutput();  // called from interface thread
    void processStreamOutput();     // called from interface thread
};

class MidiInterfaceThread : public QThread
{
public:
    MidiInterfaceThread(MidiInterface* midiInterface);
    void stop() { stopThread.store(true); triggerThread(); }
    void triggerThread() { waitCondition.wakeOne(); }
    static void inputCallbackProc(void* input_callback_info);

protected:
    virtual void run();

    QWaitCondition waitCondition;
    QMutex waitConditionMutex;

    MidiInterface* midiInterface;
    std::atomic_bool stopThread;
};

#endif // MIDIINTERFACE_H
