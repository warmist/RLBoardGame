
#include "common.hpp"
#include "console.hpp"

#include "map.hpp"
#include <unordered_set>
#include <functional>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
//assets
#include "asset_cp437_12x12.hpp"

#include <array>

using std::string;

const int map_w = 200;
const int map_h = 200;
const int view_w = 100;
const int view_h = 100;

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

void draw_asciimap(console& con)
{
    for(int i=0;i<16;i++)
        for (int j = 0; j < 16; j++)
        {
            con.set_char(v2i(i, j), (i + j * 16));
        }
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
}
struct card
{
	std::string name;
	std::string desc;

	void render(console& c,int x, int y)
	{
		const float phi = (1.f + sqrt(5.f)) / 2.f;
		const int h = 20;
		const int w = int(h*phi);
		

		for (int i = 0; i < w; i++)
		{
			for (int j = 0; j < h; j++)
			{
				c.set_char(v2i(i + x, j + y), ' ');
			}
		}

		for (int i = 0; i < w; i++)
		{
			c.set_char(v2i(x + i, y), tile_hline_double);
			c.set_char(v2i(x + i, y + h - 1), tile_hline_double);
		}
		for (int i = 0; i < h; i++)
		{
			c.set_char(v2i(x, y + i), tile_vline_double);
			c.set_char(v2i(x + w - 1, y + i), tile_vline_double);
		}
		c.set_char(v2i(x, y), tile_es_corner_double);
		c.set_char(v2i(x + w - 1, y), tile_ws_corner_double);
		c.set_char(v2i(x, y + h - 1), tile_ne_corner_double);
		c.set_char(v2i(x + w - 1, y + h - 1), tile_wn_corner_double);

		const int text_start = int(w / 2 - name.length() / 2);
		c.set_text(v2i(x + text_start, y), name);
		c.set_char(v2i(x + text_start + int(name.length()), y), tile_nse_t_double);
		c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double);

		const int desc_start_y = y + 2;
		c.set_text(v2i(x + 2, desc_start_y), desc);
	}
};

void game_loop(console& graphics_console, console& text_console)
{
	auto& window = text_console.get_window();
	bool restart = false;
	float time = 0;
	
	while (window.isOpen())
	{
		state_map states;
		states.init_default_states();

		state current_state = states.data["game_start"];

		auto world = map(map_w, map_h);
		dbg_init_world(world);
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
		int map_window_start_x = view_w/2;// player->x - view_w / 2;
		int map_window_start_y = view_h/2;// player->y - view_h / 2;
		while (!restart && window.isOpen())
		{
			// Process events
			sf::Event event;

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
						window.close();
					// space -> toggle pause
					if (event.key.code == sf::Keyboard::Space)
					{
						current_state = states.next_state(current_state);
					}
					if (event.key.code == sf::Keyboard::S)
						save_map(world);
					if (event.key.code == sf::Keyboard::L)
						load_map(world);
					// shift+r -> reset
					if (event.key.shift && (event.key.code == sf::Keyboard::R))
						restart = true;
					
				}
				if (event.type == sf::Event::MouseMoved)
				{
					auto cur_mouse = v2i(int(round(event.mouseMove.x / 12)), int(round(event.mouseMove.y / 12))); //FIXME: @hardcoded font size
					if (cur_mouse.x < 0)cur_mouse.x = 0;
					if (cur_mouse.y < 0)cur_mouse.y = 0;
					if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle))
					{
						auto delta = cur_mouse - last_mouse;
						map_window_start_x -= delta.x;
						map_window_start_y -= delta.y;
					}
					last_mouse = cur_mouse;
				}
			}
			//simulate world if that is needed
			/*
			world.tick();
			if (ticks_to_do > 0)
				ticks_to_do--;
			*/
			//drawing part
			//if (&text_console != &graphics_console)
			//	text_console.clear_transperent();
			graphics_console.clear(); //wtf?! not clearing stuff

			//FIXME: @GLITCH when draggin sometimes you see an empty rect? Probably not redrawing empty space?
			world.render(graphics_console, map_window_start_x, map_window_start_y, view_w, view_h, -map_window_start_x, -map_window_start_y);
			//gui
			card t_card;
			t_card.name = "Strike";
			t_card.desc = "Perform an attack with\nvery big sword";

			t_card.render(graphics_console, 4, 4);
			text_console.set_text_centered(v2i(view_w / 2, view_h - 1), current_state.name, v3f(0.4f, 0.5f, 0.5f));

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