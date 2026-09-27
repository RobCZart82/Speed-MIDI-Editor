/***************************************************************************
 *  swingifydialog.h - Dialog: Add Swing
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

#ifndef SWINGIFYDIALOG_H
#define SWINGIFYDIALOG_H

#include "global.h"
#include <QtGui/QDialog>

namespace Ui {
    class SwingifyDialog;
}

class SwingifyDialog : public QDialog {
    Q_OBJECT
public:
    SwingifyDialog(QWidget *parent = 0);
    ~SwingifyDialog();
    int exec();

protected:
    void changeEvent(QEvent *e);

    virtual void accept();
    virtual void reject();
    void enableAndDisable();

    QMenu* toolButtonSwingHardnessMenu;

    // Dialog data
    bool updateData(bool saveAndValidate = true);
public:
    int swingHardness;

protected slots:
    void menuSwingHardnessTriggered();

private:
    Ui::SwingifyDialog *ui;
};

#endif // SWINGIFYDIALOG_H
