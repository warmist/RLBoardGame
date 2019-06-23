#include "booster.hpp"
#include "lua_helper.hpp"

void lua_load_booster(lua_State * L, int arg, lua_booster& output)
{
	lua_stack_guard g(L, -1);
	lua_getfield(L, arg, "cards");
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		lua_pushvalue(L, -2); //copy key
		const char *key = lua_tostring(L, -1);

		card tmp;
		tmp.key = key;
		lua_load_card(L, -2, tmp);
		output.cards[key] = tmp;
		lua_pop(L, 2);//pop key copy and value

	}
	lua_pop(L, 1);

	lua_getfield(L, arg, "mobs");
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		lua_pushvalue(L, -2); //copy key
		const char *key = lua_tostring(L, -1);

		e_enemy tmp;
		//tmp.key = key;
		lua_load_enemy(L, -2, tmp);
		output.enemies[key] = tmp;
		lua_pop(L, 2);//pop key copy and value
	}
	lua_pop(L, 2);
}