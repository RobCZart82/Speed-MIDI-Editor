/***************************************************************************
 *  doc_event.h - Model Class: Track Event
 *                (cf. Model–View–Controller Architecture)
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

#ifndef DOC_EVENT_H
#define DOC_EVENT_H

#include "global.h"

class DocEvent
{
public:
    DocEvent();
    DocEvent(const DocEvent& rhs);
    ~DocEvent();
    void makeDeepCopy(const DocEvent& rhs);

    DocEvent& operator=(const DocEvent& rhs);
    bool operator!=(const DocEvent& rhs) const;

    void invalidate();
    bool mustSerialize(int selectionTicksLeft, int selectionTicksRight) const
    {
        return touchesRange(selectionTicksLeft, selectionTicksRight);
    }
    void serialize(QDataStream& dataStream, int selectionTicksLeft, int selectionTicksRight, bool extended=true) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream);    // paste from clipboard

    void scaleTickResolution(int newResolution, int oldResolution);
    void makeCompatible(DocRoot* docRoot, const ConversionOptions& conversionOptions);

    struct AddSwingSpecification
    {
        int selectionTicksLeft;
        int selectionTicksRight;
        int selectionSwingHardness;
    };
    struct SwingPosition
    {
        int startTicks;
        int endTicks;
    };
    SwingPosition calculateSwingStartAndEndTicks(const DocRoot* docRoot, AddSwingSpecification* addSwingSpecification=NULL) const;
protected:
    int getSwingHardnessAt(const DocRoot* docRoot, int ticks, AddSwingSpecification* addSwingSpecification) const;
public:

    bool touchesRange(int ticksLeft, int ticksRight) const
    {
        Q_ASSERT(ticksLeft < ticksRight);
        // Range is [ticksLeft; ticksRight) so ticksRight itself is excluded from range.

        // If event is completely before or completely behind given range then it does not touch the range,
        //  otherwise it does.

        // Ticks are interpreted here as small time intervals, not as points in time.
        // Thus the time interval "tickPosition + tickLength" does not belong to the event anymore
        // and events of length 0 do not touch any range. (Therefore minimum event length is defined to be 1.)

        if(tickPosition + tickLength <= ticksLeft || tickPosition >= ticksRight)return false;
        else return true;
    }

    // Double-linked list. NULL terminates the list.
    DocEvent* prevEvent;
    DocEvent* nextEvent;

    class NoteEvent
    {
    public:
        int noteNumber;               // [0;MIDI_MAX_DATA_VALUE]
        int velocity;                 // [0;MIDI_MAX_DATA_VALUE]
        int releaseVelocity;          // [0;MIDI_MAX_DATA_VALUE], default 64 for new notes
        qint64 importOnOrder;          // source event order, -1 for editor-created notes
        qint64 importOffOrder;

        quint32 midiKeypressSerialNo; // Serial number of the MIDI keypress that created this note, used for
                                      //  writing with space bar. Is neither saved nor serialized to clipboard.
                                      //  Is 0 for all notes where the keypress S/N is unknown.

        // voiceDetectionResult is simply a result flag for an algorithm in CS_Utilities
        // (not saved/serialized/undone)
        enum VoiceDetectionResultType { VD_Unknown, VD_Yes, VD_MayBe, VD_No } voiceDetectionResult;

        NoteEvent();
        void invalidate();
        bool operator!=(const NoteEvent& rhs) const;
        void serialize(QDataStream& dataStream) const;
        void deserialize(QDataStream& dataStream);
        static QString getLongestPossibleName();
        QString getName(int keySignature) const;
    };
    class OtherMidiEvent
    {
    public:
        quint8 midiCommand[3];
        qint64 importOrder;            // source event order, -1 if unknown

        // Other MIDI command same-tick-subordering for sorting algorithms. Assigned when reading from SMF.
        struct SameTickSubOrderType
        {
            bool beforeNoteEvents;
            int index;
        }
        sameTickSubOrdering;

        OtherMidiEvent();
        void invalidate();
        bool operator!=(const OtherMidiEvent& rhs) const;
        void serialize(QDataStream& dataStream) const;
        void deserialize(QDataStream& dataStream);

        bool isVolumeControlCommand() const
        {
            return (midiCommand[0] & 0xf0) == 0xb0 && (midiCommand[1] == 0x07 || midiCommand[1] == 0x27);
        }
        bool isPanoramaControlCommand() const
        {
            return (midiCommand[0] & 0xf0) == 0xb0 && (midiCommand[1] == 0x0a || midiCommand[1] == 0x2a);
        }
    };
    class MetaEvent
    {
    public:
        MetaEvent();
        ~MetaEvent();
        void invalidate();
        MetaEvent& operator=(const MetaEvent& rhs);
        bool operator!=(const MetaEvent& rhs) const;

        SmfMetaEvent* metaEvent;       // time-shiftable meta event from SMF_Document (not editable)
        void serialize(QDataStream& dataStream) const;
        void deserialize(QDataStream& dataStream);
    };

    class SysExEvent
    {
    public:
        SysExEvent();
        ~SysExEvent();
        void invalidate();
        SysExEvent& operator=(const SysExEvent& rhs);
        bool operator!=(const SysExEvent& rhs) const;
        SmfSysExEvent* sysExEvent; // Opaque file packet; MIDI output is short-message-only.
        void serialize(QDataStream& dataStream) const;
        void deserialize(QDataStream& dataStream);
    };

    // -----------------------------------------------------------------------------
    // Properties covered by undo/redo

    enum EventType { E_Invalid, E_Note, E_OtherMidi, E_Meta, E_SysEx } type;

    int tickPosition;                       // >= 0
    int tickLength;                         // >= 1   IMPORTANT! NO ZERO LENGTHS!
    int tickPositionEnd() const { return tickPosition + tickLength; }

    NoteEvent noteEventData;                // type == E_Note      only
    OtherMidiEvent otherMidiEventData;      // type == E_OtherMidi only
    SysExEvent sysExEventData;              // type == E_SysEx     only
    MetaEvent metaEventData;                // type == E_Meta      only
};

#endif // DOC_EVENT_H
