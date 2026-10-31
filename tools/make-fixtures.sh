#!/usr/bin/env bash
# Rebuilds the Microsoft-format test files from the readable flat sources.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../tests/fixtures"
profile="$(mktemp -d)"
trap 'rm -rf "$profile"' EXIT
soffice -env:UserInstallation="file://$profile" --headless --convert-to docx letter.fodt >/dev/null
soffice -env:UserInstallation="file://$profile" --headless --convert-to xlsx budget.fods >/dev/null
soffice -env:UserInstallation="file://$profile" --headless --convert-to pptx deck.fodp >/dev/null
