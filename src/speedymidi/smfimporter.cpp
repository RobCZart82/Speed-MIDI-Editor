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
#include <QQueue>
#include <QVector>
#include <QCoreApplication>
#include <algorithm>
#include <limits>

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
    importError.clear();
    // Normalize source timestamps before any measure arithmetic. A low PPQN
    // can otherwise make a valid 1/32 measure zero ticks long.
    const int sourceResolution=smfDocument->getMidiTicksPerWholeNote();
    if(sourceResolution <= 0)return false;
    const int resolution=qMax(sourceResolution,DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE);
    const bool scaled=resolution != sourceResolution;
    // Leave space for rounding a conductor event up to the next measure and
    // for synthesizing a minimum-length note at the end of the document.
    const qint64 maxTick=std::numeric_limits<int>::max() -
            qint64(resolution) * EDITOR_MAX_TIME_SIGNATURE_DENOMINATOR;
    for(const SmfTrack* track : smfDocument->trackList)
    {
        if(qint64(track->endTick) * resolution / sourceResolution > maxTick)return false;
        for(const SmfEvent* event : track->eventList)
            if(qint64(event->tickPosition) * resolution / sourceResolution > maxTick)
                return false;
    }
    if(scaled)
    {
        for(SmfTrack* track : smfDocument->trackList)
        {
            track->endTick=quint32(qint64(track->endTick) * resolution / sourceResolution);
            for(SmfEvent* event : track->eventList)
                event->tickPosition=quint32(qint64(event->tickPosition) * resolution / sourceResolution);
        }
        smfDocument->setMidiTicksPerWholeNote(resolution);
    }
    if(smfDocument->getFormatTag() == 0 || smfDocument->hasMixedChannelsInTrack())
        smfDocument->convertToFormat1(true);

    // Set resolution obtained from SMF. Resolution may be adjusted when document has been loaded successfully.
    docRoot->midiTicksPerWholeNote=smfDocument->getMidiTicksPerWholeNote();

    if(!importConductorTrack())return false;
    if(!importNormalTracks())return false;

    // If no editor state was saved within the SMF, set a default startup state
    if(!foundEditorState || scaled)
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
    // The physical default tempo is independent of the time-signature beat.
    measureItem->BPM=measureItem->tempoBPM(measureItem->timeSignatureDenominator);
    if(!importTempos())return false;
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
        // Compatible exports deliberately omit editor_state. Their recognized
        // config still belongs to us and must not accumulate on each save.
        if(!editorStateElement.isNull()) {
            if(!editorState->loadFromXML(editorStateElement,xmlConfigVersion))continue;
            foundEditorState=true;
        }
        textMetaEvent->dataLength=0;
        delete[] textMetaEvent->data;
        textMetaEvent->data=nullptr;
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

        if(qint64(docRoot->midiTicksPerWholeNote) * nominator %
           timeSignatureItem.timeSignatureDenominator != 0)
        {
            importError=QCoreApplication::translate("SmfImporter",
                "This file contains a time signature with a fractional-tick measure length. The editor cannot represent it safely. The file has not been changed.");
            return false;
        }

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
            // Do not accept a partial timeline: saving it would erase this
            // meter and every later meter from the user's file.
            importError=QCoreApplication::translate("SmfImporter",
                "This file contains a time signature change inside a measure. The editor cannot represent it safely. The file has not been changed.");
            return false;
        }

        effectiveMeasureProperties.makeEffectiveMeasureProperties(*measureItem);
    }
    return true;
}

bool SmfImporter::importTempos()
{
    // Tempo applies globally even when a format-1 file places it outside the
    // conductor. Flatten tracks in source order, then stable-sort by tick so
    // equal-tick tempos retain their source track/event order.
    QList<const SmfMetaEvent*> tempoEvents;
    for(const SmfTrack* track : smfDocument->trackList)
        for(const SmfEvent* event : track->eventList)
            if(const SmfMetaEvent* tempo=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TEMPO))
                if(tempo->dataLength >= 3)tempoEvents.append(tempo);
    std::stable_sort(tempoEvents.begin(),tempoEvents.end(),
                    [](const SmfMetaEvent* left,const SmfMetaEvent* right) {
                        return left->tickPosition < right->tickPosition;
                    });

    int previousTempoTick=-1;
    for(const SmfMetaEvent* event : tempoEvents)
    {
        const int microseconds=(int(event->data[0]) << 16) |
                               (int(event->data[1]) << 8) | int(event->data[2]);
        if(microseconds == 0)continue;

        const int tick=int(event->tickPosition);
        const int denominator=docRoot->ticksToMeasure(tick).
                              measureProperties.timeSignatureDenominator;
        DocMeasureItem tempoItem;
        tempoItem.tickPosition=tick;
        tempoItem.setTempo=true;
        tempoItem.microsecondsPerQuarter=microseconds;
        // BPM is a display cache only. Exact SMF timing is retained above,
        // including valid values outside the editor's integer BPM range.
        tempoItem.BPM=qMax(1,int(tempoItem.tempoBPM(denominator)));

        DocMeasureItem* existing=docRoot->getMeasureItemAtExact(tick);
        if(existing && previousTempoTick == tick)
            existing->mergeMeasureItemsPreservingTempo(tempoItem,denominator);
        else
            setMeasureProperty(tempoItem);
        previousTempoTick=tick;
    }
    return true;
}

bool SmfImporter::importOtherConductorTrackMetaEvents()
{
    int nextRehearsalMarkerColor=0;

    // Scan for other meta events
    SmfTrack* conductorTrack=smfDocument->trackList[0];
    docRoot->conductorEndTick=int(conductorTrack->endTick);
    for(int i=0; i < conductorTrack->eventList.size(); ++i)
    {
        SmfEvent* event=conductorTrack->eventList[i];
        if(SmfSysExEvent* sysEx=event->isSysExEvent())
            docRoot->sysExEventList.append(new SmfSysExEvent(*sysEx));

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
                if(metaEvent->dataLength == 0)break; // configuration already consumed
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
                if(rootElement.tagName() != XML_TAG_MEASURE_ITEM) {
                    docRoot->metaEventList.append(new SmfMetaEvent(*metaEvent));
                    break;
                }
                    
                // Delete meta event data to indicate it was handled
                metaEvent->dataLength=0;
                delete[] metaEvent->data;
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
        case SMF_META_EVENT_TYPE_TEMPO:
            // Imported from every source track by importTempos().
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
    if(propertyItem.setTimeSignature || propertyItem.setTempo)
    {
        // Meter changes define the grid. Tempo changes retain their exact
        // absolute positions and need not fall on measure borders.
    }
    else
    {
        // Other editor attributes currently apply at measure borders.
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
        delete[] textMetaEvent->data;
        textMetaEvent->data=NULL;
    }
    return true;
}

bool SmfImporter::importTrackEvents(DocTrack* track, SmfTrack* smfTrack, bool filterOutConductorEvents)
{
    bool foundTrackName=false;
    int channelPrefix=-1;
    int firstNoteChannel=-1;
    int firstVoiceChannel=-1;
    int patch=-1;
    int volume=-1;
    int panorama=-1;
    bool beforeFirstNote=true;
    bool hasInitialBankSelect=false;
    bool hasInitialSysEx=false;
    if(!filterOutConductorEvents)
        for(const SmfEvent* event : smfTrack->eventList)
            if(event->tickPosition == 0 && event->isSysExEvent())hasInitialSysEx=true;

    // 1st pass: find global information
    for(int i=0; i < smfTrack->eventList.size(); ++i)
    {
        SmfEvent* event=smfTrack->eventList[i];

        SmfMetaEvent* metaEvent=event->isMetaEvent();
        SmfMidiEvent* midiEvent=event->isMidiEvent();
        // Global configuration is inspected here; packets are copied in pass 2.

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
                    if(metaEvent->dataLength == 1 && metaEvent->data[0] < MIDI_MAX_CHANNEL)
                        channelPrefix=metaEvent->data[0] + 1;
                }
            }
        }
        if(midiEvent)
        {
            quint8 command=midiEvent->midiCommand[0] & 0xf0;
            if(firstVoiceChannel == -1 && command >= 0x80 && command < 0xf0)
                firstVoiceChannel=(midiEvent->midiCommand[0] & 0xf) + 1;
            if(midiEvent->tickPosition == 0 && command == 0xb0 &&
               (midiEvent->midiCommand[1] == 0 || midiEvent->midiCommand[1] == 32))
                hasInitialBankSelect=true;

            if(firstNoteChannel == -1 && command == 0x90)
            {
                // Remember channel of first note-on event.
                //  Will be used if no channel prefix meta event occurs for this track
                firstNoteChannel=(int)(midiEvent->midiCommand[0] & 0xf) + 1;
            }
            if(command == 0x90 && midiEvent->midiCommand[2] != 0)beforeFirstNote=false;
            if(midiEvent->tickPosition != 0 || !beforeFirstNote)continue;
            if(command == 0xc0)
            {
                // Last initial patch before the first note wins.
                patch=midiEvent->midiCommand[1] + 1;
            }
            else if(command == 0xb0 && midiEvent->midiCommand[1] == 0x07)
            {
                // Remember initial volume control command (controller number 0x07)
                volume=midiEvent->midiCommand[2];
            }
            else if(command == 0xb0 && midiEvent->midiCommand[1] == 0x0a)
            {
                // Remember initial panorama control command (controller number 0x0a)
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

    // Explicit channel voice messages carry their own channel. Prefer the
    // first note-on channel for this editor track; use channel-prefix only for
    // tracks without notes, then fall back to the least-used channel.
    if(firstNoteChannel != -1)
        channelPrefix=firstNoteChannel;
    else if(firstVoiceChannel != -1)
        channelPrefix=firstVoiceChannel;
    else if(channelPrefix == -1)
        channelPrefix=docRoot->getNewTrackChannel();

    track->midiChannel=channelPrefix;

    // 2nd pass: process all events

    int lastEventTickPosition=-1;

    DocEvent::OtherMidiEvent::SameTickSubOrderType sameTickSubOrdering;
    sameTickSubOrdering.beforeNoteEvents=false;  // invalidate member
    sameTickSubOrdering.index=-1;                // invalidate member

    // Pair each note-on with one note-off of the same channel and pitch.
    // A FIFO per key prevents overlapping repeated notes from reusing the
    // same note-off event. Channel is part of the key because MIDI note
    // messages on different channels are independent.
    QVector<int> matchedNoteOffTicks(smfTrack->eventList.size(),-1);
    QVector<int> matchedNoteOffIndices(smfTrack->eventList.size(),-1);
    QVector<QQueue<int>> pendingNoteOns(MIDI_MAX_CHANNEL * MIDI_N_NOTE_NUMBERS);
    for(int eventIndex=0; eventIndex < smfTrack->eventList.size(); ++eventIndex)
    {
        SmfMidiEvent* midiEvent=smfTrack->eventList[eventIndex]->isMidiEvent();
        if(!midiEvent)continue;

        const int commandFamily=midiEvent->midiCommand[0] & 0xf0;
        if(commandFamily != 0x80 && commandFamily != 0x90)continue;

        const int channel=midiEvent->midiCommand[0] & 0x0f;
        const int noteNumber=midiEvent->midiCommand[1];
        const int keyIndex=channel * MIDI_N_NOTE_NUMBERS + noteNumber;
        const bool noteOn=commandFamily == 0x90 && midiEvent->midiCommand[2] != 0;

        if(noteOn)
        {
            pendingNoteOns[keyIndex].enqueue(eventIndex);
        }
        else if(!pendingNoteOns[keyIndex].isEmpty())
        {
            const int noteOnIndex=pendingNoteOns[keyIndex].dequeue();
            matchedNoteOffTicks[noteOnIndex]=static_cast<int>(midiEvent->tickPosition);
            matchedNoteOffIndices[noteOnIndex]=eventIndex;
        }
    }

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
        if(SmfSysExEvent* sysEx=event->isSysExEvent())
        {
            // Mixed conductor tracks have already copied their global packets.
            if(!filterOutConductorEvents)
            {
                DocEvent* packet=new DocEvent;
                packet->type=DocEvent::E_SysEx;
                packet->tickPosition=int(event->tickPosition);
                packet->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
                packet->sysExEventData.sysExEvent=new SmfSysExEvent(*sysEx);
                packet->sysExEventData.sysExEvent->tickPosition=0xffffffff;
                track->insertEvent(packet);
            }
        }

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

                    int tickPositionOff=matchedNoteOffTicks[i];

                    // Recover an unmatched note-on by sustaining it to the
                    // original track endpoint instead of silently losing it.
                    if(tickPositionOff == -1)
                        tickPositionOff=qMax(int(smfTrack->endTick),int(event->tickPosition) + 1);

                    // Minimum event length is 1.
                    if(tickPositionOff == (int)event->tickPosition)
                        ++tickPositionOff;

                    DocEvent* docNoteEvent=new DocEvent;
                    docNoteEvent->type=DocEvent::E_Note;
                    docNoteEvent->tickPosition=(int)midiEvent->tickPosition;
                    docNoteEvent->tickLength=tickPositionOff - docNoteEvent->tickPosition;
                    docNoteEvent->noteEventData.noteNumber=noteNumber;
                    docNoteEvent->noteEventData.velocity=velocity;
                    docNoteEvent->noteEventData.importOnOrder=i;
                    docNoteEvent->noteEventData.importOffOrder=matchedNoteOffIndices[i];
                    if(matchedNoteOffIndices[i] >= 0)
                    {
                        const SmfMidiEvent* release=smfTrack->eventList[matchedNoteOffIndices[i]]->isMidiEvent();
                        // A zero-velocity NoteOn represents a release with the
                        // conventional velocity 64. Explicit NoteOff velocity
                        // is meaningful and must remain exact, including zero.
                        docNoteEvent->noteEventData.releaseVelocity=(release->midiCommand[0] & 0xf0) == 0x90 ?
                                    64 : release->midiCommand[2];
                    }
                    track->insertEvent(docNoteEvent);

                    // Note event occurred, remember this for same-tick-subordering
                    sameTickSubOrdering.beforeNoteEvents=false;
                }
                break;
            default:    // all other: store as shiftable other MIDI event
                {
                    // On tick position zero, skip "program change", "set volume", and "set panorama".
                    //  They were handled in pass 1.
                    // Opaque SysEx may reset patch/controllers. Keep initial
                    // setup in source order rather than moving it before that packet.
                    if(!hasInitialSysEx && midiEvent->tickPosition == 0 && sameTickSubOrdering.beforeNoteEvents)
                    {
                        int command=midiEvent->midiCommand[0] & 0xf0;

                        // program change
                        if(command == 0xc0 && !hasInitialBankSelect)break;

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
                    docOtherMidiEvent->otherMidiEventData.importOrder=i;
                    track->insertEvent(docOtherMidiEvent);

                    ++sameTickSubOrdering.index;
                }
                break;
            }
        }
    }
    int contentEnd=0;
    for(DocEvent* event=track->firstEvent; event; event=event->nextEvent)
        contentEnd=qMax(contentEnd,event->type == DocEvent::E_Note ?
                        event->tickPositionEnd() : event->tickPosition);
    if(int(smfTrack->endTick) > contentEnd)
    {
        // An endpoint is represented by its last occupied tick, so copying
        // [0,endTick) includes the duration marker without extending the song.
        DocEvent* endpoint=new DocEvent;
        endpoint->type=DocEvent::E_Meta;
        endpoint->tickPosition=int(smfTrack->endTick) - 1;
        endpoint->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
        endpoint->metaEventData.metaEvent=new SmfMetaEvent;
        endpoint->metaEventData.metaEvent->metaEventType=SMF_META_EVENT_TYPE_END_OF_TRACK;
        endpoint->metaEventData.metaEvent->tickPosition=0xffffffff;
        track->insertEvent(endpoint);
    }
    return true;
}
