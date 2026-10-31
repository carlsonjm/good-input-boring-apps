# Concept

## Who it is for

Someone who opens a Word file from an email, writes a letter, keeps a budget or
shows a few slides, and wants the page in front of them rather than a ribbon of
tools. Docs, Sheets and Slides are the office part of Good Input Basics,
alongside Notes, Mail and PDF.

## What it looks like

Just paper. The page lies on the desk and fills the window; above it is one
quiet line: back to your files, the file's name, whether it is saved, undo and
redo, **Aa** and More. Nothing else stays on screen. A selection brings a small
bar above it; Aa brings every other style in one note. Opened with no file, an
app lists the files of its kind used lately, newest first, with one button to
start a new one.

## How it is built

- `src/engine/Engine` runs LibreOfficeKit, once per process, on its own thread.
  The engine keeps a profile of its own, separate from any LibreOffice the
  person uses.
- `src/engine/OfficeDocument` is one open file: what the engine reports (the
  cursor, the selection, styles in force, the cell, the size) arrives as
  properties, and what a person does goes in as calls. Positions are the
  engine's twips.
- `src/view/PageView` shows a window onto the pages, drawn by the engine in
  tiles that are kept until the engine says an area changed. Keys and
  input-method text typed into it go to the document.
- `qml/` draws everything else: `Paper` (the page, cursor, selection, handles,
  cell ring), `TopLine`, `SelectionBar`, `StyleSheet` (Aa), `HomeView` and
  `DocsWindow`, which is Docs, Sheets or Slides by its `kind`.
- One program, `good-input-office`, is all three apps: `--docs`, `--sheets` or
  `--slides`, each with its own launcher and desktop name
  (`co.goodinput.Docs`, `.Sheets`, `.Slides`).
