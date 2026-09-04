#!/bin/sh
# Overlay build used to be this script because a Make recipe is one Windows
# command line (8191 characters). The builder is tools/build_overlays.py;
# keep this wrapper so existing `sh tools/build_overlays.sh` calls still work.
exec "${PYTHON:-python3}" "$(dirname "$0")/build_overlays.py" "$@"
