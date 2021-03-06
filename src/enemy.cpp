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



int lua_enemy_ref_tostring(lua_State* L)
{
	auto ref = luaL_check_enemy(L, 1);
	lua_getfield(L, 1, "name");
	const char* str = lua_tostring(L, -1);
	lua_pushfstring(L, "enemy<%p : %s>", ref, str);
	return 1;
}
static int index_to_proto(lua_State* L)
{
	lua_pushstring(L, "_proto");
	lua_rawget(L, 1);
	lua_insert(L, -2);

	lua_gettable(L, -2);

	return 1;
}
int lua_entity_pos(lua_State* L);

void lua_set_mt_proto_enemy(lua_State* L, int arg)
{
	luaL_checktype(L, arg, LUA_TTABLE);
	if (luaL_newmetatable(L, "enemy.proto"))
	{
		//constructed by sys init due to ... reasons
		lua_getfield(L, LUA_REGISTRYINDEX, "_enemy_moves");
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			lua_pushvalue(L, -2);
			lua_insert(L, -2);
			lua_settable(L, -5);
		}
		lua_pop(L, 1);

		lua_pushcfunction(L, lua_entity_pos);
		lua_setfield(L, -2, "pos");

		
		lua_getfield(L, LUA_REGISTRYINDEX, "system");
		lua_getfield(L, -1, "damage");
		lua_setfield(L, -3, "damage");
		lua_pop(L, 1);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, arg);
}

void lua_read_tile(lua_State * L, int arg, tile_attr& output)
{
	lua_pushnumber(L, 1);
	lua_gettable(L, arg);

	if (lua_isstring(L, -1))
	{
		size_t len;
		auto s = lua_tolstring(L, -1, &len);
		if(len>0)
			output.glyph = s[0];
		else
		{
			luaL_error(L, "Invalid glyph: zero sized string");
		}
		lua_pop(L, 1);
	}
	else if (lua_isnumber(L, -1))
	{
		auto n = lua_tonumber(L, -1);
		output.glyph = (int)n;
		lua_pop(L, 1);
	}
	else
	{
		luaL_error(L, "Invalid glyph: not a number or string");
	}

	lua_pushnumber(L, 2);
	lua_gettable(L, arg);
	if (lua_isnumber(L, -1))
	{
		float r = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushnumber(L, 3);
		lua_gettable(L, arg);
		float g = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushnumber(L, 4);
		lua_gettable(L, arg);
		float b = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		output.color_fore = v3f(r, g, b);
	}
	else
	{
		output.color_fore = v3f(1, 1, 1);
		output.color_back = v3f(0, 0, 0);
		return;
	}

	lua_pushnumber(L, 5);
	lua_gettable(L, arg);
	if (lua_isnumber(L, -1))
	{
		float r = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushnumber(L, 6);
		lua_gettable(L, arg);
		float g = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushnumber(L, 7);
		lua_gettable(L, arg);
		float b = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		output.color_back = v3f(r, g, b);
	}
	else
	{
		output.color_back = v3f(0, 0, 0);
	}
}

void lua_load_enemy(lua_State* L, int arg, e_enemy& output)
{
	lua_stack_guard g(L);
	lua_pushvalue(L, arg);
	int p = lua_gettop(L);

	output.type = entity_type::enemy;

	lua_pushvalue(L, p);
	output.my_id = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_getfield(L, p, "name");
	output.name = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, p, "img");
	lua_read_tile(L, lua_gettop(L), output);
	lua_pop(L, 1);

	lua_getfield(L, p, "hp");
	output.max_hp=(int)lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_set_mt_proto_enemy(L, p);
	lua_pop(L, 2);//pop copy of enemy
}
void new_lua_enemy(lua_State * L, int proto_id, e_enemy* ptr)
{
	/*
		enemy structure from lua:
			* userdata ptr
			* lua table for writing (e.g. self.current_anger)
			* reference to enemy proto if index fails in local table (i.e. class data)
	*/
	lua_newtable(L); //this will be my data table

	//lua_rawgeti(L, LUA_REGISTRYINDEX, proto_id);
	//lua_setfield(L, -2, "_proto");

	lua_pushstring(L, "_ptr");
	lua_pushlightuserdata(L, ptr);
	lua_rawset(L, -3);

	if (luaL_newmetatable(L, "entity.enemy"))
	{
		lua_pushcfunction(L, lua_enemy_ref_tostring);
		lua_setfield(L, -2, "__tostring");

		//FIXME: actually hide the values inside the table... Prob. should just use the userdata way either way...
		//lua_pushvalue(L, -1);
		//lua_setfield(L, -2, "__index");

		//lua_pushcfunction(L, index_to_proto); //TODO: @NOTNICE maybe there is more elegant way?
		lua_rawgeti(L, LUA_REGISTRYINDEX, proto_id);
		lua_setfield(L, -2, "__index");

		//lua_setmetatable(L, -2);
	}
	lua_setmetatable(L, -2);
}
e_enemy* luaL_check_enemy(lua_State*L, int arg) { 
	if (!lua_istable(L, arg))
		return nullptr;

	lua_pushstring(L, "_ptr");
	lua_rawget(L, arg);
	if (lua_isnil(L, -1))
		return nullptr;

	auto ptr = lua_touserdata(L, -1);
	if (ptr == nullptr)
		return nullptr;

	return static_cast<e_enemy*>(ptr); 
}
void lua_push_enemy(lua_State*L, e_enemy* ptr)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->my_id);
}
e_enemy* enemy_registry::new_enemy(const e_enemy & proto, lua_State * L,map& m)
{
	auto new_enemy = new e_enemy;
	new_lua_enemy(L, proto.my_id, new_enemy);
	
	lua_pushvalue(L, -1);
	int current_id = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, current_id);
	lua_setfield(L, -2, "_ref_idx");
	lua_pop(L, 1);

	
	enemies[current_id] = new_enemy;
	
	*new_enemy = proto;
	new_enemy->my_id = current_id;
	m.entities.emplace_back(new_enemy);

	return new_enemy;
}

void enemy_registry::unreg_enemy(int id, lua_State * L)
{
	luaL_unref(L, LUA_REGISTRYINDEX, enemies[id]->my_id);
	enemies.erase(id);
}
