#include "card.hpp"
#include "console.hpp"


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
inline v3f card::get_ap_color()
{
	const float warn_ap_max_t = 0.5;//in sec
	if (!is_warn_ap)
	{
		return v3f(1, 1, 1);
	}
	else
	{
		float tv = warn_ap.getElapsedTime().asSeconds();
		if (tv > warn_ap_max_t)
		{
			is_warn_ap = false;
			return v3f(1, 1, 1);
		}
		else
		{
			float t = tween_warn(tv / warn_ap_max_t);
			return v3f(1, 0, 0)*t + v3f(1, 1, 1)*(1 - t);
		}
	}
}

void card::render(console & c, int x, int y)
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

	c.draw_box(recti(x, y, w, h), true, border_color);

	const int text_start = int(w / 2 - name.length() / 2);
	c.set_text(v2i(x + text_start, y), name);
	c.set_char(v2i(x + text_start + int(name.length()), y), tile_nse_t_double, border_color);
	c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color);
	int cur_y = y + 1;
	if (type != card_type::wound) //TODO: replace with "usable" flag?
	{
		c.set_text(v2i(x + 2, cur_y), "Cost");
		for (int i = 0; i<cost_ap; i++)
			c.set_char(v2i(x + card::card_w - cost_ap + i - 2, cur_y), (unsigned char)'\xad', get_ap_color());
		cur_y++;
	}
	c.set_text_boxed(recti(x + 2, cur_y, w - 2, h - (cur_y - y) - 1), desc);
}
#include "lua_helper.hpp"
void lua_push_needs(lua_State* L, card* c, card_needs_output* data)
{
	lua_newtable(L);
	switch (c->needs.type)
	{
	case card_needs::walkable_path:
	{
		//lua_push_path(L,data->walkable_path)
		break;
	}
	case card_needs::visible_target_unit:
	{
		//lua_push_entity(L,data->visible_target_unit)
		break;
	}
	case card_needs::nothing:
	default:
		break;
	}
}
void card::use(lua_State* L, card_needs_output* out)
{
	//start a lua coroutine. 
	//This then can get yielded a few times until the effects are all "animated"
	lua_getregistry(L);
	lua_getfield(L, -1, "cards");
	lua_getfield(L, -1, key.c_str());
	int card_data_pos = lua_gettop(L);
	print_stack(L);
	lua_getfield(L, -1, "use");
	if (!lua_isfunction(L, -1))
	{
		return;
	}
	lua_pushvalue(L, card_data_pos); //copy card data (i.e. card class?)
	//TODO: sort of card class, not actual card data here :|
	//push lua table with this cards "data"

	//copy lua table with global systems (from registry)
	lua_getfield(L, card_data_pos-2, "system");
	
	//push required data from needs output
	lua_push_needs(L,this, out);
	print_stack(L);
	//call the function
	auto ret = lua_pcall(L, 3, LUA_MULTRET,0);
	if (ret != 0)
	{
		printf("Error:%s\n", lua_tostring(L, -1));
		assert(false);\
	}
}

void lua_load_card(lua_State* L, int arg, card& output)
{
	lua_stack_guard g(L);
	lua_pushvalue(L, arg);
	int p = lua_gettop(L);
	lua_getregistry(L);

	lua_getfield(L, -1, "cards");
	lua_pushvalue(L, p);
	lua_setfield(L, -2, output.key.c_str());
	lua_pop(L, 2);//pop reg.cards,registry

	lua_getfield(L, p, "name");
	output.name = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, p, "description");
	output.desc = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, p, "cost");
	output.cost_ap = (int)luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	//TODO: read tags
	//		use tags for card needs
	//		use tags for display too?
	lua_pop(L, 1);//pop copy of card
}
void lua_load_booster(lua_State * L,int arg,lua_booster& output)
{
	lua_stack_guard g(L,-1);
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		lua_pushvalue(L, -2); //copy key
		const char *key = lua_tostring(L, -1);
		
		card tmp;
		tmp.key = key;
		lua_load_card(L, -2, tmp);
		output[key] = tmp;
		lua_pop(L, 2);//pop key copy and value
		
	}
	lua_pop(L, 1);//pop 
}
