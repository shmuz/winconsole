-- This script is intended to generate the "flags.c" file
local io_write     = io.write
local table_insert = table.insert

local function add_defines (src, trg)
  for c in src:gmatch("#define%s+([%w_]+)%s+%(?%s*-?%d+%s*%)?") do
    table_insert(trg, c)
  end
end

local function add_enums (src, trg)
  for enum in src:gmatch("%senum%s*[%w_]*%s*(%b{})") do
    for c in enum:gmatch("\n%s*([%w_]+)") do
      table_insert(trg, c)
    end
  end
end

local function add_ids (src, trg)
  for c in src:gmatch("[%a_][%w_]*") do
    table_insert(trg, c)
  end
end

local function write_target (trg)
  io_write [[
// this array must be sorted by 'key' to allow for binary search
static const flag_pair flags[] = {
]]
  table.sort(trg) -- sort the table: this will allow for binary search
  for k,v in ipairs(trg) do
    io_write(string.format('  {"%s", %s},\n', v, v))
  end
  io_write("};\n\n")
end

-- file "wincon.h"
local failed_to_extract = [[
]]

-- Windows API constants
local s_winapi = [[
  STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE 
]]


local file_top = [[
// flags.c
// DON'T EDIT: THIS FILE IS AUTO-GENERATED.

#define _WIN32_WINNT 0x0601
#define WINVER       0x0601

#include <lua.h>
#include <windows.h>
#include <wincon.h>

typedef struct {
  const char* key;
  int val;
} flag_pair;

]]


local file_bottom = [[
// create a table; fill with flags; leave on stack
void push_flags_table (lua_State *L)
{
  int i = 0;
  const int nelem = sizeof(flags) / sizeof(flags[0]);
  lua_createtable (L, 0, nelem);
  for (i=0; i<nelem; ++i) {
    lua_pushinteger(L, flags[i].val);
    lua_setfield(L, -2, flags[i].key);
  }
}

]]

local function write_common_flags_file (fname)
  assert (fname, "input file not specified")
  local fp = assert (io.open (fname))
  local src = fp:read ("*all")
  fp:close()

  local collector = {}
  add_defines(src, collector)
  add_enums(src, collector)
  add_ids(failed_to_extract, collector)
  add_ids(s_winapi, collector)

  io_write(file_top)
  write_target(collector)
  io_write(file_bottom)
end

-- file "wincon.h"
local fname = ... --> "path-to/wincon.h"
write_common_flags_file(fname)

