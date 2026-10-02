# Speed MIDI Editor — User Guide

This guide describes the current development version of Speed MIDI Editor, a multi-track editor for Standard MIDI Files (SMF). It covers the inherited Speedy MIDI workflow as well as the newer piano-roll tools. Menu wording can vary slightly by build and translation. Features described here should be checked again against the release candidate before publication.

## 1. What the program does

Speed MIDI Editor opens, creates, edits, plays, and saves MIDI files. It is a MIDI editor and rehearsal tool; it does not record or mix audio and does not include its own sample library. Playback is sent to a MIDI output device or software synthesizer selected by the operating system and the application preferences.

The project continues Speedy MIDI 1.1 by Holger Hoffmann. Its original workflow includes entering notes from a MIDI keyboard or the on-screen Mouse Piano, editing multiple tracks, changing measure attributes, and extracting parts. Newer versions add direct note drawing, erasing, moving, resizing, and song-fit controls.

## 2. The main window

- **Menu bar and toolbar:** file, editing, playback, note-entry, and utility commands. Hover over toolbar buttons for tooltips.
- **Cell Length:** the rhythmic grid resolution used for note entry and quantization. A cell is a subdivision of a measure; it is not the MIDI note's duration unless you enter or draw a note with that length.
- **Beat / measure ruler:** shows the current beat and measure positions. Measure attributes such as time signature, key signature, tempo, swing, and rehearsal markers are attached to measures.
- **Track information panel:** each row is a MIDI track. It shows its name, instrument/program, MIDI channel, volume, pan, and Solo (S), Mute (M), and Record (R) controls. A MIDI activity indicator responds to note activity.
- **Piano-roll grid:** horizontal position represents song time; vertical position represents pitch. Colored rectangles are MIDI notes. The left keyboard scale labels octaves and pitches.
- **Scrollbars and zoom:** use the horizontal scrollbar to move through measures, the vertical scrollbar to move through tracks, and the adjacent controls to change horizontal/time zoom or vertical/pitch zoom.

### Navigation and fitting

Drag the horizontal or vertical scrollbar thumb to move through the song or track list. Use the zoom sliders or their plus/minus buttons to change the amount of time or pitch shown. **Fit Entire Song** (the four outward arrows in the lower-right corner) adjusts the view so the song's time range and tracks fit the editor area. **Default Track Height** gives tracks a common height sized to show their track information; it does not fit the whole song horizontally. These commands change the view, not the MIDI data.

## 3. Create, open, and save a song

- **New, with Wizard:** create a document and use the track wizard to add named voice or instrument tracks. The wizard accepts track abbreviations such as `satb` for soprano, alto, tenor, and bass; its on-screen examples show other supported abbreviations and instrument types.
- **New Default Document:** create a blank document using the default setup.
- **Open:** open a MIDI file. The original application also supports RIFF/RMID-wrapped MIDI files.

Files with time signature changes inside a measure or fractional-tick measure lengths cannot be represented by the editor's grid. Opening them is refused with an explanation, preserving the original file and the currently open document. Whole-measure boundaries at odd PPQN resolutions remain supported when the measure length is an integer number of ticks.
- **Save / Save As:** save the current document or save a copy under a new name. Use Save As before experimenting with an important file.
- **Save Compatible File:** create an output MIDI file with the editor's playback speed and/or swing playback options converted to ordinary MIDI events, where selected. Use this when another player does not understand Speed MIDI's playback-only settings.
- **Extract Parts…:** create separate MIDI files from selected track groups. The dialog can also create inverse parts containing all unmarked tracks.
- **Close / Quit:** close the current document or application. Save changes when prompted.

Files with time signature changes inside a measure or fractional-tick measure lengths cannot be represented by the editor's grid. Opening them is refused with an explanation, preserving the original file and the currently open document. Whole-measure boundaries at odd PPQN resolutions remain supported when the measure length is an integer number of ticks.

System Exclusive (SysEx) packets are retained when opening and saving MIDI files, including segmented packets. Packets on ordinary tracks follow selection-based copy, paste, clear, and undo operations; conductor-track packets remain global file data. SysEx packets are not sent during playback or MIDI Thru, so device-specific setup in those packets must be applied separately.

The application edits Standard MIDI Files. It does not promise to preserve every vendor-specific or malformed MIDI extension. Keep a backup of important originals and verify a saved copy in another MIDI player or editor when file fidelity is critical.

## 4. Select tracks and time ranges

Click in the piano-roll grid to position the editing cursor and select a cell. Drag across cells and/or track rows to select a time range. Many Edit and Utilities commands operate on the current selection; check the selected measures and tracks before applying a bulk operation. **Select All** selects the document's editable range.

Use **Insert Selected Range** to insert the currently selected range. **Insert…** lets you choose whether to insert cells, measures, or tracks and how many measures or tracks to add. **Delete** removes the selected range; **Clear Cells** clears note contents while retaining the surrounding grid. These are different operations, so use Undo if the result is not what you expected.

## 5. Edit notes in the piano roll

### Persistent tools

The **Move Notes** (hand), **Draw Notes** (pencil), and **Erase Notes** (eraser) toolbar buttons are persistent and mutually exclusive. A selected tool stays active after an operation until you click it again or switch to another tool. **Right-click inside the piano-roll grid** to turn off the active hand, pencil, or eraser. Right-click does not edit notes; it does not interrupt an active drag. With no such tool active, right-click has no effect.

### Move and resize notes

Select the hand tool. Drag a note from its body left or right to change its time position, or up and down to change its pitch. The note snaps to the grid and keeps its duration. Drag the left or right edge to shorten or lengthen the note. Undo reverses completed changes.

### Draw notes

Select the pencil, then drag on an empty part of a track's piano roll. The row determines pitch and the horizontal grid determines start and end. The note uses the new-note velocity set in Preferences. Click the pencil again, choose another tool, or right-click the grid to leave pencil mode.

### Erase notes

Select the eraser, then click a note or drag across notes to remove them. Undo restores erased notes. Click the eraser button again, select another tool, or right-click the grid to leave eraser mode.

### Copy, cut, paste, and undo

Select the desired cells or notes, then use **Cut**, **Copy**, or **Paste**. **Paste and Scale to Selection** adapts pasted material to the selected range. **Undo** and **Redo** reverse or restore supported editing actions. Save the song after editing; clipboard contents are not a substitute for saving.

## 6. Enter notes with a MIDI keyboard or Mouse Piano

The original Speedy MIDI workflow supports step entry from a MIDI keyboard and from the on-screen **Mouse Piano**. Select the destination track or tracks and a cell, choose a cell length, then play a key or click a Mouse Piano key and use **Write or Extend Note** (Space) to enter the held pitch or pitches at the current position. **Write Multiple Cells** writes a chosen number of cells; **Extend Note** lengthens notes from the preceding cell into the current cell. The Write menu contains shortcuts for writing or extending one to ten cells.

When multiple tracks are enabled for writing and multiple keys are pressed, the counts must match for notes to be distributed one per track. To record a chord into one track, enable only that track for writing. Use the Record (R) track button or the Write menu's track-flag commands to choose which tracks receive note entry.

To use the Mouse Piano, show it from **View → Mouse Piano**. Configure its MIDI device and normal/percussion mode in Preferences/Options if those pages are available in the build. Press **F7** (Listen Chord) to audition held notes; release the key to stop the audition. **Escape** stops audition/playback and releases Mouse Piano notes.

## 7. Cell length, note entry, and rhythm

The Cell Length control in the upper-left sets the grid subdivision for step entry and related commands. Choose a base note value, then use the Write menu to double or halve it, switch between triplet and duplet subdivisions, or enter an arbitrary tuplet. The ruler and grid redraw to reflect the chosen subdivision. Cell length sets the editing raster; a note can span one or more cells.

Use **Quantize to Cell Raster** to align selected note positions to the current grid. Use **Scale Note Length…** to scale selected note durations. **Split Notes** divides notes that cross the current selection boundary; **Connect Notes** joins suitable adjacent notes around that boundary. Check the selection and save a copy before applying these operations to a large passage.

## 8. Track controls and properties

- **S (Solo):** hear the selected track(s) in isolation. The exclusive Solo command toggles the selected track and disables Solo on other tracks.
- **M (Mute):** silence the selected track(s). The exclusive Mute command changes the selection while clearing Mute from other tracks.
- **R (Record):** enable the track(s) as destinations for MIDI note entry. The exclusive Record command disables the flag on other tracks.
- **Track Attributes…:** edit track-level settings such as the track name, MIDI channel, instrument/program, volume, and pan, subject to the controls shown in the dialog.
- **New Track Wizard / Add New Track:** add one or more named or instrument tracks. The wizard includes common voice/instrument abbreviations; use a percussion/drum track type when creating a drum part.
- **Default Track Height:** normalize visible track heights so track information fits consistently.

Solo, mute, and record flags affect playback or note entry; they are not substitutes for deleting or changing MIDI events.

## 9. Measure attributes and rehearsal markers

Open **Measure Attributes…** for the measure at the cursor/selection. The dialog can set the time signature, key signature, tempo, playback swing settings, and an optional rehearsal marker with text and color. Rehearsal markers label sections for navigation and rehearsal. Playback swing and relative playback speed are editor playback settings; use **Save Compatible File** to convert the selected playback options into standard MIDI events for other players. Imported MIDI tempo values retain their exact precision and tick positions, including changes within a measure. The tempo field accepts fractional BPM; changing another measure attribute leaves the imported tempo unchanged.

## 10. Playback and MIDI setup

External MIDI outputs receive a small amount of playback data in advance. After Stop, Pause, or Mute, already queued notes can still sound briefly (normally under approximately 0.1 seconds). Later notes are no longer submitted; this is the output scheduling limit, not a change to the saved MIDI file.

Use **Play** (F6), **Stop** (F5), and **Return to Start** to control playback. Playback normally begins at the configured start point (beginning, leftmost visible measure, or cursor position). The blue playback cursor indicates the current location; if scrolling playback is enabled, the view follows it. The playback-speed control changes audition speed without changing the song's tempo events.

Open **Options → Preferences** to choose the program language, musical symbol/name language, MIDI input and output ports, playback behavior, note-entry velocity, and Mouse Piano settings available in the current build. Choose an output port connected to a synthesizer or sound source to hear MIDI. Selecting a MIDI output does not install or provide a sound library. On macOS, **Apple Built-in General MIDI** is one possible system MIDI destination when present. An external synthesizer or another system MIDI destination can also be selected.

Enable **MIDI Thru** in Options when incoming MIDI should be forwarded to the selected output. If a port is missing, close the dialog and reopen it after connecting or enabling the device. If playback is silent, verify the selected output, the destination synthesizer's audio output, and the track's mute/solo state.

## 11. Utilities

Utilities act on the current selection or selected tracks. Make a copy first when applying a bulk transformation.

- **Transpose:** move selected notes by octaves, diatonic steps, or chromatic semitones. Drag/hold commands provide interactive transposition; multiple-step commands apply a chosen number of steps. The active keyboard operation can be canceled with Escape.
- **Split Notes / Connect Notes:** split notes at the selection boundary or join compatible notes on either side of it.
- **Quantize to Cell Raster:** align note timing to the current cell grid.
- **Scale Note Length…:** proportionally change selected note durations.
- **Add Swing…:** apply swing timing to notes. The dialog warns that this changes event timing; measure playback swing is easier to switch on or off later.
- **Remove Top Voice / Remove Bottom Voice:** remove the highest- or lowest-pitched voice from the selected passage. Review the selection before applying.

The output filename format for extracted parts can include `%s` for the song name and `%t` for the track names in a part. Parts with no tracks are ignored.

## 12. Useful keyboard shortcuts

Shortcuts can vary by operating system and keyboard layout; the menu displays the active shortcut.

| Action | Shortcut |
|---|---|
| New / Open / Save / Save As | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Undo / Redo | Ctrl+Z / Ctrl+Y |
| Cut / Copy / Paste / Select All | Ctrl+X / Ctrl+C / Ctrl+V / Ctrl+A |
| Play / Stop / Return to Start | F6 / F5 / toolbar or Playback menu |
| Listen Chord (hold) | F7 |
| Playback Speed | F8 |
| Write or Extend Note | Space |
| Extend Note | Shift+Space |
| Quantize to Cell Raster | Q |
| Transpose one octave while held | F2 |
| Transpose diatonically while held | F3 |
| Transpose chromatically while held | F4 |
| Toggle selected track Solo / Mute / Record | S / M / R |
| Fit Entire Song | Ctrl+F |

## 13. A simple first workflow

1. Open a MIDI file or create a document with the New Track Wizard.
2. Choose the track and output in Preferences; select a useful cell length.
3. Use the hand, pencil, or eraser to edit notes, or enter notes from a MIDI keyboard/Mouse Piano.
4. Use Solo and Mute to inspect individual parts; press Play to audition and Return to Start to restart.
5. Save As to preserve the original, then reopen the saved file to verify it.
6. Use Save Compatible File if another player needs the playback speed/swing settings converted into MIDI events.

## 14. Current scope and troubleshooting

Speed MIDI Editor edits MIDI note and track data; it is not a score-notation editor, audio workstation, or built-in synthesizer. Drum parts use MIDI percussion conventions and rely on the selected MIDI destination for drum sounds. MIDI playback compatibility depends on the selected output and its sound set.

If a file does not open, confirm it is a supported Standard MIDI File and try another known-good `.mid` file. If playback is silent, check the output port and sound source. If an edit is unexpected, use Undo immediately. Keep backup copies before bulk utilities or file conversion.

This guide describes intended commands from the current source and the inherited Speedy MIDI workflow. Confirm each dialog and shortcut against the actual release build before the first public release; some legacy features need release-candidate regression testing.

## 15. Installing the GitHub macOS download

The planned GitHub download is not signed or notarized. macOS may show a security warning the first time you open it. Download the app only from this project's GitHub Releases page. After attempting to open **Speed MIDI Editor.app**, go to **System Settings → Privacy & Security → Open Anyway** and confirm the exception for this app. macOS remembers this one-time approval for the app. See Apple's [instructions for opening an app from an unknown developer](https://support.apple.com/en-am/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac). This special step applies only to the unsigned, unnotarized GitHub build.
