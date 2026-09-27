/***************************************************************************
 *  partextractiondialog.h - Dialog: Extract Parts
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

#ifndef PARTEXTRACTIONDIALOG_H
#define PARTEXTRACTIONDIALOG_H

#include "global.h"
#include <QDialog>

class ExtractedPart
{
public:
    QString filePath;
    QList<int> trackIndexList;
};

namespace Ui {
    class PartExtractionDialog;
}

class PartExtractionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PartExtractionDialog(CS_File* csFile);
    ~PartExtractionDialog();

    int exec();

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    void resetPartDefinition();
    QString getUnusedPartFilePath(const QString& basePath, const QString& partFileName);

    CS_File* csFile;
    const DocRoot* docRoot;

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    // IN/OUT parameters
    bool generateInverseParts;
    QString fileNameFormat;
    bool separateTrackNamesByComma;
    bool saveCompatibleFiles;

    // OUT parameters
    QList<ExtractedPart> partList;

protected slots:
    void pushButtonResetClicked();
    void pushButtonClearClicked();
    void pushButtonAddPartClicked();
    void pushButtonConversionOptionsClicked();
    void checkBoxSaveCompatibleFilesToggled(bool checked);

private:
    Ui::PartExtractionDialog *ui;
};

#endif // PARTEXTRACTIONDIALOG_H
