# Decisions

## LibreOffice's engine underneath, our own interface on top

Rule: files are read, laid out, drawn and written by LibreOfficeKit; every
control is drawn here.
Why: it opens Office files faithfully, its licence (MPL) asks for no logo, and
it lets the page be the whole interface. A cloud proof on 27 Oct 2026 typed,
selected, styled, recalculated and saved Word, Excel and PowerPoint files.
Rejected: ONLYOFFICE, whose editor and toolbar are one web page and whose
licence requires its logo; and opening LibreOffice itself as a card.

## One program, three apps

Rule: `good-input-office` with `--docs`, `--sheets` or `--slides`.
Why: one engine, one install, one set of screens; each app still has its own
launcher and name on the desktop.

## The engine in-process first, on its own thread

Rule: LibreOfficeKit runs inside the program on one dedicated thread; every
call goes through `Engine::run`. A helper process per document, planned in the
Basics review, comes later behind the same `OfficeDocument` calls.
Why: the in-process engine is enough to build and check every screen; moving it
out later changes only `OfficeDocument`'s inside.
Rejected for now: a helper process from the start, which doubles the plumbing
before any screen exists.

## Saving as you work

Rule: a changed file is saved over itself, in its own format, a moment after
the last change, and when it is closed. "Saved" in the top line says so.
Why: the same promise Notes makes; nobody loses work to a forgotten save.
Rejected: a Save button and a question on close.

## New files are Microsoft format

Rule: New document, New spreadsheet and New presentation create `.docx`,
`.xlsx` and `.pptx` in Documents.
Why: they are what other people can open. Open Document files open and save in
their own format as before.

## Text boundaries off

Rule: the engine's profile hides the marks at the text area's corners.
Why: paper has no such marks.

## Exit without LibreOffice's own clean-up

Rule: after every document is closed and the engine shut down, the program
leaves with `_Exit`.
Why: LibreOffice's clean-up at exit runs after its office is gone and crashes;
by then everything is saved.

## Helpers do one job and wait for Keep

Rule: each Slides helper makes one change to the deck and leaves it waiting
for Keep or Undo, saying who made it. Fit this slide and Make slides are
Codex's, asked through Split Rock's broker on the person's tap with only the
slide or their notes; Tidy the look is the app's own and needs no model.
Without Split Rock the Codex helpers are not offered; without a Codex sign-in
they open Codex's own sign-in through Split Rock, never one of ours.
Why: a slide is compact information on an attractive page. A helper that
takes over the deck would undo that; one that does a boring task and can be
taken back keeps the person in charge.
