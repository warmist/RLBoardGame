#pragma once

#include "map.hpp"
//#include "lua_helper.hpp"
using enemy_ref = int;
struct lua_State;
struct e_enemy :public entity
{
	int current_hp = 3;
	int max_hp = 3;

	enemy_ref my_id = -1;

	lua_State* yieldable_turn(lua_State* L);
};

struct enemy_turn_data
{
	std::vector<e_enemy*> enemies;
	int current_enemy_turn = 0;
};

struct enemy_registry
{
	std::unordered_map<int, e_enemy> enemies;

	int new_enemy(const e_enemy& proto, lua_State* L);
	void unreg_enemy(int id, lua_State* L);
};