/***************************************************************************
 *  preferencesdialog.cpp - Dialog: Preferences
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

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "speedymidiapp.h"
#include "settings.h"
#include "midiinterface.h"

#include <QButtonGroup>
#include <QDir>
#include <QLocale>
#include <QTranslator>

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    scrollingPlayback=false;
    playbackStartPosition=Settings::PSP_fromBeginning;
    checkForChannelCollisions=false;
    checkFileCompatibility=false;

    newNoteMidiVelocity=0;

    mousePianoMidiChannelChromatic=1;
    mousePianoMidiChannelPercussion=10;
    mousePianoMidiVelocity=0;

    buttonGroupPlaybackStartPosition=new QButtonGroup(this);
    buttonGroupPlaybackStartPosition->addButton(ui->radioButtonFromBeginning, 0);
    buttonGroupPlaybackStartPosition->addButton(ui->radioButtonFromLeftmostMeasure, 1);
    buttonGroupPlaybackStartPosition->addButton(ui->radioButtonFromCursorPosition, 2);
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::changeEvent(QEvent *e)
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

int PreferencesDialog::exec(bool startWithPageMidiDevices)
{
    // initialize dialog data

    SpeedyMidiApp* app=(SpeedyMidiApp*)qApp;
    QString translationsDir=app->getAppExecutablePath() + '/' + APP_TRANSLATOR_PATH_PREFIX;
    QDir translationsDirObject(translationsDir);

    // ------------------------------------------------------------------------------------------
    // Search for available message language files and add language/country names to list widgets

    messageLocaleNameList=
            translationsDirObject.entryList(QStringList(QString(APP_TRANSLATOR_MSG_PREFIX) + "*.qm"),
                                            QDir::Files,
                                            QDir::Name | QDir::IgnoreCase);

    for(int i=0; i < messageLocaleNameList.size(); ++i)
    {
        // Show language name in local format
        QTranslator* languageNameTranslator=new QTranslator(this);
        QString localLanguageName;
        if(languageNameTranslator->load(messageLocaleNameList[i], translationsDir))
        {
            localLanguageName=languageNameTranslator->translate("<LanguageName>", "<Name>");
        }
        delete languageNameTranslator;
        languageNameTranslator=NULL;

        // extract locale name from file name
        messageLocaleNameList[i].
                remove(APP_TRANSLATOR_MSG_PREFIX, Qt::CaseInsensitive).
                remove(".qm", Qt::CaseInsensitive);

        if(!localLanguageName.isEmpty())
            messageLanguageCountryList.append(localLanguageName);
        else
        {
            QLocale locale(messageLocaleNameList[i]);
            messageLanguageCountryList.append(locale.languageToString(locale.language()));
        }
    }

    ui->listWidgetProgramInterfaceLanguage->addItems(messageLanguageCountryList);

    // ------------------------------------------------------------------------------------------
    // Search for available musical name/symbols language files and add language/country names to list widgets

    musicalLocaleNameList=
            translationsDirObject.entryList(QStringList(QString(APP_TRANSLATOR_MUSIC_PREFIX) + "*.qm"),
                                            QDir::Files,
                                            QDir::Name | QDir::IgnoreCase);

    for(int i=0; i < musicalLocaleNameList.size(); ++i)
    {
        // Show language name in local format
        QTranslator* languageNameTranslator=new QTranslator(this);
        QString localLanguageName;
        if(languageNameTranslator->load(musicalLocaleNameList[i], translationsDir))
        {
            localLanguageName=languageNameTranslator->translate("<LanguageName>", "<Name>");
        }
        delete languageNameTranslator;
        languageNameTranslator=NULL;

        // extract locale name from file name
        musicalLocaleNameList[i].
                remove(APP_TRANSLATOR_MUSIC_PREFIX, Qt::CaseInsensitive).
                remove(".qm", Qt::CaseInsensitive);

        if(!localLanguageName.isEmpty())
            musicalLanguageCountryList.append(localLanguageName);
        else
        {
            QLocale locale(musicalLocaleNameList[i]);
            musicalLanguageCountryList.append(locale.languageToString(locale.language()));
        }
    }

    ui->listWidgetMusicalLanguage->addItems(musicalLanguageCountryList);

    // ------------------------------------------------------------------------------------------
    // List of MIDI devices

    inputDeviceList = app->getMidiInterface()->getInputDeviceList();
    outputDeviceList = app->getMidiInterface()->getOutputDeviceList();

    ui->listWidgetInputDeviceName->addItem(tr("none"));
    ui->listWidgetInputDeviceName->addItems(inputDeviceList);

    ui->listWidgetOutputDeviceName->addItem(tr("none"));
    ui->listWidgetOutputDeviceName->addItems(outputDeviceList);

    // ------------------------------------------------------------------------------------------
    // Page selector

    ui->listWidgetPageSelector->addItem(tr("Language"));
    ui->listWidgetPageSelector->addItem(tr("MIDI ports"));
    ui->listWidgetPageSelector->addItem(tr("Playback"));
    ui->listWidgetPageSelector->addItem(tr("Note entry"));
    ui->listWidgetPageSelector->addItem(tr("Mouse piano"));

    int initialPageIndex=0;
    if(startWithPageMidiDevices)initialPageIndex=1;

    ui->stackedWidget->setCurrentIndex(initialPageIndex);

    connect(ui->listWidgetPageSelector,SIGNAL(itemSelectionChanged()),SLOT(listWidgetPageSelectorSelectionChanged()));
    ui->listWidgetPageSelector->setCurrentRow(initialPageIndex);
    ui->listWidgetPageSelector->setFocus();

    // ------------------------------------------------------------------------------------------

    updateData(false);
    enableAndDisable();

    return QDialog::exec();
}

void PreferencesDialog::enableAndDisable()
{
    // nothing to do
}

void PreferencesDialog::accept()
{
    if(updateData())
    {
        QDialog::accept();
    }
}

void PreferencesDialog::reject()
{
    QDialog::reject();
}

bool PreferencesDialog::updateData(bool saveAndValidate)
{
    if(saveAndValidate)
    {
        int selectedRow;

        // Page 1: Language
        selectedRow=listWidgetSelectedRow(ui->listWidgetProgramInterfaceLanguage);
        if(selectedRow >= 0)
            messageTranslationLocaleName = messageLocaleNameList[selectedRow];

        selectedRow=listWidgetSelectedRow(ui->listWidgetMusicalLanguage);
        if(selectedRow >= 0)
            musicalTranslationLocaleName = musicalLocaleNameList[selectedRow];

        // Page 2: MIDI devices
        selectedRow=listWidgetSelectedRow(ui->listWidgetInputDeviceName);
        if(selectedRow >= 1)
            selectedInputDevice = inputDeviceList[selectedRow - 1];
        else if(selectedRow == 0)
            selectedInputDevice.clear();    // selected entry "none"

        selectedRow=listWidgetSelectedRow(ui->listWidgetOutputDeviceName);
        if(selectedRow >= 1)
            selectedOutputDevice = outputDeviceList[selectedRow - 1];
        else if(selectedRow == 0)
            selectedOutputDevice.clear();   // selected entry "none"

        // Page 3: Playback options
        scrollingPlayback=ui->checkBoxScrollingPlayback->isChecked();
        playbackStartPosition = (Settings::PlaybackStartPositionType)
                                buttonGroupPlaybackStartPosition->checkedId();
        checkForChannelCollisions=ui->checkBoxCheckForChannelCollisions->isChecked();
        checkFileCompatibility=ui->checkBoxCheckFileCompatibility->isChecked();

        // Page 4: Note entry
        newNoteMidiVelocity=ui->spinBoxNewNoteVelocity->value();

        // Page 5: Mouse piano
        mousePianoMidiChannelChromatic=ui->spinBoxMousePianoDefaultPlaybackChannel->value();
        mousePianoMidiChannelPercussion=ui->spinBoxMousePianoPercussionPlaybackChannel->value();
        mousePianoMidiVelocity=ui->spinBoxMousePianoPlaybackVelocity->value();
    }
    else
    {
        int selectedIndex;

        // Page 1: Language
        selectedIndex=messageLocaleNameList.indexOf(messageTranslationLocaleName);
        if(selectedIndex < 0)
        {
            // Locale name not found, retry with language only and no country
            int countrySpecStartIndex=messageTranslationLocaleName.lastIndexOf("_");
            if(countrySpecStartIndex >= 0)
            {
                QString languageOnly=messageTranslationLocaleName;
                languageOnly.truncate(countrySpecStartIndex);

                selectedIndex=messageLocaleNameList.indexOf(languageOnly);
            }

            if(selectedIndex < 0)   // Language still not found, use English
                selectedIndex=messageLocaleNameList.indexOf("English");
        }

        if(selectedIndex >= 0)
        {
            listWidgetSelectText(ui->listWidgetProgramInterfaceLanguage,
                                 messageLanguageCountryList[selectedIndex],
                                 Qt::MatchStartsWith);
        }
        else
        {
            ui->listWidgetProgramInterfaceLanguage->setCurrentRow(0);
        }

        selectedIndex=musicalLocaleNameList.indexOf(musicalTranslationLocaleName);
        if(selectedIndex < 0)
        {
            // Locale name not found, retry with language only and no country
            int countrySpecStartIndex=musicalTranslationLocaleName.lastIndexOf("_");
            if(countrySpecStartIndex >= 0)
            {
                QString languageOnly=musicalTranslationLocaleName;
                languageOnly.truncate(countrySpecStartIndex);

                selectedIndex=musicalLocaleNameList.indexOf(languageOnly);
            }

            if(selectedIndex < 0)   // Language still not found, use English
                selectedIndex=musicalLocaleNameList.indexOf("English");
        }

        if(selectedIndex >= 0)
        {
            listWidgetSelectText(ui->listWidgetMusicalLanguage,
                                 musicalLanguageCountryList[selectedIndex],
                                 Qt::MatchStartsWith);
        }
        else
        {
            ui->listWidgetMusicalLanguage->setCurrentRow(0);
        }

        // Page 2: MIDI devices
        if(selectedInputDevice.isEmpty())
            ui->listWidgetInputDeviceName->setCurrentRow(0);      // select entry "none"
        else
            listWidgetSelectText(ui->listWidgetInputDeviceName, selectedInputDevice);

        if(selectedOutputDevice.isEmpty())
            ui->listWidgetOutputDeviceName->setCurrentRow(0);      // select entry "none"
        else
            listWidgetSelectText(ui->listWidgetOutputDeviceName, selectedOutputDevice);

        // Page 3: Playback options
        ui->checkBoxScrollingPlayback->setChecked(scrollingPlayback);
        buttonGroupPlaybackStartPosition->button((int)playbackStartPosition)->setChecked(true);
        ui->checkBoxCheckForChannelCollisions->setChecked(checkForChannelCollisions);
        ui->checkBoxCheckFileCompatibility->setChecked(checkFileCompatibility);

        // Page 4: Note entry
        ui->spinBoxNewNoteVelocity->setValue(newNoteMidiVelocity);

        // Page 5: Mouse piano
        ui->spinBoxMousePianoDefaultPlaybackChannel->setValue(mousePianoMidiChannelChromatic);
        ui->spinBoxMousePianoPercussionPlaybackChannel->setValue(mousePianoMidiChannelPercussion);
        ui->spinBoxMousePianoPlaybackVelocity->setValue(mousePianoMidiVelocity);
    }
    return true;
}

void PreferencesDialog::listWidgetPageSelectorSelectionChanged()
{
    int pageIndex=listWidgetSelectedRow(ui->listWidgetPageSelector);
    if(pageIndex != -1)
        ui->stackedWidget->setCurrentIndex(pageIndex);
}

int PreferencesDialog::listWidgetSelectedRow(QListWidget* listWidget)
{
    QList<QListWidgetItem*> selectedItems=listWidget->selectedItems();
    if(!selectedItems.isEmpty())
        return listWidget->row(selectedItems[0]);
    else
        return -1;
}

void PreferencesDialog::listWidgetSelectText(QListWidget* listWidget, const QString& text, Qt::MatchFlags flags)
{
    QList<QListWidgetItem*> matchingItemList=listWidget->findItems(text, flags);

    if(!matchingItemList.isEmpty())
        listWidget->setCurrentItem(matchingItemList[0], QItemSelectionModel::Select);
    else
        listWidget->setCurrentRow(0);
}
