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

#ifndef SMFIMPORTER_H
#define SMFIMPORTER_H

#include "global.h"

class SmfImporter
{
public:
    SmfImporter(DocRoot* docRoot, SmfDocument* smfDocument, EditorState* editorState);
    bool doImport();

protected:
    bool importConductorTrack();
    bool importMainConfigXML();
    bool importTimeSignatures();
    bool importTempos();
    bool importOtherConductorTrackMetaEvents();
    bool importNormalTracks();
    bool importTrackConfigXML(DocTrack* track, SmfTrack* smfTrack);
    bool importTrackEvents(DocTrack* track, SmfTrack* smfTrack, bool filterOutConductorEvents);

    void setMeasureProperty(DocMeasureItem propertyItem);

    // Source and destination documents
    DocRoot* docRoot;
    SmfDocument* smfDocument;
    EditorState* editorState;

    int xmlConfigVersion;   // -1 if no configuration available

    bool foundEditorState;  // true if SMF contains a valid editor state as XML text event

    bool mixedConductorAndMidiEventsTrack;  // true if format 1 SMF contains conductor and midi events
                                            // in same track (track 0)
};

#endif // SMFIMPORTER_H
