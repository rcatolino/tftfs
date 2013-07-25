OBJDIR:=./bin
SRCDIR:=./src
AR:=ar
CC:=gcc #Compiler
EDL:=gcc #Linker
ARFLAGS:=rcs
CCFLAGS:=-Wall -g -std=c99 `pkg-config fuse glib-2.0 --cflags` #Compiler options
EDLFLAGS:=-Wall -g -std=c99 `pkg-config fuse glib-2.0 --libs` #Linker options
EXE:=$(OBJDIR)/tftfs
DEFINES:=-D DEBUG #Preprocessor definitions
ECHO:=@echo

OBJ:=$(OBJDIR)/tftfs.o $(OBJDIR)/utils.o

.PHONY: all clean

all: $(OBJDIR) $(EXE)

$(OBJDIR) :
	test -d $(OBJDIR) || mkdir $(OBJDIR)

$(EXE): $(OBJ)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(OBJ)
	@echo done

$(OBJDIR)/%.o : $(SRCDIR)/%.c $(SRCDIR)/*.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c $(DEFINES) -o $@ $<
	@echo done

clean:
	@echo -n cleaning repository...
	@rm -rf *.o
	@rm -rf *.a
	@rm -rf *.so*
	@rm -rf $(OBJDIR)/*.o
	@rm -rf $(OBJDIR)/*.a
	@rm -rf $(OBJDIR)/*.so*
	@rm -f *~
	@echo cleaned.
