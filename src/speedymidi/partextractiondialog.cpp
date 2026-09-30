/***************************************************************************
 *  partextractiondialog.cpp - Dialog: Extract Parts
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

#include "partextractiondialog.h"
#include "ui_partextractiondialog.h"
#include "mainwindow.h"
#include "cs_file.h"
#include "doc_root.h"
#include "doc_track.h"
#include "conversionoptionsdialog.h"
#include "settings.h"

#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QKeyEvent>
#include <QFileInfo>

namespace
{
QString makeSafePartFileName(QString fileName)
{
    for(int i=0; i < fileName.size(); ++i)
    {
        const QChar character=fileName[i];
        const ushort value=character.unicode();
        if(value < 0x20 || value == 0x7f || character == '/' || character == '\\' ||
           character == ':' || character == '*' || character == '?' || character == '"' ||
           character == '<' || character == '>' || character == '|')
            fileName[i]='_';
    }

    while(fileName.endsWith('.') || fileName.endsWith(' '))
        fileName.chop(1);

    if(fileName.isEmpty() || fileName == "." || fileName == "..")
        fileName="part";

    const QString deviceName=fileName.section('.',0,0).toUpper();
    const bool numberedDevice=(deviceName.size() == 4 &&
                               (deviceName.startsWith("COM") || deviceName.startsWith("LPT")) &&
                               deviceName[3] >= '1' && deviceName[3] <= '9');
    if(numberedDevice || deviceName == "CON" || deviceName == "PRN" ||
       deviceName == "AUX" || deviceName == "NUL")
        fileName.prepend('_');

    return fileName;
}

QString normalizedPathKey(const QString& path)
{
    return path.normalized(QString::NormalizationForm_C).toCaseFolded();
}
}

class PartDefinitionItemDelegate : public QStyledItemDelegate
{
public:
    PartDefinitionItemDelegate(QObject* parent) : QStyledItemDelegate(parent)
    {
        circleRadius=4;
    }

    virtual QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
    {
        return QSize(2*circleRadius,2*circleRadius);
    }

    virtual bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem& /*option*/, const QModelIndex &index)
    {
        // partly copied from QStyledItemDelegate::editorEvent

        // make sure that we have a check state
        QVariant value = index.data(Qt::CheckStateRole);
        if (!value.isValid())
            return false;

        if(event->type() == QEvent::KeyPress)
        {
            int key = static_cast<QKeyEvent*>(event)->key();
            if(key == Qt::Key_Space || key == Qt::Key_Select)
            {
                // toggle check state
                bool checked = model->data(index, Qt::CheckStateRole) == Qt::Checked;
                model->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);

                return true;    // eat keyboard event
            }
        }

        if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if(mouseEvent->button() == Qt::LeftButton)
            {
                // toggle check state
                bool checked = model->data(index, Qt::CheckStateRole) == Qt::Checked;
                model->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);

                // do not eat event
            }
        }

        return false;
    }

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        // Focus rect
        if(option.state & QStyle::State_HasFocus)
        {
            QStyleOptionFocusRect o;
            o.QStyleOption::operator=(option);
            o.state |= QStyle::State_KeyboardFocusChange;
            o.state |= QStyle::State_Item;

            o.backgroundColor = option.palette.color(QPalette::Window);
            QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
        }

        // Check mark: centered circle
        bool checked = index.model()->data(index, Qt::CheckStateRole).toBool();
        if(checked)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(option.palette.text());

            painter->drawEllipse(option.rect.center(), circleRadius, circleRadius);

            painter->restore();
        }
    }

protected:
    int circleRadius;
};

PartExtractionDialog::PartExtractionDialog(CS_File* csFile) :
    QDialog(csFile->getMainWindow()),
    ui(new Ui::PartExtractionDialog)
{
    ui->setupUi(this);

    this->csFile=csFile;
    docRoot=csFile->getDocRoot();

    generateInverseParts=false;
    separateTrackNamesByComma=false;

    saveCompatibleFiles=false;

    connect(ui->pushButtonReset, SIGNAL(clicked()), SLOT(pushButtonResetClicked()));
    connect(ui->pushButtonClear, SIGNAL(clicked()), SLOT(pushButtonClearClicked()));
    connect(ui->pushButtonAddPart, SIGNAL(clicked()), SLOT(pushButtonAddPartClicked()));
    connect(ui->pushButtonConversionOptions, SIGNAL(clicked()), SLOT(pushButtonConversionOptionsClicked()));
    connect(ui->checkBoxSaveCompatibleFiles, SIGNAL(toggled(bool)), SLOT(checkBoxSaveCompatibleFilesToggled(bool)));
}

PartExtractionDialog::~PartExtractionDialog()
{
    delete ui;
}

void PartExtractionDialog::changeEvent(QEvent *e)
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

int PartExtractionDialog::exec()
{
    // ------------------------------------------------------------------------------------------
    // initialize dialog data

    updateData(false);
    enableAndDisable();

    ui->tableWidgetPartDef->setItemDelegate(new PartDefinitionItemDelegate(this));
    ui->tableWidgetPartDef->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidgetPartDef->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    resetPartDefinition();

    // ------------------------------------------------------------------------------------------

    return QDialog::exec();
}

void PartExtractionDialog::enableAndDisable()
{
    ui->pushButtonConversionOptions->setEnabled(saveCompatibleFiles);
}

void PartExtractionDialog::accept()
{
    if(!updateData())return;

    partList.clear();   // restart with empty list (previous attempts may have failed)

    QFileInfo fileInfo(csFile->getMainWindow()->getCurrentFilePath());
    QString basePath=fileInfo.path();
    QString songName=fileInfo.completeBaseName();   // contents of placeholder %s

    // Generate ExtractedPart-objects
    for(int column=0; column < ui->tableWidgetPartDef->columnCount(); ++column)
    {
        ExtractedPart part;
        ExtractedPart inversePart;
        QStringList trackNamePlaceholderTrackNameList;

        // generate list of track indices and text for %t placeholder
        for(int row=0; row < ui->tableWidgetPartDef->rowCount(); ++row)
        {
            if(ui->tableWidgetPartDef->item(row, column)->checkState() == Qt::Checked)
            {
                part.trackIndexList.append(row);

                QString trackName=docRoot->trackList[row]->name;

                // do not add duplicate track names
                if(!trackNamePlaceholderTrackNameList.contains(trackName, Qt::CaseSensitive))
                    trackNamePlaceholderTrackNameList.append(trackName);
            }
            else    // track not checked => add to inverse part list
            {
                inversePart.trackIndexList.append(row);
            }
        }

        // generate text for track name placeholder
        QString trackNamePlaceholderText;
        for(int i=0; i < trackNamePlaceholderTrackNameList.size(); ++i)
        {
            if(i > 0)
            {
                if(separateTrackNamesByComma)trackNamePlaceholderText+=',';
                trackNamePlaceholderText+=' ';
            }
            trackNamePlaceholderText+=trackNamePlaceholderTrackNameList[i];
        }

        // Do not allow an empty placeholder because that might delete the original file!
        //  (an empty placeholder may occur if a track name is empty)
        if(trackNamePlaceholderText.isEmpty())trackNamePlaceholderText=tr("no name");

        // Generate file name, replace placeholder %s if it exists
        QString partFileName=fileNameFormat;

        int placeholderIndex=partFileName.indexOf("%s", 0, Qt::CaseSensitive);
        if(placeholderIndex >= 0)
        {
            partFileName.remove(placeholderIndex, 2);           // remove "%s"
            partFileName.insert(placeholderIndex, songName);    // insert song name
        }

        // use this name as base for the inverted part, too
        QString inversePartFileName=partFileName;

        // generate standard (not inverted) part?
        if(part.trackIndexList.size() > 0)
        {
            // replace placeholder %t
            placeholderIndex=partFileName.indexOf("%t", 0, Qt::CaseSensitive);
            Q_ASSERT(placeholderIndex >= 0);    // must exist, checked by updateData

            partFileName.remove(placeholderIndex,2);       // remove "%t"
            partFileName.insert(placeholderIndex,
                                trackNamePlaceholderText); // insert track name placeholder text

            // generate full file path
            part.filePath=getUnusedPartFilePath(basePath,partFileName);

            // add to list
            partList.append(part);
        }

        // generate inverse part only if
        // - checkbox was checked
        // - the corresponding part has at least one track
        // - the inverse part has at least one track

        if(generateInverseParts &&
           part.trackIndexList.size() > 0 &&
           inversePart.trackIndexList.size() > 0)
        {
            // replace placeholder %t by "without " + trackNames in (not inverted) part
            placeholderIndex=inversePartFileName.indexOf("%t", 0, Qt::CaseSensitive);
            Q_ASSERT(placeholderIndex >= 0);    // must exist, checked by updateData

            inversePartFileName.remove(placeholderIndex,2);       // remove "%t"
            inversePartFileName.insert(placeholderIndex,
                                       tr("without ") +
                                       trackNamePlaceholderText); // insert track name placeholder text

            // generate full file path
            inversePart.filePath=getUnusedPartFilePath(basePath,inversePartFileName);

            // add to list
            partList.append(inversePart);
        }
    }

    if(partList.isEmpty())
    {
        QMessageBox::warning(csFile->getMainWindow(),
                             tr("Extract Parts"),
                             tr("No parts to be generated."));
        return;
    }

    // check if any of the files to be generated exists
    for(int i=0; i < partList.size(); ++i)
    {
        QFileInfo partFileInfo(partList[i].filePath);
        if(partFileInfo.exists())
        {
            if(QMessageBox::warning(csFile->getMainWindow(),
                                    tr("Confirm overwrite"),
                                    tr("Overwrite existing part files?"),
                                    QMessageBox::Ok | QMessageBox::Cancel,
                                    QMessageBox::Cancel)
                != QMessageBox::Ok)
                return;  // cancel generation
            else
                break;  // one warning suffices
        }
    }

    QDialog::accept();
}

QString PartExtractionDialog::getUnusedPartFilePath(const QString& basePath, const QString& partFileName)
{
    QString fileExtension=".mid";
    const QString safePartFileName=makeSafePartFileName(partFileName);
    QString partFilePath=basePath + '/' + safePartFileName + fileExtension;

    // if path name already exists, add an index (may happen if tracks have the same name)
    int fileIndex=0;

    bool pathInUse;
    do
    {
        pathInUse=false;
        for(int i=0; i < partList.size(); ++i)
        {
            if(normalizedPathKey(partList[i].filePath) == normalizedPathKey(partFilePath))
            {
                // path already in use, add a different index and retry
                pathInUse=true;

                ++fileIndex;
                partFilePath=basePath + '/' + safePartFileName + tr("(%1)").arg(fileIndex) + fileExtension;
                break;
            }
        }
    }
    while(pathInUse);

    return partFilePath;
}

void PartExtractionDialog::reject()
{
    QDialog::reject();
}

bool PartExtractionDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        generateInverseParts      = ui->checkBoxGenerateInverseParts->isChecked();
        fileNameFormat            = ui->lineEditFileNameFormat->text();
        separateTrackNamesByComma = ui->checkBoxSeparateByCommas->isChecked();
        saveCompatibleFiles       = ui->checkBoxSaveCompatibleFiles->isChecked();

        // Validate file name format: String must contain placeholder %s zero times or once, and %t exactly once.
        if(fileNameFormat.count("%s", Qt::CaseSensitive) > 1)
        {
            QMessageBox::warning(csFile->getMainWindow(),
                                 tr("Error"),
                                 tr("The file name format contains the %s placeholder more than once."),
                                 QMessageBox::Ok);

            return false;   // Validation failed
        }

        if(fileNameFormat.count("%t", Qt::CaseSensitive) != 1)
        {
            QMessageBox::warning(csFile->getMainWindow(),
                                 tr("Error"),
                                 tr("The file name format must contain the %t "
                                    "placeholder exactly once."),
                                 QMessageBox::Ok);

            return false;   // Validation failed
        }
    }
    else
    {
        ui->checkBoxGenerateInverseParts  ->setChecked(generateInverseParts);
        ui->lineEditFileNameFormat        ->setText(fileNameFormat);
        ui->checkBoxSeparateByCommas      ->setChecked(separateTrackNamesByComma);

        ui->checkBoxSaveCompatibleFiles->blockSignals(true);
        ui->checkBoxSaveCompatibleFiles->setChecked(saveCompatibleFiles);
        ui->checkBoxSaveCompatibleFiles->blockSignals(false);
    }
    return true;
}

void PartExtractionDialog::pushButtonResetClicked()
{
    resetPartDefinition();
}

void PartExtractionDialog::resetPartDefinition()
{
    // initialize rows = tracks
    ui->tableWidgetPartDef->setRowCount(docRoot->trackList.size());
    for(int row=0; row < docRoot->trackList.size(); ++row)
    {
        DocTrack* track=docRoot->trackList[row];

        QTableWidgetItem* trackHeaderItem = new QTableWidgetItem(track->name);
        ui->tableWidgetPartDef->setVerticalHeaderItem(row, trackHeaderItem);
    }

    // initialize columns
    ui->tableWidgetPartDef->setColumnCount(docRoot->trackList.size());
    for(int column=0; column < docRoot->trackList.size(); ++column)
    {
        QTableWidgetItem* partHeaderItem = new QTableWidgetItem(tr("Part\n%1").arg(column+1));
        ui->tableWidgetPartDef->setHorizontalHeaderItem(column, partHeaderItem);
    }

    // initialize check mark items
    for(int row=0; row < docRoot->trackList.size(); ++row)
    {
        for(int column=0; column < docRoot->trackList.size(); ++column)
        {
            QTableWidgetItem* checkMarkItem = new QTableWidgetItem();
            checkMarkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            checkMarkItem->setCheckState(Qt::Unchecked);
            ui->tableWidgetPartDef->setItem(row, column, checkMarkItem);
        }
    }

    // set default checks: Traverse all tracks, each track gets one part.
    //  Exception: subsequent tracks with same name are put into the same part
    QString prevTrackName;
    int currentPart=0;
    for(int row=0; row < docRoot->trackList.size(); ++row)
    {
        DocTrack* track=docRoot->trackList[row];
        if(row > 0 && track->name != prevTrackName)
            ++currentPart;

        ui->tableWidgetPartDef->item(row, currentPart)->setCheckState(Qt::Checked);

        prevTrackName=track->name;
    }

    // delete superfluous parts
    ui->tableWidgetPartDef->setColumnCount(currentPart + 1);

    // resize to contents
    ui->tableWidgetPartDef->verticalHeader()->resizeSections(QHeaderView::ResizeToContents);
    ui->tableWidgetPartDef->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void PartExtractionDialog::pushButtonClearClicked()
{
    // remove all checks
    for(int row=0; row < ui->tableWidgetPartDef->rowCount(); ++row)
    {
        for(int column=0; column < ui->tableWidgetPartDef->columnCount(); ++column)
        {
            ui->tableWidgetPartDef->item(row, column)->setCheckState(Qt::Unchecked);
        }
    }
}

void PartExtractionDialog::pushButtonAddPartClicked()
{
    // append one column
    int newColumn=ui->tableWidgetPartDef->columnCount();
    ui->tableWidgetPartDef->setColumnCount(newColumn + 1);

    QTableWidgetItem* partHeaderItem = new QTableWidgetItem(tr("Part\n%1").arg(newColumn+1));
    ui->tableWidgetPartDef->setHorizontalHeaderItem(newColumn, partHeaderItem);

    // create items for appended column
    for(int row=0; row < ui->tableWidgetPartDef->rowCount(); ++row)
    {
        QTableWidgetItem* checkMarkItem = new QTableWidgetItem();
        checkMarkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkMarkItem->setCheckState(Qt::Unchecked);

        ui->tableWidgetPartDef->setItem(row, newColumn, checkMarkItem);
    }

    ui->tableWidgetPartDef->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void PartExtractionDialog::pushButtonConversionOptionsClicked()
{
    // First check if any non-standard playback option is present
    QString nonStandardPlaybackOptionsDescription=
            docRoot->collectNonStandardPlaybackOptionsDescription(
                    csFile->getMainWindow()->getSpinBoxRelativePlaybackSpeed()->value());

    if(nonStandardPlaybackOptionsDescription.isEmpty())
    {
        // No non-standard playback option found
        QMessageBox::information(this,
                                 tr("Save Compatible MIDI File"),
                                 tr("This document does not contain any Speedy-MIDI specific additions."));
        return;
    }

    ConversionOptionsDialog dlg(this, csFile->getMainWindow(), docRoot);
    dlg.convertRelativePlaybackSpeed = csFile->getSettings()->LRU.conversionOptionsConvertRelativePlaybackSpeed;
    dlg.convertSwing                 = csFile->getSettings()->LRU.conversionOptionsConvertSwing;

    if(dlg.exec() != QDialog::Accepted)return;

    csFile->getSettings()->LRU.conversionOptionsConvertRelativePlaybackSpeed = dlg.convertRelativePlaybackSpeed;
    csFile->getSettings()->LRU.conversionOptionsConvertSwing                 = dlg.convertSwing;
}

void PartExtractionDialog::checkBoxSaveCompatibleFilesToggled(bool checked)
{
    saveCompatibleFiles=checked;
    enableAndDisable();
}
