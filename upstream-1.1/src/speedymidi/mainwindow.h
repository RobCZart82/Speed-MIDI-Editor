/***************************************************************************
 *  mainwindow.h - Editor Main Window
 *                 Owner of Model, View, and Controller
 *                 (cf. Model–View–Controller Architecture)
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "global.h"
#include <QMainWindow>
#include <QScrollBar>
#include <QSpinBox>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Constructor 1: new default document
    MainWindow();

    // Constructor 2: new file initialized with settings collected by track wizard
    MainWindow(const DocMeasureItem& firstMeasureProperties, const QString& trackWizardString);

    // Constructor 3: open one or more files
    MainWindow(const QString& listRelativePath, const QStringList& filesToOpenList);

    ~MainWindow();

    bool createSiblingUsingWizard();
    void createSiblingDefaultDocument();

    void openFiles(const QString& listRelativePath, const QStringList& filesToOpenList, bool reuseCurrentWindowAllowed);
protected:
    void openFiles_(const QString& listRelativePath, const QStringList& filesToOpenList, bool reuseCurrentWindowAllowed);
    bool loadFile(const QString &filePath);
public:
    bool saveFile(const QString &filePath, const ConversionOptions& conversionOptions);
    static QString strippedName(const QString &filePath);
    static MainWindow *findMainWindow(const QString& relativePath, const QString& filePath);

    void stopPlayback();

    void attachToMousePiano();
    void detachFromMousePiano();

    // Selectors
    SpeedyMidiApp* getApp() const { return (SpeedyMidiApp*)qApp; }
    EditorScrollBar* getScrollBarHorizontal() const { return scrollBarHorizontal; }
    EditorScrollBar* getScrollBarVertical() const { return scrollBarVertical; }
    ZoomSliderWidget* getXZoomSliderWidget() const { return xZoomSliderWidget; }
    ZoomSliderWidget* getYZoomSliderWidget() const { return yZoomSliderWidget; }
    ToolBarSpinBox* getSpinBoxRelativePlaybackSpeed() const { return spinBoxRelativePlaybackSpeed; }

    View* getView() const { return view; }
    Ui::MainWindow* getUI() const { return ui; }

    bool isUntitled() const { return untitled; }
    bool isCreatedByWizard() const { return createdByWizard; }
    QString getCurrentFilePath() const { return currentFilePath; }

public slots:
    void show();
protected slots:
    void openFiles(const QStringList& absoluteFilePathList);     // Used by file CS to open a file.
                                                //  The file may be loaded in the current main window, 
                                                //  so the controller would be deleted while it's still 
                                                //  on the stack. 
                                                //  This effect is avoided by using a Qt::QueuedConnection.

    void menuAboutToShow();

protected:
    virtual bool event(QEvent *e);
    virtual void closeEvent(QCloseEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dropEvent(QDropEvent * event);

    void init();
    void retranslateUi();

    void setGeometryFromSettings();
    void copyGeometryToSettings();

    void cascadeSibling(MainWindow* sibling, int offsetFactor) const;
    void setCurrentFile(const QString &filePath);
    void removeFromRecentFileList(const QString& filePath);
    void updateRecentFileActions();

    // ------------------------------------------------------------
    // File name issues

    QString windowTitleAppName;
    QString currentFilePath;
    bool untitled;
    bool createdByWizard;

public:
    enum { MaxRecentFiles = 8 };
    QAction *recentFileActs[MaxRecentFiles];

protected:
    QAction *recentFileActsSeparatorAct;

    // ------------------------------------------------------------
    // Subwidgets

    View* view;
    EditorScrollBar* scrollBarHorizontal;
    EditorScrollBar* scrollBarVertical;
    ZoomSliderWidget* xZoomSliderWidget;
    ZoomSliderWidget* yZoomSliderWidget;
    ToolBarSpinBox* spinBoxRelativePlaybackSpeed;

    // ------------------------------------------------------------
    // Mouse piano attachment status

    bool mousePianoAttached;
    bool mousePianoAttachmentOK;

    // ------------------------------------------------------------
    // Document and controller owned by this main window

    DocRoot* docRoot;
    Controller* controller;

private:
    Ui::MainWindow *ui;
};

class EditorScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    EditorScrollBar( Qt::Orientation orientation, QWidget * parent = 0 )  : QScrollBar(orientation,parent) { }
    virtual void contextMenuEvent(QContextMenuEvent *);
};

class ToolBarSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    ToolBarSpinBox(MainWindow* mainWindow)  : QSpinBox(NULL) { this->mainWindow=mainWindow; }
    virtual void keyPressEvent(QKeyEvent* event);
    MainWindow* mainWindow;
};

#endif // MAINWINDOW_H
