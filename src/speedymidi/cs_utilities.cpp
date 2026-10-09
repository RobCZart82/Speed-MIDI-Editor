/***************************************************************************
 *  cs_utilities.cpp - Controller Subsystem: Utilities
 *                     (Split, Connect, Quantize, ScaleLength, Swing, Transpose)
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

#include "cs_utilities.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "commands.h"
#include "cs_navigation.h"
#include "swingifydialog.h"

#include <QInputDialog>
#include <QMessageBox>
#include <cmath>

#define CS_UTILITIES_CONNECT_NOTES_TICK_TOLERANCE_FACTOR  0.1

CS_Utilities::CS_Utilities(Controller* controller)
        : CS_Common(controller)
{
    transposeMode=TM_None;

    // guard type used for all handlers
    Controller::ActionGuards g = Controller::DisallowWhenInputCaptured |
                                 Controller::CancelInterruptibleStates |
                                 Controller::MustHaveTracks;

    registerActionHandler(ui->actionUtilities_SplitNotes, "actionUtilities_SplitNotes_Triggered", g);
    registerActionHandler(ui->actionUtilities_ConnectNotes, "actionUtilities_ConnectNotes_Triggered", g);
    registerActionHandler(ui->actionUtilities_QuantizeToCellRaster, "actionUtilities_QuantizeToCellRaster_Triggered", g);
    registerActionHandler(ui->actionUtilities_ScaleNoteLength, "actionUtilities_ScaleNoteLength_Triggered", g);
    registerActionHandler(ui->actionUtilities_AddSwing, "actionUtilities_AddSwing_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeOctaveDrag, "actionUtilities_TransposeOctaveDrag_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeOctaveMulti, "actionUtilities_TransposeOctaveMulti_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeDiatonicDrag, "actionUtilities_TransposeDiatonicDrag_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeDiatonicMulti, "actionUtilities_TransposeDiatonicMulti_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeChromaticDrag, "actionUtilities_TransposeChromaticDrag_Triggered", g);
    registerActionHandler(ui->actionUtilities_TransposeChromaticMulti, "actionUtilities_TransposeChromaticMulti_Triggered", g);
    registerActionHandler(ui->actionUtilities_RemoveTopVoice, "actionUtilities_RemoveTopVoice_Triggered", g);
    registerActionHandler(ui->actionUtilities_RemoveBottomVoice, "actionUtilities_RemoveBottomVoice_Triggered", g);
}

bool CS_Utilities::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Escape:    // cancel keyboard drag operation
        {
            if(transposeMode == TM_None)
                break;      // no drag mode active

            cancelInputCapture();
            return true;
        }
    }

    if(transposeMode != TM_None)
    {
        Q_ASSERT(capturingInput());

        switch(event->key())
        {
        case Qt::Key_Up:
            transposeSelection(transposeMode, 1);
            return true;
        case Qt::Key_Down:
            transposeSelection(transposeMode, -1);
            return true;
        }

        // During transpose mode, input is captured. Reach event also to navigation subsystem.
        CS_Navigation* csNavigation=
                qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
        if(csNavigation != NULL)return csNavigation->keyPressEvent(event);
    }

    return false;   // event not handled
}

void CS_Utilities::keyReleaseEvent(QKeyEvent* event)
{
    if(!event->isAutoRepeat())
    {
        // disabling of keyboard modal states
        if((event->key() == Qt::Key_F2 && transposeMode == TM_InOctaves) ||
           (event->key() == Qt::Key_F3 && transposeMode == TM_Diatonically) ||
           (event->key() == Qt::Key_F4 && transposeMode == TM_Chromatically))
        {
        	cancelInputCapture();
        }
    }
}

bool CS_Utilities::mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone)
{
    // During transpose mode, input is captured. Reach event also to navigation subsystem.
    if(transposeMode != TM_None)
    {
        Q_ASSERT(capturingInput());

        CS_Navigation* csNavigation=
                qobject_cast<CS_Navigation*>(controller->getSubsystemByClassName("CS_Navigation"));
        if(csNavigation != NULL)return csNavigation->mousePressEvent(event, mouseZone);
    }

    return false;   // event not handled
}

bool CS_Utilities::wheelEvent(QWheelEvent* event)
{
    mouseWheelAccu_8thsOfDegrees+=event->angleDelta().y();
    const int MOUSE_WHEEL_DELTA=120;    // 15 degrees * 8, see documentation for QWheelEvent::delta()

    // Wheel during active transpose mode transposes the selection up or down 1 step per delta
    if(transposeMode != TM_None)
    {
        while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
        {
            mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;
            transposeSelection(transposeMode, 1);
        }
        while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
        {
            mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;
            transposeSelection(transposeMode, -1);
        }
        return true;
    }

    return false;   // event not handled
}

void CS_Utilities::cancelInputCapture()
{
    Q_ASSERT(transposeMode != TM_None);

    // remove special selection color (transpose mode)
    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.keyboardDragMode=VolatileEditorState::KDM_None;
    applyVolatileStateAndUpdate(newVolatileState);

    transposeMode=TM_None;
    releaseInputCapture();
    
    controller->restoreMouseCursor();  // mouse cursor can differ during keyboard drag mode != KDM_None
}

void CS_Utilities::actionUtilities_SplitNotes_Triggered()
{
    beginMacro(tr("Split Notes"), getEditorState().selection);

    int cutAtTicks=getEditorState().selection.ticksLeft;

    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            if(event->type == DocEvent::E_Note &&
               intervalRelation(cutAtTicks,cutAtTicks,event->tickPosition,event->tickPositionEnd()) ==
               IR_contains)
            {
                // split this note

                // right event
                DocEvent* newEvent=new DocEvent(*event);
                newEvent->tickPosition=cutAtTicks;
                newEvent->tickLength=event->tickPositionEnd() - newEvent->tickPosition;
                addCommand(new Command_InsertEvent(trackIndex, newEvent));

                // left event (re-use old event)
                DocEvent changedEventProperties(*event);
                changedEventProperties.tickLength=cutAtTicks - event->tickPosition;
                if(changedEventProperties != *event)
                    addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
            }

            event=event->nextEvent;
        }
    }

    // reduce selection to a local cell selection with 1 cell selected in x-direction
    EditorState newState=getEditorState();

    newState.selection.ticksRight=
            docRoot->roundUpTicksToCellBorder(newState.selection.ticksLeft + 1, newState.writeLength);
    newState.selection.trackTop    = getEditorState().firstSelectedTrack();
    newState.selection.trackBottom = getEditorState().lastSelectedTrack(docRoot);

    newState.selection.anchor.ticksLeft=newState.selection.ticksLeft;   // Anchor: Copy tick settings
    newState.selection.anchor.ticksRight=newState.selection.ticksRight;
    newState.selection.anchor.track=newState.selection.trackTop;

    // When undone, scroll modified range and new cursor cells into view
    endMacro(newState, EditorRange(newState.selection.ticksLeft,  getEditorState().firstSelectedTrack(),
                                   newState.selection.ticksRight, getEditorState().lastSelectedTrack(docRoot)));
}

void CS_Utilities::actionUtilities_ConnectNotes_Triggered()
{
    beginMacro(tr("Connect Notes"), getEditorState().selection);

    int connectAtTicks=getEditorState().selection.ticksLeft;
    int regularCellLength=docRoot->getNextCellMeasureInternalTickPosition(0,getEditorState().writeLength);
    int tickTolerance=(int)(regularCellLength * CS_UTILITIES_CONNECT_NOTES_TICK_TOLERANCE_FACTOR);

    // track loop
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];

        // Prepare two event lists:
        //  1. events ending at connectAtTicks
        //  2. events starting at connectAtTicks
        EventList endingEventList;
        EventList startingEventList;

        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            if(event->type == DocEvent::E_Note)
            {
                Q_ASSERT(event->tickLength > 0);

                if(event->tickPositionEnd() >= connectAtTicks - tickTolerance &&
                   event->tickPositionEnd() <= connectAtTicks + tickTolerance)
                {
                    endingEventList.append(event);
                }
                else if(event->tickPosition >= connectAtTicks - tickTolerance &&
                        event->tickPosition <= connectAtTicks + tickTolerance)
                {
                    startingEventList.append(event);
                }
            }

            event=event->nextEvent;
        }

        // Traverse ending-list and find a corresponding note number in starting list
        for(int i=0; i < endingEventList.size(); ++i)
        {
            DocEvent* noteLeft=endingEventList[i];

            for(int j=0; j < startingEventList.size(); ++j)
            {
                DocEvent* noteRight=startingEventList[j];
                if(noteLeft->noteEventData.noteNumber == noteRight->noteEventData.noteNumber)
                {
                    // connect these two notes

                    // extend the first note to the end of the second note
                    DocEvent changedEventProperties(*noteLeft);
                    changedEventProperties.tickLength=noteRight->tickPositionEnd() - noteLeft->tickPosition;

                    if(changedEventProperties != *noteLeft)
                        addCommand(new Command_EventProperties(trackIndex, noteLeft,
                                                               changedEventProperties));

                    // delete right note
                    // Each right-hand note can be connected only once, even
                    // when multiple overlapping notes end with the same pitch.
                    startingEventList.removeAt(j);
                    addCommand(new Command_DeleteEvent(trackIndex, noteRight));

                    // Do not connect any other notes to noteLeft, so break
                    break;
                }
            }
        }
    }

    // reduce selection to a local cell selection with 1 cell selected in x-direction
    EditorState newState=getEditorState();

    newState.selection.ticksRight=
            docRoot->roundUpTicksToCellBorder(newState.selection.ticksLeft + 1, newState.writeLength);
    newState.selection.trackTop    = getEditorState().firstSelectedTrack();
    newState.selection.trackBottom = getEditorState().lastSelectedTrack(docRoot);

    newState.selection.anchor.ticksLeft=newState.selection.ticksLeft;   // Anchor: Copy tick settings
    newState.selection.anchor.ticksRight=newState.selection.ticksRight;
    newState.selection.anchor.track=newState.selection.trackTop;

    // When undone, scroll modified range and new cursor cells into view
    endMacro(newState, EditorRange(newState.selection.ticksLeft,  getEditorState().firstSelectedTrack(),
                                   newState.selection.ticksRight, getEditorState().lastSelectedTrack(docRoot)));
}

void CS_Utilities::actionUtilities_QuantizeToCellRaster_Triggered()
{
    beginMacro(tr("Quantize to Cell Raster"), getEditorState().selection);

    // track loop
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            if(event->type == DocEvent::E_Note)
            {
                switch(intervalRelation(getEditorState().selection, event))
                {
                case IR_before:
                case IR_after:
                case IR_contains:
                    // leave event untouched
                    break;
                case IR_overlaps:
                    {
                        // Quantize event end
                        int quantizedEndTicks=quantizeTicksToCellRaster(event->tickPositionEnd());

                        DocEvent changedEventProperties(*event);
                        changedEventProperties.tickLength=quantizedEndTicks - event->tickPosition;
                        if(changedEventProperties != *event)
                            addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                        break;
                    }
                case IR_during:
                    {
                        // Quantize event start and end
                        int quantizedStartTicks=quantizeTicksToCellRaster(event->tickPosition);
                        int quantizedEndTicks=quantizeTicksToCellRaster(event->tickPositionEnd());

                        // If result is zero length, quantize event to fill out exactly one cell.
                        //  Choose the cell where the center of the event currently resides.
                        if(quantizedStartTicks == quantizedEndTicks)
                        {
                            int centerTicks=event->tickPosition + event->tickLength / 2;
                            quantizedStartTicks=docRoot->roundDownTicksToCellBorder(
                                    centerTicks,getEditorState().writeLength);
                            quantizedEndTicks=docRoot->roundUpTicksToCellBorder(
                                    quantizedStartTicks + 1, getEditorState().writeLength);
                        }

                        DocEvent changedEventProperties(*event);
                        changedEventProperties.tickPosition=quantizedStartTicks;
                        changedEventProperties.tickLength=quantizedEndTicks - quantizedStartTicks;
                        if(changedEventProperties != *event)
                            addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                        break;
                    }
                case IR_overlappedBy:
                    {
                        // Quantize event start
                        int quantizedStartTicks=quantizeTicksToCellRaster(event->tickPosition);

                        // If result is zero length, then quantize start ticks to left cell border
                        if(event->tickPositionEnd() - quantizedStartTicks == 0)
                        {
                            quantizedStartTicks=docRoot->roundDownTicksToCellBorder(
                                    quantizedStartTicks - 1, getEditorState().writeLength);
                        }

                        DocEvent changedEventProperties(*event);
                        changedEventProperties.tickPosition=quantizedStartTicks;
                        changedEventProperties.tickLength=event->tickPositionEnd() - quantizedStartTicks;
                        if(changedEventProperties != *event)
                            addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                        break;
                    }
                }
            }

            event=event->nextEvent;
        }
    }

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Utilities::actionUtilities_ScaleNoteLength_Triggered()
{
    // get percent value for scaling, use LRU value for initialization
    bool ok;
    double scalePercent=
            QInputDialog::getDouble(mainWindow,
                                    tr("Scale Note Length"),
                                    tr("Multiply length of every note starting in selection by &percent value"),
                                    settings->LRU.scaleNoteLengthPercent,1.,1600.,5,&ok);
    if(!ok)return;

    // save LRU value
    settings->LRU.scaleNoteLengthPercent=scalePercent;

    // Validate the complete operation before creating any undo command.
    const qint64 maxTick=INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                   EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
    for(int i=getEditorState().firstSelectedTrack(); i<=getEditorState().lastSelectedTrack(docRoot); ++i)
        for(const DocEvent* event=docRoot->trackList[i]->firstEvent; event; event=event->nextEvent)
            if(event->type == DocEvent::E_Note &&
               event->tickPosition >= getEditorState().selection.ticksLeft &&
               event->tickPosition < getEditorState().selection.ticksRight)
            {
                const double length=event->tickLength*scalePercent/100.;
                if(!std::isfinite(length) || event->tickPosition + qMax(1.,length) > maxTick)
                {
                    QMessageBox::warning(mainWindow,tr("Cannot Scale Note Length"),
                        tr("The scaled notes would exceed the supported MIDI tick range. The document has not been changed."));
                    return;
                }
            }

    // apply scale factor to each note event STARTING in selection
    beginMacro(tr("Scale Note Length"), getEditorState().selection);

    // track loop
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            if(event->type == DocEvent::E_Note)
            {
                switch(intervalRelation(getEditorState().selection, event))
                {
                case IR_before:
                case IR_after:
                case IR_contains:
                case IR_overlaps:
                    // leave event untouched
                    break;
                case IR_during:
                case IR_overlappedBy:
                    // event starts within selected range: scale note length
                    DocEvent changedEventProperties(*event);
                    changedEventProperties.tickLength=(int)(event->tickLength * scalePercent / 100.);

                    // check for minimum event length
                    if(changedEventProperties.tickLength < DOCUMENT_MIN_EVENT_LENGTH_TICKS)
                        changedEventProperties.tickLength = DOCUMENT_MIN_EVENT_LENGTH_TICKS;

                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                    break;
                }
            }

            event=event->nextEvent;
        }
    }

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Utilities::actionUtilities_AddSwing_Triggered()
{
    EditorState backupState=getEditorState();

    // set write length to swing base length
    EditorState newState=getEditorState();
    newState.writeLength.denominator=DOCUMENT_SWING_PLAYBACK_BASE_NOTE_DENOMINATOR;
    newState.writeLength.tupletNominator=1;
    newState.writeLength.tupletDenominator=1;
    newState.adjustForChangedWriteLength(docRoot);

    // Determine tick length of a note subject to swing adjustments
    int swingNoteBaseTickLength=
            docRoot->midiTicksPerWholeNote / DOCUMENT_SWING_PLAYBACK_BASE_NOTE_DENOMINATOR;

    // extend selection to next even multiple of swing base length
    if((docRoot->ticksToMeasure(newState.selection.ticksLeft).measureInternalTicks
       / swingNoteBaseTickLength) & 1)
    {
        newState.selection.ticksLeft=docRoot->roundDownTicksToCellBorder(newState.selection.ticksLeft - 1,
                                                                         newState.writeLength);
    }
    if((docRoot->ticksToMeasure(newState.selection.ticksRight).measureInternalTicks
       / swingNoteBaseTickLength) & 1)
    {
        newState.selection.ticksRight=docRoot->roundUpTicksToCellBorder(newState.selection.ticksRight + 1,
                                                                        newState.writeLength);
    }

    applyStateAndUpdate(newState);

    SwingifyDialog dlg;
    dlg.swingHardness  = settings->LRU.addSwingDialogSwingHardness;
    if(dlg.exec() != QDialog::Accepted)
    {
        // Restore backup editor state on cancel
        applyStateAndUpdate(backupState);
        return;
    }

    settings->LRU.addSwingDialogSwingHardness = dlg.swingHardness;

    addSwingToSelection(dlg.swingHardness);
}

void CS_Utilities::actionUtilities_TransposeOctaveDrag_Triggered()
{
    transposeMode=TM_InOctaves;
    setInputCapture();

    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.keyboardDragMode=VolatileEditorState::KDM_Transpose;  // show special selection color
    applyVolatileStateAndUpdate(newVolatileState);

    controller->restoreMouseCursor();     // mouse cursor can differ during keyboard drag mode != KDM_None
}

void CS_Utilities::actionUtilities_TransposeOctaveMulti_Triggered()
{
    bool ok;
    int numberOfSteps=
            QInputDialog::getInt(mainWindow,tr("Transpose in Octaves"),tr("&How many octaves"),
                                 settings->LRU.transposeOctaveSteps,
                                 -MIDI_MAX_OCTAVE, MIDI_MAX_OCTAVE, 1, &ok);
    if(!ok)return;

    settings->LRU.transposeOctaveSteps=numberOfSteps;
    transposeSelection(TM_InOctaves, numberOfSteps);
}

void CS_Utilities::actionUtilities_TransposeDiatonicDrag_Triggered()
{
    transposeMode=TM_Diatonically;
    setInputCapture();

    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.keyboardDragMode=VolatileEditorState::KDM_Transpose;  // show special selection color
    applyVolatileStateAndUpdate(newVolatileState);

    controller->restoreMouseCursor();     // mouse cursor can differ during keyboard drag mode != KDM_None
}

void CS_Utilities::actionUtilities_TransposeDiatonicMulti_Triggered()
{
    bool ok;
    int numberOfSteps=
            QInputDialog::getInt(mainWindow,
                                 tr("Transpose Diatonically"),tr("&How many steps"),
                                 settings->LRU.transposeDiatonicSteps,
                                 -(MIDI_MAX_OCTAVE+1)*12, (MIDI_MAX_OCTAVE+1)*12, 1, &ok);
    if(!ok)return;

    settings->LRU.transposeDiatonicSteps=numberOfSteps;
    transposeSelection(TM_Diatonically, numberOfSteps);
}

void CS_Utilities::actionUtilities_TransposeChromaticDrag_Triggered()
{
    transposeMode=TM_Chromatically;
    setInputCapture();

    VolatileEditorState newVolatileState=getVolatileEditorState();
    newVolatileState.keyboardDragMode=VolatileEditorState::KDM_Transpose;  // show special selection color
    applyVolatileStateAndUpdate(newVolatileState);

    controller->restoreMouseCursor();     // mouse cursor can differ during keyboard drag mode != KDM_None
}

void CS_Utilities::actionUtilities_TransposeChromaticMulti_Triggered()
{
    bool ok;
    int numberOfSteps=
            QInputDialog::getInt(mainWindow,
                                 tr("Transpose Chromatically"),tr("&How many steps"),
                                 settings->LRU.transposeChromaticSteps,
                                 -MIDI_MAX_DATA_VALUE, MIDI_MAX_DATA_VALUE, 1, &ok);
    if(!ok)return;

    settings->LRU.transposeChromaticSteps=numberOfSteps;
    transposeSelection(TM_Chromatically, numberOfSteps);
}

void CS_Utilities::actionUtilities_RemoveTopVoice_Triggered()
{
    beginMacro(tr("Remove top voice"), getEditorState().selection);

    removeVoice(V_Top);

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Utilities::actionUtilities_RemoveBottomVoice_Triggered()
{
    beginMacro(tr("Remove bottom voice"), getEditorState().selection);

    removeVoice(V_Bottom);

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Utilities::removeVoice(VoiceType voice)
{
    // Create an index structure as a partitioning of the tick space of the piece with
    // measure granularity. This replaces the O(n^2) complexity of the algorithm with an O(1) complexity
    // because the number of notes in a measure is expected to be bound by a reasonable constant (~32)

    QList<EventList> overlapIndex;

    // generate an event list for each measure
    int maxMeasure=docRoot->getMaxMeasure() + 1;
    for(int i=0; i < maxMeasure; ++i)overlapIndex.append(EventList());

    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        // Voice number detection always operates on the whole track.
        // Modifications, i.e. the clearing some notes, will be done only within the current selection.

        // distribute note events to index structure
        createIndexOnNotesForTrack(trackIndex, overlapIndex);

        // detect selected voice
        detectVoice(voice, trackIndex, overlapIndex);

        // remove the voice's notes
        removeDetectedVoice(trackIndex);

        // reset index structure
        for(int i=0; i < overlapIndex.size(); ++i)
            overlapIndex[i].clear();
    }
}

void CS_Utilities::createIndexOnNotesForTrack(int trackIndex, QList<EventList>& overlapIndex)
{
    // distribute note events to index structure

    DocTrack* track=docRoot->trackList[trackIndex];
    for(DocEvent* event = track->firstEvent; event != NULL; event = event->nextEvent)   // event loop
    {
        if(event->type != DocEvent::E_Note)continue;

        // add an entry for all touched measures
        int firstMeasure=docRoot->ticksToMeasure(event->tickPosition).measureIndex;

        TicksToMeasureResult r=docRoot->ticksToMeasure(event->tickPositionEnd());
        int lastMeasure=r.measureIndex;
        if(r.measureInternalTicks == 0)--lastMeasure;

        for(int i=firstMeasure; i <= lastMeasure; ++i)
            overlapIndex[i].append(event);
    }
}

CS_Utilities::OverlapFinder::OverlapFinder(const DocRoot* docRoot, const DocEvent* event, const QList<EventList>& overlapIndex)
    : _overlapIndex(overlapIndex)
{
    this->docRoot=docRoot;
    this->event=event;
    this->ticksLeft=event->tickPosition;
    this->ticksRight=event->tickPositionEnd();

    init();
}

CS_Utilities::OverlapFinder::OverlapFinder(const DocRoot* docRoot, int ticksLeft, int ticksRight, const QList<EventList>& overlapIndex)
    : _overlapIndex(overlapIndex)
{
    this->docRoot=docRoot;
    this->event=NULL;
    this->ticksLeft=ticksLeft;
    this->ticksRight=ticksRight;

    init();
}

void CS_Utilities::OverlapFinder::init()
{
    // find overlapping notes, filter step: find all events in measures touched by current event
    firstMeasure=docRoot->ticksToMeasure(ticksLeft).measureIndex;

    TicksToMeasureResult r=docRoot->ticksToMeasure(ticksRight);
    lastMeasure=r.measureIndex;
    if(r.measureInternalTicks == 0)--lastMeasure;

    reset();
}

void CS_Utilities::OverlapFinder::reset()
{
    // initialize next() loop
    currentMeasure=firstMeasure;
    eventIndex=0;
}

DocEvent* CS_Utilities::OverlapFinder::next()
{
    while(true)
    {
        if(currentMeasure > lastMeasure)return NULL;

        if(eventIndex == _overlapIndex[currentMeasure].size())
        {
            // to next measure
            ++currentMeasure;
            eventIndex=0;
            continue;
        }

        // filter step: find all events in measures touched by current event
        DocEvent* otherEvent=_overlapIndex[currentMeasure][eventIndex++];
        if(otherEvent == event)continue;    // skip self (in case event is NULL there is no "self")

        // refinement step: check precisely for overlap
        switch(CS_Common::intervalRelation(ticksLeft, ticksRight,
                                           otherEvent->tickPosition, otherEvent->tickPositionEnd()))
        {
        case IR_contains:
        case IR_overlaps:
        case IR_during:
        case IR_overlappedBy:
            break;
        case IR_before:
        case IR_after:
            // does not overlap, skip filter candidate
            continue;
        }

        // found overlapping event
        return otherEvent;
    }
}

void CS_Utilities::detectVoice(VoiceType voice, int trackIndex, QList<EventList>& overlapIndex)
{
    EventList mayBeList;    // Remember the VD_MayBe notes for later processing

    // Classify notes as VD_Yes, VD_No, or a preliminary VD_MayBe
    detectVoice_initialClassification(voice, trackIndex, mayBeList, overlapIndex);

    // Process mayBe notes
    while(!mayBeList.isEmpty())
    {
        // Repeated sequential passes over mayBeList
        while( detectVoice_processUnambiguousMayBeNotes(mayBeList, overlapIndex) );

        detectVoice_cleanUpMayBeList(mayBeList);
        if(mayBeList.isEmpty())break;

        // For remaining mayBe notes where classification is ambiguous, apply heuristics:
        // Try to classify mayBe note with lowest tickPosition.
        DocEvent* minTickEvent=detectVoice_getMinTickEvent(mayBeList);

        if(detectVoice_classifyAmbiguousMayBeNoteByIntervals(minTickEvent, overlapIndex))
        {
            // success
            detectVoice_cleanUpMayBeList(mayBeList);
            continue;
        }

        if(detectVoice_classifyAmbiguousMayBeNoteByPrecedingNotes(trackIndex, minTickEvent, overlapIndex))
        {
            // success
            detectVoice_cleanUpMayBeList(mayBeList);
            continue;
        }

        // Ambiguous case: make a guess and classify as VD_Yes
        detectVoice_classifyAsYes(minTickEvent, overlapIndex);
        detectVoice_cleanUpMayBeList(mayBeList);
    }
}

void CS_Utilities::detectVoice_initialClassification(VoiceType voice, int trackIndex, EventList& mayBeList, QList<EventList>& overlapIndex)
{
    // Classify notes as VD_Yes, VD_No, or (preliminarily) VD_MayBe

    // All rules and comments here are instanciated for voice == V_Top,
    // for V_Bottom all note height comparisons must be inversed by multiplying with csgn.

    // VD_MayBe notes can never overlap with VD_Yes notes, only with VD_No or other VD_MayBe notes

    int csgn = voice == V_Top ? 1 : -1; // voice note height comparison sign

    DocTrack* track=docRoot->trackList[trackIndex];
    for(DocEvent* event = track->firstEvent; event != NULL; event = event->nextEvent)   // event loop
    {
        if(event->type != DocEvent::E_Note)continue;

        DocEvent::NoteEvent& ned=event->noteEventData;

        int noteNumber = ned.noteNumber;
        ned.voiceDetectionResult = DocEvent::NoteEvent::VD_Yes;  // Initialize with "yes"

        // find overlapping notes
        bool foundOverlappingNote=false;

        OverlapFinder f(docRoot, event, overlapIndex);
        DocEvent* ovlEvent;
        while((ovlEvent = f.next()) != NULL)
        {
            foundOverlappingNote=true;

            // check note height
            int ovlNoteNumber = ovlEvent->noteEventData.noteNumber;
            if(csgn * ovlNoteNumber > csgn * noteNumber)    // higher overlapping note?
            {
                ned.voiceDetectionResult = DocEvent::NoteEvent::VD_No; // Cannot be V_Top
                break;
            }
            else if(ovlNoteNumber == noteNumber)
            {
                ned.voiceDetectionResult = DocEvent::NoteEvent::VD_MayBe;  // Not sure yet
            }
        }

        if(!foundOverlappingNote)
        {
            // do not remove a single voice
            ned.voiceDetectionResult = DocEvent::NoteEvent::VD_No;
        }

        if(ned.voiceDetectionResult == DocEvent::NoteEvent::VD_MayBe)
            mayBeList.append(event);
    }
}

bool CS_Utilities::detectVoice_processUnambiguousMayBeNotes(EventList& mayBeList, QList<EventList>& overlapIndex)
{
    bool anyNoteClassified=false;

    for(int i=0; i < mayBeList.size(); ++i)
    {
        DocEvent* event=mayBeList[i];

        if(event->noteEventData.voiceDetectionResult != DocEvent::NoteEvent::VD_MayBe)
            continue;   // Note already classified

        // Check whether event overlaps any other mayBe note
        bool foundOverlappingMayBeNote=false;
        bool foundUnequalOverlappingMayBeNote=false;

        OverlapFinder f(docRoot, event, overlapIndex);
        DocEvent* ovlEvent;
        while((ovlEvent = f.next()) != NULL)
        {
            if(ovlEvent->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_MayBe)
            {
                foundOverlappingMayBeNote=true;

                // check whether this maybe note exactly overlaps
                if(event->tickPosition != ovlEvent->tickPosition ||
                   event->tickLength   != ovlEvent->tickLength)
                {
                    foundUnequalOverlappingMayBeNote=true;  // no exact overlap
                }
            }
        }

        if(foundOverlappingMayBeNote)
        {
            if(!foundUnequalOverlappingMayBeNote)
            {
                // all other mayBe notes exactly overlap (interval relation = "equals")

                // classify event as VD_Yes
                detectVoice_classifyAsYes(event, overlapIndex);
                anyNoteClassified=true;
            }
            else
            {
                // Found overlapping mayBe note with interval relation != "equals".
                // Cannot decide here what voice this note belongs to.
            }
        }
        else    // found no overlapping mayBe note
        {
            // classify event as VD_Yes
            event->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_Yes;
            anyNoteClassified=true;
        }
    }

    return anyNoteClassified;
}

DocEvent* CS_Utilities::detectVoice_getMinTickEvent(EventList& mayBeList)
{
    // get and remove note with lowest tickPosition
    int minTickPosition=INT_MAX;
    DocEvent* minTickEvent=NULL;

    for(int i=0; i < mayBeList.size(); ++i)
    {
        DocEvent* event=mayBeList[i];
        if(event->tickPosition < minTickPosition)
        {
            minTickPosition = event->tickPosition;
            minTickEvent    = event;
        }
    }

    Q_ASSERT(minTickEvent != NULL);
    return minTickEvent;
}

bool CS_Utilities::detectVoice_classifyAmbiguousMayBeNoteByIntervals(DocEvent* event, QList<EventList>& overlapIndex)
{
    // create a list of tick intervals not overlapped by other mayBe notes
    QList<IntervalType> intervalList;
    intervalList.append(IntervalType(event->tickPosition, event->tickPositionEnd()));

    OverlapFinder f_iv(docRoot, event, overlapIndex);
    DocEvent* ovlEvent;
    while((ovlEvent = f_iv.next()) != NULL)
    {
        if(ovlEvent->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_MayBe)
        {
            // remove all intervals in list overlapping with this ovlEvent
            int i=0;
            while(i < intervalList.size())
            {
                switch(intervalRelation(ovlEvent->tickPosition, ovlEvent->tickPositionEnd(),
                                        intervalList[i].start, intervalList[i].end))
                {
                case IR_before:
                case IR_after:
                    ++i;                        // leave interval untouched
                    break;
                case IR_during:
                    intervalList.removeAt(i);   // delete interval
                    break;
                case IR_contains:
                    // split interval
                    intervalList.append(IntervalType(intervalList[i].start, ovlEvent->tickPosition));
                    intervalList.append(IntervalType(ovlEvent->tickPositionEnd(), intervalList[i].end));
                    intervalList.removeAt(i);
                    break;
                case IR_overlappedBy:
                    intervalList[i].start=ovlEvent->tickPositionEnd();  // shift event start
                    ++i;
                    break;
                case IR_overlaps:
                    intervalList[i].end=ovlEvent->tickPosition;         // shorten interval
                    ++i;
                    break;
                }
            }
        }
    }

    // if there is a VD_No note in any of these intervals, classify event as VD_Yes
    //  and all overlapping notes as VD_No

    for(int i=0; i < intervalList.size(); ++i)
    {
        OverlapFinder f_no(docRoot, intervalList[i].start, intervalList[i].end, overlapIndex);
        while((ovlEvent = f_no.next()) != NULL)
        {
            if(ovlEvent->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_No)
            {
                detectVoice_classifyAsYes(event, overlapIndex);
                return true;
            }
        }
    }

    return false;
}

bool CS_Utilities::detectVoice_classifyAmbiguousMayBeNoteByPrecedingNotes(int trackIndex, DocEvent* event, QList<EventList>& overlapIndex)
{
    // Determine distance from event->tickPosition to last
    //  VD_Yes/VD_No notes ending before event->tickPosition

    int latestYesNoteEnd=0;
    int latestNoNoteEnd=0;

    DocEvent* latestYesNote=NULL;
    DocEvent* latestNoNote=NULL;

    // Make a sequential scan
    DocTrack* track=docRoot->trackList[trackIndex];
    DocEvent* precedingEvent=track->firstEvent;
    while(precedingEvent)
    {
        if(precedingEvent->type == DocEvent::E_Note &&
           precedingEvent->tickPositionEnd() <= event->tickPosition)
        {
            if(precedingEvent->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_Yes)
            {
                if(precedingEvent->tickPositionEnd() > latestYesNoteEnd)
                {
                    latestYesNoteEnd = precedingEvent->tickPositionEnd();
                    latestYesNote    = precedingEvent;
                }
            }
            else if(precedingEvent->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_No)
            {
                if(precedingEvent->tickPositionEnd() > latestNoNoteEnd)
                {
                    latestNoNoteEnd = precedingEvent->tickPositionEnd();
                    latestNoNote    = precedingEvent;
                }
            }
            else
            {
                // precedingEvent must have been classified before because
                //  event has the lowest tickPosition of all mayBe notes
                Q_ASSERT(false);
            }
        }
        precedingEvent=precedingEvent->nextEvent;
    }

    // if only found notes in one voice, use that voice
    if(latestYesNote == NULL)
    {
        if(latestNoNote == NULL)return false;   // no preceding note at all => ambigous

        // Classify as VD_No
        event->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_No;
        return true;
    }
    else
    {
        if(latestNoNote == NULL)
        {
            // Classify as VD_Yes
            detectVoice_classifyAsYes(event, overlapIndex);
            return true;
        }
    }

    // found notes in both voices:
    // use the voice of the note with the shortest distance to event as classification for event
    if(latestYesNoteEnd > latestNoNoteEnd)
    {
        // Classify as VD_Yes
        detectVoice_classifyAsYes(event, overlapIndex);
        return true;
    }
    else if(latestYesNoteEnd < latestNoNoteEnd)
    {
        // Classify as VD_No
        event->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_No;
        return true;
    }

    // equal distance: check length ratios

    float yesNoteLengthRatio=(float)latestYesNote->tickLength / event->tickLength;
    if(yesNoteLengthRatio < 1)yesNoteLengthRatio=1/yesNoteLengthRatio;

    float noNoteLengthRatio=(float)latestNoNote->tickLength / event->tickLength;
    if(noNoteLengthRatio < 1)noNoteLengthRatio=1/noNoteLengthRatio;

    if(yesNoteLengthRatio < noNoteLengthRatio)
    {
        // yesNote is more similar in length: Classify as VD_Yes
        detectVoice_classifyAsYes(event, overlapIndex);
        return true;
    }
    else if(yesNoteLengthRatio > noNoteLengthRatio)
    {
        // noNote is more similar in length: Classify as VD_No
        event->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_No;
        return true;
    }

    // same length ratios => ambigous
    return false;
}

void CS_Utilities::detectVoice_classifyAsYes(DocEvent* event, QList<EventList>& overlapIndex)
{
    // classify current event as VD_Yes
    event->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_Yes;

    // classify all overlapping notes as VD_No
    OverlapFinder f(docRoot, event, overlapIndex);
    DocEvent* ovlEvent;
    while((ovlEvent = f.next()) != NULL)
    {
        ovlEvent->noteEventData.voiceDetectionResult = DocEvent::NoteEvent::VD_No;
    }
}

void CS_Utilities::detectVoice_cleanUpMayBeList(EventList& mayBeList)
{
    // delete all notes from mayBeList that have been classified as VD_Yes or VD_No
    for(int i=mayBeList.size()-1; i >= 0; --i)
    {
        if(mayBeList[i]->noteEventData.voiceDetectionResult != DocEvent::NoteEvent::VD_MayBe)
            mayBeList.removeAt(i);
    }
}

void CS_Utilities::removeDetectedVoice(int trackIndex)
{
    // remove all events marked for removal

    DocTrack* track=docRoot->trackList[trackIndex];
    DocEvent* event=track->firstEvent;
    while(event)    // event loop
    {
        if(event->type != DocEvent::E_Note)
        {
            event=event->nextEvent;
            continue;
        }

        // Linked-List may be modified, so remember next event
        DocEvent* nextEvent=event->nextEvent;

        if(event->noteEventData.voiceDetectionResult == DocEvent::NoteEvent::VD_Yes)
        {
            // Note belongs to selected voice. Remove the parts inside current selection.
            clearEventRange_(trackIndex, event,
                             getEditorState().selection.ticksLeft,
                             getEditorState().selection.ticksRight,
                             false);
        }

        event=nextEvent;
    }
}

int CS_Utilities::quantizeTicksToCellRaster(int ticks)
{
    int leftCellBorderTicks  = docRoot->roundDownTicksToCellBorder(ticks, getEditorState().writeLength);

    if(ticks == leftCellBorderTicks)
        return ticks;   // already on cell border

    int rightCellBorderTicks = docRoot->roundUpTicksToCellBorder  (ticks, getEditorState().writeLength);

    if(ticks - leftCellBorderTicks < rightCellBorderTicks - ticks)
    {
        // nearest border is left border
        return leftCellBorderTicks;
    }
    else
    {
        // nearest border is right border
        return rightCellBorderTicks;
    }
}

void CS_Utilities::transposeSelection(TransposeModeType transposeMode, int steps)
{
    QString description;
    switch(transposeMode)
    {
    case TM_InOctaves:
        description=tr("Transpose in Octaves");
        break;
    case TM_Diatonically:
        description=tr("Transpose Diatonically");
        break;
    case TM_Chromatically:
        description=tr("Transpose Chromatically");
        break;
    default:
        Q_ASSERT(false);   // no transpose mode
        return;
    }
    beginMacro(description, getEditorState().selection);

    // track loop
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            if(event->type == DocEvent::E_Note)
            {
                switch(intervalRelation(getEditorState().selection, event))
                {
                case IR_before:
                case IR_after:
                    // leave event untouched
                    break;
                case IR_contains:
                case IR_overlaps:
                case IR_during:
                case IR_overlappedBy:
                    {
                        // Transpose note depending on transpose mode
                        DocEvent changedEventProperties(*event);

                        transposeNoteEvent(changedEventProperties, transposeMode, steps);

                        if(changedEventProperties != *event)
                            addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                        break;
                    }
                }
            }

            event=event->nextEvent;
        }
    }

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_Utilities::transposeNoteEvent(DocEvent& eventProperties, TransposeModeType transposeMode, int steps)
{
    int noteNumber=eventProperties.noteEventData.noteNumber;

    switch(transposeMode)
    {
    case TM_InOctaves:
        noteNumber+=steps * 12;
        break;
    case TM_Diatonically:
        {
            // Diatonic transposition in multiple steps is quite complicated.
            //  The whole calculation depends on the key signature at the start position of the note
            int keySignature=docRoot->ticksToMeasure(eventProperties.tickPosition).
                             measureProperties.keySignature;

            // Get root in circle of fifths. Make sure all note numbers are positive.
            int keySignatureRoot=(keySignature * 7) % 12;
            if(keySignatureRoot < 0) keySignatureRoot+=12;

            // calculate chromatic offset of current note number to nearest root below or equal to note number
            int chromaticOffset=(noteNumber - keySignatureRoot) % 12;
            if(chromaticOffset < 0) chromaticOffset+=12;

            // round down note number to nearest root
            noteNumber-=chromaticOffset;

            // get heptatonic index and check whether tone was augmented or diminished (accidentals -1,0,1)
            int heptatonicOffset=MOUSEPIANO_NOTE_INDEX_TO_WHITE_KEY_INDEX[chromaticOffset];
            bool offScale=MOUSEPIANO_NOTE_INDEX_TO_BLACK_KEY_INDEX[chromaticOffset] != -1;

            int accidental;
            if(!offScale)accidental=0;     // on scale
            else
            {
                if(keySignature < 0)
                {
                    accidental=-1;                // flat
                    heptatonicOffset=(heptatonicOffset + 1) % 7; // cyclic increment in key signature with flats
                }
                else accidental=1;                 // sharp
            }

            // transpose diatonically on heptatonic scale
            heptatonicOffset+=steps;

            // while heptatonic offset is out of range [0;6] shift root by octaves
            while(heptatonicOffset < 0)
            {
                heptatonicOffset+=7;
                noteNumber-=12;             // shift down full octave
            }
            while(heptatonicOffset >= 7)
            {
                heptatonicOffset-=7;
                noteNumber+=12;             // shift up full octave
            }

            // rebuild chromatic offset from heptatonic offset and accidental
            int transposedChromaticOffset=
                    MOUSEPIANO_WHITE_KEY_INDEX_TO_NOTE_INDEX[heptatonicOffset] + accidental;

            // rebuild note number
            noteNumber+=transposedChromaticOffset;
            break;
        }
    case TM_Chromatically:
        noteNumber+=steps;
        break;
    default:
        Q_ASSERT(false);   // no transpose mode
        return;
    }

    // clamp note number to allowed range
    if(noteNumber < 0                  )noteNumber = 0;
    if(noteNumber > MIDI_MAX_DATA_VALUE)noteNumber = MIDI_MAX_DATA_VALUE;

    eventProperties.noteEventData.noteNumber=noteNumber;
}

void CS_Utilities::addSwingToSelection(int swingHardness)
{
    beginMacro(tr("Add Swing"), getEditorState().selection);

    DocEvent::AddSwingSpecification addSwingSpecification;
    addSwingSpecification.selectionTicksLeft     = getEditorState().selection.ticksLeft;
    addSwingSpecification.selectionTicksRight    = getEditorState().selection.ticksRight;
    addSwingSpecification.selectionSwingHardness = swingHardness;

    // track loop
    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)    // event loop
        {
            switch(intervalRelation(getEditorState().selection, event))
            {
            case IR_before:
            case IR_after:
                // leave event untouched
                break;
            case IR_contains:
            case IR_overlaps:
            case IR_during:
            case IR_overlappedBy:
                {
                    DocEvent changedEventProperties(*event);

                    // Add swing
                    DocEvent::SwingPosition swingPosition=
                            event->calculateSwingStartAndEndTicks(docRoot, &addSwingSpecification);
                    changedEventProperties.tickPosition = swingPosition.startTicks;
                    changedEventProperties.tickLength   = swingPosition.endTicks - swingPosition.startTicks;

                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                    break;
                }
            }

            event=event->nextEvent;
        }
    }

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}
