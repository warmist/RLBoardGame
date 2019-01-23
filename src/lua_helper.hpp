#include <cassert>
#include "lua.hpp"

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