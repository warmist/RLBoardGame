#pragma once
#include "map.hpp"
#include "SFML\System.hpp"
enum class card_type
{
	action,
	generated,
	wound,
};
enum class card_needs
{
	nothing,
	visible_target_unit,
	walkable_path,
	//more?
};
enum class card_fate
{
	destroy, //as in - no longer in game
	discard, //put in the discard pile
	hand, //move to hand
	draw_pile_top, //top of the draw pile
	draw_pile_rand, //somewhere in the pile
	draw_pile_bottom,
	//etc...
};

struct card_needs_output
{
	entity* visible_target_unit;
	std::vector<v2i> walkable_path;
};
struct card_needs_input
{
	card_needs type = card_needs::nothing;
	float distance;
};
struct console;
struct lua_State;
struct card
{
	std::string key;
	std::string name;
	std::string desc;
	static const int card_w = 15;
	static const int card_h = int(card_w*phi);

	/*
	ART and stuff -> type
	pre-use conditions -> cost, prereq_function (with somehow indicating failure?)
	* selecting target ?
	use -> callback with somehow animating/changing world
	post-use action-> card destroy/move etc...

	NOT handled:
	* passives
	* special triggers (e.g. when you are hit)
	*/
	//UI and other
	card_type type = card_type::action;
	//PRE_USE
	int cost_ap = 0;
	card_needs_input needs;
	//USE
	int lua_func_use_ref = -2;//LUA_NOREF
	void use(lua_State* L, card_needs_output* out);
	//POST_USE
	card_fate after_use = card_fate::destroy;


	bool is_warn_ap = false;
	sf::Clock warn_ap;

	v3f get_ap_color();
	void render(console& c, int x, int y);
};

#include <unordered_map>
using lua_booster = std::unordered_map<std::string, card>;

void lua_load_booster(lua_State* L,int arg,lua_booster& output);