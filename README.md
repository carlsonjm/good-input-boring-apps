# Good Input Office

Docs, Sheets and Slides: three paper-simple apps for writing, numbers and
presentations, part of Good Input Basics. Each one is a sheet of paper on your
desk with a single quiet line above it. Word, Excel and PowerPoint files open,
change and save in their own format.

Underneath is LibreOffice's engine, used through LibreOfficeKit without any of
LibreOffice's windows. The engine reads and writes the files, lays out the
pages and draws them; everything a person touches (the cursor, the selection
and its handles, every button) is drawn here, in the Shuffle suite's look.

## What each app does

- **Docs** opens on your recent documents. A tap places the cursor, a double
  tap or a long press takes a word, and the handles stretch the selection. A
  small bar above a selection offers bold, italic, underline, highlight, link,
  comment, copy and cut. **Aa** holds every other style: paragraph styles,
  strike-through, size, lists, alignment, and inserting a picture, a table or a
  page break.
- **Sheets** rings the chosen cell, shows what it holds above the sheet, and
  lists the file's sheets as pills along the bottom.
- **Slides** shows every slide down the side, drawn by the engine, and
  presents full screen.

Every app saves as you work, over the file it opened, and can save a PDF copy.

## PDF

The PDF app lives in `pdf/`, a project of its own with its own build,
check and Flathub manifest; see `pdf/README.md`.

## Build and check

Needs Qt 6.4 or newer (Core, Gui, Qml, Quick, Quick Controls, Svg) and
LibreOffice with its LibreOfficeKit headers.

```bash
./verify.sh
```

Install with CMake as usual; `good-input-office --docs`, `--sheets` or
`--slides` starts each app, and `data/` has their launchers.
`GOODINPUT_OFFICE_LIBREOFFICE` points the program at a LibreOffice installed
somewhere other than `/usr/lib/libreoffice/program`.

## Licence

GPL-2.0-or-later, like the rest of the Shuffle suite; see `LICENSE`. The
program is still called `good-input-office`; this repository holds it.
