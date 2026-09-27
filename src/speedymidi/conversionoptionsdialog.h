/***************************************************************************
 *  conversionoptionsdialog.h - Dialog: 
 *                              Options when Saving Compatible MIDI File
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

#ifndef CONVERSIONOPTIONSDIALOG_H
#define CONVERSIONOPTIONSDIALOG_H

#include "global.h"
#include <QDialog>

namespace Ui {
    class ConversionOptionsDialog;
}

class ConversionOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConversionOptionsDialog(QWidget *parent, MainWindow* mainWindow, const DocRoot* docRoot);
    ~ConversionOptionsDialog();

    int exec();

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    MainWindow* mainWindow;
    const DocRoot* docRoot;

    int relativePlaybackSpeedInPercent;
    bool swingPresent;

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    // IN/OUT parameters
    bool convertRelativePlaybackSpeed;
    bool convertSwing;

private:
    Ui::ConversionOptionsDialog *ui;
};

#endif // CONVERSIONOPTIONSDIALOG_H
