#pragma once
#include "map.hpp"
#include "SFML\System.hpp"
enum class card_type
{
	action,
	generated,
	wound,
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
struct console;
struct lua_State;
constexpr float phi = 1.61803398874989484820f;
struct card
{
	static const int card_w = 15;
	static const int card_h = int(card_w*phi);

	std::string key;
	std::string name;
	std::string desc;
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
	//USE
	//actual function is in lua
	lua_State* yieldable_use(lua_State* L);
	//POST_USE
	card_fate after_use = card_fate::discard;


	bool is_warn_ap = false;
	sf::Clock warn_ap;

	v3f get_ap_color();
	bool selected = false;
	void render(console& c, int x, int y);
};

#include <unordered_map>
using lua_booster = std::unordered_map<std::string, card>;

void lua_load_booster(lua_State* L,int arg,lua_booster& output);
//TODO: ugh ugly ref stuff. Should probably make cards pointers and then moving them would be easier
struct card_ref
{
	std::vector<card>* vec;
	int id;
};
void lua_push_card_ref(lua_State* L, const card_ref& r);
card_ref lua_tocard_ref(lua_State* L, int arg);