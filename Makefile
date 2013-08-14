OBJDIR:=./bin
SRCDIR:=./src
AR:=ar
CC:=gcc #Compiler
EDL:=gcc #Linker
ARFLAGS:=rcs
CCFLAGS:=-Wall -g -std=gnu99 `pkg-config fuse libcurl glib-2.0 jansson openssl --cflags` #Compiler options
EDLFLAGS:=-Wall -g `pkg-config fuse glib-2.0 jansson openssl --libs` #Linker options
EXE:=$(OBJDIR)/tftfs
DEFINES:=-D DEBUG #Preprocessor definitions
ECHO:=@echo

OBJ:=$(OBJDIR)/tftfs.o $(OBJDIR)/utils.o $(OBJDIR)/http.o $(OBJDIR)/connections.o $(OBJDIR)/tft.o \
		 $(OBJDIR)/response_parsing.o $(OBJDIR)/websocket.o

.PHONY: all clean debug

all: $(OBJDIR) $(EXE)

debug: $(EXE)-debug
$(OBJDIR) :
	test -d $(OBJDIR) || mkdir $(OBJDIR)

$(EXE): $(OBJ)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(OBJ) -lcurl
	@echo done

$(EXE)-debug: $(OBJ)
	@echo building $<
	$(EDL) -o $(EXE)-debug $(EDLFLAGS) $(OBJ) libcurl.a -lz -ldl -lssl -lcrypto -lssh2 -lidn -lrtmp -lldap -llber
	@echo done

$(OBJDIR)/%.o : $(SRCDIR)/%.c $(SRCDIR)/*.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c $(DEFINES) -o $@ $<
	@echo done

clean:
	@echo -n cleaning repository...
	@rm -rf *.o
	@rm -rf *.so*
	@rm -rf $(OBJDIR)/*.o
	@rm -f *~
	@echo cleaned.
