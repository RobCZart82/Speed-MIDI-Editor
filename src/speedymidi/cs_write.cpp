/***************************************************************************
 *  cs_write.cpp - Controller Subsystem: Write
 *                 (Write/Extend Note)
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

#include "cs_write.h"
#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "commands.h"

#include <QMessageBox>

CS_Write::CS_Write(Controller* controller)
        : CS_Common(controller)
{
    Controller::ActionGuards gDIC = Controller::DisallowWhenInputCaptured;
    Controller::ActionGuards gCIS = Controller::CancelInterruptibleStates;
    Controller::ActionGuards gMHT = Controller::MustHaveTracks;

    Controller::ActionGuards gWrite = gDIC | gCIS | gMHT;

    registerActionHandler(ui->actionWrite_WriteOrExtendNote, "actionWrite_WriteOrExtendNote_Triggered", gWrite);

    registerActionHandler(ui->actionWrite_Write1, "actionWrite_Write1_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write2, "actionWrite_Write2_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write3, "actionWrite_Write3_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write4, "actionWrite_Write4_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write5, "actionWrite_Write5_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write6, "actionWrite_Write6_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write7, "actionWrite_Write7_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write8, "actionWrite_Write8_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write9, "actionWrite_Write9_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Write10, "actionWrite_Write10_Triggered", gWrite);

    registerActionHandler(ui->actionWrite_ExtendNote, "actionWrite_ExtendNote_Triggered", gWrite);

    registerActionHandler(ui->actionWrite_Extend1, "actionWrite_Extend1_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend2, "actionWrite_Extend2_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend3, "actionWrite_Extend3_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend4, "actionWrite_Extend4_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend5, "actionWrite_Extend5_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend6, "actionWrite_Extend6_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend7, "actionWrite_Extend7_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend8, "actionWrite_Extend8_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend9, "actionWrite_Extend9_Triggered", gWrite);
    registerActionHandler(ui->actionWrite_Extend10, "actionWrite_Extend10_Triggered", gWrite);

    registerActionHandler(ui->actionWrite_TrackFlags_ToggleRecording, "actionWrite_TrackFlags_ToggleRecording_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionWrite_TrackFlags_ToggleRecordingExclusive, "actionWrite_TrackFlags_ToggleRecordingExclusive_Triggered", gDIC | gMHT);
}

bool CS_Write::keyPressEvent(QKeyEvent* event)
{
    // Modifiers
    bool modShift=(event->modifiers() & Qt::ShiftModifier  ) != 0;

    switch(event->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:

        controller->cancelInterruptibleStates();
        if(!docRoot->hasTracks())return false;

        if(modShift || controller->isKeyboardModifierCommaPressed())
            actionWrite_Extend1_Triggered();
        else
            actionWrite_WriteOrExtendNote_Triggered();
        return true;
    }

    return false;   // event not handled
}

void CS_Write::actionWrite_WriteOrExtendNote_Triggered()  // Space, Return, Enter
{
    writeCells(1,true);
}

void CS_Write::actionWrite_Write1_Triggered()
{
    actionWrite_WriteX_Triggered(1);
}

void CS_Write::actionWrite_Write2_Triggered()
{
    actionWrite_WriteX_Triggered(2);
}

void CS_Write::actionWrite_Write3_Triggered()
{
    actionWrite_WriteX_Triggered(3);
}

void CS_Write::actionWrite_Write4_Triggered()
{
    actionWrite_WriteX_Triggered(4);
}

void CS_Write::actionWrite_Write5_Triggered()
{
    actionWrite_WriteX_Triggered(5);
}

void CS_Write::actionWrite_Write6_Triggered()
{
    actionWrite_WriteX_Triggered(6);
}

void CS_Write::actionWrite_Write7_Triggered()
{
    actionWrite_WriteX_Triggered(7);
}

void CS_Write::actionWrite_Write8_Triggered()
{
    actionWrite_WriteX_Triggered(8);
}

void CS_Write::actionWrite_Write9_Triggered()
{
    actionWrite_WriteX_Triggered(9);
}

void CS_Write::actionWrite_Write10_Triggered()
{
    actionWrite_WriteX_Triggered(10);
}

void CS_Write::actionWrite_WriteX_Triggered(int numberOfCells)
{
    if(controller->isKeyboardModifierCommaPressed())
    {
        actionWrite_ExtendX_Triggered(numberOfCells);
        return;
    }

    writeCells(numberOfCells,false);
}

void CS_Write::actionWrite_ExtendNote_Triggered() // Shift+Space
{
    actionWrite_ExtendX_Triggered(1);
}

void CS_Write::actionWrite_Extend1_Triggered()
{
    actionWrite_ExtendX_Triggered(1);
}

void CS_Write::actionWrite_Extend2_Triggered()
{
    actionWrite_ExtendX_Triggered(2);
}

void CS_Write::actionWrite_Extend3_Triggered()
{
    actionWrite_ExtendX_Triggered(3);
}

void CS_Write::actionWrite_Extend4_Triggered()
{
    actionWrite_ExtendX_Triggered(4);
}

void CS_Write::actionWrite_Extend5_Triggered()
{
    actionWrite_ExtendX_Triggered(5);
}

void CS_Write::actionWrite_Extend6_Triggered()
{
    actionWrite_ExtendX_Triggered(6);
}

void CS_Write::actionWrite_Extend7_Triggered()
{
    actionWrite_ExtendX_Triggered(7);
}

void CS_Write::actionWrite_Extend8_Triggered()
{
    actionWrite_ExtendX_Triggered(8);
}

void CS_Write::actionWrite_Extend9_Triggered()
{
    actionWrite_ExtendX_Triggered(9);
}

void CS_Write::actionWrite_Extend10_Triggered()
{
    actionWrite_ExtendX_Triggered(10);
}

void CS_Write::actionWrite_ExtendX_Triggered(int numberOfCells)
{
    writeExtendPreviousCellNotes(numberOfCells);
}

void CS_Write::actionWrite_TrackFlags_ToggleRecording_Triggered()
{
    toggleTrackFlags(TF_Record,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     false);
}

void CS_Write::actionWrite_TrackFlags_ToggleRecordingExclusive_Triggered()
{
    toggleTrackFlags(TF_Record,
                     getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot),
                     true);
}

void CS_Write::writeCells(int numberOfCells, bool enableNoteExtension)
{
    // Determine list of tracks enabled for writing
    QList<int> writeEnabledTrackList=prepareTrackListForWriting();
    if(writeEnabledTrackList.isEmpty())return;

    int minTrackIndex=INT_MAX,maxTrackIndex=0;
    for(int i=0; i < writeEnabledTrackList.size(); ++i)
    {
        int trackIndex=writeEnabledTrackList[i];
        if(trackIndex < minTrackIndex)minTrackIndex=trackIndex;
        if(trackIndex > maxTrackIndex)maxTrackIndex=trackIndex;
    }

    // ------------------------------------------------------------------------------------------
    // Determine list of note keys pressed on MIDI keyboard or mouse piano
    QList<int> noteKeyPressedList;
    for(int noteNumber=0; noteNumber < MIDI_N_NOTE_NUMBERS; ++noteNumber)
    {
        if(app->getKeypressSerialNo(noteNumber) != 0)
            noteKeyPressedList.append(noteNumber);
    }

    // If both lists have more than one entry, their sizes must match
    if(writeEnabledTrackList.size() >= 2 && noteKeyPressedList.size() >= 2 &&
       writeEnabledTrackList.size() != noteKeyPressedList.size())
    {
        QMessageBox::warning(mainWindow,
                             tr("Recording"),
                             tr("Unable to distribute pressed keys to tracks.\n\n"
                                "Please press as many keys as tracks are enabled for writing\n"
                                "- OR -\n"
                                "Select only one track for chord recording into that track."),
                             QMessageBox::Ok);
        return;
    }

    // Ratio of tracks:notes is now out of 1:n, n:n, n:1, n:0

    // ------------------------------------------------------------------------------------------
    // Determine tick range to operate on
    int ticksLeft=getEditorState().selection.ticksLeft;
    int ticksRight=ticksLeft;
    for(int i=0; i < numberOfCells; ++i)
    {
        ticksRight=docRoot->roundUpTicksToCellBorder(ticksRight + 1, getEditorState().writeLength);
    }

    // ------------------------------------------------------------------------------------------
    // Begin operation.

    beginMacro(tr("Write to %n Cell(s)","",numberOfCells),
               EditorRange(ticksLeft,minTrackIndex,ticksRight,maxTrackIndex));

    // Clear the range in write-enabled tracks
    for(int i=0; i < writeEnabledTrackList.size(); ++i)
        clearTrackRange_(writeEnabledTrackList[i],ticksLeft,ticksRight,false);

    // Distribute pressed note keys to tracks
    if(!noteKeyPressedList.isEmpty())
    {
        for(int i=0; i < writeEnabledTrackList.size(); ++i)
        {
            int trackIndex=writeEnabledTrackList[i];

            if(writeEnabledTrackList.size() >= 2 && noteKeyPressedList.size() >= 2)
            {
                // Distribute one of the notes to this track.
                //  Take the inverse order, so give the highest note to the topmost track on screen.

                int noteNumber=noteKeyPressedList[noteKeyPressedList.size() - 1 - i];
                writeNote_(trackIndex,ticksLeft,ticksRight,noteNumber,enableNoteExtension);
            }
            else if(writeEnabledTrackList.size() >= 2)
            {
                // Distribute the one pressed note to all tracks, thus also to this one
                int noteNumber=noteKeyPressedList[0];
                writeNote_(trackIndex,ticksLeft,ticksRight,noteNumber,enableNoteExtension);
            }
            else
            {
                // Distribute all notes to this single track
                for(int j=0; j < noteKeyPressedList.size(); ++j)
                {
                    int noteNumber=noteKeyPressedList[j];
                    writeNote_(trackIndex,ticksLeft,ticksRight,noteNumber,enableNoteExtension);
                }
            }
        }
    }

    // ------------------------------------------------------------------------------------------
    // Advance to next cell, set local cell selection

    EditorState newState=getEditorState();

    newState.selection.ticksLeft=ticksRight;
    newState.selection.ticksRight=docRoot->roundUpTicksToCellBorder(ticksRight + 1, newState.writeLength);
    newState.selection.trackTop=minTrackIndex;
    newState.selection.trackBottom=maxTrackIndex;

    newState.selection.anchor.ticksLeft=newState.selection.ticksLeft;   // Anchor: Copy tick settings
    newState.selection.anchor.ticksRight=newState.selection.ticksRight;
    newState.selection.anchor.track=minTrackIndex;

    // When undone, scroll modified range and new cursor cells into view
    endMacro(newState, EditorRange(ticksLeft, minTrackIndex, newState.selection.ticksRight, maxTrackIndex));

    // After macro creation, scroll cursor into view in any case.
    //  This helps when a ticksLeft setting lets scrollRangeIntoView keep the previous measure on screen
    //  and hide the next measure with the cursor/selection. This behaviour is OK for real redo,
    //  but not when the user is actually issuing a sequence of write commands.
    newState=getEditorState();
    scrollRangeIntoView(newState,
            EditorRange(newState.selection.anchor.ticksLeft,  -1,
                        newState.selection.anchor.ticksRight, -1));
    applyStateAndUpdate(newState);
}

void CS_Write::writeNote_(int trackIndex, int ticksLeft, int ticksRight, int noteNumber, bool enableNoteExtension)
{
    // Precondition: in macro execution
    Q_ASSERT(isInMacro());

    DocTrack* track=docRoot->trackList[trackIndex];

    // Remember MIDI or mouse piano keypress that generated this note.
    //  Used for extending the note by pressing space bar multiple times without releasing
    //  the MIDI or mouse piano key.
    quint32 keypressSerialNumber=app->getKeypressSerialNo(noteNumber);

    // If note should be extended...
    if(enableNoteExtension)
    {
        // ... check if note in previous cell exists ending on ticksLeft with same note number
        //      and same keypress serial number
        DocEvent* event=track->firstEvent;
        while(event)
        {
            if(event->type == DocEvent::E_Note &&
               event->noteEventData.noteNumber == noteNumber &&
               event->tickPositionEnd() == ticksLeft &&
               event->noteEventData.midiKeypressSerialNo == keypressSerialNumber)
            {
                // extend this note up to ticksRight
                DocEvent changedEventProperties(*event);
                changedEventProperties.tickLength  = ticksRight - changedEventProperties.tickPosition;

                if(changedEventProperties != *event)
                    addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                return;
            }

            event=event->nextEvent;
        }
    }

    // create new note
    DocEvent* newEvent=new DocEvent;
    newEvent->type=DocEvent::E_Note;
    newEvent->tickPosition=ticksLeft;
    newEvent->tickLength=ticksRight - newEvent->tickPosition;
    newEvent->noteEventData.noteNumber=noteNumber;
    newEvent->noteEventData.velocity=settings->newNoteMidiVelocity;
    newEvent->noteEventData.midiKeypressSerialNo=keypressSerialNumber;
    addCommand(new Command_InsertEvent(trackIndex, newEvent));
}

void CS_Write::writeExtendPreviousCellNotes(int numberOfCells)
{
    // Determine list of tracks enabled for writing
    QList<int> writeEnabledTrackList=prepareTrackListForWriting();
    if(writeEnabledTrackList.isEmpty())return;

    int minTrackIndex=INT_MAX,maxTrackIndex=0;
    for(int i=0; i < writeEnabledTrackList.size(); ++i)
    {
        int trackIndex=writeEnabledTrackList[i];
        if(trackIndex < minTrackIndex)minTrackIndex=trackIndex;
        if(trackIndex > maxTrackIndex)maxTrackIndex=trackIndex;
    }

    // ------------------------------------------------------------------------------------------
    // Determine tick range to operate on
    int ticksLeft=getEditorState().selection.ticksLeft;
    int ticksRight=ticksLeft;
    for(int i=0; i < numberOfCells; ++i)
    {
        ticksRight=docRoot->roundUpTicksToCellBorder(ticksRight + 1, getEditorState().writeLength);
    }

    // ------------------------------------------------------------------------------------------
    // Begin operation by clearing the range in write-enabled tracks,
    //  however in contrast to writeCells, let all notes untouched starting before ticksLeft

    beginMacro(tr("Extend Notes %n Cell(s)","",numberOfCells),
               EditorRange(ticksLeft,minTrackIndex,ticksRight,maxTrackIndex));

    for(int i=0; i < writeEnabledTrackList.size(); ++i)
        clearTrackRange_(writeEnabledTrackList[i],ticksLeft,ticksRight,true);

    // ------------------------------------------------------------------------------------------
    // Extend any notes existing in previous cell to ticksRight.
    //  This also includes notes not reaching to the cell border at ticksLeft.

    if(ticksLeft > 0)   // Is there a previous cell?
    {
        // Determine extent of previous cell
        int prevCellTicksLeft=docRoot->roundDownTicksToCellBorder(ticksLeft - 1, getEditorState().writeLength);

        for(int i=0; i < writeEnabledTrackList.size(); ++i)
        {
            int trackIndex=writeEnabledTrackList[i];

            DocEvent* event=docRoot->trackList[trackIndex]->firstEvent;
            while(event)
            {
                if(event->type != DocEvent::E_Note)
                {
                    event=event->nextEvent;
                    continue;
                }

                switch(intervalRelation(prevCellTicksLeft, ticksLeft, event->tickPosition, event->tickPositionEnd()))
                {
                case IR_before:
                case IR_after:
                    // leave event untouched
                    break;
                case IR_during:
                case IR_contains:
                case IR_overlappedBy:
                case IR_overlaps:
                    // if note does not reach up to ticksRight, extend it to ticksRight
                    if(event->tickPositionEnd() < ticksRight)
                    {
                        DocEvent changedEventProperties(*event);
                        changedEventProperties.tickLength  = ticksRight - changedEventProperties.tickPosition;

                        if(changedEventProperties != *event)
                            addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                    }
                    break;
                }

                event=event->nextEvent;
            }
        }
    }

    // ------------------------------------------------------------------------------------------
    // Advance to next cell
    EditorState newState=getEditorState();

    newState.selection.ticksLeft=ticksRight;
    newState.selection.ticksRight=docRoot->roundUpTicksToCellBorder(ticksRight + 1, newState.writeLength);
    newState.selection.trackTop=minTrackIndex;
    newState.selection.trackBottom=maxTrackIndex;

    newState.selection.anchor.ticksLeft=newState.selection.ticksLeft;   // Anchor: Copy tick settings
    newState.selection.anchor.ticksRight=newState.selection.ticksRight;
    newState.selection.anchor.track=minTrackIndex;

    // When undone, scroll modified range and new cursor cells into view
    endMacro(newState, EditorRange(ticksLeft, minTrackIndex, newState.selection.ticksRight, maxTrackIndex));

    // After macro creation, scroll cursor into view in any case.
    //  This helps when a ticksLeft setting lets scrollRangeIntoView keep the previous measure on screen
    //  and hide the next measure with the cursor/selection. This behaviour is OK for real redo,
    //  but not when the user is actually issuing a sequence of write commands.
    newState=getEditorState();
    scrollRangeIntoView(newState,
            EditorRange(newState.selection.anchor.ticksLeft,  -1,
                        newState.selection.anchor.ticksRight, -1));
    applyStateAndUpdate(newState);
}

QList<int> CS_Write::prepareTrackListForWriting()
{
    // Determine list of tracks enabled for writing
    QList<int> writeEnabledTrackList;
    for(int i=getEditorState().firstSelectedTrack(); i <= getEditorState().lastSelectedTrack(docRoot); ++i)
    {
        if(getEditorState().trackStateList[i].recordingEnabled)
            writeEnabledTrackList.append(i);
    }

    // No tracks? Show a message.
    if(writeEnabledTrackList.isEmpty())
    {
        if(QMessageBox::warning(mainWindow,
                                tr("Recording"),
                                tr("None of the selected tracks is enabled for recording.\n\n"
                                   "Enable recording for selected tracks?"),
                                QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes) == QMessageBox::Yes)
        {
            // Enable recording flag for selected tracks
            toggleTrackFlags(TF_Record,
                             getEditorState().firstSelectedTrack(),
                             getEditorState().lastSelectedTrack(docRoot),
                             false);
        }
    }

    return writeEnabledTrackList;
}
