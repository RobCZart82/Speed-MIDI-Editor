#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "doc_root.h"
#include "doc_track.h"
#include "doc_event.h"
#include "controller.h"
#include "cs_file.h"
#include "partextractiondialog.h"
#include "view.h"
#include "smfdocument.h"

#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QTemporaryDir>
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

int main(int argc, char** argv)
{
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temporary.path());
    EditorTestApp application(argc,argv);
    application.initialize();
    connectDuplicatePitches();
    extendOnlyNotes();
    boundHorizontalScroll();
    boundHighResolutionScroll();
    clippedNoteIsBody();
    reserveSourceFilePath(temporary.path());
    std::puts("Editor note connection/extension, undo/redo, clipping, scroll and part path tests passed");
    return 0;
}
