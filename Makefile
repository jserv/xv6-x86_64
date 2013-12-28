OBJS = \
	kobj/bio.o\
	kobj/console.o\
	kobj/exec.o\
	kobj/file.o\
	kobj/fs.o\
	kobj/ide.o\
	kobj/ioapic.o\
	kobj/kalloc.o\
	kobj/kbd.o\
	kobj/lapic.o\
	kobj/log.o\
	kobj/main.o\
	kobj/mp.o\
	kobj/picirq.o\
	kobj/pipe.o\
	kobj/proc.o\
	kobj/spinlock.o\
	kobj/string.o\
	kobj/swtch.o\
	kobj/syscall.o\
	kobj/sysfile.o\
	kobj/sysproc.o\
	kobj/timer.o\
	kobj/trapasm.o\
	kobj/trap.o\
	kobj/uart.o\
	kobj/vectors.o\
	kobj/vm.o\

# Cross-compiling (e.g., on Mac OS X)
#TOOLPREFIX = i386-jos-elf-

# Using native tools (e.g., on X86 Linux)
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# If the makefile can't find QEMU, specify its path here
#QEMU = 

# Try to infer the correct QEMU
ifndef QEMU
QEMU = $(shell if which qemu > /dev/null; \
	then echo qemu; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

XFLAGS = -I/usr/include/x86_64-linux-gnu

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
#CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -Iinclude $(XFLAGS)
ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
ASFLAGS += -Iinclude $(XFLAGS)

# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)

xv6.img: out/bootblock out/kernel.elf fs.img
	dd if=/dev/zero of=xv6.img count=10000
	dd if=out/bootblock of=xv6.img conv=notrunc
	dd if=out/kernel.elf of=xv6.img seek=1 conv=notrunc

xv6memfs.img: out/bootblock out/kernelmemfs.elf
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=out/bootblock of=xv6memfs.img conv=notrunc
	dd if=out/kernelmemfs.elf of=xv6memfs.img seek=1 conv=notrunc

# kernel object files
kobj/%.o: kernel/%.c
	@mkdir -p kobj
	$(CC) $(CFLAGS) -c -o $@ $<

kobj/%.o: kernel/%.S
	@mkdir -p kobj
	$(CC) $(ASFLAGS) -c -o $@ $<

# userspace object files
uobj/%.o: user/%.c
	@mkdir -p uobj
	$(CC) $(CFLAGS) -c -o $@ $<

uobj/%.o: ulib/%.c
	@mkdir -p uobj
	$(CC) $(CFLAGS) -c -o $@ $<

uobj/%.o: ulib/%.S
	@mkdir -p uobj
	$(CC) $(ASFLAGS) -c -o $@ $<

out/bootblock: kernel/bootasm.S kernel/bootmain.c
	@mkdir -p out
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -o out/bootmain.o -c kernel/bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -o out/bootasm.o -c kernel/bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o out/bootblock.o out/bootasm.o out/bootmain.o
	$(OBJDUMP) -S out/bootblock.o > out/bootblock.asm
	$(OBJCOPY) -S -O binary -j .text out/bootblock.o out/bootblock
	tools/sign.pl out/bootblock

out/entryother: kernel/entryother.S
	@mkdir -p out
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -o out/entryother.o -c kernel/entryother.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o out/bootblockother.o out/entryother.o
	$(OBJCOPY) -S -O binary -j .text out/bootblockother.o out/entryother
	$(OBJDUMP) -S out/bootblockother.o > out/entryother.asm

out/initcode: kernel/initcode.S
	@mkdir -p out
	$(CC) $(CFLAGS) -nostdinc -I. -o out/initcode.o -c kernel/initcode.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o out/initcode.out out/initcode.o
	$(OBJCOPY) -S -O binary out/initcode.out out/initcode
	$(OBJDUMP) -S out/initcode.o > out/initcode.asm

out/kernel.elf: $(OBJS) kernel/entry.o out/entryother out/initcode kernel/kernel.ld
	$(LD) $(LDFLAGS) -T kernel/kernel.ld -o out/kernel.elf kernel/entry.o $(OBJS) -b binary out/initcode out/entryother
	$(OBJDUMP) -S out/kernel.elf > out/kernel.asm
	$(OBJDUMP) -t out/kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > out/kernel.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
MEMFSOBJS = $(filter-out kernel/ide.o,$(OBJS)) kernel/memide.o
out/kernelmemfs.elf: $(MEMFSOBJS) kernel/entry.o out/entryother out/initcode fs.img kernel/kernel.ld
	$(LD) $(LDFLAGS) -T kernel/kernel.ld -o out/kernelmemfs.elf kernel/entry.o  $(MEMFSOBJS) -b binary out/initcode out/entryother fs.img
	$(OBJDUMP) -S out/kernelmemfs.elf > kernelmemfs.asm
	$(OBJDUMP) -t out/kernelmemfs.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernelmemfs.sym

tags: $(OBJS) entryother.S _init
	etags *.S *.c

kernel/vectors.S: tools/vectors.pl
	perl tools/vectors.pl > kernel/vectors.S

ULIB = uobj/ulib.o uobj/usys.o uobj/printf.o uobj/umalloc.o

fs/%: uobj/%.o $(ULIB)
	@mkdir -p fs out
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > out/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > out/$*.sym

fs/forktest: uobj/forktest.o $(ULIB)
	@mkdir -p fs
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o fs/forktest uobj/forktest.o uobj/ulib.o uobj/usys.o
	$(OBJDUMP) -S fs/forktest > out/forktest.asm

out/mkfs: tools/mkfs.c include/fs.h
	gcc -Werror -Wall -o out/mkfs tools/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: uobj/%.o

UPROGS=\
	fs/cat\
	fs/echo\
	fs/forktest\
	fs/grep\
	fs/init\
	fs/kill\
	fs/ln\
	fs/ls\
	fs/mkdir\
	fs/rm\
	fs/sh\
	fs/stressfs\
	fs/usertests\
	fs/wc\
	fs/zombie\

fs/README: README
	@mkdir -p fs
	cp README fs/README

fs.img: out/mkfs README $(UPROGS)
	out/mkfs fs.img README $(UPROGS)

-include */*.d

clean: 
	rm -rf out fs uobj kobj
	rm -f kernel/vectors.S xv6.img xv6memfs.img fs.img .gdbinit

# run in emulators

bochs : fs.img xv6.img
	if [ ! -e .bochsrc ]; then ln -s tools/dot-bochsrc .bochsrc; fi
	bochs -q

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 2
endif
QEMUOPTS = -hdb fs.img xv6.img -smp $(CPUS) -m 512 $(QEMUEXTRA)

qemu: fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) xv6memfs.img -smp $(CPUS)

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

