/***************************************************************************
 *  mousepianowidget.h - Mouse Piano
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

#ifndef MOUSEPIANOWIDGET_H
#define MOUSEPIANOWIDGET_H

#include "global.h"
#include <QWidget>

class MousePianoWidget : public QWidget
{
    Q_OBJECT
public:
    MousePianoWidget(QWidget * parent = 0);

    void updateKeyStatus(int noteNumber);
    void allNotesOff();

signals:
    void keyPressed(int noteNumber);
    void keyReleased(int noteNumber);

protected:
    SpeedyMidiApp* getApp() const { return (SpeedyMidiApp*)qApp; }
    int getLeftWhiteKey() const;
    void setLeftWhiteKey(int leftWhiteKey);
    void clampLeftWhiteKey();

    virtual void resizeEvent(QResizeEvent* event);
    virtual void paintEvent(QPaintEvent* event);
    virtual void hideEvent(QHideEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void wheelEvent(QWheelEvent* event);

    QColor shadedPaletteColor(float foreground_factor, QColor foregroundColor, QColor backgroundColor);

    enum MouseDragModeType { None, DragOctaveBar, NoteScrub, ToggleOneNote } mouseDragMode;
    QPoint mouseDragStartReferencePoint;
    int mouseDragPreviousValueInt;    // used for drag modes AdjustDisplayedNoteRange, AdjustTrackHeight
    int noteScrubCurrentNoteNumber;

    enum MouseZoneType {
        NoArea,
        OctaveBar,
        Key
    };
    struct MouseZoneResult { MouseZoneType zoneType; int noteNumber; Qt::CursorShape cursor; };
    MouseZoneResult getMouseZone(const QPoint& p);

    int mouseWheelAccu_8thsOfDegrees;   // Mouse wheel rotation accumulator

    QRect getKeyRect(int noteNumber) const;
    int getNoteNumber(const QPoint& p) const;

    QSize whiteKeySize;

    bool mousePianoKeyPressedArray[MIDI_N_NOTE_NUMBERS];            // mouse piano input only (not MIDI)

    QFont octaveNumberFont;
};

#endif // MOUSEPIANOWIDGET_H
