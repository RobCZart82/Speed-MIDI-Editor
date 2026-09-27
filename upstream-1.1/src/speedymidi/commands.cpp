/***************************************************************************
 *  commands.cpp - Undo/Redo Commands Used by Controller
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

#include "commands.h"
#include "controller.h"
#include "doc_root.h"

Command_SetEditorState::Command_SetEditorState(const EditorState& state, const EditorRange& scrollToRange, bool afterOperation)
{
    this->state=state;
    this->scrollToRange=scrollToRange;
    this->afterOperation=afterOperation;
}

void Command_SetEditorState::redo()
{
    // only set the state in redo() if this is the state AFTER the operation
    if(afterOperation)controller->commandApplyUndoCommandStateAndUpdate(state,scrollToRange);
}

void Command_SetEditorState::undo()
{
    // only set the state in undo() if this is the state BEFORE the operation
    if(!afterOperation)controller->commandApplyUndoCommandStateAndUpdate(state,scrollToRange);
}

Command_MeasureItemProperties::Command_MeasureItemProperties(DocMeasureItem* measureItem, const DocMeasureItem& newPropertiesObject)
        : backupObject(newPropertiesObject)
{
    this->measureItem=measureItem;
    this->measureItem->clean();
}

void Command_MeasureItemProperties::swap()
{
    DocMeasureItem swapTemp(*measureItem);
    *measureItem=backupObject;
    backupObject=swapTemp;

    // View update: Measure properties may affect this measure and every measure behind
    controller->commandCollectDocumentModificationRange(
            EditorRange( measureItem->tickPosition, 0, INT_MAX, INT_MAX) );
}

Command_InsertMeasureItem::Command_InsertMeasureItem(DocMeasureItem* measureItem)
{
    this->measureItem=measureItem;
    this->measureItem->clean();
    owningObject=true;  // command type "insert", so use true
}

void Command_InsertMeasureItem::redo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
    {
        Q_ASSERT(docRoot->measureItemList[i]->tickPosition != measureItem->tickPosition);
        if(docRoot->measureItemList[i]->tickPosition > measureItem->tickPosition)break;
    }
    docRoot->measureItemList.insert(i, measureItem);
    controller->commandCollectDocumentModificationRange(
            EditorRange( measureItem->tickPosition, 0, INT_MAX, INT_MAX) );
    owningObject=false;
}

void Command_InsertMeasureItem::undo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
        if(docRoot->measureItemList[i] == measureItem)break;
    docRoot->measureItemList.removeAt(i);
    controller->commandCollectDocumentModificationRange( EditorRange( measureItem->tickPosition, 0, INT_MAX, INT_MAX) );
    owningObject=true;
}

Command_DeleteMeasureItem::Command_DeleteMeasureItem(DocMeasureItem* measureItem)
{
    this->measureItem=measureItem;
    owningObject=false;  // command type "delete", so use false
}

void Command_DeleteMeasureItem::redo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
        if(docRoot->measureItemList[i]->tickPosition == measureItem->tickPosition)break;
    docRoot->measureItemList.removeAt(i);
    controller->commandCollectDocumentModificationRange(
            EditorRange( measureItem->tickPosition, 0, INT_MAX, INT_MAX) );
    owningObject=true;
}

void Command_DeleteMeasureItem::undo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
    {
        Q_ASSERT(docRoot->measureItemList[i]->tickPosition != measureItem->tickPosition);
        if(docRoot->measureItemList[i]->tickPosition > measureItem->tickPosition)break;
    }

    docRoot->measureItemList.insert(i, measureItem);
    controller->commandCollectDocumentModificationRange(
            EditorRange( measureItem->tickPosition, 0, INT_MAX, INT_MAX) );
    owningObject=false;
}

Command_ShiftMeasureItems::Command_ShiftMeasureItems(int fromTickPosition, int deltaTicks)
{
    this->fromTickPosition=fromTickPosition;
    this->deltaTicks=deltaTicks;
}

void Command_ShiftMeasureItems::redo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
        if(docRoot->measureItemList[i]->tickPosition >= fromTickPosition)break;

    for(; i < docRoot->measureItemList.size(); ++i)
    {
        // Disallow any events that would be shifted over fromTickPosition (if deltaTicks < 0)
        Q_ASSERT(docRoot->measureItemList[i]->tickPosition + deltaTicks >= fromTickPosition);

        docRoot->measureItemList[i]->tickPosition += deltaTicks;
    }
    controller->commandCollectDocumentModificationRange(
            EditorRange( fromTickPosition, 0, INT_MAX, INT_MAX) );
}

void Command_ShiftMeasureItems::undo()
{
    int i;
    for(i=0; i < docRoot->measureItemList.size(); ++i)
        if(docRoot->measureItemList[i]->tickPosition >= fromTickPosition)break;

    for(; i < docRoot->measureItemList.size(); ++i)
    {
        // Disallow any events that would be shifted over fromTickPosition (if deltaTicks > 0)
        Q_ASSERT(docRoot->measureItemList[i]->tickPosition - deltaTicks >= fromTickPosition);

        docRoot->measureItemList[i]->tickPosition -= deltaTicks;
    }
    controller->commandCollectDocumentModificationRange(
            EditorRange( fromTickPosition, 0, INT_MAX, INT_MAX) );
}

Command_TrackProperties::Command_TrackProperties(int trackIndex, const DocTrack& newPropertiesObject)
        : backupObject(newPropertiesObject)
{
    this->trackIndex=trackIndex;
}

void Command_TrackProperties::swap()
{
    DocTrack swapTemp(*docRoot->trackList[trackIndex]);
    *docRoot->trackList[trackIndex]=backupObject;
    backupObject=swapTemp;

    controller->commandCollectDocumentModificationRange(
            EditorRange( 0, trackIndex, INT_MAX, trackIndex) );
}

Command_InsertTrack::Command_InsertTrack(int trackIndex, DocTrack* newTrack)
{
    this->trackIndex=trackIndex;
    this->track=newTrack;
    owningObject=true;  // command type "insert", so use true
}

void Command_InsertTrack::redo()
{
    docRoot->trackList.insert(trackIndex, track);
    controller->commandCollectDocumentModificationRange( EditorRange( 0, trackIndex, INT_MAX, INT_MAX) );
    owningObject=false;
    // GUI must modify EditorTrackState objects appropriately
}

void Command_InsertTrack::undo()
{
    docRoot->trackList.removeAt(trackIndex);
    controller->commandCollectDocumentModificationRange( EditorRange( 0, trackIndex, INT_MAX, INT_MAX) );
    owningObject=true;
    // GUI must modify EditorTrackState objects appropriately
}

Command_DeleteTrack::Command_DeleteTrack(int trackIndex, DocTrack* track)
{
    this->trackIndex=trackIndex;
    this->track=track;
    owningObject=false;  // command type "delete", so use false
}

void Command_DeleteTrack::redo()
{
    docRoot->trackList.removeAt(trackIndex);
    controller->commandCollectDocumentModificationRange( EditorRange( 0, trackIndex, INT_MAX, INT_MAX) );
    owningObject=true;
    // GUI must modify EditorTrackState objects appropriately
}

void Command_DeleteTrack::undo()
{
    docRoot->trackList.insert(trackIndex, track);
    controller->commandCollectDocumentModificationRange( EditorRange( 0, trackIndex, INT_MAX, INT_MAX) );
    owningObject=false;
    // GUI must modify EditorTrackState objects appropriately
}

Command_MoveTrack::Command_MoveTrack(int trackIndexFrom, int trackIndexTo)
{
    this->trackIndexFrom=trackIndexFrom;
    this->trackIndexTo=trackIndexTo;
}

void Command_MoveTrack::redo()
{
    DocTrack* track=docRoot->trackList[trackIndexFrom];
    docRoot->trackList.removeAt(trackIndexFrom);
    docRoot->trackList.insert(trackIndexTo, track);

    controller->commandCollectDocumentModificationRange(
            EditorRange( 0, qMin(trackIndexFrom,trackIndexTo), INT_MAX, qMax(trackIndexFrom,trackIndexTo)) );
    // GUI must modify EditorTrackState objects appropriately
}

void Command_MoveTrack::undo()
{
    DocTrack* track=docRoot->trackList[trackIndexTo];
    docRoot->trackList.removeAt(trackIndexTo);
    docRoot->trackList.insert(trackIndexFrom, track);

    controller->commandCollectDocumentModificationRange(
            EditorRange( 0, qMin(trackIndexFrom,trackIndexTo), INT_MAX, qMax(trackIndexFrom,trackIndexTo)) );
    // GUI must modify EditorTrackState objects appropriately
}

Command_EventProperties::Command_EventProperties(int trackIndex, DocEvent* event, const DocEvent& newPropertiesObject)
        : backupObject(newPropertiesObject)
{
    this->trackIndex=trackIndex;
    this->event=event;
}

void Command_EventProperties::swap()
{
    DocEvent swapTemp(*event);
    *event=backupObject;
    backupObject=swapTemp;

    controller->commandCollectDocumentModificationRange(
            EditorRange( qMin( event->tickPosition,backupObject.tickPosition),
                             trackIndex,
                             qMax( event->tickPosition + event->tickLength, backupObject.tickPosition + backupObject.tickLength),
                             trackIndex)
            );
}

Command_InsertEvent::Command_InsertEvent(int trackIndex, DocEvent* event)
{
    this->trackIndex=trackIndex;
    this->event=event;
    owningObject=true;  // command type "insert", so use true
}

void Command_InsertEvent::redo()
{
    docRoot->trackList[trackIndex]->insertEvent(event);

    controller->commandCollectDocumentModificationRange(
            EditorRange( event->tickPosition, trackIndex, event->tickPosition + event->tickLength, trackIndex) );
    owningObject=false;
}

void Command_InsertEvent::undo()
{
    docRoot->trackList[trackIndex]->removeEvent(event);

    controller->commandCollectDocumentModificationRange(
            EditorRange( event->tickPosition, trackIndex, event->tickPosition + event->tickLength, trackIndex) );
    owningObject=true;
}

Command_DeleteEvent::Command_DeleteEvent(int trackIndex, DocEvent* event)
{
    this->trackIndex=trackIndex;
    this->event=event;
    owningObject=false;  // command type "delete", so use false
}

void Command_DeleteEvent::redo()
{
    docRoot->trackList[trackIndex]->removeEvent(event);

    controller->commandCollectDocumentModificationRange(
            EditorRange( event->tickPosition, trackIndex, event->tickPosition + event->tickLength, trackIndex) );
    owningObject=true;
}

void Command_DeleteEvent::undo()
{
    docRoot->trackList[trackIndex]->insertEvent(event);

    controller->commandCollectDocumentModificationRange(
            EditorRange( event->tickPosition, trackIndex, event->tickPosition + event->tickLength, trackIndex) );
    owningObject=false;
}

Command_ShiftEvents::Command_ShiftEvents(int trackIndex, int fromTickPosition, int deltaTicks)
{
    this->trackIndex=trackIndex;
    this->fromTickPosition=fromTickPosition;
    this->deltaTicks=deltaTicks;
}

void Command_ShiftEvents::redo()
{
    DocTrack* parentTrack=docRoot->trackList[trackIndex];

    // ShiftEvents only considers events starting after fromTickPosition.
    DocEvent* event=parentTrack->firstEvent;
    while(event)
    {
        if(event->tickPosition >= fromTickPosition)
        {
            // Disallow any events that would be shifted over fromTickPosition (if deltaTicks < 0)
            Q_ASSERT(event->tickPosition + deltaTicks >= fromTickPosition);

            event->tickPosition += deltaTicks;
        }

        event=event->nextEvent;
    }

    controller->commandCollectDocumentModificationRange(
            EditorRange( fromTickPosition, trackIndex, INT_MAX, trackIndex) );
}

void Command_ShiftEvents::undo()
{
    DocTrack* parentTrack=docRoot->trackList[trackIndex];

    // ShiftEvents only considers events starting after fromTickPosition.
    DocEvent* event=parentTrack->firstEvent;
    while(event)
    {
        if(event->tickPosition >= fromTickPosition)
        {
            // Disallow any events that would be shifted over fromTickPosition (if deltaTicks > 0)
            Q_ASSERT(event->tickPosition - deltaTicks >= fromTickPosition);

            event->tickPosition -= deltaTicks;
        }

        event=event->nextEvent;
    }

    controller->commandCollectDocumentModificationRange(
            EditorRange( fromTickPosition, trackIndex, INT_MAX, trackIndex) );
}
