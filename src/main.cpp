
#include "common.hpp"
#include "console.hpp"
#include "lua_helper.hpp"

#include "map.hpp"
#include <unordered_set>
#include <functional>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
//assets
#include "asset_cp437_12x12.hpp"
#include "asset_paul_10x10.hpp"

#include <array>

using std::string;
string asset_path;
//900/12=85
const int map_w = 200;
const int map_h = 200;
const int view_w = 80;
const int view_h = 80;

std::uniform_int_distribution<int> r_color(0, 255);
std::mt19937_64 global_rand; //used for non important things
struct state
{
	string name;
	string default_next;
};
/*
	default states:
		start->round_start->
			for each force
				turn start->
				turn end->
			round_end->round_start

	special, no normal way to go there:
		win
		lose
*/
struct state_map
{
	std::unordered_map<string, state> data;

	void add_state(const state& s)
	{
		data[s.name] = s;
	}
	void init_default_states()
	{
		add_state(state{ "game_start","round_start" });
		add_state(state{ "round_start","round_end" });
		add_state(state{ "round_end","round_start" });
		add_state(state{ "win","" });
		add_state(state{ "lose","" });
	}
	state next_state(const state& cur_state)
	{
		auto n = cur_state.default_next;
		if (data.count(n))
		{
			return data[n];
		}
		else
		{
			//TODO: other way to handle this?
			return data["lose"];
		}
	}
	void insert_state(const state& new_state, const string& after)
	{
		if (data.count(after) == 0)
		{
			//TODO: how to handle this?
			return;
		}
		
		auto nn = data[after].default_next;
		data[after].default_next = new_state.name;
		state tmp_state = new_state;
		tmp_state.default_next = nn;
		add_state(tmp_state);
	}
};
char to_hex_char(int c)
{
	if (c < 10)
		return '0' + c;
	else
		return 'A' + c-10;
}
v2i get_mouse(console& con)
{
	auto b = con.get_glypht_size();
	auto p = sf::Mouse::getPosition(con.get_window());
	p.x /= b.x;
	p.y /= b.y;

	//FIXME: ignore input from OUTSIDE of window. Not sure how to do that though
	if (p.x < 0)p.x = 0;
	if (p.x >= view_w)p.x = view_w - 1;
	if (p.y < 0)p.y = 0;
	if (p.y >= view_h)p.y = view_h - 1;

	return v2i(p.x,p.y);
}
bool get_mouse_left()
{
	return sf::Mouse::isButtonPressed(sf::Mouse::Left);
}
bool get_mouse_right()
{
	return sf::Mouse::isButtonPressed(sf::Mouse::Right);
}
void draw_asciimap(console& con)
{
    for(int i=0;i<16;i++)
        for (int j = 0; j < 16; j++)
        {
            con.set_char(v2i(i, j), (i + j * 16));
        }
	auto p = get_mouse(con);
	int id = p.x + p.y * 16;
	char buf[3] = { 0 };
	buf[0] = to_hex_char((id & 0xF0)>>4);
	buf[1] = to_hex_char(id & 0xF);
	std::string ps = "Mouse:" + std::to_string(p.x) + ", " + std::to_string(p.y) + " ->" + std::to_string(id)+"("+ buf+")";
	con.set_text(v2i(0, 17), ps);
}
void set_tile(console& c, int x, int y, tile_attr t,bool is_red)
{
	c.set_char(v2i(x, y), t.glyph, is_red?v3f(1.0f,0,0):t.color_fore, t.color_back);
}

void load_map(map& m)
{
	auto f = fopen("map1.mp", "rb");
	m.load(f);
	fclose(f);
}
void save_map(map& m)
{
	auto f = fopen("map1.mp", "wb");
	m.save(f);
	fclose(f);
}
void dbg_init_world(map& m)
{
	tile_attr wall = { v3f(0,0,0),v3f(1,1,1),'#',tile_flags::block_move | tile_flags::block_sight };
	tile_attr floor = { v3f(0,0,0),v3f(0.2f,0.2f,0.2f),'+',tile_flags::none };

	const int r = map_w / 5;
	int mw = map_w / 2;
	int mh = map_h / 2;
	for (int x = 0; x < r; x++)
	{
		for (int y = 0; y < r; y++)
		{
			int len = x*x + y*y;
			if (len > r*r)
				continue;

			if(len>=(r-2)*(r-1))
			{
				m.static_layer(mw + x, mh + y) = wall;
				m.static_layer(mw - x, mh + y) = wall;
				m.static_layer(mw + x, mh - y) = wall;
				m.static_layer(mw - x, mh - y) = wall;
			}
			else
			{
				m.static_layer(mw + x, mh + y) = floor;
				m.static_layer(mw - x, mh + y) = floor;
				m.static_layer(mw + x, mh - y) = floor;
				m.static_layer(mw - x, mh - y) = floor;
			}
		}
	}
	for(int i=-3;i<=3;i++)
		m.static_layer(mw + 1, mh + i) = wall;
}
enum class gui_state
{
	player_turn,
	selecting_path,
	selecting_enemy,
	exiting,
	animating,
	enemy_turn,
	lost,
};
enum class animation_type
{
	unit_walk,
	gui_flash,
};
const float animation_max_time = 0.5f;

struct anim
{
	//animation_type type;
	int step;
	sf::Clock anim_clock;
	animation_type anim_type;

	virtual ~anim() {}
	virtual int max_steps() = 0;
	virtual void do_step() = 0;

	void start_animation()
	{
		step = 0;
		anim_clock.restart();
	}
	
	float time_stamp_next_step()
	{
		float m = animation_max_time/max_steps();
		return step*m;
	}
	bool done_animation()
	{
		return step == max_steps();
	}
	void animate_step()
	{
		float cur_time = anim_clock.getElapsedTime().asSeconds();
		if (cur_time < time_stamp_next_step())
			return;
		if (step < max_steps())
		{
			do_step();
			step++;
		}
	}
};
using anim_ptr = std::unique_ptr<anim>;
using anim_vec = std::vector<anim_ptr>;

struct anim_unit_walk :public anim
{
	std::vector<v2i> path;
	entity* walker;
	int max_steps() override
	{
		return (int)path.size();
	}
	void do_step() override
	{
		const auto& p = path[step];

		walker->pos = p;
	}
};

#include "card.hpp"

struct card_deck
{
	std::vector<card> cards;
	int x, y;
	std::string text;
	bool is_selected = false;
	recti get_bounds()
	{
		const int w = card::card_w;
		const int h = card::card_h;
		recti my_rect(x, y, w, h);
		if (is_selected)
		{
			if (my_rect.pos.x < 0)
				my_rect.pos.x = 0;
			else
			{
				my_rect.pos.x = view_w - w;
			}
		}
		return my_rect;
	}
	void input(console& c)
	{
		auto m = get_mouse(c);

		auto my_rect = get_bounds();
		if (my_rect.is_inside(m))
			is_selected = true;
		else
			is_selected = false;
	}
	int count_wounds()
	{
		int ret = 0;
		for (auto& c : cards)
		{
			if (c.type == card_type::wound)
				ret++;
		}
		return ret;
	}
	void render(console& c)
	{
		v3f border_color = v3f(1, 1, 1);
		auto my_rect = get_bounds();
		int x = my_rect.x();
		int y = my_rect.y();
		int w = my_rect.width();
		int h = my_rect.height();
		c.draw_box(recti(x, y, w, h), true, border_color);

		const int text_start = int(w / 2 - text.length() / 2);
		
		c.set_text(v2i(x + text_start, y), text);
		c.set_char(v2i(x + text_start + int(text.length()), y), tile_nse_t_double, border_color);
		c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color);

		int wounds = count_wounds();

		const int buffer_size = 12;
		char buffer[buffer_size];

		const v3f wound_color=  { 0.76f, 0.04f, 0.01f };
		const int wound_y = y+card::card_h - 2;
		snprintf(buffer, buffer_size, "Wounds :%3d", (wounds>999) ? (999) : (wounds));

		c.set_text(v2i(x + 2, wound_y), buffer, wound_color);
		c.set_char(v2i(x + 2 + buffer_size - 1, wound_y), tile_ws_corner_double, wound_color);
		c.set_char(v2i(x + 2 - 1, wound_y), tile_es_corner_double, wound_color);

		const int count_y = y+card::card_h-1;
		snprintf(buffer, buffer_size, "Count  :%3d", (cards.size()>999)?(999):((int)cards.size()));
		c.set_text(v2i(x + 2, count_y), buffer);
		c.set_char(v2i(x + 2 + buffer_size-1, count_y), tile_nse_t_double, border_color);
		c.set_char(v2i(x + 2 - 1, count_y), tile_nsw_t_double, border_color);
	}
};
struct card_hand
{
	std::vector<card> cards;
	int selected_card=-1;
	int hand_gui_y;
	v2i get_card_extents(int y,int card_no)
	{
		bool is_crouded = cards.size()*card::card_w > view_w;
		if (is_crouded)
		{
			int card_size = (view_w) / int(cards.size()+1);
			return v2i(card_size*card_no, y);
		}
		else
		{
			int skip_space = (view_w - int(cards.size())*card::card_w) / 2;
			return v2i(card::card_w*card_no + skip_space, y);
		}
	}
	void input(console& c)
	{
		if (cards.size() == 0)
			return;
		//selection logic:
		//if we have selected card, deselect it if we go out of bounds
		auto m = get_mouse(c);
		if (selected_card >= 0)
		{
			auto p = get_card_extents(view_h - card::card_h, selected_card);//selected card is higher
			if (m.x < p.x || m.y < p.y || m.x >= p.x + card::card_w || m.y >= p.y + card::card_h)
				selected_card = -1;
		}
		//then if we are over another card, select it
		if (selected_card == -1)
		{
			for (int i = 0; i < cards.size(); i++)
			{
				auto p = get_card_extents(hand_gui_y, i);
				if (m.x >= p.x && m.y >= p.y && m.x < p.x + card::card_w && m.y < p.y + card::card_h)
				{
					selected_card = i;
					break;
				}
			}
		}
	}
	void render(console& c)
	{
		//two ways to lay the cards out:
		//if enough space dont overlap them and just put side by side from center
		//if not enough space, start overlapping them
		//still somehow need to ensure that there is not more than view_w cards as then you could not select a card :|
		for (int i = int(cards.size()) - 1; i >= 0; i--)
		{
			auto p = get_card_extents(hand_gui_y, i);
			if (i == selected_card)
			{
				//skip this card as we are going to draw it last
			}
			else
				cards[i].render(c, p.x, p.y);
		}
		if (selected_card >= 0 && selected_card < cards.size())
		{
			auto p = get_card_extents(hand_gui_y, selected_card);
			cards[selected_card].render(c, p.x, view_h - card::card_h);
		}
	}
};
card wound_card()
{
	card t_card;
	t_card.name = "Wound";
	t_card.desc = "Loose game if 3 are in hand";
	t_card.type = card_type::wound;
	return t_card;
}
//player receives damage
struct anim_player_damage :public anim
{
	entity* player;
	card_hand* player_wound_hand;
	int damage_to_do;

	static const int tween_frames = 10;

	v3f original_color;
	int max_steps() override
	{
		return (int)damage_to_do*tween_frames;
	}
	v3f tween_func(int s)
	{
		float sf = (float)s / (float)tween_frames;
		float f = 1 - sf*sf*sf*sf;
		return v3f(1, 0, 0)*f + original_color*(1 - f);
	}
	void do_step() override
	{
		if (step == 0)
		{
			original_color = player->color_fore;
		}
		//blink red for now
		int tween_step = step % tween_frames;
		player->color_fore = tween_func(tween_step);
		if (step == max_steps() - 1)
		{
			player->color_fore = original_color;
		}
		if (tween_step == tween_frames - 1)
		{
			player_wound_hand->cards.push_back(wound_card());
		}
	}
};
class e_player :public entity
{
public:
	int actions_per_turn= 2;
	int current_ap = 2;
	void render_gui(console& c, int x, int y)
	{
		//TODO: ugly, boring, not easy to see/use etc...
		c.set_text(v2i(x, y), "Actions:");

		int dx = 0;
		for (int i = 0; i < actions_per_turn- current_ap; i++)
			c.set_char(v2i(x + 10 + (dx++), y ), (unsigned char)'\xad', v3f(0.4f, 0.4f, 0.4f));
		for (int i = 0; i < current_ap; i++)
			c.set_char(v2i(x + 10 + (dx++), y ), (unsigned char)'\xad', v3f(1, 1, 1));

		//c.set_text(v2i(x + 10, y+1), std::to_string(current_action) + "/" + std::to_string(actions_per_turn));
	}
};
struct e_enemy :public entity
{
	int current_hp = 3;
	int max_hp = 3;

	int move = 3;
	int dmg = 1;

	//simulation stuff
	bool done_move = false;
	bool done_dmg = false;
};

entity* luaL_check_entity(lua_State*L, int arg) { 
	
	if (auto ret = luaL_testudata(L, arg, "entity"))
	{
		return *static_cast<entity**>(ret);
	}
	else if (auto ret = luaL_testudata(L, arg, "entity.player"))
	{
		return *static_cast<e_player**>(ret);
	}
	else if (auto ret = luaL_testudata(L, arg, "entity.enemy"))
	{
		return *static_cast<e_enemy**>(ret);
	}
	luaL_typerror(L, arg, "entity.*");
	return nullptr;
}
e_player* luaL_check_player(lua_State*L ,int arg){return *static_cast<e_player**>(luaL_checkudata(L, arg, "entity.player"));}
e_enemy* luaL_check_enemy(lua_State*L, int arg) { return *static_cast<e_enemy**>(luaL_checkudata(L, arg, "entity.enemy")); }
int lua_entity_pos(lua_State*L)
{
	auto p = luaL_check_entity(L, 1);
	lua_push_v2i(L, p->pos);
	return 1;
}
void lua_push_player(lua_State*L, e_player* ptr)
{
	auto np = lua_newuserdata(L, sizeof(ptr));
	*reinterpret_cast< e_player**>(np) = ptr;
	if (luaL_newmetatable(L, "entity.player"))
	{
		lua_pushcfunction(L, lua_entity_pos);
		lua_setfield(L, -2, "pos");
		
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
}
void lua_push_enemy(lua_State*L, e_enemy* ptr)
{
	auto np = lua_newuserdata(L, sizeof(ptr));
	*reinterpret_cast< e_enemy**>(np) = ptr;
	if (luaL_newmetatable(L, "entity.enemy"))
	{
		lua_pushcfunction(L, lua_entity_pos);
		lua_setfield(L, -2, "pos");

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
}

//enemy receives damage
struct anim_enemy_damage :public anim
{
	e_enemy* enemy;
	int damage_to_do;
	static const int tween_frames = 10;

	v3f original_color;
	int max_steps() override
	{
		return (int)damage_to_do*tween_frames;
	}
	v3f tween_func(int s)
	{
		float sf = (float)s / (float)tween_frames;
		float f = 1 - sf*sf*sf*sf;
		return v3f(1, 0, 0)*f + original_color*(1 - f);
	}
	void do_step() override
	{
		if (step == 0)
		{
			original_color = enemy->color_fore;
		}
		//blink red for now
		int tween_step = step % tween_frames;
		enemy->color_fore = tween_func(tween_step);
		if (step == max_steps() - 1)
		{
			enemy->color_fore = original_color;
		}
		if (tween_step == tween_frames - 1)
		{
			enemy->current_hp--;
			if (enemy->current_hp <= 0)
				enemy->removed = true;
		}
	}
};
struct enemy_turn_data
{
	std::vector<e_enemy*> enemies;
	int current_enemy_turn=0;
	bool current_enemy_changed = false;

	anim_ptr current_animation=nullptr;
};

struct game_systems
{
	std::mt19937 rand;
	bool restart = false;

	e_player* player;
	map* map;
	gui_state gui_state = gui_state::player_turn;
	//state specific stuff
	anim_ptr animation;
	std::unique_ptr<enemy_turn_data> e_turn;
	std::vector<e_enemy*> enemy_choices;
	//
	card_hand* hand;

	card_deck* deck;
	card_deck* discard;

	bool first_lua_target_function;
	//lua stuff
	lua_State* L = nullptr;
	lua_State* yielded_L = nullptr;
	int coroutine_ref = LUA_NOREF;
	lua_booster possible_cards;
};
void act_move(card& c, game_systems& g, card_needs_output* nd)
{
	auto anim= std::make_unique<anim_unit_walk>();
	anim->path = nd->walkable_path;
	std::reverse(anim->path.begin(), anim->path.end());
	anim->walker = g.player;
	g.animation = std::move(anim);
	g.animation->start_animation();
	g.gui_state = gui_state::animating;
}
void done_yielded_lua(game_systems& sys);
void finish_card_use(game_systems& sys, bool soft_cancel);
void cancel_card_use(game_systems& sys);
void resume_card_use(game_systems& sys, int args_pushed = 0)
{
	if (!sys.yielded_L)
	{
		//not got here with animation not from lua?
		done_yielded_lua(sys);
		return;
	}
	print_stack(sys.yielded_L);
	auto result = lua_resume(sys.yielded_L, args_pushed);
	if (result == 0)
	{
		//all done running state from lua return to normal working
		done_yielded_lua(sys);
		finish_card_use(sys, false);
		return;
	}
	else if (result != LUA_YIELD)
	{
		printf("Error:%s\n", lua_tostring(sys.yielded_L, -1));
		assert(false);
	}
}
void use_card_actual(card& card, game_systems& g)
{
	auto& hand = *g.hand;

	g.first_lua_target_function = true; //reset "first target func" condition
	g.yielded_L = card.yieldable_use(g.L);
	g.coroutine_ref=luaL_ref(g.L, LUA_REGISTRYINDEX);

	resume_card_use(g,2);
}
void fill_enemy_choices(v2i center, float distance,map& m, std::vector<e_enemy*>& enemies)
{
	enemies.clear();
	float dist_sqr = distance*distance;
	for (auto& e : m.entities)
	{
		if (e->type != entity_type::enemy)
			continue;
		auto delta = e->pos - center;
		float v = delta.dotf(delta);
		if (v <= dist_sqr)
		{
			enemies.push_back(static_cast<e_enemy*>(e.get()));
		}
	}
}
void handle_card_use(bool click,game_systems& g)
{
	auto& hand = *g.hand;
	if (click && hand.selected_card != -1)
	{
		auto& card = hand.cards[hand.selected_card];
		if (card.cost_ap > g.player->current_ap)
		{
			card.is_warn_ap = true;
			card.warn_ap.restart();
			return;
		}
		use_card_actual(card, g);
	}
}
void strike_action(card&,game_systems& g, card_needs_output* needs)
{
	auto anim = std::make_unique<anim_enemy_damage>();
	anim->enemy = static_cast<e_enemy*>(needs->visible_target_unit);
	anim->damage_to_do = 3;

	g.animation = std::move(anim);
	g.animation->start_animation();
	g.gui_state = gui_state::animating;
}
void push_action(card&, game_systems& g, card_needs_output* needs)
{
	const int move_dist = 3;
	auto anim = std::make_unique<anim_unit_walk>();
	auto enemy = static_cast<e_enemy*>(needs->visible_target_unit);
	anim->walker = enemy;
	auto& p = anim->path;
	auto dir = enemy->pos - g.player->pos;
	auto cpos = enemy->pos;
	for (int i = 0; i < move_dist; i++)
	{
		cpos += dir;
		if (!g.map->is_passible(cpos.x, cpos.y))
			break;
		p.push_back(cpos);
	}
	g.animation = std::move(anim);
	g.animation->start_animation();
	g.gui_state = gui_state::animating;
}
template <typename V>
void pop_front(V & v)
{
	v.erase(v.begin());
}
bool draw_card(game_systems& sys)
{
	auto& deck = sys.deck->cards;
	auto& hand = sys.hand->cards;
	auto& discard = sys.discard->cards;
	if (deck.size() == 0)
	{
		if (discard.size() == 0)
			return false;
		else
		{
			std::shuffle(discard.begin(), discard.end(),sys.rand);
			deck.insert(deck.end(), discard.begin(), discard.end());
			discard.clear();
		}
	}
	auto card = deck.front();
	pop_front(deck);
	hand.push_back(card);
	return true;
}
void remove_dead_enemies(game_systems& sys)
{
	sys.map->compact_entities();
}
void end_enemy_turn(game_systems& sys)
{
	remove_dead_enemies(sys);

	auto& hand = sys.hand->cards;
	auto& discard = sys.discard->cards;
	//mix in the wounds
	discard.insert(discard.end(), hand.begin(), hand.end());
	hand.clear();
	//draw new cards
	const int hand_draw_count = 5;
	const int hand_max_count = 7;
	sys.gui_state = gui_state::player_turn;
	sys.player->current_ap = sys.player->actions_per_turn;

	int draw_count = std::max(std::min(int(hand_max_count- hand.size()),hand_draw_count), 0);
	for (int i = 0; i < draw_count; i++)
	{
		draw_card(sys);
	}
	//add generated cards

	hand.push_back(sys.possible_cards["move"]);
}
void end_player_turn(game_systems& sys)
{
	sys.gui_state = gui_state::enemy_turn;
	auto& dis = sys.discard->cards;
	auto& hand = sys.hand->cards;
	int count_wound = 0;
	for (auto& c : hand)
	{
		if (c.type == card_type::wound)
			count_wound++;
	}
	if (count_wound >= 3)
	{
		sys.gui_state = gui_state::lost;
	}
	dis.insert(dis.end(), hand.begin(), hand.end());
	hand.clear();
	remove_dead_enemies(sys);
}
void handle_player_turn(console& con,game_systems& sys)
{
	sys.hand->input(con);
	sys.deck->input(con);
	sys.discard->input(con);
	handle_card_use(get_mouse_left(), sys);
	sys.deck->render(con);
	sys.discard->render(con);
	sys.hand->render(con);
}
void handle_exiting(console& con, game_systems& sys)
{
	int exit_h = view_h / 2;
	con.set_text_centered(v2i(view_w / 2, exit_h), "Are you sure you want to exit?", v3f(0.6f, 0, 0));
	con.set_text_centered(v2i(view_w / 2, exit_h + 1), "(press esc to confirm, anything else - cancel)");
	if (get_mouse_right())
	{
		sys.gui_state = gui_state::player_turn;
	}
}
void handle_lost(console& con, game_systems& sys)
{
	int exit_h = view_h / 2;
	con.set_text_centered(v2i(view_w / 2, exit_h), "You have died!", v3f(0.6f, 0, 0));
	con.set_text_centered(v2i(view_w / 2, exit_h + 1), "(press esc to exit, anything else - restart)");
	if (get_mouse_right())
	{
		sys.restart = true;
	}
}
void handle_selecting_path(console& con, game_systems& sys)
{
	auto& hand = *sys.hand;
	auto id = hand.selected_card;
	if (id == -1 || id >= hand.cards.size())
	{
		assert(false);
	}
	auto& card = hand.cards[id];
	//render range highlight
	sys.map->render_reachable(con, v3f(0.1f, 0.2f, 0.5f));
	//render mouse/path to target
	v2i mouse_pos = get_mouse(con);
	auto path = sys.map->get_path(mouse_pos + sys.map->view_pos);
	v3f color_fail = v3f(0.8f, 0.2f, 0.4f);
	sys.map->render_path(con, path, v3f(0.3f, 0.7f, 0.2f));

	if (path.size() == 0)
		con.set_back(mouse_pos, color_fail);
	//render selected card

	card.render(con, view_w / 2 - card::card_w / 2, view_h - card::card_h);

	if (get_mouse_left())
	{
		if (path.size() != 0)
		{
			std::reverse(path.begin(), path.end());
			lua_push_path(sys.yielded_L, path);
			sys.first_lua_target_function = false;
			resume_card_use(sys,1);
			return;
		}
	}
	if (get_mouse_right())
	{
		cancel_card_use(sys);
		sys.gui_state = gui_state::player_turn;
	}
}
void handle_selecting_enemy(console& con, game_systems& sys)
{
	auto& hand = *sys.hand;
	auto id = hand.selected_card;
	if (id == -1 || id >= hand.cards.size())
	{
		assert(false);
	}
	auto& card = hand.cards[id];
	auto& choices = sys.enemy_choices;
	//highlight possible choices
	const v3f color_choice = v3f(0.1f, 0.2f, 0.5f);
	const v3f color_fail = v3f(0.8f, 0.2f, 0.4f);
	const v3f color_ok = v3f(0.3f, 0.7f, 0.2f);

	v2i mouse_pos = get_mouse(con);

	auto view_pos  = sys.map->view_pos;
	auto view_rect = sys.map->view_rect;
	int selected_id = -1;

	for(int i=0;i<(int)choices.size();i++)
	{
		auto& e = choices[i];
		auto p=e->pos - view_pos;
		if (view_rect.is_inside(view_pos))
		{
			v3f color=color_choice;
			if(p==mouse_pos) //highlight (and select) current target
			{
				color = color_ok;
				selected_id = i;
			}
			con.set_back(p, color);
		}
	}
	if (selected_id == -1)
	{
		con.set_back(mouse_pos, color_fail);
	}
	//render selected card
	card.render(con, view_w / 2 - card::card_w / 2, view_h - card::card_h);

	if (get_mouse_left())
	{
		if (choices.size() != 0 && selected_id != -1)
		{
			lua_push_enemy(sys.yielded_L, choices[selected_id]);
			sys.first_lua_target_function = false;
			resume_card_use(sys,1);
			return;
		}
	}
	if (get_mouse_right())
	{
		cancel_card_use(sys);
		sys.gui_state = gui_state::player_turn;
	}
}

void done_yielded_lua(game_systems& sys)
{
	sys.gui_state = gui_state::player_turn;
	if (sys.yielded_L)
	{
		sys.yielded_L = nullptr;
		luaL_unref(sys.L, LUA_REGISTRYINDEX, sys.coroutine_ref);
	}
}
void cancel_card_use(game_systems& sys)
{
	//if it didn't yet do anything, return it
	//if it already might have done something. procede to ??
	bool soft_cancel = sys.first_lua_target_function;

	done_yielded_lua(sys);
	finish_card_use(sys, soft_cancel);
}

void finish_card_use(game_systems& sys,bool soft_cancel)
{
	auto& hand = *sys.hand;
	auto id = hand.selected_card;
	if (id == -1 || id >= hand.cards.size())
	{
		assert(false);
	}
	auto& card = hand.cards[id];
	if (!soft_cancel)
	{
		sys.player->current_ap -= card.cost_ap;
	}
	auto ret = (soft_cancel)?(card_fate::hand):(card.after_use);
	switch (ret)
	{
	case card_fate::destroy:
		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::discard:
		sys.discard->cards.push_back(card);
		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::hand:
		//do nothing?
		break;
	case card_fate::draw_pile_top:
		sys.deck->cards.insert(sys.deck->cards.begin(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::draw_pile_rand:
		sys.deck->cards.insert(sys.deck->cards.begin() + rand() % sys.deck->cards.size(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::draw_pile_bottom:
		sys.deck->cards.insert(sys.deck->cards.end(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	default:
		break;
	}
}
void handle_animating(console& con, game_systems& sys)
{
	if (auto anim = sys.animation.get())
	{
		anim->animate_step();
		if (anim->done_animation())
		{
			sys.animation.reset();
			//int args_from_anim = anim->push_lua_args();
			resume_card_use(sys);
		}
	}
	else
	{
		sys.gui_state = gui_state::player_turn;
	}
}
void handle_enemy_turn(console& con, game_systems& sys)
{
	sys.hand->render(con);
	const float max_player_distance = 30;
	if (!sys.e_turn)
	{
		//if called first time, fill out entity data
		sys.e_turn.reset(new enemy_turn_data);
		auto& v = sys.e_turn->enemies;
		for (auto& en : sys.map->entities)
		{
			if (en->type == entity_type::enemy)
			{
				v.push_back(static_cast<e_enemy*>(en.get()));
			}
		}
		sys.e_turn->current_enemy_turn = 0;
		sys.e_turn->current_enemy_changed = true;

		sys.map->pathfind_field(sys.player->pos, max_player_distance);
	}
	auto& edata = *sys.e_turn;
	//if none are left, switch the state to player turn
	if (edata.current_enemy_turn >= edata.enemies.size())
	{
		sys.e_turn.reset();
		end_enemy_turn(sys);
		return;
	}
	if (edata.current_animation)
	{
		//just animate without thinking too much
		auto anim = edata.current_animation.get();

		anim->animate_step();
		if (anim->done_animation())
		{
			edata.current_animation.reset();
		}
		else
		{
			return; //bail early. we've done something already OR we are waiting for next frame!
		}
	}
	auto& simulated = *edata.enemies[edata.current_enemy_turn];

	if (edata.current_enemy_changed)
	{
		edata.current_enemy_changed = false;

		simulated.done_dmg = false;
		simulated.done_move = false;
	}
	//animate it's actions
	bool enemy_done = false;
	if (!simulated.done_move)
	{
		simulated.done_move = true;

		auto anim = std::make_unique<anim_unit_walk>();
		anim->path = sys.map->get_path(simulated.pos, simulated.move+1);
		anim->walker = &simulated;
		anim->start_animation();

		edata.current_animation= std::move(anim);	
		return;
	}
	else if (!simulated.done_dmg)
	{
		simulated.done_dmg = true;

		auto delta = sys.player->pos - simulated.pos;
		if(abs(delta.x)<=1 && abs(delta.y)<=1)
		{
			auto anim = std::make_unique<anim_player_damage>();
			anim->player = sys.player;
			anim->player_wound_hand = sys.hand;
			anim->damage_to_do = simulated.dmg;
			anim->start_animation();
		
			edata.current_animation = std::move(anim);
		}
		return;
	}
	else
	{
		enemy_done = true;
	}
	//switch to the other one
	if (enemy_done=true)
	{
		edata.current_enemy_turn++;
		edata.current_enemy_changed = true;
	}
}
int lua_sys_damage(lua_State* L)
{
	printf("Called damage!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	
	auto enemy = luaL_check_enemy(L, 1);
	int damage = luaL_checkint(L, 2);

	auto anim = std::make_unique<anim_enemy_damage>();
	anim->enemy = enemy;
	anim->damage_to_do = damage;

	sys.animation = std::move(anim);
	sys.animation->start_animation();
	sys.gui_state = gui_state::animating;
	return lua_yield(L,0);
}
int lua_sys_move(lua_State* L)
{
	printf("Called move!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto mover = luaL_check_entity(L, 1);
	auto path = lua_to_path(L, 2);

	auto anim = std::make_unique<anim_unit_walk>();
	anim->path = path;

	anim->walker = mover;
	sys.animation = std::move(anim);
	sys.animation->start_animation();
	sys.gui_state = gui_state::animating;
	
	return lua_yield(L, 0);
}
int lua_sys_target_path(lua_State* L)
{
	printf("Called target path!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	float range = (float)luaL_checknumber(L, 2);

	sys.map->pathfind_field(pos, range);
	sys.gui_state = gui_state::selecting_path;

	return lua_yield(L, 0);
}
int lua_sys_target_enemy(lua_State* L)
{
	printf("Called target enemy!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	float range = (float)luaL_checknumber(L, 2);

	fill_enemy_choices(pos, range, *sys.map, sys.enemy_choices);
	sys.gui_state = gui_state::selecting_enemy;

	return lua_yield(L, 0);
}
#define ADD_SYS_COMMAND(name) lua_pushlightuserdata(L, &sys);lua_pushcclosure(L, lua_sys_## name, 1); lua_setfield(L, -2, # name)
void init_lua_system(game_systems& sys)
{
	auto L = sys.L;
	lua_getregistry(L);
	lua_newtable(L);

	ADD_SYS_COMMAND(damage);
	ADD_SYS_COMMAND(move);
	ADD_SYS_COMMAND(target_path);
	ADD_SYS_COMMAND(target_enemy);

	lua_push_player(L, sys.player);
	lua_setfield(L, -2, "player");

	lua_setfield(L, -2, "system");

	lua_newtable(L);
	lua_setfield(L, -2, "cards");

	lua_pop(L, 1);
}
#undef ADD_SYS_COMMAND
static int wrap_exceptions(lua_State *L, lua_CFunction f)
{
	try {
		return f(L);  // Call wrapped function and return result.
	}
	catch (const char *s) {  // Catch and convert exceptions.
		lua_pushstring(L, s);
	}
	catch (std::exception& e) {
		lua_pushstring(L, e.what());
	}
	catch (...) {
		lua_pushliteral(L, "caught (...)");
	}
	return lua_error(L);  // Rethrow as a Lua error.
}
void init_lua(game_systems& sys)
{
	lua_stack_guard g(sys.L);
	luaL_openlibs(sys.L);

	lua_pushlightuserdata(sys.L, (void *)wrap_exceptions);
	luaJIT_setmode(sys.L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
	lua_pop(sys.L, 1);

	init_lua_system(sys);
	
	string path = asset_path + "/booster_starter.lua";
	auto ret=luaL_dofile(sys.L, path.c_str());
	if (ret != 0)
	{
		printf("Error:%s\n", lua_tostring(sys.L, -1));
		assert(false);
	}
	lua_load_booster(sys.L,1, sys.possible_cards);
}
void game_loop(console& graphics_console, console& text_console)
{
	auto& window = text_console.get_window();
	float time = 0;
	while (window.isOpen())
	{
		game_systems sys;
		
		std::random_device rd;
		sys.rand.seed(rd());

		state_map states;
		states.init_default_states();

		state current_state = states.data["game_start"];

		card_hand hand;
		hand.hand_gui_y = view_h - 8;

		card_deck deck;
		deck.y = view_h - card::card_h - 10;
		deck.x =  -card::card_w+3;
		deck.text = "Deck";

		card_deck discard;
		discard.y = view_h - card::card_h - 10;
		discard.x = view_w - card::card_w /4;
		discard.text = "Discard";

		sys.hand = &hand;
		sys.deck = &deck;
		sys.discard = &discard;

		////TEMP cards fill

		
		/////////////////////////


		auto world = map(map_w, map_h);
		world.view_rect = { 0,0,view_w,view_h };
		world.view_pos= { map_w / 2 - view_w / 2,map_h / 2 - view_h / 2 };
		dbg_init_world(world);
		sys.map = &world;

		{
			std::unique_ptr<e_player> ptr_player;
		
			ptr_player.reset(new e_player);
			auto player = sys.player = ptr_player.get();

			player->pos = v2i(map_w / 2, map_h / 2);
			player->glyph = '@';
			player->color_fore = v3f(1, 1, 1);
			player->type = entity_type::player;

			world.entities.emplace_back(std::move(ptr_player));
		}
		{
			auto enemy = new e_enemy;

			enemy->pos = v2i(map_w / 2+5, map_h / 2);
			enemy->glyph = 'g';
			enemy->color_fore = v3f(0.2f, 0.8f, 0.1f);
			enemy->type = entity_type::enemy;

			world.entities.emplace_back(enemy);
		}
		{
			auto enemy = new e_enemy;

			enemy->pos = v2i(map_w / 2 -1, map_h / 2);
			enemy->glyph = 'g';
			enemy->color_fore = v3f(0.2f, 0.8f, 0.1f);
			enemy->type = entity_type::enemy;

			world.entities.emplace_back(enemy);
		}
		{
			auto enemy = new e_enemy;

			enemy->pos = v2i(map_w / 2 - 15, map_h / 2);
			enemy->glyph = 'G';
			enemy->dmg = 3;
			enemy->move = 1;
			enemy->color_fore = v3f(0.2f, 0.8f, 0.1f);
			enemy->type = entity_type::enemy;

			world.entities.emplace_back(enemy);
		}

		
		
		//int size_min = std::min(map_w, map_h);
		v2i center = v2i(map_w, map_h) / 2;
		sys.restart = false;
		v2i from_click;
		v2i last_mouse;

		//TODO: defer lua_close
		sys.L = luaL_newstate();
		init_lua(sys);


		for (int i = 0; i<15; i++)
			discard.cards.push_back(sys.possible_cards["strike"]);
		for (int i = 0; i<8; i++)
			discard.cards.push_back(sys.possible_cards["push"]);
		end_enemy_turn(sys);

		while (!sys.restart && window.isOpen())
		{
			// Process events
			sf::Event event;

			while (window.pollEvent(event))
			{
				// Close window: exit
				if (event.type == sf::Event::Closed)
					window.close();

				if (event.type == sf::Event::KeyPressed)
				{
					// Escape key: exit
					if (event.key.code == sf::Keyboard::Escape)
					{
						if (sys.gui_state == gui_state::exiting || sys.gui_state == gui_state::lost)
							window.close();
						else if (sys.gui_state == gui_state::player_turn)
							sys.gui_state = gui_state::exiting;
						else
							sys.gui_state = gui_state::player_turn;
					}
					else
					{
						if (sys.gui_state == gui_state::exiting)
						{
							sys.gui_state = gui_state::player_turn;
						}
						else if (sys.gui_state == gui_state::lost)
						{
							sys.restart = true;
						}
					}
					if (event.key.code == sf::Keyboard::Space)
					{
						current_state = states.next_state(current_state);
						if (sys.gui_state == gui_state::player_turn)
						{
							end_player_turn(sys);
						}
					}
					if (event.key.code == sf::Keyboard::S)
						save_map(world);
					if (event.key.code == sf::Keyboard::L)
						load_map(world);
					if (event.key.code == sf::Keyboard::A)
					{
						sys.player->current_ap = sys.player->actions_per_turn;
					}
				}
				if (event.type == sf::Event::MouseMoved)
				{
					auto tile_size = graphics_console.get_glypht_size();
					auto cur_mouse = v2i(int(round(event.mouseMove.x / tile_size.x)), int(round(event.mouseMove.y / tile_size.y)));
					if (cur_mouse.x < 0)cur_mouse.x = 0;
					if (cur_mouse.y < 0)cur_mouse.y = 0;
					if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle))
					{
						auto delta = cur_mouse - last_mouse;
						sys.map->view_pos -= delta;
					}
					last_mouse = cur_mouse;
				}
			}

			graphics_console.clear();
			//state independant stuff
			world.render(graphics_console);
			sys.player->render_gui(graphics_console, 0, 0);
			//state dep. stuff
			if(sys.gui_state ==gui_state::player_turn)
			{
				handle_player_turn(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::exiting)
			{
				handle_exiting(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::lost)
			{
				handle_lost(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::selecting_path)
			{
				handle_selecting_path(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::selecting_enemy)
			{
				handle_selecting_enemy(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::animating)
			{
				handle_animating(graphics_console, sys);
			}
			else if (sys.gui_state == gui_state::enemy_turn)
			{
				handle_enemy_turn(graphics_console, sys);
			}
			
			text_console.set_text_centered(v2i(view_w / 2, view_h - 1), current_state.name, v3f(0.4f, 0.5f, 0.5f));
			for(float t=0;t<3.14;t+=0.01f)
			{
				auto p=world.raycast(sys.player->pos+v2i(0,1), v2f(cos(t), sin(t)));
				world.render_path(graphics_console, p, v3f(0.5, 1, 1));
			}

			//draw_asciimap(graphics_console);
			//if (&text_console != &graphics_console)
			//	text_console.render();
			graphics_console.render();
			

			window.display();
		}
		lua_close(sys.L);
	}
}



int main(int argc,char **argv)
{
	if (argc > 1)
	{
		asset_path = argv[1];
	}
    console text_console(view_w, view_h, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_Paul_10x10), EMB_FILE_SIZE_Paul_10x10, 10, 10));
	console& graphics_console = text_console;
	//console graphics_console(&text_console,(view_w /10)*12, (view_h / 10) * 12, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_cp437_12x12), EMB_FILE_SIZE_cp437_12x12, 12, 12));

	game_loop(graphics_console,text_console);
  
    return EXIT_SUCCESS;
}