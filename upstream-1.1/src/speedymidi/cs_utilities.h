/***************************************************************************
 *  cs_utilities.h - Controller Subsystem: Utilities
 *                   (Split, Connect, Quantize, ScaleLength, Swing, Transpose)
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

#ifndef CS_UTILITIES_H
#define CS_UTILITIES_H

#include "cs_common.h"

class CS_Utilities : public CS_Common
{
    Q_OBJECT
public:
    typedef QList<DocEvent*> EventList;

    CS_Utilities(Controller* controller);

    virtual bool keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual bool mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone);
    virtual bool wheelEvent(QWheelEvent* event);
    virtual void cancelInputCapture();

protected slots:
    void actionUtilities_SplitNotes_Triggered();
    void actionUtilities_ConnectNotes_Triggered();
    void actionUtilities_QuantizeToCellRaster_Triggered();
    void actionUtilities_ScaleNoteLength_Triggered();
    void actionUtilities_AddSwing_Triggered();
    void actionUtilities_TransposeOctaveDrag_Triggered();
    void actionUtilities_TransposeOctaveMulti_Triggered();
    void actionUtilities_TransposeDiatonicDrag_Triggered();
    void actionUtilities_TransposeDiatonicMulti_Triggered();
    void actionUtilities_TransposeChromaticDrag_Triggered();
    void actionUtilities_TransposeChromaticMulti_Triggered();
    void actionUtilities_RemoveTopVoice_Triggered();
    void actionUtilities_RemoveBottomVoice_Triggered();

protected:

    class OverlapFinder
    {
    public:
        OverlapFinder(const DocRoot* docRoot, const DocEvent* event, const QList<EventList>& _overlapIndex);
        OverlapFinder(const DocRoot* docRoot, int ticksLeft, int ticksRight, const QList<EventList>& _overlapIndex);
        void reset();
        DocEvent* next();

    protected:
        void init();

        const DocRoot* docRoot;
        const DocEvent* event;  // if event is NULL, only [ticksLeft;ticksRight) is searched
        int ticksLeft;
        int ticksRight;
        const QList<EventList>& _overlapIndex;

        int firstMeasure;
        int lastMeasure;

        int currentMeasure;
        int eventIndex;
    };

    class IntervalType
    {
    public:
        IntervalType(int start, int end)
        {
            this->start = start;
            this->end   = end;
        }
        int start,end;
    };

    enum VoiceType { V_Top, V_Bottom };
    void removeVoice(VoiceType voice);

    void createIndexOnNotesForTrack(int trackIndex, QList<EventList>& overlapIndex);

    void detectVoice(VoiceType voice, int trackIndex, QList<EventList>& overlapIndex);
    void detectVoice_initialClassification(VoiceType voice, int trackIndex, EventList& mayBeList, QList<EventList>& overlapIndex);
    bool detectVoice_processUnambiguousMayBeNotes(EventList& mayBeList, QList<EventList>& overlapIndex);
    DocEvent* detectVoice_getMinTickEvent(EventList& mayBeList);
    bool detectVoice_classifyAmbiguousMayBeNoteByIntervals(DocEvent* event, QList<EventList>& overlapIndex);
    bool detectVoice_classifyAmbiguousMayBeNoteByPrecedingNotes(int trackIndex, DocEvent* event, QList<EventList>& overlapIndex);
    void detectVoice_classifyAsYes(DocEvent* event, QList<EventList>& overlapIndex);
    void detectVoice_cleanUpMayBeList(EventList& mayBeList);

    void removeDetectedVoice(int trackIndex);

    int quantizeTicksToCellRaster(int ticks);

    enum TransposeModeType { TM_None, TM_InOctaves, TM_Diatonically, TM_Chromatically } transposeMode;
    void transposeSelection(TransposeModeType transposeMode, int steps);
    void transposeNoteEvent(DocEvent& eventProperties, TransposeModeType transposeMode, int steps);

    void addSwingToSelection(int swingHardness);
};

#endif // CS_UTILITIES_H
