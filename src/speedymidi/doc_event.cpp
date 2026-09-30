/***************************************************************************
 *  doc_event.cpp - Model Class: Track Event
 *                  (cf. Model–View–Controller Architecture)
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

#include "doc_event.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "smfdocument.h"
#include "settings.h"

#include <QApplication>

DocEvent::DocEvent()
{
    prevEvent=NULL;
    nextEvent=NULL;

    invalidate();
}

DocEvent::DocEvent(const DocEvent& rhs)
{
    prevEvent=NULL;
    nextEvent=NULL;

    operator=(rhs);
}

void DocEvent::makeDeepCopy(const DocEvent& rhs)
{
    // deep copy constructor for "save compatible file"
    operator=(rhs);

    // copy subobjects from rhs
    // (no subobjects)
}

DocEvent::~DocEvent()
{
}

DocEvent& DocEvent::operator=(const DocEvent& rhs)
{
    if(&rhs == this)return *this;

    // Undo/Redo: Command_EventProperties

    type                        =   rhs.type;

    tickPosition                =   rhs.tickPosition;
    tickLength                  =   rhs.tickLength;

    noteEventData               =   rhs.noteEventData;
    otherMidiEventData          =   rhs.otherMidiEventData;
    metaEventData               =   rhs.metaEventData;

    return *this;
}

bool DocEvent::operator!=(const DocEvent& rhs) const
{
    // Undo/Redo: check before properties change undo command is required
    return
            type                        !=   rhs.type ||

            tickPosition                !=   rhs.tickPosition ||
            tickLength                  !=   rhs.tickLength ||

            noteEventData               !=   rhs.noteEventData ||
            otherMidiEventData          !=   rhs.otherMidiEventData ||
            metaEventData               !=   rhs.metaEventData;
}

void DocEvent::invalidate()
{
    type=E_Invalid;
    tickPosition=-1;
    tickLength=-1;

    noteEventData.invalidate();
    otherMidiEventData.invalidate();
    metaEventData.invalidate();
}

void DocEvent::serialize(QDataStream& dataStream, int selectionTicksLeft, int selectionTicksRight) const
{
    // if event touches the range of the selection, copy properties to clipboard

    if(!mustSerialize(selectionTicksLeft, selectionTicksRight))return;

    dataStream << (int)type;

    // Store start and end position relative to selection start. Clamp values to selection borders.

    int relativeTickStart = tickPosition - selectionTicksLeft;
    if(relativeTickStart < 0)relativeTickStart=0;

    int relativeTickEnd  = qMin(tickPositionEnd(), selectionTicksRight) - selectionTicksLeft;

    dataStream << relativeTickStart;
    dataStream << relativeTickEnd;

    switch(type)
    {
    case E_Note     : noteEventData.serialize(dataStream);break;
    case E_OtherMidi: otherMidiEventData.serialize(dataStream);break;
    case E_Meta     : metaEventData.serialize(dataStream);break;
    default:Q_ASSERT(false);break;  // invalid event type
    }
}

void DocEvent::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    int iTemp;
    dataStream >> iTemp;
    type=(EventType)iTemp;

    dataStream >> tickPosition;     // relative to clipboard selection start

    int tickPositionEnd;
    dataStream >> tickPositionEnd;
    tickLength = tickPositionEnd - tickPosition;

    switch(type)
    {
    case E_Note     : noteEventData.deserialize(dataStream);break;
    case E_OtherMidi: otherMidiEventData.deserialize(dataStream);break;
    case E_Meta     : metaEventData.deserialize(dataStream);break;
    default:Q_ASSERT(false);break;  // invalid event type
    }
}

void DocEvent::scaleTickResolution(int newResolution, int oldResolution)
{
    // Don't scale the length directly, but the end position for better integer rounding precision
    int ticksEnd=tickPositionEnd();

    // use 64 bit to retain full integer precision
    tickPosition = (int)((qint64)tickPosition * newResolution / oldResolution);
    tickLength   = (int)((qint64)ticksEnd     * newResolution / oldResolution) - tickPosition;

    // If resolution is scaled down, check for a minimum tick length of 1
    if(tickLength < DOCUMENT_MIN_EVENT_LENGTH_TICKS)tickLength=DOCUMENT_MIN_EVENT_LENGTH_TICKS;
}

void DocEvent::makeCompatible(DocRoot* docRoot, const ConversionOptions& conversionOptions)
{
    // only playback option is currently swing
    if(conversionOptions.convertSwing)
    {
        SwingPosition swingPosition=calculateSwingStartAndEndTicks(docRoot);

        tickPosition = swingPosition.startTicks;
        tickLength   = swingPosition.endTicks - swingPosition.startTicks;
    }
}

DocEvent::SwingPosition DocEvent::calculateSwingStartAndEndTicks(const DocRoot* docRoot, AddSwingSpecification* addSwingSpecification) const
{
    SwingPosition swingPosition;
    swingPosition.startTicks = tickPosition;
    swingPosition.endTicks   = tickPositionEnd();

    int swingHardnessStart=getSwingHardnessAt(docRoot, tickPosition, addSwingSpecification);

    // Determine tick length of a note subject to swing adjustments
    int swingNoteBaseTickLength=
            docRoot->midiTicksPerWholeNote / DOCUMENT_SWING_PLAYBACK_BASE_NOTE_DENOMINATOR;

    if(type == E_Note)
    {
        // do not swing notes shorter than swingNoteBaseTickLength
        if(tickLength >= swingNoteBaseTickLength)
        {
            int swingHardnessEnd=getSwingHardnessAt(docRoot, tickPositionEnd(), addSwingSpecification);

            // adjust start and end position
            swingPosition.startTicks += swingNoteBaseTickLength * swingHardnessStart / 100 / 3;
            swingPosition.endTicks += swingNoteBaseTickLength * swingHardnessEnd / 100 / 3;
        }
    }
    else    // not a note (event without a length) => shift without changing duration
    {
        // Shift the event without changing its sentinel duration. Adjusting
        // only the start would make endTicks precede startTicks after a swing.
        const int swingOffset=swingNoteBaseTickLength * swingHardnessStart / 100 / 3;
        swingPosition.startTicks += swingOffset;
        swingPosition.endTicks += swingOffset;
    }

    return swingPosition;
}

int DocEvent::getSwingHardnessAt(const DocRoot* docRoot, int ticks, AddSwingSpecification* addSwingSpecification) const
{
    // Determine tick length of a note subject to swing adjustments
    int swingNoteBaseTickLength=
            docRoot->midiTicksPerWholeNote / DOCUMENT_SWING_PLAYBACK_BASE_NOTE_DENOMINATOR;

    int swingHardness=0;

    TicksToMeasureResult res = docRoot->ticksToMeasure(ticks);

    if(  res.measureInternalTicks % swingNoteBaseTickLength == 0 &&  // on base length?
       ((res.measureInternalTicks / swingNoteBaseTickLength) & 1))   // on odd multiple of base length?
    {
        if(addSwingSpecification != NULL)   // use manual swing specification
        {
            if(ticks >= addSwingSpecification->selectionTicksLeft &&
               ticks <  addSwingSpecification->selectionTicksRight)
            {
                swingHardness=addSwingSpecification->selectionSwingHardness;
            }
        }
        else    // use document playback options
        {
            swingHardness=res.measureProperties.swingHardness;
        }
    }

    return swingHardness;
}

DocEvent::NoteEvent::NoteEvent()
{
    invalidate();
}

void DocEvent::NoteEvent::invalidate()
{
    noteNumber=-1;
    velocity=-1;
    midiKeypressSerialNo=0;     // unknown serial number
    voiceDetectionResult=VD_Unknown;
}

bool DocEvent::NoteEvent::operator!=(const NoteEvent& rhs) const
{
    return
            noteNumber           != rhs.noteNumber ||
            velocity             != rhs.velocity ||
            midiKeypressSerialNo != rhs.midiKeypressSerialNo;
}

void DocEvent::NoteEvent::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard
    dataStream << noteNumber;
    dataStream << velocity;
    // midiKeypressSerialNo is not serialized, it is local to a document
}

void DocEvent::NoteEvent::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard
    dataStream >> noteNumber;
    dataStream >> velocity;
    midiKeypressSerialNo=0; // midiKeypressSerialNo is not serialized, it is local to a document
}

QString DocEvent::NoteEvent::getLongestPossibleName()
{
    // max. MIDI note   number = 127 (MIDI_MAX_DATA_VALUE)
    // max. MIDI octave number =  10 (MIDI_MAX_OCTAVE)
    // base for \' is octave 4, so return longest note name plus 6 x '
    return QApplication::translate("DocEvent","C#''''''");
}

QString DocEvent::NoteEvent::getName(int keySignature) const
{
    QString noteName;

    switch(noteNumber % 12)
    {
    case 0: noteName=QApplication::translate("DocEvent", "C");break;
    case 1: noteName=keySignature < 0 ? QApplication::translate("DocEvent","Db"):QApplication::translate("DocEvent","C#");break;
    case 2: noteName=QApplication::translate("DocEvent","D");break;
    case 3: noteName=keySignature <= 0 ? QApplication::translate("DocEvent","Eb"):QApplication::translate("DocEvent","D#");break;    // C-major: use Eb
    case 4: noteName=QApplication::translate("DocEvent","E");break;
    case 5: noteName=QApplication::translate("DocEvent","F");break;
    case 6: noteName=keySignature < 0 ? QApplication::translate("DocEvent","Gb"):QApplication::translate("DocEvent","F#");break;
    case 7: noteName=QApplication::translate("DocEvent","G");break;
    case 8: noteName=keySignature < 0 ? QApplication::translate("DocEvent","Ab"):QApplication::translate("DocEvent","G#");break;
    case 9: noteName=QApplication::translate("DocEvent","A");break;
    case 10:noteName=keySignature <= 0 ? QApplication::translate("DocEvent","Bb"):QApplication::translate("DocEvent","A#");break;   // C-major: use Bb
    case 11:noteName=QApplication::translate("DocEvent","B");break;
    }

    QString fullName;

    int midiOctave=noteNumber / 12;
    if(midiOctave >= 4)
    {
        fullName=noteName.toLower();    // use lowercase letters for octaves 4 and higher
        for(int i=4; i < midiOctave; ++i)
        {
            fullName+='\'';
        }
    }
    else
    {
        for(int i=3; i > midiOctave; --i)
        {
            fullName+=',';
        }
        fullName+=noteName;
    }

    return fullName;
}

DocEvent::OtherMidiEvent::OtherMidiEvent()
{
    invalidate();
}

void DocEvent::OtherMidiEvent::invalidate()
{
    midiCommand[0]=midiCommand[1]=midiCommand[2]=0xff;
    sameTickSubOrdering.beforeNoteEvents=false;
    sameTickSubOrdering.index=-1;
}

bool DocEvent::OtherMidiEvent::operator!=(const OtherMidiEvent& rhs) const
{
    return midiCommand[0] != rhs.midiCommand[0] ||
           midiCommand[1] != rhs.midiCommand[1] ||
           midiCommand[2] != rhs.midiCommand[2];
    // Do not compare same-tick-subordering
}

void DocEvent::OtherMidiEvent::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard
    dataStream << midiCommand[0];
    dataStream << midiCommand[1];
    dataStream << midiCommand[2];

    dataStream << sameTickSubOrdering.beforeNoteEvents;
    dataStream << sameTickSubOrdering.index;
}

void DocEvent::OtherMidiEvent::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard
    dataStream >> midiCommand[0];
    dataStream >> midiCommand[1];
    dataStream >> midiCommand[2];

    dataStream >> sameTickSubOrdering.beforeNoteEvents;
    dataStream >> sameTickSubOrdering.index;
}

DocEvent::MetaEvent::MetaEvent()
{
    metaEvent=NULL;
}

DocEvent::MetaEvent::~MetaEvent()
{
    delete metaEvent;
}

void DocEvent::MetaEvent::invalidate()
{
    delete metaEvent;
    metaEvent=NULL;
}

DocEvent::MetaEvent& DocEvent::MetaEvent::operator=(const MetaEvent& rhs)
{
    if(&rhs == this)return *this;

    delete metaEvent;
    metaEvent=NULL;

    // make an object copy
    if(rhs.metaEvent) metaEvent = new SmfMetaEvent(*rhs.metaEvent);

    return *this;
}

bool DocEvent::MetaEvent::operator!=(const MetaEvent& rhs) const
{
    if( metaEvent && !rhs.metaEvent)return true;
    if(!metaEvent &&  rhs.metaEvent)return true;
    if( metaEvent &&  rhs.metaEvent)return *metaEvent != *rhs.metaEvent;
    return false;   // Both NULL, so equal
}

void DocEvent::MetaEvent::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard

    bool metaEventExists=metaEvent != NULL;
    dataStream << metaEventExists;

    if(metaEvent != NULL)
        metaEvent->serialize(dataStream);
}

void DocEvent::MetaEvent::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    delete metaEvent;
    metaEvent=NULL;

    bool metaEventExists;
    dataStream >> metaEventExists;

    if(metaEventExists)
    {
        metaEvent=new SmfMetaEvent;
        metaEvent->deserialize(dataStream);
    }
}
