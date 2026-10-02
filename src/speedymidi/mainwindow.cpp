/***************************************************************************
 *  mainwindow.cpp - Editor Main Window
 *                   Owner of Model, View, and Controller
 *                   (cf. Model–View–Controller Architecture)
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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "speedymidiapp.h"
#include "settings.h"
#include "view.h"
#include "controller.h"
#include "doc_root.h"
#include "zoomsliderwidget.h"
#include "mousepianodockwidget.h"
#include "midiinterface.h"
#include "cs_playback.h"
#include "cs_navigation.h"

#include <QtGui>
#include <QtWidgets>
#include <QScreen>
#include <QSaveFile>
#include <QStatusBar>

// Constructor 1: new default document
MainWindow::MainWindow()
    : QMainWindow(NULL), ui(new Ui::MainWindow)
{
    init();

    // create new default document
    docRoot=new DocRoot;
    EditorState startupState=docRoot->prepareDefaultDocument();

    // controller setup
    controller=new Controller(this, docRoot, view, startupState);

    // set file name to untitled
    setCurrentFile("");
}

// Constructor 2: new file initialized with settings collected by track wizard
MainWindow::MainWindow(const DocMeasureItem& firstMeasureProperties, const QString& trackWizardString)
    : QMainWindow(NULL), ui(new Ui::MainWindow)
{
    init();
    createdByWizard=true;

    // create document as indicated by wizard
    docRoot=new DocRoot;
    EditorState startupState=docRoot->prepareDocumentByWizardSettings(
            firstMeasureProperties,
            trackWizardString,
            getApp()->getSettings()->LRU.trackWizardAssignPatches);

    // controller setup
    controller=new Controller(this, docRoot, view, startupState);

    // set file name to untitled
    setCurrentFile("");
}

// Constructor 3: open one or more files
MainWindow::MainWindow(const QString& listRelativePath, const QStringList& filesToOpenList)
    : QMainWindow(NULL), ui(new Ui::MainWindow)
{
    init();

    // create new default document
    docRoot=new DocRoot;
    EditorState startupState=docRoot->prepareDefaultDocument();

    // controller setup
    controller=new Controller(this, docRoot, view, startupState);

    // open the given files, re-use this window for first file in list
    openFiles_(listRelativePath, filesToOpenList, true);

    // set file name to untitled if loading failed
    if(untitled)setCurrentFile("");
}

MainWindow::~MainWindow()
{
    // delete controller, then document referenced by controller
    delete controller;
    delete docRoot;

    delete ui;
}

void MainWindow::init()
{
    ui->setupUi(this);
    // Keep the app menus in the window so the application theme can style them.
    ui->menuBar->setNativeMenuBar(false);
    // Keep the native macOS title bar separate from the application toolbar.
    setUnifiedTitleAndToolBarOnMac(false);
    ui->mainToolBar->setAttribute(Qt::WA_StyledBackground, true);
    ui->mainToolBar->setAutoFillBackground(true);
    ui->actionPlayback_ReturnToStart->setIcon(QIcon(QStringLiteral(":/images/flat/return-start.svg")));
    ui->mainToolBar->setStyleSheet(QStringLiteral(R"(
        QToolBar {
            background: #dce9f5;
            border: 0;
            border-bottom: 1px solid #8eaed0;
            spacing: 4px;
            padding: 3px;
        }
        QToolButton {
            color: #202a35;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 2px;
        }
        QToolButton:hover { background: #edf5fc; border-color: #9cb9d9; }
        QToolButton:pressed, QToolButton:checked {
            background: #c0d8f1;
            border-color: #7fa6d1;
        }
    )"));
    setAttribute(Qt::WA_DeleteOnClose);

    windowTitleAppName=tr("Speed MIDI Editor");

    untitled=true;
    createdByWizard=false;

    view=NULL;
    scrollBarHorizontal=NULL;
    scrollBarVertical=NULL;
    xZoomSliderWidget=NULL;
    yZoomSliderWidget=NULL;
    spinBoxRelativePlaybackSpeed=NULL;

    mousePianoAttached=false;
    mousePianoAttachmentOK=false;

    docRoot=NULL;
    controller=NULL;

    // -------------------------------------------------------------------------
    // Setup recent file list:
    // Actions
    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
    }

    // Menu
    for (int i = 0; i < MaxRecentFiles; ++i)
        ui->menuFile->insertAction(ui->actionFile_Quit,recentFileActs[i]);
    recentFileActsSeparatorAct = new QAction(this);
    recentFileActsSeparatorAct->setSeparator(true);
    ui->menuFile->insertAction(ui->actionFile_Quit,recentFileActsSeparatorAct);

    // Initial list update
    updateRecentFileActions();

    // -------------------------------------------------------------------------
    // additional toolbar widgets

    // relative playback speed from 10%-500%
    spinBoxRelativePlaybackSpeed=new ToolBarSpinBox(this);
    spinBoxRelativePlaybackSpeed->setMinimum(10);
    spinBoxRelativePlaybackSpeed->setMaximum(500);
    spinBoxRelativePlaybackSpeed->setSingleStep(10);
    spinBoxRelativePlaybackSpeed->setValue(100);
    spinBoxRelativePlaybackSpeed->setSuffix(" %");
    spinBoxRelativePlaybackSpeed->setMinimumWidth(ui->mainToolBar->iconSize().width() * 2);
    spinBoxRelativePlaybackSpeed->setMinimumHeight(ui->mainToolBar->iconSize().height());

    ui->mainToolBar->addWidget(spinBoxRelativePlaybackSpeed);
    ui->mainToolBar->addSeparator();

    // -------------------------------------------------------------------------
    // application-wide actions setup

    ui->menuView   ->insertAction(ui->menuView->actions()[0],getApp()->getActionView_MousePiano());
    ui->menuView   ->insertAction(ui->menuView->actions()[1],getApp()->getActionView_Toolbar());

    ui->menuOptions->insertAction(NULL,getApp()->getActionOptions_MidiThru());
    ui->menuOptions->insertSeparator(NULL);
    ui->menuOptions->insertAction(NULL,getApp()->getActionOptions_MousePianoPercussionMode());

    ui->mainToolBar->addAction(getApp()->getActionView_MousePiano());
    ui->mainToolBar->addAction(getApp()->getActionOptions_MousePianoPercussionMode());

    // -------------------------------------------------------------------------
    // Central widget setup

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    scrollBarHorizontal=new EditorScrollBar(Qt::Horizontal, centralWidget);
    scrollBarVertical=new EditorScrollBar(Qt::Vertical, centralWidget);

    xZoomSliderWidget=new ZoomSliderWidget(Qt::Horizontal,centralWidget);
    xZoomSliderWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    xZoomSliderWidget->setMinimumSize(VIEW_CELL_AREA_LEFT, 0);
    xZoomSliderWidget->setMaximumSize(VIEW_CELL_AREA_LEFT, QWIDGETSIZE_MAX);

    yZoomSliderWidget=new ZoomSliderWidget(Qt::Vertical,centralWidget);
    yZoomSliderWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    const int verticalZoomControlHeight = VIEW_CELL_AREA_TOP + VIEW_CELL_AREA_TOP / 2;
    yZoomSliderWidget->setMinimumSize(0, verticalZoomControlHeight);
    yZoomSliderWidget->setMaximumSize(QWIDGETSIZE_MAX, verticalZoomControlHeight);

    QToolButton* fitAllTracksButton = new QToolButton(centralWidget);
    fitAllTracksButton->setObjectName(QStringLiteral("fitAllTracksButton"));
    fitAllTracksButton->setIcon(QIcon(QStringLiteral(":/images/flat/fit-all-tracks.svg")));
    fitAllTracksButton->setIconSize(QSize(18, 18));
    fitAllTracksButton->setFixedSize(22, 22);
    fitAllTracksButton->setAutoRaise(true);
    fitAllTracksButton->setFocusPolicy(Qt::NoFocus);
    fitAllTracksButton->setToolTip(tr("Fit Entire Song"));
    fitAllTracksButton->setAccessibleName(tr("Fit Entire Song"));
    ui->actionView_FitAllTracks->setIcon(QIcon(QStringLiteral(":/images/flat/fit-all-tracks.svg")));
    connect(fitAllTracksButton, &QToolButton::clicked,
            ui->actionView_FitAllTracks, &QAction::trigger);

    // Stack vertical navigation controls together so the zoom slider stays
    // centered beneath the vertical scrollbar as the window is resized.
    QWidget* verticalNavigationWidget = new QWidget(centralWidget);
    QVBoxLayout* verticalNavigationLayout = new QVBoxLayout(verticalNavigationWidget);
    // Keep the scrollbar centered on the same axis as the wider zoom and fit
    // controls below it.
    verticalNavigationLayout->addWidget(scrollBarVertical, 1, Qt::AlignHCenter);
    verticalNavigationLayout->addWidget(yZoomSliderWidget, 0, Qt::AlignHCenter | Qt::AlignTop);
    verticalNavigationLayout->addSpacing(6);
    verticalNavigationLayout->addWidget(fitAllTracksButton, 0, Qt::AlignHCenter | Qt::AlignTop);
    verticalNavigationLayout->setContentsMargins(0, 0, 0, 2);
    verticalNavigationLayout->setSpacing(0);

    statusBar()->setSizeGripEnabled(false);

    // Continue the horizontal scrolling row with its zoom control.
    QWidget* horizontalNavigationWidget = new QWidget(centralWidget);
    QHBoxLayout* horizontalNavigationLayout = new QHBoxLayout(horizontalNavigationWidget);
    QFrame* trackPaneSeparator = new QFrame(horizontalNavigationWidget);
    trackPaneSeparator->setFrameShape(QFrame::VLine);
    trackPaneSeparator->setFrameShadow(QFrame::Plain);
    trackPaneSeparator->setLineWidth(1);
    trackPaneSeparator->setMidLineWidth(0);
    trackPaneSeparator->setFixedWidth(1);
    trackPaneSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    QPalette separatorPalette=trackPaneSeparator->palette();
    separatorPalette.setColor(QPalette::WindowText,QColor(18,31,52));
    trackPaneSeparator->setPalette(separatorPalette);
    horizontalNavigationLayout->addWidget(trackPaneSeparator);
    horizontalNavigationLayout->addWidget(scrollBarHorizontal);
    horizontalNavigationLayout->addWidget(xZoomSliderWidget);
    horizontalNavigationLayout->setContentsMargins(0, 0, 4, 0);
    horizontalNavigationLayout->setSpacing(0);
    horizontalNavigationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    view = new View(centralWidget);
    connect(getApp()->getMidiInterface(), &MidiInterface::trackMidiActivity,
            this, [this](int trackIndex, int velocity) {
                if(!controller || !view)return;
                CS_Playback* playback=qobject_cast<CS_Playback*>(
                            controller->getSubsystemByClassName("CS_Playback"));
                if(playback && playback->getPlaybackMode() != CS_Playback::PBM_None)
                    view->setMidiActivity(trackIndex,velocity);
            }, Qt::QueuedConnection);

    QPushButton* defaultTrackHeightButton = new QPushButton(
                QIcon(QStringLiteral(":/images/flat/track-height-default.svg")),
                tr("Default Track Height"), centralWidget);
    defaultTrackHeightButton->setIconSize(QSize(18, 18));
    defaultTrackHeightButton->setFixedHeight(24);
    defaultTrackHeightButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    defaultTrackHeightButton->setCursor(Qt::PointingHandCursor);
    defaultTrackHeightButton->setToolTip(
                tr("Set every track to an equal height that shows all track information."));
    defaultTrackHeightButton->setAccessibleName(tr("Default Track Height"));
    QFont defaultTrackHeightFont=defaultTrackHeightButton->font();
    defaultTrackHeightFont.setPixelSize(8);
    defaultTrackHeightFont.setBold(true);
    defaultTrackHeightButton->setFont(defaultTrackHeightFont);
    defaultTrackHeightButton->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            color: #203f5e;
            background: #dce9f5;
            border: 1px solid #315b82;
            border-radius: 3px;
            padding: 1px 4px;
            text-align: left;
        }
        QPushButton:hover { background: #edf5fc; border-color: #244e77; }
        QPushButton:pressed { background: #c0d8f1; }
    )"));
    connect(defaultTrackHeightButton, &QPushButton::clicked, this, [this]() {
        if(!controller)return;
        CS_Navigation* navigation=qobject_cast<CS_Navigation*>(
                    controller->getSubsystemByClassName("CS_Navigation"));
        if(navigation)navigation->setDefaultTrackHeights();
    });

    QGridLayout* gridLayout = new QGridLayout(centralWidget);
    gridLayout->addWidget(view, 0, 0, 2, 2);
    gridLayout->addWidget(defaultTrackHeightButton, 2, 0);
    gridLayout->addWidget(horizontalNavigationWidget, 2, 1);
    gridLayout->addWidget(verticalNavigationWidget, 0, 2, 3, 1);
    gridLayout->setContentsMargins(0,0,0,0);
    gridLayout->setSpacing(0);
    gridLayout->setColumnMinimumWidth(0, VIEW_CELL_AREA_LEFT);
    gridLayout->setColumnStretch(0, 0);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setColumnStretch(2, 0);

    // -------------------------------------------------------------------------

    // call menuAboutToShow() when showing any top-level menus
    for(int i=0; i < ui->menuBar->actions().size(); ++i)
        connect(ui->menuBar->actions()[i]->menu(), SIGNAL(aboutToShow()), SLOT(menuAboutToShow()));

    setAcceptDrops(true);
    setGeometryFromSettings();
    retranslateUi();
    view->setFocus();
}

bool MainWindow::event(QEvent *e)
{
    bool b=QWidget::event(e);

    switch(e->type())
    {
    case QEvent::WindowActivate:

        if(mousePianoAttachmentOK)
            getApp()->reattachMousePiano();
        break;

    case QEvent::LanguageChange:

        ui->retranslateUi(this);
        retranslateUi();
        break;

    default:
        break;
    }
    return b;
}

void MainWindow::retranslateUi()
{
    spinBoxRelativePlaybackSpeed->setToolTip(tr("Relative Playback Speed"));

    // -------------------------------------------------------------------------
    // modify action shortcuts not settable in Qt Designer

    ui->actionWrite_Extend1->setShortcut(QKeySequence(tr("Shift+1")));
    ui->actionWrite_Extend2->setShortcut(QKeySequence(tr("Shift+2")));
    ui->actionWrite_Extend3->setShortcut(QKeySequence(tr("Shift+3")));
    ui->actionWrite_Extend4->setShortcut(QKeySequence(tr("Shift+4")));
    ui->actionWrite_Extend5->setShortcut(QKeySequence(tr("Shift+5")));
    ui->actionWrite_Extend6->setShortcut(QKeySequence(tr("Shift+6")));
    ui->actionWrite_Extend7->setShortcut(QKeySequence(tr("Shift+7")));
    ui->actionWrite_Extend8->setShortcut(QKeySequence(tr("Shift+8")));
    ui->actionWrite_Extend9->setShortcut(QKeySequence(tr("Shift+9")));
    ui->actionWrite_Extend10->setShortcut(QKeySequence(tr("Shift+0")));
    ui->actionView_HorizontalZoom_ZoomIn->setShortcut(QKeySequence(tr("Ctrl++")));
    ui->actionView_HorizontalZoom_ZoomOut->setShortcut(QKeySequence(tr("Ctrl+-")));
    ui->actionView_VerticalZoom_ZoomIn->setShortcut(QKeySequence(tr("Ctrl+Shift++")));
    ui->actionView_VerticalZoom_ZoomOut->setShortcut(QKeySequence(tr("Ctrl+Shift+-")));

    // -------------------------------------------------------------------------
    // update dynamic window title and dynamic menu entries

    if(!currentFilePath.isEmpty())
        setWindowTitle(tr("%1[*] - %2").arg(strippedName(currentFilePath)).arg(windowTitleAppName));

    if(controller != NULL)controller->retranslateUi();
}

void MainWindow::stopPlayback()
{
    controller->cancelInterruptibleStates();
}

void MainWindow::attachToMousePiano()
{
    MousePianoDockWidget* dockWidget=getApp()->getMousePianoDockWidget();

    if(!mousePianoAttached)
    {
        // preliminarily add to the bottom area (addDockWidget does not accept Qt::NoDockWidgetArea as parameter)
        dockWidget->setAllowedAreas(Qt::BottomDockWidgetArea);
        addDockWidget(Qt::BottomDockWidgetArea,dockWidget);
        dockWidget->setAllowedAreas(Qt::NoDockWidgetArea);
        dockWidget->setFloating(true);

        mousePianoAttached=true;
    }

    // also show the piano if visibility action is checked
    if(getApp()->getSettings()->mousePiano.visible)
    {
        if(!dockWidget->onceShown)
        {
            dockWidget->move(getApp()->getSettings()->mousePiano.dockWidgetFramePos);
            dockWidget->onceShown=true;
        }

        dockWidget->setVisible(true);
    }
}

void MainWindow::detachFromMousePiano()
{
    if(mousePianoAttached)
    {
        MousePianoDockWidget* dockWidget=getApp()->getMousePianoDockWidget();

        removeDockWidget(dockWidget);
        dockWidget->setParent(NULL);

        mousePianoAttached=false;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(controller->viewCloseEvent())
    {
        copyGeometryToSettings();
        detachFromMousePiano();

        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // File drop from Explorer etc.
    // accept just text/uri-list mime format
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent * event)
{
    // File drop from Explorer etc.
    event->acceptProposedAction();
    event->accept();

    const QMimeData* mimeData=event->mimeData();
    if(!mimeData->hasUrls())return;

    QList<QUrl> urlList=mimeData->urls();
    QStringList filesToOpenList;
    for(int i=0; i < urlList.size(); ++i)
    {
        QString filePath=urlList[i].toLocalFile();
        if(!filePath.isEmpty()) // If URL references a local file, try to open it
            filesToOpenList.append(filePath);
    }

    // BEWARE: We must use a Qt::QueuedConnection to open the file!
    //         Reason: Warning dialogs may occur. Creating dialogs will crash when inside an event handler.

    // Try to open first file in local window, the others in new cascaded (or maximized) other main windows.
    //  The parameter relativePath should not matter here, because QDropEvent contains only absolute paths.
    metaObject()->invokeMethod(this,
                               "openFiles",
                               Qt::QueuedConnection,
                               Q_ARG(QStringList,filesToOpenList));

}

bool MainWindow::createSiblingUsingWizard()
{
    // Copy geometry to settings object: new main window will get correct maximized or cascaded position
    copyGeometryToSettings();

    MainWindow* other=getApp()->createDocumentByWizard();
    if(other != NULL)   // Wizard finished?
    {
        cascadeSibling(other, 1);
        return true;
    }
    else return false;
}

void MainWindow::createSiblingDefaultDocument()
{
    // Copy geometry to settings object: new main window will get correct maximized or cascaded position
    copyGeometryToSettings();

    MainWindow *other = new MainWindow;
    cascadeSibling(other, 1);
}

void MainWindow::show()
{
    QMainWindow::show();

    mousePianoAttachmentOK=true;
    getApp()->reattachMousePiano();
}

void MainWindow::openFiles(const QStringList& absoluteFilePathList)
{
    // used by file CS to open file maybe in current window
    openFiles(QDir::currentPath(), absoluteFilePathList, true);
}

void MainWindow::openFiles(const QString& listRelativePath, const QStringList& filesToOpenList, bool reuseCurrentWindowAllowed)
{
    // Copy geometry to settings object: new main window will get correct maximized or cascaded position
    copyGeometryToSettings();

    openFiles_(listRelativePath, filesToOpenList, reuseCurrentWindowAllowed);
}

void MainWindow::openFiles_(const QString& listRelativePath, const QStringList& filesToOpenList, bool reuseCurrentWindowAllowed)
{
    if(filesToOpenList.isEmpty())return;    // nothing to do

    for(int i=0; i < filesToOpenList.size(); ++i)
    {
        QString filePath=filesToOpenList[i];

        // If document is opened in another window, activate it
        MainWindow *existing = findMainWindow(listRelativePath, filePath);
        if (existing)
        {
            existing->show();
            existing->raise();
            existing->activateWindow();
        }
        else if(reuseCurrentWindowAllowed && untitled && docRoot->unmodifiedDefaultDocument && !isWindowModified())
        {
            QString currentPath=QDir::currentPath();
            QDir::setCurrent(listRelativePath);

            // If current document is empty and unmodified, load first given file in current window
            if(loadFile(filePath))
            {
                // Success: do not use current window again for other files in filesToOpenList
                reuseCurrentWindowAllowed=false;
            }
            else
            {
                // Failure: restore current path before loadFile call
                QDir::setCurrent(currentPath);
            }
        }
        else
        {
            // Open new additional document window

            // call constructor with a single file to open, constructor will call openFile again and
            //  load into this window as it is untitled, unmodified and reuseCurrentWindowAllowed==true
            MainWindow *other = new MainWindow(listRelativePath, QStringList(filePath));
            if(other->untitled)
            {
                // failed to load
                delete other;
                return;
            }
            else
            {
                // Success. Cascade and show window.
                cascadeSibling(other, i + 1);
            }
        }
    }
}

void MainWindow::cascadeSibling(MainWindow* sibling, int offsetFactor) const
{
    QPoint newMainWindowPos=pos() + offsetFactor * QPoint(MAINWINDOW_CASCADE_OFFSET,MAINWINDOW_CASCADE_OFFSET);

    // If new position is right or below screen center point, restart cascading from top left screen corner
    QScreen* currentScreen=screen();
    QRect screenGeometry=currentScreen ? currentScreen->geometry() : QRect();

    if(newMainWindowPos.x() > screenGeometry.center().x() ||
       newMainWindowPos.y() > screenGeometry.center().y())
    {
        // go back to screen corner, but leave a bit of space
        newMainWindowPos=QPoint(MAINWINDOW_CASCADE_OFFSET,MAINWINDOW_CASCADE_OFFSET);
    }

    sibling->move(newMainWindowPos);
    sibling->show();
}

void MainWindow::updateRecentFileActions()
{
    Settings* settings=getApp()->getSettings();

    int numRecentFiles = qMin(settings->recentFileList.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i)
    {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(settings->recentFileList[i]));
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData(settings->recentFileList[i]);
        recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        recentFileActs[j]->setVisible(false);

    recentFileActsSeparatorAct->setVisible(numRecentFiles > 0);
}

void MainWindow::setGeometryFromSettings()
{
    bool reattach=mousePianoAttached;
    detachFromMousePiano();

    Settings* settings=getApp()->getSettings();

    if(settings->mainWindowGeometry.size() > 0)
        restoreGeometry(settings->mainWindowGeometry);
    else
    {
        // first time startup: maximize window
        setWindowState(windowState() | Qt::WindowMaximized);
    }

    restoreState(settings->mainWindowState);

    if(reattach)attachToMousePiano();
}

void MainWindow::copyGeometryToSettings()
{
    bool reattach=mousePianoAttached;
    detachFromMousePiano();

    Settings* settings=getApp()->getSettings();
    settings->mainWindowGeometry=saveGeometry();
    settings->mainWindowState=saveState();

    if(reattach)attachToMousePiano();
}

bool MainWindow::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        removeFromRecentFileList(filePath);

        QMessageBox::warning(this, tr("File open error"),
                             tr("Cannot read file") + "\n\n"
                             + filePath + "\n\n"
                             + file.errorString());
        return false;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);

    EditorState loadedEditorState;

    DocRoot* newDocRoot=new DocRoot;
    QString loadError;
    if(newDocRoot->load(&file, &loadedEditorState, &loadError))
    {
        // Success
        QApplication::restoreOverrideCursor();

        // delete controller, then document referenced by controller
        delete controller; controller=NULL;
        delete docRoot;

        docRoot=newDocRoot;

        setCurrentFile(filePath);

        // new controller setup
        controller=new Controller(this, docRoot, view, loadedEditorState);

        // Change current directory to this file's directory
        QFileInfo fileInfo(filePath);
        QDir::setCurrent(fileInfo.canonicalPath());
        return true;
    }
    else
    {
        // failed to load
        QApplication::restoreOverrideCursor();

        delete newDocRoot;

        removeFromRecentFileList(filePath);

        QMessageBox::warning(this, tr("File format error"),
                             tr("Failed to load") + "\n\n" + filePath + "\n\n" +
                             (loadError.isEmpty() ? tr("Wrong file format.") : loadError));
        return false;
    }
}

bool MainWindow::saveFile(const QString &filePath, const ConversionOptions& conversionOptions)
{
    // QSaveFile stages output beside the target and replaces it only after a
    // complete successful write, preserving the previous file on failure.
    QSaveFile saveFile(filePath);
    if(!saveFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("File save error"),
                             tr("Cannot write file\n\n%1\n\n%2")
                             .arg(filePath).arg(saveFile.errorString()));
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success=docRoot->save(&saveFile, view->getEditorState(), conversionOptions);
    if(success)
        success=saveFile.commit();
    else
        saveFile.cancelWriting();
    QApplication::restoreOverrideCursor();

    if(!success)
    {
        QMessageBox::warning(this, tr("File save error"),
                             tr("Failed to save file\n\n%1\n\n%2")
                             .arg(filePath).arg(saveFile.errorString()));
        return false;
    }

    if(conversionOptions.savingOriginalFile)
    {
        setCurrentFile(filePath);   // Change current file path and add to LRU file list
        controller->setClean();
    }
    return true;
}

void MainWindow::setCurrentFile(const QString &filePath)
{
    static int sequenceNumber = 1;

    untitled = filePath.isEmpty();
    if (untitled) {
        currentFilePath = tr("Song%1.mid").arg(sequenceNumber++);
    } else {
        currentFilePath = QFileInfo(filePath).canonicalFilePath();
    }

    setWindowTitle(tr("%1[*] - %2").arg(strippedName(currentFilePath)).arg(windowTitleAppName));

    if(!untitled)
    {
        // Update recent file list
        Settings* settings=getApp()->getSettings();

        settings->recentFileList.removeAll(currentFilePath);
        settings->recentFileList.prepend(currentFilePath);

        while(settings->recentFileList.size() > MaxRecentFiles)
            settings->recentFileList.removeLast();

        settings->write();

        foreach (QWidget *widget, QApplication::topLevelWidgets()) {
            MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
            if (mainWin)
                mainWin->updateRecentFileActions();
        }
    }
}

void MainWindow::removeFromRecentFileList(const QString& filePath)
{
    // Remove file from recent file list if it is currently listed there
    Settings* settings=getApp()->getSettings();
    settings->recentFileList.removeAll(QFileInfo(filePath).absoluteFilePath());
    settings->write();

    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
        if (mainWin)
            mainWin->updateRecentFileActions();
    }
}

QString MainWindow::strippedName(const QString &filePath)
{
    return QFileInfo(filePath).fileName();
}

MainWindow *MainWindow::findMainWindow(const QString& relativePath, const QString& filePath)
{
    QString currentPath=QDir::currentPath();
    QDir::setCurrent(relativePath);

    QString canonicalFilePath = QFileInfo(filePath).canonicalFilePath();

    QDir::setCurrent(currentPath);

    if(canonicalFilePath.isEmpty())
        return NULL;    // file not found

    foreach (QWidget *widget, qApp->topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
        if (mainWin && mainWin->currentFilePath == canonicalFilePath)
            return mainWin;
    }
    return NULL;
}

void MainWindow::menuAboutToShow()
{
    controller->viewMenuAboutToShow();
}

EditorScrollBar::EditorScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent), decrementButton(new QToolButton(this)),
      incrementButton(new QToolButton(this))
{
    decrementButton->setObjectName(QStringLiteral("scrollArrowButton"));
    incrementButton->setObjectName(QStringLiteral("scrollArrowButton"));
    decrementButton->setFocusPolicy(Qt::NoFocus);
    incrementButton->setFocusPolicy(Qt::NoFocus);
    decrementButton->setAutoRaise(false);
    incrementButton->setAutoRaise(false);
    decrementButton->setIcon(QIcon(orientation == Qt::Horizontal
                                   ? QStringLiteral(":/images/flat/scroll-left.svg")
                                   : QStringLiteral(":/images/flat/scroll-up.svg")));
    incrementButton->setIcon(QIcon(orientation == Qt::Horizontal
                                   ? QStringLiteral(":/images/flat/scroll-right.svg")
                                   : QStringLiteral(":/images/flat/scroll-down.svg")));
    decrementButton->setIconSize(QSize(12, 12));
    incrementButton->setIconSize(QSize(12, 12));
    decrementButton->setFixedSize(14, 14);
    incrementButton->setFixedSize(14, 14);
    connect(decrementButton, &QToolButton::clicked, this, [this]() {
        triggerAction(QAbstractSlider::SliderSingleStepSub);
    });
    connect(incrementButton, &QToolButton::clicked, this, [this]() {
        triggerAction(QAbstractSlider::SliderSingleStepAdd);
    });
}

void EditorScrollBar::resizeEvent(QResizeEvent *event)
{
    QScrollBar::resizeEvent(event);
    if(orientation() == Qt::Horizontal)
    {
        decrementButton->move(0, (height() - decrementButton->height()) / 2);
        incrementButton->move(width() - incrementButton->width(), (height() - incrementButton->height()) / 2);
    }
    else
    {
        decrementButton->move((width() - decrementButton->width()) / 2, 0);
        incrementButton->move((width() - incrementButton->width()) / 2, height() - incrementButton->height());
    }
    decrementButton->raise();
    incrementButton->raise();
}

void EditorScrollBar::contextMenuEvent(QContextMenuEvent *event)
{
    // Modified version of QScrollBar::contextMenuEvent(...):  No action "actScrollHere"
    bool horiz = orientation() == Qt::Horizontal;
    QPointer<QMenu> menu = new QMenu(this);
    QAction *actScrollTop =  menu->addAction(horiz ? tr("Left edge") : tr("Top"));
    QAction *actScrollBottom = menu->addAction(horiz ? tr("Right edge") : tr("Bottom"));
    menu->addSeparator();
    QAction *actPageUp = menu->addAction(horiz ? tr("Page left") : tr("Page up"));
    QAction *actPageDn = menu->addAction(horiz ? tr("Page right") : tr("Page down"));
    menu->addSeparator();
    QAction *actScrollUp = menu->addAction(horiz ? tr("Scroll left") : tr("Scroll up"));
    QAction *actScrollDn = menu->addAction(horiz ? tr("Scroll right") : tr("Scroll down"));
    QAction *actionSelected = menu->exec(event->globalPos());
    delete menu;
    if (actionSelected == 0)
        /* do nothing */ ;
    else if (actionSelected == actScrollTop)
        triggerAction(QAbstractSlider::SliderToMinimum);
    else if (actionSelected == actScrollBottom)
        triggerAction(QAbstractSlider::SliderToMaximum);
    else if (actionSelected == actPageUp)
        triggerAction(QAbstractSlider::SliderPageStepSub);
    else if (actionSelected == actPageDn)
        triggerAction(QAbstractSlider::SliderPageStepAdd);
    else if (actionSelected == actScrollUp)
        triggerAction(QAbstractSlider::SliderSingleStepSub);
    else if (actionSelected == actScrollDn)
        triggerAction(QAbstractSlider::SliderSingleStepAdd);
}

void ToolBarSpinBox::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Escape:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        mainWindow->getView()->setFocus();
        break;
    default:
        QSpinBox::keyPressEvent(event);
        break;
    }
}
