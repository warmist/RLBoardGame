#include "enemy.hpp"
#include "lua_helper.hpp"
//TODO: @refactor 1:1 @copy from card.cpp
static lua_State* yieldable_func(lua_State* L, int enemy_id, const char* fname)
{
	lua_stack_guard g(L, 1);
	//start a lua coroutine. 
	//This then can get yielded a few times until the effects are all "animated"

	lua_rawgeti(L, LUA_REGISTRYINDEX, enemy_id);
	int enemy_data_pos = lua_gettop(L);
	lua_getfield(L, -1, fname);
	if (!lua_isfunction(L, -1))
	{
		g.offset = 0;
		lua_settop(L, 0);
		return nullptr;
	}
	lua_pushvalue(L, enemy_data_pos);
	//copy lua table with global systems (from registry)
	lua_getfield(L, LUA_REGISTRYINDEX, "system");

	auto L1 = lua_newthread(L);
	lua_stack_guard g1(L1, 3);
	lua_insert(L, lua_gettop(L) - 3); //move thread under args+function
	lua_xmove(L, L1, 3); //transfer everything needed for the fcall
	lua_pop(L, 1);
	return L1;
}
lua_State * e_enemy::yieldable_turn(lua_State * L)
{
	return yieldable_func(L, my_id, "turn");
}

enemy_ref lua_toenemy_ref(lua_State* L, int arg)
{
	lua_getfield(L, arg, "_ref_idx");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	return enemy_ref{ id };
}
enemy_ref lua_check_enemy_ref(lua_State * L, int arg)
{
	if (lua_getmetatable(L, arg)) {  /* does it have a metatable? */
		lua_getfield(L, LUA_REGISTRYINDEX, "enemy.ref");  /* get correct metatable */
		if (lua_rawequal(L, -1, -2)) {  /* does it have the correct mt? */
			lua_pop(L, 2);  /* remove both metatables */
			return lua_toenemy_ref(L, arg);
		}
	}
	luaL_typerror(L, arg, "enemy.ref");  /* else error */
	return enemy_ref();
}
int lua_enemy_ref_tostring(lua_State* L)
{
	auto ref = lua_toenemy_ref(L, 1);
	lua_getfield(L, 1, "name");
	const char* str = lua_tostring(L, -1);
	lua_pushfstring(L, "enemy<%d: %s>", ref, str);
	return 1;
}
void lua_push_card_ref(lua_State * L, const enemy_ref& r)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, r);
}
static int index_to_proto(lua_State* L)
{
	lua_pushstring(L, "_proto");
	lua_rawget(L, 1);
	lua_insert(L, -2);

	lua_gettable(L, -2);

	return 1;
}

void new_lua_enemy(lua_State * L, int proto_id)
{
	lua_newtable(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, proto_id);
	lua_setfield(L, -2, "_proto");

	if (luaL_newmetatable(L, "enemy.ref"))
	{
		lua_pushcfunction(L, lua_enemy_ref_tostring);
		lua_setfield(L, -2, "__tostring");



		//FIXME: actually hide the values inside the table... Prob. should just use the userdata way either way...
		//lua_pushvalue(L, -1);
		//lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, index_to_proto); //TODO: @NOTNICE maybe there is more elegant way?
		lua_setfield(L, -2, "__index");

		//lua_setmetatable(L, -2);
	}
	lua_setmetatable(L, -2);
}

int enemy_registry::new_enemy(const e_enemy & proto, lua_State * L)
{
	new_lua_enemy(L, proto.my_id);
	lua_pushvalue(L, -1);
	int current_id = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, current_id);
	lua_setfield(L, -2, "_ref_idx");
	lua_pop(L, 1);

	auto& new_enemy = enemies[current_id];
	new_enemy = proto;
	new_enemy.my_id = current_id;

	return current_id;
}

void enemy_registry::unreg_enemy(int id, lua_State * L)
{
	luaL_unref(L, LUA_REGISTRYINDEX, enemies[id].my_id);
	enemies.erase(id);
}
