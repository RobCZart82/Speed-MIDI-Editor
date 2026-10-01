/***************************************************************************
 *  cs_playback.cpp - Controller Subsystem: Playback
 *                    (StreamPlayback, ListenChord, Stop, Track Flags)
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

#include "cs_playback.h"
#include <limits>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "speedymidiapp.h"
#include "settings.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "smfexporter.h"
#include "cs_navigation.h"
#include <algorithm>
#include "mousepianowidget.h"

#include <QMessageBox>
#include <QPushButton>

#define CS_PLAYBACK_POSITION_UPDATE_TIMER_INTERVAL         20

CS_Playback::CS_Playback(Controller* controller)
        : CS_Common(controller)
{
    playbackMode=PBM_None;
    playbackStartTicks=-1;

    timestampTranslationTableIndexCache=-1;

    playbackPositionUpdateTimerActive=false;
    playbackPositionUpdateTimerId=-1;

    Controller::ActionGuards gDIC = Controller::DisallowWhenInputCaptured;
    Controller::ActionGuards gMHT = Controller::MustHaveTracks;

    registerActionHandler(ui->actionPlayback_Stop, "actionPlayback_Stop_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_ReturnToStart, "actionPlayback_ReturnToStart_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_Play, "actionPlayback_Play_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_ListenChord, "actionPlayback_ListenChord_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_SetPlaybackSpeed, "actionPlayback_SetPlaybackSpeed", gDIC);
    registerActionHandler(ui->actionPlayback_TrackFlags_ToggleSolo, "actionPlayback_TrackFlags_ToggleSolo_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_TrackFlags_ToggleMute, "actionPlayback_TrackFlags_ToggleMute_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_TrackFlags_ToggleSoloExclusive, "actionPlayback_TrackFlags_ToggleSoloExclusive_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionPlayback_TrackFlags_ToggleMuteExclusive, "actionPlayback_TrackFlags_ToggleMuteExclusive_Triggered", gDIC | gMHT);

    ui->actionPlayback_Stop->setChecked(true);

    connect(mainWindow->getSpinBoxRelativePlaybackSpeed(), SIGNAL(valueChanged(int)), SLOT(spinBoxRelativePlaybackSpeedValueChanged(int)));
}

bool CS_Playback::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Escape:

        // stop stream or immediate-listen playback
        if(playbackMode != PBM_None)
        {
            stopPlayback();
            return true;
        }

        if(!capturingInput())
        {
            // switch off all pressed keys on mouse piano
            app->getMousePianoWidget()->allNotesOff();
        }
        break;
    }

    // During immediate listen mode, input is captured. Reach event also to navigation subsystem.
    if(playbackMode == PBM_ImmediateListen)
    {
        Q_ASSERT(capturingInput());

        CS_Navigation* csNavigation=
                qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
        if(csNavigation != NULL)return csNavigation->keyPressEvent(event);
    }

    return false;   // event not handled
}

void CS_Playback::keyReleaseEvent(QKeyEvent* event)
{
    if(!event->isAutoRepeat())
    {
        // disabling of keyboard modal states
        switch(event->key())
        {
        case Qt::Key_F7:
            if(playbackMode == PBM_ImmediateListen)
                stopPlayback();
            break;
        }
    }
}

bool CS_Playback::mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone)
{
    bool modShift=(event->modifiers() & Qt::ShiftModifier  ) != 0;

    if(event->button() == Qt::LeftButton)
    {
        switch(mouseZone.zoneType)
        {
        case View::TrackButtonSolo:
            toggleTrackFlags(TF_Solo,mouseZone.trackIndex,mouseZone.trackIndex,modShift);
            setPlaybackMuteConfiguration_Live();
            return true;
        case View::TrackButtonMute:
            toggleTrackFlags(TF_Mute,mouseZone.trackIndex,mouseZone.trackIndex,modShift);
            setPlaybackMuteConfiguration_Live();
            return true;
        case View::TrackButtonRecord:
            toggleTrackFlags(TF_Record,mouseZone.trackIndex,mouseZone.trackIndex,modShift);
            return true;
        default:
            // do nothing
            break;
        }
    }

    // During immediate listen mode, input is captured. Reach event also to navigation subsystem.
    if(playbackMode == PBM_ImmediateListen)
    {
        Q_ASSERT(capturingInput());

        CS_Navigation* csNavigation=
                qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
        if(csNavigation != NULL)return csNavigation->mousePressEvent(event, mouseZone);

    }

    return false;   // event not handled
}

bool CS_Playback::wheelEvent(QWheelEvent* event)
{
    // During immediate listen mode, input is captured. Reach event also to navigation subsystem.
    if(playbackMode == PBM_ImmediateListen)
    {
        Q_ASSERT(capturingInput());

        CS_Navigation* csNavigation=
                qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
        if(csNavigation != NULL)return csNavigation->wheelEvent(event);
    }

    return false;   // event not handled
}

void CS_Playback::stateChanged()
{
    // If in immediate-listen playback mode and the left selection border has changed, hit the notes again
    if(playbackMode == PBM_ImmediateListen &&
       playbackStartTicks != getEditorState().selection.ticksLeft)
    {
        stopPlayback();
        startImmediateListenPlayback(false);
    }
}

void CS_Playback::cancelInterruptibleState()
{
    stopPlayback();
}

void CS_Playback::cancelInputCapture()
{
    stopPlayback();
}

void CS_Playback::timerEvent(QTimerEvent* event)
{
    if(playbackPositionUpdateTimerActive && event->timerId() == playbackPositionUpdateTimerId)
    {
        MidiInterface* midiInterface=app->getMidiInterface();
        const int timestamp=midiInterface->getCurrentPlayTimestamp();
        // A backend failure can stop the worker before its queued notification
        // reaches the GUI. Never use the stopped sentinel as a table index.
        if(timestamp < 0) { stopPlayback(); return; }
        int ticks=timestampToTicks(timestamp);

        // In scrolling playback mode, scroll a page to the right as often as necessary
        if(settings->scrollingPlayback)
        {
            while(view->getMapper()->ticksToViewX(ticks).cellRightX >=
                  view->getCellArea().right() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
            {
                // Make a test scroll towards "ticks"
                EditorState testState=getEditorState();
                scrollRangeIntoView(testState,EditorRange(ticks,-1,ticks,-1));

                // Emit a page right command only if we do not overscroll "ticks"
                if(testState.firstMeasure > getEditorState().firstMeasure)
                {
                    CS_Navigation* csNavigation=
                            qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
                    if(csNavigation != NULL)csNavigation->scrollBarHorizontalPageStepAdd();
                }
                else
                    break;
            }
        }

        // 2. Redraw playback position line
        VolatileEditorState newVolatileState=getVolatileEditorState();

        TicksToViewXResult res=view->getMapper()->ticksToViewX(ticks);
        newVolatileState.playbackLineCellX=res.cellLeftX + res.cellInternalOffsetX;

        applyVolatileStateAndUpdate(newVolatileState);
    }
}

void CS_Playback::actionPlayback_Stop_Triggered()
{
    stopPlayback();
}

void CS_Playback::actionPlayback_ReturnToStart_Triggered()
{
    stopPlayback();

    EditorState newState=getEditorState();
    newState.firstMeasure=0;
    newState.firstTrack=0;
    newState.setStartupSelection(docRoot);
    applyStateAndUpdate(newState);
}

void CS_Playback::actionPlayback_Play_Triggered()
{
    startStreamPlayback();
}

void CS_Playback::actionPlayback_ListenChord_Triggered()
{
    startImmediateListenPlayback(true);
}

void CS_Playback::actionPlayback_SetPlaybackSpeed()
{
    app->showToolBar(true);
    mainWindow->getSpinBoxRelativePlaybackSpeed()->setFocus();
}

void CS_Playback::actionPlayback_TrackFlags_ToggleSolo_Triggered()
{
    toggleTrackFlags(TF_Solo,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     false);
    setPlaybackMuteConfiguration_Live();
}

void CS_Playback::actionPlayback_TrackFlags_ToggleMute_Triggered()
{
    toggleTrackFlags(TF_Mute,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     false);
    setPlaybackMuteConfiguration_Live();
}

void CS_Playback::actionPlayback_TrackFlags_ToggleSoloExclusive_Triggered()
{
    toggleTrackFlags(TF_Solo,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     true);
    setPlaybackMuteConfiguration_Live();
}

void CS_Playback::actionPlayback_TrackFlags_ToggleMuteExclusive_Triggered()
{
    toggleTrackFlags(TF_Mute,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     true);
    setPlaybackMuteConfiguration_Live();
}

void CS_Playback::startStreamPlayback()
{
    if(playbackMode == PBM_Stream)return;   // already playing in correct mode

    if(!app->isMidiOutputAvailable())
    {
        QMessageBox::warning(mainWindow,tr("Playback"),
                             tr("Please select a MIDI output device."));
        app->execPreferencesDialog(true);
        return;
    }

    if(!checkForChannelCollisions())return;

    MidiInterface* midiInterface=app->getMidiInterface();
    app->stopAnyPlayback();  // stop any stream or immediate-listen playback (even for *this)

    Q_ASSERT(midiInterface->getPlayMode() == MidiInterface::PM_Stop);

    playbackMode=PBM_Stream;
    setInterruptibleState();

    ui->actionPlayback_Stop->setChecked(false);
    ui->actionPlayback_Play->setChecked(true);

    // Determine start of playback
    switch(settings->playbackStartPosition)
    {
    case Settings::PSP_fromBeginning:
        playbackStartTicks=0;
        break;
    case Settings::PSP_fromLeftmostVisibleMeasure:
        playbackStartTicks=docRoot->measureToTicks(getEditorState().firstMeasure);
        break;
    case Settings::PSP_fromCursorPosition:
        playbackStartTicks=getEditorState().selection.ticksLeft;
        break;
    }

    rebuildTimestampTranslationTable();

    // Convert and add tracks to MIDI interface stream playback interface
    for(int i=0; i < docRoot->trackList.size(); ++i)
    {
        QList<MidiShortMsg> msgList;
        convertTrackToShortMessages(i,msgList);
        midiInterface->addStreamOutputTrack(msgList);
    }

    // mute and unmute tracks according to the tracks' mute and solo flags
    setPlaybackMuteConfiguration();

    // Start playing from defined playback start position, converted to timestamp value
    connect(midiInterface,SIGNAL(midiStreamFinished()),SLOT(midiStreamFinished()));
    midiInterface->setRelativePlaybackSpeed(mainWindow->getSpinBoxRelativePlaybackSpeed()->value());

    if(!midiInterface->play(ticksToTimestamp(playbackStartTicks)))
    {
        QString errorText=midiInterface->getErrorText();

        stopPlayback();
        QMessageBox::warning(mainWindow,tr("Playback error"),
                             tr("Unable to start playback:") + "\n\n" + errorText);
        return;
    }

    playbackPositionUpdateTimerId=startTimer(CS_PLAYBACK_POSITION_UPDATE_TIMER_INTERVAL);
    playbackPositionUpdateTimerActive=true;

    // scroll playback start position into view
    EditorState newState=getEditorState();
    scrollRangeIntoView(newState,EditorRange(playbackStartTicks,-1,playbackStartTicks,-1));
    applyStateAndUpdate(newState);
}

void CS_Playback::startImmediateListenPlayback(bool makeChecks)
{
    if(!app->isMidiOutputAvailable())
    {
        QMessageBox::warning(mainWindow,tr("Playback"),
                             tr("Please select a MIDI output device."));
        app->execPreferencesDialog(true);
        return;
    }

    if(makeChecks)
    {
        if(!checkForChannelCollisions())return;
    }

    MidiInterface* midiInterface=app->getMidiInterface();
    app->stopAnyPlayback();  // stop any stream or immediate-listen playback (even for *this)

    Q_ASSERT(midiInterface->getPlayMode() == MidiInterface::PM_Stop);

    playbackMode=PBM_ImmediateListen;
    setInterruptibleState();

    setInputCapture();

    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.keyboardDragMode=VolatileEditorState::KDM_ImmediateListen;  // show special selection color
    applyVolatileStateAndUpdate(newVolatileState);

    // Start of playback is always left border of current selection
    playbackStartTicks=getEditorState().selection.ticksLeft;

    // Convert and add tracks to MIDI interface stream playback interface
    for(int i=0; i < docRoot->trackList.size(); ++i)
    {
        QList<MidiShortMsg> msgList;
        convertTrackToShortMessages(i,msgList);
        midiInterface->addStreamOutputTrack(msgList);
    }

    // mute and unmute tracks according to the tracks' mute and solo flags
    setPlaybackMuteConfiguration();

    // Start playing from timestamp 0
    connect(midiInterface,SIGNAL(midiStreamFinished()),SLOT(midiStreamFinished()));
    if(!midiInterface->play(0))
    {
        QString errorText=midiInterface->getErrorText();

        stopPlayback();
        QMessageBox::warning(mainWindow,tr("Playback error"),
                             tr("Unable to start playback:") + "\n\n" + errorText);
        return;
    }

    EditorState newState=getEditorState();

    // Scroll playback start position into view
    scrollRangeIntoView(newState,EditorRange(playbackStartTicks,-1,playbackStartTicks,-1));

    // Select only one cell in horizontal direction, always in local cell selection mode
    newState.selection.trackTop=newState.firstSelectedTrack();
    newState.selection.ticksRight=docRoot->roundUpTicksToCellBorder(newState.selection.ticksLeft + 1, newState.writeLength);
    newState.selection.trackBottom=newState.lastSelectedTrack(docRoot);

    // Reset anchor cell
    newState.selection.anchor.setTo(
            newState.selection.ticksLeft,
            newState.selection.ticksRight,
            newState.selection.trackTop);

    applyStateAndUpdate(newState);

    controller->restoreMouseCursor();     // mouse cursor can differ during keyboard drag mode != KDM_None
}

bool CS_Playback::checkForChannelCollisions()
{
    if(!settings->checkForChannelCollisions)return true;

    // traverse all channels and check whether they have the same channel assigned
    bool channelUsed[MIDI_MAX_CHANNEL - MIDI_MIN_CHANNEL + 1] = {};
    for(int ch=MIDI_MIN_CHANNEL; ch <= MIDI_MAX_CHANNEL; ++ch)
        channelUsed[ch - MIDI_MIN_CHANNEL]=false;

    for(int i=0; i < docRoot->trackList.size(); ++i)
    {
        int midiChannel=docRoot->trackList[i]->midiChannel;
        if(midiChannel < MIDI_MIN_CHANNEL || midiChannel > MIDI_MAX_CHANNEL)
        {
            QMessageBox::warning(mainWindow, tr("Invalid MIDI channel"),
                                 tr("Track %1 has an invalid MIDI channel (%2).")
                                 .arg(i + 1).arg(midiChannel));
            return false;
        }
        if(channelUsed[midiChannel - MIDI_MIN_CHANNEL])
        {
            // found a collision, show a message to the user
            QMessageBox msgBox(QMessageBox::Warning,
                               tr("MIDI channel collision detected"),
                               tr("Some tracks use the same MIDI channel.\n"
                                  "This can lead to wrong settings for patches, volume, "
                                  "and panorama during playback."),
                               QMessageBox::Ignore|QMessageBox::Cancel, mainWindow);

            QAbstractButton* buttonDontShowAgain=msgBox.addButton(tr("Ignore && Never Warn Again"),
                                                                  QMessageBox::AcceptRole);
            switch(msgBox.exec())
            {
            case QMessageBox::Ignore:
                return true;
            case QMessageBox::Cancel:
                return false;
            default:
                if(msgBox.clickedButton() == buttonDontShowAgain)
                {
                    settings->checkForChannelCollisions=false;
                    return true;
                }
                return false;
            }
        }

        // Mark channel as used
        channelUsed[midiChannel - MIDI_MIN_CHANNEL]=true;
    }

    return true;    // found no collisions
}

void CS_Playback::setPlaybackMuteConfiguration()
{
    MidiInterface* midiInterface=app->getMidiInterface();

    // Check if any track has the solo flag set
    bool anySoloEnabled=false;
    for(int i=0; i < getEditorState().trackStateList.size(); ++i)
    {
        if(getEditorState().trackStateList[i].solo)
        {
            anySoloEnabled=true;
            break;
        }
    }

    if(anySoloEnabled)
    {
        // mute the tracks that have the solo flag unset
        for(int i=0; i < getEditorState().trackStateList.size(); ++i)
            midiInterface->setMute(i, getEditorState().trackStateList[i].solo == false);
    }
    else
    {
        // mute the tracks with mute flag set
        for(int i=0; i < getEditorState().trackStateList.size(); ++i)
            midiInterface->setMute(i, getEditorState().trackStateList[i].mute);
    }
}

void CS_Playback::setPlaybackMuteConfiguration_Live()
{
    // When playback is active, make a live update of solo and mute flags
    if(playbackMode != PBM_None)setPlaybackMuteConfiguration();
    if(playbackMode == PBM_ImmediateListen)
    {
        // hit the notes in selected cell again
        stopPlayback();
        startImmediateListenPlayback(false);
    }
}

void CS_Playback::convertTrackToShortMessages(int trackIndex, QList<MidiShortMsg>& msgList)
{
    DocTrack* track=docRoot->trackList[trackIndex];
    const auto initialSetup=track->initialMidiSetup();

    // 1. Prepare list of SmfExporterMidiEvent objects for sorting
    QList<SmfExporterMidiEvent*> eventList;
    int playbackStartStateEventIndex=0;

    // MIDI volume
    SmfExporterMidiEvent* volumeMidiEvent=new SmfExporterMidiEvent;
    volumeMidiEvent->tickPosition=playbackStartTicks;
    volumeMidiEvent->midiCommand[0]= 0xb0 + track->midiChannel - 1; // MIDI command: set controller
    volumeMidiEvent->midiCommand[1]= 0x07;                          // volume controller
    volumeMidiEvent->midiCommand[2]= track->midiVolume;
    volumeMidiEvent->stateRestoration=playbackMode == PBM_Stream;
    volumeMidiEvent->beforeNoteEvents=true;     // for stable-sort
    volumeMidiEvent->index=playbackStartStateEventIndex++; // for stable-sort
    eventList.append(volumeMidiEvent);

    // MIDI panorama
    SmfExporterMidiEvent* panoramaMidiEvent=new SmfExporterMidiEvent;
    panoramaMidiEvent->tickPosition=playbackStartTicks;
    panoramaMidiEvent->midiCommand[0]= 0xb0 + track->midiChannel - 1; // MIDI command: set controller
    panoramaMidiEvent->midiCommand[1]= 0x0a;                          // panorama controller
    panoramaMidiEvent->midiCommand[2]= track->midiPanorama;
    panoramaMidiEvent->stateRestoration=playbackMode == PBM_Stream;
    panoramaMidiEvent->beforeNoteEvents=true;     // for stable-sort
    panoramaMidiEvent->index=playbackStartStateEventIndex++; // for stable-sort
    eventList.append(panoramaMidiEvent);

    // MIDI patch
    SmfExporterMidiEvent* patchMidiEvent=new SmfExporterMidiEvent;
    patchMidiEvent->tickPosition=playbackStartTicks;
    patchMidiEvent->midiCommand[0]= 0xc0 + track->midiChannel - 1; // MIDI command: program change
    patchMidiEvent->midiCommand[1]= track->midiPatch - 1;
    patchMidiEvent->stateRestoration=playbackMode == PBM_Stream;
    patchMidiEvent->beforeNoteEvents=true;      // for stable-sort
    patchMidiEvent->index=playbackStartStateEventIndex++; // for stable-sort
    eventList.append(patchMidiEvent);

    QList<SmfExporterMidiEvent*> priorStateEvents;
    for(DocEvent* event=track->firstEvent; event; event=event->nextEvent)
    {
        // Reapply prior channel state at the seek position before notes begin.
        // Note events themselves are intentionally not replayed here: active
        // notes crossing the seek point are scheduled by the normal note path.
        if(playbackMode == PBM_Stream && event->tickPosition < playbackStartTicks &&
           event->type == DocEvent::E_OtherMidi)
        {
            const quint8 command=event->otherMidiEventData.midiCommand[0];
            const quint8 commandFamily=command & 0xf0;
            if(commandFamily == 0xb0 || commandFamily == 0xc0 ||
               commandFamily == 0xd0 || commandFamily == 0xe0)
            {
                SmfExporterMidiEvent* stateEvent=new SmfExporterMidiEvent;
                stateEvent->tickPosition=event->tickPosition;

                quint8 outputCommand=command;
                if(command < 0xf0)
                {
                    outputCommand=static_cast<quint8>((command & 0xf0) + track->midiChannel - 1);
                }

                stateEvent->midiCommand[0]=outputCommand;
                stateEvent->midiCommand[1]=event->otherMidiEventData.midiCommand[1];
                stateEvent->midiCommand[2]=event->otherMidiEventData.midiCommand[2];
                if(initialSetup.hasSysEx)initialSetup.applyProperties(*track,event,stateEvent->midiCommand);
                stateEvent->stateRestoration=true;
                stateEvent->beforeNoteEvents=true;
                stateEvent->index=event->otherMidiEventData.sameTickSubOrdering.index;
                stateEvent->importOrder=event->otherMidiEventData.importOrder;
                priorStateEvents.append(stateEvent);
            }
        }
    }
    std::stable_sort(priorStateEvents.begin(),priorStateEvents.end(),eventPlaybackOrderingLessThan);
    for(SmfExporterMidiEvent* stateEvent : priorStateEvents)
    {
        stateEvent->tickPosition=playbackStartTicks;
        stateEvent->importOrder=-1;
        stateEvent->index=playbackStartStateEventIndex++;
        eventList.append(stateEvent);
    }

    int playbackStartCellRightTicks=docRoot->roundUpTicksToCellBorder(playbackStartTicks + 1,
                                                                      getEditorState().writeLength);

    // Event list
    DocEvent* event=track->firstEvent;
    while(event)
    {
        // Skip all events that have ended before playbackStartTicks. Swing is ignored in this case.
        if(event->tickPositionEnd() <= playbackStartTicks)
        {
            // skip this event
            event=event->nextEvent;
            continue;
        }

        DocEvent::SwingPosition swingPosition = event->calculateSwingStartAndEndTicks(docRoot);

        // if event starts before playback start position, adjust beginning
        int adjustedStartTicks=qMax(swingPosition.startTicks,playbackStartTicks);

        switch(event->type)
        {
        case DocEvent::E_Note:
            {
                if(playbackMode == PBM_Stream)
                {
                    SmfExporterMidiEvent* noteOnEvent=new SmfExporterMidiEvent;
                    noteOnEvent->tickPosition=adjustedStartTicks;
                    noteOnEvent->midiCommand[0]=0x90 + track->midiChannel - 1;  // MIDI command: note-on
                    noteOnEvent->midiCommand[1]=event->noteEventData.noteNumber;
                    noteOnEvent->midiCommand[2]=event->noteEventData.velocity;
                    noteOnEvent->index=-1;
                    noteOnEvent->importOrder=event->noteEventData.importOnOrder;
                    eventList.append(noteOnEvent);

                    SmfExporterMidiEvent* noteOffEvent=new SmfExporterMidiEvent;
                    noteOffEvent->tickPosition=swingPosition.endTicks;
                    noteOffEvent->midiCommand[0]=0x80 + track->midiChannel - 1;  // MIDI command: note-off
                    noteOffEvent->midiCommand[1]=event->noteEventData.noteNumber;
                    noteOffEvent->midiCommand[2]=event->noteEventData.releaseVelocity;
                    noteOffEvent->index=-1;
                    noteOffEvent->importOrder=event->noteEventData.importOffOrder;
                    eventList.append(noteOffEvent);
                }
                else if(playbackMode == PBM_ImmediateListen)
                {
                    // add only note-on events for notes touching the cell starting at playbackStartTicks
                    if(adjustedStartTicks >= playbackStartTicks && adjustedStartTicks < playbackStartCellRightTicks)
                    {
                        SmfExporterMidiEvent* noteOnEvent=new SmfExporterMidiEvent;
                        noteOnEvent->tickPosition=adjustedStartTicks;
                        noteOnEvent->midiCommand[0]=0x90 + track->midiChannel - 1;  // MIDI command: note-on
                        noteOnEvent->midiCommand[1]=event->noteEventData.noteNumber;
                        noteOnEvent->midiCommand[2]=event->noteEventData.velocity;
                        noteOnEvent->index=-1;
                        noteOnEvent->importOrder=event->noteEventData.importOnOrder;
                        eventList.append(noteOnEvent);
                    }
                }
            }
            break;
        case DocEvent::E_OtherMidi:
            {
                if(playbackMode == PBM_Stream)
                {
                    SmfExporterMidiEvent* otherMidiEvent=new SmfExporterMidiEvent;
                    otherMidiEvent->tickPosition=adjustedStartTicks;

                    int command=event->otherMidiEventData.midiCommand[0];
                    if(command >= 0x80 && command < 0xf0)
                    {
                        // Change channel of command
                        command &= 0xf0;
                        command += track->midiChannel - 1;
                    }

                    otherMidiEvent->midiCommand[0]=command;
                    otherMidiEvent->midiCommand[1]=event->otherMidiEventData.midiCommand[1];
                    otherMidiEvent->midiCommand[2]=event->otherMidiEventData.midiCommand[2];
                    if(initialSetup.hasSysEx)initialSetup.applyProperties(*track,event,otherMidiEvent->midiCommand);
                    otherMidiEvent->beforeNoteEvents=event->otherMidiEventData.sameTickSubOrdering.beforeNoteEvents;
                    otherMidiEvent->index=event->otherMidiEventData.sameTickSubOrdering.index;
                    otherMidiEvent->importOrder=event->otherMidiEventData.importOrder;
                    if(event->tickPosition == playbackStartTicks && otherMidiEvent->beforeNoteEvents)
                        otherMidiEvent->index+=playbackStartStateEventIndex;
                    eventList.append(otherMidiEvent);
                }
            }
            break;
        case DocEvent::E_SysEx:
            // Preserve opaque packets in files; current MIDI output accepts only short messages.
            break;
        case DocEvent::E_Meta:
            // not used for playback
            break;
        default:
            Q_ASSERT(false);
            break;
        }
        event=event->nextEvent;
    }

    // Stable-Sort SmfExporterMidiEvent objects
    std::stable_sort(eventList.begin(),eventList.end(),eventPlaybackOrderingLessThan);

    // 2. convert to short messages
    for(int i=0; i < eventList.size(); ++i)
    {
        SmfExporterMidiEvent* midiEvent=eventList[i];

        int timestamp=-1;   // invalidate variable
        if(playbackMode == PBM_Stream)
        {
            timestamp=ticksToTimestamp(midiEvent->tickPosition);
        }
        else if(playbackMode == PBM_ImmediateListen)
        {
            timestamp=0;
        }

        MidiShortMsg msg(timestamp,
                         midiEvent->midiCommand[0],
                         midiEvent->midiCommand[1],
                         midiEvent->midiCommand[2]);
        msg.stateRestoration=midiEvent->stateRestoration;
        msgList.append(msg);
    }

    // delete SmfExporterMidiEvent objects
    for(int i=0; i < eventList.size(); ++i)
        delete eventList[i];
    eventList.clear();
}

void CS_Playback::rebuildTimestampTranslationTable()
{
    timestampTranslationTable.clear();
    DocMeasureItem effective=docRoot->getFirstMeasureEffectiveProperties();
    qint64 elapsedMicrosecondTicks=0;
    for(const DocMeasureItem* item : docRoot->measureItemList)
    {
        effective.makeEffectiveMeasureProperties(*item);
        if(!item->setTempo)continue;
        TimestampTranslationTableEntry entry;
        entry.ticksLeft=item->tickPosition;
        entry.ticksRight=INT_MAX;
        entry.timestampRight=std::numeric_limits<double>::infinity();
        entry.microsecondsPerQuarter=effective.tempoMicrosecondsPerQuarter(effective.timeSignatureDenominator);
        entry.relativeTicksToTimestampFactor=double(entry.microsecondsPerQuarter) /
                (1000.0 * (docRoot->midiTicksPerWholeNote / 4));
        if(!timestampTranslationTable.isEmpty())
        {
            TimestampTranslationTableEntry& previous=timestampTranslationTable.last();
            previous.ticksRight=entry.ticksLeft;
            elapsedMicrosecondTicks += qint64(entry.ticksLeft - previous.ticksLeft) *
                    previous.microsecondsPerQuarter;
            previous.timestampRight=double(elapsedMicrosecondTicks) /
                    (1000.0 * (docRoot->midiTicksPerWholeNote / 4));
        }
        entry.elapsedMicrosecondTicks=elapsedMicrosecondTicks;
        entry.timestampLeft=double(elapsedMicrosecondTicks) /
                (1000.0 * (docRoot->midiTicksPerWholeNote / 4));
        timestampTranslationTable.append(entry);
    }
    timestampTranslationTableIndexCache=0;
}

int CS_Playback::ticksToTimestamp(int ticks)
{
    Q_ASSERT(playbackMode != PBM_None);
    Q_ASSERT(!timestampTranslationTable.isEmpty());
    Q_ASSERT(timestampTranslationTableIndexCache >= 0 &&
             timestampTranslationTableIndexCache < timestampTranslationTable.size());

    // Try to use cached index for a O(1) operation
    TimestampTranslationTableEntry tte=timestampTranslationTable[timestampTranslationTableIndexCache];

    // look before current entry
    while(timestampTranslationTableIndexCache > 0 && ticks < tte.ticksLeft)   // use < for left border
    {
        --timestampTranslationTableIndexCache;
        tte=timestampTranslationTable[timestampTranslationTableIndexCache];
    }

    // look after current entry
    while(timestampTranslationTableIndexCache + 1 < timestampTranslationTable.size() && ticks >= tte.ticksRight) // use >= for right border
    {
        ++timestampTranslationTableIndexCache;
        tte=timestampTranslationTable[timestampTranslationTableIndexCache];
    }

    if(tte.microsecondsPerQuarter > 0)
    {
        const qint64 numerator=tte.elapsedMicrosecondTicks +
                qint64(ticks - tte.ticksLeft) * tte.microsecondsPerQuarter;
        const qint64 milliseconds=numerator / (qint64(1000) * (docRoot->midiTicksPerWholeNote / 4));
        return int(qBound<qint64>(qint64(0),milliseconds,qint64(INT_MAX)));
    }
    return int(qBound(0.0,tte.timestampLeft +
                     (ticks-tte.ticksLeft) * tte.relativeTicksToTimestampFactor,double(INT_MAX)));
}

int CS_Playback::timestampToTicks(int timestamp)
{
    Q_ASSERT(playbackMode != PBM_None);
    Q_ASSERT(!timestampTranslationTable.isEmpty());
    Q_ASSERT(timestampTranslationTableIndexCache >= 0 &&
             timestampTranslationTableIndexCache < timestampTranslationTable.size());

    // Try to use cached index for a O(1) operation
    TimestampTranslationTableEntry tte=timestampTranslationTable[timestampTranslationTableIndexCache];

    // look before current entry
    while(timestampTranslationTableIndexCache > 0 && timestamp < tte.timestampLeft)   // use < for left border
    {
        --timestampTranslationTableIndexCache;
        tte=timestampTranslationTable[timestampTranslationTableIndexCache];
    }

    // look after current entry
    while(timestampTranslationTableIndexCache + 1 < timestampTranslationTable.size() && timestamp >= tte.timestampRight) // use >= for right border
    {
        ++timestampTranslationTableIndexCache;
        tte=timestampTranslationTable[timestampTranslationTableIndexCache];
    }

    if(tte.microsecondsPerQuarter > 0)
    {
        const qint64 numerator=qint64(timestamp) * 1000 * (docRoot->midiTicksPerWholeNote / 4) -
                tte.elapsedMicrosecondTicks;
        return int(qBound<qint64>(qint64(0),qint64(tte.ticksLeft) + numerator / tte.microsecondsPerQuarter,qint64(INT_MAX)));
    }
    return int(qBound(0.0,tte.ticksLeft +
                     (timestamp-tte.timestampLeft) / tte.relativeTicksToTimestampFactor,double(INT_MAX)));
}

bool CS_Playback::eventPlaybackOrderingLessThan(SmfExporterMidiEvent* e1, SmfExporterMidiEvent* e2)
{
    // see also SmfExporter::eventOrderingLessThan(...)

    // First criterion: tick position
    if(e1->tickPosition < e2->tickPosition)return true;
    if(e1->tickPosition > e2->tickPosition)return false;

    return SmfExporterMidiEvent::sameTickLessThan(e1,e2);
}

void CS_Playback::midiStreamFinished()
{
    if(playbackMode == PBM_Stream)stopPlayback();
}

void CS_Playback::stopPlayback()
{
    if(playbackMode == PBM_None)return;     // already stopped

    if(playbackPositionUpdateTimerActive)
    {
        killTimer(playbackPositionUpdateTimerId);
        playbackPositionUpdateTimerActive=false;
    }

    MidiInterface* midiInterface=app->getMidiInterface();
    midiInterface->stop();
    midiInterface->resetStreamOutputTracks();
    disconnect(midiInterface,SIGNAL(midiStreamFinished()),this,SLOT(midiStreamFinished()));

    VolatileEditorState newVolatileState=getVolatileEditorState();

    if(playbackMode == PBM_ImmediateListen)
    {
        releaseInputCapture();

        // remove special selection color (immediate listen mode)
        newVolatileState.keyboardDragMode=VolatileEditorState::KDM_None;
    }

    playbackMode=PBM_None;
    releaseInterruptibleState();

    ui->actionPlayback_Stop->setChecked(true);
    ui->actionPlayback_Play->setChecked(false);

    // remove playback position line
    newVolatileState.playbackLineCellX=-1;
    applyVolatileStateAndUpdate(newVolatileState);
    controller->restoreMouseCursor();     // mouse cursor can differ during keyboard drag mode != KDM_None

    timestampTranslationTableIndexCache=-1;
    timestampTranslationTable.clear();
}

void CS_Playback::spinBoxRelativePlaybackSpeedValueChanged(int value)
{
    MidiInterface* midiInterface=app->getMidiInterface();
    midiInterface->setRelativePlaybackSpeed(value);
}
