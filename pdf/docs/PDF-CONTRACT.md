# Good Input PDF: what it promises

What the app does for a person, part by part. The look is described by its
approved mock-up; this file is the behaviour underneath it.

## Opening

- A file opens, or the app says which of these it is: not there, not a PDF,
  locked (asks for its password), or the password was wrong.
- It opens on the page it was left on. Only the page number and a fingerprint
  of the file's path are kept, for the last 200 files.
- The title shown is the PDF's own title, else the file name without ".pdf".

## Pages

- A page is always white paper, drawn at the width the screen needs, never
  wider than 4096 pixels.
- Marks added in the app are in the page picture the moment they are made.

## Finding and selecting text

- Search ignores case and accents and covers every page, in reading order. It
  can say how many matches each page has.
- A double tap selects the word under the finger, with a little slack so a tap
  between lines still finds one.
- Dragging a selection's ends selects everything between them in reading order,
  whichever way round the ends are. A selection on one line is one band.
- Copy puts the selected text on the clipboard.

## Forms

- Fields are walked in the order a person fills them: page by page, top to
  bottom, left to right, going round from the last to the first. Read-only and
  signature fields are skipped.
- "Still empty" counts text and choice fields with nothing in them. Tick boxes
  never count, since unticked is an answer. Poppler does not say which fields
  a form marks required, so the count is of empty fields, not required ones.
- A one-line field turns a typed line break into a space and keeps to the
  field's length limit. Choosing a radio button turns its group's others off.

## Highlights

- Four colours: yellow, then the Visual sheet's bright teal, blue and lavender.
- Each highlight is a standard PDF highlight, so any reader shows it. The app's
  own carry a `goodinput-` name, and only those are offered for removal.

## Signing

- A drawn signature is kept on this computer only, in a file only its owner can
  read. Points closer than a finger can mean, and strokes too short to be ink,
  are dropped. "Forget" removes it.
- Placed on a page, it fills the chosen area as far as it can without being
  stretched, centred, in near-black ink, as a standard PDF ink mark.
- A certificate-backed digital signature is a later step; Poppler can make one
  when a person has a certificate installed.

## Saving

- Save writes the changes beside the file, checks the new file opens with the
  same number of pages, keeps the old file's permissions and only then swaps it
  in. A failed save leaves the person's file exactly as it was.
- After a save the app carries on from the saved file, so more changes build on it.
- Save a copy writes elsewhere and leaves the open file untouched; it refuses to
  write over the open file itself.
