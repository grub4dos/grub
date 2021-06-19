#! /usr/bin/env bash

set -e

if [ ! -e grub-core/lib/gnulib/stdlib.in.h ]; then
  echo "Gnulib not yet bootstrapped; run ./bootstrap instead." >&2
  exit 1
fi

# Set ${PYTHON} to plain 'python' if not set already
: ${PYTHON:=python}

export LC_COLLATE=C
unset LC_ALL

find . -iname '*.[ch]' ! -ipath './grub-core/lib/libgcrypt-grub/*' ! -ipath './build-aux/*' ! -ipath './grub-core/lib/libgcrypt/src/misc.c' ! -ipath './grub-core/lib/libgcrypt/src/global.c' ! -ipath './grub-core/lib/libgcrypt/src/secmem.c'  ! -ipath './util/grub-gen-widthspec.c' ! -ipath './util/grub-gen-asciih.c' ! -ipath './gnulib/*' ! -ipath './grub-core/lib/gnulib/*' |sort > po/POTFILES.in
find util -iname '*.in' ! -name Makefile.in  |sort > po/POTFILES-shell.in

echo "Importing unicode..."
${PYTHON} util/import_unicode.py unicode/UnicodeData.txt unicode/BidiMirroring.txt unicode/ArabicShaping.txt grub-core/unidata.c

echo "Generating Automake input..."

# Automake doesn't like including files from a path outside the project.
rm -f contrib grub-core/contrib
if [ "x${GRUB_CONTRIB}" != x ]; then
  [ "${GRUB_CONTRIB}" = contrib ] || ln -s "${GRUB_CONTRIB}" contrib
  [ "${GRUB_CONTRIB}" = grub-core/contrib ] || ln -s ../contrib grub-core/contrib
fi

UTIL_DEFS='Makefile.util.def'
CORE_DEFS='grub-core/Makefile.core.def'

for extra in contrib/*/Makefile.util.def; do
  if test -e "$extra"; then
    UTIL_DEFS="$UTIL_DEFS $extra"
  fi
done

for extra in contrib/*/Makefile.core.def; do
  if test -e "$extra"; then
    CORE_DEFS="$CORE_DEFS $extra"
  fi
done

${PYTHON} gentpl.py $UTIL_DEFS > Makefile.util.am
${PYTHON} gentpl.py $CORE_DEFS > grub-core/Makefile.core.am

for extra in contrib/*/Makefile.common; do
  if test -e "$extra"; then
    echo "include $extra" >> Makefile.util.am
    echo "include $extra" >> grub-core/Makefile.core.am
  fi
done

for extra in contrib/*/Makefile.util.common; do
  if test -e "$extra"; then
    echo "include $extra" >> Makefile.util.am
  fi
done

for extra in contrib/*/Makefile.core.common; do
  if test -e "$extra"; then
    echo "include $extra" >> grub-core/Makefile.core.am
  fi
done

echo "Saving timestamps..."
echo timestamp > stamp-h.in

if [ -z "$FROM_BOOTSTRAP" ]; then
  # Unaided autoreconf is likely to install older versions of many files
  # than the ones provided by Gnulib, but in most cases this won't matter
  # very much.  This mode is provided so that you can run ./autogen.sh to
  # regenerate the GRUB build system in an unpacked release tarball (perhaps
  # after patching it), even on systems that don't have access to
  # gnulib.git.
  echo "Running autoreconf..."
  cp -a INSTALL INSTALL.grub
  autoreconf -vif
  mv INSTALL.grub INSTALL
fi

exit 0
