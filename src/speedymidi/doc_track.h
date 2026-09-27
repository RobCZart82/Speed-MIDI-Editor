/***************************************************************************
 *  doc_track.h - Model class: Track
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

#ifndef DOC_TRACK_H
#define DOC_TRACK_H

#include "global.h"

class DocTrack
{
public:
    DocTrack();
    DocTrack(const DocTrack& rhs);
    ~DocTrack();
    void makeDeepCopy(const DocTrack& rhs);

    DocTrack& operator=(const DocTrack& rhs);
    bool operator!=(const DocTrack& rhs) const;

    void serialize(QDataStream& dataStream) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream);        // paste from clipboard

    int getEventColorStandardIndex() const;   // returns -1 if no standard color
    void setDefaultProperties(const DocRoot* docRoot);
    int centerNoteNumber() const;
    void scaleTickResolution(int newResolution, int oldResolution);
    void makeCompatible(DocRoot* docRoot, const ConversionOptions& conversionOptions);

    void insertEvent(DocEvent* event);
    void removeEvent(DocEvent* event);

    bool loadXMLTrackConfig(QDomElement& rootElement, int xmlConfigVersion);
    bool saveXMLTrackConfig(QDomElement& rootElement, int xmlConfigVersion) const;

    // Entry pointer to double-linked event list
    DocEvent* firstEvent;

    // non-time-shiftable track meta events from SMF_Document (not editable)
    QList<SmfMetaEvent*> metaEventList;

    // -----------------------------------------------------------------------------
    // Properties covered by undo/redo

    QString name;
    QColor eventColor;

    // MIDI settings
    int midiVolume;     // range [0;127]
    int midiPanorama;   // range [0;127], center=MIDI_PANORAMA_CENTER
    int midiPatch;      // range [1;128]
    int midiChannel;    // range [1;16]
};

#endif // DOC_TRACK_H
