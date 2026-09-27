/***************************************************************************
 *  doc_root.cpp - Model class: Model Root
 *                 (cf. Model–View–Controller Architecture)
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

#include "doc_root.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"

#include "smfdocument.h"
#include "smfimporter.h"
#include "smfexporter.h"

#define DOCUMENT_AUTO_PANORAMA_MAX_SHIFT   (MIDI_PANORAMA_CENTER-20)  // auto panorama between 20 and 127-20

struct TrackWizardTrackType
{
    QChar code;
    const char* trackName;
    int midiPatch;      // range [1;128]
    int baseMidiNote;
    int heightInNotes;
    bool assignPercussionChannel;
};

const TrackWizardTrackType DOCUMENT_TRACK_WIZARD_TRACK_TYPES[]=
{
    // Vocals

    { 's', QT_TRANSLATE_NOOP("DocRoot","Soprano" ), 54 /* Voice Oohs */, 5*12  , 2*12 /* c'-c''' */, false },
    { 'm', QT_TRANSLATE_NOOP("DocRoot","Mezzo"   ), 54 /* Voice Oohs */, 4*12+9, 2*12 /* a -a''  */, false },
    { 'a', QT_TRANSLATE_NOOP("DocRoot","Alto"    ), 54 /* Voice Oohs */, 4*12+5, 2*12 /* f -f''  */, false },
    { 't', QT_TRANSLATE_NOOP("DocRoot","Tenor"   ), 54 /* Voice Oohs */, 4*12  , 2*12 /* c -c''  */, false },
    { 'r', QT_TRANSLATE_NOOP("DocRoot","Baritone"), 54 /* Voice Oohs */, 3*12+7, 2*12 /* G -g'   */, false },
    { 'b', QT_TRANSLATE_NOOP("DocRoot","Bass"    ), 54 /* Voice Oohs */, 3*12+2, 2*12 /* D -d'   */, false },

    // String ensemble

    { 'v', QT_TRANSLATE_NOOP("DocRoot","Violin"     ), 49 /* Str.Ens.1 */, 4*12+7, 2*12+9 /* g -e''' */, false },
    { 'V', QT_TRANSLATE_NOOP("DocRoot","Viola"      ), 49 /* Str.Ens.1 */, 4*12  , 2*12+4 /* c -e''  */, false },
    { 'c', QT_TRANSLATE_NOOP("DocRoot","Cello"      ), 49 /* Str.Ens.1 */, 3*12  , 2*12+4 /* C -e'   */, false },
    { 'd', QT_TRANSLATE_NOOP("DocRoot","Double bass"), 49 /* Str.Ens.1 */, 2*12+4, 2*12+3 /* ,E-g    */, false },

    // Other
    { 'p', QT_TRANSLATE_NOOP("DocRoot","Piano"   ), 1  /* Gr. Piano    */, 5*12, 4*12 /* C -c''' */, false },
    { 'u', QT_TRANSLATE_NOOP("DocRoot","Drums"   ), 1  /* Std. Drumset */, 3*12, 3*12 /* C3-C6   */, true },
};
const int DOCUMENT_TRACK_WIZARD_N_TRACK_TYPES=
        sizeof(DOCUMENT_TRACK_WIZARD_TRACK_TYPES) / sizeof(DOCUMENT_TRACK_WIZARD_TRACK_TYPES[0]);

DocRoot::DocRoot()
{
    unmodifiedDefaultDocument=false;
    midiTicksPerWholeNote=-1;
}

void DocRoot::makeDeepCopy(const DocRoot& rhs)
{
    // deep copy constructor for "save compatible file"

    unmodifiedDefaultDocument   =   rhs.unmodifiedDefaultDocument;
    midiTicksPerWholeNote       =   rhs.midiTicksPerWholeNote;

    // copy subobjects from rhs
    for(int i=0; i < rhs.measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItemCopy=new DocMeasureItem;
        measureItemCopy->makeDeepCopy(*rhs.measureItemList[i]);
        measureItemList.append(measureItemCopy);
    }
    for(int i=0; i < rhs.trackList.size(); ++i)
    {
        DocTrack* trackCopy=new DocTrack;
        trackCopy->makeDeepCopy(*rhs.trackList[i]);
        trackList.append(trackCopy);
    }
    for(int i=0; i < rhs.metaEventList.size(); ++i)
        metaEventList.append(new SmfMetaEvent(*rhs.metaEventList[i]));
}

DocRoot::~DocRoot()
{
    // Delete subobjects

    for(int i=0; i < measureItemList.size(); ++i)delete measureItemList[i];
    measureItemList.clear();

    for(int i=0; i < trackList.size(); ++i)delete trackList[i];
    trackList.clear();

    for(int i=0; i < metaEventList.size(); ++i)
        delete metaEventList[i];
    metaEventList.clear();
}

DocMeasureItem* DocRoot::getMeasureItemAtExact(int ticks) const
{
    for(int i=0; i < measureItemList.size(); ++i)   // items are sorted for tickPosition
    {
        if(measureItemList[i]->tickPosition == ticks)return measureItemList[i];
        if(measureItemList[i]->tickPosition > ticks)return NULL;
    }
    return NULL;    // return NULL if no such item exists
}

DocMeasureItem DocRoot::getFirstMeasureEffectiveProperties() const
{
    Q_ASSERT(!measureItemList.isEmpty());
    Q_ASSERT(measureItemList.first()->tickPosition == 0);

    DocMeasureItem firstMeasureEffectiveProperties;
    firstMeasureEffectiveProperties.setFirstMeasureDefaultProperties();
    firstMeasureEffectiveProperties.makeEffectiveMeasureProperties(*measureItemList[0]);

    Q_ASSERT(firstMeasureEffectiveProperties.isValidFirstMeasureItem());

    return firstMeasureEffectiveProperties;
}

TicksToMeasureResult DocRoot::ticksToMeasure(int ticks) const
{
    TicksToMeasureResult result;
    result.measureIndex=0;
    result.measureInternalTicks=ticks;

    // Collect information set in all previous measure items:
    // 1. Start at first measure
    DocMeasureItem effectiveMeasureProperties=getFirstMeasureEffectiveProperties();

    // 2. Traverse measure item array and calculate all required data for result structure
    for(int i=1; i < measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=measureItemList[i];
        if(measureItem->tickPosition > ticks)break;  // Not applicable any more

        int measureItemIntervalTicks = measureItem->tickPosition - effectiveMeasureProperties.tickPosition;

        // new measure item must be on measure border
        Q_ASSERT(measureItemIntervalTicks % ticksPerMeasure(effectiveMeasureProperties) == 0);

		// refine tick and measure index approximation
        result.measureInternalTicks -= measureItemIntervalTicks;
        result.measureIndex += measureItemIntervalTicks / ticksPerMeasure(effectiveMeasureProperties);

        // accumulate new measure item data
        effectiveMeasureProperties.makeEffectiveMeasureProperties(*measureItem);
    }

    // Calculate measure count and internal ticks in measure after last applicable measure item
    int additionalMeasures=result.measureInternalTicks / ticksPerMeasure(effectiveMeasureProperties);
    result.measureInternalTicks -= additionalMeasures * ticksPerMeasure(effectiveMeasureProperties);
    result.measureIndex += additionalMeasures;

    // Prepare information about current measure settings.
    //  If there is actually a measure item at "ticks", the flags show what properties are set by that item.
    //  If there is no measure item at "ticks", the item settings will have no set-flags set,
    //   but the measure properties information and tickPosition will be correct.
    result.measureProperties=effectiveMeasureProperties;
    result.measureProperties.tickPosition=ticks;

    if(additionalMeasures > 0)  // There is no measure item at "ticks", so remove all set-flags
        result.measureProperties.resetSetFlags();

    return result;
}

int DocRoot::measureToTicks(int measureIndex) const
{
    Q_ASSERT(measureIndex >= 0);

    // Collect information set in all previous measure items
    DocMeasureItem effectiveMeasureProperties=getFirstMeasureEffectiveProperties();

    // Calculate first approximation
    int remainingMeasures=measureIndex;
    int ticks=remainingMeasures * ticksPerMeasure(effectiveMeasureProperties);

    // Traverse measure item array and calculate all required data for result structure
    for(int i=1; i < measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=measureItemList[i];
        if(measureItem->tickPosition > ticks)break;  // Not applicable any more

        // Calculate how many measures we can skip.
        int measureItemIntervalTicks = measureItem->tickPosition - effectiveMeasureProperties.tickPosition;
        remainingMeasures -= measureItemIntervalTicks / ticksPerMeasure(effectiveMeasureProperties);

        // new measure item must be on measure border
        Q_ASSERT(measureItemIntervalTicks % ticksPerMeasure(effectiveMeasureProperties) == 0);

        // New measure item must be taken into account. Accumulate new measure item data.
        effectiveMeasureProperties.makeEffectiveMeasureProperties(*measureItem);

        // Calculate new approximation
        ticks = measureItem->tickPosition + remainingMeasures * ticksPerMeasure(effectiveMeasureProperties);
    }

    return ticks;
}

int DocRoot::ticksPerBeat(const DocMeasureItem& measureProperties) const
{
    return midiTicksPerWholeNote / measureProperties.timeSignatureDenominator;
}

int DocRoot::ticksPerMeasure(const DocMeasureItem& measureProperties) const
{
    return measureProperties.timeSignatureNominator * ticksPerBeat(measureProperties);
}

int DocRoot::getNextCellMeasureInternalTickPosition(int measureInternalCellIndex, const WriteLength& writeLength) const
{
    // Calculate cell tick position borders, rounded to integer tick precision.
    //  BEWARE: This function does NOT care about shrinked cells! It may return a tick count > ticksPerMeasure!
    return (measureInternalCellIndex + 1)
            * writeLength.tupletNominator
            * midiTicksPerWholeNote
            / writeLength.denominator
            / writeLength.tupletDenominator;
}

int DocRoot::roundDownTicksToCellBorder(int ticks, const WriteLength& writeLength) const
{
    // If currently on a cell border, then return unmodified value. Otherwise round down.
    Q_ASSERT(ticks >= 0);
    TicksToMeasureResult r=ticksToMeasure(ticks);

    int measureInternalCellIndex=0;
    int measureInternalCellTickPosition=0;

    while(true)
    {
        int nextCellMeasureInternalTickPosition=
                getNextCellMeasureInternalTickPosition(measureInternalCellIndex,writeLength);

        // Last cell may be shrinked
        if(nextCellMeasureInternalTickPosition > ticksPerMeasure(r.measureProperties))
            nextCellMeasureInternalTickPosition=ticksPerMeasure(r.measureProperties);

        if(r.measureInternalTicks >= measureInternalCellTickPosition &&
           r.measureInternalTicks < nextCellMeasureInternalTickPosition)
        {
            // Found correct cell
            return ticks - r.measureInternalTicks + measureInternalCellTickPosition;
        }

        // Advance to next cell
        ++measureInternalCellIndex;
        measureInternalCellTickPosition=nextCellMeasureInternalTickPosition;
    }
}

int DocRoot::roundUpTicksToCellBorder(int ticks, const WriteLength& writeLength) const
{
    // If currently on a cell border, then return unmodified value. Otherwise round up.
    Q_ASSERT(ticks >= 0);
    TicksToMeasureResult r=ticksToMeasure(ticks);

    if(r.measureInternalTicks == 0)return ticks;

    int measureInternalCellIndex=0;
    int measureInternalCellTickPosition=0;

    while(true)
    {
        int nextCellMeasureInternalTickPosition=
                getNextCellMeasureInternalTickPosition(measureInternalCellIndex,writeLength);

        // Last cell may be shrinked
        if(nextCellMeasureInternalTickPosition > ticksPerMeasure(r.measureProperties))
            nextCellMeasureInternalTickPosition=ticksPerMeasure(r.measureProperties);

        if(r.measureInternalTicks <= nextCellMeasureInternalTickPosition)
        {
            // Found correct cell
            return ticks - r.measureInternalTicks + nextCellMeasureInternalTickPosition;
        }

        // Advance to next cell
        ++measureInternalCellIndex;
        measureInternalCellTickPosition=nextCellMeasureInternalTickPosition;
    }
}

int DocRoot::roundDownTicksToMeasureBorder(int ticks) const
{
    // If currently on a measure border, then return unmodified value. Otherwise round down.
    Q_ASSERT(ticks >= 0);
    TicksToMeasureResult r = ticksToMeasure(ticks);
    return ticks - r.measureInternalTicks;
}

int DocRoot::roundUpTicksToMeasureBorder(int ticks) const
{
    // If currently on a measure border, then return unmodified value. Otherwise round up.
    Q_ASSERT(ticks >= 0);
    TicksToMeasureResult r = ticksToMeasure(ticks);
    if(r.measureInternalTicks == 0)return ticks;
    return ticks - r.measureInternalTicks + ticksPerMeasure(r.measureProperties);
}

int DocRoot::getMaxTicks() const
{
    // 1. Traverse all tracks and find event with latest end tick position.
    int maxTicks=0;
    for(int i=0; i < trackList.size(); ++i)
    {
        DocTrack* track=trackList[i];
        DocEvent* event=track->firstEvent;
        while(event)
        {
            int ticksEnd = event->tickPositionEnd() - 1;     // last tick is excluded in getMaxDocTicks()
            if(ticksEnd > maxTicks) maxTicks = ticksEnd;
            event=event->nextEvent;
        }
    }

    // 2. Traverse all measure items and find item with latest tick position
    for(int i=0; i < measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=measureItemList[i];
        if(measureItem->tickPosition > maxTicks) maxTicks = measureItem->tickPosition;
    }
    return maxTicks;
}

int DocRoot::getMaxMeasure() const
{
    // Get last measure occupied touched by an event in any track
    return ticksToMeasure(getMaxTicks()).measureIndex;
}

QString DocRoot::getNewTrackIdentifier() const
{
    // Assign "Track x", where x is the first integer >= 1 where no such name exists in the track list so far.
    QString nameTemplate=QApplication::translate("DocRoot","Track %1");
    QString newName;

    int x=1;
    bool trackNameAlreadyExists;
    do
    {
        trackNameAlreadyExists=false;
        newName=nameTemplate.arg(x);

        for(int i=0; i < trackList.size(); ++i)
        {
            if(trackList[i]->name == newName)
            {
                // found track with same name, increase x
                ++x;
                trackNameAlreadyExists=true;
                break;
            }
        }
    }
    while(trackNameAlreadyExists);

    return newName;
}

int DocRoot::getNewTrackChannel() const
{
    // If possible, assign unused non-drum channel
    int channel=MIDI_MIN_CHANNEL;
    bool channelAlreadyExists;
    do
    {
        channelAlreadyExists=false;

        for(int i=0; i < trackList.size(); ++i)
        {
            if(trackList[i]->midiChannel == channel)
            {
                // found track with same channel, try again with higher channel number
                ++channel;
                if(channel > MIDI_MAX_CHANNEL)return MIDI_MIN_CHANNEL; // impossible
                if(channel == MIDI_PERCUSSION_CHANNEL)++channel;       // skip percussion-channel

                channelAlreadyExists=true;
                break;
            }
        }
    }
    while(channelAlreadyExists);

    return channel;
}

QColor DocRoot::getNewTrackEventColor() const
{
    // Assign the color least used with the smallest index in the DOCUMENT_STD_EVENT_COLORS table.
    int colorCounter[DOCUMENT_N_EVENT_COLORS];
    for(int j=0; j < DOCUMENT_N_EVENT_COLORS; ++j)colorCounter[j]=0;

    for(int i=0; i < trackList.size(); ++i)
    {
        QColor color=trackList[i]->eventColor;
        for(int j=0; j < DOCUMENT_N_EVENT_COLORS; ++j)
            if(color == DOCUMENT_EVENT_COLORS[j])
            {
                ++colorCounter[j];
                break;
            }
    }

    int bestColorIndex=-1;
    int bestColorIndexCounter=INT_MAX;
    for(int j=0; j < DOCUMENT_N_EVENT_COLORS; ++j)
        if(colorCounter[j] < bestColorIndexCounter)
        {
            bestColorIndexCounter=colorCounter[j];
            bestColorIndex=j;
        }

    return DOCUMENT_EVENT_COLORS[bestColorIndex];
}

EditorState DocRoot::prepareDefaultDocument()
{
    unmodifiedDefaultDocument = true;

    // set default tick resolution
    midiTicksPerWholeNote = DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE;

    // create one default measure item
    DocMeasureItem* measureItem=new DocMeasureItem;
    measureItem->setFirstMeasureItemDefaults();
    measureItemList.append(measureItem);

    // create one default track
    DocTrack* track=new DocTrack;
    track->setDefaultProperties(this);
    trackList.append(track);

    // set default editor startup state
    EditorState startupState;
    startupState.setStartupDefaultState(this);
    return startupState;
}

EditorState DocRoot::prepareDocumentByWizardSettings(const DocMeasureItem& firstMeasureProperties, QString trackWizardString, bool assignPatches)
{
    Q_ASSERT(measureItemList.size() == 0);
    Q_ASSERT(firstMeasureProperties.tickPosition == 0);

    // set default tick resolution
    midiTicksPerWholeNote = DOCUMENT_MIN_TICKS_PER_WHOLE_NOTE;

    // create first measure item as indicated by wizard
    DocMeasureItem* firstMeasureItem=new DocMeasureItem(firstMeasureProperties);
    measureItemList.append(firstMeasureItem);

    EditorState startupState;
    startupState.setStartupDefaultState(this);

    // create tracks as indicated by wizard
    int numberOfTracksCreated=0;
    while(!trackWizardString.isEmpty())
    {
        EditorTrackState newTrackState;
        DocTrack* newTrack=processTrackWizardString(assignPatches, newTrackState, trackWizardString);

        // append track and track state
        trackList.append(newTrack);
        startupState.trackStateList.append(newTrackState);

        ++numberOfTracksCreated;
    }

    if(numberOfTracksCreated >= 2)
    {
        // generate default panorama for all tracks just created
        for(int trackIndex=0; trackIndex < trackList.size(); ++trackIndex)
        {
            trackList[trackIndex]->midiPanorama=
                    (2*DOCUMENT_AUTO_PANORAMA_MAX_SHIFT) * trackIndex / (numberOfTracksCreated - 1)
                    - DOCUMENT_AUTO_PANORAMA_MAX_SHIFT + MIDI_PANORAMA_CENTER;
        }
    }

    // If document now contains tracks, set selection to first cell in top track
    startupState.setStartupSelection(this);

    return startupState;
}

bool DocRoot::isValidTrackWizardString(const QString& tracksToAdd)
{
    // check for syntax "(c[d])*", where c is a valid track code and d an optional decimal digit
    int i=0;
    bool lastWasDigit=false;
    while(i < tracksToAdd.length())
    {
        QChar c=tracksToAdd[i];
        if(c.isLetter())
        {
            // check for valid code
            int trackType=0;
            for(; trackType < DOCUMENT_TRACK_WIZARD_N_TRACK_TYPES; ++trackType)
                if(c == DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].code)break;

            if(trackType == DOCUMENT_TRACK_WIZARD_N_TRACK_TYPES)
                return false;   // found invalid code

            lastWasDigit=false;
        }
        else if(c.isDigit())
        {
            if(lastWasDigit)
                return false;   // found double digit

            lastWasDigit=true;
        }
        else return false;      // found invalid character

        // next character
        ++i;
    }

    // string passed all tests
    return true;
}

DocTrack* DocRoot::processTrackWizardString(bool assignPatch, EditorTrackState& newTrackState, QString& trackWizardString) const
{
    // extract first track specification in wizard string and create new track and new EditorTrackState

    Q_ASSERT(isValidTrackWizardString(trackWizardString));
    Q_ASSERT(!trackWizardString.isEmpty());

    // Create a new track:
    //  Set some special attributes: track name, MIDI patch, MIDI channel, displayed note range
    //  Set rest of track attributes to default values

    // analyse track code

    QChar trackCode=trackWizardString[0];
    trackWizardString.remove(0,1);  // remove code from string

    Q_ASSERT(trackCode.isLetter());

    int trackType=0;
    for(; trackType < DOCUMENT_TRACK_WIZARD_N_TRACK_TYPES; ++trackType)
        if(trackCode == DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].code)break;

    Q_ASSERT(trackType < DOCUMENT_TRACK_WIZARD_N_TRACK_TYPES);  // track type must exist

    // check for a digit following the letter
    int trackSequentialNumber=-1;
    if(!trackWizardString.isEmpty())
    {
        QChar digit=trackWizardString[0];
        if(digit.isDigit())
        {
            trackWizardString.remove(0,1);  // remove digit from string

            trackSequentialNumber=digit.digitValue();
            if(trackSequentialNumber == 0)trackSequentialNumber=10; // 0 means 10
        }
    }

    // generate new track with default attributes
    DocTrack* newTrack=new DocTrack;
    newTrack->setDefaultProperties(this);

    // customize some track attributes
    newTrack->name=QApplication::translate("DocRoot",DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].trackName);

    if(trackSequentialNumber != -1)
    {
        // add sequential number
        newTrack->name+=QApplication::translate("DocRoot"," %1").arg(trackSequentialNumber);
    }

    if(assignPatch)
        newTrack->midiPatch=DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].midiPatch;

    if(DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].assignPercussionChannel)
        newTrack->midiChannel=MIDI_PERCUSSION_CHANNEL;

    // generate new editor track state
    newTrackState=EditorTrackState();
    newTrackState.centerMidiNote=
            DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].baseMidiNote +
            DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].heightInNotes / 2 - 0.5;
    newTrackState.heightInNotes=
            DOCUMENT_TRACK_WIZARD_TRACK_TYPES[trackType].heightInNotes;

    return newTrack;
}

bool DocRoot::swingPresent() const
{
    for(int i=0; i < measureItemList.size(); ++i)
    {
        DocMeasureItem* measureItem=measureItemList[i];
        if(measureItem->setPlaybackOptions)
        {
            if(measureItem->swingHardness != 0)
                return true;
        }
    }
    return false;
}

QString DocRoot::collectNonStandardPlaybackOptionsDescription(int relativePlaybackSpeed) const
{
    // return empty description if document does not contain any non-standard playback options
    QString description;

    if(relativePlaybackSpeed != 100)
    {
        // found non-standard playback option: relative playback speed != 100 %
        description+=QApplication::translate(
                "DocRoot","- Relative Playback Speed = %1%\n").arg(relativePlaybackSpeed);
    }

    if(swingPresent())
    {
        // found non-standard playback option: swing
        description+=QApplication::translate("DocRoot","- Swing\n");
    }

    return description;
}

bool DocRoot::load(QFile* smfFile, EditorState* loadedEditorState)
{
    SmfDocument smfDoc(smfFile);
    if(!smfDoc.load())return false;

    SmfImporter importer(this,&smfDoc,loadedEditorState);
    if(!importer.doImport())return false;

    return true;
}

bool DocRoot::save(QFile* smfFile, const EditorState& editorStateToSave, bool saveEditorState) const
{
    // standard save

    SmfDocument smfDoc(smfFile);

    SmfExporter smfExporter(this,&smfDoc,&editorStateToSave);
    if(!smfExporter.doExport(saveEditorState))return false;

    return smfDoc.save();
}

bool DocRoot::save(QFile* smfFile, const EditorState& editorStateToSave, const ConversionOptions& conversionOptions) const
{
    if(conversionOptions.savingOriginalFile)
    {
        return save(smfFile, editorStateToSave, true);    // standard document save
    }
    else    // save compatible file
    {
        // create a duplicate document with selected playback options
        //  converted to compatible MIDI commands
        DocRoot compatibleDoc;
        compatibleDoc.makeDeepCopy(*this);
        compatibleDoc.makeCompatible(conversionOptions);

        // Save modified document. Do not save editor state because it may be invalid due to modifications.
        return compatibleDoc.save(smfFile, editorStateToSave, false);
    }
}

bool DocRoot::save(QFile* smfFile, const EditorState& editorStateToSave, const ConversionOptions& conversionOptions, const QList<int> trackIndexList) const
{
    // save a part (called during process of part extraction)

    // create a duplicate document with selected compatibility conversions
    DocRoot partDoc;
    partDoc.makeDeepCopy(*this);
    partDoc.makeCompatible(conversionOptions);

    // index shifts occur during deletion, so start from end of track list
    for(int trackIndex=partDoc.trackList.size()-1; trackIndex >= 0; --trackIndex)
    {
        // remove tracks not in trackIndexList
        if(!trackIndexList.contains(trackIndex))
            delete partDoc.trackList.takeAt(trackIndex);
    }

    // Save modified document. Do not save editor state because it may be invalid due to modifications.
    return partDoc.save(smfFile, editorStateToSave, false);
}

void DocRoot::makeCompatible(const ConversionOptions& conversionOptions)
{
    for(int i=0; i < trackList.size(); ++i)
        trackList[i]->makeCompatible(this, conversionOptions);

    for(int i=0; i < measureItemList.size(); ++i)
        measureItemList[i]->makeCompatible(conversionOptions);

    // delete all playback options
    for(int i=0; i < measureItemList.size(); ++i)
        measureItemList[i]->removePlaybackOptions(conversionOptions);
}

void DocRoot::scaleTickResolution(int newTicksPerWholeNote)
{
    // All tick specifications are scaled with integer precision, so first multiply and then divide.

    // Update document-global meta event list
    for(int j=0; j < metaEventList.size(); ++j)
        metaEventList[j]->scaleTickResolution(newTicksPerWholeNote, midiTicksPerWholeNote);

    // Update all measure items
    for(int i=0; i < measureItemList.size(); ++i)
        measureItemList[i]->scaleTickResolution(newTicksPerWholeNote, midiTicksPerWholeNote);

    // Update all tracks
    for(int i=0; i < trackList.size(); ++i)
        trackList[i]->scaleTickResolution(newTicksPerWholeNote, midiTicksPerWholeNote);

    // remember new resolution
    midiTicksPerWholeNote=newTicksPerWholeNote;
}
