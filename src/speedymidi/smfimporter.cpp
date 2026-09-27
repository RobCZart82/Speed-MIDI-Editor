/***************************************************************************
 *  smfimporter.h - Imports Standard MIDI File Data-Tree to Editor Model
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

#include "smfimporter.h"
#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include "smfdocument.h"

#include <QDomDocument>

SmfImporter::SmfImporter(DocRoot* docRoot, SmfDocument* smfDocument, EditorState* editorState)
{
    this->docRoot=docRoot;
    this->smfDocument=smfDocument;
    this->editorState=editorState;

    xmlConfigVersion=-1;

    foundEditorState=false;
    mixedConductorAndMidiEventsTrack=false;
}

bool SmfImporter::doImport()
{
    if(smfDocument->getFormatTag() == 0 || smfDocument->hasMixedChannelsInTrack())
        smfDocument->convertToFormat1(true);

    // Set resolution obtained from SMF. Resolution may be adjusted when document has been loaded successfully.
    docRoot->midiTicksPerWholeNote=smfDocument->getMidiTicksPerWholeNote();

    if(!importConductorTrack())return false;
    if(!importNormalTracks())return false;

    // If no editor state was saved within the SMF, set a default startup state
    if(!foundEditorState)
    {
        editorState->setStartupDefaultState(docRoot);
    }
    else
    {
        // Check if saved editor state has correct track states number:
        // a) remove superflous track states
        while(editorState->trackStateList.size() > docRoot->trackList.size())
            editorState->trackStateList.removeLast();

        // b) add missing track states
        while(editorState->trackStateList.size() < docRoot->trackList.size())
        {
            EditorTrackState trackState;
            trackState.centerMidiNote=docRoot->trackList[editorState->trackStateList.size()]->centerNoteNumber();
            editorState->trackStateList.append(trackState);
        }
    }

    // After reading all events in all tracks, check for too small tick resolution
    adjustTickResolution();
    return true;
}

bool SmfImporter::importConductorTrack()
{
    // Initialize first measure item with default values for time signature, key signature, and tempo
    DocMeasureItem* measureItem=new DocMeasureItem;
    measureItem->setFirstMeasureItemDefaults();
    docRoot->measureItemList.append(measureItem);

    if(!importMainConfigXML())return false;
    if(!importTimeSignatures())return false;
    if(!importOtherConductorTrackMetaEvents())return false;
    return true;
}

bool SmfImporter::importMainConfigXML()
{
    // Initialize editor state to default settings.
    //  A valid configuration XML text meta event will override these settings.
    editorState->setStartupDefaultState(docRoot);

    // Scan for configuration XML text meta event in conductor track
    SmfTrack* conductorTrack=smfDocument->trackList[0];
    for(int i=0; i < conductorTrack->eventList.size(); ++i)
    {
        SmfMetaEvent* textMetaEvent=conductorTrack->eventList[i]->isMetaEventOfType(SMF_META_EVENT_TYPE_TEXT);
        if(!textMetaEvent)continue; // filter out irrelevant events

        QDomDocument domDoc;
        if(!domDoc.setContent(textMetaEvent->dataToString()))
            continue;  // parser error, no valid XML document

        /* XML format specification:

           <speedy_midi_config version="[int]">
               <editor_state>
                 ...
               </editor_state>
           </speedy_midi_config>

        */

        QDomElement configElement=domDoc.documentElement();
        if(configElement.tagName() != XML_TAG_SPEEDY_MIDI_CONFIG)continue;

        bool ok;
        xmlConfigVersion=configElement.attribute(XML_ATTR_VERSION).toInt(&ok);if(!ok)continue;

        if(xmlConfigVersion != XML_CONFIG_CURRENT_VERSION)continue;

        QDomElement editorStateElement=configElement.elementsByTagName(XML_TAG_EDITOR_STATE).item(0).toElement();
        if(editorStateElement.isNull())continue;

        if(editorState->loadFromXML(editorStateElement,xmlConfigVersion))
            foundEditorState=true;
        else
            continue;
    }
    return true;
}

bool SmfImporter::importTimeSignatures()
{
    // Scan for time signature meta events
    SmfTrack* conductorTrack=smfDocument->trackList[0];
    for(int i=0; i < conductorTrack->eventList.size(); ++i)
    {
        SmfMetaEvent* timeSignatureMetaEvent=
                conductorTrack->eventList[i]->isMetaEventOfType(SMF_META_EVENT_TYPE_TIME_SIGNATURE);
        if(!timeSignatureMetaEvent)continue; // filter out irrelevant events

        if(timeSignatureMetaEvent->dataLength < 4)continue; // Illegal time signature meta event

        int nominator           = (int)timeSignatureMetaEvent->data[0];
        int denominatorExponent = (int)timeSignatureMetaEvent->data[1];
        //UNUSED midiClocks     = (int)timeSignatureMetaEvent->data[2];
        //UNUSED _32ths         = (int)timeSignatureMetaEvent->data[3];

        // Range checks
        if(nominator < 1 || nominator > EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR)continue;
        if(denominatorExponent > EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR_EXP)continue;

        DocMeasureItem timeSignatureItem;
        timeSignatureItem.tickPosition=timeSignatureMetaEvent->tickPosition;
        timeSignatureItem.setTimeSignature=true;
        timeSignatureItem.timeSignatureNominator=nominator;
        timeSignatureItem.timeSignatureDenominator=1;
        for(int exponent=denominatorExponent; exponent > 0; --exponent)
            timeSignatureItem.timeSignatureDenominator*=2;

        setMeasureProperty(timeSignatureItem);
    }

    // All measure items existing so far set the time signature. Check if all items are on measure borders.
    DocMeasureItem effectiveMeasureProperties=docRoot->getFirstMeasureEffectiveProperties();

    for(int i=1; i < docRoot->measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=docRoot->measureItemList[i];
        if((measureItem->tickPosition - effectiveMeasureProperties.tickPosition) %
           docRoot->ticksPerMeasure(effectiveMeasureProperties) != 0)
        {
            // Item not on measure border.
            //  Recover from this error by discarding all items from here on, but accept the rest of the file.
            while(docRoot->measureItemList.size() > i) delete docRoot->measureItemList.takeLast();
            break;
        }

        effectiveMeasureProperties.makeEffectiveMeasureProperties(*measureItem);
    }
    return true;
}

bool SmfImporter::importOtherConductorTrackMetaEvents()
{
    int nextRehearsalMarkerColor=0;

    // Scan for other meta events
    SmfTrack* conductorTrack=smfDocument->trackList[0];
    for(int i=0; i < conductorTrack->eventList.size(); ++i)
    {
        SmfEvent* event=conductorTrack->eventList[i];

        SmfMidiEvent* midiEvent=event->isMidiEvent();
        if(midiEvent)
        {
            // Found a MIDI event in track 0 => rescan this track also as a normal track.
            mixedConductorAndMidiEventsTrack=true;
        }

        SmfMetaEvent* metaEvent=event->isMetaEvent();
        if(!metaEvent)continue; // filter out irrelevant events

        switch(metaEvent->metaEventType)
        {
        case SMF_META_EVENT_TYPE_MARKER:
            {
                // set rehearsal marker, assign default color
                DocMeasureItem rehearsalMarkerItem;
                rehearsalMarkerItem.tickPosition=metaEvent->tickPosition;
                rehearsalMarkerItem.setRehearsalMarker=true;
                rehearsalMarkerItem.rehearsalMarkerText=metaEvent->dataToString();
                rehearsalMarkerItem.rehearsalMarkerColor=DOCUMENT_MARKER_COLORS[nextRehearsalMarkerColor];

                setMeasureProperty(rehearsalMarkerItem);

                // cycle through available rehearsal marker colors
                ++nextRehearsalMarkerColor;
                if(nextRehearsalMarkerColor >= DOCUMENT_N_MARKER_COLORS)nextRehearsalMarkerColor=0;
            }
            break;
        case SMF_META_EVENT_TYPE_TEXT:
            {
                // Analyse event for futher measure item information (rehearsal marker color, swing, ...)
                QDomDocument domDoc;
                if(!domDoc.setContent(metaEvent->dataToString()))
                {
                    // No valid XML text event, store event in global meta event list instead.
                    docRoot->metaEventList.append(new SmfMetaEvent(*metaEvent));
                    break;
                }

                /* XML format specification:
                   <measure_item>
                       [optional] <rehearsal_marker color="#RRGGBB"/>
                       [optional] <playback_options swing="[int]">
                   </measure_item>
                */

                QDomElement rootElement=domDoc.documentElement();
                if(rootElement.tagName() != XML_TAG_MEASURE_ITEM)
                    continue;   // other XML text not handled here
                    
                // Delete meta event data to indicate it was handled
                metaEvent->dataLength=0;
                delete metaEvent->data;
                metaEvent->data=NULL;

                // Rehearsal marker element present?
                if(!rootElement.elementsByTagName(XML_TAG_REHEARSAL_MARKER).isEmpty())
                {
                    QDomElement rehearsalMarkerElement=rootElement.elementsByTagName(XML_TAG_REHEARSAL_MARKER).
                                                       item(0).toElement();

                    QString colorString=rehearsalMarkerElement.attribute(XML_ATTR_COLOR);
                    QColor color(colorString);
                    if(!color.isValid())continue;

                    int tickPosition=docRoot->roundUpTicksToMeasureBorder(metaEvent->tickPosition);
                    DocMeasureItem* measureItem=docRoot->getMeasureItemAtExact(tickPosition);

                    if(measureItem == NULL || !measureItem->setRehearsalMarker)continue;

                    // assign the color
                    measureItem->rehearsalMarkerColor=color;
                }
                // Playback options element present?
                if(!rootElement.elementsByTagName(XML_TAG_PLAYBACK_OPTIONS).isEmpty())
                {
                    QDomElement playbackOptionsElement=rootElement.elementsByTagName(XML_TAG_PLAYBACK_OPTIONS).
                                                       item(0).toElement();

                    bool ok;
                    int swingHardness=playbackOptionsElement.attribute(XML_ATTR_SWING).toInt(&ok);if(!ok)continue;
                    if(swingHardness != 0 && swingHardness < DOCUMENT_MIN_SWING_HARDNESS)continue;
                    if(swingHardness > DOCUMENT_MAX_SWING_HARDNESS)continue;

                    DocMeasureItem playbackOptionsItem;
                    playbackOptionsItem.tickPosition=metaEvent->tickPosition;
                    playbackOptionsItem.setPlaybackOptions=true;
                    playbackOptionsItem.swingHardness = swingHardness;

                    setMeasureProperty(playbackOptionsItem);
                }
            }
            break;
        case SMF_META_EVENT_TYPE_TEMPO: // set tempo
            {
                if(metaEvent->dataLength < 3)continue; // Illegal set tempo meta event

                int microSecondsPerQuarter=             // extract 24-bit big endian format
                        (((int)metaEvent->data[0]) << 16) +
                        (((int)metaEvent->data[1]) <<  8) +
                        (((int)metaEvent->data[2]));
                int microsecondsPerWholeNote=microSecondsPerQuarter * 4;

                // convert value into beats per second, where a beat is defined by timeSignatureDenominator
                int timeSignatureDenominator = docRoot->ticksToMeasure(metaEvent->tickPosition).
                                               measureProperties.timeSignatureDenominator;

                DocMeasureItem tempoItem;
                tempoItem.tickPosition=metaEvent->tickPosition;
                tempoItem.setTempo=true;

                // a minute consists of 60,000,000 microseconds.
                tempoItem.BPM = 60 * 1000000 * timeSignatureDenominator / microsecondsPerWholeNote;

                setMeasureProperty(tempoItem);
            }
            break;
        case SMF_META_EVENT_TYPE_KEY_SIGNATURE: // set key signature
            {
                if(metaEvent->dataLength < 2)continue; // Illegal key signature meta event

                DocMeasureItem keySignatureItem;
                keySignatureItem.tickPosition=metaEvent->tickPosition;
                keySignatureItem.setKeySignature=true;
                keySignatureItem.keySignature = (int) (signed char) metaEvent->data[0];
                keySignatureItem.keySignatureScale =
                        metaEvent->data[1] == 1 ? DocMeasureItem::KSS_Minor : DocMeasureItem::KSS_Major;

                setMeasureProperty(keySignatureItem);
            }
            break;
        case SMF_META_EVENT_TYPE_TIME_SIGNATURE:
            // already handled
            break;

            // all other events (even unknown types): copy to global meta event list
        case SMF_META_EVENT_TYPE_SEQ_NUMBER:
        case SMF_META_EVENT_TYPE_COPYRIGHT:
        case SMF_META_EVENT_TYPE_TRACK_NAME:
        case SMF_META_EVENT_TYPE_INSTRUMENT_NAME:
        case SMF_META_EVENT_TYPE_LYRICS:
        case SMF_META_EVENT_TYPE_CUE_POINT:
        case SMF_META_EVENT_TYPE_CHANNEL_PREFIX:
        case SMF_META_EVENT_TYPE_SMPTE_OFFSET:
        case SMF_META_EVENT_TYPE_SEQUENCER_SPECIFIC:
        default:
            docRoot->metaEventList.append(new SmfMetaEvent(*metaEvent));
            break;
        }
    }

    if(mixedConductorAndMidiEventsTrack)
    {
        // Delete all unparsed meta events, because the first track will be rescanned in importNormalTracks.
        // Otherwise the events would be duplicated.
        for(int i=0; i < docRoot->metaEventList.size(); ++i)
            delete docRoot->metaEventList[i];
        docRoot->metaEventList.clear();
    }

    return true;
}

void SmfImporter::setMeasureProperty(DocMeasureItem propertyItem)
{
    // All time signatures are imported before all other measure properties
    if(propertyItem.setTimeSignature)
    {
        //  As measures are defined by these time signatures, up to now the measures are
        //  not defined and thus no tick position checking can occur.
    }
    else
    {
        // Other changes must be on measure border. If not, round up to next measure border.
        //  This also applies to tempo settings in this editor,
        //  whereas in an SMF file, set-tempo commands can occur at any time.
        propertyItem.tickPosition=docRoot->roundUpTicksToMeasureBorder(propertyItem.tickPosition);
    }

    DocMeasureItem* measureItem=NULL;
    for(int i=0; i < docRoot->measureItemList.size(); ++i)  // items are sorted for tickPosition
    {
        measureItem=docRoot->measureItemList[i];
        if(measureItem->tickPosition == propertyItem.tickPosition)
        {
            // add properties to item
            measureItem->mergeMeasureItems(propertyItem);
            return;
        }
        if(measureItem->tickPosition >  propertyItem.tickPosition)
        {
            // insert new item
            measureItem=new DocMeasureItem(propertyItem);
            docRoot->measureItemList.insert(i,measureItem);
            return;
        }
    }
    // append new item
    measureItem=new DocMeasureItem(propertyItem);
    docRoot->measureItemList.append(measureItem);
}

bool SmfImporter::importNormalTracks()
{
    // create an editor track for each smf track, but not for a track that contains only conductor events

    int firstNormalTrackIndex=mixedConductorAndMidiEventsTrack ? 0 : 1;

    for(int smfTrackIndex=firstNormalTrackIndex;
        smfTrackIndex < smfDocument->trackList.size();
        ++smfTrackIndex)
    {
        SmfTrack* smfTrack=smfDocument->trackList[smfTrackIndex];

        DocTrack* track=new DocTrack;

        // Set default configuration, may be overwritten by configuration in XML text meta event
        track->eventColor=docRoot->getNewTrackEventColor();
        track->midiVolume=100;
        track->midiPatch=1;      // Grand Piano
        track->midiPanorama=MIDI_PANORAMA_CENTER;
        //track->midiChannel will be assigned in importTrackEvents

        docRoot->trackList.append(track);

        // If SMF contains a mixed track 0, this track must be filtered for non-conductor events only
        bool filterOutConductorEvents= mixedConductorAndMidiEventsTrack &&
                                       smfTrackIndex == firstNormalTrackIndex;

        if(!importTrackConfigXML(track,smfTrack))return false;
        if(!importTrackEvents(track,smfTrack,filterOutConductorEvents))return false;
    }
    return true;
}

bool SmfImporter::importTrackConfigXML(DocTrack* track, SmfTrack* smfTrack)
{
    for(int i=0; i < smfTrack->eventList.size(); ++i)
    {
        SmfMetaEvent* textMetaEvent=smfTrack->eventList[i]->isMetaEventOfType(SMF_META_EVENT_TYPE_TEXT);
        if(!textMetaEvent)continue; // filter out irrelevant events

        QDomDocument domDoc;
        if(!domDoc.setContent(textMetaEvent->dataToString()))
            continue;  // parser error, no valid XML document

        /* XML format specification:

           <track_config ...>
               ...
           </track_config>
        */

        QDomElement trackConfigElement=domDoc.documentElement();
        if(trackConfigElement.tagName() != XML_TAG_TRACK_CONFIG)continue;

        if(!track->loadXMLTrackConfig(trackConfigElement,xmlConfigVersion))continue;

        // Delete meta event data to indicate it was handled
        textMetaEvent->dataLength=0;
        delete textMetaEvent->data;
        textMetaEvent->data=NULL;
    }
    return true;
}

bool SmfImporter::importTrackEvents(DocTrack* track, SmfTrack* smfTrack, bool filterOutConductorEvents)
{
    bool foundTrackName=false;
    int channelPrefix=-1;
    int firstNoteChannel=-1;
    int patch=-1;
    int volume=-1;
    int panorama=-1;

    // 1st pass: find global information
    for(int i=0; i < smfTrack->eventList.size(); ++i)
    {
        SmfEvent* event=smfTrack->eventList[i];

        SmfMetaEvent* metaEvent=event->isMetaEvent();
        SmfMidiEvent* midiEvent=event->isMidiEvent();
        // (discard SysEx events)

        if(metaEvent)
        {
            if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_TRACK_NAME)
            {
                if(!foundTrackName) // use only first occurrence
                {
                    foundTrackName=true;
                    track->name=metaEvent->dataToString();
                }
            }
            if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_CHANNEL_PREFIX)
            {
                if(channelPrefix == -1) // use only first occurrence
                {
                    if(metaEvent->dataLength >= 1)
                        channelPrefix=metaEvent->data[0] + 1;
                }
            }
        }
        if(midiEvent)
        {
            quint8 command=midiEvent->midiCommand[0] & 0xf0;

            if(firstNoteChannel == -1 && command == 0x90)
            {
                // Remember channel of first note-on event.
                //  Will be used if no channel prefix meta event occurs for this track
                firstNoteChannel=(int)(midiEvent->midiCommand[0] & 0xf) + 1;
            }
            else if(patch == -1 && command == 0xc0)
            {
                // Remember first patch change command.
                patch=midiEvent->midiCommand[1] + 1;
            }
            else if(volume == -1 && command == 0xb0 && midiEvent->midiCommand[1] == 0x07)
            {
                // Remember first volume control command (controller number 0x07)
                volume=midiEvent->midiCommand[2];
            }
            else if(panorama == -1 && command == 0xb0 && midiEvent->midiCommand[1] == 0x0a)
            {
                // Remember first panorama control command (controller number 0x0a)
                panorama=midiEvent->midiCommand[2];
            }
        }
    }

    // Assign patch if there was a patch change command
    if(patch != -1)track->midiPatch=patch;

    // Assign volume if there was a volume control command
    if(volume != -1)track->midiVolume=volume;

    // Assign panorama if there was a panorama control command
    if(panorama != -1)track->midiPanorama=panorama;

    // Assign channel prefix if there was a channel prefix meta event.
    //  If not, use channel of first note-on event.
    //  If no such event is found, assign channel least used so far.
    if(channelPrefix == -1)
    {
        // Use first note-on event channel. If no such channel exists, use the channel least used so far.
        if(firstNoteChannel != -1)
            channelPrefix=firstNoteChannel;
        else
            channelPrefix=docRoot->getNewTrackChannel();
    }

    track->midiChannel=channelPrefix;

    // 2nd pass: process all events

    int lastEventTickPosition=-1;

    DocEvent::OtherMidiEvent::SameTickSubOrderType sameTickSubOrdering;
    sameTickSubOrdering.beforeNoteEvents=false;  // invalidate member
    sameTickSubOrdering.index=-1;                // invalidate member

    for(int i=0; i < smfTrack->eventList.size(); ++i)
    {
        SmfEvent* event=smfTrack->eventList[i];

        if((int)event->tickPosition != lastEventTickPosition)
        {
            // restart same-tick-subordering of "other MIDI events"
            lastEventTickPosition=(int)event->tickPosition;
            sameTickSubOrdering.beforeNoteEvents=true;
            sameTickSubOrdering.index=0;
        }

        SmfMetaEvent* metaEvent=event->isMetaEvent();
        SmfMidiEvent* midiEvent=event->isMidiEvent();
        // (discard SysEx events)

        if(metaEvent)
        {
            switch(metaEvent->metaEventType)
            {
            case SMF_META_EVENT_TYPE_TEMPO:
            case SMF_META_EVENT_TYPE_TIME_SIGNATURE:
            case SMF_META_EVENT_TYPE_KEY_SIGNATURE:
                // no conductor track events allowed in normal track
                break;

            case SMF_META_EVENT_TYPE_SEQ_NUMBER:
            case SMF_META_EVENT_TYPE_COPYRIGHT:
            case SMF_META_EVENT_TYPE_INSTRUMENT_NAME:
                // not shiftable
                track->metaEventList.append(new SmfMetaEvent(*metaEvent));
                break;

            case SMF_META_EVENT_TYPE_TRACK_NAME:
            case SMF_META_EVENT_TYPE_CHANNEL_PREFIX:
                // first occurrences were already handled explicitly, duplicates are removed
                break;

            default:
                {
                    if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_MARKER)
                    {
                        if(filterOutConductorEvents)
                            break;    // filter any conductor event
                    }
                    if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_TEXT)
                    {
                        if(metaEvent->dataLength == 0)
                            break;    // Already handled XML data

                        // else: No XML-Test => shiftable meta event
                    }

                    // all other events are shiftable
                    DocEvent* docMetaEvent=new DocEvent;
                    docMetaEvent->type=DocEvent::E_Meta;
                    docMetaEvent->tickPosition=(int)metaEvent->tickPosition;
                    docMetaEvent->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
                    docMetaEvent->metaEventData.metaEvent=new SmfMetaEvent(*metaEvent);

                    // Invalidate tick position, unused during encapsulation in DocEvent. Updated when saved.
                    docMetaEvent->metaEventData.metaEvent->tickPosition=0xffffffff;

                    track->insertEvent(docMetaEvent);
                }
                break;
            }
        }
        if(midiEvent)
        {
            quint8 command=midiEvent->midiCommand[0];

            switch(command & 0xf0)
            {
            case 0x80:  // Note-off: used only in connection with a corresponding note-on event
                break;
            case 0x90:  // Note-on
                {
                    int noteNumber = midiEvent->midiCommand[1];
                    int velocity   = midiEvent->midiCommand[2];

                    // Zero velocity is equivalent to note-off
                    if(velocity == 0)break;

                    // Search for corresponding note-off event, or note-on event with velocity zero
                    int tickPositionOff=-1;
                    for(int j=i+1; j < smfTrack->eventList.size(); ++j)
                    {
                        SmfMidiEvent* event2=smfTrack->eventList[j]->isMidiEvent();
                        if(!event2)continue; // filter out irrelevant events

                        int command2    = event2->midiCommand[0];
                        int noteNumber2 = event2->midiCommand[1];

                        bool noteOff=(command2 & 0xf0) == 0x80;
                        bool noteOnZeroVelocity=(command2 & 0xf0) == 0x90 && event2->midiCommand[2] == 0;

                        if((noteOff || noteOnZeroVelocity) && noteNumber2 == noteNumber)
                        {
                            // Corresponding event found. Remember tick position.
                            tickPositionOff=(int)event2->tickPosition;
                            break;
                        }
                    }

                    // Discard the event if there was no corresponding event switching the note off
                    if(tickPositionOff == -1)break;

                    // Minimum event length is 1.
                    if(tickPositionOff == (int)event->tickPosition)
                        ++tickPositionOff;

                    DocEvent* docNoteEvent=new DocEvent;
                    docNoteEvent->type=DocEvent::E_Note;
                    docNoteEvent->tickPosition=(int)midiEvent->tickPosition;
                    docNoteEvent->tickLength=tickPositionOff - docNoteEvent->tickPosition;
                    docNoteEvent->noteEventData.noteNumber=noteNumber;
                    docNoteEvent->noteEventData.velocity=velocity;
                    track->insertEvent(docNoteEvent);

                    // Note event occurred, remember this for same-tick-subordering
                    sameTickSubOrdering.beforeNoteEvents=false;
                }
                break;
            default:    // all other: store as shiftable other MIDI event
                {
                    // On tick position zero, skip "program change", "set volume", and "set panorama".
                    //  They were handled in pass 1.
                    if(midiEvent->tickPosition == 0)
                    {
                        int command=midiEvent->midiCommand[0] & 0xf0;

                        // program change
                        if(command == 0xc0)break;

                        // set controller 0x07: volume
                        if(command == 0xb0 && midiEvent->midiCommand[1] == 0x07)break;

                        // set controller 0x0a: panorama
                        if(command == 0xb0 && midiEvent->midiCommand[1] == 0x0a)break;
                    }

                    DocEvent* docOtherMidiEvent=new DocEvent;
                    docOtherMidiEvent->type=DocEvent::E_OtherMidi;
                    docOtherMidiEvent->tickPosition=(int)midiEvent->tickPosition;
                    docOtherMidiEvent->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
                    docOtherMidiEvent->otherMidiEventData.midiCommand[0]=midiEvent->midiCommand[0];
                    docOtherMidiEvent->otherMidiEventData.midiCommand[1]=midiEvent->midiCommand[1];
                    docOtherMidiEvent->otherMidiEventData.midiCommand[2]=midiEvent->midiCommand[2];
                    docOtherMidiEvent->otherMidiEventData.sameTickSubOrdering=sameTickSubOrdering;
                    track->insertEvent(docOtherMidiEvent);

                    ++sameTickSubOrdering.index;
                }
                break;
            }
        }
    }
    return true;
}

void SmfImporter::adjustTickResolution()
{
    // Check for too small tick resolution
    if(docRoot->midiTicksPerWholeNote >= DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE)
        return;  // Resolution is sufficient

    docRoot->scaleTickResolution(DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE);

    // reset editor state to new resolution cell boundaries
    editorState->setStartupDefaultState(docRoot);
}
