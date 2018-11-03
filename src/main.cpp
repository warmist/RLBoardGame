
#include "common.hpp"
#include "console.hpp"

#include "map.hpp"
#include "room.hpp"
#include <functional>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
//assets
#include "asset_cp437_12x12.hpp"

#include <array>

#include "worker.hpp"
#include "e_furniture.hpp"
const int map_w = 200;
const int map_h = 200;
const int view_w = 100;
const int view_h = 100;

std::uniform_int_distribution<int> r_color(0, 255);
std::mt19937_64 global_rand; //used for non important things

enum class cursor_mode {
	view,
	build_room,
};


struct building
{
	dyn_array2d<tile> tiles;
	void draw(console& c, int x, int y, int wx, int wy)
	{
		
		auto hx = (tiles.w + x > wx) ? (wx) : (tiles.w + x);
		auto hy = (tiles.h + y > wy) ? (wy) : (tiles.h + y);

		for (auto tx = x; tx < hx;x++)
			for (auto ty = y; ty < hy; y++)
		{
			auto &t = tiles(tx-x, ty-y);
			v2i pos(tx, ty);
			c.set_char(pos, t.glyph(), t.color_fore(), t.color_back());
		}
	}
};
void save_builing_file(const char* fname, const std::vector<building>& buildings)
{
	auto f = fopen(fname, "wb");
	fprintf(f, "%zu\n", buildings.size());

	for (auto& b : buildings)
	{
		fprintf(f, "%u %u\n", b.tiles.w, b.tiles.h);
		for (size_t i = 0; i < b.tiles.data.size(); i++)
		{
			fprintf(f, "%c", tile_attrs[static_cast<int>(b.tiles.data[i].t)].glyph);
			if ((i % b.tiles.w) == b.tiles.w - 1)
				fprintf(f, "\n");
		}
	}
	fclose(f);
}
tile_type find_tile(char c)
{
	//TODO: actually define saved tile char
	for (int i = 0; i < array_size(tile_attrs); i++)
		if (tile_attrs[i].glyph == c)
			return static_cast<tile_type>(i);
	assert(false);
	return tile_type::nothing;
}
bool load_builing_file(const char* fname, std::vector<building>& buildings)
{
	buildings.clear();
	auto f = fopen(fname, "rb");
	if (!f)
		return false;
	size_t count;
	fscanf(f, "%zu", &count);
	buildings.resize(count);

	for (auto& b : buildings)
	{
		unsigned w, h;
		fscanf(f, "%u %u", &w, &h);

		b.tiles.resize(w, h);
		for (size_t i = 0; i < b.tiles.data.size(); i++)
		{
			char c=0;
			fscanf(f, "%c", &c);
			b.tiles.data[i].t = find_tile(c);
			if (i%b.tiles.w == b.tiles.w - 1)
				fscanf(f, " ");
		}
		fread(b.tiles.data.data(), sizeof(tile_type), b.tiles.data.size(), f);
	}
	fclose(f);
	return true;
}



void draw_asciimap(console& con)
{
    for(int i=0;i<16;i++)
        for (int j = 0; j < 16; j++)
        {
            con.set_char(v2i(i, j), (i + j * 16));
        }
}
void set_tile(console& c, int x, int y, tile t,bool is_red)
{
	c.set_char_safe(v2i(x, y), t.glyph(), is_red?v3f(1.0f,0,0):t.color_fore(), t.color_back());
}
struct build_designation
{
	recti build_cursor;
	room_type current_type=room_type::ai_core;

	void draw(console& c, map& m,v2i world_offset)
	{
		tile floor = { tile_type::floor };
		tile wall = { tile_type::wall };

		auto bc = build_cursor;
	
		fix_neg_size(bc);

		auto world_build_cursor = bc;
		world_build_cursor.pos += world_offset;

		bool overlaps = !m.is_rect_room_empty(world_build_cursor);

		auto& start = bc.pos;
		auto end = bc.max_corner();

		auto is_valid = [&m](v2i p)
		{
			return p.x >= 0 && p.y >= 0 && p.x < m.static_layer.w && p.y < m.static_layer.h;
		};
		auto is_empty = [&m, is_valid](int x,int y) {
			if (is_valid(v2i(x,y)))
			{
				auto t = m.static_layer(x,y);
				if (t.t == tile_type::nothing)
					return true;
				return false;
			}
			else
				return false;
		};
		//draw room
		for (int x = start.x; x < end.x; x++)
			for (int y = start.y; y < end.y; y++)
			{
				auto wp = v2i(x, y) + world_offset;
				if (is_valid(wp))
					set_tile(c, x, y, floor, overlaps);
			}
		//decorate
		for (int x = start.x - 1; x < end.x + 1; x++)
		{

			if (is_empty(x + world_offset.x, start.y - 1 + world_offset.y))
				set_tile(c, x, start.y - 1, wall, overlaps);
			if (is_empty(x + world_offset.x, end.y + world_offset.y))
				set_tile(c, x, end.y, wall, overlaps);
		}
		for (int y = start.y - 1; y < end.y + 1; y++)
		{
			if (is_empty(start.x + world_offset.x - 1, y + world_offset.y))
				set_tile(c, start.x - 1, y, wall, overlaps);
			if (is_empty(end.x + world_offset.x, y + world_offset.y))
				set_tile(c, end.x, y, wall, overlaps);
		}
	}
	bool add_room(map& m,v2i world_offset)
	{
		auto bc = build_cursor;
		fix_neg_size(bc);

		auto world_build_cursor = bc;
		world_build_cursor.pos += world_offset;

		room rd;
		rd.floor = world_build_cursor;
		rd.type = current_type;
		auto ret = rd.place(m);
		if(ret)
			m.rooms.push_back(rd);
		return ret;
	}
private:
	void fix_neg_size(recti& r)
	{
		//the +=1 in these cases fixes designation so it's 'lower' corner is correctly pinned
		if (r.size.x < 0)
		{
			r.pos.x += r.size.x;
			r.size.x *= -1;
			r.size.x += 1;
		}
		if (r.size.y < 0)
		{
			r.pos.y += r.size.y;
			r.size.y *= -1;
			r.size.y += 1;
		}
	}
};
void set_room(map& m,int x,int y,int w,int h,int sth)
{
	build_designation bd;
	bd.build_cursor.pos = v2i(x, y);
	bd.build_cursor.size = v2i(w, h);
	bd.add_room(m, v2i(0, 0));
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

		cursor_mode cmode=cursor_mode::view;
		restart = false;
		bool paused = false;
		v2i from_click;
		v2i last_mouse;
		build_designation build_desig;

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
					if (event.key.code == sf::Keyboard::Tab)
					{
						if (cmode == cursor_mode::view)
							cmode = cursor_mode::build_room;
						else
							cmode = cursor_mode::view;
					}
					//TODO: add gui buttons for all things. That way you could see keybindings
					if (event.key.code == sf::Keyboard::LBracket)
					{
						build_desig.current_type = prev_looped(build_desig.current_type);
						//@REFACTOR: copy of code
						build_desig.build_cursor.size = get_room_specs(build_desig.current_type).min_size;
					}
					if (event.key.code == sf::Keyboard::RBracket)
					{
						build_desig.current_type = next_looped(build_desig.current_type);
						//@REFACTOR: copy of code
						build_desig.build_cursor.size = get_room_specs(build_desig.current_type).min_size;
					}
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
					if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
					{
						build_desig.build_cursor.size = get_room_specs(build_desig.current_type).min_size;
						build_desig.build_cursor.pos = cur_mouse;
					}
					else
					{
						//auto end = build_cursor.max_corner();
						auto& s = build_desig.build_cursor.size;
						s= (cur_mouse - build_desig.build_cursor.pos);
						//clamp to min_size
						auto min_size= get_room_specs(build_desig.current_type).min_size;
						//FIXME: big things (size>1) jump when dragged in neg dir
						if (abs(s.x) < min_size.x)s.x = sign_no_zero(s.x)*min_size.x;
						if (abs(s.y) < min_size.y)s.y = sign_no_zero(s.y)*min_size.y;

					}
					last_mouse = cur_mouse;
				}
				if (event.type == sf::Event::MouseButtonPressed)
				{
					from_click = last_mouse;
					build_desig.build_cursor.pos = last_mouse;
				}
				if (event.type == sf::Event::MouseButtonReleased)
				{
					auto to_click = last_mouse;
					if (event.mouseButton.button==sf::Mouse::Button::Left && cmode == cursor_mode::build_room)
					{
						build_desig.add_room(world, v2i(map_window_start_x, map_window_start_y));
						build_desig.build_cursor.size = v2i(0, 0);
					}
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

			if (cmode == cursor_mode::view)
			{
				graphics_console.invert_tile(v2i(last_mouse.x, last_mouse.y), true);
			}
			else if (cmode == cursor_mode::build_room)
			{
				build_desig.draw(graphics_console, world, v2i(map_window_start_x, map_window_start_y));

				//Draw current building type
				//TODO: where to put it?
				text_console.set_text(v2i(0, 0), get_room_specs(build_desig.current_type).name);
			}
			
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