#include "console.hpp"
#include "version.hpp"
#include <SFML\Graphics\Shader.hpp>
const int char_size=32;
void console::load_font(const font_descriptor& data)
{
	if (data.is_png)
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
	else
	{
		font_.loadFromFile(data.path);
		auto glyph = font_.getGlyph('A', char_size, false, 0);
		glypth_bounds_ = glyph.bounds;
		font_texture_ = font_.getTexture(char_size);
	}
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
                if (font_data_.is_png)
                {
                    glypt.advance = 0;
                    glypt.bounds = sf::FloatRect(0, 0, glypt_w_, glypt_h_);
                    glypt.textureRect = sf::IntRect(int((tile % 16)*glypt_w_), int((tile / 16)*glypt_h_), int(glypt_w_), int(glypt_h_));
                }
                else
                    glypt = font_.getGlyph(tile, char_size, false);

                char_rect.setTextureRect(glypt.textureRect);
                char_rect.setPosition(x*glypt_w_*scale_x_, y*glypt_h_*scale_y_);
                char_rect.setFillColor(fore_(x , y));
                char_rect.setScale(scale_x_, scale_y_);
                w->draw(char_rect); 
            }
		}
	}
}