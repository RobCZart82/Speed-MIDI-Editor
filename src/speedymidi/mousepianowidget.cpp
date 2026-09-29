/***************************************************************************
 *  mousepianowidget.cpp - Mouse Piano
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

#include "mousepianowidget.h"
#include "speedymidiapp.h"
#include "settings.h"
#include "view.h"

#include <QPainter>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>

const QColor MOUSEPIANO_OCTAVE_COLORS[MIDI_MAX_OCTAVE+1]={

    // MIDI_MAX_OCTAVE == 10, thus 11 colors

    QColor(0x00,0x00,0x00),
    QColor(0x00,0x00,0x80),
    QColor(0x00,0x00,0xff),
    QColor(0x80,0x00,0xff),
    QColor(0xff,0x00,0x00),
    QColor(0xff,0x80,0x00),
    QColor(0xff,0xff,0x00),
    QColor(0x80,0xff,0x00),
    QColor(0x00,0xff,0x80),
    QColor(0xff,0xff,0xff),
    QColor(0x80,0x80,0x80),
};

const double MOUSEPIANO_BLACK_KEY_POSITION[5]={

    // left border of C key is 0, coordinate is x center position

    0.92,   // C#
    2.08,   // D#
    3.88,   // F#
    5.00,   // G#
    6.12    // A#
};

const int MOUSEPIANO_NOTE_INDEX_TO_WHITE_KEY_INDEX[12]= { 0,0,1,1,2,3,3,4,4,5,5,6 };
const int MOUSEPIANO_NOTE_INDEX_TO_BLACK_KEY_INDEX[12]= { -1,0,-1,1,-1,-1,2,-1,3,-1,4,-1 };
const int MOUSEPIANO_WHITE_KEY_INDEX_TO_NOTE_INDEX[ 7]= { 0,2,4,5,7,9,11 };
const int MOUSEPIANO_BLACK_KEY_INDEX_TO_NOTE_INDEX[ 5]= { 1,3,6,8,10 };

MousePianoWidget::MousePianoWidget(QWidget * parent)
        : QWidget(parent)
{
    setMaximumSize(QWIDGETSIZE_MAX, MOUSEPIANO_MAX_YSIZE);
    setMinimumSize(150,MOUSEPIANO_MIN_YSIZE);
    setBackgroundRole(QPalette::Window);
    setMouseTracking(true); // needed for cursor updates

    mouseDragMode=None;
    mouseDragPreviousValueInt=-1;
    noteScrubCurrentNoteNumber=-1;

    mouseWheelAccu_8thsOfDegrees=0;

    for(int i=0; i < MIDI_N_NOTE_NUMBERS; ++i)
        mousePianoKeyPressedArray[i]=false;

    octaveNumberFont=QFont(VIEW_FONT_NAME, 10, QFont::Bold);
}

void MousePianoWidget::resizeEvent(QResizeEvent* event)
{
    whiteKeySize.setHeight(event->size().height() - MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE);
    whiteKeySize.setWidth((int)(whiteKeySize.height() * MOUSEPIANO_WHITE_KEY_Y_TO_X_SIZE_FACTOR));

    clampLeftWhiteKey();
    update();
}

int MousePianoWidget::getLeftWhiteKey() const
{
    return getApp()->getSettings()->mousePiano.leftWhiteKey;
}

void MousePianoWidget::setLeftWhiteKey(int leftWhiteKey)
{
    getApp()->getSettings()->mousePiano.leftWhiteKey=leftWhiteKey;
}

void MousePianoWidget::clampLeftWhiteKey()
{
    // adjust center white key to avoid any empty areas left or right (priority: left)
    int maxNoteRightX = getKeyRect(MIDI_N_NOTE_NUMBERS-1).right();

    if(maxNoteRightX < rect().width())
        setLeftWhiteKey(MOUSEPIANO_MAX_WHITE_KEY - rect().width() / whiteKeySize.width());

    if(getLeftWhiteKey() < 0)setLeftWhiteKey(0);
}

void MousePianoWidget::paintEvent(QPaintEvent* event)
{
    UNUSED(event);

    QPainter painter(this);

    QColor textColor=shadedPaletteColor(.4,
                                        palette().color(QPalette::WindowText),
                                        palette().color(QPalette::Window));

    // get minimum and maximum displayed noteNumber (white keys only)
    int minNoteNumber=getNoteNumber(QPoint(0,rect().bottom()));
    int maxNoteNumber=getNoteNumber(QPoint(rect().right(),rect().bottom()));

    if(minNoteNumber <  0)minNoteNumber=0;
    if(maxNoteNumber >= MIDI_N_NOTE_NUMBERS)maxNoteNumber=MIDI_N_NOTE_NUMBERS-1;

    int minOctave=minNoteNumber / 12;
    int maxOctave=maxNoteNumber / 12;
    for(int midiOctave=minOctave; midiOctave <= maxOctave; ++midiOctave)
    {
        if(midiOctave > MIDI_MAX_OCTAVE)break;

        int leftX=getKeyRect(midiOctave * 12).left();
        int rightX=getKeyRect(qMin((midiOctave+1) * 12, MIDI_N_NOTE_NUMBERS)).left();

        // draw octave drag bar
        painter.setBrush(Qt::NoBrush);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawLine(leftX, MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE, rightX, MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE);

        // draw rectangle showing octave color corresponding to mouse piano octave color
        painter.setBrush(QBrush(MOUSEPIANO_OCTAVE_COLORS[midiOctave]));
        painter.setPen(Qt::NoPen);
        painter.drawRect(leftX, 0,
                         3, MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE);

        // draw big MIDI octave number
        painter.setPen(textColor);
        painter.setFont(octaveNumberFont);
        QRect octaveNumberTextRect(QPoint(leftX, 0), QPoint(rightX, MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE));
        painter.drawText(octaveNumberTextRect,
                         Qt::AlignCenter | Qt::AlignVCenter,
                         QString("%1").arg(midiOctave));

        // draw bottom C in musical notation standard
        QString CText;
        if(midiOctave >= 4)
        {
            CText=tr("c");
            for(int i=4; i < midiOctave; ++i)
                CText+='\'';
        }
        else
        {
            for(int i=3; i > midiOctave; --i)
                CText+=',';
            CText+=tr("C");
        }

        QRect octaveCTextRect(QPoint(leftX + 4, 0), QPoint(rightX, MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE));
        painter.drawText(octaveCTextRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         CText);

        // draw white keys
        painter.setPen(palette().color(QPalette::WindowText));
        for(int i=0; i < 7; ++i)
        {
            int noteNumber=midiOctave * 12 + MOUSEPIANO_WHITE_KEY_INDEX_TO_NOTE_INDEX[i];
            if(noteNumber >= MIDI_N_NOTE_NUMBERS)break;

            // display state of MIDI keyboard merged with mouse piano input, so use getKeypressSerialNo
            bool keyPressed=getApp()->getKeypressSerialNo(noteNumber) != 0;

            QRect keyRect=getKeyRect(noteNumber);

            painter.setBrush(keyPressed ? QColor(0xc0,0xc0,0xff) : Qt::white);
            painter.drawRect(keyRect);
        }

        // draw black keys
        painter.setPen(Qt::NoPen);
        for(int i=0; i < 5; ++i)
        {
            int noteNumber=midiOctave * 12 + MOUSEPIANO_BLACK_KEY_INDEX_TO_NOTE_INDEX[i];
            if(noteNumber >= MIDI_N_NOTE_NUMBERS)break;

            // display state of MIDI keyboard merged with mouse piano input, so use getKeypressSerialNo
            bool keyPressed=getApp()->getKeypressSerialNo(noteNumber) != 0;

            QRect keyRect=getKeyRect(noteNumber);

            painter.setBrush(keyPressed ? QColor(0x60,0x60,0xc0) : Qt::black);
            painter.drawRect(keyRect);
        }
    }
}

QColor MousePianoWidget::shadedPaletteColor(float foreground_factor, QColor foregroundColor, QColor backgroundColor)
{
    return QColor(
            static_cast<int>((foreground_factor*foregroundColor.red()   + (1-foreground_factor)*backgroundColor.red())),
            static_cast<int>((foreground_factor*foregroundColor.green() + (1-foreground_factor)*backgroundColor.green())),
            static_cast<int>((foreground_factor*foregroundColor.blue()  + (1-foreground_factor)*backgroundColor.blue())));
}

void MousePianoWidget::hideEvent(QHideEvent* event)
{
    UNUSED(event);

    // cancel any drag operations and switch off all notes
    switch(mouseDragMode)
    {
    case None:
        // Nothing to do
        break;
    case DragOctaveBar:
    case NoteScrub:
        {
            // emulate left button release
            QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                     QPoint(0,0),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            mouseReleaseEvent(&releaseEvent);
            break;
        }
    case ToggleOneNote:
        {
            // emulate right button release
            QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                     QPoint(0,0),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
            mouseReleaseEvent(&releaseEvent);
            break;
        }
    }

    allNotesOff();
}

void MousePianoWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept();

    bool modShift=(event->modifiers() & Qt::ShiftModifier) != 0;

    if(event->button() == Qt::LeftButton)
    {
        if(mouseDragMode != None)return;    // other drag mode active

        MouseZoneResult mouseZone=getMouseZone(event->pos());
        setCursor(mouseZone.cursor);

        switch(mouseZone.zoneType)
        {
        case NoArea:
            break;
        case OctaveBar:
            mouseDragMode=DragOctaveBar;
            mouseDragPreviousValueInt=getLeftWhiteKey();
            mouseDragStartReferencePoint=event->pos();
            break;
        case Key:
            if(modShift)allNotesOff();    // shift-click switches off all currently enabled notes

            mouseDragMode=NoteScrub;
            noteScrubCurrentNoteNumber=mouseZone.noteNumber;

            if(mousePianoKeyPressedArray[noteScrubCurrentNoteNumber])
            {
                // note currently pressed, release it first
                mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=false;
                emit keyReleased(noteScrubCurrentNoteNumber);
            }
            mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=true;
            emit keyPressed(noteScrubCurrentNoteNumber);
            break;
        }
        return;
    }

    if(event->button() == Qt::RightButton)
    {
        if(mouseDragMode != None)return;    // other drag mode active

        MouseZoneResult mouseZone=getMouseZone(event->pos());
        setCursor(mouseZone.cursor);

        if(mouseZone.zoneType == Key)
        {
            if(modShift)allNotesOff();    // shift-click switches off all currently enabled notes

            mouseDragMode=ToggleOneNote;
            noteScrubCurrentNoteNumber=mouseZone.noteNumber;

            // toggle key state
            if(mousePianoKeyPressedArray[noteScrubCurrentNoteNumber])
            {
                mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=false;
                emit keyReleased(noteScrubCurrentNoteNumber);
            }
            else
            {
                mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=true;
                emit keyPressed(noteScrubCurrentNoteNumber);
            }
        }
        return;
    }
}

void MousePianoWidget::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();

    switch(mouseDragMode)
    {
    case None:
        {
            // update cursor shape during tracking (no mouse button pressed)
            MouseZoneResult mouseZone=getMouseZone(event->pos());
            setCursor(mouseZone.cursor);
            break;
        }
    case DragOctaveBar:
        {
            if((event->buttons() & Qt::LeftButton) == 0)
            {
                // mouse button was released without notice (e.g. by showing modal dialog or menu)
                QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                         QPoint(0,0),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                mouseReleaseEvent(&releaseEvent);
                return;
            }

            int relativeX=event->pos().x() - mouseDragStartReferencePoint.x();
            setLeftWhiteKey(-relativeX / whiteKeySize.width() + mouseDragPreviousValueInt);

            clampLeftWhiteKey();
            update();
            break;
        }
    case NoteScrub:
        {
            if((event->buttons() & Qt::LeftButton) == 0)
            {
                // mouse button was released without notice (e.g. by showing modal dialog or menu)
                QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                         QPoint(0,0),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                mouseReleaseEvent(&releaseEvent);
                return;
            }

            // when scrubbing notes, clamp event coordinates to range filled with piano keys
            QPoint p=event->pos();
            if(p.y() < MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE)p.ry()=MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE;

            int minNoteLeftX  = getKeyRect(0).left();
            int maxNoteRightX = getKeyRect(MIDI_N_NOTE_NUMBERS-1).right();

            if(p.x() < minNoteLeftX)p.rx()=minNoteLeftX;
            else if(p.x() >= maxNoteRightX)p.rx()=maxNoteRightX-1;

            MouseZoneResult mouseZone=getMouseZone(p);
            Q_ASSERT(mouseZone.zoneType == Key);

            if(mouseZone.noteNumber != noteScrubCurrentNoteNumber)
            {
                // scrubbed note changed
                mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=false;
                emit keyReleased(noteScrubCurrentNoteNumber);

                noteScrubCurrentNoteNumber=mouseZone.noteNumber;

                if(mousePianoKeyPressedArray[noteScrubCurrentNoteNumber])
                {
                    // new note currently pressed, release it first
                    mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=false;
                    emit keyReleased(noteScrubCurrentNoteNumber);
                }

                mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=true;
                emit keyPressed(noteScrubCurrentNoteNumber);
            }
            break;
        }
    case ToggleOneNote:
        if((event->buttons() & Qt::RightButton) == 0)
        {
            // mouse button was released without notice (e.g. by showing modal dialog or menu)
                QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                         QPoint(0,0),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
                mouseReleaseEvent(&releaseEvent);
            return;
        }
        // otherwise do nothing
        break;
    }
}

void MousePianoWidget::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();

    switch(mouseDragMode)
    {
    case None:
        // nothing to do, return immediately
        return;
    case NoteScrub:
        if(event->button() != Qt::LeftButton)return;

        // release currently pressed note
        mousePianoKeyPressedArray[noteScrubCurrentNoteNumber]=false;
        emit keyReleased(noteScrubCurrentNoteNumber);
        break;
    case DragOctaveBar:
        if(event->button() != Qt::LeftButton)return;
        break;
    case ToggleOneNote:
        if(event->button() != Qt::RightButton)return;
        break;
    }

    // Restore correct cursor
    MouseZoneResult mouseZone=getMouseZone(event->pos());
    setCursor(mouseZone.cursor);

    mouseDragMode=None;
}

void MousePianoWidget::wheelEvent(QWheelEvent* event)
{
    event->accept();

    mouseWheelAccu_8thsOfDegrees+=event->angleDelta().y();
    const int MOUSE_WHEEL_DELTA=120;    // 15 degrees * 8, see documentation for QWheelEvent::delta()

    if(mouseDragMode == None)
    {
        while(mouseWheelAccu_8thsOfDegrees >= MOUSE_WHEEL_DELTA)  // wheel up
        {
            mouseWheelAccu_8thsOfDegrees-=MOUSE_WHEEL_DELTA;
            setLeftWhiteKey(getLeftWhiteKey() + 1);
        }
        while(mouseWheelAccu_8thsOfDegrees <= -MOUSE_WHEEL_DELTA) // wheel down
        {
            mouseWheelAccu_8thsOfDegrees+=MOUSE_WHEEL_DELTA;
            setLeftWhiteKey(getLeftWhiteKey() - 1);
        }

        // Clamp value to allowed range
        clampLeftWhiteKey();
        update();
    }
}

void MousePianoWidget::updateKeyStatus(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);
    update(getKeyRect(noteNumber));
}

void MousePianoWidget::allNotesOff()
{
    for(int noteNumber=0; noteNumber < MIDI_N_NOTE_NUMBERS; ++noteNumber)
    {
        if(mousePianoKeyPressedArray[noteNumber])
        {
            mousePianoKeyPressedArray[noteNumber]=false;
            emit keyReleased(noteNumber);
        }
    }
}

MousePianoWidget::MouseZoneResult MousePianoWidget::getMouseZone(const QPoint& p)
{
    MouseZoneResult result;
    result.zoneType=NoArea;
    result.noteNumber=-1;               // invalidate
    result.cursor=Qt::ArrowCursor;      // default cursor

    int minNoteLeftX  = getKeyRect(0).left();
    int maxNoteRightX = getKeyRect(MIDI_N_NOTE_NUMBERS-1).right();

    if(p.x() >= minNoteLeftX && p.x() < maxNoteRightX)
    {
        if(p.y() < MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE)
        {
            result.zoneType=OctaveBar;
            result.cursor=Qt::SizeHorCursor;
        }
        else
        {
            result.zoneType=Key;
            result.cursor=Qt::PointingHandCursor;
            result.noteNumber=getNoteNumber(p);
        }
    }

    return result;
}

QRect MousePianoWidget::getKeyRect(int noteNumber) const
{
    Q_ASSERT(noteNumber >= 0);

    // determine octave start
    int midiOctave = noteNumber / 12;
    int C_KeyIndex=midiOctave * 7;
    int whiteKeyIndex=MOUSEPIANO_NOTE_INDEX_TO_WHITE_KEY_INDEX[noteNumber % 12] + C_KeyIndex;
    int blackKeyIndex=MOUSEPIANO_NOTE_INDEX_TO_BLACK_KEY_INDEX[noteNumber % 12];

    QRect keyRect;
    keyRect.setTop(MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE);

    if(blackKeyIndex == -1)
    {
        // white key
        keyRect.setLeft((whiteKeyIndex - getLeftWhiteKey()) * whiteKeySize.width());
        keyRect.setWidth(whiteKeySize.width());
        keyRect.setBottom(rect().bottom()); // white key occupies full space down to bottom of widget
    }
    else
    {
        // black key
        QSize blackKeySize((int)(whiteKeySize.width() * MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_X),
                           (int)(whiteKeySize.height() * MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_Y));

        keyRect.setLeft((int)(
                (C_KeyIndex + MOUSEPIANO_BLACK_KEY_POSITION[blackKeyIndex] - getLeftWhiteKey())
                * whiteKeySize.width()
                - blackKeySize.width() / 2));

        keyRect.setWidth(blackKeySize.width());
        keyRect.setHeight(blackKeySize.height());
    }

    return keyRect;
}

int MousePianoWidget::getNoteNumber(const QPoint& p) const
{
    // "inverse function" to getKeyRect, returns -1 or MIDI_N_NOTE_NUMBERS if point out of range

    double whiteKeyIndex=(double)p.x() / whiteKeySize.width() + getLeftWhiteKey();
    int midiOctave=(int)(whiteKeyIndex / 7);

    if(whiteKeyIndex < 0)return -1;    // out of range

    QSize blackKeySize((int)(whiteKeySize.width() * MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_X),
                       (int)(whiteKeySize.height() * MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_Y));

    if(p.y() < MOUSEPIANO_OCTAVE_DRAG_BAR_YSIZE + blackKeySize.height())
    {
        // maybe a black key
        for(int i=0; i < 5; ++i)
        {
            // no G# after note number 127
            if(midiOctave == MIDI_MAX_OCTAVE && i == 3)break;

            if(whiteKeyIndex - midiOctave*7 > MOUSEPIANO_BLACK_KEY_POSITION[i] - MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_X/2 &&
               whiteKeyIndex - midiOctave*7 < MOUSEPIANO_BLACK_KEY_POSITION[i] + MOUSEPIANO_BLACK_KEY_RELATIVE_SIZE_X/2)
            {
                // on black key
                int noteNumber=midiOctave * 12 + MOUSEPIANO_BLACK_KEY_INDEX_TO_NOTE_INDEX[i];
                return qMin(noteNumber,MIDI_N_NOTE_NUMBERS);
            }
        }
    }

    // white key
    int noteNumber=midiOctave * 12 + MOUSEPIANO_WHITE_KEY_INDEX_TO_NOTE_INDEX[((int)whiteKeyIndex)%7];
    return qMin(noteNumber,MIDI_N_NOTE_NUMBERS);
}
