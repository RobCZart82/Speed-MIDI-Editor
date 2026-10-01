/***************************************************************************
 *  cs_file.cpp - Controller Subsystem: File
 *                (New, Open, Save, Close, Quit ...)
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

#include "cs_file.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "speedymidiapp.h"
#include "settings.h"
#include "doc_root.h"
#include "conversionoptionsdialog.h"
#include "partextractiondialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QPushButton>
#include <QPainter>
#include <QSvgRenderer>
#include <QSaveFile>

CS_File::CS_File(Controller* controller)
        : CS_Common(controller)
{
    // guard types used for handlers
    Controller::ActionGuards gDIC = Controller::DisallowWhenInputCaptured;
    Controller::ActionGuards gCIS = Controller::CancelInterruptibleStates;
    Controller::ActionGuards gMHT = Controller::MustHaveTracks;

    registerActionHandler(ui->actionFile_ShowLaunchDialog,  "actionFile_ShowLaunchDialog_Triggered", gDIC | gCIS);
    registerActionHandler(ui->actionFile_NewWithWizard,     "actionFile_NewWithWizard_Triggered", gDIC | gCIS);
    registerActionHandler(ui->actionFile_NewDefaultDocument,"actionFile_NewDefaultDocument_Triggered", gDIC);
    registerActionHandler(ui->actionFile_Open,              "actionFile_Open_Triggered", gDIC);
    registerActionHandler(ui->actionFile_Close,             "actionFile_Close_Triggered", gDIC | gCIS);
    registerActionHandler(ui->actionFile_Save,              "actionFile_Save_Triggered", gDIC);
    registerActionHandler(ui->actionFile_SaveAs,            "actionFile_SaveAs_Triggered", gDIC);
    registerActionHandler(ui->actionFile_SaveCompatibleFile,"actionFile_SaveCompatibleFile_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionFile_ExtractParts,      "actionFile_ExtractParts_Triggered", gDIC | gMHT);
    registerActionHandler(ui->actionFile_Quit,              "actionFile_Quit_Triggered", gDIC | gCIS);

    // recent file connections
    for(int i=0; i < mainWindow->MaxRecentFiles; ++i)
        registerActionHandler(mainWindow->recentFileActs[i], "actionFile_OpenRecentFile_Triggered", gDIC);

    registerActionHandler(ui->actionOptions_Preferences,    "actionOptions_Preferences_Triggered", gDIC | gCIS);

    registerActionHandler(ui->actionHelp_About,             "actionHelp_About_Triggered", gDIC);
}

bool CS_File::closeEvent()
{
    return maybeSave();
}

void CS_File::actionFile_ShowLaunchDialog_Triggered()
{
    app->execLaunchDialog();
}

void CS_File::actionFile_NewWithWizard_Triggered()
{
    mainWindow->createSiblingUsingWizard();
}

void CS_File::actionFile_NewDefaultDocument_Triggered()
{
    mainWindow->createSiblingDefaultDocument();
}

void CS_File::actionFile_Open_Triggered()
{
    QString filePath = QFileDialog::getOpenFileName(mainWindow,
                                                    tr("Open MIDI file"),
                                                    QDir::currentPath(),
                                                    tr("MIDI files (*.mid);;All files (*.*)"));
    if(!filePath.isEmpty())
    {
        // BEWARE: We must use a Qt::QueuedConnection to open the file!

        //  The file might be opened reusing the current main window and view,
        //  the controller and all subsystems (also *this) will be destroyed in this case.
        //  By calling MainWindow::openFiles we would get stack corruption.

        // The only action handlers where this special implementation is required are:
        //  - actionFile_Open_Triggered
        //  - actionFile_OpenRecentFile_Triggered

        // All other handlers will never destroy the current document and controller.

        metaObject()->invokeMethod(mainWindow,
                                   "openFiles",
                                   Qt::QueuedConnection,
                                   Q_ARG(QStringList,QStringList(filePath)));
    }
}

void CS_File::actionFile_Close_Triggered()
{
    mainWindow->close();
}

bool CS_File::maybeSave()
{
    if(!controller->isClean())
    {
        QMessageBox::StandardButton ret =
            QMessageBox::warning(mainWindow,
                                 tr("Save changes"),
                                 tr("The document has been modified.\nDo you want to save your changes?"),
                                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (ret == QMessageBox::Save)
            return actionFile_Save_Triggered();
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

bool CS_File::actionFile_Save_Triggered()
{
    if(mainWindow->isUntitled())
    {
        return actionFile_SaveAs_Triggered();
    }
    else
    {
        if(!showCompatibilityWarning())return false;

        ConversionOptions conversionOptions(true);
        return mainWindow->saveFile(mainWindow->getCurrentFilePath(), conversionOptions);
    }
}

bool CS_File::actionFile_SaveAs_Triggered()
{
    if(!showCompatibilityWarning())return false;

    QString filePath = QFileDialog::getSaveFileName(mainWindow,
                                                    tr("Save MIDI File As"),
                                                    mainWindow->getCurrentFilePath(),
                                                    tr("MIDI files (*.mid);;All files (*.*)"));
    if (filePath.isEmpty())
        return false;

    ConversionOptions conversionOptions(true);
    return mainWindow->saveFile(filePath, conversionOptions);
}

bool CS_File::showCompatibilityWarning()
{
    if(!settings->checkFileCompatibility)return true;

    // check for playback options that must be converted for compatibility with other MIDI software
    QString nonStandardPlaybackOptionsDescription=
            docRoot->collectNonStandardPlaybackOptionsDescription(
                    mainWindow->getSpinBoxRelativePlaybackSpeed()->value());

    if(!nonStandardPlaybackOptionsDescription.isEmpty())
    {
        QMessageBox msgBox(QMessageBox::Warning,
                           tr("Compatibility Warning"),
                           tr("This document uses the following playback options:")
                           + "\n\n"
                           + nonStandardPlaybackOptionsDescription
                           + "\n"
                           + tr("These options can only be reproduced by Speedy MIDI.\n\n"
                                "For correct playback in other MIDI playback programs, "
                                "additionally create a compatible file (Menu: File / Save Compatible File)"),
                           QMessageBox::Save | QMessageBox::Cancel, mainWindow);

        QAbstractButton* buttonDontShowAgain=msgBox.addButton(tr("Save && Never Warn Again"),
                                                              QMessageBox::AcceptRole);
        msgBox.button(QMessageBox::Save)->setText(tr("Save Anyway"));

        switch(msgBox.exec())
        {
        case QMessageBox::Save:
            return true;
        case QMessageBox::Cancel:
            return false;
        default:
            if(msgBox.clickedButton() == buttonDontShowAgain)
            {
                settings->checkFileCompatibility=false;
                return true;
            }
            return false;
        }
    }
    return true;
}

bool CS_File::actionFile_SaveCompatibleFile_Triggered()
{
    // check for playback options that must be converted for compatibility with other MIDI software
    QString nonStandardPlaybackOptionsDescription=
            docRoot->collectNonStandardPlaybackOptionsDescription(
                    mainWindow->getSpinBoxRelativePlaybackSpeed()->value());

    if(nonStandardPlaybackOptionsDescription.isEmpty())
    {
        // No non-standard playback option found
        QMessageBox::information(mainWindow,
                                 tr("Save Compatible MIDI File"),
                                 tr("This document does not contain any Speedy-MIDI specific additions.\n\n"
                                    "As the saved file is already compatible with other MIDI software,\n"
                                    "no additional \"compatible file\" is required."));
        return true;
    }

    // -----------------------------------------------------------------------------------------
    // ask which playback options should be converted to compatible MIDI commands

    ConversionOptionsDialog dlg(mainWindow, mainWindow, docRoot);
    dlg.convertRelativePlaybackSpeed = settings->LRU.conversionOptionsConvertRelativePlaybackSpeed;
    dlg.convertSwing                 = settings->LRU.conversionOptionsConvertSwing;

    if(dlg.exec() != QDialog::Accepted)return false;

    settings->LRU.conversionOptionsConvertRelativePlaybackSpeed = dlg.convertRelativePlaybackSpeed;
    settings->LRU.conversionOptionsConvertSwing                 = dlg.convertSwing;

    // -----------------------------------------------------------------------------------------
    // ask for different file name for compatible file, default: append " compatible.mid"

    QFileInfo fileInfo(mainWindow->getCurrentFilePath());

    QString defaultPath = fileInfo.path() + '/' + fileInfo.completeBaseName();
    defaultPath += tr(" compatible.mid");

    // Ask user for filename
    QString filePath = QFileDialog::getSaveFileName(mainWindow,
                                                    tr("Save Compatible MIDI File"),
                                                    defaultPath,
                                                    tr("MIDI files (*.mid);;All files (*.*)"));
    if (filePath.isEmpty())
        return false;

    // Setup conversion options
    //  (conversion-flags may be true even if corresponding playback option is never used in document)
    ConversionOptions conversionOptions(dlg.convertRelativePlaybackSpeed,
                                        mainWindow->getSpinBoxRelativePlaybackSpeed()->value(),
                                        dlg.convertSwing);

    return mainWindow->saveFile(filePath, conversionOptions);
}

void CS_File::actionFile_ExtractParts_Triggered()
{
    // document must have been saved at least once, so we know the directory for the part files
    if(mainWindow->isUntitled())
    {
        QMessageBox::warning(mainWindow,
                             tr("Extract Parts"),
                             tr("Please save the document before extracting parts."),
                             QMessageBox::Ok);
        return;
    }

    // -----------------------------------------------------------------------------------------

    PartExtractionDialog dlg(this);

    // IN/OUT LRU parameters
    dlg.fileNameFormat            = settings->LRU.extractPartsFileNameFormat;
    dlg.generateInverseParts      = settings->LRU.extractPartsGenerateInverseParts;
    dlg.separateTrackNamesByComma = settings->LRU.extractPartsSeparateTrackNamesByComma;
    dlg.saveCompatibleFiles       = settings->LRU.extractPartsSaveCompatibleFiles;

    if(dlg.exec() != QDialog::Accepted)return;

    settings->LRU.extractPartsFileNameFormat            = dlg.fileNameFormat;
    settings->LRU.extractPartsSeparateTrackNamesByComma = dlg.separateTrackNamesByComma;
    settings->LRU.extractPartsGenerateInverseParts      = dlg.generateInverseParts;
    settings->LRU.extractPartsSaveCompatibleFiles       = dlg.saveCompatibleFiles;

    // -----------------------------------------------------------------------------------------

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Setup conversion options
    ConversionOptions conversionOptions(false);
    if(dlg.saveCompatibleFiles)
    {
        conversionOptions=ConversionOptions(settings->LRU.conversionOptionsConvertRelativePlaybackSpeed,
                                            mainWindow->getSpinBoxRelativePlaybackSpeed()->value(),
                                            settings->LRU.conversionOptionsConvertSwing);
    }

    // extract parts as given in PartExtractionDialog
    for(int i=0; i < dlg.partList.size(); ++i)
    {
        QSaveFile file(dlg.partList[i].filePath);
        if (!file.open(QFile::WriteOnly))
        {
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(mainWindow,
                                 tr("Extract Parts Error"),
                                 tr("Cannot write file\n\n%1\n\n%2")
                                 .arg(file.fileName()).arg(file.errorString()));
            return;
        }

        if(!docRoot->save(&file, view->getEditorState(), conversionOptions, dlg.partList[i].trackIndexList) ||
           !file.commit())
        {
            file.cancelWriting();
            QApplication::restoreOverrideCursor();

            QMessageBox::warning(mainWindow,
                                 tr("Extract Parts Error"),
                                 tr("Failed to save file\n\n%1\n\n%2")
                                 .arg(file.fileName()).arg(file.errorString()));
            return;
        }

    }

    QApplication::restoreOverrideCursor();

    QMessageBox::information(mainWindow,
                             tr("Extract Parts"),
                             tr("Part extraction successful."),
                             QMessageBox::Ok);
}

void CS_File::actionFile_Quit_Triggered()
{
    app->quit();
}

void CS_File::actionFile_OpenRecentFile_Triggered()
{
    QAction *action=controller->getCurrentSendingAction();
    if(action)
    {
        QString filePath=action->data().toString();

        // BEWARE: We must use a Qt::QueuedConnection to open the file!
        //  (see actionFile_Open_Triggered for details)

        metaObject()->invokeMethod(mainWindow,
                                   "openFiles",
                                   Qt::QueuedConnection,
                                   Q_ARG(QStringList,QStringList(filePath)));
    }
}

void CS_File::actionOptions_Preferences_Triggered()
{
    app->execPreferencesDialog(false);
}

void CS_File::actionHelp_About_Triggered()
{
    QMessageBox* msgBox=new QMessageBox(mainWindow);
    msgBox->setWindowTitle(tr("About Speed MIDI Editor"));
    msgBox->setTextFormat(Qt::RichText);
    msgBox->setText(tr("<b>Speed MIDI Editor 0.1.1</b><br>"
                       "A multi-track Standard MIDI File editor based on Speedy MIDI 1.1.<br><br>"
                       "Original program: Speedy MIDI 1.1 by Holger&nbsp;Hoffmann.<br>"
                       "Copyright &copy; 2010-2013 Holger&nbsp;Hoffmann.<br>"
                       "All rights reserved.<br>"
                       "Community development: Gyuricza&nbsp;R&oacute;bert.<br><br>"
                       "Licensed under the GNU General Public License, version 3 or later.<br>"
                       "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">View the license.</a><br><br>"
                       "This program is distributed in the hope that it will be useful, "
                       "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."));
    msgBox->setTextInteractionFlags(Qt::TextBrowserInteraction);

    msgBox->addButton(QMessageBox::Ok);
    QPixmap aboutIcon(72, 148);
    aboutIcon.fill(Qt::transparent);
    QPainter painter(&aboutIcon);
    painter.drawPixmap(12, 0, QPixmap(":/images/app_icon_48.png"));
    QSvgRenderer logoRenderer(QStringLiteral(":/images/GYR-black.svg"));
    if(logoRenderer.isValid())
        logoRenderer.render(&painter, QRectF(10, 60, 52, 76));
    msgBox->setIconPixmap(aboutIcon);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->exec();
}
