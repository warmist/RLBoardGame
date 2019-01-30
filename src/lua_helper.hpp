#ifndef LUA_HELPER_HPP_INCLUDED
#define LUA_HELPER_HPP_INCLUDED
#include <cassert>
#include "lua.hpp"
#include "common.hpp"

struct lua_stack_guard
{
	int offset;
	int start_top;
	lua_State* L;
	lua_stack_guard(lua_State* L, int offset = 0);
	~lua_stack_guard();
};

void print_stack(lua_State *L);
v2i luaL_check_v2i(lua_State* L, int arg);
v2f luaL_check_v2f(lua_State* L, int arg);
void lua_push_v2i(lua_State* L, v2i arg);
std::vector<v2i> lua_to_path(lua_State* L, int arg);
void lua_push_path(lua_State* L, const std::vector<v2i>& path);
int msghandler(lua_State *L);
int lua_safecall(lua_State *L, int narg, int nres);

#endif

#ifdef LUA_HELPER_IMPLEMENTATION

lua_stack_guard::lua_stack_guard(lua_State* L, int offset) :L(L), offset(offset) { start_top = lua_gettop(L); }
lua_stack_guard::~lua_stack_guard(){
	int top = lua_gettop(L);
	assert(top == start_top + offset);
}

void print_stack(lua_State *L) {
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

v2i luaL_check_v2i(lua_State* L, int arg)
{
	if (lua_type(L, arg) != LUA_TTABLE || lua_objlen(L, arg)<2)
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
v2f luaL_check_v2f(lua_State* L, int arg)
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
void lua_push_v2i(lua_State* L, v2i arg)
{
	lua_createtable(L, 2, 0);
	lua_pushinteger(L, arg.x);
	lua_rawseti(L, -2, 1);

	lua_pushinteger(L, arg.y);
	lua_rawseti(L, -2, 2);

	lua_getfield(L, LUA_REGISTRYINDEX, "vec2");
	lua_setmetatable(L, -2);

}
std::vector<v2i> lua_to_path(lua_State* L, int arg)
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

void lua_push_path(lua_State* L, const std::vector<v2i>& path)
{
	lua_createtable(L, (int)path.size(), 0);

	for (int i = 0; i < (int)path.size(); i++)
	{
		lua_createtable(L, 2, 0);
		lua_pushinteger(L, path[i].x);
		lua_rawseti(L, -2, 1);

		lua_pushinteger(L, path[i].y);
		lua_rawseti(L, -2, 2);

		lua_getfield(L, LUA_REGISTRYINDEX, "vec2");
		lua_setmetatable(L, -2);

		lua_rawseti(L, -2, i + 1);
	}
}
int msghandler(lua_State *L) {
	int arg = lua_gettop(L);
	const char *msg = lua_tostring(L, arg);
	if (msg == NULL) {  /* is error object not a string? */
		if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
			lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
			return 1;  /* that is the message */
		else
			msg = lua_pushfstring(L, "(error object is a %s value)",
				luaL_typename(L, arg));
	}
	luaL_traceback(L, L, msg, arg);  /* append a standard traceback */
	return 1;  /* return the traceback */
}

int lua_safecall(lua_State *L, int narg, int nres) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, msghandler);  /* push message handler */
	lua_insert(L, base);  /* put it under function and args */
	status = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);  /* remove message handler from the stack */
	return status;
}
#endif