#include "doc_root.h"
#include "doc_measureitem.h"
#include "editorstate.h"
#include <QCoreApplication>
#include <QDomDocument>
#include <QDataStream>
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    DocRoot doc;
    doc.midiTicksPerWholeNote=32767 * 4;
    auto* measure=new DocMeasureItem;
    measure->setFirstMeasureItemDefaults();
    measure->timeSignatureNominator=32;
    measure->timeSignatureDenominator=1;
    doc.measureItemList.append(measure);
    CHECK(doc.measureToTicks(512)==2147418112);
    CHECK(doc.measureToTicks(513)==INT_MAX);
    CHECK(doc.measureToTicks(INT_MAX)==INT_MAX);
    CHECK(doc.getMaxFirstMeasure()==511);
    CHECK(doc.roundUpTicksToMeasureBorder(INT_MAX)==INT_MAX);
    EditorState state;
    state.setStartupDefaultState(&doc);
    state.firstMeasure=511; CHECK(state.isValid(&doc));
    state.firstMeasure=512; CHECK(!state.isValid(&doc));
    measure->timeSignatureNominator=4;
    measure->timeSignatureDenominator=4;
    doc.midiTicksPerWholeNote=DOCUMENT_DEFAULT_TICKS_PER_WHOLE_NOTE;
    CHECK(doc.getMaxFirstMeasure()==CS_NAVIGATION_MAX_FIRST_MEASURE);
    EditorTrackState track;
    CHECK(track.isValid());
    for(double value : {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e100}) {
        track.heightInNotes=value; CHECK(!track.isValid());
        QByteArray bytes; QDataStream out(&bytes,QIODevice::WriteOnly); track.serialize(out);
        QDataStream in(bytes); EditorTrackState decoded; decoded.deserialize(in);
        CHECK(in.status()==QDataStream::ReadCorruptData);
    }
    track=EditorTrackState(); track.centerMidiNote=std::numeric_limits<double>::quiet_NaN();
    CHECK(!track.isValid());
    state.setStartupDefaultState(&doc);
    state.trackStateList.append(EditorTrackState());
    QDomDocument xml;
    auto root=xml.createElement("test"); xml.appendChild(root);
    CHECK(state.saveToXML(root,1));
    auto trackElement=root.elementsByTagName("track").item(0).toElement();
    CHECK(!trackElement.isNull());
    trackElement.setAttribute(XML_ATTR_HEIGHT_IN_NOTES,"inf");
    CHECK(!state.loadFromXML(root,1));
    CHECK(state.trackStateList.size()==1 && state.trackStateList.first().isValid());
    DocMeasureItem unset;
    DocMeasureItem copy(unset); copy.clean();
    CHECK(copy.keySignatureScale==DocMeasureItem::KSS_Major);
    std::puts("validation regression passed");
}
