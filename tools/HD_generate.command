#!/bin/bash
# HD_generate.command - macOS: double-click opens Terminal and runs HD_generate.sh from the same folder.
cd "$(dirname "$0")" || exit 1
./HD_generate.sh "$@"
rc=$?
echo
read -r -p "Press Enter to close this window. " _
exit $rc
