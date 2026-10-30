# Testing

## Automated (`./verify.sh`)

`tests/tst_engine.cpp`, with its own home and engine profile, on copies of
`tests/fixtures/`:

- A Word file opens as text, has a size and holds its words.
- A missing file says why it could not open.
- Typing at the end, saving and opening again keeps the words, and the cursor
  is reported.
- A double tap takes one word, dragging the end handle takes the rest of the
  line, and italic shows as in force.
- In a spreadsheet, typing 10 into B2 makes the total in B4 read 14; the cell,
  its formula and the sheet names are reported.
- A presentation has its three slides and moves between them.
- The engine draws a page with text on it.
- A PDF copy is written and the document stays where it was.

`tests/office-snap` (not a test) draws a window into a picture after a step
(`open`, `select`, `style`, `cell`, `home`) for looking at the screens without a
display.

## By hand, after installing

1. Docs, open a Word file: the page fills the width, one quiet line above it.
2. Tap in the text: the cursor shows; the Shuffle Keyboard types where it is.
3. Double tap a word: handles and the bar appear; drag the end handle across
   the line; Bold, then Copy.
4. Aa: Heading on a line; a bulleted list; a table.
5. Back, then the file shows first in the list as just now; reopen it in
   LibreOffice to see the change saved in Word format.
6. Sheets: tap a cell, type a number, check a total; switch sheets with the
   pills.
7. Slides: tap slide 2 in the strip; Present, tap through, Escape.
