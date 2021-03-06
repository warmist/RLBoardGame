#include "console.hpp"
#include "version.hpp"
#include <SFML\Graphics\Shader.hpp>

void console::load_font(const font_descriptor& data)
{
	sf::Image image;
	if (data.is_memory_file)
		image.loadFromMemory(data.data, data.data_size);
	else
		image.loadFromFile(data.path);
	image.createMaskFromColor(sf::Color::Magenta);
	font_texture_.loadFromImage(image);
	glypth_bounds_.left = glypth_bounds_.top = 0;
	glypth_bounds_.width = float(data.tile_w);
	glypth_bounds_.height = float(data.tile_h);
}
console::console(console* parent, int w, int h, font_descriptor data, float scale) :parent_(parent),w_(w), h_(h), font_data_(data), scale_x_(scale), scale_y_(scale)
{
	load_font(data);

	tiles_.resize(w, h);
	fore_.resize(w, h);
	back_.resize(w, h);
}
console::console(int w, int h, font_descriptor data,float scale) :w_(w),h_(h),font_data_(data),scale_x_(scale),scale_y_(scale)
{
	load_font(data);
    //auto glypt_w_ = abs(glypth_bounds_.left - glypth_bounds_.width);
    //auto glypt_h_ = abs(glypth_bounds_.top - glypth_bounds_.height);
    //TODO: random gen edition of tacticus. Use boosters, boards etc as input seed
    window_.create(sf::VideoMode(unsigned int(glypth_bounds_.width*w*scale_x_), unsigned int(glypth_bounds_.height *h*scale_y_)), "Tacticus. Version:" TACTICUS_VERSION, sf::Style::Titlebar | sf::Style::Close);
    window_.setFramerateLimit(20);

	tiles_.resize(w,h);
	fore_.resize(w,h);
	back_.resize(w,h);
}

void console::draw_box(recti box, bool double_line, v3f foreground, v3f background)
{
	int w = box.width();
	int h = box.height();
	int x = box.x();
	int y = box.y();

	int hl = (double_line) ? (tile_hline_double) : (tile_hline_single);
	int vl = (double_line) ? (tile_vline_double) : (tile_vline_single);
	for (int i = 0; i < w; i++)
	{
		for (int j = 0; j < h; j++)
		{
			set_char(v2i(i + x, j + y), ' ', foreground, background);
		}
	}

	for (int i = 0; i < w; i++)
	{
		set_char(v2i(x + i, y), hl, foreground, background);
		set_char(v2i(x + i, y + h - 1), hl, foreground, background);
	}
	for (int i = 0; i < h; i++)
	{
		set_char(v2i(x, y + i), vl, foreground, background);
		set_char(v2i(x + w - 1, y + i), vl, foreground, background);
	}

	set_char(v2i(x, y), (double_line) ? (tile_es_corner_double) : (tile_es_corner_single), foreground, background);
	set_char(v2i(x + w - 1, y), (double_line) ? (tile_ws_corner_double) : (tile_ws_corner_single), foreground, background);
	set_char(v2i(x, y + h - 1), (double_line) ? (tile_ne_corner_double) : (tile_ne_corner_single), foreground, background);
	set_char(v2i(x + w - 1, y + h - 1), (double_line) ? (tile_wn_corner_double) : (tile_wn_corner_single), foreground, background);
}

void console::render()
{
	sf::RenderWindow* w = &window_;
	if (parent_)
		w = &parent_->window_;
	else
		w->clear();
    auto glypt_w_ = glypth_bounds_.width;
    auto glypt_h_ = glypth_bounds_.height;
	//Draw background first
	sf::RectangleShape background(sf::Vector2f(glypt_w_, glypt_h_));
	for (int x = 0; x < tiles_.w; ++x)
	{
		for (int y = 0; y < tiles_.h; ++y)
		{
			if (tiles_(x, y) == 0)
				continue;
			background.setFillColor(back_(x,y));
			background.setPosition(x*glypt_w_*scale_x_,y*glypt_h_*scale_y_);
            background.setScale(scale_x_, scale_y_);
			w->draw(background);
		}
	}

    sf::RectangleShape char_rect(sf::Vector2f(glypt_w_, glypt_h_));
    char_rect.setTexture(&font_texture_);
    sf::Glyph glypt;
	for (int x = 0; x < tiles_.w; ++x)
	{
		for (int y = 0; y < tiles_.h; ++y)
		{
            auto tile = tiles_(x,y);
            if (tile)
            {
                glypt.advance = 0;
                glypt.bounds = sf::FloatRect(0, 0, glypt_w_, glypt_h_);
                glypt.textureRect = sf::IntRect(int((tile % 16)*glypt_w_), int((tile / 16)*glypt_h_), int(glypt_w_), int(glypt_h_));

                char_rect.setTextureRect(glypt.textureRect);
                char_rect.setPosition(x*glypt_w_*scale_x_, y*glypt_h_*scale_y_);
                char_rect.setFillColor(fore_(x , y));
                char_rect.setScale(scale_x_, scale_y_);
                w->draw(char_rect); 
            }
		}
	}
}