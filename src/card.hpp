#pragma once
#include "map.hpp"
#include "SFML\System.hpp"
enum class card_type
{
	action,
	generated,
	wound,
};
enum class card_state_change
{
	destroy, //as in - no longer in game
	discard, //put in the discard pile
	hand, //move to hand
	draw_pile_top, //top of the draw pile
	draw_pile_rand, //somewhere in the pile
	draw_pile_bottom,
};
enum class card_state
{
	proto, //not created, but a prototype/class of a card
	destroyed,
	discard,
	hand,
	draw_pile,
	//reward,
};
struct card_needs_output
{
	entity* visible_target_unit;
	std::vector<v2i> walkable_path;
};
struct console;
struct lua_State;
constexpr float phi = 1.61803398874989484820f;

using card_ref = int;

struct card
{
	static const int card_w = 15;
	static const int card_h = int(card_w*phi);
	card_ref my_id = -1;

	card_state current_state=card_state::proto;

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
	lua_State* yieldable_turn_end(lua_State* L);

	bool is_warn_ap = false;
	sf::Clock warn_ap;

	v3f get_ap_color();
	bool selected = false;
	void render(console& c, int x, int y);
};

//TODO: @REFACTOR - merge with lua_booster? add new_card(string name)
struct card_registry
{
	std::unordered_map<int, card> cards;

	int new_card(const card& proto,lua_State* L);
	void unreg_card(int id, lua_State* L);
};
template <card_state S> 
struct card_vector
{
	card_registry* r;
	std::vector<int> ids;

	int size() { return (int)ids.size(); }

	card& operator[](int id) { return r->cards[ids[id]]; }
	const card& operator[](int id) const { return r->cards[ids[id]]; }

	int count_wounds()
	{
		int ret = 0;
		for (auto id : ids)
		{
			if (r->cards[id].type == card_type::wound)
				ret++;
		}
		return ret;
	}

	void push_back(int id)
	{
		ids.push_back(id);
		r->cards[id].current_state = S;
	}
	void push_top(int id)
	{
		ids.insert(ids.begin(), id);
		r->cards[id].current_state = S;
	}
	template<typename rand>
	void push_rand(int id, rand& rnd)
	{
		std::uniform_int_distribution<> dis(0, (int)ids.size());
		ids.insert(ids.begin() + dis(rnd), id);
		r->cards[id].current_state = S;
	}
	template<card_state S2>
	void append(card_vector<S2>& other)
	{
		for (auto id : other.ids)
		{
			r->cards[id].current_state = S;
		}
		ids.insert(ids.end(), other.ids.begin(), other.ids.end());
		other.ids.clear();
	}
	void remove_card(int id)
	{
		auto it=std::find(ids.begin(), ids.end(), id);
		ids.erase(it);
	}
};

//TODO: @refactor and stuff. lua_booster eventually will have: map(s?) or map pieces, cards (done?), enemies (and their dropped cards?). Maybe even new classes/races.
#include <unordered_map>
using lua_booster = std::unordered_map<std::string, card>;

void lua_load_booster(lua_State* L,int arg,lua_booster& output);

void lua_push_card_ref(lua_State* L, const card_ref& r);
card_ref lua_tocard_ref(lua_State* L, int arg);
card_ref lua_check_card_ref(lua_State* L, int arg);

struct card_change_ref
{
	card_ref id;
	card_state_change new_state;
};

