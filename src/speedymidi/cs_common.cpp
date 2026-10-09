/***************************************************************************
 *  cs_common.cpp - Common Functionality for all Controller Subsystems
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

#include "cs_common.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "commands.h"

CS_Common::CS_Common(Controller* controller)
        : ControllerSubsystem(controller)
{
}

void CS_Common::scrollRangeIntoView(EditorState& newState, EditorRange range) const
{
    // Parameter info:
    //  If range.ticksLeft == -1 and range.ticksRight  == -1, no horizontal scrolling is done
    //  If range.trackTop  == -1 and range.trackBottom == -1, no vertical   scrolling is done

    Q_ASSERT(range.ticksLeft <= range.ticksRight);

    EditorMapper tempMapper(view,&newState);
    tempMapper.refreshDisplayedItemLists();

    // Vertical scrolling
    if(docRoot->hasTracks() && range.getSelectionMode() != S_GlobalMeasure &&
       range.trackTop != -1 && range.trackBottom != -1)
    {
        Q_ASSERT(range.trackTop <= range.trackBottom);
        Q_ASSERT(range.trackTop >= 0 && range.trackBottom <= docRoot->trackList.size());

        // When requesting the first non-existing track as a scrollToPosition, use the last existing track.
        //  This behaviour is useful for a lot of begin/endMacro-scrollToRanges.
        if(range.trackBottom >= docRoot->trackList.size())
            range.trackBottom = docRoot->trackList.size() - 1;

        // 1. Bottom track
        while(!tempMapper.getDisplayedTrackList().isEmpty())
        {
            int lastTrackIndex=
                    tempMapper.getDisplayedTrackList()[tempMapper.getDisplayedTrackList().size()-1]->trackIndex;
            if(range.trackBottom < lastTrackIndex)break;
            if(range.trackBottom == lastTrackIndex)
            {
                // Check if completely visible
                if(tempMapper.trackToViewY(lastTrackIndex).BottomY < view->getCellArea().bottom())
                    break;  // Yes, completely visible.
            }

            // otherwise scroll down one track, but one track should remain visible
            if(newState.firstTrack == docRoot->trackList.size()-1)break;

            ++newState.firstTrack;
            tempMapper.refreshDisplayedItemLists();
        }

        // 2. Top track
        if(!tempMapper.getDisplayedTrackList().isEmpty())
        {
            if(range.trackTop < tempMapper.getDisplayedTrackList()[0]->trackIndex)
            {
                // Scroll up
                newState.firstTrack=range.trackTop;
                tempMapper.refreshDisplayedItemLists();
            }
        }
        else
        {
            // No track shown. Scroll up.
            newState.firstTrack=range.trackTop;
            tempMapper.refreshDisplayedItemLists();
        }
    }

    // Horizontal scrolling
    if(range.getSelectionMode() != S_GlobalTrack &&
       range.ticksLeft != -1 && range.ticksRight != -1)
    {
        // Use one tick less for right measure index:
        //  if ticksRight is on left border of last measure, we need not to scroll necessarily
        int leftMeasureIndex=docRoot->ticksToMeasure(range.ticksLeft).measureIndex;
        int rightMeasureIndex=docRoot->ticksToMeasure(range.ticksRight-1).measureIndex;

        // 3. Right measure

        // Check if corresponding cell is completely visible (not in the shading area)
        if(tempMapper.ticksToViewX(range.ticksRight-1).cellRightX >=
           view->getCellArea().right() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
        {
            // Scroll directly to that measure
            newState.firstMeasure=rightMeasureIndex;
            if(newState.firstMeasure > docRoot->getMaxFirstMeasure())
                newState.firstMeasure=docRoot->getMaxFirstMeasure();

            tempMapper.refreshDisplayedItemLists();

            if(tempMapper.ticksToViewX(range.ticksRight-1).cellRightX >=
               view->getCellArea().right() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
            {
                // Measure is too wide to fit in cellArea. User should zoom out. Do nothing automatically.
            }
            else
            {
                // This scroll setting shows the right area border, but try to minimize the scrolling needed.
                while(newState.firstMeasure >= 0)
                {
                    --newState.firstMeasure; // Back measure by measure
                    tempMapper.refreshDisplayedItemLists();

                    if(tempMapper.ticksToViewX(range.ticksRight-1).cellRightX >=
                       view->getCellArea().right() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
                    {
                        // Stop here. This was one measure too much.
                        ++newState.firstMeasure;
                        tempMapper.refreshDisplayedItemLists();
                        break;
                    }
                }
            }
        }

        // 4. Left measure
        if(leftMeasureIndex < newState.firstMeasure)
        {
            // Scroll to the left, directly to requested measure
            newState.firstMeasure=leftMeasureIndex;
            // not needed: tempMapper.refreshDisplayedItemLists();
        }
    }
}

int CS_Common::ticksPerBeat(const DocMeasureItem& measureProperties) const
{
    return docRoot->ticksPerBeat(measureProperties);
}

int CS_Common::ticksPerMeasure(const DocMeasureItem& measureProperties) const
{
    return docRoot->ticksPerMeasure(measureProperties);
}

int CS_Common::getFirstSelectedMeasureIndex() const
{
    Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);
    return docRoot->ticksToMeasure(getEditorState().selection.ticksLeft).measureIndex;
}

int CS_Common::getLastSelectedMeasureIndex() const
{
    Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);
    return docRoot->ticksToMeasure(getEditorState().selection.ticksRight).measureIndex - 1;
}

int CS_Common::getNumberOfSelectedMeasures() const
{
    Q_ASSERT(getEditorState().selection.getSelectionMode() == S_GlobalMeasure);
    return getLastSelectedMeasureIndex() - getFirstSelectedMeasureIndex() + 1;
}

CS_Common::IntervalRelationType CS_Common::intervalRelation(int queryRangeLeft, int queryRangeRight, int eventLeft, int eventRight)
{
    // Names of interval relations adapted from
    //  Allen: Maintaining Knowledge about Temporal Intervals. Communications ACM, 1983
    // where the "query interval" here is the current selection (orange on my lecture slides)

    // Not all relations are needed. The following relations are mapped to IntervalRelationType as follows
    //  meets      => before
    //  finishedBy => overlaps
    //  startedBy  => overlappedBy
    //  starts     => during
    //  equals     => during
    //  finishes   => during
    //  metBy      => after

    // queryRangeRight and eventRight are EXCLUDED from the actual interval

    // The remaining relations we need are:
    //  before, overlaps, during, contains, overlappedBy, after

    // In case queryRangeLeft==queryRangeRight this function only returns
    //  before, contains, during (event length == 0 and on query point), after

    Q_ASSERT(queryRangeLeft <= queryRangeRight);

    if(eventLeft < queryRangeLeft)
    {
        // before, meets, overlaps, finishedBy, contains
        if(eventRight <= queryRangeLeft)return IR_before;    // before, meets
        if(eventRight > queryRangeRight)return IR_contains;  // contains
        return IR_overlaps;                                  // overlaps, finishedBy
    }

    if(eventRight > queryRangeRight)
    {
        // startedBy, overlappedBy, metBy, after
        if(eventLeft >= queryRangeRight)return IR_after;    // metBy, after
        return IR_overlappedBy;                             // startedBy, overlappedBy
    }

    // starts, equals, during, finishes
    return IR_during;
}

CS_Common::IntervalRelationType CS_Common::intervalRelation(const EditorRange& range, DocEvent* event)
{
    return intervalRelation(range.ticksLeft, range.ticksRight, event->tickPosition, event->tickPositionEnd());
}

void CS_Common::toggleTrackFlags(TrackFlagType flagType, int trackTop, int trackBottom, bool modShift)
{
    EditorState newState=getEditorState();

    // Check if we should enable or disable the flag(s)
    bool enableFlags=false;

    if(modShift)
    {
        // Enable the flags, but disable when the flag is set exactly only in track range to modify
        for(int i=0; i < docRoot->trackList.size(); ++i)
        {
            bool inTrackRange= i >= trackTop && i <= trackBottom;   // in track range to modify?

            if(getTrackFlag(newState, flagType, i) != inTrackRange)
            {
                enableFlags=true;
                break;
            }
        }
    }
    else
    {
        // Enable the flags when at least one track has the flag cleared, otherwise disable
        for(int i=trackTop; i <= trackBottom; ++i)
        {
            if(!getTrackFlag(newState, flagType, i))
            {
                enableFlags=true;
                break;
            }
        }
    }

    for(int i=0; i < docRoot->trackList.size(); ++i)
    {
        if(i >= trackTop && i <= trackBottom)   // in track range to modify?
        {
            setTrackFlag(newState, flagType, i, enableFlags);
        }
        else    // out of track range to modify
        {
            // When pressing shift, disable all flags for the other tracks
            if(modShift)
                setTrackFlag(newState, flagType, i, false);
        }
    }

    applyStateAndUpdate(newState);
}

bool CS_Common::getTrackFlag(const EditorState& state, TrackFlagType flagType, int trackIndex)
{
    switch(flagType)
    {
    case TF_Solo:  return state.trackStateList[trackIndex].solo;
    case TF_Mute:  return state.trackStateList[trackIndex].mute;
    case TF_Record:return state.trackStateList[trackIndex].recordingEnabled;
    default:Q_ASSERT(false);return false;
    }
}

void CS_Common::setTrackFlag(EditorState& newState, TrackFlagType flagType, int trackIndex, bool value)
{
    switch(flagType)
    {
    case TF_Solo:  newState.trackStateList[trackIndex].solo            =value;break;
    case TF_Mute:  newState.trackStateList[trackIndex].mute            =value;break;
    case TF_Record:newState.trackStateList[trackIndex].recordingEnabled=value;break;
    default:Q_ASSERT(false);break;
    }
}

void CS_Common::clearTrackRange_(int trackIndex, int ticksLeft, int ticksRight, bool leaveNotesStartingEarlierUntouched)
{
    // Precondition: in macro execution
    Q_ASSERT(isInMacro());

    DocTrack* track=docRoot->trackList[trackIndex];
    DocEvent* event=track->firstEvent;

    while(event)
    {
        // Linked-List may be modified, so remember next event
        DocEvent* nextEvent=event->nextEvent;

        clearEventRange_(trackIndex, event, ticksLeft, ticksRight, leaveNotesStartingEarlierUntouched);

        event=nextEvent;
    }
}

void CS_Common::clearEventRange_(int trackIndex, DocEvent* event, int ticksLeft, int ticksRight, bool leaveNotesStartingEarlierUntouched)
{
    // Precondition: in macro execution
    Q_ASSERT(isInMacro());

    // Delete the part of the event within the selection range. Might split event into two.

    switch(intervalRelation(ticksLeft,ticksRight,event->tickPosition,event->tickPositionEnd()))
    {
    case IR_before:
    case IR_after:
        // leave event untouched
        break;
    case IR_during:
        // delete event
        addCommand(new Command_DeleteEvent(trackIndex, event));
        break;
    case IR_contains:
        // split event
        if(!leaveNotesStartingEarlierUntouched)
        {
            // right event
            DocEvent* newEvent=new DocEvent(*event);
            newEvent->tickPosition=ticksRight;
            newEvent->tickLength=event->tickPositionEnd() - newEvent->tickPosition;
            addCommand(new Command_InsertEvent(trackIndex, newEvent));

            // left event (re-use old event)
            DocEvent changedEventProperties(*event);
            changedEventProperties.tickLength=ticksLeft - event->tickPosition;
            if(changedEventProperties != *event)
                addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
        }
        break;
    case IR_overlappedBy:
        // shift event start
        {
            DocEvent changedEventProperties(*event);
            changedEventProperties.tickPosition=ticksRight;
            changedEventProperties.tickLength=event->tickPositionEnd() - changedEventProperties.tickPosition;
            if(changedEventProperties != *event)
                addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
        }
        break;
    case IR_overlaps:
        // shorten event
        if(!leaveNotesStartingEarlierUntouched)
        {
            DocEvent changedEventProperties(*event);
            changedEventProperties.tickLength=ticksLeft - event->tickPosition;
            if(changedEventProperties != *event)
                addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
        }
        break;
    }
}

void CS_Common::deleteCells_(const EditorRange& range)
{
    Q_ASSERT(isInMacro());

    for(int trackIndex=range.trackTop; trackIndex <= range.trackBottom; ++trackIndex)
    {
        bool shiftSubsequentEvents=false;

        DocEvent* event=docRoot->trackList[trackIndex]->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;

            // Delete the part of the event within the selection range.
            //  In contrast to the clear command, events here are never split.

            switch(intervalRelation(range,event))
            {
            case IR_before:
                // leave event untouched
                break;
            case IR_after:
                // event will be shifted by ShiftEvents command
                shiftSubsequentEvents=true;
                break;
            case IR_during:
                // delete event
                addCommand(new Command_DeleteEvent(trackIndex, event));
                break;
            case IR_contains:
                {
                    // re-use old event, shift event end
                    DocEvent changedEventProperties(*event);
                    changedEventProperties.tickLength-=range.ticksRight - range.ticksLeft;
                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                }
                break;
            case IR_overlappedBy:
                // shift event start
                {
                    DocEvent changedEventProperties(*event);
                    changedEventProperties.tickPosition=range.ticksRight;
                    changedEventProperties.tickLength=event->tickPositionEnd() - changedEventProperties.tickPosition;
                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));

                    shiftSubsequentEvents=true;
                }
                break;
            case IR_overlaps:
                // shorten event
                {
                    DocEvent changedEventProperties(*event);
                    changedEventProperties.tickLength=range.ticksLeft - event->tickPosition;
                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                }
                break;
            }

            event=nextEvent;
        }

        if(shiftSubsequentEvents)
        {
            // Shift all commands starting at range.ticksLeft or later
            addCommand(new Command_ShiftEvents(trackIndex,
                                               range.ticksLeft,
                                               - (range.ticksRight - range.ticksLeft)));
        }
    }
}

void CS_Common::deleteMeasureItems_(int ticksLeft, int ticksRight)
{
    Q_ASSERT(isInMacro());

    TicksToMeasureResult rRight=docRoot->ticksToMeasure(ticksRight);
    bool rightMeasureItemExists=rRight.measureProperties.measureItemRequired();

    DocMeasureItem measureItemProperties(rRight.measureProperties);

    if(ticksLeft == 0)
    {
        // There is no previous measure.
        //  There must be a measure item after the selection setting all available properties.
        //  This item will be shifted to tickPosition 0 afterwards.
        measureItemProperties.setRequiredFlagsFirstMeasure();
    }
    else
    {
        // There is a previous measure. Get measure item properties for that measure.
        TicksToMeasureResult rLeft=docRoot->ticksToMeasure(ticksLeft - 1);

        // Check which properties should be changed additionally to those changed by
        //  a measure item maybe existing at ticksRight.
        measureItemProperties.enforceChangedProperties(rLeft.measureProperties);
    }

    // Check if we actually need to have a measure item at ticksRight
    if(measureItemProperties.measureItemRequired())
    {
        if(rightMeasureItemExists)
        {
            // modify the existing item
            addCommand(new Command_MeasureItemProperties(
                    docRoot->getMeasureItemAtExact(ticksRight),
                    measureItemProperties));
        }
        else
        {
            // create a new item
            addCommand(new Command_InsertMeasureItem(new DocMeasureItem(measureItemProperties)));
        }
    }

    // Delete all measure items within [ticksLeft;ticksRight)
    bool shiftSubsequentMeasureItems=false;
    for(int i=0; i < docRoot->measureItemList.size();)  // beware of index shifts
    {
        DocMeasureItem* item=docRoot->measureItemList[i];
        if(item->tickPosition < ticksLeft)
        {
            ++i;
            continue;
        }
        if(item->tickPosition >= ticksRight)  // exclude ticksRight, so use >=
        {
            shiftSubsequentMeasureItems=true;
            break;
        }

        addCommand(new Command_DeleteMeasureItem(item));
    }

    if(shiftSubsequentMeasureItems)
    {
        // Shift all measure items starting at ticksLeft or later
        addCommand(new Command_ShiftMeasureItems(ticksLeft, -(ticksRight - ticksLeft)));
    }
}

bool CS_Common::canInsertCells_(int insertAtTick, qint64 ticksToInsert, int firstSelectedTrack, int lastSelectedTrack) const
{
    // Reserve a full maximum-sized bar for raster/selection calculations.
    const qint64 maxTick=INT_MAX - qint64(docRoot->midiTicksPerWholeNote) *
                                   EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
    if(insertAtTick < 0 || ticksToInsert <= 0 || insertAtTick + ticksToInsert > maxTick)
        return false;
    for(int i=firstSelectedTrack; i<=lastSelectedTrack; ++i)
        for(const DocEvent* event=docRoot->trackList[i]->firstEvent; event; event=event->nextEvent)
        {
            const qint64 end=qint64(event->tickPosition)+event->tickLength;
            if((event->tickPosition >= insertAtTick || end > insertAtTick) &&
               end + ticksToInsert > maxTick)return false;
        }
    return true;
}

void CS_Common::insertCells_(int insertAtTick, int ticksToInsert, int firstSelectedTrack, int lastSelectedTrack)
{
    Q_ASSERT(isInMacro());

    for(int trackIndex=firstSelectedTrack; trackIndex <= lastSelectedTrack; ++trackIndex)
    {
        bool shiftSubsequentEvents=false;

        DocTrack* track=docRoot->trackList[trackIndex];
        DocEvent* event=track->firstEvent;
        while(event)
        {
            DocEvent* nextEvent=event->nextEvent;

            switch(intervalRelation(insertAtTick,insertAtTick,
                                    event->tickPosition,event->tickPositionEnd()))
            {
            case IR_before:
                // leave event untouched
                break;
            case IR_after:
            case IR_during: // when event length == 0
                // event will be shifted by ShiftEvents command
                shiftSubsequentEvents=true;
                break;
            case IR_contains:
                // split event
                {
                    // right event, will be shifted afterwards
                    DocEvent* newEvent=new DocEvent(*event);
                    newEvent->tickPosition=insertAtTick;
                    newEvent->tickLength=event->tickPositionEnd() - newEvent->tickPosition;
                    addCommand(new Command_InsertEvent(trackIndex, newEvent));

                    shiftSubsequentEvents=true;

                    // left event (re-use old event)
                    DocEvent changedEventProperties(*event);
                    changedEventProperties.tickLength=insertAtTick - event->tickPosition;
                    if(changedEventProperties != *event)
                        addCommand(new Command_EventProperties(trackIndex, event, changedEventProperties));
                }
                break;
            case IR_overlappedBy:
            case IR_overlaps:
                Q_ASSERT(false);    // may not happen
                break;
            }

            event=nextEvent;
        }

        if(shiftSubsequentEvents)
        {
            // Shift all commands starting at insertAtTick or later
            addCommand(new Command_ShiftEvents(trackIndex, insertAtTick, ticksToInsert));
        }
    }
}
