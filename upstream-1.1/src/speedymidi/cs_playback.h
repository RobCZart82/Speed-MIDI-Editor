/***************************************************************************
 *  cs_playback.h - Controller Subsystem: Playback
 *                  (StreamPlayback, ListenChord, Stop, Track Flags)
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

#ifndef CS_PLAYBACK_H
#define CS_PLAYBACK_H

#include "cs_common.h"
#include "midiinterface.h"

class CS_Playback : public CS_Common
{
    Q_OBJECT
public:
    CS_Playback(Controller* controller);

    // editing is partly disabled during playback
    enum PlaybackModeType { PBM_None, PBM_Stream, PBM_ImmediateListen };
    PlaybackModeType getPlaybackMode() const { return playbackMode; }

    virtual bool keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual bool mousePressEvent(QMouseEvent* event, const View::MouseZoneResult& mouseZone);
    virtual bool wheelEvent(QWheelEvent* event);
    virtual void timerEvent(QTimerEvent* timerEvent);
    virtual void stateChanged();
    virtual void cancelInterruptibleState();
    virtual void cancelInputCapture();

protected slots:
    void actionPlayback_Stop_Triggered();
    void actionPlayback_Play_Triggered();
    void actionPlayback_ListenChord_Triggered();
    void actionPlayback_SetPlaybackSpeed();
    void actionPlayback_TrackFlags_ToggleSolo_Triggered();
    void actionPlayback_TrackFlags_ToggleMute_Triggered();
    void actionPlayback_TrackFlags_ToggleSoloExclusive_Triggered();
    void actionPlayback_TrackFlags_ToggleMuteExclusive_Triggered();

    void spinBoxRelativePlaybackSpeedValueChanged(int value);

    void midiStreamFinished();

protected:

    void startStreamPlayback();
    void startImmediateListenPlayback(bool makeChecks);
    void stopPlayback();

    bool checkForChannelCollisions();
    void setPlaybackMuteConfiguration();
    void setPlaybackMuteConfiguration_Live();
    void convertTrackToShortMessages(int trackIndex, QList<MidiShortMsg>& msgList);
    static bool eventPlaybackOrderingLessThan(SmfExporterMidiEvent* e1, SmfExporterMidiEvent* e2);

    int ticksToTimestamp(int ticks);
    int timestampToTicks(int timestamp);

    PlaybackModeType playbackMode;
    int playbackStartTicks;

    struct TimestampTranslationTableEntry
    {
        int ticksLeft;
        int ticksRight;     // last entry: ticksRight == INT_MAX
        int timestampLeft;
        int timestampRight;
        double relativeTicksToTimestampFactor;
    };
    QList<TimestampTranslationTableEntry> timestampTranslationTable;
    int timestampTranslationTableIndexCache;

    bool playbackPositionUpdateTimerActive;
    int playbackPositionUpdateTimerId;
};

#endif // CS_PLAYBACK_H
