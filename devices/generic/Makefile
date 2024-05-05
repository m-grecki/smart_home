#SDCC makefile (test)

CC          = sdcc
#DEBUG=0 # no debug
DEBUG=0 # debug level 1
#DEBUG=2 # debug level 2 (more output)
TEST=1 # program do testowania
#CFLAGS      = -mmcs51 --verbose -V --model-small --debug -DDEBUG=$(DEBUG) -DTEST=$(TEST)
CFLAGS      = -mmcs51 --model-small --debug --nooverlay --opt-code-size --no-xinit-opt -DDEBUG=$(DEBUG) -DTEST=$(TEST)
# -cyclomatic --acall-ajmp
# -I/usr/local/share/include
CODELOC     = 0x0000
RAMLOC      = 0x10
IRAMSIZE    = 0x80
XRAMLOC     = 0xa000
XRAMSIZE    = 0x0
STACK       = 0x28
#LFLAGS      = --verbose -V --code-loc $(CODELOC) --xram-loc $(XRAMLOC) --iram-size $(IRAMSIZE) --data-loc $(RAMLOC) --stack-loc $(STACK) 
LFLAGS      = --code-loc $(CODELOC) --iram-size $(IRAMSIZE)
ASM         = sdas8051
ASMFLAGS    = -log
LIBS        = -lsdcc
RELS_DEV_TEST = dev_test.rel
RELS_IH1    = ih1.rel ih1_def.rel
RELS_IHT    = iht.rel ih1_def.rel
RELS_IHB    = ihb.rel
RELS_TEST   = iht.rel ih-test.rel ih1_def.rel
INCLUDEPATH = 
#LIBPATH     = -L/usr/local/share/lib/small
LIBPATH    = 
#-Lc:/sdcc/device/lib/small
RM          = rm -f
CP          = cp


.SUFFIXES: .rel .c .asm

all : dev_test.ihx
# ih1 ih-test

#dev_test.hex: dev_test.ihx
#	packihx dev_test.ihx >dev_test.hex

dev_test.ihx: $(RELS_DEV_TEST)
	$(CC) $(CFLAGS) $(LFLAGS) $(RELS_DEV_TEST) $(LIBPATH)

.c.rel:
	$(CC) $(CFLAGS) -c $<

dev_test.c: ../ihmg.h


#dev_test.rel : dev_test.asm
#	$(ASM) $(ASMFLAGS) $<

#dev_test.asm : dev_test.c
#	$(CC) -c $(CFLAGS) $<

iht: $(RELS_IHT)
	$(CC) $(CFLAGS) $(LFLAGS) $(RELS_IHT) $(LIBPATH)
	packihx iht.ihx >iht.hex

ihb: $(RELS_IHB)
	$(CC) $(CFLAGS) $(LFLAGS) $(RELS_IHB) $(LIBPATH)
	packihx ihb.ihx >ihb.hex

ih-test: $(RELS_TEST)
	$(CC) $(CFLAGS) --code-loc 0x8000 --xram-loc 0xa000 --iram-size 0x80 --data-loc 0x28 --stack-loc 0x68 $(RELS_TEST) $(LIBPATH)
	packihx ih-test.ihx >ih-test.hex

ih1.rel : ih1.asm transm.inc
	$(ASM) $(ASMFLAGS) $<

ih-test.rel : ih-test.asm transm.inc
	$(ASM) $(ASMFLAGS) $<

ih1_def.rel: ih1_def.c

iht.rel: iht.c

ihb.rel: ihb.c ihs.h ihs_ext.h

#.asm.rel:
#	$(ASM) $(ASMFLAGS) $<

.PHONY : clean

-include dummy

dummy: Makefile
	@touch $@
	@$(MAKE) -s clean

clean:
	-$(RM) e
	-$(RM) ih1_def.asm
	-$(RM) dev_test.asm
	-$(RM) v1.2
	-$(RM) *.ihx
	-$(RM) *.hex
	-$(RM) *.map
	-$(RM) *.mem
	-$(RM) *.rel
	-$(RM) *.cdb
	-$(RM) *.lst
	-$(RM) *.rst
	-$(RM) *.sym
	-$(RM) *.~*
	-$(RM) *~
	-$(RM) *.$$$

