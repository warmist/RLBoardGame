#include <cassert>
#include "lua.hpp"
#include "common.hpp"

struct lua_stack_guard
{
	int offset;
	int start_top;
	lua_State* L;
	lua_stack_guard(lua_State* L, int offset = 0) :L(L), offset(offset) { start_top = lua_gettop(L); }
	~lua_stack_guard()
	{
		int top = lua_gettop(L);
		assert(top == start_top + offset);
	}
};

inline void print_stack(lua_State *L) {
	int top = lua_gettop(L);
	for (int i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {

		case LUA_TSTRING:  /* strings */
			printf("`%s'", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			printf(lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:  /* numbers */
			printf("%g", lua_tonumber(L, i));
			break;

		default:  /* other values */
			printf("%s", lua_typename(L, t));
			break;

		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */
}
/*
static int typeerror(lua_State *L, int narg, const char *tname) {
	const char *msg = lua_pushfstring(L, "%s expected, got %s",
		tname, luaL_typename(L, narg));
	return luaL_argerror(L, narg, msg);
}


static void tag_error(lua_State *L, int narg, int tag) {
	typeerror(L, narg, lua_typename(L, tag));
}

LUALIB_API const char *luaL_checklstring (lua_State *L, int narg, size_t *len) {
const char *s = lua_tolstring(L, narg, len);
if (!s) tag_error(L, narg, LUA_TSTRING);
return s;
}

*/
inline v2i luaL_check_v2i(lua_State* L,int arg)
{
	if(lua_type(L,arg)!=LUA_TTABLE || lua_objlen(L,arg)<2)
	{
		auto err = lua_pushfstring(L, "{x,y} expected, got %s", luaL_typename(L, arg));
		luaL_argerror(L, arg, err);
	}
	v2i ret;
	lua_rawgeti(L, arg, 1);
	ret.x = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_rawgeti(L, arg, 2);
	ret.y = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	return ret;
}
inline v2f luaL_check_v2f(lua_State* L, int arg)
{
	if (lua_type(L, arg) != LUA_TTABLE || lua_objlen(L, arg)<2)
	{
		auto err = lua_pushfstring(L, "{x,y} expected, got %s", luaL_typename(L, arg));
		luaL_argerror(L, arg, err);
	}
	v2f ret;
	lua_rawgeti(L, arg, 1);
	ret.x = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_rawgeti(L, arg, 2);
	ret.y = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
	return ret;
}
inline void lua_push_v2i(lua_State* L, v2i arg)
{
	lua_createtable(L, 2, 0);
	lua_pushinteger(L, arg.x);
	lua_rawseti(L, -2, 1);

	lua_pushinteger(L, arg.y);
	lua_rawseti(L, -2, 2);
}
inline std::vector<v2i> lua_to_path(lua_State* L, int arg)
{
	luaL_checktype(L, arg, LUA_TTABLE);
	int count = (int)lua_objlen(L, arg);
	std::vector<v2i> ret;
	ret.resize(count);
	for (int i = 1; i <= count; i++)
	{
		
		lua_rawgeti(L, arg, i);
		
		lua_rawgeti(L, -1, 1);
		ret[i - 1].x = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_rawgeti(L, -1, 2);
		ret[i - 1].y = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_pop(L, 1);
	}
	return ret;
}

inline void lua_push_path(lua_State* L, const std::vector<v2i>& path)
{
	lua_createtable(L, (int)path.size(), 0);

	for (int i = 0; i < (int)path.size();i++)
	{
		lua_createtable(L, 2, 0);
		lua_pushinteger(L, path[i].x);
		lua_rawseti(L, -2, 1);

		lua_pushinteger(L, path[i].y);
		lua_rawseti(L, -2, 2);

		lua_rawseti(L, -2, i + 1);
	}
}