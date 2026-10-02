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
    void serialize(QDataStream& dataStream, int selectionTicksLeft, bool exactTempo=false,
                   int effectiveDenominator=4, bool meterMetadata=false) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream, bool exactTempo=false, bool meterMetadata=false); // paste from clipboard

    void clean();
    void resetSetFlags();
    void makeEffectiveMeasureProperties(const DocMeasureItem& otherItem);
    void mergeMeasureItems(const DocMeasureItem& otherItem);
    void mergeMeasureItemsPreservingTempo(const DocMeasureItem& otherItem, int effectiveDenominator);
    int tempoMicrosecondsPerQuarter(int effectiveDenominator) const;
    double tempoBPM(int effectiveDenominator) const;
    void setTempoBPM(double bpm, int effectiveDenominator);
    bool measureItemRequired() const;
    void setRequiredFlagsFirstMeasure();
    void setFirstMeasureDefaultProperties();
    void setFirstMeasureItemDefaults();
    bool isValidFirstMeasureItem() const;
    bool hasValidProperties() const;
    void enforceChangedProperties(const DocMeasureItem& previousMeasureProperties);
    QString keySignatureName() const;
    int getMarkerStdColorIndex() const;     // returns -1 if no standard color

    void scaleTickResolution(int newResolution, int oldResolution);
    void makeCompatible(const ConversionOptions& conversionOptions, int effectiveDenominator=4);
    void removePlaybackOptions(const ConversionOptions& conversionOptions);

    // -----------------------------------------------------------------------------
    // Properties covered by undo/redo

    int tickPosition;       // Absolute tick position

    bool setTempo;
    int BPM;
    // SMF tempo is canonical. Zero retains the legacy integer-BPM API/clipboard fallback.
    int microsecondsPerQuarter;
    QList<int> precedingTempoValues; // Earlier tempo messages at the same tick, in source order.

    bool setTimeSignature;
    int timeSignatureNominator;
    int timeSignatureDenominator;
    int midiClocksPerMetronomeClick; // -1 uses the historical default for newly created/legacy items.
    int notated32ndNotesPerQuarter; // Raw SMF meter metadata; independent of tick resolution.

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
