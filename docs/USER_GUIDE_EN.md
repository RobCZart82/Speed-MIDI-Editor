# Speed MIDI Editor — User Guide

This preliminary guide covers the piano-roll editing tools currently available in the development build.

## Piano-roll tools

The **Move Notes** (hand), **Draw Notes** (pencil), and **Erase Notes** (eraser) buttons are persistent tools. A selected tool stays active after each operation until you click its toolbar button again or switch to one of the other two tools. Only one of these tools can be active at a time.

You can also leave the active tool quickly: **right-click in the piano-roll grid** to turn off the hand, pencil, or eraser and return to the normal editing mode. Right-clicking does not change any MIDI notes. If no tool is active, right-click has no effect. A right-click during an active mouse drag does not interrupt that drag.

### Move Notes

Select the hand button, then drag a note from its body. Drag left or right to change its position in time; drag up or down to change its pitch. The note snaps to the editor grid and keeps its length. Drag either end of the note to resize it instead. Each completed drag can be undone with **Undo**.

### Draw Notes

Select the pencil button and drag on an empty part of the piano roll to create a note. Its pitch follows the row where you draw, and its timing follows the editor grid. Click the pencil button again, switch tools, or right-click the grid to leave pencil mode.

### Erase Notes

Select the eraser button and click or drag over notes to remove them. Use **Undo** to restore erased notes. Click the eraser button again, switch tools, or right-click the grid to leave eraser mode.
