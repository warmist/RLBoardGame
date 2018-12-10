
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

#include <array>

using std::string;
//900/12=85
const int map_w = 200;
const int map_h = 200;
const int view_w = 75;
const int view_h = 75;

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
	return v2i(p.x,p.y);
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
	normal,
	selecting_target,
	exiting,
	animating,
};
/*enum class animation_type
{
	walk,
};*/
const float animation_max_time = 0.5f;

struct animation_data
{
	//animation_type type;
	int step;
	sf::Clock anim_clock;

	std::vector<v2i> walk_path;
	entity* walker;

	void start_animation()
	{
		step = 0;
		anim_clock.restart();
	}
	int max_steps()
	{
		return (int)walk_path.size();
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
			const auto& p = walk_path[walk_path.size()-step-1];
			step++;

			walker->x = p.x;
			walker->y = p.y;
		}
	}
};

enum class card_type
{
	action,
	generated,
	attack,
};
enum class card_needs
{
	nothing,
	target_unit,
	empty_square,
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
struct card_needs_data //TODO: duplication/mutable state in card? need better/nicer way to store input/output stuff
{
	//output
	int x, y;
	//input
	float distance;

	animation_data animation;
};
struct card;
class e_player;
using card_action = card_fate(*)(card& ,e_player& , map&, card_needs_data* );

constexpr float phi = 1.61803398874989484820f;
struct card
{
	std::string name;
	std::string desc;
	static const int card_w = 15;
	static const int card_h= int(card_w*phi);
	
	card_type type=card_type::action;
	//what to ask when using the card
	card_needs needs=card_needs::nothing;
	card_needs_data needs_data; //TODO: UGH...
	//what to do when card is used
	card_action use_callback = nullptr;

	void render(console& c,int x, int y)
	{
		v3f border_color;
		if (type == card_type::generated)
			border_color = { 0.08f, 0.04f, 0.37f };
		else if (type == card_type::action)
			border_color = { 0.57f, 0.44f, 0.07f };
		else if (type == card_type::attack)
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

		const int desc_start_y = y + 1;
		c.set_text(v2i(x + 2, desc_start_y), desc);
	}
};
struct card_deck
{
	std::vector<card> cards;
	int x, y;
	std::string text;
	void render(console& c)
	{
		v3f border_color = v3f(1, 1, 1);
		const int w = card::card_w;
		const int h = card::card_h;

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
		c.set_text(v2i(x + text_start, y+4), text);
		//c.set_char(v2i(x + text_start + int(name.length()), y), tile_nse_t_double, border_color);
		//c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color);

		const int desc_start_y = y + 8;
		std::string desc = "No.: " + std::to_string(cards.size());
		c.set_text(v2i(x + 2, desc_start_y), desc);
	}
};
struct card_hand
{
	std::vector<card> cards;
	int selected_card=-1;
	int hand_gui_y;
	v2i get_card_extents(int y,int card_no)
	{
		bool is_crouded = cards.size()*card::card_w >= view_w;
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
class e_player :public entity
{
public:
	virtual const char* name() const { return "player_figurine"; }
	int actions_per_turn= 2;
	int current_action = 1;
	int max_hp = 20;
	int current_hp = 20;
	void render_gui(console& c, int x, int y)
	{
		c.set_text(v2i(x, y),   "Health: ");
		c.set_text(v2i(x, y+1), "Actions:");

		c.set_text(v2i(x + 10, y), std::to_string(current_hp) + "/" + std::to_string(max_hp));

		int dx = 0;
		for (int i = 0; i < actions_per_turn-current_action; i++)
			c.set_char(v2i(x + 10 + (dx++), y + 1), (unsigned char)'\xad', v3f(0.4f, 0.4f, 0.4f));
		for (int i = 0; i < current_action; i++)
			c.set_char(v2i(x + 10 + (dx++), y + 1), (unsigned char)'\xad', v3f(1, 1, 1));

		//c.set_text(v2i(x + 10, y+1), std::to_string(current_action) + "/" + std::to_string(actions_per_turn));
	}
};
card default_move_action()
{
	card ret;
	ret.name = "Run";
	ret.desc = "Cost      \xad\n\nMove\nRange      5\n\n\n\nRun to\nthe target";
	ret.type = card_type::generated;
	ret.needs = card_needs::empty_square;
	ret.needs_data = { 0,0,5 };
	return ret;
}

void handle_card_use(bool click,card_hand& hand,map& m,e_player& p,gui_state& current_state,card_needs_data& cur_needs)
{
	if (click && hand.selected_card != -1)
	{

		auto& card = hand.cards[hand.selected_card];
		if (card.needs != card_needs::nothing)
		{
			current_state = gui_state::selecting_target;
			cur_needs = card.needs_data;
			return;
		}
		if (card.use_callback)
		{
			auto ret=card.use_callback(card, p,m,nullptr );
			switch (ret)
			{
			case card_fate::destroy:
				hand.cards.erase(hand.cards.begin() + hand.selected_card);
				hand.selected_card = -1;
				break;
			case card_fate::discard:
				//todo
				break;
			case card_fate::hand:
				//todo
				break;
			case card_fate::draw_pile_top:
				//todo
				break;
			case card_fate::draw_pile_rand:
				//todo
				break;
			case card_fate::draw_pile_bottom:
				//todo
				break;
			default:
				break;
			}
		}
	}
}
card_fate strike_action(card&, e_player&, map&, card_needs_data*)
{
	return card_fate::destroy;
}
void game_loop(console& graphics_console, console& text_console)
{
	auto& window = text_console.get_window();
	bool restart = false;
	float time = 0;
	e_player* player = nullptr;
	while (window.isOpen())
	{
		state_map states;
		states.init_default_states();

		state current_state = states.data["game_start"];

		card_hand hand;
		card_deck deck;
		deck.y = view_h - card::card_h - 10;
		deck.x = 0;// -card::card_w / 2;
		deck.text = "Deck";

		card_deck discard;
		discard.y = view_h - card::card_h - 10;
		discard.x = view_w - card::card_w / 2;
		discard.text = "Discard";

		hand.hand_gui_y = view_h - 8;
		card t_card;
		t_card.name = "Strike";
		t_card.desc = "Cost      \xad\n\nAttack\nRange      0\nDmg      1D6\n\n\n\nPerform an\nattack with\na very big\nsword";
		t_card.type = card_type::attack;
		t_card.use_callback = strike_action;
		for(int i=0;i<3;i++)
			hand.cards.push_back(t_card);
		t_card.name = "Push";
		t_card.desc = "Cost      \xad\n\nAction\nRange      0\nDistance 1D4\n\n\n\nForce a\nmove";
		t_card.type = card_type::action;
		hand.cards.push_back(t_card);
		hand.cards.push_back(default_move_action());
		auto world = map(map_w, map_h);
		dbg_init_world(world);

		std::unique_ptr<e_player> ptr_player;
		ptr_player.reset(new e_player);
		player = ptr_player.get();

		player->x = map_w / 2;
		player->y = map_h / 2;
		player->glyph = '@';
		player->color_fore = v3f(1, 1, 1);

		world.entities.emplace_back(std::move(ptr_player));

		//int size_min = std::min(map_w, map_h);
		v2i center = v2i(map_w, map_h) / 2;
		//set_room(world, center.x-15, center.y-15, 30, 30, 0);
		/*auto core = make_furniture(furniture_type::ai_core);
		core->pos = v2i(center.x - 3, center.y - 3);
		core->removed = false;
		world.furniture.emplace_back(std::move(core));*/
		//make_worker(world, center.x, center.y);
		restart = false;
		v2i from_click;
		v2i last_mouse;

		int ticks_to_do = 0;

		recti map_window = { 0,0,view_w,view_h };
		v2i map_view_pos = { view_w / 2,view_h / 2 };

		bool mouse_down;
		gui_state gui_state=gui_state::normal;
		card_needs_data cur_needs = { 0 };

		while (!restart && window.isOpen())
		{
			// Process events
			sf::Event event;
			mouse_down = false;
			while (window.pollEvent(event))
			{
				//NOTE: changes here need update in docs!
				// Close window: exit
				if (event.type == sf::Event::Closed)
					window.close();

				if (event.type == sf::Event::KeyPressed)
				{
					// Escape key: exit
					if (event.key.code == sf::Keyboard::Escape)
					{
						if (gui_state == gui_state::exiting)
							window.close();
						else if (gui_state == gui_state::normal)
							gui_state = gui_state::exiting;
						else
							gui_state = gui_state::normal;
					}
					else
					{
						if (gui_state == gui_state::exiting)
						{
							gui_state = gui_state::normal;
						}
					}
					if (event.key.code == sf::Keyboard::Space)
					{
						current_state = states.next_state(current_state);
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
						map_view_pos -= delta;
					}
					last_mouse = cur_mouse;
				}
				if (event.type == sf::Event::MouseButtonPressed)
				{
					if(event.mouseButton.button== sf::Mouse::Left)
					{
						mouse_down = true;
					}
					
				}
			}

			graphics_console.clear();
			world.render(graphics_console,map_window, map_view_pos);
			player->render_gui(graphics_console, 0, 0);

			if(gui_state==gui_state::normal)
			{
				hand.input(graphics_console);
				handle_card_use(mouse_down,hand,world,*player,gui_state, cur_needs);
				deck.render(graphics_console);
				discard.render(graphics_console);
				hand.render(graphics_console);
			}
			else if (gui_state == gui_state::exiting)
			{
				int exit_h = view_h / 2;
				graphics_console.set_text_centered(v2i(view_w / 2, exit_h), "Are you sure you want to exit?",v3f(0.6f,0,0));
				graphics_console.set_text_centered(v2i(view_w / 2, exit_h+1), "(press esc to confirm, anything else - cancel)");
			}
			else if (gui_state == gui_state::selecting_target)
			{
				world.pathfind_field(v2i(player->x, player->y), cur_needs.distance); //TODO: do this only once when player position changes, or card has been selected
				//render range highlight
				world.render_reachable(graphics_console, map_window, map_view_pos, v3f(0.1f, 0.2f, 0.5f));
				//render mouse/path to target
				v2i mouse_pos = get_mouse(graphics_console);
				auto path = world.get_path(mouse_pos + map_view_pos);
				v3f color_fail = v3f(0.8f, 0.2f, 0.4f);
				world.render_path(graphics_console, path, map_window, map_view_pos, v3f(0.3f, 0.7f, 0.2f), color_fail);

				if (path.size() == 0)
					graphics_console.set_back(mouse_pos, color_fail);
				//render selected card
				auto id = hand.selected_card;
				if (id != -1 && id < hand.cards.size())
				{
					auto& card = hand.cards[id];
					card.render(graphics_console, view_w/2 - card::card_w / 2, view_h - card::card_h);
				}
				if (mouse_down)
				{
					if (path.size() != 0)
					{
						gui_state = gui_state::animating;
						cur_needs.animation.start_animation();
						cur_needs.animation.walk_path = path;
						cur_needs.animation.walker = player;
					}
				}
			}
			else if (gui_state == gui_state::animating)
			{
				cur_needs.animation.animate_step();

				if (cur_needs.animation.done_animation())
					gui_state = gui_state::normal;
			}
			text_console.set_text_centered(v2i(view_w / 2, view_h - 1), current_state.name, v3f(0.4f, 0.5f, 0.5f));
			//draw_asciimap(graphics_console);
			graphics_console.render();
			//if(&text_console!=&graphics_console)
			//	text_console.render();
			// Finally, display the rendered frame on screen
			window.display();
		}
	}
}



int main(int argc,char **argv)
{
    console text_console(view_w, view_h, font_descriptor(reinterpret_cast<const unsigned char*>(EMB_FILE_cp437_12x12), EMB_FILE_SIZE_cp437_12x12, 12, 12));
	console& graphics_console = text_console;
	//console graphics_console(&text_console,view_w * 2, view_h * 2, font_descriptor(Zaratustra_custom_5x5, array_size(Zaratustra_custom_5x5), 5, 5));

	game_loop(graphics_console,text_console);
  
    return EXIT_SUCCESS;
}