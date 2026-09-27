/***************************************************************************
 *  cs_write.h - Controller Subsystem: Write
 *               (Write/Extend Note)
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

#ifndef CS_WRITE_H
#define CS_WRITE_H

#include "cs_common.h"

class CS_Write : public CS_Common
{
    Q_OBJECT
public:
    CS_Write(Controller* controller);

    virtual bool keyPressEvent(QKeyEvent* event);

protected slots:
    void actionWrite_WriteOrExtendNote_Triggered();
    void actionWrite_Write1_Triggered();
    void actionWrite_Write2_Triggered();
    void actionWrite_Write3_Triggered();
    void actionWrite_Write4_Triggered();
    void actionWrite_Write5_Triggered();
    void actionWrite_Write6_Triggered();
    void actionWrite_Write7_Triggered();
    void actionWrite_Write8_Triggered();
    void actionWrite_Write9_Triggered();
    void actionWrite_Write10_Triggered();
    void actionWrite_ExtendNote_Triggered();
    void actionWrite_Extend1_Triggered();
    void actionWrite_Extend2_Triggered();
    void actionWrite_Extend3_Triggered();
    void actionWrite_Extend4_Triggered();
    void actionWrite_Extend5_Triggered();
    void actionWrite_Extend6_Triggered();
    void actionWrite_Extend7_Triggered();
    void actionWrite_Extend8_Triggered();
    void actionWrite_Extend9_Triggered();
    void actionWrite_Extend10_Triggered();
    void actionWrite_TrackFlags_ToggleRecording_Triggered();
    void actionWrite_TrackFlags_ToggleRecordingExclusive_Triggered();

protected:
    void actionWrite_WriteX_Triggered(int numberOfCells);
    void actionWrite_ExtendX_Triggered(int numberOfCells);

    void writeCells(int numberOfCells, bool enableNoteExtension);
    void writeNote_(int trackIndex, int ticksLeft, int ticksRight, int noteNumber, bool enableNoteExtension);
    void writeExtendPreviousCellNotes(int numberOfCells);
    QList<int> prepareTrackListForWriting();
};

#endif // CS_WRITE_H
