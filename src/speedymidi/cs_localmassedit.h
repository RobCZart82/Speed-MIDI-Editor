/***************************************************************************
 *  cs_localmassedit.h - Controller Subsystem: Local Mass Edit
 *                       (Clear, Delete, Insert, Track/Measure Attributes)
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

#ifndef CS_LOCALMASSEDIT_H
#define CS_LOCALMASSEDIT_H

#include "cs_common.h"

class CS_LocalMassEdit : public CS_Common
{
    Q_OBJECT
public:
    CS_LocalMassEdit(Controller* controller);

    // Callbacks from track or measure properties dialogs
    void insertTrack(int beforeTrackIndex, DocTrack* newTrack, bool setSelectionToNewTrack);
    void modifyTrack(int trackIndex, const DocTrack& changedTrackProperties);
    void moveTrack(int trackIndexFrom, int trackIndexTo);
    bool setMeasureProperties(int measureIndex, DocMeasureItem changedMeasureProperties, int oldTicksPerMeasure, int newTicksPerMeasure, bool rebarEvents);

protected:
    virtual bool mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone);
    virtual bool mouseDoubleClickEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone);

protected slots:
    void actionEdit_ClearCells_Triggered();
    void actionEdit_Delete_Triggered();
    void actionEdit_Insert_Triggered();
    void actionEdit_InsertSelectedRange_Triggered();
    void actionEdit_NewTrackWizard_Triggered();
    void actionEdit_MeasureAttributes_Triggered();
    void actionEdit_TrackAttributes_Triggered();

protected:
    void execTrackPropertiesDialog(int trackIndex);
    void execMeasurePropertiesDialog(int measureIndex);

    void deleteTracks(int topTrack, int nTracksToDelete);
    void setNoTracksSelection(EditorState& newState) const;
    void insertMeasures(int nMeasures);
    void insertDefaultTracks(int nTracks);
    void insertDefaultTracks_(int beforeTrackIndex, int nTracks, EditorState& newState);
    void insertCells(int ticksToInsert);

    void rebarMeasureItemsAndEvents_(int rebarAreaTicksLeft, int oldTicksPerMeasure, int newTicksPerMeasure, bool rebarEvents);
    bool canRebar_(int rebarAreaTicksLeft, int oldTicksPerMeasure, int newTicksPerMeasure, bool rebarEvents) const;
    qint64 rebarTickPosition_(int rebarAreaTicksLeft, int rebarAreaTicksRight, int tickPosition, bool eventEnd, int oldTicksPerMeasure, int newTicksPerMeasure) const;
};

#endif // CS_LOCALMASSEDIT_H
