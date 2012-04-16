# Start of user configuration 
#------------------------------------------
ifeq ($(LUAVERSION),52)
  LUAINC = s:\progr\work\system\include\lua52
  LUADLL = c:\exe32\lua52.dll
  LUAEXE = lua52.exe
else
  LUAINC = s:\progr\work\system\include\lua51
  LUADLL = c:\exe32\lua5.1.dll
  LUAEXE = lua.exe
endif
WINCON_H = c:\mingw32\include\wincon.h
#------------------------------------------
# End of user configuration

CC      = gcc
PROJECT = cons
BIN     = $(PROJECT).dll
DEF     = $(PROJECT).def
OBJ     = cons.o flags.o
CFLAGS  = -I$(LUAINC) -W -Wall -O2

.PHONY: all clean

all: $(BIN)

clean:
	del $(OBJ) $(BIN) $(DEF) flags.c

$(BIN): $(OBJ) $(DEF)
	$(CC) -shared -o $@ $^ $(LUADLL) -s

$(DEF):
	$(LUAEXE) -e"print('EXPORTS\n\tluaopen_$(PROJECT)')" > $@

flags.c: $(WINCON_H) makeflags.lua
	$(LUAEXE) makeflags.lua $(WINCON_H) > $@

# End of Makefile
