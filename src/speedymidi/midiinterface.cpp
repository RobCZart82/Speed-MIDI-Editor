/***************************************************************************
 *  midiinterface.cpp - Interface SpeedyMidi <-> PortMidi
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

#include "midiinterface.h"

#include <algorithm>
#include <limits>
#if defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace
{
#if defined(Q_OS_MACOS)
const QString appleGmOutputName = QStringLiteral("Apple Built-in General MIDI");
#endif
}

MidiInterface::MidiInterface(QObject* parent)
        : QObject(parent)
{
    pmInitialized=false;

    inputDeviceOpened=false;
    inputDeviceID=pmNoDevice;
    inputStream=NULL;
    for(int i=0; i < MIDI_INTERFACE_N_NOTE_NUMBERS; ++i)
        midiKeyDownArray[i]=false;

    outputDeviceOpened=false;
    outputDeviceID=pmNoDevice;
    outputStream=NULL;
#if defined(Q_OS_MACOS)
    appleGmOutputOpened=false;
    appleGmGraph=nullptr;
    appleGmSynthNode=0;
    appleGmSynthUnit=nullptr;
#endif
    playMode=PM_Stop;

    timeAtTimestampZero=-1;     // invalid in playMode PM_Stop
    pausedAtTimestamp=-1;       // invalid in playMode PM_Stop
    relativePlaybackSpeedInPercent=100;

    midiThru=false;

    midiInterfaceThread=new MidiInterfaceThread(this);
    midiInterfaceThread->start(QThread::TimeCriticalPriority);

    rescanDevices();
}

MidiInterface::~MidiInterface()
{
    midiInterfaceThread->stop();
    midiInterfaceThread->wait();

    if(pmInitialized)
    {
        if(inputDeviceOpened)closeInput();
        if(outputDeviceOpened)closeOutput();
        
        Pm_Terminate();
        Pt_Stop();
        pmInitialized=false;
    }

    qDeleteAll(outputStreamTrackList);
}

/* All portmidi commands:

    Pm_Abort;                       -
    Pm_Close;                       used
    Pm_CountDevices;                used
    Pm_GetDefaultInputDeviceID;     -
    Pm_GetDefaultOutputDeviceID;    -
    Pm_GetDeviceInfo;               used
    Pm_GetErrorText;                used
    Pm_GetHostErrorText;            used
    Pm_HasHostError;                -
    Pm_Initialize;                  used
    Pm_OpenInput;                   used
    Pm_OpenOutput;                  used
    Pm_Poll;                        -
    Pm_Read;                        used
    Pm_SetChannelMask;              -
    Pm_SetFilter;                   used
    Pm_Synchronize;                 -
    Pm_Terminate;                   used
    Pm_Write;                       used
    Pm_WriteShort;                  used
    Pm_WriteSysEx;                  -
*/

void MidiInterface::rescanDevices()
{
    QMutexLocker locker(&internalThreadMutex);
    errorText.clear();
    if(pmInitialized)
    {
        if(inputDeviceOpened)closeInput();
        if(outputDeviceOpened)closeOutput();

        // Also rescan Port-MIDI device lists
        Pm_Terminate();
        Pt_Stop();
        pmInitialized=false;
    }

    inputDeviceList.clear();
    outputDeviceList.clear();

    const PmError initializeError=Pm_Initialize();
    if(initializeError != pmNoError)
    {
        setErrorText(initializeError);
        return;
    }

    const PtError timerError=Pt_Start(MIDI_INTERFACE_TIMER_RESOLUTION,NULL,NULL);
    if(timerError != ptNoError)
    {
        Pm_Terminate();
        errorText=tr("Could not start the MIDI timer service (error %1).").arg((int)timerError);
        return;
    }
    pmInitialized=true;
    clockInitialized=false; // Pt_Start reset the backend clock

    for(PmDeviceID id=0; id < Pm_CountDevices(); ++id)
    {
        const PmDeviceInfo* deviceInfo=Pm_GetDeviceInfo(id);

        if(deviceInfo->input != 0)
            inputDeviceList.append(QString(deviceInfo->name));

        if(deviceInfo->output != 0)
            outputDeviceList.append(QString(deviceInfo->name));
    }
#if defined(Q_OS_MACOS)
    outputDeviceList.append(appleGmOutputName);
#endif
}

bool MidiInterface::openInput(const QString& deviceName)
{
    QMutexLocker locker(&internalThreadMutex);
    // Keep the initialization diagnostic and never query a terminated backend.
    if(!pmInitialized)return false;
    errorText.clear();

    if(inputDeviceOpened)
    {
        errorText=tr("MIDI input already opened");
        return false;
    }

    // Look for an input device with given name. Keep the member invalid until
    // the whole open sequence has succeeded.
    PmDeviceID candidateDeviceID=pmNoDevice;
    for(PmDeviceID id=0; id < Pm_CountDevices(); ++id)
    {
        const PmDeviceInfo* deviceInfo=Pm_GetDeviceInfo(id);

        if(deviceInfo->input != 0 && deviceName == QString(deviceInfo->name))
        {
            candidateDeviceID=id;
            break;
        }
    }
    if(candidateDeviceID == pmNoDevice)
    {
        // No such device found
        errorText=tr("Illegal MIDI input device name");
        return false;
    }

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        PmError pmError=Pm_OpenInput(&inputStream,candidateDeviceID,
                                     NULL,MIDI_INTERFACE_BUFFER_SIZE,
                                     NULL,NULL,
                                     midiInterfaceThread->inputCallbackProc, midiInterfaceThread);
        if(pmError != pmNoError)
        {
            setErrorText(pmError);
            return false;
        }

        // Catch only events from 0x80 to 0xEF
        pmError=Pm_SetFilter(inputStream, PM_FILT_REALTIME | PM_FILT_SYSTEMCOMMON);
        if(pmError != pmNoError)
        {
            setErrorText(pmError);
            Pm_Close(inputStream);
            inputStream=NULL;
            inputDeviceID=pmNoDevice;
            return false;
        }

        inputDeviceID=candidateDeviceID;
        inputDeviceOpened=true;
    }
    return true;
}

bool MidiInterface::closeInput()
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(!inputDeviceOpened)
    {
        errorText=tr("MIDI input not opened");
        return false;
    }

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        PmError pmError=Pm_Close(inputStream);
        // Pm_Close consumes the stream even when the backend reports an error.
        inputDeviceOpened=false;
        inputDeviceID=pmNoDevice;
        inputStream=NULL;

        bool keyStateChanged=false;
        for(int i=0; i < MIDI_INTERFACE_N_NOTE_NUMBERS; ++i)
        {
            if(midiKeyDownArray[i])
            {
                midiKeyDownArray[i]=false;
                emit midiKeyReleased(i);
                keyStateChanged=true;
            }
        }

        if(keyStateChanged) // Key state changed due to close input device?
            emit midiKeyStateChanged();
        if(pmError != pmNoError)
        {
            setErrorText(pmError);
            return false;
        }
    }
    return true;
}

bool MidiInterface::openOutput(const QString& deviceName)
{
    QMutexLocker locker(&internalThreadMutex);
    if(!pmInitialized)return false;
    errorText.clear();

    if(outputDeviceOpened)
    {
        errorText=tr("MIDI output already opened");
        return false;
    }

#if defined(Q_OS_MACOS)
    if(deviceName == appleGmOutputName)
    {
        if(!openAppleGmOutput())
            return false;
        QMutexLocker locker(&internalThreadMutex);
        appleGmOutputOpened=true;
        outputDeviceOpened=true;
        outputFailed=false;
        outputDeviceID=pmNoDevice;
        outputStream=NULL;
        return true;
    }
#endif

    // Look for an output device with given name. Do not retain stale IDs
    // when the selected name is no longer available.
    PmDeviceID candidateDeviceID=pmNoDevice;
    for(PmDeviceID id=0; id < Pm_CountDevices(); ++id)
    {
        const PmDeviceInfo* deviceInfo=Pm_GetDeviceInfo(id);

        if(deviceInfo->output != 0 && deviceName == QString(deviceInfo->name))
        {
            candidateDeviceID=id;
            break;
        }
    }
    if(candidateDeviceID == pmNoDevice)
    {
        // No such device found
        errorText=tr("Illegal MIDI output device name");
        return false;
    }

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        PmError pmError=Pm_OpenOutput(&outputStream,candidateDeviceID,NULL,MIDI_INTERFACE_BUFFER_SIZE,NULL,NULL,
                                      MIDI_INTERFACE_LATENCY);
        if(pmError != pmNoError)
        {
            setErrorText(pmError);
            return false;
        }

        outputDeviceID=candidateDeviceID;
        outputDeviceOpened=true;
        outputFailed=false;
    }
    return true;
}

bool MidiInterface::closeOutput()
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(!outputDeviceOpened)
    {
        errorText=tr("MIDI output not opened");
        return false;
    }

#if defined(Q_OS_MACOS)
    if(appleGmOutputOpened)
    {
        QMutexLocker locker(&internalThreadMutex);
        closeAppleGmOutput();
        appleGmOutputOpened=false;
        outputDeviceOpened=false;
        outputDeviceID=pmNoDevice;
        outputStream=NULL;
        playMode=PM_Stop;
        outputImmediateMsgList.clear();
        return true;
    }
#endif

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        PmError pmError=Pm_Close(outputStream);
        // Never leave the worker with a freed PortMidi stream.
        outputDeviceOpened=false;
        outputDeviceID=pmNoDevice;
        outputStream=NULL;
        playMode=PM_Stop;
        outputImmediateMsgList.clear();
        if(pmError != pmNoError)
        {
            setErrorText(pmError);
            return false;
        }
    }
    return true;
}

#if defined(Q_OS_MACOS)
bool MidiInterface::openAppleGmOutput()
{
    AudioComponentDescription synthDescription = {};
    synthDescription.componentType = kAudioUnitType_MusicDevice;
    synthDescription.componentSubType = kAudioUnitSubType_DLSSynth;
    synthDescription.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponentDescription outputDescription = {};
    outputDescription.componentType = kAudioUnitType_Output;
    outputDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
    outputDescription.componentManufacturer = kAudioUnitManufacturer_Apple;

    OSStatus status = NewAUGraph(&appleGmGraph);
    if(status == noErr)
        status = AUGraphAddNode(appleGmGraph, &synthDescription, &appleGmSynthNode);

    AUNode outputNode = 0;
    if(status == noErr)
        status = AUGraphAddNode(appleGmGraph, &outputDescription, &outputNode);
    if(status == noErr)
        status = AUGraphOpen(appleGmGraph);
    if(status == noErr)
        status = AUGraphNodeInfo(appleGmGraph, appleGmSynthNode, NULL, &appleGmSynthUnit);
    if(status == noErr)
        status = AUGraphConnectNodeInput(appleGmGraph, appleGmSynthNode, 0, outputNode, 0);
    if(status == noErr)
        status = AUGraphInitialize(appleGmGraph);
    if(status == noErr)
        status = AUGraphStart(appleGmGraph);

    if(status != noErr)
    {
        closeAppleGmOutput();
        errorText=tr("Could not start the built-in macOS General MIDI synthesizer (Audio Unit error %1).")
                .arg(static_cast<int>(status));
        return false;
    }
    return true;
}

void MidiInterface::closeAppleGmOutput()
{
    if(appleGmGraph)
    {
        Boolean running = false;
        if(AUGraphIsRunning(appleGmGraph, &running) == noErr && running)
            AUGraphStop(appleGmGraph);
        AUGraphUninitialize(appleGmGraph);
        AUGraphClose(appleGmGraph);
        DisposeAUGraph(appleGmGraph);
    }
    appleGmGraph=nullptr;
    appleGmSynthNode=0;
    appleGmSynthUnit=nullptr;
}

void MidiInterface::sendAppleGmMessage(const MidiShortMsg& msg)
{
    if(!appleGmOutputOpened || !appleGmSynthUnit)
        return;

    const UInt32 status = msg.data[0];
    if(status >= 0x80 && status <= 0xef)
        MusicDeviceMIDIEvent(appleGmSynthUnit, status, msg.data[1], msg.data[2], 0);
}
#endif

bool MidiInterface::isKeyDown(int noteNumber)
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    // immediate input : noteNumber in [0;MIDI_INTERFACE_N_NOTE_NUMBERS)
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_INTERFACE_N_NOTE_NUMBERS);
    return midiKeyDownArray[noteNumber];
}

bool MidiInterface::writeShortMessage(const MidiShortMsg& msg)
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    if(outputFailed)return false; // reopen the device before retrying
    errorText.clear();

    if(!outputDeviceOpened)
    {
        errorText=tr("MIDI output not opened");
        return false;
    }

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);
        outputImmediateMsgList.append(msg);
    }
    midiInterfaceThread->triggerThread();
    return true;
}

void MidiInterface::resetStreamOutputTracks()
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(playMode != PM_Stop)stop();

    // delete all tracks
    for(int i=0; i < outputStreamTrackList.size(); ++i)
        delete outputStreamTrackList[i];
    outputStreamTrackList.clear();
}

int MidiInterface::addStreamOutputTrack(const QList<MidiShortMsg> msgList)
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(playMode != PM_Stop)stop();

    // append a new track object
    outputStreamTrackList.append(new MidiStreamOutputTrack(msgList));
    return outputStreamTrackList.size() - 1;    // return index of appended track
}

bool MidiInterface::setMute(int trackIndex, bool muteTrack)
{
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(trackIndex < 0 || trackIndex >= outputStreamTrackList.size())
    {
        errorText=tr("Invalid track index");
        return false;
    }

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        MidiStreamOutputTrack* track = outputStreamTrackList[trackIndex];
        track->mute=muteTrack;

        if(muteTrack)
        {
            // switch all notes off that are currently enabled in this track
            while(!track->playingNoteList.isEmpty())
            {
                MidiStreamOutputTrack::PlayingNoteType playingNote = track->playingNoteList.takeFirst();

                // send immediate note-off message for this note on correct channel
                MidiShortMsg noteOffMsg(0, 0x80 + playingNote.midiChannel, playingNote.noteNumber, 0x40);
                outputImmediateMsgList.append(noteOffMsg);
            }
        }
    }
    midiInterfaceThread->triggerThread();

    return true;
}

bool MidiInterface::play(int fromTimestamp)
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    if(outputFailed)return false; // reopen the device before retrying
    errorText.clear();

    if(!outputDeviceOpened)
    {
        errorText=tr("MIDI output not opened");
        return false;
    }

    if(playMode == PM_Play)stop(); // if already playing, stop stream output before repositioning

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        // update index pointers in all tracks so they point to the next message to send
        for(int i=0; i < outputStreamTrackList.size(); ++i)
            outputStreamTrackList[i]->setNextMsgIndexToTimestamp(fromTimestamp);

        // start playing at fromTimestamp
        timeAtTimestampZero=currentTimeMs() - qint64(fromTimestamp) * 100 / relativePlaybackSpeedInPercent;

        playMode=PM_Play;
        midiInterfaceThread->triggerThread();
    }
    return true;
}

bool MidiInterface::pause()
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(!outputDeviceOpened)
    {
        errorText=tr("MIDI output not opened");
        return false;
    }

    if(playMode != PM_Play)return true; // already paused/stopped

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        pausedAtTimestamp=getCurrentPlayTimestamp();
        playMode=PM_Pause;

        // Pause: Send command all-notes-off for each channel
        for(int i=0; i < MIDI_INTERFACE_N_MIDI_CHANNELS; ++i)
        {
            outputImmediateMsgList.append(MidiShortMsg(0,0xb0+i,0x7b,0));
        }

        for(int i=0; i < outputStreamTrackList.size(); ++i)
            outputStreamTrackList[i]->playingNoteList.clear();
    }
    midiInterfaceThread->triggerThread();
    return true;
}

bool MidiInterface::stop()
{
    QMutexLocker locker(&internalThreadMutex);
    Q_ASSERT(pmInitialized);
    errorText.clear();

    if(!outputDeviceOpened)
    {
        errorText=tr("MIDI output not opened");
        return false;
    }

    if(playMode == PM_Stop)return true; // already stopped

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);
        playMode=PM_Stop;

        // Stop: Send commands all-notes-off and reset-all-controllers for each channel
        for(int i=0; i < MIDI_INTERFACE_N_MIDI_CHANNELS; ++i)
        {
            outputImmediateMsgList.append(MidiShortMsg(0,0xb0+i,0x7b,0));   // all-notes-off
            outputImmediateMsgList.append(MidiShortMsg(0,0xb0+i,0x79,0));   // reset-all-controllers
        }

        for(int i=0; i < outputStreamTrackList.size(); ++i)
            outputStreamTrackList[i]->playingNoteList.clear();
    }
    midiInterfaceThread->triggerThread();
    return true;
}

void MidiInterface::setRelativePlaybackSpeed(int percent)
{
    QMutexLocker locker(&internalThreadMutex);
    if(percent <= 0)return;
    if(percent == relativePlaybackSpeedInPercent)return;

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);

        // Adjust playback position timing according to new settings.
        //  It is possible to change the value during playback, but then we must recalculate timeAtTimestampZero.
        switch(playMode)
        {
        case PM_Stop:
        case PM_Pause:
            relativePlaybackSpeedInPercent=percent;
            break;
        case PM_Play:
            {
                // convert current time to current timestamp
                int currentPlayTimestamp=getCurrentPlayTimestamp();

                // set new speed
                relativePlaybackSpeedInPercent=percent;

                // reset time at timestamp 0
                timeAtTimestampZero=currentTimeMs() - qint64(currentPlayTimestamp) * 100 / relativePlaybackSpeedInPercent;
                break;
            }
        }
    }
}

int MidiInterface::getCurrentPlayTimestamp()
{
    QMutexLocker locker(&internalThreadMutex);
    if(!outputDeviceOpened)return -1;

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);
        switch(playMode)
        {
        case PM_Stop:
            return -1; // not playing
        case PM_Play:
            // convert current time to current timestamp
            return int(qBound<qint64>(qint64(0), (currentTimeMs() - timeAtTimestampZero) * relativePlaybackSpeedInPercent / 100,
                                     qint64(std::numeric_limits<int>::max())));
        case PM_Pause:
            return pausedAtTimestamp;
        }
        return -1;
    }
}

qint64 MidiInterface::currentTimeMs()
{
    // Pt_Time is a wrapping 32-bit millisecond clock. Sampled by the worker
    // while playing, unsigned subtraction extends both signed and full wraps.
    const quint32 now=quint32(Pt_Time());
    if(!clockInitialized) {
        extendedClock=now;
        clockInitialized=true;
    } else {
        extendedClock+=quint32(now-lastClock);
    }
    lastClock=now;
    return extendedClock;
}

void MidiInterface::failOutput(PmError error)
{
    if(outputFailed)return;
    setErrorText(error);
    outputFailed=true;
    playMode=PM_Stop;
    outputImmediateMsgList.clear();
    for(auto* track : outputStreamTrackList)track->playingNoteList.clear();
    emit midiOutputError(errorText);
    emit midiStreamFinished();
}

void MidiInterface::setErrorText(PmError pmError)
{
    if(pmError == pmHostError)
    {
        char msg[256+1];
        Pm_GetHostErrorText(msg,256);
        errorText=QString(tr("PortMidi Host error: "))+msg;
    }
    else
    {
        errorText=Pm_GetErrorText(pmError);
    }
}

void MidiInterface::pollInput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!inputDeviceOpened)return;

    static const int INPUT_BUFFER_LENGTH=32;    // #events stored in buffer
    PmEvent eventBuffer[INPUT_BUFFER_LENGTH];

    bool keyStateChanged=false;
    while(true)
    {
        int numberOfEventsRead=Pm_Read(inputStream,eventBuffer,INPUT_BUFFER_LENGTH);
        if(numberOfEventsRead <= 0)break;  // no events read or error (error is ignored here)

        // If enabled, simulate MIDI Thru
        if(outputDeviceOpened && !outputFailed && midiThru)
        {
#if defined(Q_OS_MACOS)
            if(appleGmOutputOpened)
            {
                for(int i=0; i < numberOfEventsRead; ++i)
                {
                    const quint32 message=eventBuffer[i].message;
                    sendAppleGmMessage(MidiShortMsg(0,
                                                    static_cast<quint8>(message & 0xff),
                                                    static_cast<quint8>((message >> 8) & 0xff),
                                                    static_cast<quint8>((message >> 16) & 0xff)));
                }
            }
            else
#endif
            {
                const PmError error=Pm_Write(outputStream,eventBuffer,numberOfEventsRead);
                if(error < 0)failOutput(error);
            }
        }

        // Analyse events
        for(int i=0; i < numberOfEventsRead; ++i)
        {
            quint32 message=eventBuffer[i].message;
            quint8 command = (quint8)((message      ) & 0xff);
            quint8 data1   = (quint8)((message >>  8) & 0xff);
            quint8 data2   = (quint8)((message >> 16) & 0xff);

            if((command & 0xf0) == 0x90 && data2 != 0)
            {
                // Note-on
                int noteNumber=data1;
                if(midiKeyDownArray[noteNumber] == false)
                {
                    // Key state changed to down
                    midiKeyDownArray[noteNumber]=true;
                    emit midiKeyPressed(noteNumber);
                    keyStateChanged=true;
                }
            }
            else if( (command & 0xf0) == 0x80 ||
                    ((command & 0xf0) == 0x90 && data2 == 0))
            {
                // Note-off
                int noteNumber=data1;
                if(midiKeyDownArray[noteNumber] == true)
                {
                    // Key state changed to down
                    midiKeyDownArray[noteNumber]=false;
                    emit midiKeyReleased(noteNumber);
                    keyStateChanged=true;
                }
            }
        }
    }

    if(keyStateChanged)
        emit midiKeyStateChanged();
}

void MidiInterface::processImmediateOutput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!outputDeviceOpened || outputFailed)return;

    while(!outputImmediateMsgList.isEmpty())
    {
        MidiShortMsg msg=outputImmediateMsgList.takeFirst();

        int32_t pm_msg;
        pm_msg= msg.data[0] + (((int32_t)msg.data[1]) << 8) + (((int32_t)msg.data[2]) << 16);

#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)
            sendAppleGmMessage(msg);
        else
#endif
        {
            const PmError error=Pm_WriteShort(outputStream,0,pm_msg);
            if(error < 0) { failOutput(error); return; }
        }
    }
}

void MidiInterface::processStreamOutput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!outputDeviceOpened || outputFailed)return;
    if(playMode != PM_Play)return;

    // Determine time slot of messages to copy to the buffer
    qint64 currentTime=currentTimeMs();
    qint64 maxMsgTime=currentTime + MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE;
#if defined(Q_OS_MACOS)
    if(appleGmOutputOpened)
        maxMsgTime=currentTime + 1;
#endif

    // Collect upcoming messages in all tracks, sorted by their timestamp
    QList<MidiShortMsg> timeSlotMsgList;
    int numberOfTracksFinished=0;

    for(int i=0; i < outputStreamTrackList.size(); ++i)
    {
        MidiStreamOutputTrack* track=outputStreamTrackList[i];
        int peakNoteVelocity=0;

        while(track->nextStreamMsgIndex < track->msgList.size())
        {
            const MidiShortMsg& msg=track->msgList[track->nextStreamMsgIndex];

            // convert timestamp to time
            qint64 msgTime=qint64(msg.timestamp) * 100 / relativePlaybackSpeedInPercent + timeAtTimestampZero;

            if(msgTime >= maxMsgTime)break; // no more messages to copy

            if(msgTime < currentTime)
            {
                // thread was too slow
                msgTime=currentTime;
            }

            // If track is not muted, copy this message to the buffer
            if(!track->mute)
            {
                timeSlotMsgList.append(msg);

                // analyse message to keep track of notes currently playing
                quint8 command = msg.data[0] & 0xf0;
                int channel    = msg.data[0] & 0x0f;

                bool noteOn = command == 0x90 && msg.data[2] != 0;
                bool noteOff= command == 0x80 || (command == 0x90 && msg.data[2] == 0);

                if(noteOn)
                {
                    peakNoteVelocity=qMax(peakNoteVelocity,static_cast<int>(msg.data[2]));
                    MidiStreamOutputTrack::PlayingNoteType newNote;
                    newNote.midiChannel=channel;
                    newNote.noteNumber=msg.data[1];
                    track->playingNoteList.append(newNote);
                }
                else if(noteOff)
                {
                    MidiStreamOutputTrack::PlayingNoteType disabledNote;
                    disabledNote.midiChannel=channel;
                    disabledNote.noteNumber=msg.data[1];

                    for(int j=0; j < track->playingNoteList.size(); ++j)
                    {
                        if(track->playingNoteList[j] == disabledNote)
                        {
                            track->playingNoteList.removeAt(j);
                            break;
                        }
                    }
                }
            }

            // advance to next message
            ++track->nextStreamMsgIndex;
        }

        if(peakNoteVelocity > 0)
            emit trackMidiActivity(i,peakNoteVelocity);

        // Finished the message list?
        if(track->nextStreamMsgIndex == track->msgList.size()) ++numberOfTracksFinished;
    }

    // Stable-Sort messages in time slot
    std::stable_sort(timeSlotMsgList.begin(), timeSlotMsgList.end());

    // output the messages
    for(int i=0; i < timeSlotMsgList.size(); ++i)
    {
        const MidiShortMsg& msg=timeSlotMsgList[i];

        // convert timestamp to time
        qint64 msgTime=qint64(msg.timestamp) * 100 / relativePlaybackSpeedInPercent + timeAtTimestampZero;

        // convert to Portmidi short message
        int32_t pm_msg;
        pm_msg= msg.data[0] + (((int32_t)msg.data[1]) << 8) + (((int32_t)msg.data[2]) << 16);

#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)
            sendAppleGmMessage(msg);
        else
#endif
        {
            // Preserve the low 32 clock bits without signed arithmetic overflow.
            const quint32 raw=quint32(qMax(msgTime,currentTime));
            const PmTimestamp timestamp=raw <= quint32(std::numeric_limits<int>::max())
                    ? PmTimestamp(raw) : PmTimestamp(qint64(raw)-0x100000000LL);
            const PmError error=Pm_WriteShort(outputStream,timestamp,pm_msg);
            if(error < 0) { failOutput(error); return; }
        }
    }

    // All tracks finished?
    if(numberOfTracksFinished == outputStreamTrackList.size())
        emit midiStreamFinished();
}

MidiInterfaceThread::MidiInterfaceThread(MidiInterface* midiInterface)
        : QThread(midiInterface)
{
    this->midiInterface=midiInterface;
    stopThread.store(false);
}

void MidiInterfaceThread::run()
{
    while(!stopThread.load())
    {
        int waitInterval=MIDI_INTERFACE_THREAD_SLEEP_INTERVAL;
        // INTERNAL LOCK
        {
            QMutexLocker locker(&midiInterface->internalThreadMutex);

            midiInterface->pollInput();
            midiInterface->processImmediateOutput();
            midiInterface->processStreamOutput();
#if defined(Q_OS_MACOS)
            if(midiInterface->appleGmOutputOpened && midiInterface->playMode == MidiInterface::PM_Play)
                waitInterval=2;
#endif
        }

        // Wait for MIDI_INTERFACE_THREAD_SLEEP_INTERVAL milliseconds or until wake up by triggerThread()
        waitConditionMutex.lock();
        waitCondition.wait(&waitConditionMutex,waitInterval);
        waitConditionMutex.unlock();
    }
}

void MidiInterfaceThread::inputCallbackProc(void* input_callback_info)
{
    MidiInterfaceThread* midiInterfaceThread=(MidiInterfaceThread*)input_callback_info;
    midiInterfaceThread->triggerThread();
}

int MidiShortMsg::length()
{
    switch(data[0])
    {
    case 0xf0:return 0;  // SysEx
    case 0xf1:return 2;  // MTC
    case 0xf2:return 3;  // Song position: status + 2 data bytes
    case 0xf3:return 2;  // Song select
    case 0xf4:return 0;  // undefined
    case 0xf5:return 0;  // undefined
    case 0xf6:return 1;  // Tune request
    case 0xf7:return 0;  // SysEx EOX
    case 0xf8:return 1;  // RealTime Timing clock
    case 0xf9:return 0;  // undefined
    case 0xfa:return 1;  // RealTime Start
    case 0xfb:return 1;  // RealTime Continue
    case 0xfc:return 1;  // RealTime Stop
    case 0xfd:return 0;  // undefined
    case 0xfe:return 1;  // RealTime Active Sensing
    case 0xff:return 1;  // RealTime System Reset

    default:
        if(data[0] < 0x80)return 0; // Illegal MIDI command, length unknown
        if(data[0] < 0xc0)return 3; // 8x note-off, 9x note-on, Ax polyphonic aftertouch, Bx control change
        if(data[0] < 0xe0)return 2; // Cx Program change, Dx channel pressure
        return 3;                   // Ex Pitch wheel
    }
}

void MidiStreamOutputTrack::setNextMsgIndexToTimestamp(int msgTimestamp)
{
    // Internal thread mutex should be locked!
    // Find the first message at or after the requested timestamp. This also
    // safely handles the end position (size), which is not a valid list index.
    int first=0;
    int last=msgList.size();
    while(first < last)
    {
        const int middle=first + (last-first)/2;
        if(msgList[middle].timestamp < msgTimestamp)
            first=middle+1;
        else
            last=middle;
    }
    nextStreamMsgIndex=first;
}
