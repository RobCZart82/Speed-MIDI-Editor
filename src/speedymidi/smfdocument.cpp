/***************************************************************************
 *  smfdocument.cpp - Standard MIDI File Parser and Writer
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

#include "smfdocument.h"
#include <algorithm>
#include <QFile>
#include <QDataStream>
#include <QApplication>

SmfDocument::SmfDocument(QFile* smfFile)
{
    this->file=smfFile;

    formatTag=1;    // default: format 1 (multi-track sequence)
    division=480;   // usual default in Standard MIDI files
}

SmfDocument::~SmfDocument()
{
    for(int i=0; i < trackList.size(); ++i) delete trackList[i];
    trackList.clear();
}

bool SmfDocument::load()
{
    // check for an RIFF/RMID document
    if(!skipRiffHeader())return false;

    // read header data
    SmfHeaderType smfHeader;
    if(!readID   ("MThd"                  ) ||
       !readLong (smfHeader.length        ) ||
       !readShort(smfHeader.formatTag     ) ||
       !readShort(smfHeader.numberOfTracks) ||
       !readShort(smfHeader.division      )
        )
        return false;

    if(smfHeader.division & 0x8000)
        return false;   // SMPTE format not recognized

    formatTag=smfHeader.formatTag;
    division=smfHeader.division;

    // read individual tracks
    for(quint32 trackIndex=0; trackIndex < smfHeader.numberOfTracks; ++trackIndex)
    {
        // read track header
        quint32 trackLength;

        if(!readID   ("MTrk"     ) ||
           !readLong (trackLength)
            )
            return false;

        quint32 nextTrackStartPos=file->pos() + trackLength;

        // create new track object
        SmfTrack* smfTrack=new SmfTrack;
        trackList.append(smfTrack);

        // reset SMF running status
        quint8 runningStatus=0;
        quint32 previousMidiEventLength=0;

        // reset time to zero
        quint32 absoluteTicks=0;

        // read individual track events
        while(file->pos() < nextTrackStartPos)
        {
            // read delta time and SMF event type.

            quint32 deltaTicks;
            quint8 eventType;

            if(!readVarLong(deltaTicks))return false;
            if(!readByte   (eventType ))return false;

            // advance in time
            absoluteTicks += deltaTicks;

            // distinguish between different event types

            if(eventType == 0xf0 || eventType == 0xf7)  // SysEx
            {
                quint32 dataLength;
                if(!readVarLong(dataLength))return false;

                // plausibility check
                if(dataLength >= file->size())return false;

                quint8* sysExData=new quint8[dataLength];
                if(sysExData == NULL)return false;

                if(file->read((char*)sysExData,dataLength) != dataLength)  { delete[] sysExData;return false; }

                if(eventType == 0xf0)   // SysEx type 0xf0 needs an EOX byte with value 0xf7 at the end
                {
                    if(dataLength < 1)                    { delete[] sysExData;return false; }
                    if(sysExData[dataLength - 1] != 0xf7) { delete[] sysExData;return false; }
                }

                SmfSysExEvent* event=new SmfSysExEvent;
                event->tickPosition=absoluteTicks;
                event->sysExType=eventType;
                event->dataLength=dataLength;
                event->data=sysExData;
                smfTrack->eventList.append(event);
            }
            else if(eventType == 0xff)  // meta-event
            {
                quint8 metaEventType;
                if(!readByte(metaEventType))
                    return false;

                quint32 dataLength;
                if(!readVarLong(dataLength))
                    return false;

                // plausibility check
                if(dataLength >= file->size())return false;

                if(metaEventType == SMF_META_EVENT_TYPE_END_OF_TRACK)
                    break;  // end of track

                quint8* metaData=NULL;
                if(dataLength > 0)
                {
                    metaData=new quint8[dataLength];
                    if(metaData == NULL)
                        return false;

                    if(file->read((char*)metaData,dataLength) != dataLength)  { delete metaData;return false; }
                }

                SmfMetaEvent* event=new SmfMetaEvent;
                event->tickPosition=absoluteTicks;
                event->metaEventType=metaEventType;
                event->dataLength=dataLength;
                event->data=metaData;
                smfTrack->eventList.append(event);
            }
            else if(eventType < 0x80)   // MIDI event using running status
            {
                // use SMF running status as first MIDI event data byte
                quint8 DataByte1=eventType;
                quint8 DataByte2=0;

                if(previousMidiEventLength < 2 || previousMidiEventLength > 3)
                    return false; // format error

                if(previousMidiEventLength == 3)
                {
                    // read 2nd data byte
                    if(!readByte(DataByte2))
                        return false;
                }

                SmfMidiEvent* event=new SmfMidiEvent;
                event->tickPosition=absoluteTicks;
                event->midiCommand[0]=runningStatus;    // use running status
                event->midiCommand[1]=DataByte1;
                event->midiCommand[2]=DataByte2;
                smfTrack->eventList.append(event);
            }
            else    // normal MIDI event w/o running status
            {
                switch(eventType)
                {
                case 0xf4:
                case 0xf5:
                case 0xf9:
                case 0xfd:
                    return false;   // undefined event
                }

                SmfMidiEvent* event=new SmfMidiEvent;
                smfTrack->eventList.append(event);

                event->tickPosition=absoluteTicks;
                event->midiCommand[0]=eventType;
                if(event->length() >= 2)
                {
                    if(!readByte(event->midiCommand[1]))
                        return false;
                }
                else event->midiCommand[1]=0;
                if(event->length() >= 3)
                {
                    if(!readByte(event->midiCommand[2]))
                        return false;
                }
                else event->midiCommand[2]=0;

                // set new SMF running status
                runningStatus=eventType;
                previousMidiEventLength=event->length();
            }
        }

        // advance to next track
        file->seek(nextTrackStartPos);
    }
    return true;
}

bool SmfDocument::save() const
{
    // fill header
    SmfHeaderType smfHeader;
    smfHeader.length=6;
    smfHeader.formatTag=1;  // write only in format 1: multi-track sequence
    smfHeader.numberOfTracks=trackList.size();
    smfHeader.division=division;

    // write header
    if(!writeID   ("MThd"                  ) ||
       !writeLong (smfHeader.length        ) ||
       !writeShort(smfHeader.formatTag     ) ||
       !writeShort(smfHeader.numberOfTracks) ||
       !writeShort(smfHeader.division      )
        )
        return false;

    // write individual tracks
    for(quint32 trackIndex=0; trackIndex < (quint32)trackList.size(); ++trackIndex)
    {
        // write track header
        if(!writeID("MTrk"))return false;

        // length is unknown so far and will be filled in later
        quint32 trackLength=0;
        quint32 trackLengthFieldPos=file->pos();
        if(!writeLong(trackLength))return false;

        quint32 currentTrackStartPos=file->pos();

        // reset SMF running status
        quint8 runningStatus=0;

        // reset time to zero
        quint32 absoluteTicks=0;

        SmfTrack* SmfTrack=trackList[trackIndex];
        for(int eventIndex=0; eventIndex < SmfTrack->eventList.size(); ++eventIndex)
        {
            SmfEvent* event=SmfTrack->eventList[eventIndex];

            // write delta ticks
            Q_ASSERT(event->tickPosition >= absoluteTicks);   // events must have been sorted for tickPosition
            quint32 deltaTicks=event->tickPosition - absoluteTicks;

            if(!writeVarLong(deltaTicks))return false;

            absoluteTicks=event->tickPosition;

            switch(event->type())
            {
            case ET_SysEx:
                {
                    SmfSysExEvent* sysExEvent=(SmfSysExEvent*)event;
                    if(!writeByte   (sysExEvent->sysExType ))return false;
                    if(!writeVarLong(sysExEvent->dataLength))return false;
                    if(file->write((char*)sysExEvent->data,sysExEvent->dataLength) != sysExEvent->dataLength)
                        return false;

                    // reset SMF running status
                    runningStatus=0;
                }
                break;

            case ET_Meta:
                {
                    SmfMetaEvent* metaEvent=(SmfMetaEvent*)event;
                    if(!writeByte   (0xff                    ))return false;
                    if(!writeByte   (metaEvent->metaEventType))return false;
                    if(!writeVarLong(metaEvent->dataLength   ))return false;
                    if(file->write((char*)metaEvent->data,metaEvent->dataLength) != metaEvent->dataLength)
                        return false;

                    // reset SMF running status
                    runningStatus=0;
                }
                break;

            case ET_Midi:
                {
                    SmfMidiEvent* midiEvent=(SmfMidiEvent*)event;

                    // use compression by running status byte when possible
                    if(midiEvent->length() >= 2 && midiEvent->midiCommand[0] == runningStatus)
                    {
                        // write data bytes only (start at index 1)
                        for(int i=1; i < midiEvent->length(); ++i)
                            if(!writeByte(midiEvent->midiCommand[i]))return false;
                    }
                    else    // use of running status not possible
                    {
                        // write status bytes and required data bytes
                        for(int i=0; i < midiEvent->length(); ++i)
                            if(!writeByte(midiEvent->midiCommand[i]))return false;
                    }
                }
                break;
            }
        }

        // write end-of-track meta event
        if(!writeByte(0)                               )return false;   // varlong delta ticks = 0
        if(!writeByte(0xff)                            )return false;   // event type = meta event
        if(!writeByte(SMF_META_EVENT_TYPE_END_OF_TRACK))return false;   // meta event type
        if(!writeByte(0)                               )return false;   // data length

        // determine actual track length
        quint32 nextTrackStartPos=file->pos();
        trackLength=nextTrackStartPos - currentTrackStartPos;

        // fill in track length into track header
        file->seek(trackLengthFieldPos);
        writeLong(trackLength);
        file->seek(nextTrackStartPos);
    }
    return true;
}

int SmfDocument::getFormatTag() const
{
    if(trackList.size() <= 1)
        return 0;   // if there is 0 or 1 track, override the format setting in the header to 0.
    else
        return formatTag;
}

void SmfDocument::convertToFormat1(bool createTrackNames)
{
    // Traverse all tracks (there should be only 1, but no matter) and all events.
    //  Sort events into 1 conductor track and 16 tracks for the channels.
    //  If one of the tracks for a channel remains empty, delete it later.

    // Remember format 0 track list
    QList<SmfTrack*> oldTrackList(trackList);

    // Create new track list
    trackList.clear();
    for(int i=0; i < 1 + 16; ++i)
        trackList.append(new SmfTrack);

    QString songName;

    while(!oldTrackList.isEmpty())
    {
        SmfTrack* oldTrack=oldTrackList.takeFirst();

        while(!oldTrack->eventList.isEmpty())
        {
            SmfEvent* event=oldTrack->eventList.takeFirst();

            switch(event->type())
            {
            case ET_SysEx:
                // SysEx messages are not channel-based, put them into the conductor track
                trackList[0]->eventList.append(event);
                break;
            case ET_Meta:
                // meta messages are not channel-based, put them into the conductor track
                {
                    SmfMetaEvent* metaEvent=(SmfMetaEvent*)event;
                    if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_TRACK_NAME)
                    {
                        // track name in format 0 => song name, but allowed only once
                        if(songName.isEmpty())songName=metaEvent->dataToString();

                        // delete all track/song name events
                        delete event;
                    }
                    else trackList[0]->eventList.append(event);
                }
                break;
            case ET_Midi:
                {
                    SmfMidiEvent* midiEvent=(SmfMidiEvent*)event;
                    if(midiEvent->midiCommand[0] >= 0xf0)
                    {
                        // system messages are not channel-based, put them into the conductor track
                        trackList[0]->eventList.append(event);
                    }
                    else
                    {
                        // channel-based message
                        Q_ASSERT(midiEvent->midiCommand[0] >= 0x80);
                        int channel=midiEvent->midiCommand[0] & 0xf;
                        trackList[channel + 1]->eventList.append(event);
                    }
                }
                break;
            }
        }

        delete oldTrack;
    }

    songName=songName.trimmed();

    // delete unused tracks for channels
    for(int trackIndex=1; trackIndex < trackList.size(); )
    {
        SmfTrack* SmfTrack=trackList[trackIndex];
        if(SmfTrack->eventList.size() == 0)
        {
            trackList.removeAt(trackIndex);
            delete SmfTrack;
        }
        else ++trackIndex;
    }

    if(createTrackNames)
    {
        // Create names for each channel-track based on the channel of the first MIDI event.
        //  Each track must contain such a channel-based event, because all other tracks have just been deleted.
        for(int trackIndex=1; trackIndex < trackList.size(); ++trackIndex)
        {
            SmfTrack* SmfTrack=trackList[trackIndex];
            SmfEvent* event=SmfTrack->eventList[0];
            Q_ASSERT(event->type() == ET_Midi);
            SmfMidiEvent* midiEvent=(SmfMidiEvent*)event;

            // generate track name from song name and 1-based channel index
            int channel=midiEvent->midiCommand[0] & 0xf;
            QString trackName;

            if(songName.isEmpty())
                trackName=QApplication::translate("SmfDocument","Channel %1").arg(channel + 1);
            else
                trackName=QApplication::translate("SmfDocument","%1 channel %2").arg(songName).arg(channel + 1);

            SmfMetaEvent* trackNameMetaEvent=new SmfMetaEvent;
            trackNameMetaEvent->tickPosition=0;
            trackNameMetaEvent->metaEventType=SMF_META_EVENT_TYPE_TRACK_NAME;     // track name
            trackNameMetaEvent->dataLength=trackName.length();
            trackNameMetaEvent->data=new quint8[trackNameMetaEvent->dataLength];

            memcpy(trackNameMetaEvent->data,
                   trackName.toLocal8Bit().constData(),
                   trackNameMetaEvent->dataLength);

            SmfTrack->eventList.prepend(trackNameMetaEvent);
        }
    }

    // stable-sort events in the new tracks
    for(int trackIndex=0; trackIndex < trackList.size(); ++trackIndex)
    {
        SmfTrack* SmfTrack=trackList[trackIndex];
        std::stable_sort(SmfTrack->eventList.begin(),SmfTrack->eventList.end(),eventTicksLessThan);
    }

    // format has changed
    formatTag=1;
}

bool SmfDocument::hasMixedChannelsInTrack() const
{
    for(int trackIndex=0; trackIndex < trackList.size(); ++trackIndex)
    {
        SmfTrack* SmfTrack=trackList[trackIndex];

        int usedChannel=-1;     // -1 means none so far

        for(int eventIndex=0; eventIndex < SmfTrack->eventList.size(); ++eventIndex)
        {
            SmfEvent* event=SmfTrack->eventList[eventIndex];

            if(event->type() == ET_Meta)
            {
                // check for meta event "channel prefix"
                SmfMetaEvent* metaEvent=(SmfMetaEvent*)event;
                if(metaEvent->metaEventType == SMF_META_EVENT_TYPE_CHANNEL_PREFIX)
                    break;  // this channel uses always the channel given by the data byte of this meta event
            }
            if(event->type() == ET_Midi)
            {
                SmfMidiEvent* midiEvent=(SmfMidiEvent*)event;
                if(midiEvent->midiCommand[0] < 0xf0)
                {
                    int channel=midiEvent->midiCommand[0] & 0xf;
                    if(usedChannel >= 0 && usedChannel != channel)
                        return true;    // found multiple channels per track

                    usedChannel=channel;
                }
            }
        }
    }
    return false;
}

bool SmfDocument::skipRiffHeader() const
{
    // check for an RIFF/RMID document
    if(readID("RIFF"))
    {
        // Found RIFF header.
        // All 32-bit lengths are little endian (vs. SMF: big endian) and are never checked.

        quint32 riffLength;
        if(!readLongLittleEndian(riffLength))return false;

        if(!readID("RMID"))return false;
        if(!readID("data"))return false;

        quint32 dataChunkLength;
        if(!readLongLittleEndian(dataChunkLength))return false;

        return true;
    }
    else
    {
        // no RIFF header, try again using normal MThd format
        file->seek(0);
    }
    return true;
}

bool SmfDocument::readID(const char* requiredID) const
{
    int idLength=strlen(requiredID);
    quint8* data=new quint8[idLength];

    int bytesRead=file->read((char*)data,idLength);

    if(bytesRead == idLength)
    {
        if(memcmp(data,requiredID,idLength) == 0)
        {
            delete[] data;
            return true;
        }
    }

    // error
    delete[] data;
    return false;
}

bool SmfDocument::readVarLong(quint32& value) const
{
    value=0;

    while(true)
    {
        quint8 data;
        if(file->read((char*)&data,1) != 1)return false;

        // add 7 value bits to result
        value <<= 7;
        value += data & 0x7f;

        if((data & 0x80) == 0)
            return true;    // this was the last byte
    }
}


bool SmfDocument::readLong(quint32& value) const
{
    quint8 data[4];
    if(file->read((char*)data,4) != 4)return false;

    value=  ((quint32)(*(data    )) << 24) +    // swap byte order
            ((quint32)(*(data + 1)) << 16) +
            ((quint32)(*(data + 2)) <<  8) +
            ((quint32)(*(data + 3))      );

    return true;
}

bool SmfDocument::readLongLittleEndian(quint32& value) const
{
    quint8 data[4];
    if(file->read((char*)data,4) != 4)return false;

    value=  ((quint32)(*(data + 3)) << 24) +
            ((quint32)(*(data + 2)) << 16) +
            ((quint32)(*(data + 1)) <<  8) +
            ((quint32)(*(data    ))      );

    return true;
}

bool SmfDocument::readShort(quint32& value) const     // value obtained has still 32 bits
{
    quint8 data[2];
    if(file->read((char*)data,2) != 2)return false;

    value=  ((quint32)(*(data    )) <<  8) +    // swap byte order
            ((quint32)(*(data + 1))      );

    return true;
}

bool SmfDocument::readByte(quint8& value) const
{
    quint8 data;
    if(file->read((char*)&data,1) != 1)return false;

    value=data;
    return true;
}

bool SmfDocument::writeID(const char* ID) const
{
    int idLength=strlen(ID);
    int bytesWritten=file->write(ID,idLength);
    return bytesWritten == idLength;
}

bool SmfDocument::writeVarLong(quint32 value) const
{
    // encode 7 bits per byte

    quint8 data[5];
    for(int i=0; i < 5; ++i)
    {
        data[i]=value & 0x7f;
        value >>= 7;
    }

    // big endian order: Calculate index of most significant byte
    int msb=4;
    while(msb > 0)
    {
        if(data[msb] != 0)break;
        --msb;
    }

    // all but the last byte have bit 7 set (big endian order)
    for(int i=msb; i >= 1; --i)
        data[i] |= 0x80;

    // write all bytes in big endian order
    for(int i=msb; i >= 0; --i)
        if(file->write((char*)(data + i),1) != 1)return false;

    return true;
}

bool SmfDocument::writeLong(quint32 value) const
{
    quint8 data[4];
    data[0]=(quint8)(value >> 24);  // swap byte order
    data[1]=(quint8)(value >> 16);
    data[2]=(quint8)(value >>  8);
    data[3]=(quint8)(value      );

    return file->write((char*)data,4) == 4;
}

bool SmfDocument::writeShort(quint32 value) const
{
    quint8 data[4];
    data[0]=(quint8)(value >>  8);  // swap byte order
    data[1]=(quint8)(value      );

    return file->write((char*)data,2) == 2;
}

bool SmfDocument::writeByte(quint8 value) const
{
    return file->write((char*)&value,1) == 1;
}

bool SmfDocument::eventTicksLessThan(SmfEvent* e1, SmfEvent* e2)
{
    // first criterion: tick position
    if(e1->tickPosition < e2->tickPosition)return true;
    if(e1->tickPosition > e2->tickPosition)return false;

    // same tick position: Stable-sort will recognize always true as "same" and will not change the order.
    return true;
}

SmfTrack::~SmfTrack()
{
    for(int i=0; i < eventList.size(); ++i) delete eventList[i];
    eventList.clear();
}

SmfMidiEvent* SmfEvent::isMidiEvent() const
{
    if(type() == ET_Midi)
        return (SmfMidiEvent*)this;
    else
        return NULL;
}

SmfMetaEvent* SmfEvent::isMetaEvent() const
{
    if(type() == ET_Meta)
        return (SmfMetaEvent*)this;
    else
        return NULL;
}

SmfMetaEvent* SmfEvent::isMetaEventOfType(quint8 requiredMetaEventType) const
{
    if(type() == ET_Meta)
    {
        SmfMetaEvent* metaEvent=(SmfMetaEvent*)this;
        if(metaEvent->metaEventType == requiredMetaEventType)return metaEvent;
    }
    return NULL;
}
SmfSysExEvent* SmfEvent::isSysExEvent() const
{
    if(type() == ET_Midi)
        return (SmfSysExEvent*)this;
    else
        return NULL;
}

int SmfMidiEvent::length() const
{
    switch(midiCommand[0])
    {
    case 0xf0:return 0;  // SysEx
    case 0xf1:return 2;  // MTC
    case 0xf2:return 1;  // Song position
    case 0xf3:return 2;  // Song select
    case 0xf4:return 0;  // undefined
    case 0xf5:return 0;  // undefined
    case 0xf6:return 1;  // Tune request
    case 0xf7:return 0;  // SysEx EOX
    case 0xf8:return 1;  // RealTime Timing clock
    case 0xf9:return 0;  // undefined
    case 0xfa:return 1;  // RealTime Start
    case 0xfb:return 1;  // RealTime Continue
    case 0xfc:return 1;  // RealTime Stop
    case 0xfd:return 0;  // undefined
    case 0xfe:return 1;  // RealTime Active Sensing
    case 0xff:return 1;  // RealTime System Reset

    default:
        if(midiCommand[0] < 0x80)return 0; // illegal MIDI command, length unknown
        if(midiCommand[0] < 0xc0)return 3; // 8x note-off, 9x note-on, Ax polyphonic aftertouch, Bx control change
        if(midiCommand[0] < 0xe0)return 2; // Cx Program change, Dx channel pressure
        return 3;                          // Ex Pitch wheel
    }
}

void SmfMetaEvent::serialize(QDataStream& dataStream) const
{
    // copy properties to clipboard

    dataStream << tickPosition;
    dataStream << metaEventType;
    dataStream << dataLength;
    dataStream.writeRawData((const char*)data, dataLength);
}

void SmfMetaEvent::deserialize(QDataStream& dataStream)
{
    // paste properties from clipboard

    delete data; data=NULL;

    dataStream >> tickPosition;
    dataStream >> metaEventType;
    dataStream >> dataLength;

    data=new quint8[dataLength];
    dataStream.readRawData((char*)data, dataLength);
}

void SmfMetaEvent::scaleTickResolution(int newResolution, int oldResolution)
{
    // use 64 bit to retain full integer precision
    tickPosition = (int)((qint64)tickPosition * newResolution / oldResolution);
}

QString SmfMetaEvent::dataToString()
{
    QString s;
    for(int i=0; i < dataLength; ++i)
        s+=(char)data[i];
    return s;
}

void SmfMetaEvent::dataFromString(const QString& s)
{
    delete data;

    dataLength=s.length();
    data=new quint8[dataLength];

    for(int i=0; i < dataLength; ++i)
        data[i]=(quint8)s[i].toLatin1();
}
