#include "smfdocument.h"
#include "smfimporter.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "editorstate.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QDataStream>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
static QByteArray smf(const QByteArray& events) {
    QByteArray bytes=QByteArray::fromHex("4d546864000000060000000101e04d54726b");
    QDataStream stream(&bytes,QIODevice::Append); stream << quint32(events.size());
    return bytes+events;
}
static void load(DocRoot& doc,EditorState& state,QByteArray bytes) {
    QBuffer input(&bytes); CHECK(input.open(QIODevice::ReadOnly));
    SmfDocument source(&input); CHECK(source.load());
    SmfImporter importer(&doc,&source,&state); CHECK(importer.doImport());
}
static QByteArray save(DocRoot& doc,EditorState& state) {
    QByteArray bytes; QBuffer output(&bytes); CHECK(output.open(QIODevice::WriteOnly));
    CHECK(doc.save(&output,state,true)); return bytes;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    { DocRoot doc; EditorState state;
      load(doc,state,smf(QByteArray::fromHex("00e9004000ff2f00")));
      CHECK(doc.trackList.size()==1 && doc.trackList[0]->midiChannel==10);
      QByteArray bytes=save(doc,state); QBuffer input(&bytes); input.open(QIODevice::ReadOnly);
      SmfDocument saved(&input); CHECK(saved.load()); int count=0;
      for(auto* track : saved.trackList)for(auto* event : track->eventList)
          if(auto* midi=event->isMidiEvent())if((midi->midiCommand[0]&0xf0)==0xe0) { CHECK(midi->midiCommand[0]==0xe9); ++count; }
      CHECK(count==1); }
    { DocRoot doc; EditorState state;
      load(doc,state,smf(QByteArray::fromHex("00ff58040603180800903c6401803c0000ff2f00")));
      QByteArray bytes=save(doc,state); QBuffer input(&bytes); input.open(QIODevice::ReadOnly);
      SmfDocument saved(&input); CHECK(saved.load()); bool tempo=false;
      for(auto* track : saved.trackList)for(auto* event : track->eventList)
          if(auto* meta=event->isMetaEventOfType(SMF_META_EVENT_TYPE_TEMPO)) { CHECK(meta->dataLength==3); CHECK(QByteArray(reinterpret_cast<char*>(meta->data),3)==QByteArray::fromHex("07a120")); tempo=true; }
      CHECK(tempo); }
    { DocRoot doc; EditorState state;
      load(doc,state,smf(QByteArray::fromHex("00b0000100b0200200c00500903c6401803c0000ff2f00")));
      QByteArray bytes=save(doc,state); QBuffer input(&bytes); input.open(QIODevice::ReadOnly);
      SmfDocument saved(&input); CHECK(saved.load()); int bankMsb=-1,bankLsb=-1,program=-1; bool heard=false;
      for(auto* track : saved.trackList)for(auto* event : track->eventList)if(auto* midi=event->isMidiEvent()) {
          const int command=midi->midiCommand[0]&0xf0;
          if(command==0xb0 && midi->midiCommand[1]==0)bankMsb=midi->midiCommand[2];
          if(command==0xb0 && midi->midiCommand[1]==32)bankLsb=midi->midiCommand[2];
          if(command==0xc0 && bankMsb==1 && bankLsb==2)program=midi->midiCommand[1];
          if(command==0x90 && midi->midiCommand[2]) { CHECK(bankMsb==1 && bankLsb==2 && program==5); heard=true; }
      } CHECK(heard); }
    { DocRoot doc; EditorState state;
      load(doc,state,smf(QByteArray::fromHex("00903c6401803c0000ff2f00")));
      doc.trackList[0]->name=QString::fromUtf8("Őrült Űrhajó 🎵");
      QByteArray bytes=save(doc,state); DocRoot roundtrip; EditorState restored;
      load(roundtrip,restored,bytes); CHECK(roundtrip.trackList[0]->name==doc.trackList[0]->name);
      SmfMetaEvent text; text.dataFromString(doc.trackList[0]->name); CHECK(text.dataToString()==doc.trackList[0]->name);
      text.dataLength=1; text.data[0]=0xfc; CHECK(text.dataToString()==QString::fromUtf8("ü"));
      auto* meta=new SmfMetaEvent; meta->metaEventType=SMF_META_EVENT_TYPE_INSTRUMENT_NAME; meta->dataFromString("instrument");
      doc.trackList[0]->metaEventList.append(meta);
      DocTrack copied(*doc.trackList[0]); CHECK(copied.firstEvent==nullptr); CHECK(copied.metaEventList.size()==1);
      CHECK(copied.metaEventList[0]!=meta && copied.metaEventList[0]->dataToString()=="instrument"); }
    std::puts("SMF compatibility regression passed");
}
