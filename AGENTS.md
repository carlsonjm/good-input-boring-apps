# Repository context and startup

For every task, read only this startup set, in order:

1. `AGENTS.md`
2. `docs/CONCEPT.md`
3. The suite record, kept privately in the Shuffle repository and read from its
   checkout beside this one: `../shuffle/docs/suite/CURRENT_STATE.md` and
   `../shuffle/docs/suite/SWARM.md`. When it is not checked out, say that the
   suite plan was unavailable rather than inventing an order.

Then open only the documents the work touches; `docs/README.md` routes each
subject to its document. Before proposing an approach, check
`docs/DECISIONS.md` for one that was already rejected. What the apps do not do
yet is in `docs/LIMITATIONS.md`.

## Holds

These hold in every Shuffle repository.

- The maintainer approves behavior and visual direction before
  implementation begins. Engineering may present evidence, constraints and
  alternatives; an unapproved proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
- The apps work on any KDE Plasma desktop. What Kadunce, Tettegouche or
  Shuffle add switches on only when they are present, found at run time.

These hold here, because these apps keep what a person wrote.

- A person's file is only ever written in its own format, at its own place,
  or as a copy they asked for. Nothing converts a file behind their back.
- Every call into LibreOfficeKit runs on the engine's one thread
  (`src/engine/Engine.h`). The interface thread never waits on the engine for
  drawing.
- The engine draws the paper; this repository draws everything a person
  touches, to the suite's Visual sheet (Kadunce `docs/ITASCA-VISUAL-LANGUAGE.md`).
- Tests never read or change a real person's files, settings or engine profile.

## Writing here

- A ruling goes in `docs/DECISIONS.md` as its rule, why it holds, and what was
  rejected.
- Public text stays fit to publish: no attribution by initial, and no paths into
  a real home folder.
- Code comments explain code behavior and reasoning only.

## Git history

Prefer one to three meaningful commits per pull request; clean the branch
before its head is handed over for an install, and never rewrite history
already merged to `main`.

## Verification

Run `./verify.sh` for every change. A cloud session without LibreOffice cannot
run it; say so rather than claiming verification.
