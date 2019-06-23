//TODO: @refactor and stuff. lua_booster eventually will have: map(s?) or map pieces, cards (done?), enemies (and their dropped cards?). Maybe even new classes/races.
#include "card.hpp"
#include "enemy.hpp"
#include <unordered_map>
struct lua_booster
{
	std::unordered_map<std::string, card> cards;
	std::unordered_map<std::string, e_enemy> enemies;
}; 

void lua_load_booster(lua_State* L,int arg,lua_booster& output);