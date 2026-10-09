#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "controller.h"
#include "cs_file.h"
#include "cs_navigation.h"
#include "partextractiondialog.h"
#include "view.h"
#include "smfdocument.h"
#include "buildversion.h"

#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QTimer>
#include <QInputDialog>
#include <QBuffer>
#include "doc_measureitem.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

class EditorTestApp : public SpeedyMidiApp
{
public:
    using SpeedyMidiApp::SpeedyMidiApp;

    void initialize()
    {
        // QSettings uses the temporary INI location configured in main(). No
        // production preferences are read/written and initMidi() is never run.
        settings=new Settings;
        settings->mainWindowGeometry.clear();
        settings->mainWindowState.clear();
        settings->mousePiano.visible=false;
        setupAppGlobalUi();
    }

    ~EditorTestApp() override { aboutToQuitCleanup(); }
};

class EditorTestWindow : public MainWindow
{
public:
    EditorTestWindow() { showForMouseTests(); }
    using MainWindow::loadFile;
    DocRoot* document() { return docRoot; }
    Controller* editor() { return controller; }
    void setTestFilePath(const QString& path) { currentFilePath=path; untitled=false; }

    void showForMouseTests()
    {
        setWindowState(Qt::WindowNoState);
        resize(1000,700);
        // Bypass mouse-piano reattachment; the MIDI interface stays unopened.
        QMainWindow::show();
        QApplication::processEvents();
    }
};

class PartPathTestDialog : public PartExtractionDialog
{
public:
    using PartExtractionDialog::PartExtractionDialog;
    using PartExtractionDialog::getUnusedPartFilePath;
};

static DocEvent* note(int start, int length, int key=60)
{
    DocEvent* event=new DocEvent;
    event->type=DocEvent::E_Note;
    event->tickPosition=start;
    event->tickLength=length;
    event->noteEventData.noteNumber=key;
    event->noteEventData.velocity=80;
    event->noteEventData.midiKeypressSerialNo=0;
    return event;
}

using NoteProperties=std::tuple<int,int,int>;

static std::vector<NoteProperties> notes(const DocTrack* track)
{
    std::vector<NoteProperties> result;
    const DocEvent* previous=nullptr;
    int eventCount=0;
    for(const DocEvent* event=track->firstEvent; event; event=event->nextEvent)
    {
        CHECK(++eventCount < 100); // Also catch a broken list/cycle after undo.
        CHECK(event->prevEvent == previous);
        CHECK(event->tickLength >= 1);
        if(event->type == DocEvent::E_Note)
            result.emplace_back(event->tickPosition,event->tickLength,event->noteEventData.noteNumber);
        previous=event;
    }
    std::sort(result.begin(),result.end());
    return result;
}

static int countEvents(const DocTrack* track, DocEvent::EventType type)
{
    int count=0;
    for(const DocEvent* event=track->firstEvent; event; event=event->nextEvent)
        if(event->type == type)++count;
    return count;
}

static void selectCells(EditorTestWindow& window, int left, int right)
{
    Controller* editor=window.editor();
    EditorState state=editor->getEditorState();
    state.selection.ticksLeft=left;
    state.selection.ticksRight=right;
    state.selection.trackTop=state.selection.trackBottom=0;
    state.selection.anchor.setTo(left,
        window.document()->roundUpTicksToCellBorder(left+1,state.writeLength),0);
    state.trackStateList[0].recordingEnabled=true;
    CHECK(state.isValid(window.document()));
    editor->csApplyStateAndUpdate(state);
}

static void connectDuplicatePitches()
{
    EditorTestWindow window;
    DocTrack* track=window.document()->trackList[0];
    const int cell=window.document()->midiTicksPerWholeNote/8;
    track->insertEvent(note(cell,cell));
    track->insertEvent(note(0,cell));
    track->insertEvent(note(0,cell));
    track->insertEvent(note(cell*4,cell,72));
    const auto before=notes(track);
    selectCells(window,cell,cell*2);
    window.getUI()->actionUtilities_ConnectNotes->trigger();
    CHECK(notes(track).size() == 3);
    CHECK(countEvents(track,DocEvent::E_Note) == 3);
    const auto after=notes(track);
    CHECK(std::count(after.begin(),after.end(),NoteProperties(0,cell*2,60)) == 1);
    CHECK(std::count(after.begin(),after.end(),NoteProperties(0,cell,60)) == 1);
    CHECK(std::count(after.begin(),after.end(),NoteProperties(cell*4,cell,72)) == 1);
    window.getUI()->actionEdit_Undo->trigger();
    CHECK(notes(track) == before);
    window.getUI()->actionEdit_Redo->trigger();
    CHECK(notes(track) == after);
    // Destruction also verifies that no two undo commands own the same note.
}

static void extendOnlyNotes()
{
    EditorTestWindow window;
    DocTrack* track=window.document()->trackList[0];
    const int cell=window.document()->midiTicksPerWholeNote/8;
    track->insertEvent(note(0,cell));

    DocEvent* controller=new DocEvent;
    controller->type=DocEvent::E_OtherMidi;
    controller->tickPosition=0;
    controller->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
    controller->otherMidiEventData.midiCommand[0]=0xb0;
    controller->otherMidiEventData.midiCommand[1]=64;
    controller->otherMidiEventData.midiCommand[2]=127;
    track->insertEvent(controller);

    DocEvent* lyric=new DocEvent;
    lyric->type=DocEvent::E_Meta;
    lyric->tickPosition=0;
    lyric->tickLength=DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS;
    lyric->metaEventData.metaEvent=new SmfMetaEvent;
    lyric->metaEventData.metaEvent->tickPosition=0;
    lyric->metaEventData.metaEvent->metaEventType=SMF_META_EVENT_TYPE_LYRICS;
    lyric->metaEventData.metaEvent->dataFromString(QStringLiteral("word"));
    track->insertEvent(lyric);

    selectCells(window,cell,cell*2);
    window.getUI()->actionWrite_Extend1->trigger();
    window.getUI()->actionWrite_Extend1->trigger();
    CHECK(notes(track) == std::vector<NoteProperties>({NoteProperties(0,cell*3,60)}));
    CHECK(controller->tickLength == DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS);
    CHECK(lyric->tickLength == DOCUMENT_NO_NOTE_EVENT_LENGTH_TICKS);

    selectCells(window,cell,cell*2);
    window.getUI()->actionEdit_ClearCells->trigger();
    CHECK(notes(track).size() == 2);
    CHECK(countEvents(track,DocEvent::E_OtherMidi) == 1);
    CHECK(countEvents(track,DocEvent::E_Meta) == 1);
    window.getUI()->actionEdit_Undo->trigger();
    CHECK(notes(track) == std::vector<NoteProperties>({NoteProperties(0,cell*3,60)}));
    window.getUI()->actionEdit_Undo->trigger();
    CHECK(notes(track) == std::vector<NoteProperties>({NoteProperties(0,cell*2,60)}));
    window.getUI()->actionEdit_Undo->trigger();
    CHECK(notes(track) == std::vector<NoteProperties>({NoteProperties(0,cell,60)}));
    CHECK(controller->tickLength == 1 && lyric->tickLength == 1);
    window.getUI()->actionEdit_Redo->trigger();
    window.getUI()->actionEdit_Redo->trigger();
    CHECK(notes(track) == std::vector<NoteProperties>({NoteProperties(0,cell*3,60)}));
    CHECK(controller->tickLength == 1 && lyric->tickLength == 1);
}

static void boundHorizontalScroll()
{
    EditorTestWindow window;
    DocRoot* document=window.document();
    document->trackList[0]->insertEvent(note(document->midiTicksPerWholeNote*9000,240));
    window.editor()->csApplyStateAndUpdate(window.editor()->getEditorState());
    CHECK(window.getScrollBarHorizontal()->maximum() == document->getMaxFirstMeasure());
    window.getScrollBarHorizontal()->setSliderPosition(9000);
    window.getScrollBarHorizontal()->triggerAction(QAbstractSlider::SliderMove);
    CHECK(window.editor()->getEditorState().firstMeasure == document->getMaxFirstMeasure());
    CHECK(window.editor()->getEditorState().isValid(document));
    window.getScrollBarHorizontal()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    CHECK(window.editor()->getEditorState().isValid(document));
    window.getScrollBarHorizontal()->triggerAction(QAbstractSlider::SliderToMaximum);
    CHECK(window.editor()->getEditorState().isValid(document));
}

static void boundHighResolutionScroll()
{
    EditorTestWindow window;
    DocRoot* document=window.document();
    document->midiTicksPerWholeNote=32767*4;
    document->measureItemList[0]->timeSignatureNominator=32;
    document->measureItemList[0]->timeSignatureDenominator=1;
    EditorState state;
    state.setStartupDefaultState(document);
    CHECK(document->getMaxFirstMeasure() == 511);
    document->trackList[0]->insertEvent(note(document->measureToTicks(512),1));
    window.editor()->csApplyStateAndUpdate(state);
    CHECK(window.getScrollBarHorizontal()->maximum() == 511);
    window.getScrollBarHorizontal()->setSliderPosition(9000);
    window.getScrollBarHorizontal()->triggerAction(QAbstractSlider::SliderMove);
    CHECK(window.editor()->getEditorState().firstMeasure == 511);
    CHECK(window.editor()->getEditorState().isValid(document));
    window.getScrollBarHorizontal()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    CHECK(window.editor()->getEditorState().firstMeasure == 511);
}

static void clippedNoteIsBody()
{
    EditorTestWindow window;
    window.showForMouseTests();
    DocRoot* document=window.document();
    DocEvent* longNote=note(0,document->midiTicksPerWholeNote*100);
    document->trackList[0]->insertEvent(longNote);
    EditorState state=window.editor()->getEditorState();
    state.firstMeasure=1;
    state.trackStateList[0].centerMidiNote=60;
    window.editor()->csApplyStateAndUpdate(state);
    View* view=window.getView();
    const auto trackY=view->getMapper()->trackToViewY(0);
    const int centerY=(trackY.TopY+trackY.BottomY)/2-1;
    const QPoint left(view->getCellArea().left()+1,centerY);
    const QPoint right(view->getCellArea().right()-1,centerY);
    DocEvent* hit=nullptr;
    bool leftEdge=false;
    CHECK(view->getNoteAtPosition(left,0,&hit) && hit == longNote);
    CHECK(!view->getNoteResizeHit(left,0,&hit,&leftEdge));
    CHECK(view->getNoteAtPosition(right,0,&hit) && hit == longNote);
    CHECK(!view->getNoteResizeHit(right,0,&hit,&leftEdge));

    // A slight gesture on the clipped body must not trim everything before
    // the visible measure (the old behavior mistook it for the left edge).
    window.getUI()->actionEdit_MoveNotes->trigger();
    const auto before=notes(document->trackList[0]);
    QMouseEvent press(QEvent::MouseButtonPress,QPointF(left),QPointF(left),
                      Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    window.editor()->viewMousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove,QPointF(left+QPoint(1,0)),QPointF(left+QPoint(1,0)),
                     Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    window.editor()->viewMouseMoveEvent(&move);
    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(left),QPointF(left),
                        Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    window.editor()->viewMouseReleaseEvent(&release);
    CHECK(notes(document->trackList[0]) == before);
    CHECK(!window.getUI()->actionEdit_Undo->isEnabled());

    // Real visible edges retain their resize handles.
    state=window.editor()->getEditorState();
    state.firstMeasure=0;
    window.editor()->csApplyStateAndUpdate(state);
    CHECK(view->getNoteResizeHit(left,0,&hit,&leftEdge) && hit == longNote && leftEdge);
}

static void reserveSourceFilePath(const QString& directory)
{
    EditorTestWindow window;
    const QString source=directory+QStringLiteral("/Song.mid");
    QFile original(source);
    CHECK(original.open(QIODevice::WriteOnly));
    CHECK(original.write("original document") == 17);
    original.close();
    window.setTestFilePath(QFileInfo(source).canonicalFilePath());
    CS_File* file=qobject_cast<CS_File*>(window.editor()->getSubsystemByClassName("CS_File"));
    CHECK(file);
    PartPathTestDialog dialog(file);
    const QString part=dialog.getUnusedPartFilePath(directory,QStringLiteral("Song"));
    CHECK(part == directory+QStringLiteral("/Song(1).mid"));
    CHECK(dialog.getUnusedPartFilePath(directory,QStringLiteral("SONG")) !=
          directory+QStringLiteral("/SONG.mid"));
    ExtractedPart previous;
    previous.filePath=part;
    dialog.partList.append(previous);
    CHECK(dialog.getUnusedPartFilePath(directory,QStringLiteral("Song")) ==
          directory+QStringLiteral("/Song(2).mid"));

#if defined(Q_OS_UNIX)
    const QString alias=directory+QStringLiteral("/alias.mid");
    CHECK(QFile::link(source,alias));
    CHECK(QFileInfo(alias).canonicalFilePath() == window.getCurrentFilePath());
    CHECK(dialog.getUnusedPartFilePath(directory,QStringLiteral("alias")) ==
          directory+QStringLiteral("/alias(1).mid"));
#endif

    CHECK(window.saveFile(part,ConversionOptions(false)));
    CHECK(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == QByteArray("original document"));
    CHECK(window.getCurrentFilePath() == QFileInfo(source).canonicalFilePath());
}

static void aboutUsesBuildVersion()
{
    EditorTestWindow window;
    QString text;
    QTimer responder;
    QObject::connect(&responder,&QTimer::timeout,[&]() {
        auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if(message) { text=message->text(); message->accept(); }
    });
    responder.start(10);
    window.getUI()->actionHelp_About->trigger();
    responder.stop();
    CHECK(text.contains(QStringLiteral("<b>Speed MIDI Editor %1</b>").arg(QStringLiteral(SPEED_MIDI_EDITOR_VERSION))));
}

static QByteArray savedDocument(EditorTestWindow& window)
{
    QByteArray bytes; QBuffer output(&bytes); CHECK(output.open(QIODevice::WriteOnly));
    CHECK(window.document()->save(&output,window.editor()->getEditorState(),false));
    return bytes;
}

static void numericEditingBoundaries()
{
    for(int count : {1,1024})for(bool nearLimit : {false,true})
    {
        EditorTestWindow window; DocRoot* doc=window.document();
        doc->midiTicksPerWholeNote=32767*4;
        doc->measureItemList[0]->timeSignatureNominator=32;
        doc->measureItemList[0]->timeSignatureDenominator=1;
        const int maxTick=INT_MAX-doc->midiTicksPerWholeNote*32;
        const int start=nearLimit ? maxTick-100 : 10;
        // SMF VLQ deltas are bounded independently of the absolute tick domain.
        if(nearLimit)for(qint64 tick=250000000; tick<start; tick+=250000000)
        {
            DocEvent* filler=new DocEvent; filler->type=DocEvent::E_OtherMidi;
            filler->tickPosition=int(tick); filler->tickLength=1;
            filler->otherMidiEventData.midiCommand[0]=0xb0;
            filler->otherMidiEventData.midiCommand[1]=1;
            filler->otherMidiEventData.midiCommand[2]=0;
            doc->trackList[0]->insertEvent(filler);
        }
        DocEvent* event=note(start,10); doc->trackList[0]->insertEvent(event);
        EditorState state; state.setStartupDefaultState(doc); state.setGlobalMeasureSelection(0,1,doc);
        window.editor()->csApplyStateAndUpdate(state);
        const QByteArray original=savedDocument(window);
        int warnings=0; QTimer responder;
        QObject::connect(&responder,&QTimer::timeout,[&]() {
            if(auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
                { dialog->setIntValue(count); dialog->accept(); }
            else if(auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                { ++warnings; warning->accept(); }
        });
        responder.start(10); window.getUI()->actionEdit_Insert->trigger(); responder.stop();
        const bool accepted=count==1 && !nearLimit;
        CHECK(warnings==(accepted ? 0:1));
        CHECK(event->tickPosition==start+(accepted ? 4194176:0));
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==accepted);
        if(accepted)
        {
            const QByteArray inserted=savedDocument(window);
            window.getUI()->actionEdit_Undo->trigger(); CHECK(event->tickPosition==start);
            CHECK(savedDocument(window)==original);
            window.getUI()->actionEdit_Redo->trigger(); CHECK(savedDocument(window)==inserted);
        }
        else CHECK(savedDocument(window)==original);
    }
    for(double percent : {200.,1600.})
    {
        EditorTestWindow window; DocRoot* doc=window.document();
        doc->midiTicksPerWholeNote=32767*4;
        doc->measureItemList[0]->timeSignatureNominator=32;
        doc->measureItemList[0]->timeSignatureDenominator=1;
        // A valid first note must not change when a later note overflows.
        DocEvent* shortNote=note(0,100); doc->trackList[0]->insertEvent(shortNote);
        DocEvent* longNote=note(10,150000000); doc->trackList[0]->insertEvent(longNote);
        EditorState state; state.setStartupDefaultState(doc); state.setGlobalMeasureSelection(0,40,doc);
        window.editor()->csApplyStateAndUpdate(state);
        const QByteArray original=savedDocument(window);
        int warnings=0; QTimer responder;
        QObject::connect(&responder,&QTimer::timeout,[&]() {
            if(auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
                { dialog->setDoubleValue(percent); dialog->accept(); }
            else if(auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                { ++warnings; warning->accept(); }
        });
        responder.start(10); window.getUI()->actionUtilities_ScaleNoteLength->trigger(); responder.stop();
        const bool accepted=percent==200.;
        CHECK(warnings==(accepted ? 0:1));
        CHECK(shortNote->tickLength==(accepted ? 200:100));
        CHECK(longNote->tickLength==(accepted ? 300000000:150000000));
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==accepted);
        if(accepted)window.getUI()->actionEdit_Undo->trigger();
        CHECK(savedDocument(window)==original);
    }
}

static void mouseMoveNoteBoundaries()
{
    for(bool reject : {true,false})
    {
        EditorTestWindow window; DocRoot* doc=window.document();
        doc->midiTicksPerWholeNote=32767*4;
        doc->measureItemList[0]->timeSignatureNominator=32;
        doc->measureItemList[0]->timeSignatureDenominator=1;
        const int bar=doc->midiTicksPerWholeNote*32;
        const int maxTick=INT_MAX-bar;
        DocEvent* event=note(0,maxTick-(reject ? 100 : 2*bar));
        doc->trackList[0]->insertEvent(event);
        EditorState state; state.setStartupDefaultState(doc);
        state.firstMeasure=1; state.xZoomSliderValue=0;
        state.trackStateList[0].centerMidiNote=60;
        window.editor()->csApplyStateAndUpdate(state);
        View* view=window.getView(); const auto y=view->getMapper()->trackToViewY(0);
        const QPoint start(view->getCellArea().left()+10,(y.TopY+y.BottomY)/2-1);
        const QPoint finish(view->getCellArea().right()-10,start.y());
        DocEvent* hit=nullptr; bool leftEdge=false;
        CHECK(view->getNoteAtPosition(start,0,&hit) && hit==event);
        CHECK(!view->getNoteResizeHit(start,0,&hit,&leftEdge));
        window.getUI()->actionEdit_MoveNotes->trigger();
        const auto original=notes(doc->trackList[0]);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(start),QPointF(start),
                          Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        window.editor()->viewMousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove,QPointF(finish),QPointF(finish),
                         Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        window.editor()->viewMouseMoveEvent(&move);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(finish),QPointF(finish),
                            Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        window.editor()->viewMouseReleaseEvent(&release);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==!reject);
        if(reject)CHECK(notes(doc->trackList[0])==original);
        else
        {
            const auto moved=notes(doc->trackList[0]);
            CHECK(moved.size()==1 && std::get<0>(moved[0])>0);
            CHECK(qint64(std::get<0>(moved[0]))+std::get<1>(moved[0])<=maxTick);
            window.getUI()->actionEdit_Undo->trigger();
            CHECK(notes(doc->trackList[0])==original);
            window.getUI()->actionEdit_Redo->trigger();
            CHECK(notes(doc->trackList[0])==moved);
        }
    }
}

static void mouseDrawNoteBoundaries()
{
    for(bool reject : {true,false})
    {
        EditorTestWindow window; DocRoot* doc=window.document();
        doc->midiTicksPerWholeNote=32767*4;
        doc->measureItemList[0]->timeSignatureNominator=32;
        doc->measureItemList[0]->timeSignatureDenominator=1;
        const int maxTick=INT_MAX-doc->midiTicksPerWholeNote*32;
        doc->trackList[0]->insertEvent(note(0,maxTick-100));
        EditorState state; state.setStartupDefaultState(doc);
        state.firstMeasure=doc->getMaxFirstMeasure()-(reject ? 0 : 1);
        state.trackStateList[0].centerMidiNote=60;
        window.editor()->csApplyStateAndUpdate(state);
        const auto original=notes(doc->trackList[0]);
        View* view=window.getView(); const auto y=view->getMapper()->trackToViewY(0);
        const auto* target=view->getMapper()->getDisplayedCellList()[4];
        const QPoint point(view->getCellArea().left()+target->leftX+target->cellWidth/2,
                           (y.TopY+y.BottomY)/2-1);
        const auto cell=view->getMapper()->cellAreaXToTicks(point.x()-view->getCellArea().left());
        CHECK((cell.cellRightTicks>maxTick)==reject);
        window.getUI()->actionEdit_DrawNotes->trigger();
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(point),
                          Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        window.editor()->viewMousePressEvent(&press);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(point),
                            Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        window.editor()->viewMouseReleaseEvent(&release);
        CHECK(window.getUI()->actionEdit_Undo->isEnabled()==!reject);
        if(reject)CHECK(notes(doc->trackList[0])==original);
        else
        {
            CHECK(notes(doc->trackList[0]).size()==original.size()+1);
            window.getUI()->actionEdit_Undo->trigger();
            CHECK(notes(doc->trackList[0])==original);
        }
    }
}

static void offGridMarkerNavigation(const QString& directory)
{
    // Format 1, PPQN 480: markers at 123, 177, 200, 1200 and 2100.
    // The first three are inside the same 0..240 editor cell.
    const QByteArray midi=QByteArray::fromHex(
        "4d546864000000060001000201e04d54726b0000003c00ff5804040218087bff"
        "06054541524c5936ff06065345434f4e4417ff060554484952448768ff06044e"
        "4558548704ff06054c415445528d4cff2f004d54726b0000000e00903c508360"
        "803c009a20ff2f00"
    );
    const QString path=directory+QStringLiteral("/offgrid-markers.mid");
    QFile file(path); CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(midi)==midi.size()); file.close();
    EditorTestWindow window; CHECK(window.loadFile(path));
    DocRoot* doc=window.document(); selectCells(window,0,240);
    const auto& measures=window.getView()->getMapper()->getDisplayedMeasureList();
    CHECK(measures.size()>=2);
    CHECK(measures[0]->rehearsalMarkers.size()==4);
    CHECK(measures[0]->rehearsalMarkers[0].tickPosition==123);
    CHECK(measures[0]->rehearsalMarkers[3].tickPosition==1200);
    CHECK(measures[1]->rehearsalMarkers.size()==1);
    CHECK(measures[1]->rehearsalMarkers[0].tickPosition==2100);
    CHECK(measures[1]->measureOffsetToLastRehearsalMarker==1);
    const QPixmap screenshot=window.getView()->grab();
    const QImage rendered=screenshot.toImage();
    const qreal ratio=screenshot.devicePixelRatio();
    const auto markerX=window.getView()->getMapper()->ticksToViewX(123);
    const int y=qRound((VIEW_MEASURE_HEADER_ITEMS_CELL_HEIGHT+1)*ratio);
    const QColor before=rendered.pixelColor(qRound((window.getView()->getCellArea().left()+2)*ratio),y);
    CHECK(rendered.pixelColor(qRound((markerX.cellLeftX+markerX.cellInternalOffsetX)*ratio),y)!=before);
    auto* navigation=qobject_cast<CS_Navigation*>(window.editor()->getSubsystemByClassName("CS_Navigation"));
    CHECK(navigation);
    for(int expected : {0,0,0,1200,1920})
    {
        QKeyEvent key(QEvent::KeyPress,Qt::Key_PageDown,Qt::ControlModifier);
        CHECK(navigation->keyPressEvent(&key));
        CHECK(window.editor()->getEditorState().isValid(doc));
        CHECK(window.editor()->getEditorState().selection.ticksLeft==expected);
    }
    for(int expected : {1200,0,0,0,0})
    {
        QKeyEvent key(QEvent::KeyPress,Qt::Key_PageUp,Qt::ControlModifier);
        CHECK(navigation->keyPressEvent(&key));
        CHECK(window.editor()->getEditorState().isValid(doc));
        CHECK(window.editor()->getEditorState().selection.ticksLeft==expected);
    }
    // Moving the selection elsewhere must reset the exact navigation cursor.
    selectCells(window,480,720);
    QKeyEvent next(QEvent::KeyPress,Qt::Key_PageDown,Qt::ControlModifier);
    CHECK(navigation->keyPressEvent(&next));
    CHECK(window.editor()->getEditorState().selection.ticksLeft==1200);
    // Whole-track navigation reverts to cells; whole-measure navigation keeps
    // its mode and still advances through markers sharing the same measure.
    EditorState state=window.editor()->getEditorState();
    state.setGlobalTrackSelection(0,1,doc);
    window.editor()->csApplyStateAndUpdate(state);
    CHECK(navigation->keyPressEvent(&next));
    CHECK(window.editor()->getEditorState().selection.getSelectionMode()==S_LocalCells);
    CHECK(window.editor()->getEditorState().isValid(doc));
    state=window.editor()->getEditorState(); state.setGlobalMeasureSelection(0,1,doc);
    window.editor()->csApplyStateAndUpdate(state);
    for(int expected : {0,0,0,0,1920})
    {
        CHECK(navigation->keyPressEvent(&next));
        CHECK(window.editor()->getEditorState().selection.getSelectionMode()==S_GlobalMeasure);
        CHECK(window.editor()->getEditorState().selection.ticksLeft==expected);
        CHECK(window.editor()->getEditorState().isValid(doc));
    }
    // Painting exercises exact marker positions, including several per bar.
    CHECK(!window.getView()->grab().isNull());
}

static void fractionalBeatGrid()
{
    EditorTestWindow window; DocRoot* doc=window.document();
    doc->midiTicksPerWholeNote=4*481;
    doc->measureItemList[0]->timeSignatureNominator=4;
    doc->measureItemList[0]->timeSignatureDenominator=8;
    EditorState state; state.setStartupDefaultState(doc);
    window.editor()->csApplyStateAndUpdate(state);
    const auto properties=doc->getFirstMeasureEffectiveProperties();
    CHECK(doc->ticksPerMeasure(properties)==962);
    CHECK(doc->ticksPerBeat(properties)==240.5);
    const int beatTicks[]={0,240,481,721,962};
    for(int i=0; i<5; ++i)
    {
        CHECK(doc->beatToMeasureInternalTick(i,properties)==beatTicks[i]);
        CHECK(doc->isBeatBorder(beatTicks[i],properties));
    }
    CHECK(!doc->isBeatBorder(480,properties));
    CHECK(!doc->isBeatBorder(482,properties));
    bool found=false;
    for(const auto* cell : window.getView()->getMapper()->getDisplayedCellList())
        if(cell->measureIndex==0 && cell->tickPosition==481)
        { found=true; CHECK(doc->isBeatBorder(cell->tickPosition,properties)); }
    CHECK(found);
    CHECK(!window.getView()->grab().isNull());
}

int main(int argc, char** argv)
{
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temporary.path());
    EditorTestApp application(argc,argv);
    application.initialize();
    numericEditingBoundaries();
    mouseMoveNoteBoundaries();
    mouseDrawNoteBoundaries();
    offGridMarkerNavigation(temporary.path());
    fractionalBeatGrid();
    aboutUsesBuildVersion();
    connectDuplicatePitches();
    extendOnlyNotes();
    boundHorizontalScroll();
    boundHighResolutionScroll();
    clippedNoteIsBody();
    reserveSourceFilePath(temporary.path());
    std::puts("Editor note connection/extension, undo/redo, clipping, scroll and part path tests passed");
    return 0;
}
