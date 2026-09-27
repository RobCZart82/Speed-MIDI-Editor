/***************************************************************************
 *  doc_measureitem.h - Model Class: Measure Item
 *                      (cf. Model–View–Controller Architecture)
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

#ifndef DOC_MEASUREITEM_H
#define DOC_MEASUREITEM_H

#include "global.h"

class DocMeasureItem
{
public:
    DocMeasureItem();
    DocMeasureItem(const DocMeasureItem& rhs);
    ~DocMeasureItem();
    void makeDeepCopy(const DocMeasureItem& rhs);

    DocMeasureItem& operator=(const DocMeasureItem& rhs);
    bool operator!=(const DocMeasureItem& rhs) const;

    void invalidate();
    void serialize(QDataStream& dataStream, int selectionTicksLeft) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream);                                // paste from clipboard

    void clean();
    void resetSetFlags();
    void makeEffectiveMeasureProperties(const DocMeasureItem& otherItem);
    void mergeMeasureItems(const DocMeasureItem& otherItem);
    bool measureItemRequired() const;
    void setRequiredFlagsFirstMeasure();
    void setFirstMeasureDefaultProperties();
    void setFirstMeasureItemDefaults();
    bool isValidFirstMeasureItem() const;
    void enforceChangedProperties(const DocMeasureItem& previousMeasureProperties);
    QString keySignatureName() const;
    int getMarkerStdColorIndex() const;     // returns -1 if no standard color

    void scaleTickResolution(int newResolution, int oldResolution);
    void makeCompatible(const ConversionOptions& conversionOptions);
    void removePlaybackOptions(const ConversionOptions& conversionOptions);

    // -----------------------------------------------------------------------------
    // Properties covered by undo/redo

    int tickPosition;       // Absolute tick position

    bool setTempo;
    int BPM;

    bool setTimeSignature;
    int timeSignatureNominator;
    int timeSignatureDenominator;

    bool setKeySignature;
    int keySignature;       // [-MIDI_MAX_KEY_SIGNATURE;MIDI_MAX_KEY_SIGNATURE]
    enum KeySignatureScaleType { KSS_Major, KSS_Minor } keySignatureScale;

    bool setRehearsalMarker;
    QColor rehearsalMarkerColor;
    QString rehearsalMarkerText;

    bool setPlaybackOptions;
    int swingHardness;      // 0 means no swing, otherwise value ranges from
                            // DOCUMENT_MIN_SWING_HARDNESS to DOCUMENT_MAX_SWING_HARDNESS
                            // where 100 is triple meter (2:1)
};

class TicksToMeasureResult
{
public:
    TicksToMeasureResult()  { }
    TicksToMeasureResult(const TicksToMeasureResult& rhs)
            : measureProperties(rhs.measureProperties)
    {
        measureIndex=rhs.measureIndex;
        measureInternalTicks=rhs.measureInternalTicks;
    }
    int measureIndex;
    DocMeasureItem measureProperties;
    int measureInternalTicks;
};

#endif // DOC_MEASUREITEM_H
