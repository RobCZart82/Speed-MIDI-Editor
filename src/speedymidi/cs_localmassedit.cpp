/***************************************************************************
 *  cs_localmassedit.cpp - Controller Subsystem: Local Mass Edit
 *                         (Clear, Delete, Insert, Track/Measure Attributes)
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

#include "cs_localmassedit.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "commands.h"
#include "insertdialog.h"
#include "measurepropertiesdialog.h"
#include "trackpropertiesdialog.h"
#include "trackwizarddialog.h"
#include "settings.h"

#include <QInputDialog>

#define CS_LOCALMASSEDIT_INSERT_MAX_MEASURE_COUNT        1024
#define CS_LOCALMASSEDIT_INSERT_MAX_TRACK_COUNT            64

CS_LocalMassEdit::CS_LocalMassEdit(Controller* controller)
        : CS_Common(controller)
{
    Controller::ActionGuards gDIC_CIS = Controller::DisallowWhenInputCaptured |
                                        Controller::CancelInterruptibleStates;
    Controller::ActionGuards gMHT = Controller::MustHaveTracks;

    registerActionHandler(ui->actionEdit_ClearCells,          "actionEdit_ClearCells_Triggered", gDIC_CIS | gMHT);
    registerActionHandler(ui->actionEdit_Delete,              "actionEdit_Delete_Triggered",             gDIC_CIS);
    registerActionHandler(ui->actionEdit_Insert,              "actionEdit_Insert_Triggered",             gDIC_CIS);
    registerActionHandler(ui->actionEdit_InsertSelectedRange, "actionEdit_InsertSelectedRange_Triggered",gDIC_CIS);
    registerActionHandler(ui->actionEdit_NewTrackWizard,      "actionEdit_NewTrackWizard_Triggered",     gDIC_CIS);
    registerActionHandler(ui->actionEdit_MeasureAttributes,   "actionEdit_MeasureAttributes_Triggered",  gDIC_CIS);
    registerActionHandler(ui->actionEdit_TrackAttributes,     "actionEdit_TrackAttributes_Triggered",    gDIC_CIS);
}

bool CS_LocalMassEdit::mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone)
{
    if(event->button() == Qt::LeftButton)
    {
        switch(mouseZone.zoneType)
        {
        case View::TrackButtonDelete:
            controller->cancelInterruptibleStates();
            deleteTracks(mouseZone.trackIndex,1);
            return true;
        case View::AddTrackButton:
            controller->cancelInterruptibleStates();

            // Show track properties dialog with index==size(),
            //  dialog will append new track if user confirms
            execTrackPropertiesDialog(docRoot->trackList.size());
            return true;
        default:
            // do nothing
            break;
        }
    }
    return false;   // event not handled
}

bool CS_LocalMassEdit::mouseDoubleClickEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone)
{
    if(event->button() == Qt::LeftButton)
    {
        switch(mouseZone.zoneType)
        {
        case View::MeasureHeader:
            {
                controller->cancelInterruptibleStates();

                CellAreaXToTicksResult ticksResult=
                        view->getMapper()->cellAreaXToTicks(event->x() - view->getCellArea().left());

                execMeasurePropertiesDialog(ticksResult.measureIndex);
                return true;
            }
        case View::TrackHeader:
            {
                controller->cancelInterruptibleStates();
                execTrackPropertiesDialog(mouseZone.trackIndex);
                return true;
            }
        default:
            return false;   // event not handled
        }
    }
    return false;   // event not handled
}

void CS_LocalMassEdit::actionEdit_ClearCells_Triggered()
{
    // Remove all note events within current selection
    beginMacro(tr("Clear Cells"), getEditorState().selection);

    for(int trackIndex=getEditorState().firstSelectedTrack(); trackIndex <= getEditorState().lastSelectedTrack(docRoot); ++trackIndex)
        clearTrackRange_(trackIndex, getEditorState().selection.ticksLeft, getEditorState().selection.ticksRight,false);

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_LocalMassEdit::actionEdit_Delete_Triggered()
{
    // Depending on selection mode, delete whole measures, whole tracks or local cells.
    //  Deletion means emptying the selected range and shifting the subsequent range up or to the left.

    SelectionModeType selMode=getEditorState().selection.getSelectionMode();
    if(selMode == S_GlobalTrack)
    {
        deleteTracks(getEditorState().firstSelectedTrack(),
                     getEditorState().lastSelectedTrack(docRoot) - getEditorState().firstSelectedTrack() + 1);
    }
    else
    {
        if(selMode == S_GlobalMeasure)
        {
            beginMacro(tr("Delete Measures"),
                       EditorRange(getEditorState().selection.ticksLeft,-1,
                                   getEditorState().selection.ticksRight,1));
        }
        else
        {
            beginMacro(tr("Delete Cells"),getEditorState().selection);
        }

        // global measure deletion includes local cell deletion
        EditorRange deleteCellsRange=getEditorState().selection;
        deleteCellsRange.trackBottom=getEditorState().lastSelectedTrack(docRoot);
        deleteCells_(deleteCellsRange);

        // When deleting global measures, modify the measure items as required
        if(selMode == S_GlobalMeasure)
        {
            deleteMeasureItems_(getEditorState().selection.ticksLeft,
                                getEditorState().selection.ticksRight);

            // adjust the global measure selection to the first non deleted measure
            EditorState newState=getEditorState();
            newState.selection.ticksRight=docRoot->roundUpTicksToMeasureBorder(
                    getEditorState().selection.ticksLeft + 1);

            newState.selection.anchor.ticksLeft=newState.selection.ticksLeft;

            newState.selection.anchor.ticksRight=docRoot->roundUpTicksToCellBorder(
                    newState.selection.ticksLeft + 1, newState.writeLength);

            endMacro(newState, EditorRange(newState.selection.ticksLeft,-1,
                                           newState.selection.ticksRight,-1));
        }
        else    // deleting local cells
        {
            endMacro(getEditorState(), getEditorState().selection); // no state change
        }
    }
}

void CS_LocalMassEdit::actionEdit_Insert_Triggered()
{
    if(!docRoot->hasTracks())
    {
        // Show modal dialog asking for insertion type (tracks/measures only)
        InsertDialog dlg(this);
        dlg.insertionType=InsertDialog::IS_Tracks;   // default insertion type is "tracks"
        dlg.noCellInsert=true;

        if(dlg.exec() == QDialog::Accepted)
        {
            switch(dlg.insertionType)
            {
            case InsertDialog::IS_Cells:    Q_ASSERT(false);                                  break;
            case InsertDialog::IS_Measures: insertMeasures     (dlg.numberOfMeasuresToInsert);break;
            case InsertDialog::IS_Tracks:   insertDefaultTracks(dlg.numberOfTracksToInsert);  break;
            }
        }
        return;
    }

    switch(getEditorState().selection.getSelectionMode())
    {
    case S_GlobalMeasure:
        {
            // Ask how many measures to insert
            int numberOfSelectedMeasures=
                    docRoot->ticksToMeasure(getEditorState().selection.ticksRight).measureIndex -
                    docRoot->ticksToMeasure(getEditorState().selection.ticksLeft).measureIndex;

            bool ok;
            int numberOfMeasuresToInsert=
                    QInputDialog::getInt(mainWindow,
                                         tr("Insert Measures"),
                                         tr("&How many measures"),
                                         numberOfSelectedMeasures,1,CS_LOCALMASSEDIT_INSERT_MAX_MEASURE_COUNT,
                                         1,&ok);
            if(ok)insertMeasures(numberOfMeasuresToInsert);
        }
        break;
    case S_GlobalTrack:
        {
            // Ask how many tracks to insert
            int numberOfTracksSelected=getEditorState().lastSelectedTrack(docRoot) -
                                       getEditorState().firstSelectedTrack() + 1;

            bool ok;
            int numberOfTracksToInsert=
                    QInputDialog::getInt(mainWindow,
                                         tr("Insert Tracks"),
                                         tr("&How many tracks"),
                                         numberOfTracksSelected,1,CS_LOCALMASSEDIT_INSERT_MAX_TRACK_COUNT,
                                         1,&ok);
            if(ok)insertDefaultTracks(numberOfTracksToInsert);
        }
        break;
    case S_LocalCells:
        {
            // Show modal dialog asking for insertion type
            InsertDialog dlg(this);
            dlg.insertionType=InsertDialog::IS_Cells;   // default insertion type is "cells"

            if(dlg.exec() == QDialog::Accepted)
            {
                switch(dlg.insertionType)
                {
                case InsertDialog::IS_Cells:
                    insertCells(getEditorState().selectionTickLength());
                    break;

                case InsertDialog::IS_Measures:
                    insertMeasures(dlg.numberOfMeasuresToInsert);
                    break;

                case InsertDialog::IS_Tracks:
                    insertDefaultTracks(dlg.numberOfTracksToInsert);
                    break;
                }
            }
        }
        break;
    }
}

void CS_LocalMassEdit::actionEdit_InsertSelectedRange_Triggered()
{
    switch(getEditorState().selection.getSelectionMode())
    {
    case S_GlobalMeasure:
        // insert as many measures as currently selected
        insertMeasures(docRoot->ticksToMeasure(getEditorState().selection.ticksRight).measureIndex -
                       docRoot->ticksToMeasure(getEditorState().selection.ticksLeft).measureIndex);
        break;
    case S_GlobalTrack:
        // insert as many tracks as currently selected
        insertDefaultTracks(getEditorState().selection.trackBottom - getEditorState().selection.trackTop + 1);
        break;
    case S_LocalCells:
        // insert as many ticks (not #cells!) as currently selected
        insertCells(getEditorState().selectionTickLength());
        break;
    }
}

void CS_LocalMassEdit::actionEdit_NewTrackWizard_Triggered()
{
    // execute track wizard
    TrackWizardDialog dlg;
    dlg.assignPatches=settings->LRU.trackWizardAssignPatches;
    if(dlg.exec() != QDialog::Accepted)return;

    settings->LRU.trackWizardAssignPatches=dlg.assignPatches;

    // determine where new tracks should be inserted/appended
    int insertionTrackIndex;
    if(docRoot->hasTracks())
    {
        switch(getEditorState().selection.getSelectionMode())
        {
        case S_GlobalMeasure:
            // global measure selection: append tracks
            insertionTrackIndex=docRoot->trackList.size();
            break;
        case S_GlobalTrack:
        case S_LocalCells:
            // insert tracks before top track of selection
            insertionTrackIndex=getEditorState().firstSelectedTrack();
        }
    }
    else    // document has currently no tracks
    {
        insertionTrackIndex=0;
    }

    beginMacro(tr("Track Wizard"),getEditorState().selection);
    EditorState newState=getEditorState();

    int numberOfCreatedTracks=0;
    while(!dlg.tracksToAdd.isEmpty())
    {
        EditorTrackState newTrackState;
        DocTrack* newTrack=docRoot->processTrackWizardString(
                settings->LRU.trackWizardAssignPatches,
                newTrackState, dlg.tracksToAdd);

        // Keep the document unchanged if the model rejects any unexpected
        // malformed remainder instead of passing a null track to the command.
        if(!newTrack)
        {
            endMacro(getEditorState(),getEditorState().selection);
            return;
        }

        addCommand(new Command_InsertTrack(insertionTrackIndex + numberOfCreatedTracks, newTrack));
        newState.trackStateList.insert(insertionTrackIndex + numberOfCreatedTracks, newTrackState);

        ++numberOfCreatedTracks;
    }

    if(numberOfCreatedTracks > 0)
    {
        // Select inserted tracks: Set global track selection
        newState.setGlobalTrackSelection(insertionTrackIndex, numberOfCreatedTracks, docRoot);
    }
    endMacro(newState, EditorRange(-1,newState.selection.trackTop,
                                   -1,newState.selection.trackBottom));
}

void CS_LocalMassEdit::actionEdit_MeasureAttributes_Triggered()
{
    execMeasurePropertiesDialog(docRoot->ticksToMeasure(getEditorState().selection.ticksLeft).measureIndex);
}

void CS_LocalMassEdit::actionEdit_TrackAttributes_Triggered()
{
    // if document has no tracks, this command will act like the append track button
    execTrackPropertiesDialog(getEditorState().firstSelectedTrack());
}

void CS_LocalMassEdit::execTrackPropertiesDialog(int trackIndex)
{
    TrackPropertiesDialog dlg(this,trackIndex);
    dlg.exec();     // Dialog will set properties by calling insertTrack/modifyTrack
}

void CS_LocalMassEdit::execMeasurePropertiesDialog(int measureIndex)
{
    MeasurePropertiesDialog dlg(this,measureIndex);
    dlg.exec();     // Dialog will set properties by calling setMeasureProperties
}

void CS_LocalMassEdit::deleteTracks(int topTrack, int nTracksToDelete)
{
    beginMacro(tr("Delete %n Track(s)","",nTracksToDelete),
               EditorRange(-1,topTrack,-1,topTrack + nTracksToDelete - 1));
    EditorState newState=getEditorState();

    for(int i=0; i < nTracksToDelete; ++i)
    {
        // topTrack is the index where to delete the respective tracks (index shift will occur).
        addCommand(new Command_DeleteTrack(topTrack, docRoot->trackList[topTrack]));

        // modify also editor state
        newState.trackStateList.removeAt(topTrack);

        if(!docRoot->hasTracks())   // all tracks deleted?
        {
            newState.firstTrack=0;
            setNoTracksSelection(newState);
        }
        else
        {
            // adjust vertical scroll position
            if(topTrack < newState.firstTrack)--newState.firstTrack;

            switch(newState.selection.getSelectionMode())
            {
            case S_GlobalMeasure:
                break; // Global measure selection remains untouched

            case S_GlobalTrack:
            case S_LocalCells:
                if(topTrack < newState.selection.trackTop)
                {
                    // Deleted a track above current selection
                    --newState.selection.trackTop;
                    --newState.selection.trackBottom;
                }
                else if(topTrack >= newState.selection.trackTop &&
                        topTrack <= newState.selection.trackBottom)
                {
                    // Deleted a track inside current selection
                    --newState.selection.trackBottom;

                    if(newState.selection.trackBottom < newState.selection.trackTop)
                    {
                        // Null-track selection occurred: Select only selection.trackTop, if it exists.
                        if(newState.selection.trackTop >= docRoot->trackList.size())
                        {
                            // selection.trackTop does not exist, use one track higher.
                            // trackList.size is at least 1 here, so selection.trackTop cannot get negative.
                            --newState.selection.trackTop;
                        }

                        // select exactly one track
                        newState.selection.trackBottom=newState.selection.trackTop;
                    }
                }

                // adjust selection anchor track
                if(newState.selection.anchor.track > newState.selection.trackBottom)
                    newState.selection.anchor.track=newState.selection.trackBottom;
                break;
            }
        }
    }

    endMacro(newState, EditorRange(-1,topTrack,-1,topTrack));
}

void CS_LocalMassEdit::setNoTracksSelection(EditorState& newState) const
{
    // If no tracks remain, change selection to a global measure selection of the first measure
    newState.setGlobalMeasureSelection(0, 1, docRoot);
}

void CS_LocalMassEdit::insertMeasures(int nMeasures)
{
    // Insertion point is left border of global measure selection
    int firstSelectedMeasureIndex = getFirstSelectedMeasureIndex();
    int numberOfSelectedMeasures  = getNumberOfSelectedMeasures();

    beginMacro(tr("Insert %n Measure(s)","",nMeasures),
               EditorRange(getEditorState().selection.ticksLeft,-1,
                           getEditorState().selection.ticksRight,-1));
    EditorState newState=getEditorState();

    // Determine ticks per inserted measure
    int ticksPerInsertedMeasure;
    if(getEditorState().selection.ticksLeft == 0)    // Insert measures at the beginning of the piece?
    {
        ticksPerInsertedMeasure=ticksPerMeasure(docRoot->ticksToMeasure(0).measureProperties);

        // Check if we must shift a measure item (the first one will not be shifted).
        if(docRoot->measureItemList.size() >= 2)
            addCommand(new Command_ShiftMeasureItems(1,
                                                     nMeasures * ticksPerInsertedMeasure));
    }
    else    // Insert measure in the middle of the piece
    {
        ticksPerInsertedMeasure=
                ticksPerMeasure(docRoot->ticksToMeasure(getEditorState().selection.ticksLeft - 1).measureProperties);
        for(int i=0; i < docRoot->measureItemList.size(); ++i)
        {
            if(docRoot->measureItemList[i]->tickPosition >= getEditorState().selection.ticksLeft)
            {
                // Shift required
                addCommand(new Command_ShiftMeasureItems(
                        getEditorState().selection.ticksLeft,nMeasures * ticksPerInsertedMeasure));
                break;
            }
        }
    }

    // Adjust right selection such that same number of measures remain selected
    newState.setGlobalMeasureSelection(firstSelectedMeasureIndex, numberOfSelectedMeasures, docRoot);

    // Also shift data in cells
    insertCells(nMeasures * ticksPerInsertedMeasure);

    endMacro(newState,
               EditorRange(newState.selection.ticksLeft,-1,
                           newState.selection.ticksRight,-1));
}

void CS_LocalMassEdit::insertDefaultTracks(int nTracks)
{
    int insertionTrackIndex;
    EditorRange scrollToRangeUndo;

    if(docRoot->hasTracks())
    {
        // Insertion point is top border of selection
        Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalTrack);

        insertionTrackIndex=getEditorState().selection.trackTop;
        scrollToRangeUndo=EditorRange(-1,insertionTrackIndex,-1,insertionTrackIndex);
    }
    else    // Document currently has no tracks
    {
        insertionTrackIndex=0;

        // Scrolling: scrollToRangeUndo remains invalid => no scrolling when operation is undone
    }

    beginMacro(tr("Insert %n Track(s)","",nTracks),scrollToRangeUndo);

    EditorState newState=getEditorState();

    insertDefaultTracks_(insertionTrackIndex,nTracks,newState);

    // Select inserted tracks: Set global track selection
    newState.setGlobalTrackSelection(insertionTrackIndex, nTracks, docRoot);

    endMacro(newState, EditorRange(-1,newState.selection.trackTop,
                                   -1,newState.selection.trackBottom));
}

void CS_LocalMassEdit::insertDefaultTracks_(int beforeTrackIndex, int nTracks, EditorState& newState)
{
    Q_ASSERT(isInMacro());

    for(int i=0; i < nTracks; ++i)
    {
        DocTrack* newTrack=new DocTrack;
        newTrack->setDefaultProperties(docRoot);
        addCommand(new Command_InsertTrack(beforeTrackIndex + i, newTrack));

        // insert new editor track state
        newState.trackStateList.insert(beforeTrackIndex + i, EditorTrackState());
    }
}

void CS_LocalMassEdit::insertCells(int ticksToInsert)
{
    beginMacro(tr("Insert Cells"), getEditorState().selection);

    // Insertion point is top/left border of selection, insertion length is given by selection length
    Q_ASSERT(getEditorState().selection.getSelectionMode() == S_LocalCells ||
             getEditorState().selection.getSelectionMode() == S_GlobalMeasure);

    insertCells_(getEditorState().selection.ticksLeft,
                ticksToInsert,
                getEditorState().firstSelectedTrack(),
                getEditorState().lastSelectedTrack(docRoot));

    endMacro(getEditorState(), getEditorState().selection);  // no state change
}

void CS_LocalMassEdit::insertTrack(int beforeTrackIndex, DocTrack* newTrack, bool setSelectionToNewTrack)
{
    beginMacro(tr("Insert Track"), EditorRange(-1,beforeTrackIndex,-1,beforeTrackIndex));
    EditorState newState=getEditorState();

    addCommand(new Command_InsertTrack(beforeTrackIndex,newTrack));

    // insert default editor track state
    newState.trackStateList.insert(beforeTrackIndex,EditorTrackState());

    if(setSelectionToNewTrack)
    {
        newState.setGlobalTrackSelection(beforeTrackIndex, 1, docRoot);

        scrollRangeIntoView(newState, newState.selection);
    }

    endMacro(newState, EditorRange(-1,beforeTrackIndex,-1,beforeTrackIndex));
}

void CS_LocalMassEdit::modifyTrack(int trackIndex, const DocTrack& changedTrackProperties)
{
    EditorRange scrollToRange(-1,trackIndex,-1,trackIndex);
    beginMacro(tr("Change Track Attributes"), scrollToRange);

    DocTrack* track=docRoot->trackList[trackIndex];

    if(changedTrackProperties.midiVolume != track->midiVolume)
    {
        // Volume change: Delete all shiftable MIDI volume control commands for this track
        DocEvent* event=track->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;
            if(event->type == DocEvent::E_OtherMidi)
            {
                if(event->otherMidiEventData.isVolumeControlCommand())
                    addCommand(new Command_DeleteEvent(trackIndex,event));
            }
            event=nextEvent;
        }
    }
    if(changedTrackProperties.midiPanorama != track->midiPanorama)
    {
        // Panorama change: Delete all shiftable MIDI panorama control commands for this track
        DocEvent* event=track->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;
            if(event->type == DocEvent::E_OtherMidi)
            {
                if(event->otherMidiEventData.isPanoramaControlCommand())
                    addCommand(new Command_DeleteEvent(trackIndex,event));
            }
            event=nextEvent;
        }
    }
    if(changedTrackProperties.midiPatch  != track->midiPatch)
    {
        // Patch change: Delete all shiftable MIDI patch change commands (Cx)
        DocEvent* event=track->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;
            if(event->type == DocEvent::E_OtherMidi)
            {
                if((event->otherMidiEventData.midiCommand[0] & 0xf0) == 0xc0)
                    addCommand(new Command_DeleteEvent(trackIndex,event));
            }
            event=nextEvent;
        }
    }

    if(changedTrackProperties != *track)
        addCommand(new Command_TrackProperties(trackIndex,changedTrackProperties));

    endMacro(getEditorState(), scrollToRange);
}

void CS_LocalMassEdit::moveTrack(int trackIndexFrom, int trackIndexTo)
{
    beginMacro(tr("Move Track"), EditorRange(-1,trackIndexFrom,-1,trackIndexFrom));
    EditorState newState=getEditorState();

    addCommand(new Command_MoveTrack(trackIndexFrom,trackIndexTo));

    // move also editor track state
    EditorTrackState trackState=newState.trackStateList.takeAt(trackIndexFrom);
    newState.trackStateList.insert(trackIndexTo,trackState);

    // if selection is touched, reset selection to topmost track only
    if(newState.selection.getSelectionMode() != S_GlobalMeasure)
    {
        bool bothBefore=
                trackIndexFrom < newState.firstSelectedTrack() &&
                trackIndexTo   < newState.firstSelectedTrack();

        bool bothAfter=
                trackIndexFrom > newState.lastSelectedTrack(docRoot) &&
                trackIndexTo   > newState.lastSelectedTrack(docRoot);

        if(!bothBefore && !bothAfter)
        {
            // reset selection to topmost track only
            newState.selection.trackBottom  = newState.selection.trackTop;
            newState.selection.anchor.track = newState.selection.trackTop;

            if(newState.selection.trackTop == trackIndexFrom)
            {
                // When this topmost track is itself moved, the selection follows the moved track.
                newState.selection.trackTop = trackIndexTo;
                newState.selection.trackBottom  = newState.selection.trackTop;
                newState.selection.anchor.track = newState.selection.trackTop;
            }
            else if(newState.selection.trackTop >= trackIndexFrom && newState.selection.trackTop <= trackIndexTo)
            {
                // When track is moved over the 1 selected track, shift selection to stay on selected track.
                --newState.selection.trackTop;
                newState.selection.trackBottom  = newState.selection.trackTop;
                newState.selection.anchor.track = newState.selection.trackTop;
            }
            else if(newState.selection.trackTop <= trackIndexFrom && newState.selection.trackTop >= trackIndexTo)
            {
                // When track is moved over the 1 selected track, shift selection to stay on selected track.
                ++newState.selection.trackTop;
                newState.selection.trackBottom  = newState.selection.trackTop;
                newState.selection.anchor.track = newState.selection.trackTop;
            }
        }
    }

    endMacro(newState, EditorRange(-1,trackIndexTo,-1,trackIndexTo));
}

bool CS_LocalMassEdit::setMeasureProperties(int measureIndex, DocMeasureItem changedMeasureProperties, int oldTicksPerMeasure, int newTicksPerMeasure, bool rebarEvents)
{
    if(oldTicksPerMeasure <= 0 || newTicksPerMeasure <= 0 ||
       measureIndex < 0)return false;
    const int measureTicksLeft=docRoot->measureToTicks(measureIndex);
    if(measureTicksLeft > INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                   EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR)return false;
    // Reject an out-of-domain result before changing properties or opening an
    // undo macro. Checking only the requested bar length misses later events.
    if(oldTicksPerMeasure != newTicksPerMeasure &&
       !canRebar_(measureTicksLeft,oldTicksPerMeasure,newTicksPerMeasure,rebarEvents))return false;
    changedMeasureProperties.clean();

    beginMacro(tr("Change Measure Attributes"), EditorRange(getEditorState().selection.ticksLeft,-1,
                                                            getEditorState().selection.ticksRight,-1));
    EditorState newState=getEditorState();

    // Determine numbers of measures currently selected.
    int firstSelectedMeasureIndex = getFirstSelectedMeasureIndex();
    int numberOfSelectedMeasures  = getNumberOfSelectedMeasures();

    DocMeasureItem* currentItem=docRoot->getMeasureItemAtExact(measureTicksLeft);
    if(changedMeasureProperties.measureItemRequired())
    {
        if(currentItem != NULL)
        {
            // modify existing measure item
            if(changedMeasureProperties != *currentItem)
                addCommand(new Command_MeasureItemProperties(currentItem,changedMeasureProperties));

            rebarMeasureItemsAndEvents_(measureTicksLeft, oldTicksPerMeasure, newTicksPerMeasure, rebarEvents);
        }
        else
        {
            // add a new measure item
            addCommand(new Command_InsertMeasureItem(new DocMeasureItem(changedMeasureProperties)));

            rebarMeasureItemsAndEvents_(measureTicksLeft, oldTicksPerMeasure, newTicksPerMeasure, rebarEvents);
        }
    }
    else
    {
        if(currentItem != NULL)
        {
            // delete measure item
            addCommand(new Command_DeleteMeasureItem(currentItem));

            rebarMeasureItemsAndEvents_(measureTicksLeft, oldTicksPerMeasure, newTicksPerMeasure, rebarEvents);
        }
        // else: no item existing, none required => do nothing
    }

    // modify also editor state: Select the measures with the numbers that were selected before.
    newState.setGlobalMeasureSelection(firstSelectedMeasureIndex, numberOfSelectedMeasures, docRoot);

    endMacro(newState, EditorRange(newState.selection.ticksLeft,-1,
                                   newState.selection.ticksRight,-1));
    return true;
}

bool CS_LocalMassEdit::canRebar_(int left, int oldLength, int newLength, bool rebarEvents) const
{
    int right=INT_MAX;
    for(const DocMeasureItem* item : docRoot->measureItemList)
        if(item->tickPosition > left && item->setTimeSignature) { right=item->tickPosition; break; }
    // Keep the same safety margin as MIDI import for the visible grid and
    // minimum-length notes at the end of a document.
    const qint64 maxTick=INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                  EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
    const auto fits=[&](int tick,bool end=false) {
        const qint64 mapped=rebarTickPosition_(left,right,tick,end,oldLength,newLength);
        return mapped >= 0 && mapped <= maxTick;
    };
    for(const DocMeasureItem* item : docRoot->measureItemList)
        if(!fits(item->tickPosition))return false;
    if(rebarEvents)
        for(const DocTrack* track : docRoot->trackList)
            for(const DocEvent* event=track->firstEvent;event;event=event->nextEvent)
                if(!fits(event->tickPosition) || !fits(event->tickPositionEnd(),true))return false;
    const EditorSelection& selection=getEditorState().selection;
    return fits(selection.ticksLeft) &&
           (selection.ticksRight == INT_MAX || fits(selection.ticksRight,true));
}

void CS_LocalMassEdit::rebarMeasureItemsAndEvents_(int rebarAreaTicksLeft, int oldTicksPerMeasure, int newTicksPerMeasure, bool rebarEvents)
{
    // Precondition: in macro execution
    Q_ASSERT(isInMacro());

    if(oldTicksPerMeasure == newTicksPerMeasure)
        return; // Nothing to do

    // Snapshot the conductor timeline so moving or merging entries is fully undoable.
    int rebarAreaTicksRight=INT_MAX;
    for(const DocMeasureItem* item : docRoot->measureItemList)
    {
        if(item->tickPosition > rebarAreaTicksLeft && item->setTimeSignature)
        {
            rebarAreaTicksRight=item->tickPosition;
            break;
        }
    }
    QList<DocMeasureItem> rebared;
    int denominator=4;
    for(const DocMeasureItem* original : docRoot->measureItemList)
    {
        DocMeasureItem item(*original);
        if(item.setTimeSignature)denominator=item.timeSignatureDenominator;
        if(item.setTempo && item.microsecondsPerQuarter == 0)
            item.microsecondsPerQuarter=item.tempoMicrosecondsPerQuarter(denominator);
        item.tickPosition=int(rebarTickPosition_(rebarAreaTicksLeft,rebarAreaTicksRight,
                                                 item.tickPosition,false,oldTicksPerMeasure,newTicksPerMeasure));
        if(!rebared.isEmpty() && rebared.last().tickPosition == item.tickPosition)
            rebared.last().mergeMeasureItemsPreservingTempo(item,denominator);
        else
            rebared.append(item);
    }
    const QList<DocMeasureItem*> originals=docRoot->measureItemList;
    for(DocMeasureItem* original : originals)addCommand(new Command_DeleteMeasureItem(original));
    for(const DocMeasureItem& item : rebared)
        addCommand(new Command_InsertMeasureItem(new DocMeasureItem(item)));

    // 2. rebar events if requested to do so
    if(!rebarEvents)return;

    // Iterate over all events in all tracks
    for(int trackIndex=0; trackIndex < docRoot->trackList.size(); ++trackIndex)
    {
        DocEvent* event=docRoot->trackList[trackIndex]->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;

            int newTickPositionStart=int(rebarTickPosition_(rebarAreaTicksLeft,rebarAreaTicksRight,
                                                        event->tickPosition,false,
                                                        oldTicksPerMeasure,newTicksPerMeasure));
            int newTickPositionEnd  =int(rebarTickPosition_(rebarAreaTicksLeft,rebarAreaTicksRight,
                                                        event->tickPositionEnd(),true,
                                                        oldTicksPerMeasure,newTicksPerMeasure));

            if(newTickPositionStart == newTickPositionEnd)
            {
                // Event was shrinked to zero length. Delete it.
                addCommand(new Command_DeleteEvent(trackIndex,event));
            }
            else
            {
                // Modify event properties
                DocEvent changedEventProperties(*event);
                changedEventProperties.tickPosition=newTickPositionStart;
                changedEventProperties.tickLength  =newTickPositionEnd - newTickPositionStart;

                if(changedEventProperties != *event)
                    addCommand(new Command_EventProperties( trackIndex, event, changedEventProperties));
            }

            event=nextEvent;
        }
    }
}

qint64 CS_LocalMassEdit::rebarTickPosition_(int rebarAreaTicksLeft, int rebarAreaTicksRight, int tickPosition, bool eventEnd, int oldTicksPerMeasure, int newTicksPerMeasure) const
{
    // before rebar area?
    if(tickPosition <= rebarAreaTicksLeft)  // use <= so first measure in rebar area is handled correctly
        return tickPosition;

    if(rebarAreaTicksRight != INT_MAX)
    {
        int rebarAreaTickLength=rebarAreaTicksRight - rebarAreaTicksLeft;
        Q_ASSERT(rebarAreaTickLength % oldTicksPerMeasure == 0); // must be full measures

        // after rebar area?
        if(tickPosition > rebarAreaTicksRight)  // use > because of end events
        {
            qint64 afterRebarAreaTickShift=
                    qint64(rebarAreaTickLength / oldTicksPerMeasure) * newTicksPerMeasure - rebarAreaTickLength;
            return qint64(tickPosition) + afterRebarAreaTickShift;
        }
    }

    // within rebar area
    int measureOffset  = (tickPosition - rebarAreaTicksLeft) / oldTicksPerMeasure;
    int inMeasureTicks = (tickPosition - rebarAreaTicksLeft) % oldTicksPerMeasure;

    if(eventEnd)
    {
        // Event end: tick on measure boundary belongs to previous measure
        if(inMeasureTicks == 0)
        {
            inMeasureTicks=oldTicksPerMeasure;
            --measureOffset;
        }
    }

    // Measure shrinked?
    if(inMeasureTicks > newTicksPerMeasure) inMeasureTicks=newTicksPerMeasure;

    // calculate new tick count
    return qint64(rebarAreaTicksLeft) + qint64(measureOffset) * newTicksPerMeasure + inMeasureTicks;
}
