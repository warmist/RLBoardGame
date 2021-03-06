
#include "common.hpp"
#include "console.hpp"
#define LUA_HELPER_IMPLEMENTATION
#include "lua_helper.hpp"

#include "map.hpp"
#include <unordered_set>
#include <functional>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

#include "virtual_fsys.hpp"

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
	selecting_card,
	ending_turn,
	yielded_lua, //actually a pseudo state as it can never reach "handle"
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
template<card_state S>
struct card_deck
{
	card_vector<S> cards;

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

		int wounds = cards.count_wounds();

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
	card_vector<card_state::hand> cards;

	int selected_card=-1; //FIXME: bad idea prob...
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
			auto& card = cards[i];
			auto p = get_card_extents(hand_gui_y, i);
			if (i == selected_card)
			{
				//skip this card as we are going to draw it last
			}
			else
				card.render(c, p.x, p.y);
		}
		if (selected_card >= 0 && selected_card < cards.size())
		{
			auto p = get_card_extents(hand_gui_y, selected_card);
			auto& card = cards[selected_card];
			card.render(c, p.x, view_h - card::card_h);
		}
	}
};
//player receives damage
struct anim_player_damage :public anim
{
	entity* player;
	card_hand* player_wound_hand;
	lua_State* L; 
	card wound_card; //TODO: this could be card_ref instead

	static const int tween_frames = 10;

	v3f original_color;
	int max_steps() override
	{
		return (int)tween_frames;
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
			//TODO: NEED LUA HERE!
			auto id = player_wound_hand->cards.r->new_card(wound_card,L);
			player_wound_hand->cards.push_back(id);
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
#include "booster.hpp"

entity* luaL_check_entity(lua_State*L, int arg) { 
	
	if (auto ret = luaL_testudata(L, arg, "entity"))
	{
		return *static_cast<entity**>(ret);
	}
	else if (auto ret = luaL_testudata(L, arg, "entity.player"))
	{
		return *static_cast<e_player**>(ret);
	}
	else if (auto ret = luaL_check_enemy(L, arg))
	{
		return ret;
	}
	luaL_typerror(L, arg, "entity.*");
	return nullptr;
}
e_player* luaL_check_player(lua_State*L ,int arg){return *static_cast<e_player**>(luaL_checkudata(L, arg, "entity.player"));}

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
		//in this place it should be inited already
		assert(false);
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

#define MAX_STATES 16 //should be enough for everybody
struct game_state_machine
{
	gui_state state_stack[MAX_STATES];
	int current_top = 0;

	game_state_machine() { state_stack[0] = gui_state::player_turn; }
	void reset() { state_stack[0] = gui_state::player_turn; current_top = 0; }

	gui_state state() const { return state_stack[current_top]; }
};
struct game_systems
{
	std::mt19937 rand;
	bool restart = false;

	e_player* player;
	map* map;
	game_state_machine gui_state;
	bool mouse_down = false;
	//state specific stuff
	anim_ptr animation;
	std::unique_ptr<enemy_turn_data> e_turn;
	std::vector<e_enemy*> enemy_choices;

	int num_cards_select;
	std::vector<card_ref> card_choices;
	std::vector<card_change_ref> card_state_changes;
	int ending_current_card;
	//
	card_registry all_cards;
	enemy_registry all_enemies;

	card_hand* hand;

	card_deck<card_state::draw_pile>* deck;
	card_deck<card_state::discard>* discard;

	bool first_lua_target_function;
	//lua stuff
	lua_State* L = nullptr;
	lua_State* yielded_L = nullptr;
	bool lua_card_use = false;
	int coroutine_ref = LUA_NOREF;
	lua_booster booster;
	
};

void exit_state_enemy_turn(game_systems& sys);
void enter_state_enemy_turn(game_systems& sys);
void exit_state(game_systems& sys, gui_state s)
{
#define HANDLE_STATE(name) if(s==gui_state::name) exit_state_ ## name (sys)
	HANDLE_STATE(enemy_turn);
#undef HANDLE_STATE
}
void enter_state(game_systems& sys, gui_state s)
{
#define HANDLE_STATE(name) if(s==gui_state::name) enter_state_ ## name (sys)
	HANDLE_STATE(enemy_turn);
#undef HANDLE_STATE
}
void push_state(game_systems& g, gui_state s)
{
	auto& gs = g.gui_state;
	assert(gs.current_top < MAX_STATES - 1); //TODO: always assert!
	gs.state_stack[++gs.current_top] = s;
	enter_state(g, s);
}
gui_state pop_state(game_systems& g)
{
	auto& gs = g.gui_state;
	assert(gs.current_top>0); //TODO: assert in release mode too!
	gs.current_top--;
	auto old_state = gs.state_stack[gs.current_top + 1];
	exit_state(g, old_state);
	return old_state;
}
void remove_dead_enemies(game_systems& sys)
{
	sys.map->compact_entities();
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
			std::shuffle(discard.ids.begin(), discard.ids.end(), sys.rand);
			deck.append(discard);

			//Add hunger each time you shuffle
			auto id = sys.all_cards.new_card(sys.booster.cards["hunger"], sys.L);
			discard.push_back(id);
		}
	}
	auto card = deck.ids.front();
	pop_front(deck.ids);
	hand.push_back(card);
	return true;
}
void exit_state_enemy_turn(game_systems& sys)
{
	remove_dead_enemies(sys);

	auto& hand = sys.hand->cards;
	auto& discard = sys.discard->cards;
	//mix in the wounds
	discard.append(hand);
	//draw new cards
	const int hand_draw_count = 5;
	const int hand_max_count = 7;
	sys.player->current_ap = sys.player->actions_per_turn;

	int draw_count = std::max(std::min(int(hand_max_count - hand.size()), hand_draw_count), 0);
	for (int i = 0; i < draw_count; i++)
	{
		if (!draw_card(sys))
		{
			push_state(sys, gui_state::lost);
			return;
		}
	}
	//add generated cards
	//FIXME: might be yielded L here? probably not but still
	auto id = sys.all_cards.new_card(sys.booster.cards["move"], sys.L);
	hand.push_back(id);
}
void enter_state_enemy_turn(game_systems& sys)
{
	auto& hand = sys.hand->cards;

	int wound_count = hand.count_wounds();
	//CANT DO THIS state pushes here but dunno...
	if (wound_count >= 3)
	{
		push_state(sys, gui_state::lost);
		return;
	}
}
void done_yielded_lua(game_systems& sys);
void finish_card_use(game_systems& sys, bool soft_cancel);
void cancel_card_use(game_systems& sys);
void resume_lua(game_systems& sys, int args_pushed = 0)
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
		if(sys.lua_card_use)
			finish_card_use(sys, false);
		return;
	}
	else if (result != LUA_YIELD)
	{
		msghandler(sys.yielded_L);
		printf("Error:%s\n", lua_tostring(sys.yielded_L, -1));
		assert(false);
	}
}
void use_card_actual(card& card, game_systems& g)
{
	g.first_lua_target_function = true; //reset "first target func" condition
	g.yielded_L = card.yieldable_use(g.L);
	g.lua_card_use = true;
	g.coroutine_ref=luaL_ref(g.L, LUA_REGISTRYINDEX);
	push_state(g, gui_state::yielded_lua);
	resume_lua(g,2);
}
void end_turn_card_actual(card& card, game_systems& g)
{
	g.first_lua_target_function = true; //reset "first target func" condition
	//TODO: actually block all selections? maybe not?
	g.yielded_L = card.yieldable_turn_end(g.L);
	if(g.yielded_L)
	{
		g.lua_card_use = false;
		g.coroutine_ref = luaL_ref(g.L, LUA_REGISTRYINDEX);
		push_state(g, gui_state::yielded_lua);
		resume_lua(g, 2);
	}
}
void fill_enemy_choices(v2i center, float distance,map& m,bool ignore_visibility, std::vector<e_enemy*>& enemies)
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
            auto ee = static_cast<e_enemy*>(e.get());
                if(ee->current_hp>0)
			        enemies.push_back(ee);
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

void handle_player_turn(console& con,game_systems& sys)
{
	sys.hand->input(con);
	sys.deck->input(con);
	sys.discard->input(con);
	handle_card_use(get_mouse_left(), sys);
	sys.deck->render(con);
	sys.discard->render(con);
	sys.hand->render(con);
	
	
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
	{
		sys.ending_current_card = 0;
		push_state(sys, gui_state::ending_turn);
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
		push_state(sys, gui_state::exiting);
}
void handle_exiting(console& con, game_systems& sys)
{
	int exit_h = view_h / 2;
	con.set_text_centered(v2i(view_w / 2, exit_h), "Are you sure you want to exit?", v3f(0.6f, 0, 0));
	con.set_text_centered(v2i(view_w / 2, exit_h + 1), "(press esc to confirm, anything else - cancel)");
	if (get_mouse_right())
	{
		push_state(sys, gui_state::player_turn);
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
	auto path = sys.map->get_path(mouse_pos + sys.map->view_pos, false);
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
			auto s = pop_state(sys);
			assert(s == gui_state::selecting_path);
			resume_lua(sys,1);
			return;
		}
	}
	if (get_mouse_right())
	{
		auto s = pop_state(sys);
		assert(s == gui_state::selecting_path);
		cancel_card_use(sys);
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
			auto s = pop_state(sys);
			assert(s == gui_state::selecting_enemy);
			resume_lua(sys,1);
			return;
		}
	}
	if (get_mouse_right())
	{
		auto s = pop_state(sys);
		assert(s == gui_state::selecting_enemy);
		cancel_card_use(sys);
	}
}
void handle_selecting_card(console& con, game_systems& sys)
{
	//draw all the possible choices
	//TODO: simple layout for now
	int count_w = view_w / card::card_w;
	auto& choices = sys.card_choices;
	int selected_cards = 0;
	card* current_card = nullptr;
	auto mouse = get_mouse(con);
	for (size_t i = 0; i < choices.size(); i++)
	{
		int card_x = (int)i % count_w;
		int card_y = (int)i / count_w;
		auto& c = sys.all_cards.cards[choices[i]];
		c.render(con, card_x*card::card_w, card_y*card::card_h);
		if (c.selected)
			selected_cards++;
		if (recti(card_x*card::card_w, card_y*card::card_h, card::card_w, card::card_h).is_inside(mouse))
			current_card = &c;
	}
	//handle input
	if (get_mouse_left() && !sys.mouse_down)
	{
		//figure out where is the mouse (on which card)
		if (current_card && current_card->selected)//unselect if already selected
		{
			current_card->selected = false;
		}
		else 
		{
			if (current_card)
			{
				if (selected_cards < sys.num_cards_select)
				{
					current_card->selected = true;
				}
			}
		}
		sys.mouse_down = true;
	}
	else if(!get_mouse_left())
		sys.mouse_down = false;
	//return selected card list
	if (get_mouse_right())
	{
		for (size_t i = 0; i < choices.size(); i++)
		{
			auto& c = sys.all_cards.cards[choices[i]];
			c.selected = false;
		}
		choices.clear();
		auto s = pop_state(sys);
		assert(s == gui_state::selecting_card);
		cancel_card_use(sys);
		return;
	}
	bool finish_selection = selected_cards == sys.num_cards_select;
	if (finish_selection)
	{
		lua_newtable(sys.yielded_L);
		int num_exported = 0;
		for (size_t i = 0; i < choices.size(); i++)
		{
			auto& c = sys.all_cards.cards[choices[i]];
			if (c.selected)
			{
				lua_push_card_ref(sys.yielded_L, choices[i]);
				lua_rawseti(sys.yielded_L, -2, ++num_exported);
				c.selected = false;
			}
		}
		sys.first_lua_target_function = false;
		choices.clear();
		auto s = pop_state(sys);
		assert(s == gui_state::selecting_card);
		resume_lua(sys, 1);
		return;
	}
}
void done_yielded_lua(game_systems& sys)
{
	auto s=pop_state(sys);
	assert(s == gui_state::yielded_lua);

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
void apply_card_state_changes(game_systems& sys)
{
	auto& v = sys.card_state_changes;

	if (v.size() == 0)
		return; //yay nothing to do !

	//	first need to sort stuff because remove duplicates
	std::sort(v.begin(), v.end(), [](const card_change_ref& a,const card_change_ref& b)
	{ return a.id<b.id;});
	//remove dupplicates
	v.erase(std::unique(v.begin(), v.end(), [](const card_change_ref& a, const card_change_ref& b) {return a.id == b.id; }), v.end());
	//now iterate over the copies and reinsert them into the correct place
	for (auto& c : v)
	{
		auto& card = sys.all_cards.cards[c.id];
		switch (card.current_state)
		{
		case card_state::proto:
		case card_state::destroyed:
			assert(false);//should not happen
			break;
		case card_state::discard:
			sys.discard->cards.remove_card(c.id);
			break;
		case card_state::draw_pile:
			sys.deck->cards.remove_card(c.id);
			break;
		case card_state::hand:
			sys.hand->cards.remove_card(c.id);
			break;
		}

		switch (c.new_state)
		{
		case card_state_change::destroy:
			sys.all_cards.unreg_card(c.id,sys.L);
			break;
		case card_state_change::discard:
			sys.discard->cards.push_back(c.id);
			break;
		case card_state_change::draw_pile_bottom:
			sys.deck->cards.push_back(c.id);
			break;
		case card_state_change::hand:
			sys.hand->cards.push_back(c.id);
			break;
		case card_state_change::draw_pile_top:
			sys.deck->cards.push_top(c.id);
			break;
		case card_state_change::draw_pile_rand:
			sys.deck->cards.push_rand(c.id, sys.rand);
			break;
		default:
			break;
		}
	}
	//finally clear it for the future generations
	v.clear();
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
	apply_card_state_changes(sys);
}
void handle_animating(console& con, game_systems& sys)
{
	sys.hand->render(con); //TODO: only render this when animating ENEMY turn
	if (auto anim = sys.animation.get())
	{
		anim->animate_step();
		if (anim->done_animation())
		{
			sys.animation.reset();
			//int args_from_anim = anim->push_lua_args();
			auto s = pop_state(sys);
			assert(s == gui_state::animating);
			resume_lua(sys);
		}
	}
	else
	{
		pop_state(sys);
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
	}
	auto& edata = *sys.e_turn;
	//if none are left, switch the state to player turn
	if (edata.current_enemy_turn >= edata.enemies.size())
	{
		sys.e_turn.reset();
		pop_state(sys);
		return;
	}

	auto& enemy = edata.enemies[edata.current_enemy_turn];
	
	sys.yielded_L = enemy->yieldable_turn(sys.L);
	sys.lua_card_use = false;
	sys.coroutine_ref = luaL_ref(sys.L, LUA_REGISTRYINDEX);
	push_state(sys, gui_state::yielded_lua);
	resume_lua(sys, 2);

	edata.current_enemy_turn++;
}
void handle_ending_turn(console& con, game_systems& sys)
{
	auto& dis = sys.discard->cards;
	auto& hand = sys.hand->cards;

	if (sys.ending_current_card >= hand.ids.size())
	{
		
		remove_dead_enemies(sys);
		//FIXME: at this state the card selects and others might be incorrect due to pending card state changes. 
		// easy fix: block selection/enumeration. But that would limit what could be done.
		apply_card_state_changes(sys);
		
		dis.append(hand);

		pop_state(sys);
		push_state(sys, gui_state::enemy_turn);
		return;
	}
	auto& card = hand[sys.ending_current_card];
	sys.ending_current_card++;
	end_turn_card_actual(card, sys);
}
//////////////////////////////////////////////////////////////////
int lua_card_mark_for_state_change(lua_State* L) 
{
	//Grr... should be in cards but they don't know the game_system stuff
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	int remove_type = (int)lua_tointeger(L, lua_upvalueindex(2));
	
	card_ref ref = lua_check_card_ref(L, 1);
	card_change_ref cref;

	cref.id = ref;

	cref.new_state = (card_state_change)remove_type;
	sys.card_state_changes.push_back(cref);
	return 0;
}
void init_card_moves(game_systems& sys)
{
	lua_State* L = sys.L;
	lua_newtable(L);
#define ADD_MOVE_MODE(x) lua_pushlightuserdata(L, &sys);\
		lua_pushnumber(L,(int)(card_state_change::  x));\
		lua_pushcclosure(L, lua_card_mark_for_state_change, 2);\
		lua_setfield(L,-2,#x)
	ADD_MOVE_MODE(destroy);
	ADD_MOVE_MODE(discard);
	ADD_MOVE_MODE(hand);
	ADD_MOVE_MODE(draw_pile_top);
	ADD_MOVE_MODE(draw_pile_rand);
	ADD_MOVE_MODE(draw_pile_bottom);
#undef ADD_MOVE_MODE
	lua_setfield(L, LUA_REGISTRYINDEX, "_card_moves");
}
void init_enemy_moves(game_systems& sys)
{
	lua_State* L = sys.L;
	lua_newtable(L);
	/*
#define ADD_MOVE_MODE(x) lua_pushlightuserdata(L, &sys);\
		lua_pushnumber(L,(int)(card_state_change::  x));\
		lua_pushcclosure(L, lua_card_mark_for_state_change, 2);\
		lua_setfield(L,-2,#x)
	ADD_MOVE_MODE(destroy);
	ADD_MOVE_MODE(discard);
	ADD_MOVE_MODE(hand);
	ADD_MOVE_MODE(draw_pile_top);
	ADD_MOVE_MODE(draw_pile_rand);
	ADD_MOVE_MODE(draw_pile_bottom);
#undef ADD_MOVE_MODE*/
	lua_setfield(L, LUA_REGISTRYINDEX, "_enemy_moves");
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
	push_state(sys, gui_state::animating);
	return lua_yield(L, 0);
}
int lua_sys_damage_player(lua_State* L)
{
	printf("Called damage player!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));

	auto player = luaL_check_player(L, 1);
	auto card_name = luaL_optstring(L, 2, "wound"); //TODO: use ref instead of string
	//TODO: arg 3 count?

	auto anim = std::make_unique<anim_player_damage>();
	anim->player = player;

	
	anim->wound_card = sys.booster.cards[card_name];
	anim->player_wound_hand = sys.hand;
	anim->L = L;

	sys.animation = std::move(anim);
	sys.animation->start_animation();
	push_state(sys, gui_state::animating);
	return lua_yield(L, 0);
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
	push_state(sys, gui_state::animating);
	
	return lua_yield(L, 0);
}
int lua_sys_target_path(lua_State* L)
{
	printf("Called target path!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	float range = (float)luaL_checknumber(L, 2);

	sys.map->pathfind_field(pos, range);
	push_state(sys, gui_state::selecting_path);

	return lua_yield(L, 0);
}
int lua_sys_target_enemy(lua_State* L)
{
	printf("Called target enemy!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	float range = (float)luaL_checknumber(L, 2);

	bool ignore_visibility = false;
	fill_enemy_choices(pos, range, *sys.map, ignore_visibility, sys.enemy_choices);
	push_state(sys, gui_state::selecting_enemy);

	return lua_yield(L, 0);
}
int lua_sys_raycast(lua_State* L)
{
	printf("Called raycast!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	v2i target = luaL_check_v2i(L, 2);
	bool ignore_last = lua_toboolean(L, 3);

	float path_len;
	auto ret=sys.map->raycast_target(pos, target,true,false, ignore_last,path_len);
	lua_push_path(L, ret);
	lua_pushnumber(L, path_len);
	return 2;
}
int lua_sys_pathfind(lua_State* L)
{
	printf("Called pathfind!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	v2i pos = luaL_check_v2i(L, 1);
	v2i target = luaL_check_v2i(L, 2);
	bool ignore_last = lua_toboolean(L, 3);
	auto ret=sys.map->pathfind(pos.x, pos.y, target.x, target.y);
	std::reverse(ret.begin(), ret.end());
	lua_push_path(L, ret);
	return 1;
}
int lua_sys_target_card(lua_State* L)
{
	printf("Called target card!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));
	luaL_checktype(L, 1, LUA_TTABLE);
	sys.num_cards_select=luaL_optint(L, 2, 1);

	for (int i = 1; i <= lua_objlen(L, 1); i++)
	{
		lua_rawgeti(L, 1, i);
		sys.card_choices.push_back(lua_tocard_ref(L,-1));
		lua_pop(L, 1);
	}
	push_state(sys, gui_state::selecting_card);

	return lua_yield(L,0);
}
int lua_sys_get_cards(lua_State* L)
{
	printf("Called get_cards!\n");
	game_systems& sys = *reinterpret_cast<game_systems*>(lua_touserdata(L, lua_upvalueindex(1)));

	int card_location = luaL_checkint(L, 1);
	if (card_location < 1 || card_location>3)
	{
		luaL_error(L, "Invalid location: %d, supported: 1,2,3",card_location);
		return 0;
	}
	std::vector<int>* cards;
	
	switch (card_location)
	{
	case 1:
		cards = &sys.deck->cards.ids;
		break;
	case 2:
		cards = &sys.hand->cards.ids;
		break;
	case 3:
		cards = &sys.discard->cards.ids;
		break;
	default:
		luaL_error(L, "Reached an unreachable part! get_cards");
		break;
	}

	lua_newtable(L);

	for (size_t i = 0; i < cards->size(); i++)
	{
		lua_push_card_ref(L, cards->at(i));
		lua_rawseti(L, -2, (int)i + 1);
	}

	return 1;
}

#define ADD_SYS_COMMAND(name) lua_pushlightuserdata(L, &sys);lua_pushcclosure(L, lua_sys_## name, 1); lua_setfield(L, -2, # name)
void init_player_mt(game_systems& sys)
{
	auto L = sys.L;
	if(luaL_newmetatable(L,"entity.player"))
	{
		lua_pushcfunction(L, lua_entity_pos);
		lua_setfield(L, -2, "pos");

		//TODO: this is almost sys_command...
		lua_pushlightuserdata(L, &sys); 
		lua_pushcclosure(L, lua_sys_damage_player, 1); 
		lua_setfield(L, -2, "damage");

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		lua_pop(L, 1);
	}
	else 
	{
		assert(false);
		//this should be not innted yet!
	}
}
void init_lua_system(game_systems& sys)
{
	auto L = sys.L;
	init_player_mt(sys);
	lua_getregistry(L);
	lua_newtable(L);
	//player interaction function
	ADD_SYS_COMMAND(target_path);
	ADD_SYS_COMMAND(target_enemy);
	ADD_SYS_COMMAND(target_card);
	//animation functions
	ADD_SYS_COMMAND(damage_player);
	ADD_SYS_COMMAND(damage);
	ADD_SYS_COMMAND(move);
	//other
	ADD_SYS_COMMAND(raycast);
	ADD_SYS_COMMAND(pathfind);
	ADD_SYS_COMMAND(get_cards);

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
int luaL_loadfile_filesys(lua_State* L, const char* fname, const char* name = nullptr)
{
    char *buffer;
    size_t size;
    bool need_free;
    if (!vfilesys_load_file(asset_path.c_str(), fname, &buffer, size, need_free))
    {
        printf("File not found:%s\n", fname);
        assert(false);
        return 0;
    }
    if (name == nullptr)
        name = fname;
    auto ret=luaL_loadbuffer(L, buffer, size, name);
    if (need_free)
    {
        delete[] buffer;
    }
    return ret;
}
void init_lua(game_systems& sys)
{
	auto L = sys.L;
	lua_stack_guard g(L);
	luaL_openlibs(L);
	//hook exceptions (luajit thing)
	lua_pushlightuserdata(L, (void *)wrap_exceptions);
	luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
	lua_pop(L, 1);
	//init vector library
	if (luaL_loadfile_filesys(L,"vector_lib.llua")!=0)
	{
		printf("Lua load error:%s\n", lua_tostring(L, -1));
		assert(false);
	}
	if (lua_safecall(L, 0, 2) != 0)
	{
		printf("Lua error:%s\n", lua_tostring(L, -1));
		assert(false);
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "vec2");
	lua_setfield(L, LUA_REGISTRYINDEX, "vec3");
	//my own systems
	init_lua_system(sys);
	init_card_moves(sys);
	init_enemy_moves(sys);
	//load the cards
    if (luaL_loadfile_filesys(L, "booster_starter.lua") != 0)
    {
        printf("Lua load booster error:%s\n", lua_tostring(L, -1));
        assert(false);
    }
	if (lua_safecall(L,0,LUA_MULTRET) != 0)
	{
		printf("Booster error:%s\n", lua_tostring(L, -1));
		assert(false);
	}
	lua_load_booster(L,1, sys.booster);
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

		card_hand hand;
		hand.cards.r= &sys.all_cards;
		hand.hand_gui_y = view_h - 8;

		card_deck<card_state::draw_pile> deck;
		deck.cards.r = &sys.all_cards;
		deck.y = view_h - card::card_h - 10;
		deck.x =  -card::card_w+3;
		deck.text = "Deck";

		card_deck<card_state::discard> discard;
		discard.cards.r = &sys.all_cards;
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
		

		
		
		//int size_min = std::min(map_w, map_h);
		v2i center = v2i(map_w, map_h) / 2;
		sys.restart = false;
		v2i from_click;
		v2i last_mouse;

		lua_wrap main_lua;
		sys.L = main_lua.L;
		init_lua(sys);

		{
			auto enemy = sys.all_enemies.new_enemy(sys.booster.enemies["goblin"], sys.L, world);

			enemy->pos = v2i(map_w / 2 + 5, map_h / 2);
		}
		{
			auto enemy = sys.all_enemies.new_enemy(sys.booster.enemies["goblin"], sys.L, world);

			enemy->pos = v2i(map_w / 2 - 1, map_h / 2);
		}
		{
			auto enemy = sys.all_enemies.new_enemy(sys.booster.enemies["goblin"], sys.L, world);

			enemy->pos = v2i(map_w / 2 - 2, map_h / 2);
		}
		{
			auto enemy = sys.all_enemies.new_enemy(sys.booster.enemies["goblin"], sys.L, world);

			enemy->pos = v2i(map_w / 2 - 3, map_h / 2);
		}
		{
			auto enemy = sys.all_enemies.new_enemy(sys.booster.enemies["gablin"], sys.L, world);

			enemy->pos = v2i(map_w / 2 - 15, map_h / 2);
		}

		for (int i = 0; i<15; i++)
		{
			auto id = sys.all_cards.new_card(sys.booster.cards["strike"], sys.L);
			deck.cards.push_back(id);
		}
		for (int i = 0; i < 8; i++)
		{
			auto id = sys.all_cards.new_card(sys.booster.cards["push"], sys.L);
			deck.cards.push_back(id);
		}

		std::shuffle(deck.cards.ids.begin(), deck.cards.ids.end(), sys.rand);

		sys.gui_state.reset();
		exit_state_enemy_turn(sys);//Note: we do "end of enemy turn/start of player turn at the start of game

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
					//TODO: keypress handling is mixed here and in e.g. player_turn handler. Unmix pls! 
					//	Also here "KeyPressed" is better (key WAS pressed). In global func isKeyPressed is bad (key IS down?)
					if (event.key.code == sf::Keyboard::Escape)
					{
						if (sys.gui_state.state() == gui_state::exiting || sys.gui_state.state() == gui_state::lost)
							window.close();
					}
					else
					{
						if (sys.gui_state.state() == gui_state::exiting)
						{
							pop_state(sys);
						}
						else if (sys.gui_state.state() == gui_state::lost)
						{
							sys.restart = true;
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
#define HSTATE(x) case gui_state:: x: handle_ ## x(graphics_console,sys); break
			switch (sys.gui_state.state())
			{
				HSTATE(player_turn);
				HSTATE(exiting);
				HSTATE(lost);
				HSTATE(selecting_path);
				HSTATE(selecting_enemy);
				HSTATE(animating);
				HSTATE(enemy_turn);
				HSTATE(selecting_card);
				HSTATE(ending_turn);

			case gui_state::yielded_lua: assert(false); break;
			}
#undef HSTATE

			//draw_asciimap(graphics_console);
			//if (&text_console != &graphics_console)
			//	text_console.render();
			graphics_console.render();
			

			window.display();
		}
	}
}



int main(int argc,char **argv)
{
	if (argc > 1)
	{
		asset_path = argv[1];
	}
    char* buffer;
    size_t size;
    bool need_free;
    vfilesys_load_file(asset_path.c_str(), "paul_10x10.png", &buffer, size, need_free);
    console text_console(view_w, view_h, font_descriptor(reinterpret_cast<const unsigned char*>(buffer), size, 10, 10));
    if (need_free)
        delete[] buffer;

	console& graphics_console = text_console;
	//console graphics_console(&text_console,(view_w /10)*12, (view_h / 10) * 12, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_cp437_12x12), EMB_FILE_SIZE_cp437_12x12, 12, 12));

	game_loop(graphics_console,text_console);
  
    return EXIT_SUCCESS;
}