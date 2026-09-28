/***************************************************************************
 *  smfdocument.h - Standard MIDI File Parser and Writer
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

#ifndef SMFDOCUMENT_H
#define SMFDOCUMENT_H

#include <QList>

class SmfTrack;
class SmfEvent;
class SmfMidiEvent;
class SmfMetaEvent;
class SmfSysExEvent;
class QIODevice;

#define SMF_META_EVENT_TYPE_SEQ_NUMBER          0x00    // not shiftable
#define SMF_META_EVENT_TYPE_TEXT                0x01    // valid XML: handled explicitly; otherwise: shiftable
#define SMF_META_EVENT_TYPE_COPYRIGHT           0x02    // not shiftable
#define SMF_META_EVENT_TYPE_TRACK_NAME          0x03    // handled explicitly
#define SMF_META_EVENT_TYPE_INSTRUMENT_NAME     0x04    // not shiftable
#define SMF_META_EVENT_TYPE_LYRICS              0x05    // shiftable
#define SMF_META_EVENT_TYPE_MARKER              0x06    // shiftable, EXTENSION: use as track local marker
#define SMF_META_EVENT_TYPE_CUE_POINT           0x07    // shiftable
#define SMF_META_EVENT_TYPE_CHANNEL_PREFIX      0x20    // handled explicitly
#define SMF_META_EVENT_TYPE_END_OF_TRACK        0x2f    // SMF serialization layer only
#define SMF_META_EVENT_TYPE_TEMPO               0x51    // conductor track only, handled explicitly
#define SMF_META_EVENT_TYPE_SMPTE_OFFSET        0x54    // conductor track only, not shiftable
#define SMF_META_EVENT_TYPE_TIME_SIGNATURE      0x58    // conductor track only, handled explicitly
#define SMF_META_EVENT_TYPE_KEY_SIGNATURE       0x59    // conductor track only, handled explicitly
#define SMF_META_EVENT_TYPE_SEQUENCER_SPECIFIC  0x7f    // conductor track only, not shiftable

class SmfDocument
{
public:
    SmfDocument(QIODevice* smfFile);
    ~SmfDocument();

    bool load();
    bool save() const;

    int getFormatTag() const;
    void convertToFormat1(bool createTrackNames);
    bool hasMixedChannelsInTrack() const;

    void setMidiTicksPerWholeNote(int midiTicksPerWholeNote)
    {
        division = midiTicksPerWholeNote / 4;  // division = midi ticks per QUARTER note
    }
    int getMidiTicksPerWholeNote() const
    {
        return division * 4;                   // division = midi ticks per QUARTER note
    }
    static bool eventTicksLessThan(SmfEvent* e1, SmfEvent* e2);

    QList<SmfTrack*> trackList;

protected:
    struct SmfHeaderType
    {
        quint32 length;
        quint32 formatTag;
        quint32 numberOfTracks;
        quint32 division;
    };

    QIODevice* file;

    quint32 formatTag;
    quint32 division;

    bool skipRiffHeader() const;

    bool readID(const char* requiredID) const;
    bool readVarLong(quint32& value) const;
    bool readLong(quint32& value) const;
    bool readLongLittleEndian(quint32& value) const;
    bool readShort(quint32& value) const;
    bool readByte(quint8& value) const;

    bool writeID(const char* ID) const;
    bool writeVarLong(quint32 value) const;
    bool writeLong(quint32 value) const;
    bool writeShort(quint32 value) const;
    bool writeByte(quint8 value) const;
};

class SmfTrack
{
public:
    ~SmfTrack();

    QList<SmfEvent*> eventList;
};

enum SmfEventType { ET_Midi, ET_Meta, ET_SysEx };

class SmfEvent
{
public:
    virtual ~SmfEvent() {}
    virtual SmfEventType type() const=0;

    // filter functions
    SmfMidiEvent* isMidiEvent() const;
    SmfMetaEvent* isMetaEvent() const;
    SmfMetaEvent* isMetaEventOfType(quint8 requiredMetaEventType) const;
    SmfSysExEvent* isSysExEvent() const;

    quint32 tickPosition;   // absolute ticks from track start

};

class SmfMidiEvent : public SmfEvent
{
public:
    SmfMidiEvent()
    {
        midiCommand[0]=0;
        midiCommand[1]=0;
        midiCommand[2]=0;
    }
    virtual SmfEventType type() const { return ET_Midi; }
    int length() const;

    quint8 midiCommand[3];
};

class SmfSysExEvent : public SmfEvent
{
public:
    SmfSysExEvent()
    {
        sysExType=0xff;
        dataLength=0;
        data=NULL;
    }
    SmfSysExEvent(const SmfSysExEvent& rhs)
    {
        data=NULL;
        operator=(rhs);
    }
    virtual ~SmfSysExEvent()
    {
        delete[] data;
    }
    virtual SmfEventType type() const { return ET_SysEx; }

    SmfSysExEvent& operator=(const SmfSysExEvent& rhs) {
        if(&rhs == this)return *this;

        SmfEvent::operator=(rhs);

        delete[] data; data=NULL;

        sysExType               =   rhs.sysExType;
        dataLength              =   rhs.dataLength;

        data=new quint8[dataLength];
        memcpy(data,rhs.data,dataLength);

        return *this;
    }

    quint8 sysExType;   // 0xf0, 0xf7
    int dataLength;
    quint8* data;
};

class SmfMetaEvent : public SmfEvent
{
public:
    SmfMetaEvent()
    {
        metaEventType=0xff;
        dataLength=0;
        data=NULL;
    }
    SmfMetaEvent(const SmfMetaEvent& rhs)
    {
        data=NULL;
        operator=(rhs);
    }
    virtual ~SmfMetaEvent()
    {
        delete[] data;
    }
    SmfMetaEvent& operator=(const SmfMetaEvent& rhs) {
        if(&rhs == this)return *this;

        SmfEvent::operator=(rhs);

        delete[] data; data=NULL;

        metaEventType           =   rhs.metaEventType;
        dataLength              =   rhs.dataLength;

        data=new quint8[dataLength];
        memcpy(data,rhs.data,dataLength);

        return *this;
    }
    bool operator!=(const SmfMetaEvent& rhs) const
    {
        if(metaEventType != rhs.metaEventType ||
           dataLength    != rhs.dataLength)return true;

        if(data == NULL && rhs.data != NULL)return true;
        if(data != NULL && rhs.data == NULL)return true;
        if(data != NULL && rhs.data != NULL)
            return memcmp(data,rhs.data,dataLength) != 0;

        return false;
    }
    void serialize(QDataStream& dataStream) const;    // copy to clipboard
    void deserialize(QDataStream& dataStream);  // paste from clipboard

    void scaleTickResolution(int newResolution, int oldResolution);

    virtual SmfEventType type() const { return ET_Meta; }
    QString dataToString();
    void dataFromString(const QString& s);

    quint8 metaEventType;
    int dataLength;
    quint8* data;
};

#endif // SMFDOCUMENT_H
