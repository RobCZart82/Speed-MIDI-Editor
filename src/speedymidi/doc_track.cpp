/***************************************************************************
 *  doc_track.cpp - Model class: Track
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

#include "doc_track.h"
#include "doc_root.h"
#include "doc_event.h"
#include "smfdocument.h"

#include <QDomElement>
#include <limits>

DocTrack::InitialMidiSetup DocTrack::initialMidiSetup() const
{
    InitialMidiSetup setup;
    qint64 firstNoteOrder=std::numeric_limits<qint64>::max();
    for(const DocEvent* event=firstEvent; event && event->tickPosition == 0; event=event->nextEvent)
    {
        qint64 order=-1;
        if(event->type == DocEvent::E_Note)order=event->noteEventData.importOnOrder;
        else if(event->type == DocEvent::E_OtherMidi &&
                (event->otherMidiEventData.midiCommand[0] & 0xf0) == 0x90 &&
                event->otherMidiEventData.midiCommand[2] != 0)
            order=event->otherMidiEventData.importOrder; // preserved unmatched NoteOn
        if(order >= 0)firstNoteOrder=qMin(firstNoteOrder,order);
    }
    for(const DocEvent* event=firstEvent; event && event->tickPosition == 0; event=event->nextEvent)
    {
        if(event->type == DocEvent::E_SysEx)
        {
            const qint64 order=event->sysExEventData.sysExEvent->importOrder;
            if(order < firstNoteOrder)setup.lastSysExBeforeNotes=qMax(setup.lastSysExBeforeNotes,order);
        }
        if(event->type != DocEvent::E_OtherMidi ||
           !event->otherMidiEventData.sameTickSubOrdering.beforeNoteEvents)continue;
        const auto& data=event->otherMidiEventData;
        const int command=data.midiCommand[0] & 0xf0;
        const DocEvent** destination=nullptr;
        if(command == 0xc0)destination=&setup.patch;
        else if(command == 0xb0 && data.midiCommand[1] == 7)destination=&setup.volume;
        else if(command == 0xb0 && data.midiCommand[1] == 10)destination=&setup.panorama;
        if(destination && (!*destination || data.importOrder > (*destination)->otherMidiEventData.importOrder ||
           (data.importOrder == (*destination)->otherMidiEventData.importOrder &&
            data.sameTickSubOrdering.index > (*destination)->otherMidiEventData.sameTickSubOrdering.index)))
            *destination=event;
    }
    return setup;
}

void DocTrack::InitialMidiSetup::applyProperties(const DocTrack& track, const DocEvent* event, quint8* message) const
{
    // Only the last initial value represents the editable track property.
    // Earlier source changes keep their values and their reset-relative order.
    if(event == volume)message[2]=track.midiVolume;
    if(event == panorama)message[2]=track.midiPanorama;
    if(event == patch)message[1]=track.midiPatch-1;
}

DocTrack::DocTrack()
{
    firstEvent=NULL;

    // invalidate properties

    //name;
    //eventColor;

    midiVolume=-1;
    midiPanorama=-1;
    midiPatch=-1;
    midiChannel=-1;
}

DocTrack::DocTrack(const DocTrack& rhs)
{
    firstEvent=NULL;
    operator=(rhs);
    // Property-only track copies used by clipboard paste also own the
    // non-shiftable metadata. Event copies are inserted separately there.
    for(const SmfMetaEvent* event : rhs.metaEventList)
        metaEventList.append(new SmfMetaEvent(*event));
}

DocTrack::~DocTrack()
{
    // Delete subobjects
    DocEvent* event=firstEvent;
    while(event)
    {
        DocEvent* nextEvent=event->nextEvent;
        delete event;
        event=nextEvent;
    }

    for(int i=0; i < metaEventList.size(); ++i)
        delete metaEventList[i];
    metaEventList.clear();
}

void DocTrack::makeDeepCopy(const DocTrack& rhs)
{
    // deep copy constructor for "save compatible file"
    operator=(rhs);

    // copy subobjects from rhs
    DocEvent* event=rhs.firstEvent;
    while(event)
    {
        DocEvent* eventCopy=new DocEvent;
        eventCopy->makeDeepCopy(*event);
        insertEvent(eventCopy);
        event=event->nextEvent;
    }
    for(int i=0; i < rhs.metaEventList.size(); ++i)
        metaEventList.append(new SmfMetaEvent(*rhs.metaEventList[i]));
}

DocTrack& DocTrack::operator=(const DocTrack& rhs)
{
    if(&rhs == this)return *this;

    // Undo/Redo: Command_TrackProperties
    name                    =   rhs.name;
    eventColor              =   rhs.eventColor;

    midiVolume              =   rhs.midiVolume;
    midiPanorama                 =   rhs.midiPanorama;
    midiPatch               =   rhs.midiPatch;
    midiChannel             =   rhs.midiChannel;

    return *this;
}

bool DocTrack::operator!=(const DocTrack& rhs) const
{
    // Undo/Redo: check before properties change undo command is required
    return
        name                    !=   rhs.name ||
        eventColor              !=   rhs.eventColor ||

        midiVolume              !=   rhs.midiVolume ||
        midiPanorama            !=   rhs.midiPanorama ||
        midiPatch               !=   rhs.midiPatch ||
        midiChannel             !=   rhs.midiChannel;
}

void DocTrack::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard

    dataStream << name;
    dataStream << eventColor;

    dataStream << midiVolume;
    dataStream << midiPanorama;
    dataStream << midiPatch;
    dataStream << midiChannel;

    // metaEventList is serialized for GlobalTrack-copy
    dataStream << qint32(metaEventList.size()); // clipboard format uses 32-bit counts
    for(int i=0; i < metaEventList.size(); ++i)
        metaEventList[i]->serialize(dataStream);
}

void DocTrack::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    dataStream >> name;
    dataStream >> eventColor;

    dataStream >> midiVolume;
    dataStream >> midiPanorama;
    dataStream >> midiPatch;
    dataStream >> midiChannel;

    // metaEventList is deserialized for GlobalTrack-paste
    int metaEventListSize=0;
    dataStream >> metaEventListSize;
    // Each entry needs at least tick (4), type (1), and data length (4).
    if(dataStream.status() != QDataStream::Ok || metaEventListSize < 0 ||
       !dataStream.device() || metaEventListSize > dataStream.device()->bytesAvailable() / 9)
    {
        dataStream.setStatus(QDataStream::ReadCorruptData);
        return;
    }
    for(int i=0; i < metaEventListSize; ++i)
    {
        SmfMetaEvent* smfMetaEvent=new SmfMetaEvent;
        smfMetaEvent->deserialize(dataStream);
        if(dataStream.status() != QDataStream::Ok)
        {
            delete smfMetaEvent;
            return;
        }
        metaEventList.append(smfMetaEvent);
    }
}

int DocTrack::getEventColorStandardIndex() const
{
    for(int j=0; j < DOCUMENT_N_EVENT_COLORS; ++j)
        if(eventColor == DOCUMENT_EVENT_COLORS[j])return j;

    return -1;  // no standard color
}

void DocTrack::setDefaultProperties(const DocRoot* docRoot)
{
    name=docRoot->getNewTrackIdentifier();
    eventColor=docRoot->getNewTrackEventColor();

    midiVolume=100;
    midiPanorama=MIDI_PANORAMA_CENTER;
    midiPatch=1;      // Grand Piano
    midiChannel=docRoot->getNewTrackChannel();
}

int DocTrack::centerNoteNumber() const
{
    // Determine minimum and maximum MIDI note number and return the average.
    //  If no note event exist, simple return the average of the data range [0; MIDI_MAX_DATA_VALUE]
    int minNoteNumber=MIDI_MAX_DATA_VALUE;
    int maxNoteNumber=0;

    DocEvent* event=firstEvent;
    while(event)
    {
        if(event->type == DocEvent::E_Note)
        {
            if(event->noteEventData.noteNumber < minNoteNumber)
                minNoteNumber=event->noteEventData.noteNumber;
            if(event->noteEventData.noteNumber > maxNoteNumber)
                maxNoteNumber=event->noteEventData.noteNumber;
        }

        event=event->nextEvent;
    }

    return (maxNoteNumber + minNoteNumber) / 2;
}

void DocTrack::scaleTickResolution(int newResolution, int oldResolution)
{
    // Update all events
    DocEvent* event=firstEvent;
    while(event)
    {
        event->scaleTickResolution(newResolution, oldResolution);
        event=event->nextEvent;
    }

    // also update track-local meta event list
    for(int j=0; j < metaEventList.size(); ++j)
        metaEventList[j]->scaleTickResolution(newResolution, oldResolution);
}

void DocTrack::makeCompatible(DocRoot* docRoot, const ConversionOptions& conversionOptions)
{
    // Update all events
    DocEvent* event=firstEvent;
    while(event)
    {
        event->makeCompatible(docRoot, conversionOptions);
        event=event->nextEvent;
    }
}

void DocTrack::insertEvent(DocEvent* event)
{
    // Insert event at head of list

    event->nextEvent=firstEvent;
    event->prevEvent=NULL;

    if(firstEvent)firstEvent->prevEvent=event;
    firstEvent=event;
}

void DocTrack::removeEvent(DocEvent* event)
{
    // Remove event from arbitrary position in list
    if(event->prevEvent)
    {
        event->prevEvent->nextEvent = event->nextEvent;
    }
    else firstEvent=event->nextEvent;

    if(event->nextEvent)
    {
        event->nextEvent->prevEvent = event->prevEvent;
        event->nextEvent=NULL;
    }
    event->prevEvent=NULL;
}

bool DocTrack::loadXMLTrackConfig(QDomElement& rootElement, int xmlConfigVersion)
{
    /* XML format specification:

       <track_config event_color="#RRGGBB"/>
    */
    UNUSED(xmlConfigVersion); // version is currently unused

    QString colorString=rootElement.attribute(XML_ATTR_EVENT_COLOR);
    QColor color(colorString);
    if(!color.isValid())return false;
    eventColor=color;

    return true;
}

bool DocTrack::saveXMLTrackConfig(QDomElement& rootElement, int xmlConfigVersion) const
{
    // Format specification: see loadFromXML(...)
    UNUSED(xmlConfigVersion); // version is currently unused

    rootElement.setAttribute(XML_ATTR_EVENT_COLOR, eventColor.name());
    return true;
}
