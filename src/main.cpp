
#include "common.hpp"
#include "console.hpp"

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
	card_needs type=card_needs::nothing;
	float distance;
};
struct card;
struct game_systems;
using card_action = void(*)(card& ,game_systems& , card_needs_output* );

constexpr float phi = 1.61803398874989484820f;
struct card
{
	std::string name;
	std::string desc;
	static const int card_w = 15;
	static const int card_h= int(card_w*phi);
	
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
	card_type type=card_type::action;
	//PRE_USE
	int cost_ap = 0;
	card_needs_input needs;
	//USE
	card_action use_callback = nullptr;
	//POST_USE
	card_fate after_use = card_fate::destroy;

	
	bool is_warn_ap=false;
	sf::Clock warn_ap;
	float tween_warn(float t)
	{
		if (t < 0.4)
		{
			float tt = 4 * t - 1;
			return 1 - tt*tt;
		}
		else
			return -1.067f*t + 1.067f;
	}
	v3f get_ap_color()
	{
		const float warn_ap_max_t = 0.5;//in sec
		if (!is_warn_ap)
		{
			return v3f(1, 1, 1);
		}
		else
		{
			float tv=warn_ap.getElapsedTime().asSeconds();
			if (tv > warn_ap_max_t)
			{
				is_warn_ap = false;
				return v3f(1, 1, 1);
			}
			else
			{
				float t=tween_warn(tv / warn_ap_max_t);
				return v3f(1, 0, 0)*t + v3f(1, 1, 1)*(1 - t);
			}
		}
	}
	void render(console& c,int x, int y)
	{
		v3f border_color;
		if (type == card_type::generated)
			border_color = { 0.08f, 0.04f, 0.37f };
		else if (type == card_type::action)
			border_color = { 0.57f, 0.44f, 0.07f };
		else if (type == card_type::wound)
			border_color = { 0.76f, 0.04f, 0.01f };

		/*
		const int h = 20;
		const int w = int(h*phi);
		*/
		const int w = card_w;
		const int h = card_h;

		for (int i = 0; i < w; i++)
		{
			for (int j = 0; j < h; j++)
			{
				c.set_char(v2i(i + x, j + y), ' ');
			}
		}

		for (int i = 0; i < w; i++)
		{
			c.set_char(v2i(x + i, y), tile_hline_double, border_color);
			c.set_char(v2i(x + i, y + h - 1), tile_hline_double, border_color);
		}
		for (int i = 0; i < h; i++)
		{
			c.set_char(v2i(x, y + i), tile_vline_double, border_color);
			c.set_char(v2i(x + w - 1, y + i), tile_vline_double, border_color);
		}
		c.set_char(v2i(x, y), tile_es_corner_double, border_color);
		c.set_char(v2i(x + w - 1, y), tile_ws_corner_double, border_color);
		c.set_char(v2i(x, y + h - 1), tile_ne_corner_double, border_color);
		c.set_char(v2i(x + w - 1, y + h - 1), tile_wn_corner_double, border_color);

		const int text_start = int(w / 2 - name.length() / 2);
		c.set_text(v2i(x + text_start, y), name);
		c.set_char(v2i(x + text_start + int(name.length()), y), tile_nse_t_double, border_color);
		c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color);
		int cur_y = y + 1;
		if(type != card_type::wound) //TODO: replace with "usable" flag?
		{
			c.set_text(v2i(x + 2, cur_y), "Cost");
			for (int i = 0; i<cost_ap;i++)
				c.set_char(v2i(x + card::card_w - cost_ap+i-2, cur_y), (unsigned char)'\xad',get_ap_color());
			cur_y++;
		}
		c.set_text(v2i(x + 2, cur_y), desc);
	}
};
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
	void render(console& c)
	{
		v3f border_color = v3f(1, 1, 1);
		auto my_rect = get_bounds();
		int x = my_rect.x();
		int y = my_rect.y();
		int w = my_rect.width();
		int h = my_rect.height();
		for (int i = 0; i < w; i++)
		{
			for (int j = 0; j < h; j++)
			{
				c.set_char(v2i(i + x, j + y), ' ');
			}
		}

		for (int i = 0; i < w; i++)
		{
			c.set_char(v2i(x + i, y), tile_hline_double, border_color);
			c.set_char(v2i(x + i, y + h - 1), tile_hline_double, border_color);
		}
		for (int i = 0; i < h; i++)
		{
			c.set_char(v2i(x, y + i), tile_vline_double, border_color);
			c.set_char(v2i(x + w - 1, y + i), tile_vline_double, border_color);
		}
		c.set_char(v2i(x, y), tile_es_corner_double, border_color);
		c.set_char(v2i(x + w - 1, y), tile_ws_corner_double, border_color);
		c.set_char(v2i(x, y + h - 1), tile_ne_corner_double, border_color);
		c.set_char(v2i(x + w - 1, y + h - 1), tile_wn_corner_double, border_color);

		
		
		const int text_start = int(w / 2 - text.length() / 2);
		
		c.set_text(v2i(x + text_start, y), text);
		c.set_char(v2i(x + text_start + int(text.length()), y), tile_nse_t_double, border_color);
		c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color);


		const int count_y = y+card::card_h-1;
		const int buffer_size = 10;
		char buffer[buffer_size];
		snprintf(buffer, buffer_size, "Count:%3d", (cards.size()>999)?(999):((int)cards.size()));
		
		c.set_text(v2i(x + 3, count_y), buffer);
		c.set_char(v2i(x + 3 + buffer_size-1, count_y), tile_nse_t_double, border_color);
		c.set_char(v2i(x + 3 - 1, count_y), tile_nsw_t_double, border_color);
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
	t_card.desc = "Loose game\nif 3 are\nin hand";
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
		c.set_text(v2i(x, y),   "Health: ");
		c.set_text(v2i(x, y+1), "Actions:");

		int dx = 0;
		for (int i = 0; i < actions_per_turn- current_ap; i++)
			c.set_char(v2i(x + 10 + (dx++), y + 1), (unsigned char)'\xad', v3f(0.4f, 0.4f, 0.4f));
		for (int i = 0; i < current_ap; i++)
			c.set_char(v2i(x + 10 + (dx++), y + 1), (unsigned char)'\xad', v3f(1, 1, 1));

		//c.set_text(v2i(x + 10, y+1), std::to_string(current_action) + "/" + std::to_string(actions_per_turn));
	}
};
struct e_enemy :public entity
{
	int current_hp=5;
	int max_hp=5;

	int move = 3;
	int dmg = 1;

	//simulation stuff
	bool done_move = false;
	bool done_dmg = false;
};

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

	e_player* player;
	map* map;
	gui_state gui_state = gui_state::player_turn;
	//state specific stuff
	anim_ptr animation;
	std::unique_ptr<enemy_turn_data> e_turn;
	std::vector<e_enemy*> enemy_choices;
	//
	card_hand* hand;
	card_hand* wounds_added;

	card_deck* deck;
	card_deck* discard;

	card_needs_output needs_out;
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
card default_move_action()
{
	card ret;
	ret.name = "Run";
	ret.cost_ap = 1;
	ret.desc = "Move\nRange      5\n\n\n\nRun to\nthe target";
	ret.type = card_type::generated;
	ret.needs.type = card_needs::walkable_path;
	ret.needs.distance = 5;
	ret.use_callback = &act_move;
	return ret;
}
void use_card_actual(card& card, game_systems& g)
{
	auto& hand = *g.hand;
	g.player->current_ap -= card.cost_ap;
	card.use_callback(card, g, &g.needs_out);
	auto ret = card.after_use;
	switch (ret)
	{
	case card_fate::destroy:
		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::discard:
		g.discard->cards.push_back(card);
		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::hand:
		//do nothing?
		break;
	case card_fate::draw_pile_top:
		g.deck->cards.insert(g.deck->cards.begin(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::draw_pile_rand:
		g.deck->cards.insert(g.deck->cards.begin()+rand()%g.deck->cards.size(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	case card_fate::draw_pile_bottom:
		g.deck->cards.insert(g.deck->cards.end(), card);

		hand.cards.erase(hand.cards.begin() + hand.selected_card);
		hand.selected_card = -1;
		break;
	default:
		break;
	}
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
		if (card.needs.type != card_needs::nothing)
		{
			//prep for needs
			switch (card.needs.type)
			{
			case card_needs::visible_target_unit:
				fill_enemy_choices(g.player->pos, card.needs.distance, *g.map, g.enemy_choices);				
				g.gui_state = gui_state::selecting_enemy;
				break;
			case card_needs::walkable_path:
				g.map->pathfind_field(g.player->pos, card.needs.distance);
				g.gui_state = gui_state::selecting_path;
				break;
			default:
				assert(false);
			}
			
			return;
		}
		if (card.use_callback)
		{
			use_card_actual(card, g);
		}
	}
}
void strike_action(card&,game_systems& g, card_needs_output* needs)
{
	auto anim = std::make_unique<anim_enemy_damage>();
	anim->enemy = static_cast<e_enemy*>(needs->visible_target_unit);
	anim->damage_to_do = rand() % 6 + 1;

	g.animation = std::move(anim);
	g.animation->start_animation();
	g.gui_state = gui_state::animating;
}
card strike_card()
{
	card t_card;
	t_card.name = "Strike";
	t_card.cost_ap = 2;
	t_card.desc = "\nAttack\nRange     0\nDmg     1D6\n\n\n\nPerform an\nattack with\na very big\nsword";
	t_card.type = card_type::action;
	t_card.use_callback = strike_action;
	t_card.needs.distance = 2;
	t_card.needs.type = card_needs::visible_target_unit;
	return t_card;
}
card push_card()
{
	//TODO: unfinished
	card t_card;
	t_card.name = "Push";
	t_card.cost_ap = 1;
	t_card.desc = "\nAction\nRange     0\nDist.   1D4\n\n\n\nForce a\nmove";
	t_card.type = card_type::action;
	return t_card;
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
void state_set_to_player(game_systems& sys)
{
	remove_dead_enemies(sys);

	auto& hand = sys.hand->cards;
	auto& discard = sys.discard->cards;
	auto& wnds = sys.wounds_added->cards;
	//mix in the wounds
	discard.insert(discard.end(), wnds.begin(), wnds.end());
	wnds.clear();
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
	hand.push_back(default_move_action());
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
	sys.map->render_path(con, path, v3f(0.3f, 0.7f, 0.2f), color_fail);

	if (path.size() == 0)
		con.set_back(mouse_pos, color_fail);
	//render selected card

	card.render(con, view_w / 2 - card::card_w / 2, view_h - card::card_h);

	if (get_mouse_left())
	{
		if (path.size() != 0)
		{
			sys.needs_out.walkable_path = path;
			use_card_actual(card, sys);
			return;
		}
	}
	if (get_mouse_right())
	{
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
			sys.needs_out.visible_target_unit = choices[selected_id];
			use_card_actual(card, sys);
			return;
		}
	}
	if (get_mouse_right())
	{
		sys.gui_state = gui_state::player_turn;
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
			sys.gui_state = gui_state::player_turn;
		}
	}
	else
	{
		sys.gui_state = gui_state::player_turn;
	}
}
void handle_enemy_turn(console& con, game_systems& sys)
{
	sys.wounds_added->render(con);
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
		state_set_to_player(sys);
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
			anim->player_wound_hand = sys.wounds_added;
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
void game_loop(console& graphics_console, console& text_console)
{
	auto& window = text_console.get_window();
	bool restart = false;
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
		card_hand wound_hand;
		wound_hand.hand_gui_y = view_h - 8;

		card_deck deck;
		deck.y = view_h - card::card_h - 10;
		deck.x =  -card::card_w+3;
		deck.text = "Deck";

		card_deck discard;
		discard.y = view_h - card::card_h - 10;
		discard.x = view_w - card::card_w /4;
		discard.text = "Discard";

		sys.hand = &hand;
		sys.wounds_added = &wound_hand;
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

			enemy->pos = v2i(map_w / 2 - 15, map_h / 2);
			enemy->glyph = 'G';
			enemy->dmg = 3;
			enemy->move = 1;
			enemy->color_fore = v3f(0.2f, 0.8f, 0.1f);
			enemy->type = entity_type::enemy;

			world.entities.emplace_back(enemy);
		}

		for (int i = 0; i<15; i++)
			discard.cards.push_back(strike_card());
		for (int i = 0; i<8; i++)
			discard.cards.push_back(push_card());
		state_set_to_player(sys);
		//int size_min = std::min(map_w, map_h);
		v2i center = v2i(map_w, map_h) / 2;
		restart = false;
		v2i from_click;
		v2i last_mouse;

		while (!restart && window.isOpen())
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
						if (sys.gui_state == gui_state::exiting)
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
					}
					if (event.key.code == sf::Keyboard::Space)
					{
						current_state = states.next_state(current_state);
						if (sys.gui_state == gui_state::player_turn)
						{
							//TODO: set_state_enemy
							sys.gui_state = gui_state::enemy_turn;
							auto& dis = sys.discard->cards;
							auto& hand = sys.hand->cards;
							dis.insert(dis.end(), hand.begin(), hand.end());
							hand.clear();
							remove_dead_enemies(sys);
						}
					}
					if (event.key.code == sf::Keyboard::S)
						save_map(world);
					if (event.key.code == sf::Keyboard::L)
						load_map(world);

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
    console text_console(view_w, view_h, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_Paul_10x10), EMB_FILE_SIZE_Paul_10x10, 10, 10));
	console& graphics_console = text_console;
	//console graphics_console(&text_console,(view_w /10)*12, (view_h / 10) * 12, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_cp437_12x12), EMB_FILE_SIZE_cp437_12x12, 12, 12));

	game_loop(graphics_console,text_console);
  
    return EXIT_SUCCESS;
}