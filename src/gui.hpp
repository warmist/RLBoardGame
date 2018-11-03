#pragma once

#include "SFML\Window.hpp"
#include "common.hpp"
#include <functional>

struct console;

int key_to_char(sf::Keyboard::Key k);
struct button
{
	std::string label;
	v2i pos;
	sf::Keyboard::Key key = sf::Keyboard::Unknown;

	std::function<void()> callback;

	bool highlight = false;
	bool down = false;

	void render(console& c);
	void update(console& c);
};
template<typename T>
T clamp(T v, T min, T max)
{
	return std::max(std::min(v, max), min);
}
v2i to_char_coords(float x, float y, const console& c);