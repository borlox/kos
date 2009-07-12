SRCFILES = $(shell find kernel -mindepth 1 -maxdepth 4 -name "*.c")
ASMFILES = $(shell find kernel -mindepth 1 -maxdepth 4 -name "*.s")

OBJFILES = $(patsubst kernel/%.c,bin/%.o,$(SRCFILES))
ASMOBJS  = $(patsubst kernel/%.s,bin/%.s.o,$(ASMFILES))

ASM = nasm
ASMFLAGS = -felf

CC = kgcc
CFLAGS = -Wall -O2 -static -c -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs \
         -Iinclude -Iinclude/arch/i386 -Ikernel/include

LD = kld
LDFLAGS = -Llib -static -Tlink.ld

LIBS = -lminc -lk

LOGFILES = -serial file:kos.log -serial file:kos_err.log -serial file:kos_dbg.log -serial file:kos_dbgv.log

.PHONY: all clean version verupdate initrd floppy iso run run-iso

all: verupdate kernel linkmap.txt

clean:
	-@rm $(wildcard $(OBJFILES) $(ASMOBJS)) linkmap.txt 
	
version:
	@$(ASM) -v
	@$(CC) -v
	@$(LD) -v
	
verupdate:
	@./version.sh

kernel: bin/kos.bin

initrd:
	mkid initrd bin/initrd

floppy: 
	@./cpyfiles.sh floppy
	@bfi -t=144 -f=img/kos.img tmp -b=../tools/grub/grldr.mbr
	@cmd "/C makeboot.bat img\kos.img "
	@rm -rf tmp
	
iso:
	@./cpyfiles.sh iso
	@mkisofs -R -b grldr -no-emul-boot -boot-load-size 4 -boot-info-table -o img/kos.iso tmp
	@rm -rf tmp
	
run:
	@rm -f kos*.log
	@qemu -m 32 $(LOGFILES) -L ../tools/qemu -no-kqemu -fda img/kos.img
	
run-iso:
	@rm -f kos*.log
	@qemu -m 32 $(LOGFILES) -L ../tools/qemu -no-kqemu -cdrom img/kos.iso


bin/kos.bin: $(OBJFILES) $(ASMOBJS) 
	@$(LD) $(LDFLAGS) $(ASMOBJS) $(OBJFILES) $(LIBS) -obin/kos.bin -Map linkmap.txt
	
##
# The following target creates automatic dependency and rule scripts for all objects
##

.rules: $(SRCFILES) Makefile
	@rm -f .rules
	@$(foreach file,$(SRCFILES),\
	echo -n $(subst kernel,bin,$(dir $(file))) >> .rules;\
	$(CC) $(CFLAGS) -MM $(file) >> .rules;\
	echo -e "\t@$(CC) $(CFLAGS) -o$(patsubst kernel/%.c,bin/%.o,$(file)) $(file)" >> .rules;\
	)
	@$(foreach file,$(ASMFILES),\
	echo "$(patsubst kernel/%.s,bin/%.s.o,$(file)): $(file)" >> .rules;\
	echo -e "\t@$(ASM) $(ASMFLAGS) -o$(patsubst kernel/%.s,bin/%.s.o,$(file)) $(file)" >> .rules;\
	)

-include .rules


