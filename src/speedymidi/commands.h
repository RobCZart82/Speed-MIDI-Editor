/***************************************************************************
 *  commands.h - Undo/Redo Commands Used by Controller
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

#ifndef COMMANDS_H
#define COMMANDS_H

#include "global.h"
#include "doc_measureitem.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"

#include <QUndoCommand>

class EditorUndoCommand : public QUndoCommand
{
public:
    EditorUndoCommand()
    {
        controller=NULL;
        docRoot=NULL;
    }
    void connectToController(Controller* controller, DocRoot* docRoot)
    {
        this->controller=controller;
        this->docRoot=docRoot;
    }
protected:
    Controller* controller;
    DocRoot* docRoot;
};

class Command_SetEditorState : public EditorUndoCommand
{
public:
    Command_SetEditorState(const EditorState& state, const EditorRange& scrollToRange, bool afterOperation);

    virtual void redo();
    virtual void undo();

protected:
    EditorState state;
    EditorRange scrollToRange;
    bool afterOperation;
};

class Command_MeasureItemProperties : public EditorUndoCommand
{
public:
    Command_MeasureItemProperties(DocMeasureItem* measureItem, const DocMeasureItem& newPropertiesObject);
    ~Command_MeasureItemProperties() { }

    virtual void redo() { swap(); }
    virtual void undo() { swap(); }

protected:
    DocMeasureItem* measureItem;
    DocMeasureItem backupObject;

    void swap();
};

class Command_InsertMeasureItem : public EditorUndoCommand
{
public:
    Command_InsertMeasureItem(DocMeasureItem* measureItem);
    ~Command_InsertMeasureItem() { if(owningObject)delete measureItem; }

    virtual void redo();
    virtual void undo();

protected:
    DocMeasureItem* measureItem;
    bool owningObject;
};

class Command_DeleteMeasureItem : public EditorUndoCommand
{
public:
    Command_DeleteMeasureItem(DocMeasureItem* measureItem);
    ~Command_DeleteMeasureItem() { if(owningObject)delete measureItem; }

    virtual void redo();
    virtual void undo();

protected:
    DocMeasureItem* measureItem;
    bool owningObject;
};

class Command_ShiftMeasureItems : public EditorUndoCommand
{
public:
    Command_ShiftMeasureItems(int fromTickPosition, int deltaTicks);
    ~Command_ShiftMeasureItems() { }

    virtual void redo();
    virtual void undo();

protected:
    int fromTickPosition;
    int deltaTicks;
};

class Command_TrackProperties : public EditorUndoCommand
{
public:
    Command_TrackProperties(int trackIndex, const DocTrack& newPropertiesObject);
    ~Command_TrackProperties() { }

    virtual void redo() { swap(); }
    virtual void undo() { swap(); }

protected:
    int trackIndex;
    DocTrack backupObject;

    void swap();
};

class Command_InsertTrack : public EditorUndoCommand
{
public:
    Command_InsertTrack(int trackIndex, DocTrack* newTrack);
    ~Command_InsertTrack() { if(owningObject)delete track; }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndex;
    DocTrack* track;
    bool owningObject;
};

class Command_DeleteTrack : public EditorUndoCommand
{
public:
    Command_DeleteTrack(int trackIndex, DocTrack* track);
    ~Command_DeleteTrack() { if(owningObject)delete track; }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndex;
    DocTrack* track;
    bool owningObject;
};

class Command_MoveTrack : public EditorUndoCommand
{
public:
    Command_MoveTrack(int trackIndexFrom, int trackIndexTo);
    ~Command_MoveTrack() { }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndexFrom;
    int trackIndexTo;
};

class Command_EventProperties : public EditorUndoCommand
{
public:
    Command_EventProperties(int trackIndex, DocEvent* event, const DocEvent& newPropertiesObject);
    ~Command_EventProperties() { }

    virtual void redo() { swap(); }
    virtual void undo() { swap(); }

protected:
    int trackIndex;
    DocEvent* event;
    DocEvent backupObject;

    void swap();
};

class Command_InsertEvent : public EditorUndoCommand
{
public:
    Command_InsertEvent(int trackIndex, DocEvent* event);
    ~Command_InsertEvent() { if(owningObject)delete event; }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndex;
    DocEvent* event;
    bool owningObject;
};

class Command_DeleteEvent : public EditorUndoCommand
{
public:
    Command_DeleteEvent(int trackIndex, DocEvent* event);
    ~Command_DeleteEvent() { if(owningObject)delete event; }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndex;
    DocEvent* event;
    bool owningObject;
};

class Command_ShiftEvents : public EditorUndoCommand
{
public:
    Command_ShiftEvents(int trackIndex, int fromTickPosition, int deltaTicks);
    ~Command_ShiftEvents() { }

    virtual void redo();
    virtual void undo();

protected:
    int trackIndex;
    int fromTickPosition;
    int deltaTicks;
};

#endif // COMMANDS_H
