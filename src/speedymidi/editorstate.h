/***************************************************************************
 *  editorstate.h - State of Visual Editor
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

#ifndef EDITORSTATE_H
#define EDITORSTATE_H

#include "global.h"

enum SelectionModeType {
    S_LocalCells,
    S_GlobalMeasure,
    S_GlobalTrack
};

class WriteLength
{
public:
    WriteLength()
    {
        invalidate();
    }
    WriteLength(const WriteLength& rhs)
    {
        operator=(rhs);
    }
    WriteLength& operator=(const WriteLength& rhs)
    {
        denominator         = rhs.denominator;
        tupletNominator     = rhs.tupletNominator;
        tupletDenominator   = rhs.tupletDenominator;
        return *this;
    }
    bool operator==(const WriteLength& rhs) const
    {
        return
                denominator         == rhs.denominator &&
                tupletNominator     == rhs.tupletNominator &&
                tupletDenominator   == rhs.tupletDenominator;
    }
    void invalidate()
    {
        denominator=-1;
        tupletNominator=-1;
        tupletDenominator=-1;
    }

    int denominator;       // is power of 2
    int tupletNominator;   // [1;EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT] and comprime with denominator
    int tupletDenominator; // [1;EDITOR_MAX_TUPLET_FRACTIONAL_COMPONENT] and comprime with nominator
};

class EditorRange
{
public:
    EditorRange()
    {
        invalidate();
    }
    EditorRange(int ticksLeft, int trackTop, int ticksRight, int trackBottom)
    {
        this->ticksLeft=ticksLeft;
        this->trackTop=trackTop;

        this->ticksRight=ticksRight;
        this->trackBottom=trackBottom;
    }
    void invalidate()
    {
        ticksLeft=-1;
        trackTop=-1;

        ticksRight=-1;
        trackBottom=-1;
    }
    bool isValidRange() const
    {
        return ticksLeft >= 0 &&
                ticksRight >= 0 &&
                ticksRight >= ticksLeft &&
                trackTop >= 0 &&
                trackBottom >= 0 &&
                trackBottom >= trackTop;
    }
    void reset(int firstCellLength)
    {
        ticksLeft=0;
        trackTop=0;

        ticksRight=firstCellLength;
        trackBottom=0;
    }
    bool operator==(const EditorRange& rhs) const
    {
        return
                ticksLeft    == rhs.ticksLeft &&
                trackTop     == rhs.trackTop &&
                ticksRight   == rhs.ticksRight &&
                trackBottom  == rhs.trackBottom;
    }
    bool operator!=(const EditorRange& rhs) const
    {
        return !operator==(rhs);
    }
    SelectionModeType getSelectionMode() const
    {
        if(trackTop == 0 && trackBottom == INT_MAX)return S_GlobalMeasure;
        else if(ticksLeft == 0 && ticksRight == INT_MAX)return S_GlobalTrack;
        else return S_LocalCells;
    }
    EditorRange unionRange(const EditorRange& newRange) const
    {
        if(isValidRange())
        {
            if(newRange.isValidRange())
            {
                // both ranges are valid, so return the union range
                return EditorRange(
                        qMin(ticksLeft,   newRange.ticksLeft),
                        qMin(trackTop,    newRange.trackTop),
                        qMax(ticksRight,  newRange.ticksRight),
                        qMax(trackBottom, newRange.trackBottom));
            }
            else return *this;              // when one range is invalid, return the other
        }
        else
        {
            // when one range is invalid, return the other
            return newRange;
        }
    }
    bool isDisjoint(const EditorRange& rhs) const
    {
        return  ticksLeft   >= rhs.ticksRight  ||   // tick counts: >=, <=
                ticksRight  <= rhs.ticksLeft   ||
                trackTop    >  rhs.trackBottom ||   // track numbers: <,>
                trackBottom <  rhs.trackTop;
    }

    int ticksLeft;
    int ticksRight;    // exclusive
    
    int trackTop;
    int trackBottom;   // inclusive
};

class EditorSelectionSupportPoint
{
public:
    EditorSelectionSupportPoint()
    {
        invalidate();
    }
    EditorSelectionSupportPoint(int newTicksLeft, int newTicksRight, int newTrack)
    {
        setTo(newTicksLeft,newTicksRight,newTrack);
    }
    void invalidate()
    {
        ticksLeft=-1;
        ticksRight=-1;
        track=-1;
    }
    void reset(int firstCellLength)
    {
        ticksLeft=0;
        ticksRight=firstCellLength;
        track=0;
    }
    void setTo(int newTicksLeft, int newTicksRight, int newTrack)
    {
        ticksLeft   = newTicksLeft;
        ticksRight  = newTicksRight;
        track       = newTrack;
    }
    EditorRange toRange() const
    {
        return EditorRange(ticksLeft,track,ticksRight,track);
    }
    bool operator==(const EditorSelectionSupportPoint& rhs) const
    {
        return
                ticksLeft       == rhs.ticksLeft &&
                ticksRight      == rhs.ticksRight &&
                track           == rhs.track;
    }
    bool operator!=(const EditorSelectionSupportPoint& rhs) const
    {
        return !operator==(rhs);
    }
    bool isValid(const DocRoot* docRoot, const WriteLength& writeLength) const;

    int ticksLeft;
    int ticksRight;
    int track;
};

class EditorSelection : public EditorRange
{
public:
    void invalidate()
    {
        EditorRange::invalidate();
        anchor.invalidate();
    }
    void reset(int firstCellLength)
    {
        EditorRange::reset(firstCellLength);
        anchor.reset(firstCellLength);
    }
    void resetAnchorToCurrentRange()
    {
        Q_ASSERT(trackTop == trackBottom);

        anchor.ticksLeft   = ticksLeft;
        anchor.ticksRight  = ticksRight;
        anchor.track       = trackTop;
    }
    bool operator==(const EditorSelection& rhs) const
    {
        return
                anchor      == rhs.anchor &&
                EditorRange::operator==(rhs);
    }
    bool operator!=(const EditorSelection& rhs) const
    {
        return !operator==(rhs);
    }
    bool isValid(const DocRoot* docRoot, const WriteLength& writeLength) const;
    bool oneCellSelectedPerTrack() const
    {
        return  ticksLeft  == anchor.ticksLeft &&
                ticksRight == anchor.ticksRight;
    }

    // The anchor cell is used for selecting with Shift-key down (mouse and/or keyboard)
    //   Even in global measure/track selection modes, the anchor always refers to a SINGLE cell.
    EditorSelectionSupportPoint anchor;
};

class EditorTrackState
{
public:
    EditorTrackState()
    {
        // defaults for new tracks

        solo=false;
        mute=false;
        recordingEnabled=true;

        heightInNotes=22;
        centerMidiNote=12*5;    // middle piano c'
    }
    EditorTrackState& operator=(const EditorTrackState& rhs)
    {
        solo               = rhs.solo;
        mute               = rhs.mute;
        recordingEnabled   = rhs.recordingEnabled;

        heightInNotes      = rhs.heightInNotes;
        centerMidiNote     = rhs.centerMidiNote;

        return *this;
    }
    bool operator==(const EditorTrackState& rhs) const
    {
        return
                solo               == rhs.solo &&
                mute               == rhs.mute &&
                recordingEnabled   == rhs.recordingEnabled &&

                heightInNotes      == rhs.heightInNotes &&
                centerMidiNote     == rhs.centerMidiNote;
    }
    bool operator!=(const EditorTrackState& rhs) const
    {
        return !operator==(rhs);
    }
    bool isValid() const;
    void serialize(QDataStream& dataStream) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream);        // paste from clipboard

    bool solo;
    bool mute;
    bool recordingEnabled;

    double heightInNotes;
    double centerMidiNote; // center of track on screen is mapped to this MIDI note height [0;MIDI_MAX_DATA_VALUE]
          //  variables are floating point because of "sub-note" precision scrolling (range slider) and sizing
};

class EditorState
{
public:
    EditorState()
    {
        invalidate();
    }
    EditorState(const EditorState& rhs)
    {
        operator=(rhs);
    }
    EditorState& operator=(const EditorState& rhs)
    {
        if(&rhs == this)return *this;

        selection                    = rhs.selection;

        firstTrack                   = rhs.firstTrack;
        firstMeasure                 = rhs.firstMeasure;

        writeLength                  = rhs.writeLength;

        xZoomSliderValue             = rhs.xZoomSliderValue;
        yZoomSliderValue             = rhs.yZoomSliderValue;

        trackStateList.clear();
        trackStateList.append(rhs.trackStateList);

        return *this;
    }
    bool operator==(const EditorState& rhs) const
    {
        if(
            selection                    == rhs.selection &&

            firstTrack                   == rhs.firstTrack &&
            firstMeasure                 == rhs.firstMeasure &&

            writeLength                  == rhs.writeLength &&

            xZoomSliderValue             == rhs.xZoomSliderValue &&
            yZoomSliderValue             == rhs.yZoomSliderValue)
        {
            // compare track state lists
            if(trackStateList.size() == rhs.trackStateList.size())
            {
                for(int i=0; i < trackStateList.size(); ++i)
                    if(trackStateList[i] != rhs.trackStateList[i])return false;

                return true;
            }
        }
        return false;
    }
    void invalidate()
    {
        selection.invalidate();

        firstTrack=-1;
        firstMeasure=-1;

        writeLength.invalidate();

        xZoomSliderValue=-1;
        yZoomSliderValue=-1;

        trackStateList.clear();
    }

    int firstSelectedTrack() const { return selection.trackTop; }
    int lastSelectedTrack(const DocRoot* docRoot) const;
    int selectionTickLength() const
    {
        Q_ASSERT(selection.getSelectionMode() != S_GlobalTrack);
        return selection.ticksRight - selection.ticksLeft;
    }
    void setGlobalMeasureSelection(int fromMeasureIndex, int nMeasures, const DocRoot* docRoot);
    void setGlobalTrackSelection(int fromTrackIndex, int nTracks, const DocRoot* docRoot);

    void setStartupDefaultState(const DocRoot* docRoot);
    void setStartupSelection(const DocRoot* docRoot);
    bool loadFromXML(QDomElement& rootElement, int xmlConfigVersion);
    bool saveToXML(QDomElement& rootElement, int xmlConfigVersion) const;
protected:
    bool tryLoadFromXML(QDomElement& rootElement, int xmlConfigVersion);
public:
    bool isValid(const DocRoot* docRoot) const;
    double getTicksPerPixel(const DocRoot* docRoot) const;
    int getTrackHeightInPixels(int trackIndex) const;
    void adjustForChangedWriteLength(const DocRoot* docRoot);
    void repairSelection(const DocRoot* docRoot);

    // Zoom functions
    void setMinimumZoom(const DocRoot* docRoot);
    static double getZoomFactor(int sliderValue, double minZoomFactor, double maxZoomFactor, double expBase);
    static int getZoomSliderValue(double currentZoomFactor, double minZoomFactor, double maxZoomFactor, double expBase);
    static int getXZoomSliderValue(double TicksPerPixel, const DocRoot* docRoot);
    double getNoteHeightInPixels() const;

    EditorSelection selection;

    int firstTrack;
    int firstMeasure;

    WriteLength writeLength;

    int xZoomSliderValue;
    int yZoomSliderValue;

    QList<EditorTrackState> trackStateList;
};

class VolatileEditorState
{
public:
    VolatileEditorState()
    {
        showNoteNames=false;
        playbackLineCellX=-1;
        keyboardDragMode=KDM_None;
    }
    VolatileEditorState& operator=(const VolatileEditorState& rhs)
    {
        if(&rhs == this)return *this;

        showNoteNames          =   rhs.showNoteNames;
        playbackLineCellX      =   rhs.playbackLineCellX;
        keyboardDragMode       =   rhs.keyboardDragMode;

        return *this;

    }

    bool showNoteNames;
    int playbackLineCellX;
    enum KeyboardDragModeType { KDM_None, KDM_ImmediateListen, KDM_Transpose } keyboardDragMode;
};

#endif // EDITORSTATE_H
