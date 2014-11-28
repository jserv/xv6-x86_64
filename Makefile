X64 = 1

ifneq ("$(X64)","")
BITS = 64
XOBJS = vm64.o
XFLAGS = -m64 -DX64 -mcmodel=kernel -mtls-direct-seg-refs -mno-red-zone
LDFLAGS = -m elf_x86_64 -nodefaultlibs
else
XFLAGS = -m32
LDFLAGS = -m elf_i386 -nodefaultlibs
endif

OPT ?= -O0

OBJS := \
	bio.o \
	console.o \
	cpuid.o \
	exec.o \
	file.o \
	fs.o \
	ide.o \
	ioapic.o \
	kalloc.o \
	kbd.o \
	lapic.o \
	log.o \
	main.o \
	mp.o \
	acpi.o \
	picirq.o \
	pipe.o \
	proc.o \
	spinlock.o \
	string.o \
	swtch$(BITS).o \
	syscall.o \
	sysfile.o \
	sysproc.o \
	timer.o \
	trapasm$(BITS).o \
	trap.o \
	uart.o \
	vectors.o \
	vm.o \
	$(XOBJS)

ifneq ("$(MEMFS)","")
# build filesystem image in to kernel and use memory-ide-device
# instead of mounting the filesystem on ide1
OBJS := $(filter-out ide.o,$(OBJS)) memide.o
FSIMAGE := fs.img
endif

KOBJ_DIR = .kobj
OBJS := $(addprefix $(KOBJ_DIR)/,$(OBJS))

# Cross-compiling (e.g., on Mac OS X)
CROSS_COMPILE ?=

# If the makefile can't find QEMU, specify its path here
QEMU ?= qemu-system-x86_64

CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)gas
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -Werror
CFLAGS += -g -Wall -MD -fno-omit-frame-pointer
CFLAGS += -ffreestanding -fno-common -nostdlib -Iinclude -gdwarf-2 $(XFLAGS) $(OPT)
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -gdwarf-2 -Wa,-divide -Iinclude $(XFLAGS)

xv6.img: out/bootblock out/kernel.elf fs.img
	dd if=/dev/zero of=xv6.img count=10000
	dd if=out/bootblock of=xv6.img conv=notrunc
	dd if=out/kernel.elf of=xv6.img seek=1 conv=notrunc

xv6memfs.img: out/bootblock out/kernelmemfs.elf
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=out/bootblock of=xv6memfs.img conv=notrunc
	dd if=out/kernelmemfs.elf of=xv6memfs.img seek=1 conv=notrunc

# kernel object files
$(KOBJ_DIR)/%.o: kernel/%.c
	@mkdir -p $(KOBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KOBJ_DIR)/%.o: kernel/%.S
	@mkdir -p $(KOBJ_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

UOBJ_DIR = .uobj
# userspace object files
$(UOBJ_DIR)/%.o: user/%.c
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(UOBJ_DIR)/%.o: ulib/%.c
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(UOBJ_DIR)/%.o: ulib/%.S
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

out/bootblock: kernel/bootasm.S kernel/bootmain.c
	@mkdir -p out
	$(CC) -fno-builtin -fno-pic -m32 -nostdinc -Iinclude -O -o out/bootmain.o -c kernel/bootmain.c
	$(CC) -fno-builtin -fno-pic -m32 -nostdinc -Iinclude -o out/bootasm.o -c kernel/bootasm.S
	$(LD) -m elf_i386 -nodefaultlibs -N -e start -Ttext 0x7C00 -o out/bootblock.o out/bootasm.o out/bootmain.o
	$(OBJDUMP) -S out/bootblock.o > out/bootblock.asm
	$(OBJCOPY) -S -O binary -j .text out/bootblock.o out/bootblock
	tools/sign.pl out/bootblock

out/entryother: kernel/entryother.S
	@mkdir -p out
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -o out/entryother.o -c kernel/entryother.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o out/bootblockother.o out/entryother.o
	$(OBJCOPY) -S -O binary -j .text out/bootblockother.o out/entryother
	$(OBJDUMP) -S out/bootblockother.o > out/entryother.asm

INITCODESRC = kernel/initcode$(BITS).S
out/initcode: $(INITCODESRC)
	@mkdir -p out
	$(CC) $(CFLAGS) -nostdinc -I. -o out/initcode.o -c $(INITCODESRC)
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o out/initcode.out out/initcode.o
	$(OBJCOPY) -S -O binary out/initcode.out out/initcode
	$(OBJDUMP) -S out/initcode.o > out/initcode.asm

ENTRYCODE = $(KOBJ_DIR)/entry$(BITS).o
LINKSCRIPT = kernel/kernel$(BITS).ld
out/kernel.elf: $(OBJS) $(ENTRYCODE) out/entryother out/initcode $(LINKSCRIPT) $(FSIMAGE)
	$(LD) $(LDFLAGS) -T $(LINKSCRIPT) -o out/kernel.elf $(ENTRYCODE) $(OBJS) -b binary out/initcode out/entryother $(FSIMAGE)
	$(OBJDUMP) -S out/kernel.elf > out/kernel.asm
	$(OBJDUMP) -t out/kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > out/kernel.sym

MKVECTORS = tools/vectors$(BITS).pl
kernel/vectors.S: $(MKVECTORS)
	perl $(MKVECTORS) > kernel/vectors.S

ULIB := \
	ulib.o \
	usys.o \
	printf.o \
	umalloc.o

ULIB := $(addprefix $(UOBJ_DIR)/,$(ULIB))

FS_DIR = .fs

$(FS_DIR)/%: $(UOBJ_DIR)/%.o $(ULIB)
	@mkdir -p $(FS_DIR) out
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > out/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > out/$*.sym

$(FS_DIR)/forktest: $(UOBJ_DIR)/forktest.o $(ULIB)
	@mkdir -p $(FS_DIR)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FS_DIR)/forktest \
		$(UOBJ_DIR)/forktest.o \
		$(UOBJ_DIR)/ulib.o \
		$(UOBJ_DIR)/usys.o
	$(OBJDUMP) -S $(FS_DIR)/forktest > out/forktest.asm

out/mkfs: tools/mkfs.c include/fs.h
	gcc -Werror -Wall -o out/mkfs tools/mkfs.c

out/opfs: tools/opfs.c tools/libfs.c
	gcc -Werror -Wall -std=c99 -pedantic -o $@ $^

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: $(UOBJ_DIR)/%.o

UPROGS := \
	cat \
	echo \
	forktest \
	grep \
	init \
	kill \
	ln \
	ls \
	mkdir \
	rm \
	sh \
	stressfs \
	usertests \
	wc \
	zombie

UPROGS := $(addprefix $(FS_DIR)/,$(UPROGS))

$(FS_DIR)/README: README
	@mkdir -p fs
	cp -f README $(FS_DIR)/README

fs.img: out/mkfs README $(UPROGS)
	out/mkfs fs.img README $(UPROGS)

-include */*.d

clean: 
	rm -rf out $(FS_DIR) $(UOBJ_DIR) $(KOBJ_DIR)
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
QEMUOPTS = -net none -hdb fs.img xv6.img -smp $(CPUS) -m 512 $(QEMUEXTRA)

qemu: fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) xv6memfs.img -smp $(CPUS)

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: tools/gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

.gdbinit64: tools/gdbinit64.tmpl out/kernel.asm out/bootblock.asm
	cat tools/gdbinit64.tmpl | sed "s/localhost:1234/localhost:$(GDBPORT)/" | sed "s/TO32/$(shell grep 'call\s*\*0x1c' out/bootblock.asm  | cut -f 1 -d: |sed -e 's/^\s*/\*0x/')/" | sed "s/TO64/$(shell grep 'ljmp' -A 1 out/kernel.asm  | grep 'ffffffff80' | cut -f 1 -d: | sed "s/ffffffff80/\*0x/")/" > $@

.gdbinit64-2: tools/gdbinit64-2.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: fs.img xv6.img .gdbinit .gdbinit64 .gdbinit64-2
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)
