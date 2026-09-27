/***************************************************************************
 *  speedymidiapp.cpp - Singleton Application Class
 *                      Owner of MIDI Interface, and Mouse Piano
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

#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "launchdialog.h"
#include "trackwizarddialog.h"
#include "doc_measureitem.h"
#include "measurepropertiesdialog.h"

#include "mousepianodockwidget.h"
#include "mousepianowidget.h"

#include "midiinterface.h"

#include <QDir>
#include <QMessageBox>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextStream>
#include <QTranslator>

SpeedyMidiApp::SpeedyMidiApp(int &argc, char **argv)
        : QtSingleApplication("SpeedyMIDI", argc, argv)
{
    settings=NULL;

    messageTranslator=NULL;
    qtMessageTranslator=NULL;
    musicalTranslator=NULL;

    launchDialogDisplayed=false;

    actionView_MousePiano=NULL;
    actionView_Toolbar=NULL;
    actionOptions_MousePianoPercussionMode=NULL;
    actionOptions_MidiThru=NULL;

    mousePianoDockWidget=NULL;
    mousePianoWidget=NULL;

    midiInterface=NULL;

    midiInputAvailable=false;
    midiOutputAvailable=false;

    for(int i=0; i < MIDI_N_NOTE_NUMBERS; ++i)
    {
        midiKeyPressedArray[i]=false;
        keypressSerialNumbersArray[i]=0;
    }

    nextKeypressSerialNumber=1;     // 1 is the first valid serial number. 0 means invalid

    // ---------------------------------------------------------------------------------------------------

    // store path to executable, because applicationDirPath() is unreliable when changing the working directory
    appExecutablePath=applicationDirPath();

    connect(this, SIGNAL(aboutToQuit()), SLOT(aboutToQuitCleanup()));

    // QApplication may have removed some arguments up to here. Parse the rest now.
    //  Skip executable file name in argv[0].
    //  Add current working directory in front, so other process will handle relative paths correctly.
    filesToOpenMessage+=QDir::currentPath() + "\n";

    for (int i = 1; i < argc; ++i)
    {
        // Convert directory to slash notation regardless of operating system.
        //  This is important for LRU file list comparisons.
        QString filePath=QDir::cleanPath(argv[i]);

        filesToOpenMessage += filePath;
        filesToOpenList.append(filePath);

        if (i < argc-1) filesToOpenMessage += "\n";
    }
}

int SpeedyMidiApp::exec()
{
    // Check for another running instance and send it the files to be opened
    if(sendMessage(filesToOpenMessage))
        return 0;   // Another running instance found. Terminate this duplicate instance.

    // ---------------------------------------------------------------------------------------------------
    // We are the only running instance of this application

    settings=new Settings();
    setupAppGlobalUi();
    setupTranslators();
    initMidi();

    connect(this, SIGNAL(messageReceived(const QString&)), this, SLOT(handleInstanceMessage(const QString&)));

    // If files were given on command line, open them now in multiple main windows
    if(!filesToOpenList.isEmpty())
    {
        MainWindow* w=new MainWindow(QDir::currentPath(), filesToOpenList);
        w->show();
    }
    else    // no files given on command line
    {
        execLaunchDialog();
    }

    // If any main window was created by opening a file or launch dialog, start message loop now
    int retCode=0;
    if(mainWindowExists())
        retCode=QApplication::exec();
    else
        aboutToQuitCleanup();

    return retCode;
}

void SpeedyMidiApp::setupTranslators()
{
    // Precondition: messageTranslationLocaleName and musicalTranslationLocaleName are valid locales.

    // Remove current translators
    if(messageTranslator)
    {
        removeTranslator(messageTranslator);
        delete messageTranslator;
        messageTranslator=NULL;
    }
    if(qtMessageTranslator)
    {
        removeTranslator(qtMessageTranslator);
        delete qtMessageTranslator;
        qtMessageTranslator=NULL;
    }
    if(musicalTranslator)
    {
        removeTranslator(musicalTranslator);
        delete musicalTranslator;
        musicalTranslator=NULL;
    }

    // reload new translators
    QString translationsDir=appExecutablePath + '/' + APP_TRANSLATOR_PATH_PREFIX;
    QString filename;

    // use message locale for own and standard Qt messages
    messageTranslator=new QTranslator(this);
    filename=APP_TRANSLATOR_MSG_PREFIX + settings->messageTranslationLocaleName;

    if(messageTranslator->load(filename, translationsDir))
    {
        installTranslator(messageTranslator);
    }
    else
    {
        qDebug() << "unable to load message translator" << filename;

        // fallback: try english
        settings->messageTranslationLocaleName="en";
        filename=APP_TRANSLATOR_MSG_PREFIX + settings->messageTranslationLocaleName;

        if(messageTranslator->load(filename, translationsDir))
        {
            installTranslator(messageTranslator);
        }
        else
        {
            qDebug() << "unable to load fallback message translator" << filename;
            delete messageTranslator;
            messageTranslator=NULL;
        }
    }

    qtMessageTranslator=new QTranslator(this);
    filename=APP_TRANSLATOR_QT_PREFIX + settings->messageTranslationLocaleName;

    if(qtMessageTranslator->load(filename, translationsDir))
    {
        installTranslator(qtMessageTranslator);
    }
    else
    {
        qDebug() << "unable to Qt load message translator" << filename;
        delete qtMessageTranslator;
        qtMessageTranslator=NULL;
    }

    // Use maybe different locale for musical translations
    musicalTranslator=new QTranslator(this);
    filename=APP_TRANSLATOR_MUSIC_PREFIX + settings->musicalTranslationLocaleName;

    if(musicalTranslator->load(filename, translationsDir))
    {
        installTranslator(musicalTranslator);
    }
    else
    {
        qDebug() << "unable to load musical names and symbols message translator" << filename;

        // fallback: try english
        settings->musicalTranslationLocaleName="en";
        filename=APP_TRANSLATOR_MUSIC_PREFIX + settings->musicalTranslationLocaleName;

        if(musicalTranslator->load(filename, translationsDir))
        {
            installTranslator(musicalTranslator);
        }
        else
        {
            qDebug() << "unable to load fallback musical names and symbols message translator" <<
                    filename;

            delete musicalTranslator;
            musicalTranslator=NULL;
        }
    }
}

void SpeedyMidiApp::setupAppGlobalUi()
{
    setApplicationName(QStringLiteral("Speed MIDI Editor"));
    setApplicationDisplayName(QStringLiteral("Speed MIDI Editor"));

    // Apply the approved blue-gray theme before creating any application windows.
    // Fusion paints Qt controls from the application palette instead of inheriting
    // dark native widget surfaces from the macOS appearance.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);

    QPalette editorPalette;
    const QColor windowColor("#c4cfda");
    const QColor surfaceColor("#e5eaf0");
    const QColor canvasColor("#dbe3eb");
    const QColor textColor("#202a35");
    const QColor mutedTextColor("#475768");
    const QColor accentColor("#2f65a0");

    const QPalette::ColorGroup enabledGroups[] = { QPalette::Active, QPalette::Inactive };
    for(QPalette::ColorGroup group : enabledGroups)
    {
        editorPalette.setColor(group, QPalette::Window, windowColor);
        editorPalette.setColor(group, QPalette::WindowText, textColor);
        editorPalette.setColor(group, QPalette::Base, canvasColor);
        editorPalette.setColor(group, QPalette::AlternateBase, QColor("#d3dce5"));
        editorPalette.setColor(group, QPalette::Text, textColor);
        editorPalette.setColor(group, QPalette::Button, surfaceColor);
        editorPalette.setColor(group, QPalette::ButtonText, textColor);
        editorPalette.setColor(group, QPalette::BrightText, QColor("#a43843"));
        editorPalette.setColor(group, QPalette::Light, QColor("#f5f7f9"));
        editorPalette.setColor(group, QPalette::Midlight, QColor("#e0e6ec"));
        editorPalette.setColor(group, QPalette::Mid, QColor("#bac6d2"));
        editorPalette.setColor(group, QPalette::Dark, QColor("#98a9b9"));
        editorPalette.setColor(group, QPalette::Shadow, QColor("#75879a"));
        editorPalette.setColor(group, QPalette::Highlight, accentColor);
        editorPalette.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
        editorPalette.setColor(group, QPalette::Link, accentColor);
        editorPalette.setColor(group, QPalette::ToolTipBase, QColor("#f4f7fa"));
        editorPalette.setColor(group, QPalette::ToolTipText, textColor);
        editorPalette.setColor(group, QPalette::PlaceholderText, mutedTextColor);
    }
    editorPalette.setColor(QPalette::Disabled, QPalette::WindowText, mutedTextColor);
    editorPalette.setColor(QPalette::Disabled, QPalette::Text, mutedTextColor);
    editorPalette.setColor(QPalette::Disabled, QPalette::ButtonText, mutedTextColor);
    editorPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#b7c5d3"));
    editorPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#475768"));
    setPalette(editorPalette);
    setStyleSheet(QStringLiteral(R"(
        QMenuBar {
            background: #347cae;
            color: #ffffff;
            border-bottom: 1px solid #244d7a;
        }
        QMenuBar::item { padding: 5px 10px; background: transparent; border: 1px solid transparent; }
        QMenuBar::item:selected {
            background: #286a9c;
            border: 1px solid #a7d2f0;
            border-radius: 3px;
        }
        QMenu {
            background-color: #e5eaf0;
            color: #202a35;
            border: 1px solid #7c9fc4;
            padding: 4px;
        }
        QMenu::item { padding: 5px 24px 5px 10px; background: transparent; }
        QMenu::item:selected { background: #c6dcf2; color: #173b62; }
        QMenu::separator { height: 1px; background: #b5c9de; margin: 4px 6px; }
        QToolBar {
            background: #dce9f5;
            border: 0;
            border-bottom: 1px solid #8eaed0;
            spacing: 4px;
            padding: 3px;
        }
        QToolBar QToolButton {
            color: #202a35;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 2px;
        }
        QToolBar QToolButton:hover { background: #edf5fc; border-color: #9cb9d9; }
        QToolBar QToolButton:pressed,
        QToolBar QToolButton:checked { background: #c0d8f1; border-color: #7fa6d1; }
        QScrollBar:horizontal {
            background: #cbd9e7;
            height: 14px;
            border: 1px solid #607b98;
        }
        QScrollBar:vertical {
            background: #cbd9e7;
            width: 14px;
            border: 1px solid #607b98;
        }
        QScrollBar::groove:horizontal { margin-left: 14px; margin-right: 14px; }
        QScrollBar::groove:vertical { margin-top: 14px; margin-bottom: 14px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::handle:horizontal {
            background: #5a8bb9;
            min-width: 30px;
            border: 1px solid #3f6890;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #5a8bb9;
            min-height: 30px;
            border: 1px solid #3f6890;
            border-radius: 4px;
        }
        QScrollBar::handle:hover { background: #6798c5; }
        QScrollBar::handle:pressed { background: #4776a3; }
        QToolButton#scrollArrowButton {
            background: #416f9f;
            border: 1px solid #31577f;
            padding: 0;
            margin: 0;
        }
        QToolButton#scrollArrowButton:hover { background: #3d74a8; }
        QToolButton#scrollArrowButton:pressed { background: #254e78; }
        QScrollBar::add-page, QScrollBar::sub-page { background: #cbd9e7; }
        QSpinBox, QComboBox, QLineEdit {
            background: #e5eaf0;
            color: #202a35;
            selection-background-color: #2f65a0;
            selection-color: #ffffff;
            border: 1px solid #bac6d1;
            border-radius: 4px;
            padding: 3px;
        }
        QAbstractItemView {
            background: #dbe3eb;
            color: #202a35;
            selection-background-color: #2f65a0;
            selection-color: #ffffff;
        }
        QStatusBar { background: #dce8f4; color: #475768; border-top: 1px solid #a9bfd6; }
    )"));

    // -----------------------------------------------------------------------------------------------
    // Action setup

    actionView_MousePiano                  = new QAction(this);
    actionView_Toolbar                     = new QAction(this);
    actionOptions_MousePianoPercussionMode = new QAction(this);
    actionOptions_MidiThru                 = new QAction(this);

    actionView_MousePiano                 ->setObjectName(QString::fromUtf8("actionView_MousePiano"));
    actionView_Toolbar                    ->setObjectName(QString::fromUtf8("actionView_Toolbar"));
    actionOptions_MousePianoPercussionMode->setObjectName(QString::fromUtf8("actionOptions_MousePianoPercussionMode"));
    actionOptions_MidiThru                ->setObjectName(QString::fromUtf8("actionOptions_MidiThru"));

    actionView_MousePiano                 ->setCheckable(true);
    actionView_Toolbar                    ->setCheckable(true);
    actionOptions_MousePianoPercussionMode->setCheckable(true);
    actionOptions_MidiThru                ->setCheckable(true);

    QIcon iconMousePiano;
    QIcon iconDrum;
    iconMousePiano.addFile(QString::fromUtf8(":/images/flat/piano.svg"), QSize(), QIcon::Normal, QIcon::Off);
    iconDrum      .addFile(QString::fromUtf8(":/images/flat/drum.svg" ), QSize(), QIcon::Normal, QIcon::Off);

    actionView_MousePiano                 ->setIcon(iconMousePiano);
    actionView_MousePiano                 ->setIconVisibleInMenu(false);     // never show the icon in menu

    actionOptions_MousePianoPercussionMode->setIcon(iconDrum);

    actionView_Toolbar                    ->setChecked(settings->mainWindowToolbarVisible);
    actionView_MousePiano                 ->setChecked(settings->mousePiano.visible);
    actionOptions_MousePianoPercussionMode->setChecked(settings->mousePiano.percussionMode);
    actionOptions_MidiThru                ->setChecked(settings->midiThru);

    connect(actionView_MousePiano,                  SIGNAL(toggled(bool)),   this, SLOT(showMousePiano(bool)));
    connect(actionView_Toolbar,                     SIGNAL(toggled(bool)),   this, SLOT(showToolBar(bool)));
    connect(actionOptions_MousePianoPercussionMode, SIGNAL(triggered(bool)), this, SLOT(actionOptions_MousePianoPercussionMode_Triggered(bool)));
    connect(actionOptions_MidiThru,                 SIGNAL(triggered(bool)), this, SLOT(actionOptions_MidiThru_Triggered(bool)));

    // -----------------------------------------------------------------------------------------------
    // Set default icon for all windows and dialogs
    QIcon appIcon;
    appIcon.addPixmap(QPixmap(":/images/app_icon_16.png"));
    appIcon.addPixmap(QPixmap(":/images/app_icon_32.png"));
    appIcon.addPixmap(QPixmap(":/images/app_icon_48.png"));
    appIcon.addPixmap(QPixmap(":/images/app_icon_96.png"));
    appIcon.addPixmap(QPixmap(":/images/app_icon_256.png"));
    setWindowIcon(appIcon);

    // -----------------------------------------------------------------------------------------------
    // mouse piano setup
    mousePianoDockWidget=new MousePianoDockWidget(NULL);
    mousePianoWidget=new MousePianoWidget(mousePianoDockWidget);
    mousePianoDockWidget->setWidget(mousePianoWidget);

    mousePianoDockWidget->resize(settings->mousePiano.size);

    // Set dock widget _position_ only after dock widget has been shown for the first time.
    //  Setting the position directly here would give problems because of unknown window frame size.

    connect(mousePianoWidget,SIGNAL(keyPressed (int)),SLOT(mousePianoKeyPressed (int)));
    connect(mousePianoWidget,SIGNAL(keyReleased(int)),SLOT(mousePianoKeyReleased(int)));

    // -----------------------------------------------------------------------------------------------

    retranslateAppGlobalUi();
}

void SpeedyMidiApp::retranslateAppGlobalUi()
{
    actionView_MousePiano                 ->setText(QApplication::translate("MainWindow", "Mouse &piano"));
    actionView_Toolbar                    ->setText(QApplication::translate("MainWindow", "&Toolbar"));
    actionOptions_MousePianoPercussionMode->setText(QApplication::translate("MainWindow", "&Mouse Piano Percussion"));
    actionOptions_MidiThru                ->setText(QApplication::translate("MainWindow", "MIDI &Thru"));

    actionView_MousePiano                 ->setShortcut(QApplication::translate("MainWindow", "F11"));
    actionView_Toolbar                    ->setShortcut(QApplication::translate("MainWindow", "F12"));
    actionOptions_MousePianoPercussionMode->setShortcut(QApplication::translate("MainWindow", "P"));

#ifndef QT_NO_TOOLTIP
    actionView_MousePiano                 ->setToolTip(QApplication::translate("MainWindow", "Show or hide mouse piano"));
    actionView_Toolbar                    ->setToolTip(QApplication::translate("MainWindow", "Show or hide the toolbar"));
    actionOptions_MousePianoPercussionMode->setToolTip(QApplication::translate("MainWindow", "Set mouse piano to percussion or normal mode"));
    actionOptions_MidiThru                ->setToolTip(QApplication::translate("MainWindow", "Enable or disable MIDI Thru"));
#endif // QT_NO_TOOLTIP
}

bool SpeedyMidiApp::event(QEvent *e)
{
    bool b=QtSingleApplication::event(e);

    switch(e->type())
    {
    case QEvent::LanguageChange:
        retranslateAppGlobalUi();
        break;
    default:
        break;
    }
    return b;
}

void SpeedyMidiApp::handleInstanceMessage(QString message)
{
    // convert message lines into array of file names to open
    QTextStream textStream(&message);
    QStringList filesToOpenList;

    // First entry is current working directory of sending process.
    //  Knowlegde about this is useful to interpret paths relative to other process'es CWD correctly.
    QString otherProcessCurrentPath=textStream.readLine();

    while(!textStream.atEnd())
    {
        QString filename=textStream.readLine();
        if(!filename.isEmpty())
            filesToOpenList.append(filename);
    }

    if(filesToOpenList.isEmpty())
    {
        // no files specified, show launch dialog
        execLaunchDialog();
    }
    else
    {
        // Try to reach "open" call to active main window.
        //  If no main window is active, take any one of the existing main windows.
        //  If no such window exists, create first main window.

        // This procedure makes sure that
        //  1) opened documents are cascaded correctly
        //  2) already opened documents will only be re-activated and not be opened again
        //  3) existing unmodified, untitled documents are re-used

        MainWindow* mainWindow = getActiveOrFallbackMainWindow();
        if(mainWindow == NULL)
        {
            // No main window exists so far, create a new one
            MainWindow* w=new MainWindow(otherProcessCurrentPath, filesToOpenList);
            w->show();
        }
        else
        {
            mainWindow->setWindowState(mainWindow->windowState() & ~Qt::WindowMinimized);
            mainWindow->raise();
            mainWindow->activateWindow();
            mainWindow->show();

            // Open all files using active main window, but never re-use the window as it may be in any
            //  modal state or have an popups/dialogs open
            mainWindow->openFiles(otherProcessCurrentPath, filesToOpenList, false);
        }
    }
}

void SpeedyMidiApp::execLaunchDialog()
{
    // No recursion allowed. This flag is necessary because instanceMessages may occur at any time
    //  and only one launch dialog should be displayed simultaneously.
    if(launchDialogDisplayed)return;
    launchDialogDisplayed=true;

    // Show launch dialog until selected operation succeeded or closed manually.
    while(true)
    {
        // Show launch dialog
        LaunchDialog launchDlg;
        launchDlg.exec();

        // Try to reach "new" or "open" calls to active main window.
        //  If no main window is active, take any one of the existing main windows.
        //  If no such window exists, create first main window.

        // This procedure makes sure that
        //  1) new/opened documents are cascaded correctly
        //  2) already opened documents will only be re-activated and not be opened again
        //  3) existing unmodified, untitled documents are re-used for "open"

        switch(launchDlg.result)
        {
        case LaunchDialog::LDR_NewDocumentWithWizard:
            {
                MainWindow* mainWindow = getActiveOrFallbackMainWindow();
                if(mainWindow == NULL)
                {
                    // create main window with default document
                    MainWindow* w=createDocumentByWizard();
                    if(w)w->show();
                    else continue;   // Error occurred, show launch dialog again
                }
                else
                {
                    mainWindow->setWindowState(mainWindow->windowState() & ~Qt::WindowMinimized);
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    mainWindow->show();

                    if(!mainWindow->createSiblingUsingWizard())
                        continue;   // Error occurred, show launch dialog again
                }
            }
            break;

        case LaunchDialog::LDR_NewDefaultDocument:
            {
                MainWindow* mainWindow = getActiveOrFallbackMainWindow();
                if(mainWindow == NULL)
                {
                    // create main window with default document
                    MainWindow* w=new MainWindow();
                    w->show();
                }
                else
                {
                    mainWindow->setWindowState(mainWindow->windowState() & ~Qt::WindowMinimized);
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    mainWindow->show();

                    mainWindow->createSiblingDefaultDocument();
                }
            }
            break;

        case LaunchDialog::LDR_OpenDocument:
            {
                MainWindow* mainWindow = getActiveOrFallbackMainWindow();
                if(mainWindow == NULL)
                {
                    // No main window exists so far, create a new one to load specified document
                    MainWindow* w=new MainWindow(QDir::currentPath(), QStringList(launchDlg.openFilePath));
                    if(!w->isUntitled())
                    {
                        w->show();
                    }
                    else    // loading failed, show launch dialog again
                    {
                        delete w;
                        continue;
                    }
                }
                else
                {
                    mainWindow->setWindowState(mainWindow->windowState() & ~Qt::WindowMinimized);
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    mainWindow->show();

                    // Open all files using active main window, but never re-use the window as it may be in any
                    //  modal state or have any popups/dialogs open
                    mainWindow->openFiles(QDir::currentPath(), QStringList(launchDlg.openFilePath), false);
                }
            }
            break;

        case LaunchDialog::LDR_Cancel:
            break;
        }

        // exit launch dialog loop
        break;
    }

    launchDialogDisplayed=false;
}

bool SpeedyMidiApp::mainWindowExists() const
{
    foreach (QWidget *widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin)return true;
    }
    return false;
}

MainWindow* SpeedyMidiApp::getActiveOrFallbackMainWindow() const
{
    MainWindow* activeMainWindow=NULL;
    MainWindow* anyMainWindow=NULL;

    foreach (QWidget *widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin)
        {
            anyMainWindow=mainWin;

            if(mainWin->isActiveWindow())
            {
                activeMainWindow=mainWin;
                break;
            }
        }
    }

    return activeMainWindow != NULL ? activeMainWindow : anyMainWindow;
}

MainWindow* SpeedyMidiApp::createDocumentByWizard()
{
    // start wizard sequence procedure

    // Page 1: Track Wizard
    TrackWizardDialog trackWizardDlg;
    trackWizardDlg.assignPatches=settings->LRU.trackWizardAssignPatches;
    if(trackWizardDlg.exec() != QDialog::Accepted)
        return NULL;    // User break

    // Page 2: First measure properties
    MeasurePropertiesDialog measurePropertiesDlg;
    if(measurePropertiesDlg.exec() != QDialog::Accepted)
        return NULL;    // User break

    // Save LRU value
    settings->LRU.trackWizardAssignPatches=trackWizardDlg.assignPatches;

    DocMeasureItem firstMeasureProperties(measurePropertiesDlg.getSetupWizardMeasureProperties());
    firstMeasureProperties.clean();

    // Create new document and editor
    return new MainWindow(firstMeasureProperties, trackWizardDlg.tracksToAdd);
}

void SpeedyMidiApp::midiKeyPressed(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);

    midiKeyPressedArray[noteNumber]=true;
    keypressSerialNumbersArray[noteNumber]=nextKeypressSerialNumber++;
    updateMousePiano(noteNumber);
}

void SpeedyMidiApp::midiKeyReleased(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);

    midiKeyPressedArray[noteNumber]=false;
    keypressSerialNumbersArray[noteNumber]=0;     // 0 is invalid serial number
    updateMousePiano(noteNumber);
}

void SpeedyMidiApp::mousePianoKeyPressed(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);

    // Immediate MIDI output
    if(midiOutputAvailable)
    {
        int midiChannel= settings->mousePiano.percussionMode ?
                         settings->mousePiano.midiChannelPercussion :
                         settings->mousePiano.midiChannelChromatic;

        midiInterface->writeShortMessage(MidiShortMsg(0,
                                                      0x90 + midiChannel-1,
                                                      noteNumber,
                                                      settings->mousePiano.midiVelocity));
    }

    if(keypressSerialNumbersArray[noteNumber] == 0)   // Changed the key state?
    {
        keypressSerialNumbersArray[noteNumber]=nextKeypressSerialNumber++;
        updateMousePiano(noteNumber);
    }
}

void SpeedyMidiApp::mousePianoKeyReleased(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);

    // Immediate MIDI output
    if(midiOutputAvailable)
    {
        int midiChannel= settings->mousePiano.percussionMode ?
                         settings->mousePiano.midiChannelPercussion :
                         settings->mousePiano.midiChannelChromatic;

        midiInterface->writeShortMessage(MidiShortMsg(0,
                                                      0x80 + midiChannel-1,
                                                      noteNumber,
                                                      0x40));
    }

    // MIDI input has priority over mouse piano, so only reset the keypress serial number
    //  when the corresponding MIDI key is currently not pressed.
    if(!midiKeyPressedArray[noteNumber])
    {
        keypressSerialNumbersArray[noteNumber]=0;   // 0 = "not pressed"
        updateMousePiano(noteNumber);
    }
}

void SpeedyMidiApp::updateMousePiano(int noteNumber)
{
    Q_ASSERT(noteNumber >= 0 && noteNumber < MIDI_N_NOTE_NUMBERS);
    mousePianoWidget->updateKeyStatus(noteNumber);
}

void SpeedyMidiApp::quit()
{
    // close all main windows
    foreach (QWidget *widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin)
        {
            if(!mainWin->close())return;
        }
    }
}

void SpeedyMidiApp::aboutToQuitCleanup()
{
    // -----------------------------------------------------------------------------------------
    // Collect and write settings

    if(mousePianoDockWidget->onceShown)
    {
        // save mouse piano position only if once shown
        settings->mousePiano.dockWidgetFramePos = mousePianoDockWidget->pos();
        settings->mousePiano.size               = mousePianoDockWidget->size();
    }

    settings->write();

    // -----------------------------------------------------------------------------------------

    delete settings;
    settings=NULL;

    delete midiInterface;
    midiInterface=NULL;

    delete mousePianoWidget;
    mousePianoWidget=NULL;

    delete mousePianoDockWidget;
    mousePianoDockWidget=NULL;
}

void SpeedyMidiApp::initMidi()
{
    // Initialize MIDI interface
    midiInterface=new MidiInterface(this);
    connect(midiInterface,SIGNAL(midiKeyPressed(int)),SLOT(midiKeyPressed(int)));
    connect(midiInterface,SIGNAL(midiKeyReleased(int)),SLOT(midiKeyReleased(int)));

    // Initialize MIDI input and output devices. Check for valid settings.
    //  An empty string means "none", a "\x1" means no settings were found.
    if(settings->selectedInputDevice == "\x1" || settings->selectedOutputDevice == "\x1")
    {
        settings->selectedInputDevice.clear();
        settings->selectedOutputDevice.clear();

        // No settings found: Execute settings dialog.
//TODO: Decide whether to call on first time startup:        execPreferencesDialog(true);
    }
    else
    {
        openMidiInput();
        openMidiOutput();
    }

    // Set MIDI Thru state
    midiInterface->setMidiThru(settings->midiThru);
}

void SpeedyMidiApp::openMidiInput()
{
    if(!settings->selectedInputDevice.isEmpty())
    {
        if(!midiInterface->openInput(settings->selectedInputDevice))
        {
            // application modal message box
            QMessageBox::warning(NULL, tr("Error connecting to MIDI device"),
                                 tr("Cannot open MIDI input port \"%1\"").arg(settings->selectedInputDevice)
                                 + "\n\n"
                                 + midiInterface->getErrorText());
        }
    }
}

void SpeedyMidiApp::openMidiOutput()
{
    if(!settings->selectedOutputDevice.isEmpty())
    {
        midiOutputAvailable=midiInterface->openOutput(settings->selectedOutputDevice);
        if(!midiOutputAvailable)
        {
            // application modal message box
            QMessageBox::warning(NULL, tr("Error connecting to MIDI device"),
                                 tr("Cannot open MIDI output port \"%1\"").arg(settings->selectedOutputDevice)
                                 + "\n\n"
                                 + midiInterface->getErrorText());
        }
    }
}

void SpeedyMidiApp::execPreferencesDialog(bool startWithPageMidiDevices)
{
    stopAnyPlayback();

    midiInterface->rescanDevices();

    midiInputAvailable=false;
    midiOutputAvailable=false;

    if(settings->execPreferencesDialog(startWithPageMidiDevices))
    {
        setupTranslators();
    }

    // try to re-open input and output devices
    openMidiInput();
    openMidiOutput();
}

void SpeedyMidiApp::stopAnyPlayback()
{
    foreach (QWidget* widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if(mainWin)mainWin->stopPlayback();
    }
}

void SpeedyMidiApp::showMousePiano(bool visible)
{
    settings->mousePiano.visible=visible;

    reattachMousePiano();
}

void SpeedyMidiApp::reattachMousePiano()
{
    MainWindow* activeMainWindow = qobject_cast<MainWindow*>(activeWindow());

    // hide the piano if visibility action is not checked
    if(!settings->mousePiano.visible)mousePianoDockWidget->setVisible(false);

    foreach (QWidget* widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if(mainWin)
        {
            if(mainWin != activeMainWindow)
                mainWin->detachFromMousePiano();
        }
    }

    // show mouse piano dock for activated main window
    if(activeMainWindow)
        activeMainWindow->attachToMousePiano();
}

void SpeedyMidiApp::showToolBar(bool visible)
{
    if(visible == settings->mainWindowToolbarVisible)return;

    settings->mainWindowToolbarVisible=visible;

    // show or hide main toolbars in all main windows
    foreach (QWidget* widget, topLevelWidgets())
    {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if(mainWin)
            mainWin->getUI()->mainToolBar->setVisible(visible);
    }
}

void SpeedyMidiApp::actionOptions_MousePianoPercussionMode_Triggered(bool checked)
{
    // toggle off all notes in old mode (thus, in correct MIDI channel)
    mousePianoWidget->allNotesOff();

    // Now use the new mode
    settings->mousePiano.percussionMode=checked;
}

void SpeedyMidiApp::actionOptions_MidiThru_Triggered(bool checked)
{
    settings->midiThru=checked;
    midiInterface->setMidiThru(settings->midiThru);
}
