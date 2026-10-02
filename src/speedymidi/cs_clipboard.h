/***************************************************************************
 *  cs_clipboard.h - Controller Subsystem: Clipboard
 *                   (Cut, Copy, Paste, Scale)
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

#ifndef CS_CLIPBOARD_H
#define CS_CLIPBOARD_H

#include "cs_common.h"

class CS_Clipboard : public CS_Common
{
    Q_OBJECT
public:
    CS_Clipboard(Controller* controller);

protected slots:
    void actionEdit_Cut_Triggered();
    void actionEdit_Copy_Triggered();
    void actionEdit_Paste_Triggered();
    void actionEdit_PasteScaleToSelection_Triggered();

protected:
    void serializeSelection(QDataStream& dataStream, bool extended=true, bool exactTempo=true, bool meterMetadata=true); // copy selection to clipboard
    void pasteFromClipboard(bool scaleToSelection);                   // paste selection from clipboard
    void deserializeAndPasteIntoSelection(QDataStream& dataStream, bool scaleToSelection);
    void pasteGlobalMeasures(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardNumberOfMeasures);
    void pasteGlobalTracks(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList);
    void pasteLocalCells(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardTickRange);
    void pasteWithScaling(DocRoot* clipboardDoc, const QList<EditorTrackState>& clipboardTrackStateList, int clipboardTickRange);
};

#endif // CS_CLIPBOARD_H
