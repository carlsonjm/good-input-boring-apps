# Agent instructions

Applies to the whole repository. `README.md` says how to build and check it;
`docs/PDF-CONTRACT.md` says what each part promises.

## Working rules

- Run `./verify.sh` before handing a head over. A cloud session without Qt and
  Poppler says so rather than claiming verification.
- Nothing visible is built before its mock-up is approved. Mock-ups cite the
  suite's Visual sheet (`kadunce/docs/ITASCA-VISUAL-LANGUAGE.md`) for every size.
- The core (`src/core/`) knows nothing of the screen. Keep page geometry
  normalised, 0 to 1 from the top-left corner.
- Nothing leaves the computer. The kept signature is a file only its owner can
  read; the reading places keep a fingerprint of each path, never the name.
- Saving never risks the original: write beside it, check it opens, then swap.
- Plain Qt and Poppler only, nothing from Plasma: the app ships on Flathub first.

## Git history

Prefer one to three meaningful commits per pull request. Clean the branch
before its head is handed over for a check, never after. Never rewrite history
already merged to `main`.
