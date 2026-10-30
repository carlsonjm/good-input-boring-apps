# Good Input PDF

A PDF is a piece of paper. This app shows it as one: a rounded sheet with no
toolbar or title bar in the way, and one small row of controls that comes when
you reach for it. It does the four things people do with paper:

- **Read**: every page at a glance, and search across the whole file.
- **Fill in forms**: one field after the next, saying how many are still empty.
- **Sign**: draw your signature once; it stays on this computer only.
- **Highlight**: four colours, saved as standard PDF marks any reader shows.

It is one of the Good Input Basics, published on Flathub as `co.goodinput.PDF`,
and runs the same on any desktop.

## State

Built to the approved mock-up (https://claude.ai/artifact/UL1HRXYzCaXa4ytqebeGZi,
approved 30 Oct 2026, with the bar lifted to 40 px from the bottom). Not yet
checked on a device. See [docs/PDF-CONTRACT.md](docs/PDF-CONTRACT.md) for what
each part promises.

| Part | Where | State |
|---|---|---|
| Opening, pages, text, search, selection | `src/core/PdfFile` | Built, tested |
| Forms | `src/core/FormFields` | Built, tested |
| Highlights and a placed signature | `src/core/Marks` | Built, tested |
| Kept signature | `src/core/Signature` | Built, tested |
| Saving safely | `src/core/Saver` | Built, tested |
| The page each file was left on | `src/core/ReadingPlace` | Built, tested |
| What QML talks to, page pictures | `src/app/Session`, `src/app/PageImages` | Built, tested |
| The look (QML) and the app | `qml/`, `src/main.cpp` | Built; every screen drawn off screen by the tests; not yet on a device |
| Flathub manifest | `co.goodinput.PDF.yml` | Written, not yet built |

## Build and check

Needs CMake, Qt 6.4 or newer (Core, Gui, Quick, Svg, Test, and the Quick Dialogs QML module) and Poppler's Qt 6 layer
24.02 or newer.

```sh
./verify.sh
```

It rebuilds the test PDFs from `tools/make-fixtures.py`, builds, runs the tests
off screen (including one that walks the app into each screen of the mock-up
and saves a picture of each in `build/screens/`), and validates the desktop entry and metainfo when their tools are
installed.

## Licence

GPL-2.0-or-later, as Poppler, which draws the pages, requires. Whoever receives
a build is entitled to its source.
