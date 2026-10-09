#include "smfdocument.h"
#include "smfimporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include "doc_measureitem.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <QMap>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static QByteArray smfBytes(const QByteArray& events, int ppqn=480) {
    QByteArray bytes=QByteArray::fromHex("4d5468640000000600000001");
    QDataStream stream(&bytes,QIODevice::Append);
    stream << quint16(ppqn);
    bytes += "MTrk";
    QDataStream length(&bytes,QIODevice::Append);
    length << quint32(events.size());
    return bytes+events;
}
static QByteArray multiTrackSmfBytes(const QList<QByteArray>& tracks, int format, int ppqn=480) {
    QByteArray bytes=QByteArray::fromHex("4d54686400000006");
    QDataStream header(&bytes,QIODevice::Append);
    header << quint16(format) << quint16(tracks.size()) << quint16(ppqn);
    for(const QByteArray& track : tracks) {
        bytes += "MTrk";
        QDataStream length(&bytes,QIODevice::Append); length << quint32(track.size());
        bytes += track;
    }
    return bytes;
}
static void appendDelta(QByteArray& bytes, quint32 delta) {
    QByteArray encoded(1,char(delta & 0x7f));
    while((delta >>= 7) != 0)encoded.prepend(char((delta & 0x7f) | 0x80));
    bytes += encoded;
}
static QByteArray tempoTrack(const QList<QPair<int,int>>& tempos, bool mixed=false) {
    QByteArray bytes;
    if(mixed)bytes=QByteArray::fromHex("00903c64");
    int previousTick=0;
    for(const auto& tempo : tempos) {
        appendDelta(bytes,quint32(tempo.first-previousTick));
        bytes += QByteArray::fromHex("ff5103");
        bytes += char((tempo.second >> 16) & 0xff);
        bytes += char((tempo.second >> 8) & 0xff);
        bytes += char(tempo.second & 0xff);
        previousTick=tempo.first;
    }
    appendDelta(bytes,quint32(4800-previousTick));
    if(mixed)bytes += QByteArray::fromHex("803c0000");
    bytes += QByteArray::fromHex("ff2f00");
    return bytes;
}
static QList<QPair<int,int>> savedTempos(QByteArray bytes) {
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument smf(&input); CHECK(smf.load());
    QList<QPair<int,int>> tempos;
    for(const SmfTrack* track : smf.trackList)
        for(const SmfEvent* event : track->eventList)
            if(const SmfMetaEvent* tempo=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TEMPO)) {
                CHECK(tempo->dataLength==3);
                tempos.append(qMakePair(int(tempo->tickPosition),
                    (int(tempo->data[0]) << 16) | (int(tempo->data[1]) << 8) | int(tempo->data[2])));
            }
    std::stable_sort(tempos.begin(),tempos.end(),[](const auto& left,const auto& right) {
        return left.first < right.first;
    });
    return tempos;
}
static void checkExactTempoRoundtrips() {
    // Include fractional BPM, sub-measure positions, both valid SMF extremes,
    // and consecutive tempos at the same tick. Their order is observable.
    const QList<QPair<int,int>> expected={
        {0,497925},{0,497926},{240,500001},{480,486003},{480,497926},
        {720,0xffffff},{1919,1},{1920,500002},{2400,600003}};
    for(int layout : {0,1,2}) {
        const int format=layout==0 ? 0 : 1;
        for(bool saveEditorState : {false,true}) {
            QList<QByteArray> tracks={tempoTrack(expected,layout!=1)};
            if(layout==1)tracks.append(QByteArray::fromHex("00903c64a540803c0000ff2f00"));
            QByteArray bytes=multiTrackSmfBytes(tracks,format);
            for(int cycle=0;cycle<3;++cycle) {
                QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
                SmfDocument source(&input); CHECK(source.load());
                DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
                CHECK(importer.doImport());
                CHECK(doc.measureToTicks(1)==1920 && doc.measureToTicks(2)==3840);
                CHECK(doc.ticksToMeasure(1919).measureIndex==0);
                CHECK(doc.ticksToMeasure(1920).measureIndex==1);
                const DocMeasureItem* at480=doc.getMeasureItemAtExact(480);
                CHECK(at480 && at480->microsecondsPerQuarter==497926);
                CHECK(at480->precedingTempoValues==QList<int>({486003}));
                QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::WriteOnly));
                CHECK(doc.save(&output,state,saveEditorState));
                CHECK(savedTempos(saved)==expected);
                bytes=saved;
            }
        }
    }
    // Resolution normalization scales event positions, not their tempo data.
    QByteArray lowResolution=multiTrackSmfBytes({tempoTrack({{0,497925},{17,500001}},true)},0,120);
    QBuffer lowInput(&lowResolution); CHECK(lowInput.open(QIODevice::ReadOnly));
    SmfDocument lowSource(&lowInput); CHECK(lowSource.load());
    DocRoot lowDoc; EditorState lowState; SmfImporter lowImporter(&lowDoc,&lowSource,&lowState);
    CHECK(lowImporter.doImport()); CHECK(lowDoc.midiTicksPerWholeNote==1920);
    QByteArray lowSaved; QBuffer lowOutput(&lowSaved); CHECK(lowOutput.open(QIODevice::WriteOnly));
    CHECK(lowDoc.save(&lowOutput,lowState,false));
    const QList<QPair<int,int>> lowExpected={{0,497925},{68,500001}};
    CHECK(savedTempos(lowSaved)==lowExpected);

    // Tempo events in ordinary format-1 tracks must not disappear. At equal
    // ticks, source track order precedes source event order.
    const QList<QPair<int,int>> conductor={{0,497925},{480,500001}};
    const QList<QPair<int,int>> misplaced={{480,600003},{480,400002},{720,500002}};
    QByteArray bytes=multiTrackSmfBytes({tempoTrack(conductor),tempoTrack(misplaced,true)},1);
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state); CHECK(importer.doImport());
    QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::WriteOnly));
    CHECK(doc.save(&output,state,false));
    const QList<QPair<int,int>> all={{0,497925},{480,500001},{480,600003},{480,400002},{720,500002}};
    CHECK(savedTempos(saved)==all);
    CHECK(doc.getMeasureItemAtExact(480)->precedingTempoValues==QList<int>({500001,600003}));

    // A meter-only change affects the beat notation, not quarter-note timing.
    bytes=multiTrackSmfBytes({QByteArray::fromHex(
        "00ff51030799158f00ff5804060318088f00ff2f00"),
        QByteArray::fromHex("00903c649e00803c0000ff2f00")},1);
    QBuffer meterInput(&bytes); CHECK(meterInput.open(QIODevice::ReadOnly));
    SmfDocument meterSource(&meterInput); CHECK(meterSource.load());
    DocRoot meterDoc; EditorState meterState; SmfImporter meterImporter(&meterDoc,&meterSource,&meterState);
    CHECK(meterImporter.doImport());
    CHECK(meterDoc.ticksToMeasure(1920).measureProperties.timeSignatureDenominator==8);
    CHECK(meterDoc.ticksToMeasure(1920).measureProperties.microsecondsPerQuarter==497941);
    CHECK(meterDoc.measureToTicks(2)==3360);
    QByteArray meterSaved; QBuffer meterOutput(&meterSaved); CHECK(meterOutput.open(QIODevice::WriteOnly));
    CHECK(meterDoc.save(&meterOutput,meterState,false));
    CHECK((savedTempos(meterSaved)==QList<QPair<int,int>>({{0,497941}})));
}

static void checkOddPpqnMeterRoundtrips() {
    // An individual eighth note can have a fractional tick while a 4/8 bar
    // remains exact. Such a valid boundary must survive import and export.
    for(int ppqn : {481,961,32767})for(int layout : {0,1,2})
        for(bool saveEditorState : {false,true}) {
            const int boundary=2*ppqn;
            QByteArray conductor=QByteArray::fromHex("00ff580404031808");
            if(layout!=1)conductor+=QByteArray::fromHex("00903c64");
            appendDelta(conductor,quint32(boundary));
            conductor+=QByteArray::fromHex("ff580403021808");
            if(layout!=1)conductor+=QByteArray::fromHex("00803c00");
            appendDelta(conductor,quint32(3*ppqn));
            conductor+=QByteArray::fromHex("ff2f00");
            QList<QByteArray> tracks={conductor};
            if(layout==1) {
                QByteArray notes=QByteArray::fromHex("00903c64");
                appendDelta(notes,quint32(boundary));
                notes+=QByteArray::fromHex("803c0000ff2f00"); tracks.append(notes);
            }
            if(layout==2)tracks.append(QByteArray::fromHex("00ff2f00"));
            QByteArray bytes=multiTrackSmfBytes(tracks,layout==0 ? 0 : 1,ppqn);
            for(int cycle=0;cycle<3;++cycle) {
                QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
                SmfDocument source(&input); CHECK(source.load());
                DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
                CHECK(importer.doImport()); CHECK(state.isValid(&doc));
                CHECK(doc.midiTicksPerWholeNote==4*ppqn);
                CHECK(doc.measureToTicks(1)==boundary && doc.measureToTicks(2)==5*ppqn);
                CHECK(doc.ticksToMeasure(boundary-1).measureIndex==0);
                CHECK(doc.ticksToMeasure(boundary).measureIndex==1);
                CHECK(doc.ticksToMeasure(boundary).measureProperties.timeSignatureNominator==3);
                int notes=0;
                for(const DocTrack* track : doc.trackList)
                    for(const DocEvent* event=track->firstEvent;event;event=event->nextEvent)
                        if(event->type==DocEvent::E_Note) {
                            CHECK(event->tickPosition==0 && event->tickLength==boundary); ++notes;
                        }
                CHECK(notes==1);
                QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
                CHECK(doc.save(&output,state,saveEditorState)); CHECK(output.seek(0));
                SmfDocument exported(&output); CHECK(exported.load());
                QList<QPair<int,int>> meters;
                for(const SmfTrack* track : exported.trackList)
                    for(const SmfEvent* event : track->eventList)
                        if(const auto* meter=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TIME_SIGNATURE))
                            meters.append(qMakePair(int(meter->tickPosition),int(meter->data[0])));
                CHECK((meters==QList<QPair<int,int>>({{0,4},{boundary,3}})));
                bytes=saved;
            }
        }
}

static void checkUnsupportedMeterImport() {
    // A successful load must never quietly discard a meter and every later
    // meter. Keep a valid whole-bar control for each layout and PPQN.
    for(int ppqn : {480,481,961})for(int format : {0,1})for(bool offBar : {false,true}) {
        QByteArray events=QByteArray::fromHex("00ff580404021808");
        appendDelta(events,quint32((offBar ? 2:4)*ppqn));
        events+=QByteArray::fromHex("ff580403021808");
        appendDelta(events,quint32(3*ppqn));
        events+=QByteArray::fromHex("ff58040502180800ff2f00");
        const QByteArray original=multiTrackSmfBytes({events},format,ppqn);
        QByteArray bytes=original; QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
        CHECK(importer.doImport()==!offBar);
        CHECK(importer.errorString().isEmpty()==!offBar);
        CHECK(bytes==original); // Input bytes are never rewritten on failure.
        if(!offBar) {
            CHECK(state.isValid(&doc));
            int count=0; for(const auto* item : doc.measureItemList)if(item->setTimeSignature)++count;
            CHECK(count==3);
        }
    }
    // A fractional whole bar is also outside the integer-tick grid domain.
    for(int numerator : {3,4}) {
        QByteArray events=QByteArray::fromHex("00ff58040403180800ff2f00");
        events[4]=char(numerator);
        QByteArray bytes=smfBytes(events,481); QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
        CHECK(importer.doImport()==(numerator==4));
        CHECK(importer.errorString().isEmpty()==(numerator==4));
    }
}

static void checkNonConductorSignaturesRejected() {
    for(const QByteArray& meta : {QByteArray::fromHex("ff580406032408"),
            QByteArray::fromHex("ff580400021808"), QByteArray::fromHex("ff59020100"),
            QByteArray::fromHex("ff59020002")})for(int trackIndex : {1,2})for(bool mixed : {false,true}) {
        QList<QByteArray> tracks={QByteArray::fromHex("00ff58040402180800ff2f00"),
            QByteArray::fromHex("00903c648360803c0000ff2f00"),
            QByteArray::fromHex("00913e648360813e0000ff2f00")};
        tracks[trackIndex]=QByteArray::fromHex(trackIndex==1 ? "00903c648360":"00913e648360")+meta+
            QByteArray::fromHex(trackIndex==1 ? "8360803c0000ff2f00":"8360813e0000ff2f00");
        if(mixed)tracks[trackIndex].insert(0,QByteArray::fromHex("00923f6400823f00"));
        QByteArray bytes=multiTrackSmfBytes(tracks,1); const QByteArray original=bytes;
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load()); DocRoot doc; EditorState state;
        SmfImporter importer(&doc,&source,&state); CHECK(!importer.doImport());
        CHECK(importer.errorString().contains("outside the conductor track"));
        CHECK(doc.trackList.isEmpty() && doc.measureItemList.isEmpty()); CHECK(bytes==original);
    }
}

static void checkRejectedSignatures() {
    for(int format : {0,1})for(const QByteArray& meta : {
            QByteArray::fromHex("ff580421021808"), QByteArray::fromHex("ff580404061808"),
            QByteArray::fromHex("ff580400021808"), QByteArray::fromHex("ff5803040218"),
            QByteArray::fromHex("ff58050402180855"),
            QByteArray::fromHex("ff59020c00"), QByteArray::fromHex("ff59027f00"),
            QByteArray::fromHex("ff5902f500"), QByteArray::fromHex("ff5902f800"),
            QByteArray::fromHex("ff59020800"), QByteArray::fromHex("ff59020b00"),
            QByteArray::fromHex("ff5902f400"), QByteArray::fromHex("ff59020002"),
            QByteArray::fromHex("ff590100"), QByteArray::fromHex("ff5903000055")}) {
        QByteArray bytes=multiTrackSmfBytes({QByteArray(1,'\0')+meta+QByteArray::fromHex("00903c648360803c0000ff2f00")},format);
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
        CHECK(!importer.doImport()); CHECK(!importer.errorString().isEmpty());
    }
    for(int sf : {-7,0,7})for(int scale : {0,1}) {
        QByteArray events=QByteArray::fromHex("00ff5902"); events+=char(sf); events+=char(scale);
        QByteArray bytes=smfBytes(events+QByteArray::fromHex("00903c648360803c0000ff2f00"));
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
        CHECK(importer.doImport()); CHECK(importer.errorString().isEmpty());
        CHECK(doc.measureItemList[0]->hasValidProperties());
        CHECK(doc.getFirstMeasureEffectiveProperties().keySignature==sf);
    }
}

static void checkExactKeyRoundtrips() {
    const QList<QPair<int,QByteArray>> expected={{0,QByteArray::fromHex("f900")},
        {240,QByteArray::fromHex("0701")},{480,QByteArray::fromHex("0100")},
        {1919,QByteArray::fromHex("ff01")},{1920,QByteArray::fromHex("0000")}};
    for(int layout : {0,1,2})for(bool saveState : {false,true}) {
        QByteArray conductor; int previous=0;
        for(const auto& key : expected) {
            appendDelta(conductor,quint32(key.first-previous));
            conductor+=QByteArray::fromHex("ff5902")+key.second; previous=key.first;
        }
        conductor+=QByteArray::fromHex("00ff2f00");
        const QByteArray notes=QByteArray::fromHex("00903c649e00803c0000ff2f00");
        QList<QByteArray> tracks;
        if(layout==0 || layout==2) {
            conductor.chop(4); conductor+=QByteArray::fromHex("00903c648360803c0000ff2f00");
            tracks={conductor}; if(layout==2)tracks.append(QByteArray::fromHex("00ff2f00"));
        } else tracks={conductor,notes};
        QByteArray bytes=multiTrackSmfBytes(tracks,layout==0 ? 0:1);
        for(int cycle=0;cycle<3;++cycle) {
            QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly)); SmfDocument source(&input); CHECK(source.load());
            DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state); CHECK(importer.doImport());
            CHECK(doc.measureToTicks(1)==1920 && doc.measureToTicks(2)==3840);
            CHECK(doc.ticksToMeasure(479).measureProperties.keySignature==7);
            CHECK(doc.ticksToMeasure(480).measureProperties.keySignature==1);
            QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
            CHECK(doc.save(&output,state,saveState)); CHECK(output.seek(0)); SmfDocument exported(&output); CHECK(exported.load());
            QList<QPair<int,QByteArray>> actual;
            for(const auto* track : exported.trackList)for(const auto* event : track->eventList)
                if(const auto* key=event->isMetaEventOfType(SMF_META_EVENT_TYPE_KEY_SIGNATURE))
                    actual.append({int(key->tickPosition),QByteArray(reinterpret_cast<const char*>(key->data),key->dataLength)});
            CHECK(actual==expected); bytes=saved;
        }
    }
}

static void checkMeterMetadataRoundtrips() {
    for(int layout : {0,1,2})for(bool editorState : {false,true})
    for(int cc : {0,12,24,36,255})for(int bb : {0,4,8,16,255}) {
        QByteArray payload=QByteArray::fromHex("0603"); payload+=char(cc); payload+=char(bb);
        QByteArray conductor=QByteArray::fromHex("00ff5804")+payload;
        const QByteArray notes=QByteArray::fromHex("00903c648360803c0000ff2f00");
        QList<QByteArray> tracks;
        if(layout==1)tracks={conductor+QByteArray::fromHex("00ff2f00"),notes};
        else { tracks={conductor+notes}; if(layout==2)tracks.append(QByteArray::fromHex("00ff2f00")); }
        QByteArray bytes=multiTrackSmfBytes(tracks,layout==0 ? 0:1);
        for(int cycle=0;cycle<3;++cycle) {
            QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
            SmfDocument source(&input); CHECK(source.load());
            DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
            CHECK(importer.doImport());
            const auto effective=doc.getFirstMeasureEffectiveProperties();
            CHECK(effective.midiClocksPerMetronomeClick==cc && effective.notated32ndNotesPerQuarter==bb);
            QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
            CHECK(doc.save(&output,state,editorState)); CHECK(output.seek(0));
            SmfDocument exported(&output); CHECK(exported.load());
            int meters=0;
            for(const auto* track : exported.trackList)for(const auto* event : track->eventList)
                if(const auto* meter=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TIME_SIGNATURE)) {
                    CHECK(meter->dataLength==4);
                    CHECK(QByteArray(reinterpret_cast<const char*>(meter->data),4)==payload); ++meters;
                }
            CHECK(meters==1); bytes=saved;
        }
    }
}

static void checkResolutionImport() {
    for(int ppqn : {1,2,4}) {
        for(int exponent : {3,4,5}) {
            QByteArray events=QByteArray::fromHex("00ff58040100180800903c6401803c0000ff510307a12000ff2f00");
            events[5]=char(exponent);
            QByteArray bytes=smfBytes(events,ppqn);
            QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
            SmfDocument smf(&input); CHECK(smf.load());
            DocRoot doc; EditorState state; SmfImporter importer(&doc,&smf,&state);
            CHECK(importer.doImport());
            CHECK(doc.midiTicksPerWholeNote==1920);
            CHECK(doc.getFirstMeasureEffectiveProperties().timeSignatureDenominator==(1<<exponent));
            CHECK(doc.trackList.size()==1);
            CHECK(doc.trackList[0]->firstEvent->tickLength==480/ppqn);
            CHECK(doc.ticksPerMeasure(doc.getFirstMeasureEffectiveProperties())>0);
            QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
            CHECK(doc.save(&output,state,false)); CHECK(output.seek(0));
            SmfDocument roundtrip(&output); CHECK(roundtrip.load());
        }
    }
    // Tick 5,000,000 at PPQN 1 cannot fit the editor's normalized int domain.
    QByteArray bytes=smfBytes(QByteArray::fromHex("82b19640903c6401803c0000ff2f00"),1);
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument smf(&input); CHECK(smf.load());
    DocRoot doc; EditorState state; SmfImporter importer(&doc,&smf,&state);
    CHECK(!importer.doImport());
    CHECK(doc.trackList.isEmpty() && doc.measureItemList.isEmpty());
}

static void checkEditorlessConfigRoundtrip() {
    QByteArray bytes=smfBytes(QByteArray::fromHex("00903c6401803c0000ff2f00"));
    for(int cycle=0;cycle<3;++cycle) {
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument smf(&input); CHECK(smf.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&smf,&state);
        CHECK(importer.doImport());
        QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::WriteOnly));
        CHECK(doc.save(&output,state,false));
        CHECK(saved.count("<speedy_midi_config")==1);
        bytes=saved;
    }
}

static void checkImportPreservation() {
    for(const char* name : {"late_program", "late_volume", "unknown_xml", "plain_text", "invalid_channel_prefix"}) {
        QFile input(QStringLiteral(SMF_FIXTURE_DIR "/")+QString::fromLatin1(name)+".mid");
        CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument smf(&input); CHECK(smf.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&smf,&state);
        CHECK(importer.doImport()); CHECK(doc.trackList.size()==1);
        CHECK(doc.trackList[0]->midiChannel==1);
        CHECK(doc.trackList[0]->midiPatch==1);
        CHECK(doc.trackList[0]->midiVolume==100);
        QByteArray saved; QBuffer output(&saved); CHECK(output.open(QIODevice::ReadWrite));
        CHECK(doc.save(&output,state,false)); CHECK(output.seek(0));
        SmfDocument roundtrip(&output); CHECK(roundtrip.load());
        if(QString::fromLatin1(name)=="unknown_xml")CHECK(saved.contains("<vendor_data>keep me</vendor_data>"));
        if(QString::fromLatin1(name)=="plain_text")CHECK(saved.contains("vendor data keep me"));
        if(QString::fromLatin1(name).startsWith("late_")) {
            int lateChanges=0;
            for(const SmfTrack* track : roundtrip.trackList)
                for(const SmfEvent* event : track->eventList)
                    if(auto* midi=event->isMidiEvent())
                        if(event->tickPosition==480 &&
                           ((midi->midiCommand[0]&0xf0)==0xc0 ||
                            ((midi->midiCommand[0]&0xf0)==0xb0 && midi->midiCommand[1]==7)))++lateChanges;
            CHECK(lateChanges==1);
        }
        doc.trackList[0]->midiChannel=256;
        CHECK(!doc.save(&output,state,false));
    }
    // Initial changes use the last value, while changes after a tick-zero note
    // retain their relative order instead of changing that note's instrument.
    QByteArray bytes=smfBytes(QByteArray::fromHex("00c00500c00600903c6400c00701803c0000ff2f00"));
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument smf(&input); CHECK(smf.load());
    DocRoot doc; EditorState state; SmfImporter importer(&doc,&smf,&state);
    CHECK(importer.doImport()); CHECK(doc.trackList[0]->midiPatch==7);
    int changes=0;
    for(DocEvent* event=doc.trackList[0]->firstEvent;event;event=event->nextEvent)
        if(event->type==DocEvent::E_OtherMidi) {
            CHECK(event->tickPosition==0 && event->otherMidiEventData.midiCommand[1]==7);
            CHECK(!event->otherMidiEventData.sameTickSubOrdering.beforeNoteEvents); ++changes;
        }
    CHECK(changes==1);
}

static void rejectMalformedTempos() {
    for(int format : {0,1})for(const char* payload : {"00ff5100","00ff510109","00ff51020927", "00ff51040927c055","00ff5103000000"}) {
        QByteArray tempo=QByteArray::fromHex(payload)+QByteArray::fromHex("00ff2f00");
        QByteArray notes=QByteArray::fromHex("00903c6401803c0000ff2f00");
        QByteArray bytes=multiTrackSmfBytes(format==0 ? QList<QByteArray>{tempo.left(tempo.size()-4)+notes} : QList<QByteArray>{notes,tempo},format);
        QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
        SmfDocument source(&input); CHECK(source.load());
        DocRoot doc; EditorState state; SmfImporter importer(&doc,&source,&state);
        CHECK(!importer.doImport()); CHECK(importer.errorString().contains("tempo"));
    }
}

int main(int argc,char** argv) {
    rejectMalformedTempos();
    QCoreApplication app(argc,argv);
    checkResolutionImport();
    checkUnsupportedMeterImport();
    checkNonConductorSignaturesRejected();
    checkRejectedSignatures();
    checkExactKeyRoundtrips();
    checkMeterMetadataRoundtrips();
    checkOddPpqnMeterRoundtrips();
    checkImportPreservation();
    checkEditorlessConfigRoundtrip();
    checkExactTempoRoundtrips();
    // Overlapping pitch 60 on channel 1; independent pitch 60 on channel 2.
    const QByteArray events=QByteArray::fromHex(
        "00903c640a903c5005913c4015803c0014913c0014903c0000ff2f00");
    QByteArray bytes=QByteArray::fromHex("4d546864000000060000000101e04d54726b");
    { QDataStream stream(&bytes,QIODevice::Append); stream << quint32(events.size()); }
    bytes+=events;
    QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly);
    SmfDocument smf(&buffer);
    CHECK(smf.load());
    DocRoot document;
    EditorState editor;
    SmfImporter importer(&document,&smf,&editor);
    CHECK(importer.doImport());
    CHECK(document.trackList.size()==2);
    QMap<int,int> lengths;
    for(DocEvent* note=document.trackList[0]->firstEvent; note; note=note->nextEvent) {
        CHECK(note->type==DocEvent::E_Note);
        lengths.insert(note->tickPosition,note->tickLength);
    }
    CHECK(lengths.size()==2 && lengths.value(0)==36 && lengths.value(10)==66);
    DocEvent* other=nullptr;
    int endpointCount=0;
    for(DocEvent* event=document.trackList[1]->firstEvent;event;event=event->nextEvent) {
        if(event->type==DocEvent::E_Note) {
            CHECK(other==nullptr);
            other=event;
        } else {
            CHECK(event->type==DocEvent::E_Meta);
            CHECK(event->metaEventData.metaEvent->metaEventType==SMF_META_EVENT_TYPE_END_OF_TRACK);
            CHECK(event->tickPositionEnd()==76);
            ++endpointCount;
        }
    }
    CHECK(other && other->tickPosition==15 && other->tickLength==41);
    CHECK(endpointCount==1);

    // Slow, valid SMF tempo in whole-note meter used to truncate to zero BPM.
    QByteArray slow=QByteArray::fromHex("4d546864000000060000000101e04d54726b00000013"
        "00ff58040100180800ff5103ffffff00ff2f00");
    QBuffer slowBuffer(&slow); slowBuffer.open(QIODevice::ReadOnly);
    SmfDocument slowSmf(&slowBuffer); CHECK(slowSmf.load());
    DocRoot slowDocument; EditorState slowEditor;
    SmfImporter slowImporter(&slowDocument,&slowSmf,&slowEditor);
    CHECK(slowImporter.doImport());
    CHECK(slowDocument.measureItemList[0]->BPM==1);
    QByteArray saved; QBuffer output(&saved); output.open(QIODevice::WriteOnly);
    CHECK(slowDocument.save(&output,slowEditor,false));
    // Every supported meter must keep the slowest valid SMF tempo exportable.
    for(int exponent=0; exponent<=5; ++exponent) {
        QByteArray meter=slow; meter[27]=char(exponent);
        QBuffer input(&meter); input.open(QIODevice::ReadOnly);
        SmfDocument meterSmf(&input); CHECK(meterSmf.load());
        DocRoot meterDoc; EditorState meterEditor;
        SmfImporter meterImporter(&meterDoc,&meterSmf,&meterEditor);
        CHECK(meterImporter.doImport());
        CHECK(meterDoc.measureItemList[0]->BPM>=1);
        CHECK(meterDoc.measureItemList[0]->microsecondsPerQuarter==0xffffff);
        QByteArray result; QBuffer resultBuffer(&result); resultBuffer.open(QIODevice::ReadWrite);
        CHECK(meterDoc.save(&resultBuffer,meterEditor,false));
        CHECK(resultBuffer.seek(0)); SmfDocument roundtrip(&resultBuffer); CHECK(roundtrip.load());
        CHECK((savedTempos(result)==QList<QPair<int,int>>({{0,0xffffff}})));
    }
    // Unrepresentable tempos must fail instead of wrapping the 24-bit SMF value.
    slowDocument.measureItemList[0]->timeSignatureDenominator=4;
    slowDocument.measureItemList[0]->microsecondsPerQuarter=0;
    CHECK(!slowDocument.save(&output,slowEditor,false));

    // Failed part export must leave a previously saved file intact.
    QTemporaryDir directory; CHECK(directory.isValid());
    const QString path=directory.filePath("part.mid");
    { QFile original(path); CHECK(original.open(QIODevice::WriteOnly)); CHECK(original.write("original")==8); }
    {
        QSaveFile staged(path); CHECK(staged.open(QIODevice::WriteOnly));
        CHECK(!slowDocument.save(&staged,slowEditor,ConversionOptions(false),QList<int>()));
        staged.cancelWriting();
    }
    { QFile original(path); CHECK(original.open(QIODevice::ReadOnly)); CHECK(original.readAll()=="original"); }
    slowDocument.measureItemList[0]->BPM=0;
    CHECK(!slowDocument.save(&output,slowEditor,false));

    // Clipboard counts are 32-bit even though QList::size() is 64-bit in Qt 6.
    DocTrack source;
    source.name=QStringLiteral("Clipboard track");
    source.midiChannel=2;
    SmfMetaEvent* text=new SmfMetaEvent;
    text->tickPosition=0;
    text->metaEventType=SMF_META_EVENT_TYPE_TEXT;
    text->dataFromString(QStringLiteral("metadata"));
    source.metaEventList.append(text);
    QByteArray clipboard;
    QDataStream writer(&clipboard,QIODevice::WriteOnly);
    source.serialize(writer);
    writer << qint32(123456); // next field must remain aligned
    DocTrack copy;
    QDataStream reader(clipboard);
    copy.deserialize(reader);
    qint32 sentinel=0;
    reader >> sentinel;
    CHECK(reader.status()==QDataStream::Ok && sentinel==123456);
    CHECK(copy.name==source.name && copy.midiChannel==2);
    CHECK(copy.metaEventList.size()==1 && copy.metaEventList[0]->dataToString()==QStringLiteral("metadata"));
    DocTrack empty;
    QByteArray header; QDataStream headerWriter(&header,QIODevice::WriteOnly);
    empty.serialize(headerWriter);
    for(qint32 count : {-1, 1000000000, 1}) {
        QByteArray corrupt=header.left(header.size()-4);
        QDataStream append(&corrupt,QIODevice::Append); append << count;
        QDataStream badReader(corrupt); DocTrack badTrack;
        badTrack.deserialize(badReader);
        CHECK(badReader.status()!=QDataStream::Ok && badTrack.metaEventList.isEmpty());
    }
    std::puts("Overlapping note FIFO, channel isolation and velocity-zero note-off passed");
    return 0;
}
