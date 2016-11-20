# Default make target
.PHONY: all
all: xv6.img fs.img

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

# Turn off PIE which is no default on ubuntu
XFLAGS += -fno-pie

OUT = out

HOST_CC ?= gcc

# specify OPT to enable optimizations. improves performance, but may make
# debugging more difficult
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

# cc-option
# Usage: OP_CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)
cc-option = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
	> /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -Werror
CFLAGS += -g -Wall -MD -fno-omit-frame-pointer
CFLAGS += -ffreestanding -fno-common -nostdlib -Iinclude -gdwarf-2 $(XFLAGS) $(OPT)
CFLAGS += $(call cc-option, -fno-stack-protector, "")
CFLAGS += $(call cc-option, -fno-stack-protector-all, "")
ASFLAGS = -gdwarf-2 -Wa,-divide -Iinclude $(XFLAGS)

xv6.img: $(OUT)/bootblock $(OUT)/kernel.elf fs.img
	dd if=/dev/zero of=xv6.img count=10000
	dd if=$(OUT)/bootblock of=xv6.img conv=notrunc
	dd if=$(OUT)/kernel.elf of=xv6.img seek=1 conv=notrunc

xv6memfs.img: $(OUT)/bootblock $(OUT)/kernelmemfs.elf
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=$(OUT)/bootblock of=xv6memfs.img conv=notrunc
	dd if=$(OUT)/kernelmemfs.elf of=xv6memfs.img seek=1 conv=notrunc

# kernel object files
$(KOBJ_DIR)/%.o: kernel/%.c
	@mkdir -p $(KOBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KOBJ_DIR)/%.o: kernel/%.S
	@mkdir -p $(KOBJ_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

UOBJ_DIR = .uobj
# userspace object files

# FIXME: -O1 and -O2 result in larger user programs, which can not fit mkfs
CFLAGS_user = $(filter-out -O1 -O2,$(CFLAGS)) -Os

$(UOBJ_DIR)/%.o: user/%.c
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(CFLAGS_user) -c -o $@ $<

$(UOBJ_DIR)/%.o: ulib/%.c
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(CFLAGS_user) -c -o $@ $<

$(UOBJ_DIR)/%.o: ulib/%.S
	@mkdir -p $(UOBJ_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

# bootblock is optimized for space
$(OUT)/bootblock: kernel/bootasm.S kernel/bootmain.c
	@mkdir -p $(OUT)
	$(CC) -fno-builtin -fno-pic -m32 -nostdinc -Iinclude -Os -o $(OUT)/bootmain.o -c kernel/bootmain.c
	$(CC) -fno-builtin -fno-pic -m32 -nostdinc -Iinclude -o $(OUT)/bootasm.o -c kernel/bootasm.S
	$(LD) -m elf_i386 -nodefaultlibs --omagic -e start -Ttext 0x7C00 \
		-o $(OUT)/bootblock.o $(OUT)/bootasm.o $(OUT)/bootmain.o
	$(OBJDUMP) -S $(OUT)/bootblock.o > $(OUT)/bootblock.asm
	$(OBJCOPY) -S -O binary -j .text $(OUT)/bootblock.o $(OUT)/bootblock
	tools/sign.pl $(OUT)/bootblock

$(OUT)/entryother: kernel/entryother.S
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -o $(OUT)/entryother.o -c kernel/entryother.S
	$(LD) $(LDFLAGS) --omagic -e start -Ttext 0x7000 -o $(OUT)/bootblockother.o $(OUT)/entryother.o
	$(OBJCOPY) -S -O binary -j .text $(OUT)/bootblockother.o $(OUT)/entryother
	$(OBJDUMP) -S $(OUT)/bootblockother.o > $(OUT)/entryother.asm

INITCODESRC = kernel/initcode$(BITS).S
$(OUT)/initcode: $(INITCODESRC)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -nostdinc -I. -o $(OUT)/initcode.o -c $(INITCODESRC)
	$(LD) $(LDFLAGS) --omagic -e start -Ttext 0 -o $(OUT)/initcode.out out/initcode.o
	$(OBJCOPY) -S -O binary out/initcode.out $(OUT)/initcode
	$(OBJDUMP) -S $(OUT)/initcode.o > $(OUT)/initcode.asm

ENTRYCODE = $(KOBJ_DIR)/entry$(BITS).o
LINKSCRIPT = kernel/kernel$(BITS).ld
$(OUT)/kernel.elf: $(OBJS) $(ENTRYCODE) $(OUT)/entryother $(OUT)/initcode $(LINKSCRIPT) $(FSIMAGE)
	$(LD) $(LDFLAGS) -T $(LINKSCRIPT) -o $(OUT)/kernel.elf \
		$(ENTRYCODE) $(OBJS) \
		-b binary $(OUT)/initcode $(OUT)/entryother $(FSIMAGE)
	$(OBJDUMP) -S $(OUT)/kernel.elf > $(OUT)/kernel.asm
	$(OBJDUMP) -t $(OUT)/kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(OUT)/kernel.sym

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

LDFLAGS_user = $(LDFLAGS)

# use simple contiguous section layout and do not use dynamic linking
LDFLAGS_user += --omagic # same as "-N"

# where program execution should begin
LDFLAGS_user += --entry=main

# location in memory where the program will be loaded
LDFLAGS_user += --section-start=.text=0x0 # same of "-Ttext="

$(FS_DIR)/%: $(UOBJ_DIR)/%.o $(ULIB)
	@mkdir -p $(FS_DIR) $(OUT)
	$(LD) $(LDFLAGS_user) -o $@ $^
	$(OBJDUMP) -S $@ > $(OUT)/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(OUT)/$*.sym

$(FS_DIR)/forktest: $(UOBJ_DIR)/forktest.o $(ULIB)
	@mkdir -p $(FS_DIR)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS_user) -o $(FS_DIR)/forktest \
		$(UOBJ_DIR)/forktest.o \
		$(UOBJ_DIR)/ulib.o \
		$(UOBJ_DIR)/usys.o
	$(OBJDUMP) -S $(FS_DIR)/forktest > $(OUT)/forktest.asm

$(OUT)/mkfs: tools/mkfs.c include/fs.h
	@mkdir -p $(OUT)
	$(HOST_CC) -Werror -Wall -o $@ tools/mkfs.c

out/opfs: tools/opfs.c tools/libfs.c
	@mkdir -p $(OUT)
	$(HOST_CC) -Werror -Wall -std=c99 -pedantic -o $@ $^

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: $(UOBJ_DIR)/%.o

UPROGS := \
	cat \
	chmod \
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
	@mkdir -p $(FS_DIR)
	cp -f README $(FS_DIR)/README

fs.img: $(OUT)/mkfs $(FS_DIR)/README $(UPROGS)
	$(OUT)/mkfs $@ $(filter-out $(OUT)/mkfs,$^)

-include */*.d

clean: 
	rm -rf $(OUT) $(FS_DIR) $(UOBJ_DIR) $(KOBJ_DIR)
	rm -f kernel/vectors.S xv6.img xv6memfs.img fs.img .gdbinit

# run in emulators

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# number of CPUs to emulate in QEMU
ifndef CPUS
CPUS := $(shell grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
endif
QEMUOPTS = -net none -hdb fs.img xv6.img -smp $(CPUS) -m 512 $(QEMUEXTRA)

qemu: fs.img xv6.img
	@echo Ctrl+a h for help
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	@echo Ctrl+a h for help
	$(QEMU) xv6memfs.img -smp $(CPUS)

qemu-nox: fs.img xv6.img
	@echo Ctrl+a h for help
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: tools/gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

.gdbinit64: tools/gdbinit64.tmpl out/kernel.asm out/bootblock.asm
	cat tools/gdbinit64.tmpl | sed "s/localhost:1234/localhost:$(GDBPORT)/" | sed "s/TO32/$(shell grep 'call\s*\*0x1c' out/bootblock.asm  | cut -f 1 -d: |sed -e 's/^\s*/\*0x/')/" | sed "s/TO64/$(shell grep 'ljmp' -A 1 out/kernel.asm  | grep 'ffffffff80' | cut -f 1 -d: | sed "s/ffffffff80/\*0x/")/" > $@

.gdbinit64-2: tools/gdbinit64-2.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	@echo Ctrl+a h for help
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: fs.img xv6.img .gdbinit .gdbinit64 .gdbinit64-2
	@echo "*** Now run 'gdb'." 1>&2
	@echo Ctrl+a h for help
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

.DEFAULT:
	@echo "No rule to make target $@"
