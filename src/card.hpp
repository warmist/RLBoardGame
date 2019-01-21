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
struct card;
struct game_systems;
using card_action = void(*)(card&, game_systems&, card_needs_output*);
struct console;
constexpr float phi = 1.61803398874989484820f;
struct card
{
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
	card_action use_callback = nullptr;
	//POST_USE
	card_fate after_use = card_fate::destroy;


	bool is_warn_ap = false;
	sf::Clock warn_ap;

	v3f get_ap_color();
	void render(console& c, int x, int y);
};