#!/usr/bin/env bash
# Starts the app off screen on copies of the test PDFs, walks it into each of
# the approved mock-up's screens and saves a picture of each. Fails if the app
# fails to start or any screen is not drawn. Pictures land in $1.
set -euo pipefail
app="$1"; fixtures="$2"; out="$3"
mkdir -p "$out"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export QT_QPA_PLATFORM=offscreen XDG_DATA_HOME="$work/data" XDG_CONFIG_HOME="$work/config"

shot() { # name file actions
    cp "$fixtures/$2" "$work/$2"
    GOODINPUT_PDF_SHOT="$out/$1.png" GOODINPUT_PDF_SHOT_DO="$3" "$app" "$work/$2"
    test -s "$out/$1.png" || { echo "screen $1 was not drawn" >&2; exit 1; }
}
shot reading lease.pdf ""
shot select lease.pdf "selectWord:0:0.3:0.148"
shot form form.pdf "goToField:0"
shot sign lease.pdf "openSign"
shot pages lease.pdf "searchFor:deposit"
shot empty not-a-pdf.pdf ""
echo "All screens drawn in $out"
