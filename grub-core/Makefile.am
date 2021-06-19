AUTOMAKE_OPTIONS = subdir-objects -Wno-portability

DEPDIR=.deps-core

include $(top_srcdir)/conf/Makefile.common

CC=$(TARGET_CC)
CPP=$(TARGET_CC)
CCAS=$(TARGET_CC)
RANLIB=$(TARGET_RANLIB)
STRIP=$(TARGET_STRIP)

MACHO2IMG=$(top_builddir)/grub-macho2img

AM_CFLAGS = $(TARGET_CFLAGS)
AM_LDFLAGS = $(TARGET_LDFLAGS)
AM_CPPFLAGS = $(TARGET_CPPFLAGS) $(CPPFLAGS_DEFAULT)
AM_CCASFLAGS = $(TARGET_CCASFLAGS) $(CCASFLAGS_DEFAULT)

CFLAGS_PROGRAM += $(CFLAGS_PLATFORM)
LDFLAGS_PROGRAM += $(LDFLAGS_PLATFORM)
CPPFLAGS_PROGRAM += $(CPPFLAGS_PLATFORM)
CCASFLAGS_PROGRAM += $(CCASFLAGS_PLATFORM)

CFLAGS_LIBRARY += $(CFLAGS_PLATFORM) -fno-builtin
CPPFLAGS_LIBRARY += $(CPPFLAGS_PLATFORM)
CCASFLAGS_LIBRARY += $(CCASFLAGS_PLATFORM)

build-grub-pep2elf$(BUILD_EXEEXT): $(top_srcdir)/util/grub-pe2elf.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_TARGET_WORDSIZE=64 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-pep2elf\" $^
CLEANFILES += build-grub-pep2elf$(BUILD_EXEEXT)

build-grub-pe2elf$(BUILD_EXEEXT): $(top_srcdir)/util/grub-pe2elf.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_TARGET_WORDSIZE=32 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-pe2elf\" $^
CLEANFILES += build-grub-pe2elf$(BUILD_EXEEXT)

# gentrigtables
gentrigtables$(BUILD_EXEEXT): gentrigtables.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) $< $(BUILD_LIBM)
CLEANFILES += gentrigtables$(BUILD_EXEEXT)

build-grub-module-verifier$(BUILD_EXEEXT): $(top_srcdir)/util/grub-module-verifier.c $(top_srcdir)/util/grub-module-verifier32.c $(top_srcdir)/util/grub-module-verifier64.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-module-verifier\" $^
CLEANFILES += build-grub-module-verifier$(BUILD_EXEEXT)

# trigtables.c
trigtables.c: gentrigtables$(BUILD_EXEEXT) gentrigtables.c $(top_srcdir)/configure.ac
	./gentrigtables$(BUILD_EXEEXT) > $@
CLEANFILES += trigtables.c

include $(srcdir)/Makefile.core.am

KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/cache.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/command.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/device.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/dl.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/sb.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/env.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/env_private.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/err.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/file.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/fs.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i18n.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/list.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lockdown.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/misc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/compiler-rt.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/mm.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/parser.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/partition.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/stack_protector.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/term.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/time.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/verify.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/mm_private.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/net.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/memory.h

if COND_i386_efi
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/pmtimer.h
endif

if COND_i386_multiboot
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_x86_64_efi
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/pmtimer.h
endif

symlist.h: $(top_builddir)/config.h $(KERNEL_HEADER_FILES)
	@list='$^'; \
	for p in $$list; do \
	  echo "#include <$$p>" >> $@ || (rm -f $@; exit 1); \
	done
CLEANFILES += symlist.h
BUILT_SOURCES += symlist.h

symlist.c: symlist.h gensymlist.sh
	$(TARGET_CPP) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS_KERNEL) $(CPPFLAGS) -DGRUB_SYMBOL_GENERATOR=1 symlist.h > symlist.p || (rm -f symlist.p; exit 1)
	cat symlist.p | $(SHELL) $(srcdir)/gensymlist.sh $(top_builddir)/config.h $(KERNEL_HEADER_FILES) >$@ || (rm -f $@; exit 1)
	rm -f symlist.p
CLEANFILES += symlist.c
BUILT_SOURCES += symlist.c

if COND_HAVE_ASM_USCORE
ASM_PREFIX=_
else
ASM_PREFIX=
endif

noinst_DATA += kernel_syms.lst

kernel_syms.lst: $(KERNEL_HEADER_FILES) $(top_builddir)/config.h
	$(TARGET_CPP) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS_KERNEL) $(CPPFLAGS) $(CFLAGS) -DGRUB_SYMBOL_GENERATOR=1 $^ >kernel_syms.input
	cat kernel_syms.input | grep -v '^#' | sed -n \
	  -e '/EXPORT_FUNC *([a-zA-Z0-9_]*)/{s/.*EXPORT_FUNC *(\([a-zA-Z0-9_]*\)).*/defined kernel '"$(ASM_PREFIX)"'\1/;p;}' \
	  -e '/EXPORT_VAR *([a-zA-Z0-9_]*)/{s/.*EXPORT_VAR *(\([a-zA-Z0-9_]*\)).*/defined kernel '"$(ASM_PREFIX)"'\1/;p;}' \
	  | sort -u >$@
	rm -f kernel_syms.input
CLEANFILES += kernel_syms.lst

syminfo.lst: gensyminfo.sh kernel_syms.lst $(MODULE_FILES)
	cat kernel_syms.lst > $@.new
	for m in $(MODULE_FILES); do \
	  sh $< $$m >> $@.new || exit 1; \
	done
	mv $@.new $@

# generate global module dependencies list
moddep.lst: syminfo.lst genmoddep.awk
	cat $< | sort | $(AWK) -f $(srcdir)/genmoddep.awk > $@ || (rm -f $@; exit 1)
platform_DATA += moddep.lst
CLEANFILES += config.log syminfo.lst moddep.lst

$(MOD_FILES): %.mod : genmod.sh moddep.lst %.module$(EXEEXT) build-grub-module-verifier$(BUILD_EXEEXT)
	TARGET_OBJ2ELF=@TARGET_OBJ2ELF@ sh $^ $@
platform_DATA += $(MOD_FILES)
platform_DATA += modinfo.sh
CLEANFILES += $(MOD_FILES)
