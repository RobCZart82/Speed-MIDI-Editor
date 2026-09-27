/***************************************************************************
 *  editormapper.cpp - View Helper Class: Coordinate Mapping Model <-> View
 *                     (cf. Model–View–Controller Architecture)
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

#include "editormapper.h"
#include "view.h"
#include "doc_root.h"
#include "editorstate.h"

EditorMapper::EditorMapper(const View* view)
{
    // mapper object directly connected to current view
    this->view=view;
    tempEditorState=NULL;
}

EditorMapper::EditorMapper(const View* view, const EditorState* tempEditorState)
{
    // temporary mapper object for temporary state
    this->view=view;
    this->tempEditorState=tempEditorState;
}

EditorMapper::~EditorMapper()
{
    deleteLists();
}

const QRect EditorMapper::getCellArea() const
{
    return view->getCellArea();
}

const DocRoot* EditorMapper::getDocRoot() const
{
    return view->getDocRoot();
}

const EditorState& EditorMapper::getEditorState() const
{
    if(tempEditorState != NULL)return *tempEditorState;
    else return view->getEditorState();
}

void EditorMapper::deleteLists()
{
    for(int i=0; i < displayedMeasureList.size(); ++i)delete displayedMeasureList[i];
    displayedMeasureList.clear();
    for(int i=0; i < displayedCellList.size(); ++i)delete displayedCellList[i];
    displayedCellList.clear();
    for(int i=0; i < displayedTrackList.size(); ++i)delete displayedTrackList[i];
    displayedTrackList.clear();
}

void EditorMapper::refreshDisplayedItemLists()
{
    // Delete old lists
    deleteLists();

    // Now recalculate new lists

    Q_ASSERT(getEditorState().isValid(getDocRoot()));

    double ticksPerPixel=getEditorState().getTicksPerPixel(getDocRoot());

    // -------------------------------------------------------------------------------------------
    // 1. Measure and cell lists (horizontal direction)

    // Collect information set in all previous measure items
    DocMeasureItem effectiveMeasureProperties=getDocRoot()->getFirstMeasureEffectiveProperties();
    int nextMeasureItemIndex=1;

    int ticksPerMeasure=getDocRoot()->ticksPerMeasure(effectiveMeasureProperties);

    int measureIndex=0;
    int globalCellIndex=0;
    int x=0;
    int globalTickPosition=0;               // tick position relative to start of piece
    int viewLocalTickPosition=0;            // tick position relative to leftmost displayed measure border
    bool lastCell=false, finished=false;

    int measureOffsetToLastRehearsalMarker=-1;  // -1 means no rehearsal marker available
    if(effectiveMeasureProperties.setRehearsalMarker)measureOffsetToLastRehearsalMarker=0;

    while(true)
    {
        if(measureIndex >= getEditorState().firstMeasure)
        {
            // Visible measure
            DisplayedMeasure* dm=new DisplayedMeasure;

            dm->measureIndex=measureIndex;

            dm->firstGlobalCellIndex=globalCellIndex;
            dm->leftX=x;
            dm->tickPosition=globalTickPosition;
            dm->tickLength=ticksPerMeasure;
            dm->measureProperties=effectiveMeasureProperties;
            dm->measureOffsetToLastRehearsalMarker=measureOffsetToLastRehearsalMarker;

            displayedMeasureList.append(dm);

            // Add cells
            int measureInternalCellIndex=0;
            int measureInternalCellTickPosition=0;

            while(true)
            {
                int nextCellMeasureInternalTickPosition=
                        getDocRoot()->getNextCellMeasureInternalTickPosition(measureInternalCellIndex,
                                                                             getEditorState().writeLength);

                int tickLength=nextCellMeasureInternalTickPosition - measureInternalCellTickPosition;

                Q_ASSERT(tickLength > 0);

                // The tick-length of a standard (=non-shrinked) cell must be >= 1 in any case.
                // This is ensured by
                //  - the mininum tick resolution DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE
                //  - all UI Dialogs and keypress-handlers modifying the write-length, the obey
                //    EDITOR_MAX_WRITELENGTH_DENOMINATOR and EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT

                // A shrinked cell of tick-length 0 will never be created, because the measure is considered
                //  to be full already when the left border of the candidate shrinked cell is reached.

                DisplayedCell* dc=new DisplayedCell;

                dc->measureIndex=measureIndex;
                dc->measureInternalCellIndex=measureInternalCellIndex;
                dc->tickPosition=globalTickPosition + measureInternalCellTickPosition;
                dc->tickLength=tickLength;
                dc->leftX=x;

                // Create a shrinked cell as last cell if the number of cells fitting in this measure
                //  is not integral.
                if(nextCellMeasureInternalTickPosition > ticksPerMeasure)
                {
                    dc->tickLength-=nextCellMeasureInternalTickPosition - ticksPerMeasure;
                    dc->shrinked=true;
                }
                else dc->shrinked=false;

                displayedCellList.append(dc);

                // Determine right cell border position. Count the ticks in viewLocalTickPosition
                //  to allow for more precise zooming (cells may have sub-pixel-widths).
                viewLocalTickPosition+=dc->tickLength;
                int rightX=(int)(viewLocalTickPosition / ticksPerPixel);
                dc->cellWidth=rightX - x;

                if(lastCell)
                {
                    // This was the last cell to be created for the view.
                    // There is always one cell guaranteed to be completely right from the right view border.
                    finished=true;
                    break;
                }

                // Advance to next cell
                x+=dc->cellWidth;

                if(x > getCellArea().width())
                    lastCell=true;  // if sequencer area is full add exactly one additional cell

                ++measureInternalCellIndex;
                ++globalCellIndex;
                measureInternalCellTickPosition=nextCellMeasureInternalTickPosition;

                if(nextCellMeasureInternalTickPosition >= dm->tickLength)
                    break;  // reached end of measure
            }

            // after all cells have been created, store right measure border position
            dm->rightX=x;
        }

        if(finished)break;

        // Advance to next measure
        globalTickPosition+=ticksPerMeasure;
        ++measureIndex;

        if(measureOffsetToLastRehearsalMarker != -1)
            ++measureOffsetToLastRehearsalMarker;

        // accumulate new measure item data
        effectiveMeasureProperties.resetSetFlags();
        if(nextMeasureItemIndex < getDocRoot()->measureItemList.size())
        {
            DocMeasureItem* measureItem=getDocRoot()->measureItemList[nextMeasureItemIndex];
            if(measureItem->tickPosition == globalTickPosition)
            {
                effectiveMeasureProperties.makeEffectiveMeasureProperties(*measureItem);
                ticksPerMeasure=getDocRoot()->ticksPerMeasure(effectiveMeasureProperties);
                if(effectiveMeasureProperties.setRehearsalMarker)measureOffsetToLastRehearsalMarker=0;

                // Advance to next measure item
                ++nextMeasureItemIndex;
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // 2. Track lists (vertical direction)

    int y=0;
    for(int trackIndex=getEditorState().firstTrack; trackIndex < getDocRoot()->trackList.size(); ++trackIndex)
    {
        DisplayedTrack* dt=new DisplayedTrack;
        dt->trackIndex=trackIndex;
        dt->cellTopY    = y;
        dt->cellBottomY = y + getEditorState().getTrackHeightInPixels(trackIndex);
        displayedTrackList.append(dt);

        y = dt->cellBottomY + VIEW_TRACK_SEPARATOR_Y_INBETWEEN;

        if(y > getCellArea().height())break;    // no more tracks on screen
    }
}

int EditorMapper::getLastVisibleMeasure() const
{
    int lastVisibleMeasure=displayedMeasureList.first()->measureIndex;

    for(int i=0; i < displayedMeasureList.size(); ++i)
    {
        DisplayedMeasure* dm=displayedMeasureList[i];

        // test if left border of measure visible and not inside end-of-screen gradient
        if(dm->leftX < getCellArea().width() - VIEW_END_OF_SCREEN_GRADIENT_SIZE)
        {
            lastVisibleMeasure=dm->measureIndex;
        }
        else break;
    }

    return lastVisibleMeasure;
}

SelRectXPos EditorMapper::getSelectionRectXPosition() const
{
    SelRectXPos result;
    result.left=-1;     // invalidate member
    result.right=-1;    // invalidate member

    // Global track selection?
    if(getEditorState().selection.getSelectionMode() == S_GlobalTrack)
    {
        Q_ASSERT(getEditorState().selection.ticksLeft == 0);

        result.left=-1;
        result.right=INT_MAX;
    }
    else
    {
        // Cell selection or global measure selection.

        int i=0;

        // Left selection end
        if(getEditorState().selection.ticksLeft < displayedCellList.first()->tickPosition)
            result.left=-1;       // out of displayed area
        else if(getEditorState().selection.ticksLeft > displayedCellList.last()->tickPosition)
            result.left=INT_MAX;  // out of displayed area
        else
        {
            for(; i < displayedCellList.size(); ++i)
            {
                DisplayedCell* dc=displayedCellList[i];
                if(dc->tickPosition == getEditorState().selection.ticksLeft)
                {
                    // Found start of selection
                    result.left=dc->leftX;
                    break;
                }
            }
            Q_ASSERT(i < displayedCellList.size());    // must reach start of selection
        }

        // Right selection end
        if(getEditorState().selection.ticksRight < displayedCellList.first()->tickPosition)
            result.right=-1;       // out of displayed area
        else if(getEditorState().selection.ticksRight > displayedCellList.last()->tickPosition)
            result.right=INT_MAX;  // out of displayed area
        else
        {
            for(; i < displayedCellList.size(); ++i)
            {
                DisplayedCell* dc=displayedCellList[i];
                if(dc->tickPosition == getEditorState().selection.ticksRight)
                {
                    // Found end of selection
                    result.right=dc->leftX;
                    break;
                }
            }
            Q_ASSERT(i < displayedCellList.size());    // must reach end of selection
        }
    }

    return result;
}

TicksToViewXResult EditorMapper::ticksToViewX(int ticks) const
{
    Q_ASSERT(!displayedMeasureList.isEmpty() && !displayedCellList.isEmpty());

    TicksToViewXResult result;

    // Check boundaries of displayed range
    if(ticks < displayedCellList.first()->tickPosition)
    {
        // position is to the left of leftmost cell
        result.measureIndex=getDocRoot()->ticksToMeasure(ticks).measureIndex;
        result.cellLeftX =-1;
        result.cellRightX=-1;
        result.cellInternalOffsetX=-1;
        return result;
    }
    if(ticks >= displayedCellList.last()->nextCellTickPosition())
    {
        // position is to the right of rightmost cell
        result.measureIndex=getDocRoot()->ticksToMeasure(ticks).measureIndex;
        result.cellLeftX =INT_MAX;
        result.cellRightX=INT_MAX;
        result.cellInternalOffsetX=-1;
        return result;
    }

    // Tick position is within displayed area, so traverse cell array
    int i;
    for(i=0; i < displayedCellList.size(); ++i)
        if(ticks < displayedCellList[i]->nextCellTickPosition())break;

    DisplayedCell* dc=displayedCellList[i];

    result.measureIndex=dc->measureIndex;
    result.cellLeftX = getCellArea().left() + dc->leftX;
    result.cellRightX= getCellArea().left() + dc->leftX + dc->cellWidth;

    // There must be no cell with dc->tickLength == 0
    //  -> see comment section "What happens if cell tick-length is zero?" in refreshDisplayedItemLists
    Q_ASSERT(dc->tickLength > 0);
    result.cellInternalOffsetX= dc->cellWidth * (ticks - dc->tickPosition) / dc->tickLength;

    return result;
}

CellAreaXToTicksResult EditorMapper::cellAreaXToTicks(int cellAreaX) const
{
    CellAreaXToTicksResult result;

    Q_ASSERT(cellAreaX >= 0 && cellAreaX < getCellArea().width());

    // Traverse cell array
    int i;
    for(i=0; i < displayedCellList.size(); ++i)
    {
        DisplayedCell* dc=displayedCellList[i];
        if(cellAreaX < dc->leftX + dc->cellWidth)
        {
            // found matching cell
            result.measureIndex=dc->measureIndex;
            result.displayedCellIndex=i;
            result.cellLeftTicks=dc->tickPosition;
            result.cellRightTicks=dc->nextCellTickPosition();
            return result;
        }
    }

    Q_ASSERT(false);    // Impossible, out of cell range
    return result;
}

TrackToViewYResult EditorMapper::trackToViewY(int trackIndex) const
{
    TrackToViewYResult result;

    if(displayedTrackList.isEmpty())
    {
        result.TopY=-1;
        result.BottomY=-1;
        return result;
    }

    if(trackIndex < displayedTrackList[0]->trackIndex)
    {
        result.TopY=-1;
        result.BottomY=-1;
        return result;
    }

    if(trackIndex > displayedTrackList[displayedTrackList.size()-1]->trackIndex)
    {
        result.TopY=INT_MAX;
        result.BottomY=INT_MAX;
        return result;
    }

    for(int i=0; i < displayedTrackList.size(); ++i)
    {
        DisplayedTrack* dt=displayedTrackList[i];
        if(dt->trackIndex == trackIndex)
        {
            result.TopY=dt->cellTopY + getCellArea().top();
            result.BottomY=dt->cellBottomY + getCellArea().top();
            return result;
        }
    }
    Q_ASSERT(false);
    result.TopY=-1;     // invalidate member
    result.BottomY=-1;  // invalidate member
    return result;
}
