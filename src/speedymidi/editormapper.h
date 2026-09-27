/***************************************************************************
 *  editormapper.h - View Helper Class: Coordinate Mapping Model <-> View
 *                   (cf. Model–View–Controller Architecture)
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

#ifndef EDITORMAPPER_H
#define EDITORMAPPER_H

#include "global.h"
#include "doc_measureitem.h"

struct SelRectXPos { int left, right; };

class DisplayedMeasure
{
public:
    int measureIndex;
    int firstGlobalCellIndex;
    int tickPosition;
    int tickLength;
    int leftX;                      // relative to cellArea.left()
    int rightX;                     // relative to cellArea.left()
    DocMeasureItem measureProperties;
    int measureOffsetToLastRehearsalMarker;  // -1 means no rehearsal marker available
};

class DisplayedCell
{
public:
    int measureIndex;
    int measureInternalCellIndex;
    int tickPosition;
    int tickLength;
    int leftX;          // relative to cellArea.left()
    int cellWidth;
    bool shrinked;      // set if tickLength is not equal to current write length in editor state

    int nextCellTickPosition() { return tickPosition + tickLength; }
};

class DisplayedTrack
{
public:
    int trackIndex;
    int cellTopY;       // relative to cellArea.top()
    int cellBottomY;    // relative to cellArea.top()
};

struct TicksToViewXResult { int measureIndex, cellLeftX, cellRightX, cellInternalOffsetX; };
struct CellAreaXToTicksResult { int measureIndex, displayedCellIndex, cellLeftTicks, cellRightTicks; };
struct TrackToViewYResult { int TopY, BottomY; };

class EditorMapper
{
public:
    // create main mapper object directly connected to view's current editorState
    EditorMapper(const View* view);

    // create temporary mapper object for temporary editor state
    EditorMapper(const View* view, const EditorState* tempEditorState);

    ~EditorMapper();

    void refreshDisplayedItemLists();   // Explicit rebuild of displayed item lists

    int getLastVisibleMeasure() const;
    SelRectXPos getSelectionRectXPosition() const;

    TicksToViewXResult ticksToViewX(int ticks) const;
    CellAreaXToTicksResult cellAreaXToTicks(int cellAreaX) const;
    TrackToViewYResult trackToViewY(int trackIndex) const;

    QList<DisplayedMeasure*>const& getDisplayedMeasureList() const { return displayedMeasureList; }
    QList<DisplayedCell*>const& getDisplayedCellList() const { return displayedCellList; }
    QList<DisplayedTrack*>const& getDisplayedTrackList() const { return displayedTrackList; }

protected:
    QList<DisplayedMeasure*> displayedMeasureList;
    QList<DisplayedCell*> displayedCellList;
    QList<DisplayedTrack*> displayedTrackList;

    void deleteLists();

    const QRect getCellArea() const;
    const DocRoot* getDocRoot() const;
    const EditorState& getEditorState() const;

    const EditorState* tempEditorState;

    // Backlink
    const View* view;
};

#endif // EDITORMAPPER_H
