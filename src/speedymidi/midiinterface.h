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

class MidiInterfaceThread;

class MidiShortMsg
{
public:
    MidiShortMsg(int timestamp, quint8 midiCommand)
    {
        this->timestamp=timestamp;
        this->data[0]=midiCommand;
        this->data[1]=0;
        this->data[2]=0;
    }
    MidiShortMsg(int timestamp, quint8 midiCommand, quint8 midiData1)
    {
        this->timestamp=timestamp;
        this->data[0]=midiCommand;
        this->data[1]=midiData1;
        this->data[2]=0;
    }
    MidiShortMsg(int timestamp, quint8 midiCommand, quint8 midiData1, quint8 midiData2)
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

    int timestamp;  // ms
    quint8 data[3];
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
    QList<PlayingNoteType> playingNoteList;
    int nextStreamMsgIndex;
    QList<MidiShortMsg> msgList;
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

    QString getErrorText() const { return errorText; }
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
    PlayMode playMode;
    int timeAtTimestampZero;  // ms, absolute time
    int pausedAtTimestamp;    // ms, timestamp (unscaled)
    QList<MidiStreamOutputTrack*> outputStreamTrackList;
    QList<MidiShortMsg> outputImmediateMsgList;
    int relativePlaybackSpeedInPercent;

    bool midiThru;

    // Interface thread
    MidiInterfaceThread* midiInterfaceThread;
    mutable QRecursiveMutex internalThreadMutex; // Protects data structures and device states

    void pollInput();               // called from interface thread
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
