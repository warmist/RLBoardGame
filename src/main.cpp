
#include "common.hpp"
#include "console.hpp"

#include "map.hpp"
#include <functional>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
//assets
#include "asset_cp437_12x12.hpp"

#include <array>

const int map_w = 200;
const int map_h = 200;
const int view_w = 100;
const int view_h = 100;

std::uniform_int_distribution<int> r_color(0, 255);
std::mt19937_64 global_rand; //used for non important things

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
	c.set_char_safe(v2i(x, y), t.glyph, is_red?v3f(1.0f,0,0):t.color_fore, t.color_back);
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
void game_loop(console& graphics_console, console& text_console)
{
	auto& window = text_console.get_window();
	bool restart = false;
	float time = 0;
	
	while (window.isOpen())
	{
		auto world = map(map_w, map_h);
		//int size_min = std::min(map_w, map_h);
		v2i center = v2i(map_w, map_h) / 2;
		//set_room(world, center.x-15, center.y-15, 30, 30, 0);
		/*auto core = make_furniture(furniture_type::ai_core);
		core->pos = v2i(center.x - 3, center.y - 3);
		core->removed = false;
		world.furniture.emplace_back(std::move(core));*/
		//make_worker(world, center.x, center.y);
		restart = false;
		bool paused = false;
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
						paused = !paused;
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
			if (!paused || ticks_to_do > 0)
			{
				world.tick();
				if (ticks_to_do > 0)
					ticks_to_do--;
			}

			//drawing part
			//if (&text_console != &graphics_console)
			//	text_console.clear_transperent();
			graphics_console.clear();

			//FIXME: @GLITCH when draggin sometimes you see an empty rect? Probably not redrawing empty space?
			world.render(graphics_console, map_window_start_x, map_window_start_y, view_w, view_h, -map_window_start_x, -map_window_start_y);
			//gui

			if (paused)
			{
				text_console.set_text_centered(v2i(view_w / 2, view_h - 1), "Stepping", v3f(0.4f, 0.5f, 0.5f));
			}
			else
			{
				text_console.set_text_centered(v2i(view_w / 2, view_h - 1), "Running", v3f(0.4f, 0.5f, 0.5f));
			}

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