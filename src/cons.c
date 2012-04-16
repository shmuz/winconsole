// Library for handling Windows console from Lua
// Started: 2009-09-17 by Shmuel Zeigerman

#include <windows.h>
#include <lua.h>
#include <lauxlib.h>
extern void push_flags_table (lua_State *L);

#if LUA_VERSION_NUM < 502
  #define ALG_ENVIRONINDEX LUA_ENVIRONINDEX
#else
  #define lua_objlen lua_rawlen
  #define ALG_ENVIRONINDEX lua_upvalueindex(1)
#endif

#ifndef __cplusplus
# define bool int
# define false 0
# define true 1
#endif

/* convert a stack index to positive */
#define abs_index(L, i)  ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
                         lua_gettop(L) + (i) + 1)

static const char ConsoleHandleType[] = "StandardConsoleHandle";

typedef enum {
  STANDARD_CONSOLE,
  SCREEN_BUFFER
} ud_type;

typedef struct {
  HANDLE hnd;
  ud_type type;
} cons_ud;

static bool get_env_flag (lua_State *L, int stack_pos, int *trg)
{
  *trg = 0;
  int type = lua_type (L, stack_pos);
  if (type == LUA_TNUMBER) {
    *trg = lua_tointeger (L, stack_pos);
    return true;
  }
  else if (type == LUA_TNONE || type == LUA_TNIL)
    return true;
  if (type == LUA_TSTRING) {
    lua_getfield (L, ALG_ENVIRONINDEX, lua_tostring(L, stack_pos));
    if (lua_isnumber(L, -1)) {
      *trg = lua_tointeger (L, -1);
      lua_pop (L, 1);
      return true;
    }
    lua_pop (L, 1);
  }
  return false;
}

static int check_env_flag (lua_State *L, int stack_pos)
{
  int trg;
  if(!get_env_flag (L, stack_pos, &trg))
    luaL_argerror(L, stack_pos, "invalid flag");
  return trg;
}

static bool GetFlagCombination (lua_State *L, int stack_pos, int *trg)
{
  *trg = 0;
  int type = lua_type (L, stack_pos);
  if (type == LUA_TNUMBER) {
    *trg = lua_tointeger (L, stack_pos);
    return true;
  }
  else if (type == LUA_TNONE || type == LUA_TNIL)
    return true;
  else if (type == LUA_TSTRING)
    return get_env_flag (L, stack_pos, trg);

  bool res = false;
  if (type == LUA_TTABLE) {
    bool done = false;
    int i = 0;
    for (*trg=0; !done; lua_pop(L,1)) {
      lua_pushinteger (L, ++i);
      lua_gettable (L, stack_pos);
      if (!lua_isnil (L, -1)) {
        int flag;
        if (get_env_flag (L, -1, &flag))
          *trg |= flag;
        else
          done = true;
      }
      else
        res = done = true;
    }
  }
  return res;
}

static int CheckFlags(lua_State* L, int stackpos)
{
  int Flags;
  if (!GetFlagCombination (L, stackpos, &Flags))
    luaL_error(L, "invalid flag combination");
  return Flags;
}

static int f_GetFlags (lua_State *L)
{
  lua_pushvalue (L, ALG_ENVIRONINDEX);
  return 1;
}

static int GetOptIntFromTable(lua_State *L, const char* key, int dflt)
{
  int ret = dflt;
  lua_getfield(L, -1, key);
  if(lua_isnumber(L,-1))
    ret = lua_tointeger(L, -1);
  lua_pop(L, 1);
  return ret;
}

static bool GetOptBoolFromTable(lua_State *L, const char* key, bool dflt)
{
  lua_getfield(L, -1, key);
  bool ret = lua_isnil(L, -1) ? dflt : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return ret;
}

static void PutIntToTable (lua_State *L, const char* key, int val)
{
  lua_pushinteger(L, val);
  lua_setfield(L, -2, key);
}

static void PutNumToTable(lua_State *L, const char* key, double num)
{
  lua_pushnumber(L, num);
  lua_setfield(L, -2, key);
}

static void PutBoolToTable(lua_State *L, const char* key, int num)
{
  lua_pushboolean(L, num);
  lua_setfield(L, -2, key);
}

static void PutStrToTable(lua_State *L, const char* key, const char* str)
{
  lua_pushstring(L, str);
  lua_setfield(L, -2, key);
}

static void FillInputRecord(lua_State *L, int pos, INPUT_RECORD *ir)
{
  pos = abs_index(L, pos);
  luaL_checktype(L, pos, LUA_TTABLE);
  memset(ir, 0, sizeof(INPUT_RECORD));

  bool hasKey;
  // determine event type
  lua_getfield(L, pos, "EventType");
  int temp;
  if(!get_env_flag(L, -1, &temp))
    luaL_argerror(L, pos, "EventType field is missing or invalid");
  lua_pop(L, 1);

  lua_pushvalue(L, pos);
  ir->EventType = temp;
  switch(ir->EventType) {
    case KEY_EVENT:
      ir->Event.KeyEvent.bKeyDown = GetOptBoolFromTable(L, "bKeyDown", false);
      ir->Event.KeyEvent.wRepeatCount = GetOptIntFromTable(L, "wRepeatCount", 1);
      ir->Event.KeyEvent.wVirtualKeyCode = GetOptIntFromTable(L, "wVirtualKeyCode", 0);
      ir->Event.KeyEvent.wVirtualScanCode = GetOptIntFromTable(L, "wVirtualScanCode", 0);
      // prevent simultaneous setting of both UnicodeChar and AsciiChar
      lua_getfield(L, -1, "UnicodeChar");
      hasKey = !(lua_isnil(L, -1));
      lua_pop(L, 1);
      if(hasKey)
        ir->Event.KeyEvent.uChar.UnicodeChar = GetOptIntFromTable(L, "UnicodeChar", 0);
      else {
        ir->Event.KeyEvent.uChar.AsciiChar = GetOptIntFromTable(L, "AsciiChar", 0);
      }
      ir->Event.KeyEvent.dwControlKeyState = GetOptIntFromTable(L, "dwControlKeyState", 0);
      break;

    case MOUSE_EVENT:
      ir->Event.MouseEvent.dwMousePosition.X = GetOptIntFromTable(L, "dwMousePositionX", 0);
      ir->Event.MouseEvent.dwMousePosition.Y = GetOptIntFromTable(L, "dwMousePositionY", 0);
      ir->Event.MouseEvent.dwButtonState = GetOptIntFromTable(L, "dwButtonState", 0);
      ir->Event.MouseEvent.dwControlKeyState = GetOptIntFromTable(L, "dwControlKeyState", 0);
      ir->Event.MouseEvent.dwEventFlags = GetOptIntFromTable(L, "dwEventFlags", 0);
      break;

    case WINDOW_BUFFER_SIZE_EVENT:
      ir->Event.WindowBufferSizeEvent.dwSize.X = GetOptIntFromTable(L, "dwSizeX", 0);
      ir->Event.WindowBufferSizeEvent.dwSize.Y = GetOptIntFromTable(L, "dwSizeY", 0);
      break;

    case MENU_EVENT:
      ir->Event.MenuEvent.dwCommandId = GetOptIntFromTable(L, "dwCommandId", 0);
      break;

    case FOCUS_EVENT:
      ir->Event.FocusEvent.bSetFocus = GetOptBoolFromTable(L, "bSetFocus", false);
      break;
  }
  lua_pop(L, 1);
}

static cons_ud* check_console_ud (lua_State *L, int index)
{
  index = abs_index(L, index);
  return (cons_ud*)luaL_checkudata(L, index, ConsoleHandleType);
}

static HANDLE check_console_handle (lua_State *L, int index)
{
  cons_ud *ud = check_console_ud(L, index);
  luaL_argcheck(L, ud->hnd != INVALID_HANDLE_VALUE, index, "access to closed handle");
  return ud->hnd;
}

static int consolehandle_tostring (lua_State *L)
{
  HANDLE h = check_console_handle (L, 1);
  lua_pushfstring (L, "%s (%p)", ConsoleHandleType, (void*)h);
  return 1;
}

static int consolehandle_gc (lua_State *L)
{
  cons_ud *ud = (cons_ud*)lua_touserdata(L, 1);
  if (ud && lua_getmetatable(L, 1)) {
    luaL_getmetatable(L, ConsoleHandleType);
    if (lua_rawequal(L, -1, -2)) {
      if (ud->type == SCREEN_BUFFER && ud->hnd != INVALID_HANDLE_VALUE) {
        CloseHandle(ud->hnd);
        ud->hnd = INVALID_HANDLE_VALUE;
      }
    }
  }
  return 0;
}

static int consolehandle_close (lua_State *L)
{
  cons_ud *ud = check_console_ud(L, 1);
  if (ud->type == SCREEN_BUFFER && ud->hnd != INVALID_HANDLE_VALUE) {
    CloseHandle(ud->hnd);
    ud->hnd = INVALID_HANDLE_VALUE;
  }
  return 0;
}

static int f_GetStdHandle (lua_State *L)
{
  DWORD nStdHandle = check_env_flag(L, 1);
  HANDLE h = GetStdHandle(nStdHandle);
  if (h == INVALID_HANDLE_VALUE)
    lua_pushnil(L);
  else {
    cons_ud *ud = (cons_ud*)lua_newuserdata(L, sizeof(cons_ud));
    ud->hnd = h;
    ud->type = STANDARD_CONSOLE;
    luaL_getmetatable (L, ConsoleHandleType);
    lua_setmetatable (L, -2);
  }
  return 1;
}

static int f_SetStdHandle (lua_State *L)
{
  DWORD nStdHandle = check_env_flag(L, 1);
  HANDLE h = check_console_handle(L, 2);
  return SetStdHandle(nStdHandle, h), 1;
}

static int f_GetConsoleCursorInfo (lua_State* L)
{
  CONSOLE_CURSOR_INFO info;
  HANDLE h = check_console_handle(L, 1);
  BOOL r = GetConsoleCursorInfo(h, &info);
  if (r == FALSE)
    return lua_pushnil(L), 1;
  lua_pushinteger(L, info.dwSize);
  lua_pushboolean(L, info.bVisible);
  return 2;
}

static int f_SetConsoleCursorInfo (lua_State* L)
{
  CONSOLE_CURSOR_INFO info;
  HANDLE h = check_console_handle(L, 1);
  info.dwSize = luaL_checkinteger(L, 2);
  info.bVisible = lua_toboolean(L, 3);
  lua_pushboolean(L, SetConsoleCursorInfo(h, &info));
  return 1;
}

static int f_SetConsoleCursorPosition (lua_State* L)
{
  COORD coord;
  HANDLE h = check_console_handle(L, 1);
  coord.X = luaL_checkinteger(L, 2);
  coord.Y = luaL_checkinteger(L, 3);
  lua_pushboolean(L, SetConsoleCursorPosition(h, coord));
  return 1;
}

static int f_GetConsoleScreenBufferInfo (lua_State* L)
{
  CONSOLE_SCREEN_BUFFER_INFO info;
  HANDLE h = check_console_handle(L, 1);
  if (!GetConsoleScreenBufferInfo(h, &info))
    return lua_pushnil(L), 1;
  lua_createtable(L, 0, 11);
  PutIntToTable(L, "dwSizeX",              info.dwSize.X);
  PutIntToTable(L, "dwSizeY",              info.dwSize.Y);
  PutIntToTable(L, "dwCursorPositionX",    info.dwCursorPosition.X);
  PutIntToTable(L, "dwCursorPositionY",    info.dwCursorPosition.Y);
  PutIntToTable(L, "wAttributes",          info.wAttributes);
  PutIntToTable(L, "srWindowLeft",         info.srWindow.Left);
  PutIntToTable(L, "srWindowTop",          info.srWindow.Top);
  PutIntToTable(L, "srWindowRight",        info.srWindow.Right);
  PutIntToTable(L, "srWindowBottom",       info.srWindow.Bottom);
  PutIntToTable(L, "dwMaximumWindowSizeX", info.dwMaximumWindowSize.X);
  PutIntToTable(L, "dwMaximumWindowSizeY", info.dwMaximumWindowSize.Y);
  return 1;
}

static int f_FlushConsoleInputBuffer (lua_State* L)
{
  HANDLE h = check_console_handle(L, 1);
  lua_pushboolean(L, FlushConsoleInputBuffer(h));
  return 1;
}

static void InputRecordToTable(lua_State *L, INPUT_RECORD *ir)
{
  switch(ir->EventType) {
    case KEY_EVENT:
      PutStrToTable(L, "EventType", "KEY_EVENT");
      PutBoolToTable(L,"bKeyDown", ir->Event.KeyEvent.bKeyDown);
      PutNumToTable(L, "wRepeatCount", ir->Event.KeyEvent.wRepeatCount);
      PutNumToTable(L, "wVirtualKeyCode", ir->Event.KeyEvent.wVirtualKeyCode);
      PutNumToTable(L, "wVirtualScanCode", ir->Event.KeyEvent.wVirtualScanCode);
      PutNumToTable(L, "UnicodeChar", ir->Event.KeyEvent.uChar.UnicodeChar);
      PutNumToTable(L, "AsciiChar", ir->Event.KeyEvent.uChar.AsciiChar);
      PutNumToTable(L, "dwControlKeyState", ir->Event.KeyEvent.dwControlKeyState);
      break;

    case MOUSE_EVENT:
      PutStrToTable(L, "EventType", "MOUSE_EVENT");
      PutNumToTable(L, "dwMousePositionX", ir->Event.MouseEvent.dwMousePosition.X);
      PutNumToTable(L, "dwMousePositionY", ir->Event.MouseEvent.dwMousePosition.Y);
      PutNumToTable(L, "dwButtonState", ir->Event.MouseEvent.dwButtonState);
      PutNumToTable(L, "dwControlKeyState", ir->Event.MouseEvent.dwControlKeyState);
      PutNumToTable(L, "dwEventFlags", ir->Event.MouseEvent.dwEventFlags);
      break;

    case WINDOW_BUFFER_SIZE_EVENT:
      PutStrToTable(L, "EventType", "WINDOW_BUFFER_SIZE_EVENT");
      PutNumToTable(L, "dwSizeX", ir->Event.WindowBufferSizeEvent.dwSize.X);
      PutNumToTable(L, "dwSizeY", ir->Event.WindowBufferSizeEvent.dwSize.Y);
      break;

    case MENU_EVENT:
      PutStrToTable(L, "EventType", "MENU_EVENT");
      PutNumToTable(L, "dwCommandId", ir->Event.MenuEvent.dwCommandId);
      break;

    case FOCUS_EVENT:
      PutStrToTable(L, "EventType", "FOCUS_EVENT");
      PutBoolToTable(L,"bSetFocus", ir->Event.FocusEvent.bSetFocus);
      break;
  }
}

static int ReadOrPeekConsoleInput (lua_State* L, int op)
{
  DWORD nRead, i;
  INPUT_RECORD* pBuffer;
  BOOL bResult;

  HANDLE h = check_console_handle(L, 1);
  DWORD nLength = luaL_checkinteger(L, 2);
  luaL_argcheck(L, nLength > 0, 2, "invalid number of records");

  pBuffer = (INPUT_RECORD*)lua_newuserdata(L, nLength*sizeof(INPUT_RECORD));
  bResult = (op == 'R') ? ReadConsoleInput(h, pBuffer, nLength, &nRead) :
                          PeekConsoleInput(h, pBuffer, nLength, &nRead);
  if (!bResult)
    return lua_pushnil(L), 1;

  lua_createtable(L, nRead, 0);
  for (i=0; i<nRead; i++)
  {
    lua_pushinteger(L, i+1);
    lua_createtable(L, 0, 0);
    InputRecordToTable(L, pBuffer+i);
    lua_rawset(L, -3);
  }
  return 1;
}

static int f_ReadConsoleInput (lua_State *L)
{
  return ReadOrPeekConsoleInput(L, 'R');
}

static int f_PeekConsoleInput (lua_State *L)
{
  return ReadOrPeekConsoleInput(L, 'P');
}

static int f_WriteConsoleInput (lua_State *L)
{
  HANDLE h;                      // handle to a console input buffer
  INPUT_RECORD *lpBuffer;        // pointer to the buffer for write data
  DWORD nLength;                 // number of records to write
  DWORD NumberOfEventsWritten;   // number of records written
  DWORD i;

  h = check_console_handle(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);
  nLength = lua_objlen(L, 2);
  luaL_argcheck(L, nLength, 2, "empty array");

  lpBuffer = (INPUT_RECORD*)lua_newuserdata(L, nLength * sizeof(INPUT_RECORD));
  for (i=0; i < nLength; i++)
  {
    lua_pushinteger(L, i+1);
    lua_gettable(L, 2);
    FillInputRecord(L, -1, lpBuffer + i);
    lua_pop(L, 1);
  }
  if (!WriteConsoleInput(h, lpBuffer, nLength, &NumberOfEventsWritten))
    return lua_pushnil(L), 1;
  lua_pushinteger(L, NumberOfEventsWritten);
  return 1;
}

static int f_WriteConsoleOutput (lua_State *L)
{
  CHAR_INFO *lpBuffer;    // pointer to buffer with data to write
  COORD dwBufferSize;     // column-row size of source buffer
  COORD dwBufferCoord;    // upper-left cell to write from
  SMALL_RECT WriteRegion; // pointer to rectangle to write to
  int src_size, i;

  HANDLE h = check_console_handle(L, 1);

  luaL_checktype(L, 2, LUA_TTABLE); // character array
  src_size = lua_objlen(L, 2);
  luaL_argcheck(L, src_size, 2, "empty array");

  luaL_checktype(L, 3, LUA_TTABLE); // attribute array
  luaL_argcheck(L, (int)lua_objlen(L, 3) == src_size, 3,
    "different sizes of character and attribute arrays");

  luaL_checktype(L, 4, LUA_TTABLE); // parameters
  lua_settop(L, 4);
  dwBufferSize.X     = GetOptIntFromTable(L, "dwBufferSizeX", 0);
  dwBufferSize.Y     = GetOptIntFromTable(L, "dwBufferSizeY", 0);
  luaL_argcheck(L, src_size >= dwBufferSize.X * dwBufferSize.Y, 4,
    "SizeX*SizeY is greater than character array size");
  dwBufferCoord.X    = GetOptIntFromTable(L, "dwBufferCoordX", 0);
  dwBufferCoord.Y    = GetOptIntFromTable(L, "dwBufferCoordY", 0);
  WriteRegion.Top    = GetOptIntFromTable(L, "WriteRegionTop", 0);
  WriteRegion.Left   = GetOptIntFromTable(L, "WriteRegionLeft", 0);
  WriteRegion.Bottom = GetOptIntFromTable(L, "WriteRegionBottom", 0);
  WriteRegion.Right  = GetOptIntFromTable(L, "WriteRegionRight", 0);

  lpBuffer = (CHAR_INFO*) lua_newuserdata(L, src_size * sizeof(CHAR_INFO));
  for (i=0; i<src_size; i++,lua_pop(L,2))
  {
    const char* s;
    lua_pushinteger(L, i+1);
    lua_gettable(L, 2);
    s = lua_tostring(L, -1);
    luaL_argcheck(L, s, 2, "non-string in the array");
    lpBuffer[i].Char.AsciiChar = *(const char*)s;
    //lpBuffer[i].Char.UnicodeChar = *(const wchar_t*)s;

    lua_pushinteger(L, i+1);
    lua_gettable(L, 3);
    luaL_argcheck(L, lua_isnumber(L,-1), 3, "non-number in the array");
    lpBuffer[i].Attributes = lua_tointeger(L, -1);
  }

  if (!WriteConsoleOutput(h, lpBuffer, dwBufferSize, dwBufferCoord, &WriteRegion))
    return lua_pushnil(L), 1;

  lua_createtable(L, 0, 4);
  PutNumToTable(L, "WriteRegionTop",    WriteRegion.Top);
  PutNumToTable(L, "WriteRegionLeft",   WriteRegion.Left);
  PutNumToTable(L, "WriteRegionBottom", WriteRegion.Bottom);
  PutNumToTable(L, "WriteRegionRight",  WriteRegion.Right);
  return 1;
}

static int f_GetConsoleMode (lua_State *L)
{
  HANDLE h = check_console_handle(L, 1);
  DWORD Mode;
  if (!GetConsoleMode(h, &Mode))
    return lua_pushnil(L), 1;
  lua_pushinteger(L, Mode);
  return 1;
}

static int f_SetConsoleMode (lua_State *L)
{
  HANDLE h = check_console_handle(L, 1);
  DWORD Mode = CheckFlags(L, 2);
  lua_pushboolean(L, SetConsoleMode(h, Mode));
  return 1;
}

static int f_AllocConsole (lua_State *L)
{
  return lua_pushboolean(L, AllocConsole()), 1;
}

static int f_FreeConsole (lua_State *L)
{
  return lua_pushboolean(L, FreeConsole()), 1;
}

static int f_CreateConsoleScreenBuffer (lua_State *L)
{
  DWORD dwDesiredAccess = CheckFlags(L, 1); // access flag
  DWORD dwShareMode = CheckFlags(L, 2);     // buffer share mode
  DWORD dwFlags = CheckFlags(L, 3);         // type of buffer to create

  HANDLE h = CreateConsoleScreenBuffer(dwDesiredAccess, dwShareMode, 0, dwFlags, 0);
  if (h == INVALID_HANDLE_VALUE)
    lua_pushnil(L);
  else {
    cons_ud *ud = (cons_ud*)lua_newuserdata(L, sizeof(cons_ud));
    ud->hnd = h;
    ud->type = SCREEN_BUFFER;
    luaL_getmetatable (L, ConsoleHandleType);
    lua_setmetatable (L, -2);
  }
  return 1;
}

static int f_GetConsoleCP (lua_State *L)
{
  return lua_pushinteger(L, GetConsoleCP()), 1;
}

static int f_GetConsoleOutputCP (lua_State *L)
{
  return lua_pushinteger(L, GetConsoleOutputCP()), 1;
}

static int f_SetConsoleCP (lua_State *L)
{
  UINT wCodePageID = luaL_checkinteger(L, 1);
  return lua_pushboolean(L, SetConsoleCP(wCodePageID)), 1;
}

static int f_SetConsoleOutputCP (lua_State *L)
{
  UINT wCodePageID = luaL_checkinteger(L, 1);
  return lua_pushboolean(L, SetConsoleOutputCP(wCodePageID)), 1;
}

static int f_GetConsoleTitle (lua_State *L)
{
  int size = luaL_optinteger(L, 1, 512);
  if (size < 512) size = 512;
  char *ptr = (char*)lua_newuserdata(L, size);
  if (GetConsoleTitle(ptr, size))
    return lua_pushstring(L, ptr), 1;
  return lua_pushnil(L), 1;
}

static int f_SetConsoleTitle (lua_State *L)
{
  return lua_pushboolean(L, SetConsoleTitle(luaL_checkstring(L, 1))), 1;
}

static int f_GetLargestConsoleWindowSize (lua_State *L)
{
  HANDLE h = check_console_handle(L, 1);
  COORD coord = GetLargestConsoleWindowSize(h);
  lua_pushinteger(L, coord.X);
  lua_pushinteger(L, coord.Y);
  return 2;
}

static int f_SetConsoleScreenBufferSize (lua_State *L)
{
  COORD coord;
  HANDLE h = check_console_handle(L, 1);
  coord.X = luaL_checkinteger(L, 2);
  coord.Y = luaL_checkinteger(L, 3);
  return lua_pushboolean(L, SetConsoleScreenBufferSize(h, coord)), 1;
}

static int f_SetConsoleTextAttribute (lua_State *L)
{
  HANDLE h = check_console_handle(L, 1);
  WORD wAttributes = CheckFlags(L, 2);
  return lua_pushboolean(L, SetConsoleTextAttribute(h, wAttributes)), 1;
}

static int f_SetConsoleActiveScreenBuffer (lua_State *L)
{
  HANDLE h = check_console_handle(L, 1);
  return lua_pushboolean(L, SetConsoleActiveScreenBuffer(h)), 1;
}

static int f_GetNumberOfConsoleInputEvents (lua_State *L)
{
  DWORD n;
  HANDLE h = check_console_handle(L, 1);
  GetNumberOfConsoleInputEvents(h, &n) ? lua_pushinteger(L, n) : lua_pushnil(L);
  return 1;
}

static int f_GetNumberOfConsoleMouseButtons (lua_State *L)
{
  DWORD n;
  GetNumberOfConsoleMouseButtons(&n) ? lua_pushinteger(L, n) : lua_pushnil(L);
  return 1;
}

static int f_GenerateConsoleCtrlEvent (lua_State *L)
{
  DWORD dwCtrlEvent = check_env_flag(L, 1);
  DWORD dwProcessGroupId = luaL_checkinteger(L, 2);
  lua_pushboolean(L, GenerateConsoleCtrlEvent(dwCtrlEvent, dwProcessGroupId));
  return 1;
}

static int f_SetConsoleCtrlHandler (lua_State *L)
{
  BOOL Add = lua_toboolean(L, 1);
  return lua_pushboolean(L, SetConsoleCtrlHandler(NULL, Add)), 1;
}

static int f_FillConsoleOutputAttribute (lua_State *L)
{
  COORD dwWriteCoord;
  DWORD NumOfAttrsWritten;
  HANDLE h = check_console_handle(L, 1);
  WORD wAttribute = CheckFlags(L, 2);
  DWORD nLength = luaL_checkinteger(L, 3);
  dwWriteCoord.X = luaL_checkinteger(L, 4);
  dwWriteCoord.Y = luaL_checkinteger(L, 5);
  FillConsoleOutputAttribute(h, wAttribute, nLength, dwWriteCoord, &NumOfAttrsWritten)
    ? lua_pushinteger(L, NumOfAttrsWritten): lua_pushnil(L);
  return 1;
}

static int f_FillConsoleOutputCharacter (lua_State *L)
{
  COORD dwWriteCoord;
  DWORD NumOfAttrsWritten;
  HANDLE h = check_console_handle(L, 1);
  TCHAR cCharacter = *(const TCHAR*)luaL_checkstring(L, 2);
  DWORD nLength = luaL_checkinteger(L, 3);
  dwWriteCoord.X = luaL_checkinteger(L, 4);
  dwWriteCoord.Y = luaL_checkinteger(L, 5);
  FillConsoleOutputCharacter(h, cCharacter, nLength, dwWriteCoord, &NumOfAttrsWritten)
    ? lua_pushinteger(L, NumOfAttrsWritten): lua_pushnil(L);
  return 1;
}

static int f_ReadConsole (lua_State *L)
{
  HANDLE hConsoleInput = check_console_handle(L, 1);
  DWORD NumOfCharsToRead = luaL_checkinteger(L, 2);
  DWORD NumOfCharsRead;
  LPVOID lpBuffer = lua_newuserdata(L, NumOfCharsToRead * sizeof(TCHAR));
  ReadConsole(hConsoleInput, lpBuffer, NumOfCharsToRead, &NumOfCharsRead, NULL)
    ? lua_pushinteger(L, NumOfCharsRead): lua_pushnil(L);
  return 1;
}

static int f_ReadConsoleOutputAttribute (lua_State *L)
{
  COORD dwReadCoord;
  DWORD NumOfAttrsRead, i;
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  DWORD nLength = luaL_checkinteger(L, 2);
  dwReadCoord.X = luaL_checkinteger(L, 3);
  dwReadCoord.Y = luaL_checkinteger(L, 4);
  LPWORD lpAttribute = (LPWORD)lua_newuserdata(L, nLength * sizeof(WORD));
  if (!ReadConsoleOutputAttribute(hConsoleOutput, lpAttribute, nLength,
                                  dwReadCoord, &NumOfAttrsRead))
  {
    return lua_pushnil(L), 1;
  }
  lua_createtable(L, NumOfAttrsRead, 0);
  for (i=0; i < NumOfAttrsRead; i++) {
    lua_pushinteger(L, i+1);
    lua_pushinteger(L, lpAttribute[i]);
    lua_rawset(L, -3);
  }
  return 1;
}

static int f_ReadConsoleOutputCharacter (lua_State *L)
{
  COORD dwReadCoord;
  DWORD NumOfCharsRead;
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  DWORD nLength = luaL_checkinteger(L, 2);
  dwReadCoord.X = luaL_checkinteger(L, 3);
  dwReadCoord.Y = luaL_checkinteger(L, 4);
  LPTSTR lpCharacter = (LPTSTR)lua_newuserdata(L, nLength * sizeof(TCHAR));
  if (!ReadConsoleOutputCharacter(hConsoleOutput, lpCharacter, nLength,
                                  dwReadCoord, &NumOfCharsRead))
  {
    return lua_pushnil(L), 1;
  }
  lua_pushlstring(L, lpCharacter, NumOfCharsRead * sizeof(TCHAR));
  return 1;
}

static int f_SetConsoleWindowInfo (lua_State *L)
{
  SMALL_RECT rect;
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  BOOL bAbsolute = lua_toboolean(L, 2);
  rect.Left = luaL_checkinteger(L, 3);
  rect.Top = luaL_checkinteger(L, 4);
  rect.Right = luaL_checkinteger(L, 5);
  rect.Bottom = luaL_checkinteger(L, 6);
  lua_pushboolean(L, SetConsoleWindowInfo(hConsoleOutput, bAbsolute, &rect));
  return 1;
}

static int f_WriteConsole (lua_State *L)
{
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  const TCHAR* lpBuffer = (const TCHAR*)luaL_checkstring(L, 2);
  DWORD nNumOfCharsToWrite = lua_objlen(L, 2) / sizeof(TCHAR);
  DWORD NumOfCharsWritten;
  WriteConsole(hConsoleOutput, lpBuffer, nNumOfCharsToWrite, &NumOfCharsWritten, 0)
    ? lua_pushinteger(L, NumOfCharsWritten) : lua_pushnil(L);
  return 1;
}

static int f_WriteConsoleOutputAttribute (lua_State *L)
{
  COORD wWriteCoord;
  DWORD NumOfAttrsWritten;
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  const WORD* lpAttribute = (const WORD*)luaL_checkstring(L, 2);
  DWORD nLength = lua_objlen(L, 2) / sizeof(WORD);
  wWriteCoord.X = luaL_checkinteger(L, 3);
  wWriteCoord.Y = luaL_checkinteger(L, 4);
  WriteConsoleOutputAttribute(hConsoleOutput, lpAttribute, nLength, wWriteCoord,
    &NumOfAttrsWritten) ? lua_pushinteger(L, NumOfAttrsWritten) : lua_pushnil(L);
  return 1;
}

static int f_WriteConsoleOutputCharacter (lua_State *L)
{
  COORD wWriteCoord;
  DWORD NumOfCharsWritten;
  HANDLE hConsoleOutput = check_console_handle(L, 1);
  LPCTSTR lpCharacter = (LPCTSTR)luaL_checkstring(L, 2);
  DWORD nLength = lua_objlen(L, 2) / sizeof(TCHAR);
  wWriteCoord.X = luaL_checkinteger(L, 3);
  wWriteCoord.Y = luaL_checkinteger(L, 4);
  WriteConsoleOutputCharacter(hConsoleOutput, lpCharacter, nLength, wWriteCoord,
    &NumOfCharsWritten) ? lua_pushinteger(L, NumOfCharsWritten) : lua_pushnil(L);
  return 1;
}

static const luaL_Reg cons_methods [] = {
  {"__tostring",                     consolehandle_tostring},
  {"__gc",                           consolehandle_gc},
  {"close",                          consolehandle_close},
  //--------------------------------------------------------------------------
  {"FillConsoleOutputAttribute",     f_FillConsoleOutputAttribute},
  {"FillConsoleOutputCharacter",     f_FillConsoleOutputCharacter},
  {"FlushConsoleInputBuffer",        f_FlushConsoleInputBuffer},
  {"GetConsoleCursorInfo",           f_GetConsoleCursorInfo},
  {"GetConsoleMode",                 f_GetConsoleMode},
  {"GetConsoleScreenBufferInfo",     f_GetConsoleScreenBufferInfo},
  {"GetLargestConsoleWindowSize",    f_GetLargestConsoleWindowSize},
  {"GetNumberOfConsoleInputEvents",  f_GetNumberOfConsoleInputEvents},
  {"PeekConsoleInput",               f_PeekConsoleInput},
  {"ReadConsole",                    f_ReadConsole},
  {"ReadConsoleInput",               f_ReadConsoleInput},
//{"ReadConsoleOutput",              f_ReadConsoleOutput},
  {"ReadConsoleOutputAttribute",     f_ReadConsoleOutputAttribute},
  {"ReadConsoleOutputCharacter",     f_ReadConsoleOutputCharacter},
//{"ScrollConsoleScreenBuffer",      f_ScrollConsoleScreenBuffer},
  {"SetConsoleActiveScreenBuffer",   f_SetConsoleActiveScreenBuffer},
  {"SetConsoleCursorInfo",           f_SetConsoleCursorInfo},
  {"SetConsoleCursorPosition",       f_SetConsoleCursorPosition},
  {"SetConsoleMode",                 f_SetConsoleMode},
  {"SetConsoleScreenBufferSize",     f_SetConsoleScreenBufferSize},
  {"SetConsoleTextAttribute",        f_SetConsoleTextAttribute},
  {"SetConsoleWindowInfo",           f_SetConsoleWindowInfo},
  {"WriteConsole",                   f_WriteConsole},
  {"WriteConsoleInput",              f_WriteConsoleInput},
  {"WriteConsoleOutput",             f_WriteConsoleOutput},
  {"WriteConsoleOutputAttribute",    f_WriteConsoleOutputAttribute},
  {"WriteConsoleOutputCharacter",    f_WriteConsoleOutputCharacter},
  {NULL, NULL}
};

static const luaL_Reg cons_functions[] = {
  {"AllocConsole",                   f_AllocConsole},
  {"CreateConsoleScreenBuffer",      f_CreateConsoleScreenBuffer},
  {"FreeConsole",                    f_FreeConsole},
  {"GenerateConsoleCtrlEvent",       f_GenerateConsoleCtrlEvent},
  {"GetConsoleCP",                   f_GetConsoleCP},
  {"GetConsoleOutputCP",             f_GetConsoleOutputCP},
  {"GetConsoleTitle",                f_GetConsoleTitle},
  {"GetFlags",                       f_GetFlags},
  {"GetNumberOfConsoleMouseButtons", f_GetNumberOfConsoleMouseButtons},
  {"GetStdHandle",                   f_GetStdHandle},
  {"SetConsoleCP",                   f_SetConsoleCP},
  {"SetConsoleCtrlHandler",          f_SetConsoleCtrlHandler},
  {"SetConsoleOutputCP",             f_SetConsoleOutputCP},
  {"SetConsoleTitle",                f_SetConsoleTitle},
  {"SetStdHandle",                   f_SetStdHandle},
  {NULL, NULL}
};

static void CreateType (lua_State *L, const char *name, const luaL_Reg *methods)
{
  luaL_newmetatable(L, name);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
#if LUA_VERSION_NUM == 501
  luaL_register(L, NULL, methods);
#else
  lua_pushvalue(L, -2);
  luaL_setfuncs(L, methods, 1);
#endif
  lua_pop(L, 1);
}

int luaopen_cons (lua_State *L)
{
  push_flags_table (L);
#if LUA_VERSION_NUM == 501
  lua_replace (L, LUA_ENVIRONINDEX);
  CreateType(L, ConsoleHandleType, cons_methods);
  luaL_register(L, "cons", cons_functions);
#else
  CreateType(L, ConsoleHandleType, cons_methods);
  lua_createtable(L, 0, sizeof(cons_functions)/sizeof(luaL_Reg) - 1);
  lua_pushvalue(L, -2);
  luaL_setfuncs(L, cons_functions, 1);
#endif
  return 1;
}
