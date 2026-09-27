/***************************************************************************
 *  smfexporter.cpp - Exports Editor Model to Standard MIDI File Data-Tree
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

#include "smfexporter.h"
#include <algorithm>
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"

#include <QDomDocument>

static const int SMF_EXPORTER_META_EVENT_ORDER[]=
{
    SMF_META_EVENT_TYPE_SEQ_NUMBER,
    SMF_META_EVENT_TYPE_TRACK_NAME,
    SMF_META_EVENT_TYPE_INSTRUMENT_NAME,
    SMF_META_EVENT_TYPE_CHANNEL_PREFIX,
    SMF_META_EVENT_TYPE_COPYRIGHT,
    SMF_META_EVENT_TYPE_TIME_SIGNATURE,
    SMF_META_EVENT_TYPE_KEY_SIGNATURE,
    SMF_META_EVENT_TYPE_TEMPO,
    SMF_META_EVENT_TYPE_CUE_POINT,
    SMF_META_EVENT_TYPE_MARKER, // Text behind ...
    SMF_META_EVENT_TYPE_TEXT,   // ... marker allows for rehearsal marker colors to be defined by text meta events
    SMF_META_EVENT_TYPE_LYRICS,
    SMF_META_EVENT_TYPE_SMPTE_OFFSET,
    SMF_META_EVENT_TYPE_SEQUENCER_SPECIFIC,
};
static const int SMF_EXPORTER_META_EVENT_ORDER_N_ENTRIES=sizeof(SMF_EXPORTER_META_EVENT_ORDER) / sizeof(SMF_EXPORTER_META_EVENT_ORDER[0]);

SmfExporter::SmfExporter(const DocRoot* docRoot, SmfDocument* smfDocument, const EditorState* editorState)
{
    this->docRoot=docRoot;
    this->smfDocument=smfDocument;
    this->editorState=editorState;

    xmlConfigVersion=-1;
}

bool SmfExporter::doExport(bool saveEditorState)
{
    xmlConfigVersion=XML_CONFIG_CURRENT_VERSION;

    // copy resolution
    smfDocument->setMidiTicksPerWholeNote(docRoot->midiTicksPerWholeNote);

    if(!exportConductorTrack(saveEditorState))return false;
    if(!exportNormalTracks())return false;
    return true;
}

bool SmfExporter::exportConductorTrack(bool saveEditorState)
{
    // Create SMF conductor track
    SmfTrack* conductorTrack=new SmfTrack;
    smfDocument->trackList.append(conductorTrack);

    if(!exportMainConfigXML(saveEditorState))return false;
    if(!exportConductorTrackMetaEvents())return false;

    // Stable-Sort generated events
    std::stable_sort(conductorTrack->eventList.begin(),conductorTrack->eventList.end(),eventOrderingLessThan);
    return true;
}

bool SmfExporter::exportMainConfigXML(bool saveEditorState)
{
    // XML format specification: see SmfImporter::importMainConfigXML

    QDomDocument domDoc;
    QDomElement configElement=domDoc.createElement(XML_TAG_SPEEDY_MIDI_CONFIG);
    domDoc.appendChild(configElement);
    configElement.setAttribute(XML_ATTR_VERSION,xmlConfigVersion);

    if(saveEditorState)
    {
        QDomElement editorStateElement=domDoc.createElement(XML_TAG_EDITOR_STATE);
        configElement.appendChild(editorStateElement);
        if(!editorState->saveToXML(editorStateElement,xmlConfigVersion))return false;
    }

    QString text=domDoc.toString(-1);    // generate XML text

    SmfMetaEvent* textMetaEvent=new SmfMetaEvent;
    textMetaEvent->tickPosition=0;
    textMetaEvent->metaEventType=SMF_META_EVENT_TYPE_TEXT;
    textMetaEvent->dataFromString(text);

    SmfTrack* conductorTrack=smfDocument->trackList[0];
    conductorTrack->eventList.append(textMetaEvent);
    return true;
}

bool SmfExporter::exportConductorTrackMetaEvents()
{
    SmfTrack* conductorTrack=smfDocument->trackList[0];

    // 1. Generate data defined by measure items
    for(int i=0; i < docRoot->measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=docRoot->measureItemList[i];

        // Prepare DOM document for possible additional information.
        //  If no information is filled in, optionalInfoDomDoc is discarded afterwards.
        //  Otherwise it is saved as a text event.
        QDomDocument measureItemDomDoc;
        QDomElement measureItemDomDocRootElement=measureItemDomDoc.createElement(XML_TAG_MEASURE_ITEM);
        measureItemDomDoc.appendChild(measureItemDomDocRootElement);

        if(measureItem->setTimeSignature)
        {
            SmfMetaEvent* metaEvent=new SmfMetaEvent;
            metaEvent->tickPosition=measureItem->tickPosition;
            metaEvent->metaEventType=SMF_META_EVENT_TYPE_TIME_SIGNATURE;
            metaEvent->dataLength=4;
            metaEvent->data=new quint8[metaEvent->dataLength];

            metaEvent->data[0]=measureItem->timeSignatureNominator;
            metaEvent->data[1]=0;
            for(int d=measureItem->timeSignatureDenominator; d > 1; d/=2)
                ++metaEvent->data[1];

            // metronome ticks per midi clocks: works correctly only up to 32 in the denominator
            metaEvent->data[2]=96 / measureItem->timeSignatureDenominator;
            metaEvent->data[3]=8;     // 32ths per 24 MIDI clocks / quarter note
            conductorTrack->eventList.append(metaEvent);
        }
        if(measureItem->setTempo)
        {
            SmfMetaEvent* metaEvent=new SmfMetaEvent;
            metaEvent->tickPosition=measureItem->tickPosition;
            metaEvent->metaEventType=SMF_META_EVENT_TYPE_TEMPO;
            metaEvent->dataLength=3;
            metaEvent->data=new quint8[metaEvent->dataLength];

            // convert value into beats per second, where a beat is defined by timeSignatureDenominator
            int timeSignatureDenominator = docRoot->ticksToMeasure(measureItem->tickPosition).
                                           measureProperties.timeSignatureDenominator;

            int microSecondsPerWholeNote= 60 * 1000000 * timeSignatureDenominator / measureItem->BPM;
            int microSecondsPerQuarter= microSecondsPerWholeNote / 4;

            // fill in 24-bit big endian format
            metaEvent->data[0]=(quint8)((microSecondsPerQuarter >> 16) & 0xff);
            metaEvent->data[1]=(quint8)((microSecondsPerQuarter >>  8) & 0xff);
            metaEvent->data[2]=(quint8)((microSecondsPerQuarter      ) & 0xff);
            conductorTrack->eventList.append(metaEvent);
        }
        if(measureItem->setKeySignature)
        {
            SmfMetaEvent* metaEvent=new SmfMetaEvent;
            metaEvent->tickPosition=measureItem->tickPosition;
            metaEvent->metaEventType=SMF_META_EVENT_TYPE_KEY_SIGNATURE;
            metaEvent->dataLength=2;
            metaEvent->data=new quint8[metaEvent->dataLength];

            metaEvent->data[0]=(quint8)(signed char)measureItem->keySignature;
            metaEvent->data[1]= (measureItem->keySignatureScale == DocMeasureItem::KSS_Major) ? 0 : 1;
            conductorTrack->eventList.append(metaEvent);
        }
        if(measureItem->setRehearsalMarker)
        {
            SmfMetaEvent* metaEvent=new SmfMetaEvent;
            metaEvent->tickPosition=measureItem->tickPosition;
            metaEvent->metaEventType=SMF_META_EVENT_TYPE_MARKER;
            metaEvent->dataFromString(measureItem->rehearsalMarkerText);
            conductorTrack->eventList.append(metaEvent);

            // XML format specification: see exportOtherConductorTrackMetaEvents
            QDomElement rehearsalMarkerElement=measureItemDomDoc.createElement(XML_TAG_REHEARSAL_MARKER);
            rehearsalMarkerElement.setAttribute(XML_ATTR_COLOR,measureItem->rehearsalMarkerColor.name());
            measureItemDomDocRootElement.appendChild(rehearsalMarkerElement);
        }
        if(measureItem->setPlaybackOptions)
        {
            // XML format specification: see exportOtherConductorTrackMetaEvents
            QDomElement playbackOptionsElement=measureItemDomDoc.createElement(XML_TAG_PLAYBACK_OPTIONS);
            playbackOptionsElement.setAttribute(XML_ATTR_SWING,measureItem->swingHardness);
            measureItemDomDocRootElement.appendChild(playbackOptionsElement);
        }

        if(measureItemDomDocRootElement.hasChildNodes())
        {
            // Save DOM document as a text event

            QString text=measureItemDomDoc.toString(-1);

            SmfMetaEvent* xmlTextMetaEvent=new SmfMetaEvent;
            xmlTextMetaEvent->tickPosition=measureItem->tickPosition;
            xmlTextMetaEvent->metaEventType=SMF_META_EVENT_TYPE_TEXT;
            xmlTextMetaEvent->dataFromString(text);
            conductorTrack->eventList.append(xmlTextMetaEvent);
        }
    }

    // 2. Copy non-shiftable meta events
    for(int i=0; i < docRoot->metaEventList.size(); ++i)
    {
        SmfMetaEvent* metaEvent=docRoot->metaEventList[i];
        conductorTrack->eventList.append(new SmfMetaEvent(*metaEvent));  // store a copy
    }
    return true;
}

bool SmfExporter::exportNormalTracks()
{
    // convert all editor tracks to SMF tracks
    for(int trackIndex=0; trackIndex < docRoot->trackList.size(); ++trackIndex)
    {
        DocTrack* track=docRoot->trackList[trackIndex];
        SmfTrack* smfTrack=new SmfTrack;
        smfDocument->trackList.append(smfTrack);

        if(!exportTrackConfigXML(track,smfTrack))return false;
        if(!exportTrackEvents(track,smfTrack))return false;

        // Stable-Sort generated events
    std::stable_sort(smfTrack->eventList.begin(),smfTrack->eventList.end(),eventOrderingLessThan);
    }
    return true;
}

bool SmfExporter::exportTrackConfigXML(DocTrack* track, SmfTrack* smfTrack)
{
    // XML format specification: see SmfImporter::importTrackConfigXML
    QDomDocument domDoc;
    QDomElement trackConfigElement=domDoc.createElement(XML_TAG_TRACK_CONFIG);
    domDoc.appendChild(trackConfigElement);

    if(!track->saveXMLTrackConfig(trackConfigElement,xmlConfigVersion))return false;

    QString text=domDoc.toString(-1);    // generate XML text

    SmfMetaEvent* textMetaEvent=new SmfMetaEvent;
    textMetaEvent->tickPosition=0;
    textMetaEvent->metaEventType=SMF_META_EVENT_TYPE_TEXT;
    textMetaEvent->dataFromString(text);

    smfTrack->eventList.append(textMetaEvent);
    return true;
}

bool SmfExporter::exportTrackEvents(DocTrack* track, SmfTrack* smfTrack)
{
    // Track name
    SmfMetaEvent* trackNameMetaEvent=new SmfMetaEvent;
    trackNameMetaEvent->tickPosition=0;
    trackNameMetaEvent->metaEventType=SMF_META_EVENT_TYPE_TRACK_NAME;
    trackNameMetaEvent->dataFromString(track->name);
    smfTrack->eventList.append(trackNameMetaEvent);

    // MIDI channel prefix
    SmfMetaEvent* channelPrefixMetaEvent=new SmfMetaEvent;
    channelPrefixMetaEvent->tickPosition=0;
    channelPrefixMetaEvent->metaEventType=SMF_META_EVENT_TYPE_CHANNEL_PREFIX;
    channelPrefixMetaEvent->dataLength=1;
    channelPrefixMetaEvent->data=new quint8[channelPrefixMetaEvent->dataLength];
    channelPrefixMetaEvent->data[0]=track->midiChannel - 1;
    smfTrack->eventList.append(channelPrefixMetaEvent);

    // MIDI volume
    SmfExporterMidiEvent* volumeMidiEvent=new SmfExporterMidiEvent;
    volumeMidiEvent->tickPosition=0;
    volumeMidiEvent->midiCommand[0]= 0xb0 + track->midiChannel - 1; // MIDI command: set controller
    volumeMidiEvent->midiCommand[1]= 0x07;                          // volume controller
    volumeMidiEvent->midiCommand[2]= track->midiVolume;
    volumeMidiEvent->beforeNoteEvents=true;     // for stable-sort
    volumeMidiEvent->index=0;                   // for stable-sort
    smfTrack->eventList.append(volumeMidiEvent);

    // MIDI panorama
    SmfExporterMidiEvent* panoramaMidiEvent=new SmfExporterMidiEvent;
    panoramaMidiEvent->tickPosition=0;
    panoramaMidiEvent->midiCommand[0]= 0xb0 + track->midiChannel - 1; // MIDI command: set controller
    panoramaMidiEvent->midiCommand[1]= 0x0a;                          // panorama controller
    panoramaMidiEvent->midiCommand[2]= track->midiPanorama;
    panoramaMidiEvent->beforeNoteEvents=true;     // for stable-sort
    panoramaMidiEvent->index=0;                   // for stable-sort
    smfTrack->eventList.append(panoramaMidiEvent);

    // MIDI patch
    SmfExporterMidiEvent* patchMidiEvent=new SmfExporterMidiEvent;
    patchMidiEvent->tickPosition=0;
    patchMidiEvent->midiCommand[0]= 0xc0 + track->midiChannel - 1; // MIDI command: program change
    patchMidiEvent->midiCommand[1]= track->midiPatch - 1;
    patchMidiEvent->beforeNoteEvents=true;      // for stable-sort
    patchMidiEvent->index=0;                    // for stable-sort
    smfTrack->eventList.append(patchMidiEvent);

    // Event list
    DocEvent* event=track->firstEvent;
    while(event)
    {
        switch(event->type)
        {
        case DocEvent::E_Note:
            {
                SmfExporterMidiEvent* noteOnEvent=new SmfExporterMidiEvent;
                noteOnEvent->tickPosition=event->tickPosition;
                noteOnEvent->midiCommand[0]=0x90 + track->midiChannel - 1;  // MIDI command: note-on
                noteOnEvent->midiCommand[1]=event->noteEventData.noteNumber;
                noteOnEvent->midiCommand[2]=event->noteEventData.velocity;
                noteOnEvent->index=-1;
                smfTrack->eventList.append(noteOnEvent);

                SmfExporterMidiEvent* noteOffEvent=new SmfExporterMidiEvent;
                noteOffEvent->tickPosition=event->tickPositionEnd();
                noteOffEvent->midiCommand[0]=0x80 + track->midiChannel - 1;  // MIDI command: note-off
                noteOffEvent->midiCommand[1]=event->noteEventData.noteNumber;
                noteOffEvent->midiCommand[2]=0x40;                           // default release velocity
                noteOffEvent->index=-1;
                smfTrack->eventList.append(noteOffEvent);
            }
            break;
        case DocEvent::E_OtherMidi:
            {
                SmfExporterMidiEvent* otherMidiEvent=new SmfExporterMidiEvent;
                otherMidiEvent->tickPosition=event->tickPosition;

                int command=event->otherMidiEventData.midiCommand[0];
                if(command >= 0x80 && command < 0xf0)
                {
                    // Change channel of command
                    command &= 0xf0;
                    command += track->midiChannel - 1;
                }

                otherMidiEvent->midiCommand[0]=command;
                otherMidiEvent->midiCommand[1]=event->otherMidiEventData.midiCommand[1];
                otherMidiEvent->midiCommand[2]=event->otherMidiEventData.midiCommand[2];
                otherMidiEvent->beforeNoteEvents=event->otherMidiEventData.sameTickSubOrdering.beforeNoteEvents;
                otherMidiEvent->index=event->otherMidiEventData.sameTickSubOrdering.index;
                smfTrack->eventList.append(otherMidiEvent);
            }
            break;
        case DocEvent::E_Meta:
            {
                SmfMetaEvent* metaEventCopy=new SmfMetaEvent(*event->metaEventData.metaEvent);
                metaEventCopy->tickPosition=event->tickPosition;    // update tick position
                smfTrack->eventList.append(metaEventCopy);
            }
            break;
        default:
            Q_ASSERT(false);
            break;
        }
        event=event->nextEvent;
    }

    // Copy non-shiftable meta events
    for(int i=0; i < track->metaEventList.size(); ++i)
    {
        SmfMetaEvent* metaEvent=track->metaEventList[i];
        smfTrack->eventList.append(new SmfMetaEvent(*metaEvent));  // store a copy
    }
    return true;
}

bool SmfExporter::eventOrderingLessThan(SmfEvent* e1, SmfEvent* e2)
{
    // see also CS_Playback::eventPlaybackOrderingLessThan(...)

    // First criterion: tick position
    if(e1->tickPosition < e2->tickPosition)return true;
    if(e1->tickPosition > e2->tickPosition)return false;

    // Same tick position: analyse contents

    SmfMetaEvent* metaEvent1=e1->isMetaEvent();
    SmfMetaEvent* metaEvent2=e2->isMetaEvent();

    SmfMidiEvent* midiEvent1=e1->isMidiEvent();
    SmfMidiEvent* midiEvent2=e2->isMidiEvent();

    // meta events always before MIDI events
    if(metaEvent1 != NULL && metaEvent2 == NULL)return true;
    if(metaEvent1 == NULL && metaEvent2 != NULL)return false;

    if(metaEvent1 != NULL && metaEvent2 != NULL)
    {
        // Both meta events: obey defined ordering
        int orderIndex1,orderIndex2;

        for(orderIndex1=0; orderIndex1 < SMF_EXPORTER_META_EVENT_ORDER_N_ENTRIES; ++orderIndex1)
            if(metaEvent1->metaEventType == SMF_EXPORTER_META_EVENT_ORDER[orderIndex1])break;

        for(orderIndex2=0; orderIndex2 < SMF_EXPORTER_META_EVENT_ORDER_N_ENTRIES; ++orderIndex2)
            if(metaEvent2->metaEventType == SMF_EXPORTER_META_EVENT_ORDER[orderIndex2])break;

        // Meta event types not listed in table will result in orderIndexX == SMF_EXPORTER_META_EVENT_ORDER_N_ENTRIES
        //  These types will always be behind the meta event types listed, sorted by their type number.

        if(orderIndex1 < orderIndex2)return true;
        if(orderIndex1 > orderIndex2)return false;

        return metaEvent1->metaEventType < metaEvent2->metaEventType;
    }

    // None is a meta event => both must be a MIDI event (no SysEx events in editor)
    Q_ASSERT(midiEvent1 != NULL && midiEvent2 != NULL);

    // get same-tick-subordering information
    SmfExporterMidiEvent* convMidiEvent1=(SmfExporterMidiEvent*)midiEvent1;
    SmfExporterMidiEvent* convMidiEvent2=(SmfExporterMidiEvent*)midiEvent2;

    int command1=midiEvent1->midiCommand[0] & 0xf0;
    int command2=midiEvent2->midiCommand[0] & 0xf0;

    bool event1isNoteEvent = command1 == 0x80 || command1 == 0x90;
    bool event2isNoteEvent = command2 == 0x80 || command2 == 0x90;

    int masterOrder1,masterOrder2;  // 0,1,2

    if(event1isNoteEvent)masterOrder1=1;
    else masterOrder1=convMidiEvent1->beforeNoteEvents ? 0:2;

    if(event2isNoteEvent)masterOrder2=1;
    else masterOrder2=convMidiEvent2->beforeNoteEvents ? 0:2;

    if(masterOrder1 < masterOrder2)return true;
    if(masterOrder1 > masterOrder2)return false;

    if(event1isNoteEvent && event2isNoteEvent)
    {
        // Both note events: note-off always before note-on
        bool noteOff1= command1 == 0x80 || (command1 == 0x90 && midiEvent1->midiCommand[2] == 0);
        bool noteOff2= command2 == 0x80 || (command2 == 0x90 && midiEvent1->midiCommand[2] == 0);

        if( noteOff1 && !noteOff2)return true;
        if(!noteOff1 &&  noteOff2)return false;

        // Both same type (on or off), sort for note number
        return midiEvent1->midiCommand[1] < midiEvent2->midiCommand[2];
    }

    // Both other events: take same-tick-subordering index
    Q_ASSERT(convMidiEvent1->index >= 0 && convMidiEvent2->index >= 0);
    return convMidiEvent1->index < convMidiEvent2->index;
}
