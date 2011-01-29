# Start of user configuration 
#------------------------------------------
LUAINC   = s:\progr\work\system\include
LUADLL   = c:\exe\lua5.1.dll
LUAEXE   = lua.exe
WINCON_H = c:\dev-cpp\include\wincon.h
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

flags.c: makeflags.lua
	$(LUAEXE) $< $(WINCON_H) > $@

# End of Makefile
