/***************************************************************************
 *  launchdialog.cpp - Dialog: Launch Dialog
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

#include "launchdialog.h"
#include "ui_launchdialog.h"

#include "speedymidiapp.h"
#include "settings.h"
#include "mainwindow.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QDir>

LaunchDialog::LaunchDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LaunchDialog)
{
    ui->setupUi(this);

    result=LDR_Cancel;

    connect(ui->pushButtonCreateNewDefaultDocument, SIGNAL(clicked()), SLOT(pushButtonCreateNewDefaultDocumentClicked()));
    connect(ui->pushButtonCreateNewWithTrackWizard, SIGNAL(clicked()), SLOT(pushButtonCreateNewWithTrackWizardClicked()));
    connect(ui->pushButtonOpenMidiFile,             SIGNAL(clicked()), SLOT(pushButtonOpenMidiFileClicked()));
    connect(ui->pushButtonOpenRecentFile,           SIGNAL(clicked()), SLOT(pushButtonOpenRecentFileClicked()));

    connect(ui->listWidgetRecentFiles, SIGNAL(itemActivated(QListWidgetItem*)), SLOT(listWidgetRecentFiles_ItemActivated(QListWidgetItem*)));
}

LaunchDialog::~LaunchDialog()
{
    delete ui;
}

void LaunchDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

int LaunchDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);
    enableAndDisable();

    Settings* settings=getApp()->getSettings();

    for (int i = 0; i < settings->recentFileList.size(); ++i)
    {
        QListWidgetItem* item=new QListWidgetItem(
                MainWindow::strippedName(settings->recentFileList[i]),
                ui->listWidgetRecentFiles);

        // set tooltip to full path
        item->setToolTip(settings->recentFileList[i]);

        ui->listWidgetRecentFiles->addItem(item);
    }

    if(!settings->recentFileList.isEmpty())
        ui->listWidgetRecentFiles->setCurrentRow(0);
    else
        ui->pushButtonOpenRecentFile->setEnabled(false);

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void LaunchDialog::enableAndDisable()
{
}

void LaunchDialog::accept()
{
    if(updateData())QDialog::accept();
}

void LaunchDialog::reject()
{
    QDialog::reject();
}

bool LaunchDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
    }
    else
    {
    }
    return true;
}

void LaunchDialog::pushButtonCreateNewWithTrackWizardClicked()
{
    result=LDR_NewDocumentWithWizard;
    accept();
}

void LaunchDialog::pushButtonCreateNewDefaultDocumentClicked()
{
    result=LDR_NewDefaultDocument;
    accept();
}

void LaunchDialog::pushButtonOpenMidiFileClicked()
{
    openFilePath = QFileDialog::getOpenFileName(this,
                                                tr("Open MIDI file"),
                                                QDir::currentPath(),
                                                tr("MIDI files (*.mid);;All files (*.*)"));
    if(!openFilePath.isEmpty())
    {
        result=LDR_OpenDocument;
        accept();
    }
}

void LaunchDialog::pushButtonOpenRecentFileClicked()
{
    QList<QListWidgetItem*> selectedItems=ui->listWidgetRecentFiles->selectedItems();
    if(!selectedItems.isEmpty())
    {
        openFilePath = selectedItems[0]->toolTip();     // item's tooltip contains full path
        result=LDR_OpenDocument;
        accept();
    }
    else    // no item selected
    {
        QMessageBox::warning(this, tr("Open recent file"), tr("Please select a file from the list."));
    }
}

void LaunchDialog::listWidgetRecentFiles_ItemActivated(QListWidgetItem* item)
{
    openFilePath = item->toolTip();     // item's tooltip contains full path

    result=LDR_OpenDocument;
    accept();
}
