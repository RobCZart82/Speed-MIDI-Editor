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
        importOrder=-1;
    }
    // store same-tick-subordering information
    bool beforeNoteEvents;
    int index;
    qint64 importOrder;

    // Shared by file export and playback. Imported events form one ordered
    // group; generated setup/releases precede it and generated note-ons follow
    // it. Grouping keeps the comparator transitive even in mixed edited tracks.
    static int sameTickGroup(const SmfExporterMidiEvent* event)
    {
        if(event->importOrder >= 0)return 1;
        const int command=event->midiCommand[0] & 0xf0;
        const bool note=command == 0x80 || command == 0x90;
        const bool off=command == 0x80 || (command == 0x90 && event->midiCommand[2] == 0);
        return (off || (!note && event->beforeNoteEvents)) ? 0 : 2;
    }
    static bool sameTickLessThan(const SmfExporterMidiEvent* e1, const SmfExporterMidiEvent* e2)
    {
        const int command1=e1->midiCommand[0] & 0xf0;
        const int command2=e2->midiCommand[0] & 0xf0;
        const bool note1=command1 == 0x80 || command1 == 0x90;
        const bool note2=command2 == 0x80 || command2 == 0x90;
        const bool off1=command1 == 0x80 || (command1 == 0x90 && e1->midiCommand[2] == 0);
        const bool off2=command2 == 0x80 || (command2 == 0x90 && e2->midiCommand[2] == 0);
        const int group1=sameTickGroup(e1);
        const int group2=sameTickGroup(e2);
        if(group1 != group2)return group1 < group2;
        if(group1 == 1)return e1->importOrder < e2->importOrder;

        const int master1=note1 ? 1 : (e1->beforeNoteEvents ? 0 : 2);
        const int master2=note2 ? 1 : (e2->beforeNoteEvents ? 0 : 2);
        if(master1 != master2)return master1 < master2;
        if(note1 && note2)
        {
            if(off1 != off2)return off1;
            return e1->midiCommand[1] < e2->midiCommand[1];
        }
        return e1->index < e2->index;
    }
};

#endif // SMFEXPORTER_H
