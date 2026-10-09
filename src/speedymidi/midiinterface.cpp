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
PmTimestamp toPortMidiTimestamp(qint64 timestamp)
{
    const quint32 raw=quint32(timestamp);
    return raw <= quint32(std::numeric_limits<PmTimestamp>::max())
            ? PmTimestamp(raw) : PmTimestamp(qint64(raw)-0x100000000LL);
}

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
    {
        midiKeyDownArray[i]=false;
        for(int channel=0; channel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++channel)
            midiKeyDownChannelCounts[channel][i]=0;
    }

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
    lastStreamOutputTime=0;
    pendingCleanupTime=0;
    streamOutputBarrierActive=false;
    streamOutputBarrierEndTime=0;
    streamCompletionNotified=false;
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

        releaseThruInputNotes();

        bool keyStateChanged=false;
        for(int i=0; i < MIDI_INTERFACE_N_NOTE_NUMBERS; ++i)
        {
            if(midiKeyDownArray[i])
            {
                midiKeyDownArray[i]=false;
                emit midiKeyReleased(i);
                keyStateChanged=true;
            }
            for(int channel=0; channel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++channel)
                midiKeyDownChannelCounts[channel][i]=0;
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
    clearThruState();

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
        cancelStateRestoration();
        for(auto* track : outputStreamTrackList)
        {
            track->playingNoteList.clear();
            track->clearHoldPedals();
        }
        outputImmediateMsgList.clear();
        pendingCleanupTime=0;
        streamOutputBarrierActive=false;
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
        cancelStateRestoration();
        for(auto* track : outputStreamTrackList)
        {
            track->playingNoteList.clear();
            track->clearHoldPedals();
        }
        outputImmediateMsgList.clear();
        pendingCleanupTime=0;
        streamOutputBarrierActive=false;
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

    cancelStateRestoration();

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
    QMutexLocker locker(&internalThreadMutex);
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
        const bool wasMuted=track->mute;
        track->mute=muteTrack;

        if(muteTrack)
        {
            track->restoringState=false;
            track->stateRestorationPending=false;
            track->restoreReadyMsgIndex=-1;
        }
        else if(wasMuted && playMode == PM_Play && track->nextStreamMsgIndex > 0)
        {
            // Store a cursor, not a copy of the history. Scanning and output
            // happen in bounded worker passes while only this track stays silent.
            beginStateRestoration(track,0,track->nextStreamMsgIndex,false,false);
        }

        if(muteTrack && (!track->playingNoteList.isEmpty() || track->sustainChannels ||
                        track->sostenutoChannels || track->hold2Channels))
        {
            // Notes may already be queued in the PortMidi backend. Put their
            // note-offs after the latest queued stream event so a pending
            // note-on cannot arrive after its cleanup message. Pause stream
            // scheduling until that cleanup time to keep later events ordered.
            const qint64 now=currentTimeMs();
            const qint64 cleanupTimestamp=playMode == PM_Play
                    ? qMax(lastStreamOutputTime + 1,now) : 0;
            if(playMode == PM_Play)
            {
                streamOutputBarrierActive=true;
                streamOutputBarrierEndTime=qMax(streamOutputBarrierEndTime,cleanupTimestamp);
                lastStreamOutputTime=streamOutputBarrierEndTime;
                pendingCleanupTime=qMax(pendingCleanupTime,streamOutputBarrierEndTime);
            }
            for(int channel=0; channel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++channel)
            {
                if(track->sustainChannels & (quint16(1) << channel))
                    outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+channel,64,0));
                if(track->sostenutoChannels & (quint16(1) << channel))
                    outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+channel,66,0));
                if(track->hold2Channels & (quint16(1) << channel))
                    outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+channel,69,0));
            }
            track->clearHoldPedals();
            while(!track->playingNoteList.isEmpty())
            {
                MidiStreamOutputTrack::PlayingNoteType playingNote = track->playingNoteList.takeFirst();

                MidiShortMsg noteOffMsg(cleanupTimestamp, 0x80 + playingNote.midiChannel, playingNote.noteNumber, 0x40);
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

        cancelStateRestoration();

        // update index pointers in all tracks so they point to the next message to send
        for(int i=0; i < outputStreamTrackList.size(); ++i)
            outputStreamTrackList[i]->setNextMsgIndexToTimestamp(fromTimestamp);

        // start playing at fromTimestamp
        const qint64 currentTime=currentTimeMs();
        const qint64 now=pendingCleanupTime > currentTime ? pendingCleanupTime + 1 : currentTime;
        timeAtTimestampZero=now - qint64(fromTimestamp) * 100 / relativePlaybackSpeedInPercent;
        lastStreamOutputTime=now;
        pendingCleanupTime=0;
        streamOutputBarrierActive=false;
        streamCompletionNotified=false;

        playMode=PM_Play;
        for(auto* track : outputStreamTrackList)
            if(!track->mute && track->nextStreamMsgIndex > 0)
                beginStateRestoration(track,0,track->nextStreamMsgIndex,false,true);
        if(stateRestorationActive)stateRestorationHoldTime=now;
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
        cancelStateRestoration();
        if(pendingPlaybackSpeedInPercent)
            relativePlaybackSpeedInPercent=pendingPlaybackSpeedInPercent;
        pendingPlaybackSpeedInPercent=0;
        const qint64 cleanupTimestamp=qMax(lastStreamOutputTime + 1,currentTimeMs());
        playMode=PM_Pause;
        lastStreamOutputTime=cleanupTimestamp;
        pendingCleanupTime=qMax(pendingCleanupTime,cleanupTimestamp);
        streamOutputBarrierActive=false;

        // Pause: send all-notes-off after events already queued by the backend.
        quint16 sostenuto=0,hold2=0;
        for(const auto* track : outputStreamTrackList)
        {
            sostenuto |= track->sostenutoChannels;
            hold2 |= track->hold2Channels;
        }
        for(int i=0; i < MIDI_INTERFACE_N_MIDI_CHANNELS; ++i)
        {
            outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,64,0));
            if(sostenuto & (quint16(1) << i))
                outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,66,0));
            if(hold2 & (quint16(1) << i))
                outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,69,0));
            outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,0x7b,0));
        }

        for(auto* track : outputStreamTrackList)
        {
            track->playingNoteList.clear();
            track->clearHoldPedals();
        }
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

    if(pendingPlaybackSpeedInPercent)
        relativePlaybackSpeedInPercent=pendingPlaybackSpeedInPercent;
    pendingPlaybackSpeedInPercent=0;
    cancelStateRestoration();
    if(playMode == PM_Stop)return true; // already stopped

    // INTERNAL LOCK
    {
        QMutexLocker locker(&internalThreadMutex);
        const qint64 cleanupTimestamp=qMax(lastStreamOutputTime + 1,currentTimeMs());
        playMode=PM_Stop;
        lastStreamOutputTime=cleanupTimestamp;
        pendingCleanupTime=qMax(pendingCleanupTime,cleanupTimestamp);
        streamOutputBarrierActive=false;

        // Stop: clean up after events already queued by the output backend.
        for(int i=0; i < MIDI_INTERFACE_N_MIDI_CHANNELS; ++i)
        {
            outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,0x7b,0));   // all-notes-off
            outputImmediateMsgList.append(MidiShortMsg(cleanupTimestamp,0xb0+i,0x79,0));   // reset-all-controllers
        }

        for(auto* track : outputStreamTrackList)
        {
            track->playingNoteList.clear();
            track->clearHoldPedals();
        }
    }
    midiInterfaceThread->triggerThread();
    return true;
}

void MidiInterface::setRelativePlaybackSpeed(int percent)
{
    QMutexLocker locker(&internalThreadMutex);
    if(percent <= 0)return;
    if(percent == relativePlaybackSpeedInPercent && !pendingPlaybackSpeedInPercent)return;

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
            pendingPlaybackSpeedInPercent=0;
            break;
        case PM_Play:
            {
                bool outputBlocked=false;
                const qint64 playbackTime=currentPlaybackClockTime(outputBlocked);
                if(lastStreamOutputTime > playbackTime)
                {
                    // Already submitted events cannot be retimed portably.
                    // Change speed at their final boundary and hold further
                    // submissions until then (at most one lookahead window).
                    pendingPlaybackSpeedInPercent=percent;
                    pendingPlaybackSpeedTime=lastStreamOutputTime;
                }
                else
                {
                    const qint64 position=(playbackTime-timeAtTimestampZero) * relativePlaybackSpeedInPercent / 100;
                    relativePlaybackSpeedInPercent=percent;
                    pendingPlaybackSpeedInPercent=0;
                    timeAtTimestampZero=playbackTime-position * 100 / percent;
                }
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
            {
                bool outputBlocked=false;
                const qint64 playbackTime=currentPlaybackClockTime(outputBlocked);
                return int(qBound<qint64>(qint64(0), (playbackTime - timeAtTimestampZero) * relativePlaybackSpeedInPercent / 100,
                                     qint64(std::numeric_limits<int>::max())));
            }
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
        // A restarted PortTime clock starts a new timestamp epoch. Any queued
        // cleanup from the previous epoch cannot still be pending here.
        lastStreamOutputTime=now;
        pendingCleanupTime=0;
        streamOutputBarrierActive=false;
        clockInitialized=true;
    } else {
        extendedClock+=quint32(now-lastClock);
    }
    lastClock=now;
    return extendedClock;
}

qint64 MidiInterface::currentPlaybackClockTime(bool& outputBlocked)
{
    const qint64 now=currentTimeMs();
    if(stateRestorationActive && stateRestorationFreezesClock)
    {
        outputBlocked=true;
        return stateRestorationHoldTime;
    }
    if(pendingPlaybackSpeedInPercent && now >= pendingPlaybackSpeedTime)
    {
        const qint64 position=(pendingPlaybackSpeedTime-timeAtTimestampZero) * relativePlaybackSpeedInPercent / 100;
        relativePlaybackSpeedInPercent=pendingPlaybackSpeedInPercent;
        timeAtTimestampZero=pendingPlaybackSpeedTime-position * 100 / relativePlaybackSpeedInPercent;
        pendingPlaybackSpeedInPercent=0;
    }
    if(streamOutputBarrierActive && now >= streamOutputBarrierEndTime)
        streamOutputBarrierActive=false;
    outputBlocked=streamOutputBarrierActive || pendingPlaybackSpeedInPercent != 0;
    return now;
}

void MidiInterface::failOutput(PmError error)
{
    if(outputFailed)return;
    setErrorText(error);
    outputFailed=true;
    clearThruState();
    pendingPlaybackSpeedInPercent=0;
    playMode=PM_Stop;
    pendingCleanupTime=0;
    streamOutputBarrierActive=false;
    outputImmediateMsgList.clear();
    cancelStateRestoration();
    for(auto* track : outputStreamTrackList)
    {
        track->playingNoteList.clear();
        track->clearHoldPedals();
    }
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

void MidiInterface::setMidiThru(bool enabled)
{
    QMutexLocker locker(&internalThreadMutex);
    if(midiThru && !enabled)releaseThruInputNotes(false);
    midiThru=enabled;
}

void MidiInterface::clearThruState(bool clearPending)
{
    midiThruUsedChannels=0;
    midiThruSustainChannels=0;
    midiThruSostenutoChannels=0;
    midiThruHold2Channels=0;
    for(auto& channel : midiThruNoteCounts)
        for(auto& count : channel)count=0;
    if(clearPending)
    {
        for(auto& channel : pendingThruNoteOffCounts)
            for(auto& count : channel)count=0;
        pendingThruNoteOffIndex=MIDI_INTERFACE_N_MIDI_CHANNELS*MIDI_INTERFACE_N_NOTE_NUMBERS;
        pendingThruNoteOffTime=0;
    }
}

void MidiInterface::rememberThruMessage(quint32 message)
{
    const int status=Pm_MessageStatus(message), command=status & 0xf0;
    const int channel=status & 0x0f, key=Pm_MessageData1(message), value=Pm_MessageData2(message);
    if(status < 0x80 || status >= 0xf0 || key >= MIDI_INTERFACE_N_NOTE_NUMBERS)return;
    midiThruUsedChannels |= quint16(1) << channel;
    auto& count=midiThruNoteCounts[channel][key];
    if(command == 0x90 && value != 0) { if(count != 0xffff)++count; }
    else if(command == 0x80 || command == 0x90) { if(count)--count; }
    else if(command == 0xb0)
    {
        quint16* holdChannels=key == 64 ? &midiThruSustainChannels :
                             key == 66 ? &midiThruSostenutoChannels :
                             key == 69 ? &midiThruHold2Channels : nullptr;
        if(holdChannels)
        {
            if(value >= 64)*holdChannels |= quint16(1) << channel;
            else *holdChannels &= ~(quint16(1) << channel);
        }
        if(key == 120 || key == 123)
            for(auto& held : midiThruNoteCounts[channel])held=0;
        // All Sound Off silences voices without resetting controller values.
        // Only Reset All Controllers (or a pedal release) clears held pedals.
        if(key == 121)
        {
            midiThruSustainChannels &= ~(quint16(1) << channel);
            midiThruSostenutoChannels &= ~(quint16(1) << channel);
            midiThruHold2Channels &= ~(quint16(1) << channel);
        }
    }
}

void MidiInterface::releaseThruInputNotes(bool allNotesOff)
{
    const quint16 usedChannels=midiThruUsedChannels;
    if(!outputDeviceOpened || outputFailed) { clearThruState(); return; }
    const qint64 now=currentTimeMs();
    const qint64 cleanupTime=qMax(lastStreamOutputTime + 1,now);
    for(int channel=0; channel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++channel)
    {
        const int before=outputImmediateMsgList.size();
        const bool held=(usedChannels & (quint16(1) << channel)) != 0;
        if(held)
        {
            if(allNotesOff || (midiThruSustainChannels & (quint16(1) << channel)))
                outputImmediateMsgList.append(MidiShortMsg(cleanupTime,0xb0+channel,64,0));
            if(midiThruSostenutoChannels & (quint16(1) << channel))
                outputImmediateMsgList.append(MidiShortMsg(cleanupTime,0xb0+channel,66,0));
            if(midiThruHold2Channels & (quint16(1) << channel))
                outputImmediateMsgList.append(MidiShortMsg(cleanupTime,0xb0+channel,69,0));
            if(allNotesOff)
                outputImmediateMsgList.append(MidiShortMsg(cleanupTime,0xb0+channel,123,0));
            else
                for(int note=0; note < MIDI_INTERFACE_N_NOTE_NUMBERS; ++note)
                    if(midiThruNoteCounts[channel][note])
                    {
                        pendingThruNoteOffCounts[channel][note]=midiThruNoteCounts[channel][note];
                        pendingThruNoteOffIndex=qMin(pendingThruNoteOffIndex,channel*MIDI_INTERFACE_N_NOTE_NUMBERS+note);
                        pendingThruNoteOffTime=qMax(pendingThruNoteOffTime,cleanupTime);
                    }
            if(playMode == PM_Play && (outputImmediateMsgList.size()!=before ||
                                      pendingThruNoteOffIndex<MIDI_INTERFACE_N_MIDI_CHANNELS*MIDI_INTERFACE_N_NOTE_NUMBERS))
            {
                streamOutputBarrierActive=true;
                streamOutputBarrierEndTime=qMax(streamOutputBarrierEndTime,cleanupTime);
            }
        }
    }
    clearThruState(false);
    midiInterfaceThread->triggerThread();
}

void MidiInterface::pollInput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!inputDeviceOpened)return;
    processImmediateOutput();
    // A quick re-enable must not let new Thru notes overtake old releases.
    // Continue observing keys while disabled; defer forwarding while enabled.
    if(midiThru && pendingThruNoteOffIndex<MIDI_INTERFACE_N_MIDI_CHANNELS*MIDI_INTERFACE_N_NOTE_NUMBERS)return;

    static const int INPUT_BUFFER_LENGTH=32;    // #events stored in buffer
    PmEvent eventBuffer[INPUT_BUFFER_LENGTH];

    bool keyStateChanged=false;
    while(true)
    {
        int numberOfEventsRead=Pm_Read(inputStream,eventBuffer,INPUT_BUFFER_LENGTH);
        if(numberOfEventsRead == 0)break; // no events available
        if(numberOfEventsRead < 0)
        {
            const PmError error=static_cast<PmError>(numberOfEventsRead);
            setErrorText(error);
            QString message=errorText;
            const bool fatal=error != pmBufferOverflow;

            // An overflow discards queued input events, so release any keys
            // that may otherwise remain visually held. PortMidi resumes input
            // after reporting this recoverable condition. Other errors make
            // the stream unusable, so close it and reset its device state.
            if(fatal)
            {
                const PmError closeError=Pm_Close(inputStream);
                inputDeviceOpened=false;
                inputDeviceID=pmNoDevice;
                inputStream=NULL;
                if(closeError != pmNoError)
                    message += QStringLiteral("\n") + QString::fromLocal8Bit(Pm_GetErrorText(closeError));
            }
            else
            {
                message=tr("MIDI input buffer overflowed. Some events were lost; held keys were released.");
            }

            releaseThruInputNotes();
            bool inputKeyStateChanged=false;
            for(int note=0; note < MIDI_INTERFACE_N_NOTE_NUMBERS; ++note)
            {
                if(midiKeyDownArray[note])
                {
                    emit midiKeyReleased(note);
                    inputKeyStateChanged=true;
                }
                midiKeyDownArray[note]=false;
                for(int channel=0; channel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++channel)
                    midiKeyDownChannelCounts[channel][note]=0;
            }
            if(inputKeyStateChanged)emit midiKeyStateChanged();
            emit midiInputError(message,fatal);
            break;
        }

        // If enabled, simulate MIDI Thru
        processImmediateOutput(); // Submit pending cleanup before new Thru notes.
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
                // Input backends can use a different clock epoch. Forward on
                // the output clock, following any events already queued there.
                const qint64 outputTime=qMax(lastStreamOutputTime,currentTimeMs());
                for(int i=0; i < numberOfEventsRead; ++i)
                    eventBuffer[i].timestamp=toPortMidiTimestamp(outputTime);
                const PmError error=Pm_Write(outputStream,eventBuffer,numberOfEventsRead);
                if(error < 0)failOutput(error);
                else lastStreamOutputTime=outputTime;
            }
            if(!outputFailed)
                for(int i=0; i < numberOfEventsRead; ++i)rememberThruMessage(eventBuffer[i].message);
        }

        // Analyse events
        for(int i=0; i < numberOfEventsRead; ++i)
        {
            quint32 message=eventBuffer[i].message;
            quint8 command = (quint8)((message      ) & 0xff);
            quint8 data1   = (quint8)((message >>  8) & 0xff);
            quint8 data2   = (quint8)((message >> 16) & 0xff);
            const int channel=command & 0x0f;

            if((command & 0xf0) == 0x90 && data2 != 0 &&
               data1 < MIDI_INTERFACE_N_NOTE_NUMBERS)
            {
                // Note-on
                int noteNumber=data1;
                quint16& channelCount=midiKeyDownChannelCounts[channel][noteNumber];
                if(channelCount < std::numeric_limits<quint16>::max())
                    ++channelCount;
                if(midiKeyDownArray[noteNumber] == false)
                {
                    // Key state changed to down
                    midiKeyDownArray[noteNumber]=true;
                    emit midiKeyPressed(noteNumber);
                    keyStateChanged=true;
                }
            }
            else if(data1 < MIDI_INTERFACE_N_NOTE_NUMBERS &&
                    ((command & 0xf0) == 0x80 ||
                     ((command & 0xf0) == 0x90 && data2 == 0)))
            {
                // Note-off
                int noteNumber=data1;
                quint16& channelCount=midiKeyDownChannelCounts[channel][noteNumber];
                if(channelCount > 0)
                    --channelCount;

                bool stillDown=false;
                for(int keyChannel=0; keyChannel < MIDI_INTERFACE_N_MIDI_CHANNELS; ++keyChannel)
                {
                    if(midiKeyDownChannelCounts[keyChannel][noteNumber] > 0)
                    {
                        stillDown=true;
                        break;
                    }
                }
                if(!stillDown && midiKeyDownArray[noteNumber])
                {
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

bool MidiInterface::processThruNoteOffs(qint64 now)
{
    const int end=MIDI_INTERFACE_N_MIDI_CHANNELS*MIDI_INTERFACE_N_NOTE_NUMBERS;
    int submitted=0;
#if defined(Q_OS_MACOS)
    if(appleGmOutputOpened && pendingThruNoteOffTime>now)return false;
#endif
    while(pendingThruNoteOffIndex<end)
    {
        const int channel=pendingThruNoteOffIndex/MIDI_INTERFACE_N_NOTE_NUMBERS;
        const int note=pendingThruNoteOffIndex%MIDI_INTERFACE_N_NOTE_NUMBERS;
        auto& count=pendingThruNoteOffCounts[channel][note];
        if(!count) { ++pendingThruNoteOffIndex; continue; }
        if(submitted==MIDI_INTERFACE_STATE_SUBMIT_BUDGET)return false;
        const qint64 time=qMax(lastStreamOutputTime,qMax(pendingThruNoteOffTime,now));
#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)sendAppleGmMessage(MidiShortMsg(time,0x80+channel,note,64));
        else
#endif
        {
            const PmError error=Pm_WriteShort(outputStream,toPortMidiTimestamp(time),Pm_Message(0x80+channel,note,64));
            if(error<0) { failOutput(error); return false; }
            lastStreamOutputTime=time;
        }
        if(outputFailed)return false;
        --count; ++submitted;
    }
    return true;
}

void MidiInterface::processImmediateOutput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!outputDeviceOpened || outputFailed)return;

    const qint64 currentTime=currentTimeMs();
    QList<MidiShortMsg> delayedMsgList;
    while(!outputImmediateMsgList.isEmpty())
    {
        MidiShortMsg msg=outputImmediateMsgList.takeFirst();
#if defined(Q_OS_MACOS)
        // PortMidi accepts scheduled messages immediately; delaying their
        // submission lets a restarted stream overtake its own cleanup.
        if(appleGmOutputOpened && msg.timestamp > currentTime)
        {
            delayedMsgList.append(msg);
            continue;
        }
#endif

        int32_t pm_msg;
        pm_msg= msg.data[0] + (((int32_t)msg.data[1]) << 8) + (((int32_t)msg.data[2]) << 16);

#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)
            sendAppleGmMessage(msg);
        else
#endif
        {
            const qint64 outputTime=qMax(lastStreamOutputTime,msg.timestamp > 0 ? msg.timestamp : currentTime);
            const PmTimestamp timestamp=toPortMidiTimestamp(outputTime);
            const PmError error=Pm_WriteShort(outputStream,timestamp,pm_msg);
            if(error < 0) { failOutput(error); return; }
            lastStreamOutputTime=outputTime;
        }
    }
    outputImmediateMsgList=delayedMsgList;
    processThruNoteOffs(currentTime);
    if(pendingCleanupTime > 0 && currentTime >= pendingCleanupTime)
        pendingCleanupTime=0;
}

void MidiInterface::beginStateRestoration(MidiStreamOutputTrack* track, int first, int end, bool consumesStream, bool freezesClock)
{
    if(first >= end)return;
    if(!stateRestorationActive)
    {
        bool blocked=false;
        stateRestorationHoldTime=currentPlaybackClockTime(blocked);
        if(freezesClock && consumesStream)
            stateRestorationHoldTime=qMax(stateRestorationHoldTime,
                    track->msgList[first].timestamp * 100 / relativePlaybackSpeedInPercent + timeAtTimestampZero);
        nextStateRestorationTime=qMax(lastStreamOutputTime + 1,currentTimeMs());
        lastStateRestorationTime=0;
        stateRestorationActive=true;
    }
    stateRestorationFreezesClock=stateRestorationFreezesClock || freezesClock;
    track->restoringState=true;
    track->stateRestorationPending=true;
    track->restorationConsumesStream=consumesStream;
    track->restoreNextMsgIndex=first;
    track->restoreEndMsgIndex=end;
    track->restoreReadyMsgIndex=-1;
}

void MidiInterface::cancelStateRestoration()
{
    stateRestorationActive=false;
    stateRestorationFreezesClock=false;
    nextStateRestorationTime=lastStateRestorationTime=0;
    for(auto* track : outputStreamTrackList)
    {
        track->restoringState=false;
        track->stateRestorationPending=false;
        track->restoreReadyMsgIndex=-1;
        track->restoreNextMsgIndex=track->restoreEndMsgIndex=0;
    }
}

bool MidiInterface::processStateRestoration()
{
    if(!stateRestorationActive)return false;
    bool blocked=false;
    currentPlaybackClockTime(blocked); // apply a due speed change during live restoration
    const qint64 now=currentTimeMs();
    const qint64 maxTime=now + MIDI_INTERFACE_THREAD_STREAM_COPY_IN_ADVANCE;
    int examined=0, submitted=0;
    bool timelineCaughtUp=true;
    if(!stateRestorationFreezesClock)
    {
        // A live-unmuted track stays silent until its state catches up with
        // the musical clock. Other tracks keep playing and releasing notes.
        // Consume elapsed target-track notes and append elapsed state to the
        // restoration range, reserving half the scan budget for replay.
        for(auto* track : outputStreamTrackList)
        {
            if(!track->stateRestorationPending || track->restorationConsumesStream)continue;
            while(track->nextStreamMsgIndex < track->msgList.size())
            {
                const auto& msg=track->msgList[track->nextStreamMsgIndex];
                const qint64 due=msg.timestamp * 100 / relativePlaybackSpeedInPercent + timeAtTimestampZero;
                if(due > now)break;
                if(examined == MIDI_INTERFACE_STATE_SCAN_BUDGET / 2)
                {
                    timelineCaughtUp=false;
                    break;
                }
                ++examined;
                ++track->nextStreamMsgIndex;
                track->restoreEndMsgIndex=track->nextStreamMsgIndex;
                track->restoringState=true;
            }
        }
    }
    while(submitted < MIDI_INTERFACE_STATE_SUBMIT_BUDGET)
    {
        // Prepare one candidate per track before choosing the earliest source
        // event. This preserves chronology even when tracks share a channel.
        bool headsReady=true;
        MidiStreamOutputTrack* earliest=nullptr;
        for(auto* track : outputStreamTrackList)
        {
            while(track->restoringState && track->restoreReadyMsgIndex < 0)
            {
                if(track->restoreNextMsgIndex == track->restoreEndMsgIndex)
                {
                    track->restoringState=false;
                    break;
                }
                if(examined == MIDI_INTERFACE_STATE_SCAN_BUDGET)
                {
                    headsReady=false;
                    break;
                }
                ++examined;
                const MidiShortMsg& msg=track->msgList[track->restoreNextMsgIndex];
                if(track->restorationConsumesStream && !msg.stateRestoration)
                {
                    track->restoringState=false;
                    break;
                }
                const quint8 family=msg.data[0] & 0xf0;
                if(family == 0xc0 || family == 0xd0 || family == 0xe0 ||
                   (family == 0xb0 && (msg.data[1] < 120 || msg.data[1] == 121)))
                {
                    track->restoreReadyMsgIndex=track->restoreNextMsgIndex;
                    break;
                }
                ++track->restoreNextMsgIndex;
                if(track->restorationConsumesStream)
                    track->nextStreamMsgIndex=track->restoreNextMsgIndex;
            }
            if(track->restoreReadyMsgIndex >= 0 &&
               (!earliest || track->msgList[track->restoreReadyMsgIndex].timestamp <
                            earliest->msgList[earliest->restoreReadyMsgIndex].timestamp))
                earliest=track;
        }
        if(!headsReady)return true;
        if(!earliest)break;
        const qint64 outputTime=qMax(qMax(nextStateRestorationTime,lastStreamOutputTime + 1),now);
        if(outputTime >= maxTime)return true;
#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened && outputTime > now)return true;
#endif
        const MidiShortMsg& msg=earliest->msgList[earliest->restoreReadyMsgIndex];
#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)
            sendAppleGmMessage(msg);
        else
#endif
        {
            const int32_t message=Pm_Message(msg.data[0],msg.data[1],msg.data[2]);
            const PmError error=Pm_WriteShort(outputStream,toPortMidiTimestamp(outputTime),message);
            if(error < 0) { failOutput(error); return true; }
        }
        earliest->rememberSustain(msg);
        lastStreamOutputTime=lastStateRestorationTime=outputTime;
        nextStateRestorationTime=outputTime + 1;
        earliest->restoreNextMsgIndex=earliest->restoreReadyMsgIndex + 1;
        if(earliest->restorationConsumesStream)
            earliest->nextStreamMsgIndex=earliest->restoreNextMsgIndex;
        earliest->restoreReadyMsgIndex=-1;
        ++submitted;
    }
    for(auto* track : outputStreamTrackList)
        if(track->restoringState)return true;
    if(!timelineCaughtUp)return true;
    // All state has been queued; wait at most the bounded backend horizon.
    // Initial seek/setup holds the clock before notes start. Live unmute keeps
    // the musical clock moving and only suppresses the restoring track.
    if(now < lastStateRestorationTime)return true;
    if(stateRestorationFreezesClock)
    {
        const qint64 position=(stateRestorationHoldTime-timeAtTimestampZero) * relativePlaybackSpeedInPercent / 100;
        if(pendingPlaybackSpeedInPercent)
            relativePlaybackSpeedInPercent=pendingPlaybackSpeedInPercent;
        pendingPlaybackSpeedInPercent=0;
        timeAtTimestampZero=now-position * 100 / relativePlaybackSpeedInPercent;
    }
    cancelStateRestoration();
    return false;
}

void MidiInterface::processStreamOutput()
{
    // called from MidiInterfaceThread while internalThreadMutex is locked
    if(!outputDeviceOpened || outputFailed)return;
    if(playMode != PM_Play)return;
    processImmediateOutput(); // Cleanup always precedes paced channel state.
    if(outputFailed)return;
    for(auto* track : outputStreamTrackList)
        if(!track->mute && !track->stateRestorationPending && track->nextStreamMsgIndex < track->msgList.size() &&
           track->msgList[track->nextStreamMsgIndex].stateRestoration)
            beginStateRestoration(track,track->nextStreamMsgIndex,track->msgList.size(),true,true);
    const bool restorationPending=processStateRestoration();
    if(outputFailed)return;
    if(restorationPending && stateRestorationFreezesClock)return;
    // A seek can require both past history and a tagged setup prefix. Start
    // the second phase in a later pass so the same scan/output budget applies.
    for(auto* track : outputStreamTrackList)
        if(!track->mute && !track->stateRestorationPending && track->nextStreamMsgIndex < track->msgList.size() &&
           track->msgList[track->nextStreamMsgIndex].stateRestoration)
            beginStateRestoration(track,track->nextStreamMsgIndex,track->msgList.size(),true,true);
    if(stateRestorationActive && stateRestorationFreezesClock)return;

    // Determine time slot of messages to copy to the buffer
    bool outputBlocked=false;
    qint64 currentTime=currentPlaybackClockTime(outputBlocked);
    if(outputBlocked)return;
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
        if(track->stateRestorationPending)continue;
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
                track->rememberSustain(msg);

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
        {
            sendAppleGmMessage(msg);
            lastStreamOutputTime=qMax(lastStreamOutputTime,currentTime);
        }
        else
#endif
        {
            // Preserve the low 32 clock bits without signed arithmetic overflow.
            const qint64 outputTime=qMax(qMax(msgTime,currentTime),lastStreamOutputTime);
            const PmTimestamp timestamp=toPortMidiTimestamp(outputTime);
            const PmError error=Pm_WriteShort(outputStream,timestamp,pm_msg);
            if(error < 0) { failOutput(error); return; }
            lastStreamOutputTime=qMax(lastStreamOutputTime,outputTime);
        }
    }

    // PortMidi schedules timestamped messages in the future. Do not stop the
    // stream merely because every message has been submitted to its queue.
    // Wait until the last submitted event has passed the output latency.
    if(!streamCompletionNotified &&
       numberOfTracksFinished == outputStreamTrackList.size())
    {
        qint64 outputLatency=MIDI_INTERFACE_LATENCY;
#if defined(Q_OS_MACOS)
        if(appleGmOutputOpened)outputLatency=0;
#endif
        if(currentTime >= lastStreamOutputTime + outputLatency)
        {
            streamCompletionNotified=true;
            emit midiStreamFinished();
        }
    }
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

void MidiStreamOutputTrack::rememberSustain(const MidiShortMsg& msg)
{
    if((msg.data[0] & 0xf0) != 0xb0)return;
    const quint16 channel=quint16(1) << (msg.data[0] & 0x0f);
    quint16* held=msg.data[1] == 64 ? &sustainChannels :
                  msg.data[1] == 66 ? &sostenutoChannels :
                  msg.data[1] == 69 ? &hold2Channels : nullptr;
    if(held)
    {
        if(msg.data[2] >= 64)*held |= channel;
        else *held &= ~channel;
    }
    else if(msg.data[1] == 121)
    {
        sustainChannels &= ~channel;
        sostenutoChannels &= ~channel;
        hold2Channels &= ~channel;
    }
}
