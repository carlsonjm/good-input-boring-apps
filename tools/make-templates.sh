#!/bin/sh
# Rebuilds the ready-made sheets in templates/ with the engine itself.
# Run from the top of the repository after building (./verify.sh).
set -e
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sed -n '/BlankSheet\[\] = R"(/,/)";/p' src/app/Recent.cpp | sed '1s/.*R"(//;$d' > "$work/blank.fods"
HOME="$work" QT_QPA_PLATFORM=offscreen build/tests/office-templates "$work/blank.fods" "$PWD/templates"
