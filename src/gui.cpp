#include "gui.hpp"
#include "console.hpp"

int key_to_char(sf::Keyboard::Key k)
{
	if (k >= sf::Keyboard::Key::A && k <= sf::Keyboard::Key::Z)
		return 'a' + k;
	else
	{
		switch (k)
		{
		default:
		case sf::Keyboard::Unknown:
			return '?';
		case sf::Keyboard::Num0:
			break;
		case sf::Keyboard::Num1:
			break;
		case sf::Keyboard::Num2:
			break;
		case sf::Keyboard::Num3:
			break;
		case sf::Keyboard::Num4:
			break;
		case sf::Keyboard::Num5:
			break;
		case sf::Keyboard::Num6:
			break;
		case sf::Keyboard::Num7:
			break;
		case sf::Keyboard::Num8:
			break;
		case sf::Keyboard::Num9:
			break;
		case sf::Keyboard::Escape:
			break;
		case sf::Keyboard::LControl:
			break;
		case sf::Keyboard::LShift:
			break;
		case sf::Keyboard::LAlt:
			break;
		case sf::Keyboard::LSystem:
			break;
		case sf::Keyboard::RControl:
			break;
		case sf::Keyboard::RShift:
			break;
		case sf::Keyboard::RAlt:
			break;
		case sf::Keyboard::RSystem:
			break;
		case sf::Keyboard::Menu:
			break;
		case sf::Keyboard::LBracket:
			return '[';
			break;
		case sf::Keyboard::RBracket:
			return ']';
			break;
		case sf::Keyboard::SemiColon:
			break;
		case sf::Keyboard::Comma:
			break;
		case sf::Keyboard::Period:
			break;
		case sf::Keyboard::Quote:
			break;
		case sf::Keyboard::Slash:
			break;
		case sf::Keyboard::BackSlash:
			break;
		case sf::Keyboard::Tilde:
			break;
		case sf::Keyboard::Equal:
			break;
		case sf::Keyboard::Dash:
			break;
		case sf::Keyboard::Space:
			break;
		case sf::Keyboard::Return:
			break;
		case sf::Keyboard::BackSpace:
			break;
		case sf::Keyboard::Tab:
			break;
		case sf::Keyboard::PageUp:
			break;
		case sf::Keyboard::PageDown:
			break;
		case sf::Keyboard::End:
			break;
		case sf::Keyboard::Home:
			break;
		case sf::Keyboard::Insert:
			break;
		case sf::Keyboard::Delete:
			break;
		case sf::Keyboard::Add:
			break;
		case sf::Keyboard::Subtract:
			break;
		case sf::Keyboard::Multiply:
			break;
		case sf::Keyboard::Divide:
			break;
		case sf::Keyboard::Left:
			break;
		case sf::Keyboard::Right:
			break;
		case sf::Keyboard::Up:
			break;
		case sf::Keyboard::Down:
			break;
		case sf::Keyboard::Numpad0:
			break;
		case sf::Keyboard::Numpad1:
			break;
		case sf::Keyboard::Numpad2:
			break;
		case sf::Keyboard::Numpad3:
			break;
		case sf::Keyboard::Numpad4:
			break;
		case sf::Keyboard::Numpad5:
			break;
		case sf::Keyboard::Numpad6:
			break;
		case sf::Keyboard::Numpad7:
			break;
		case sf::Keyboard::Numpad8:
			break;
		case sf::Keyboard::Numpad9:
			break;
		case sf::Keyboard::F1:
			break;
		case sf::Keyboard::F2:
			break;
		case sf::Keyboard::F3:
			break;
		case sf::Keyboard::F4:
			break;
		case sf::Keyboard::F5:
			break;
		case sf::Keyboard::F6:
			break;
		case sf::Keyboard::F7:
			break;
		case sf::Keyboard::F8:
			break;
		case sf::Keyboard::F9:
			break;
		case sf::Keyboard::F10:
			break;
		case sf::Keyboard::F11:
			break;
		case sf::Keyboard::F12:
			break;
		case sf::Keyboard::F13:
			break;
		case sf::Keyboard::F14:
			break;
		case sf::Keyboard::F15:
			break;
		case sf::Keyboard::Pause:
			break;
		}
	}
	return '?';
}

v2i to_char_coords(float x, float y, const console & c)
{
	v2i input = v2i(int(round(x / c.font_data_.tile_w)), int(round(y / c.font_data_.tile_h)));
	v2i ret;
	ret.x = clamp(input.x, 0, c.w_);
	ret.y = clamp(input.y, 0, c.h_);
	return ret;
}

inline void button::render(console & c)
{
	v3f h_col = v3f(0.68f, 0.68f, 0.01f);
	v3f l_col = v3f(0.6f, 0.6f, 0.6f);

	v3f col = highlight ? h_col : l_col;

	c.set_text(pos, label, col, v3f(0.1f, 0.1f, 0.1f));
	if (key != sf::Keyboard::Unknown)
	{
		c.set_text(pos + v2i(int(label.size()), 0), " ( )", col, v3f(0.1f, 0.1f, 0.1f));
		c.set_char(pos + v2i(int(label.size()) + 2, 0), key_to_char(key), v3f(0, 0, 0.8f), v3f(0.1f, 0.1f, 0.1f));
	}

}

inline void button::update(console & c)
{
	auto mpos = sf::Mouse::getPosition(c.get_window());
	auto mcell = v2i(int(round(mpos.x / 10)), int(round(mpos.y / 10)));
	int len = int(label.size());
	if (key != sf::Keyboard::Unknown)
	{
		len += 4;
	}

	if (mcell.x >= pos.x && mcell.x < pos.x + len && mcell.y == pos.y)
		highlight = true;
	else
		highlight = false;

	if (sf::Mouse::isButtonPressed(sf::Mouse::Left) ||
		(key != sf::Keyboard::Unknown && sf::Keyboard::isKeyPressed(key)))
	{
		if (callback && !down)
			callback();
		down = true;
		highlight = true;
	}
	else
	{
		down = false;
	}
}
