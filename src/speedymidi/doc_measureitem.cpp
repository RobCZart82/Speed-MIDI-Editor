/***************************************************************************
 *  doc_measureitem.cpp - Model Class: Measure Item
 *                        (cf. Model–View–Controller Architecture)
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

#include "doc_measureitem.h"
#include "doc_root.h"

DocMeasureItem::DocMeasureItem()
{
    invalidate();
}

DocMeasureItem::DocMeasureItem(const DocMeasureItem& rhs)
{
    operator=(rhs);
}

DocMeasureItem::~DocMeasureItem()
{
}

void DocMeasureItem::makeDeepCopy(const DocMeasureItem& rhs)
{
    // deep copy constructor for "save compatible file"
    operator=(rhs);

    // copy subobjects from rhs
    // (no subobjects)
}

DocMeasureItem& DocMeasureItem::operator=(const DocMeasureItem& rhs)
{
    if(&rhs == this)return *this;

    // Undo/Redo: Command_MeasureItemProperties

    tickPosition                =   rhs.tickPosition;

    setTempo                    =   rhs.setTempo;
    BPM                         =   rhs.BPM;

    setTimeSignature            =   rhs.setTimeSignature;
    timeSignatureNominator      =   rhs.timeSignatureNominator;
    timeSignatureDenominator    =   rhs.timeSignatureDenominator;

    setKeySignature             =   rhs.setKeySignature;
    keySignature                =   rhs.keySignature;
    keySignatureScale           =   rhs.keySignatureScale;

    setRehearsalMarker          =   rhs.setRehearsalMarker;
    rehearsalMarkerText         =   rhs.rehearsalMarkerText;
    rehearsalMarkerColor        =   rhs.rehearsalMarkerColor;

    setPlaybackOptions          =   rhs.setPlaybackOptions;
    swingHardness               =   rhs.swingHardness;

    return *this;
}

bool DocMeasureItem::operator!=(const DocMeasureItem& rhs) const
{
    // Undo/Redo: check before properties change undo command is required
    return
            tickPosition                !=   rhs.tickPosition ||

            setTempo                    !=   rhs.setTempo ||
            BPM                         !=   rhs.BPM ||

            setTimeSignature            !=   rhs.setTimeSignature ||
            timeSignatureNominator      !=   rhs.timeSignatureNominator ||
            timeSignatureDenominator    !=   rhs.timeSignatureDenominator ||

            setKeySignature             !=   rhs.setKeySignature ||
            keySignature                !=   rhs.keySignature ||
            keySignatureScale           !=   rhs.keySignatureScale ||

            setRehearsalMarker          !=   rhs.setRehearsalMarker ||
            rehearsalMarkerText         !=   rhs.rehearsalMarkerText ||
            rehearsalMarkerColor        !=   rhs.rehearsalMarkerColor ||

            setPlaybackOptions          !=   rhs.setPlaybackOptions ||
            swingHardness               !=   rhs.swingHardness;
}

void DocMeasureItem::invalidate()
{
    // Invalidate properties and reset all set flags
    tickPosition=-1;

    setTempo=false;
    BPM=-1;

    setTimeSignature=false;
    timeSignatureNominator=-1;
    timeSignatureDenominator=-1;

    setKeySignature=false;
    keySignature=INT_MAX;
    keySignatureScale=KSS_Major;

    setRehearsalMarker=false;
    //rehearsalMarkerColor
    //rehearsalMarkerText

    setPlaybackOptions=false;
    swingHardness=-1;
}

void DocMeasureItem::serialize(QDataStream& dataStream, int selectionTicksLeft) const
{
    // copy properties to clipboard

    dataStream << (tickPosition - selectionTicksLeft);  // tick position relative to selection start

    dataStream << setTempo;
    dataStream << BPM;

    dataStream << setTimeSignature;
    dataStream << timeSignatureNominator;
    dataStream << timeSignatureDenominator;

    dataStream << setKeySignature;
    dataStream << keySignature;
    dataStream << (int)keySignatureScale;

    dataStream << setRehearsalMarker;
    dataStream << rehearsalMarkerColor;
    dataStream << rehearsalMarkerText;

    dataStream << setPlaybackOptions;
    dataStream << swingHardness;
}

void DocMeasureItem::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    dataStream >> tickPosition;  // tick position relative to selection start

    dataStream >> setTempo;
    dataStream >> BPM;

    dataStream >> setTimeSignature;
    dataStream >> timeSignatureNominator;
    dataStream >> timeSignatureDenominator;

    dataStream >> setKeySignature;
    dataStream >> keySignature;

    int iTemp;
    dataStream >> iTemp;
    // Older clipboards used -1 for an unset scale. Do not construct an
    // out-of-range enum, even when the set flag is false.
    if(setKeySignature && iTemp != KSS_Major && iTemp != KSS_Minor)
        dataStream.setStatus(QDataStream::ReadCorruptData);
    keySignatureScale=iTemp == KSS_Minor ? KSS_Minor : KSS_Major;

    dataStream >> setRehearsalMarker;
    dataStream >> rehearsalMarkerColor;
    dataStream >> rehearsalMarkerText;

    dataStream >> setPlaybackOptions;
    dataStream >> swingHardness;
}

void DocMeasureItem::clean()
{
    if(!setTempo)BPM=-1;
    if(!setTimeSignature)
    {
        timeSignatureNominator=-1;
        timeSignatureDenominator=-1;
    }
    if(!setKeySignature)
    {
        keySignature=INT_MAX;
        keySignatureScale=KSS_Major;
    }
    if(!setRehearsalMarker)
    {
        rehearsalMarkerColor=QColor();   // assign invalid color
        rehearsalMarkerText.clear();
    }
    if(!setPlaybackOptions)
    {
        swingHardness=-1;
    }
}

void DocMeasureItem::resetSetFlags()
{
    setTempo=false;
    setTimeSignature=false;
    setKeySignature=false;
    setRehearsalMarker=false;
    setPlaybackOptions=false;
}

void DocMeasureItem::makeEffectiveMeasureProperties(const DocMeasureItem& otherItem)
{
    tickPosition=otherItem.tickPosition;

    setTempo=otherItem.setTempo;
    if(setTempo)BPM=otherItem.BPM;

    setTimeSignature=otherItem.setTimeSignature;
    if(setTimeSignature)
    {
        timeSignatureNominator=otherItem.timeSignatureNominator;
        timeSignatureDenominator=otherItem.timeSignatureDenominator;
    }

    setKeySignature=otherItem.setKeySignature;
    if(setKeySignature)
    {
        keySignature=otherItem.keySignature;
        keySignatureScale=otherItem.keySignatureScale;
    }

    setRehearsalMarker=otherItem.setRehearsalMarker;
    if(setRehearsalMarker)
    {
        rehearsalMarkerText=otherItem.rehearsalMarkerText;
        rehearsalMarkerColor=otherItem.rehearsalMarkerColor;
    }

    setPlaybackOptions=otherItem.setPlaybackOptions;
    if(setPlaybackOptions)
    {
        swingHardness=otherItem.swingHardness;
    }
}

void DocMeasureItem::mergeMeasureItems(const DocMeasureItem& otherItem)
{
    if(otherItem.setTempo)
    {
        setTempo=true;
        BPM=otherItem.BPM;
    }
    if(otherItem.setTimeSignature)
    {
        setTimeSignature=true;
        timeSignatureNominator=otherItem.timeSignatureNominator;
        timeSignatureDenominator=otherItem.timeSignatureDenominator;
    }
    if(otherItem.setKeySignature)
    {
        setKeySignature=true;
        keySignature=otherItem.keySignature;
        keySignatureScale=otherItem.keySignatureScale;
    }
    if(otherItem.setRehearsalMarker)
    {
        setRehearsalMarker=true;
        rehearsalMarkerText=otherItem.rehearsalMarkerText;
        rehearsalMarkerColor=otherItem.rehearsalMarkerColor;
    }
    if(otherItem.setPlaybackOptions)
    {
        setPlaybackOptions=true;
        swingHardness=otherItem.swingHardness;
    }
}

bool DocMeasureItem::measureItemRequired() const
{
    return setTempo || setTimeSignature || setKeySignature || setRehearsalMarker || setPlaybackOptions;
}

void DocMeasureItem::setRequiredFlagsFirstMeasure()
{
    setTempo=true;
    setTimeSignature=true;
    setKeySignature=true;
    // setRehearsalMarker is not required in first measure
    // setPlaybackOptions is not required in first measure
}

void DocMeasureItem::setFirstMeasureDefaultProperties()
{
    tickPosition=0;

    resetSetFlags();

    BPM=120;                        // 120 beats per minute

    timeSignatureNominator=4;       // 4/4
    timeSignatureDenominator=4;

    keySignature=0;                 // C-Major
    keySignatureScale=KSS_Major;

    rehearsalMarkerColor=QColor();  // invalid color
    rehearsalMarkerText.clear();    // no text

    swingHardness=0;                // no swing
}

void DocMeasureItem::setFirstMeasureItemDefaults()
{
    // Defaults in case of
    //  - there is no corresponding meta event in loaded standard MIDI file
    //  - start of setup wizard

    setFirstMeasureDefaultProperties();

    setTempo=true;
    setTimeSignature=true;
    setKeySignature=true;

    clean();
}

bool DocMeasureItem::isValidFirstMeasureItem() const
{
    return setTempo && setTimeSignature && setKeySignature;
}

void DocMeasureItem::enforceChangedProperties(const DocMeasureItem& previousMeasureProperties)
{
    // If previous measure has different properties in fields we do not set,
    //  enable our set-flag for these fields.
    if(BPM                      != previousMeasureProperties.BPM) setTempo=true;

    if(timeSignatureNominator   != previousMeasureProperties.timeSignatureNominator ||
       timeSignatureDenominator != previousMeasureProperties.timeSignatureDenominator) setTimeSignature=true;

    if(keySignature             != previousMeasureProperties.keySignature ||
       keySignatureScale        != previousMeasureProperties.keySignatureScale) setKeySignature=true;

    if(rehearsalMarkerText      != previousMeasureProperties.rehearsalMarkerText ||
       rehearsalMarkerColor     != previousMeasureProperties.rehearsalMarkerColor)
    {
        // do not enforce a non-existing rehearsal marker
        if(rehearsalMarkerColor.isValid()) setRehearsalMarker=true;
    }

    if(swingHardness            != previousMeasureProperties.swingHardness) setPlaybackOptions=true;
}

QString DocMeasureItem::keySignatureName() const
{
    QString s;
    if(keySignatureScale == DocMeasureItem::KSS_Major)
    {
        switch(keySignature)  // from -MIDI_MAX_KEY_SIGNATURE to MIDI_MAX_KEY_SIGNATURE
        {
        case -11:s=QApplication::translate("DocMeasureItem","Abb-major");break;
        case -10:s=QApplication::translate("DocMeasureItem","Ebb-major");break;
        case  -9:s=QApplication::translate("DocMeasureItem","Bbb-major");break;
        case  -8:s=QApplication::translate("DocMeasureItem","Fb-major");break;
        case  -7:s=QApplication::translate("DocMeasureItem","Cb-major");break;
        case  -6:s=QApplication::translate("DocMeasureItem","Gb-major");break;
        case  -5:s=QApplication::translate("DocMeasureItem","Db-major");break;
        case  -4:s=QApplication::translate("DocMeasureItem","Ab-major");break;
        case  -3:s=QApplication::translate("DocMeasureItem","Eb-major");break;
        case  -2:s=QApplication::translate("DocMeasureItem","Bb-major");break;
        case  -1:s=QApplication::translate("DocMeasureItem","F-major");break;
        case   0:s=QApplication::translate("DocMeasureItem","C-major");break;
        case  +1:s=QApplication::translate("DocMeasureItem","G-major");break;
        case  +2:s=QApplication::translate("DocMeasureItem","D-major");break;
        case  +3:s=QApplication::translate("DocMeasureItem","A-major");break;
        case  +4:s=QApplication::translate("DocMeasureItem","E-major");break;
        case  +5:s=QApplication::translate("DocMeasureItem","B-major");break;
        case  +6:s=QApplication::translate("DocMeasureItem","F#-major");break;
        case  +7:s=QApplication::translate("DocMeasureItem","C#-major");break;
        case  +8:s=QApplication::translate("DocMeasureItem","G#-major");break;
        case  +9:s=QApplication::translate("DocMeasureItem","D#-major");break;
        case +10:s=QApplication::translate("DocMeasureItem","A#-major");break;
        case +11:s=QApplication::translate("DocMeasureItem","E#-major");break;
        default: s=QApplication::translate("DocMeasureItem","(invalid)");break;
        }
    }
    else
    {
        switch(keySignature)  // from -MIDI_MAX_KEY_SIGNATURE to MIDI_MAX_KEY_SIGNATURE
        {
        case -11:s=QApplication::translate("DocMeasureItem","fb-minor");break;
        case -10:s=QApplication::translate("DocMeasureItem","cb-minor");break;
        case  -9:s=QApplication::translate("DocMeasureItem","gb-minor");break;
        case  -8:s=QApplication::translate("DocMeasureItem","db-minor");break;
        case  -7:s=QApplication::translate("DocMeasureItem","ab-minor");break;
        case  -6:s=QApplication::translate("DocMeasureItem","eb-minor");break;
        case  -5:s=QApplication::translate("DocMeasureItem","bb-minor");break;
        case  -4:s=QApplication::translate("DocMeasureItem","f-minor");break;
        case  -3:s=QApplication::translate("DocMeasureItem","c-minor");break;
        case  -2:s=QApplication::translate("DocMeasureItem","g-minor");break;
        case  -1:s=QApplication::translate("DocMeasureItem","d-minor");break;
        case   0:s=QApplication::translate("DocMeasureItem","a-minor");break;
        case  +1:s=QApplication::translate("DocMeasureItem","e-minor");break;
        case  +2:s=QApplication::translate("DocMeasureItem","b-minor");break;
        case  +3:s=QApplication::translate("DocMeasureItem","f#-minor");break;
        case  +4:s=QApplication::translate("DocMeasureItem","c#-minor");break;
        case  +5:s=QApplication::translate("DocMeasureItem","g#-minor");break;
        case  +6:s=QApplication::translate("DocMeasureItem","d#-minor");break;
        case  +7:s=QApplication::translate("DocMeasureItem","a#-minor");break;
        case  +8:s=QApplication::translate("DocMeasureItem","e#-minor");break;
        case  +9:s=QApplication::translate("DocMeasureItem","b#-minor");break;
        case +10:s=QApplication::translate("DocMeasureItem","f##-minor");break;
        case +11:s=QApplication::translate("DocMeasureItem","c##-minor");break;
        default: s=QApplication::translate("DocMeasureItem","(invalid)");break;
        }
    }

    return s;
}

int DocMeasureItem::getMarkerStdColorIndex() const
{
    for(int j=0; j < DOCUMENT_N_MARKER_COLORS; ++j)
        if(rehearsalMarkerColor == DOCUMENT_MARKER_COLORS[j])return j;

    return -1;  // no standard color
}

void DocMeasureItem::scaleTickResolution(int newResolution, int oldResolution)
{
    // use 64 bit to retain full integer precision
    tickPosition = (int)((qint64)tickPosition * newResolution / oldResolution);
}

void DocMeasureItem::makeCompatible(const ConversionOptions& conversionOptions)
{
    if(setTempo && conversionOptions.convertRelativePlaybackSpeed)
    {
        BPM = conversionOptions.relativePlaybackSpeedInPercent * BPM / 100;
        if(BPM < MIDI_MIN_BPM) BPM=MIDI_MIN_BPM;
        if(BPM > MIDI_MAX_BPM) BPM=MIDI_MAX_BPM;
    }
}

void DocMeasureItem::removePlaybackOptions(const ConversionOptions& conversionOptions)
{
    if(conversionOptions.convertSwing)
    {
        swingHardness=0;
        setPlaybackOptions=false; // swing is currently the only playback option, so disable the set-flag
    }
}
