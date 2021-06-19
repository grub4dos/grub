#! /usr/bin/env bash

set -e

# Set ${PYTHON} to plain 'python' if not set already
: ${PYTHON:=python}

export LC_COLLATE=C
unset LC_ALL

echo "Generating Automake input..."

CORE_DEFS='grub-core/Makefile.core.def'

${PYTHON} gentpl.py $CORE_DEFS > grub-core/Makefile.core.am

echo "Saving timestamps..."
echo timestamp > stamp-h.in

echo "Running autoreconf..."
autoreconf -vif

exit 0
