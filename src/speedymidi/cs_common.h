/***************************************************************************
 *  cs_common.h - Common Functionality for all Controller Subsystems
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

#ifndef CS_COMMON_H
#define CS_COMMON_H

#include "global.h"
#include "controllersubsystem.h"

class CS_Common : public ControllerSubsystem
{
    Q_OBJECT
public:
    CS_Common(Controller* controller);

    void scrollRangeIntoView(EditorState& newState, EditorRange range) const;

protected:

    // Document information
    int ticksPerBeat   (const DocMeasureItem& measureProperties) const;
    int ticksPerMeasure(const DocMeasureItem& measureProperties) const;
    int getFirstSelectedMeasureIndex() const;
    int getLastSelectedMeasureIndex() const;
    int getNumberOfSelectedMeasures() const;

    // Tick interval relations
    enum IntervalRelationType { IR_before, IR_overlaps, IR_during, IR_contains, IR_overlappedBy, IR_after };
    static IntervalRelationType intervalRelation(int queryRangeLeft, int queryRangeRight, int eventLeft, int eventRight);
    static IntervalRelationType intervalRelation(const EditorRange& range, DocEvent* event);

    // Track flags
    enum TrackFlagType { TF_Solo, TF_Mute, TF_Record };
    void toggleTrackFlags(TrackFlagType flagType, int trackTop, int trackBottom, bool modShift);
    bool getTrackFlag(const EditorState& state, TrackFlagType flagType, int trackIndex);
    void setTrackFlag(EditorState& newState, TrackFlagType flagType, int trackIndex, bool value);

    // Clear/insert/delete functions with _ must be called with open macro bracket
    void clearTrackRange_(int trackIndex, int ticksLeft, int ticksRight, bool leaveNotesStartingEarlierUntouched);
    void clearEventRange_(int trackIndex, DocEvent* event, int ticksLeft, int ticksRight, bool leaveNotesStartingEarlierUntouched);
    void deleteCells_(const EditorRange& range);
    void deleteMeasureItems_(int ticksLeft, int ticksRight);
    void insertCells_(int insertAtTick, int ticksToInsert, int firstSelectedTrack, int lastSelectedTrack);
    bool canInsertCells_(int insertAtTick, qint64 ticksToInsert, int firstSelectedTrack, int lastSelectedTrack) const;
};

#endif // CS_COMMON_H
