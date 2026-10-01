/***************************************************************************
 *  smfexporter.h - Exports Editor Model to Standard MIDI File Data-Tree
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

#ifndef SMFEXPORTER_H
#define SMFEXPORTER_H

#include "global.h"
#include "smfdocument.h"

class SmfExporter
{
public:
    SmfExporter(const DocRoot* docRoot, SmfDocument* smfDocument, const EditorState* editorState);
    bool doExport(bool saveEditorState);

protected:
    bool exportConductorTrack(bool saveEditorState);
    bool exportMainConfigXML(bool saveEditorState);
    bool exportConductorTrackMetaEvents();
    bool exportNormalTracks();
    bool exportTrackConfigXML(DocTrack* track, SmfTrack* smfTrack);
    bool exportTrackEvents(DocTrack* track, SmfTrack* smfTrack);

    static bool eventOrderingLessThan(SmfEvent* e1, SmfEvent* e2);

    // Source and destination documents
    const DocRoot* docRoot;
    SmfDocument* smfDocument;
    const EditorState* editorState;

    int xmlConfigVersion;   // -1 if no configuration available
};

class SmfExporterMidiEvent : public SmfMidiEvent
{
public:
    SmfExporterMidiEvent()
    {
        beforeNoteEvents=false;
        index=-1;
    }
    // store same-tick-subordering information
    bool beforeNoteEvents;
    int index;
    bool stateRestoration=false; // playback startup/seek state, not a file property
};

#endif // SMFEXPORTER_H
