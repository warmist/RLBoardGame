#pragma once
#include <memory>

#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "common.hpp"
/*
	TODO: 
		* remove font based rendering
		* remake with opengl/shader based thing

*/
typedef std::basic_string<unsigned char> ustring;

struct font_descriptor{
    std::string path;
    bool is_png = false;
    bool is_memory_file = false;
    const unsigned char* data = nullptr;
    size_t data_size = 0;
    int tile_w;
    int tile_h;
    font_descriptor(const unsigned char* data, size_t size, int tile_w, int tile_h) :is_png(true),is_memory_file(true),data(data), data_size(size), tile_w(tile_w), tile_h(tile_h)
    {
    }
    font_descriptor(std::string path, int tile_w, int tile_h) :path(path), is_png(true), tile_w(tile_w), tile_h(tile_h)
    {

    }
    font_descriptor(std::string path) :path(path), is_png(false)
    {

    }
};
struct console
{
private:
	sf::RenderWindow window_;
    
	sf::Font font_;
    sf::Texture font_texture_;
    sf::FloatRect glypth_bounds_;

    dyn_array2d<uint32_t> tiles_;
    dyn_array2d<sf::Color> fore_;
    dyn_array2d<sf::Color> back_;
    float scale_x_ = 2.0f;
    float scale_y_ = 2.0f;
    
	console* parent_ = nullptr;

	void load_font(const font_descriptor& data);
public:
	font_descriptor font_data_;
    int w_,h_;

    console(int w, int h, font_descriptor data,float scale=1.0f);
	console(console* parent,int w, int h, font_descriptor data, float scale = 1.0f);

	sf::RenderWindow& get_window()
	{
		if (parent_)
			return parent_->get_window();
		return window_;
	}
	void set_fore(v2i pos, v3f foreground)
	{
		foreground *= 255;
		fore_(pos.x, pos.y) = sf::Color(sf::Uint8(foreground.x), sf::Uint8(foreground.y), sf::Uint8(foreground.z));
	}
	void set_back(v2i pos, v3f background)
	{
		background *= 255;
		back_(pos.x, pos.y) = sf::Color(sf::Uint8(background.x), sf::Uint8(background.y), sf::Uint8(background.z));
	}
	void set_char(v2i pos, int chr, v3f foreground = v3f(1,1,1), v3f background = v3f(0, 0, 0))
	{
		foreground *= 255;
		background *= 255;
		tiles_(pos.x, pos.y) = chr;
		fore_(pos.x, pos.y) = sf::Color(sf::Uint8(foreground.x), sf::Uint8(foreground.y), sf::Uint8(foreground.z));
		back_(pos.x, pos.y) = sf::Color(sf::Uint8(background.x), sf::Uint8(background.y), sf::Uint8(background.z));
	}
	bool is_valid_pos(v2i pos)
	{
		return pos.x >= 0 && pos.y >= 0 && pos.x < w_ && pos.y < h_;
	}
	void set_fore_safe(v2i pos, v3f foreground)
	{
		if (!is_valid_pos(pos))
			return;
		foreground *= 255;
		fore_(pos.x, pos.y) = sf::Color(sf::Uint8(foreground.x), sf::Uint8(foreground.y), sf::Uint8(foreground.z));
	}
	void set_back_safe(v2i pos, v3f background)
	{
		if (!is_valid_pos(pos))
			return;
		background *= 255;
		back_(pos.x, pos.y) = sf::Color(sf::Uint8(background.x), sf::Uint8(background.y), sf::Uint8(background.z));
	}
	void set_char_safe(v2i pos, int chr, v3f foreground = v3f(1, 1, 1), v3f background = v3f(0, 0, 0))
	{
		if (!is_valid_pos(pos))
			return;
		foreground *= 255;
		background *= 255;
		tiles_(pos.x, pos.y) = chr;
		fore_(pos.x, pos.y) = sf::Color(sf::Uint8(foreground.x), sf::Uint8(foreground.y), sf::Uint8(foreground.z));
		back_(pos.x, pos.y) = sf::Color(sf::Uint8(background.x), sf::Uint8(background.y), sf::Uint8(background.z));
	}
	void invert_tile(v2i pos,bool do_back)
	{
		auto&f = fore_(pos.x, pos.y);
		f.r = 255 - f.r;
		f.g = 255 - f.g;
		f.b = 255 - f.b;

		if (do_back)
		{
			auto &b = back_(pos.x, pos.y);
			b.r = 255 - b.r;
			b.g = 255 - b.g;
			b.b = 255 - b.b;
		}
	}
	void set_text(v2i pos, const std::string& s, v3f foreground = v3f(1,1,1), v3f background = v3f(0, 0, 0))
	{
		for (int i = 0; i < s.size() && i + pos.x<tiles_.w; i++)
		{
			set_char(pos + v2i(i, 0), static_cast<unsigned char>(s[i]), foreground, background);
		}
	}
	void set_text_centered(v2i center_pos, const std::string& s, v3f foreground = v3f(1, 1, 1), v3f background = v3f(0, 0, 0))
	{
		set_text(center_pos - v2i(int(s.length()) / 2, 0), s, foreground, background);
	}
	/*
    void set_fore(sf::Vector2i pos, sf::Color foreground)
    {
        fore_(pos.x, pos.y) = foreground;
    }
    void set_back(sf::Vector2i pos, sf::Color background)
    {
        back_(pos.x, pos.y) = background;
    }
	void set_char(sf::Vector2i pos,int chr,sf::Color foreground=sf::Color(255,255,255),sf::Color background=sf::Color(0,0,0))
	{
		tiles_(pos.x,pos.y)=chr;
        fore_(pos.x, pos.y) = foreground;
        back_(pos.x, pos.y) = background;
	}
    void set_text(sf::Vector2i pos, const std::string& s, sf::Color foreground = sf::Color(255, 255, 255), sf::Color background = sf::Color(0, 0, 0))
    {
        for (int i = 0; i < s.size() && i+pos.x<tiles_.w; i++)
        {
            set_char(pos + sf::Vector2i(i, 0), static_cast<unsigned char>(s[i]), foreground, background);
        }
    }
    void set_text_centered(sf::Vector2i center_pos, const std::string& s, sf::Color foreground = sf::Color(255, 255, 255), sf::Color background = sf::Color(0, 0, 0))
    {
        set_text(center_pos-sf::Vector2i(int(s.length())/2,0), s, foreground, background);
    }
	*/
    void clear()
    {
        tiles_.data.assign(tiles_.data.size(), ' ');
    }
	void clear_transperent()
	{
		tiles_.data.assign(tiles_.data.size(), 0);
	}
	void render();
};

//tile definitions, depends on tileset used

//hor lines
const int tile_hline_single = 12 * 16 + 4;
const int tile_vline_single = 11 * 16 + 3;

const int tile_hline_double = 12 * 16 + 13;
const int tile_vline_double = 11 * 16 + 10;
//corners
const int tile_ws_corner_single = 11 * 16 + 15;
const int tile_ne_corner_single = 12 * 16 + 0;
const int tile_wn_corner_single = 13 * 16 + 9;
const int tile_es_corner_single = 13 * 16 + 10;

const int tile_ws_corner_double = 11 * 16 + 11;
const int tile_ne_corner_double = 12 * 16 + 8;
const int tile_wn_corner_double = 11 * 16 + 12;
const int tile_es_corner_double = 12 * 16 + 9;
//t sections
const int tile_nsw_t_single = 11 * 16 + 4;
const int tile_new_t_single = 12 * 16 + 1;
const int tile_sew_t_single = 12 * 16 + 2;
const int tile_nse_t_single = 12 * 16 + 3;

const int tile_nsw_t_double = 11 * 16 + 9;
const int tile_new_t_double = 12 * 16 + 10;
const int tile_sew_t_double = 12 * 16 + 11;
const int tile_nse_t_double = 12 * 16 + 12;
//other
const int tile_cross_single = 12 * 16 + 5;

const int tile_cross_double = 12 * 16 + 14;

const int tile_light_hatching = 11 * 16 + 0;
const int tile_medium_hatching = 11 * 16 + 1;
const int tile_heavy_hatching = 11 * 16 + 1;

const int tile_infinity = 236;