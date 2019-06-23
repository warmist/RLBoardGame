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
	v3f back_color = v3f(0, 0, 0);
	if (selected)
		back_color = v3f(0.4f, 0.4f, 0.4f);
	/*
	const int h = 20;
	const int w = int(h*phi);
	*/
	const int w = card_w;
	const int h = card_h;

	c.draw_box(recti(x, y, w, h), true, border_color, back_color);

	const int text_start = int(w / 2 - name.length() / 2);
	c.set_text(v2i(x + text_start, y), name, v3f(1,1,1),back_color);
	c.set_char(v2i(x + text_start + int(name.length()), y), tile_nse_t_double, border_color, back_color);
	c.set_char(v2i(x + text_start - 1, y), tile_nsw_t_double, border_color, back_color);
	int cur_y = y + 1;
	if (cost_ap != 0)
	{
		c.set_text(v2i(x + 2, cur_y), "Cost",v3f(1,1,1),back_color);
		for (int i = 0; i<cost_ap; i++)
			c.set_char(v2i(x + card::card_w - cost_ap + i - 2, cur_y), (unsigned char)'\xad', get_ap_color(), back_color);
		cur_y++;
	}
	c.set_text_boxed(recti(x + 2, cur_y, w - 2, h - (cur_y - y) - 1), desc, v3f(1, 1, 1), back_color);
}
#include "lua_helper.hpp"
static lua_State* yieldable_func(lua_State* L,int card_id, const char* fname)
{
	lua_stack_guard g(L, 1);
	//start a lua coroutine. 
	//This then can get yielded a few times until the effects are all "animated"

	lua_rawgeti(L, LUA_REGISTRYINDEX, card_id);
	int card_data_pos = lua_gettop(L);
	lua_getfield(L, -1, fname);
	if (!lua_isfunction(L, -1))
	{
		g.offset = 0;
		lua_settop(L, 0);
		return nullptr;
	}
	lua_pushvalue(L, card_data_pos);
	//copy lua table with global systems (from registry)
	lua_getfield(L, LUA_REGISTRYINDEX, "system");

	auto L1 = lua_newthread(L);
	lua_stack_guard g1(L1, 3);
	lua_insert(L, lua_gettop(L) - 3); //move thread under args+function
	lua_xmove(L, L1, 3); //transfer everything needed for the fcall
	lua_pop(L, 1);
	return L1;
}
lua_State* card::yieldable_use(lua_State* L)
{
	return yieldable_func(L, my_id, "use");
}
lua_State * card::yieldable_turn_end(lua_State * L)
{
	return yieldable_func(L, my_id, "turn_end");
}
void lua_set_mt_proto_card(lua_State* L, int arg)
{
	luaL_checktype(L, arg, LUA_TTABLE);
	if (luaL_newmetatable(L, "card.proto"))
	{
		//constructed by sys init due to ... reasons
		lua_getfield(L, LUA_REGISTRYINDEX, "_card_moves");
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			lua_pushvalue(L, -2);
			lua_insert(L, -2);
			lua_settable(L, -5);
		}
		lua_pop(L, 1);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, arg);
}
void lua_load_card(lua_State* L, int arg, card& output)
{
	lua_stack_guard g(L);
	lua_pushvalue(L, arg);
	int p = lua_gettop(L);
	
	lua_pushvalue(L, p);
	output.my_id = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_getfield(L, p, "name");
	output.name = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, p, "description");
	output.desc = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, p, "cost");
	output.cost_ap = (int)luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	lua_getfield(L, p, "generated");
	bool is_generated = lua_toboolean(L, -1);
	if (is_generated)
		output.type = card_type::generated;
	lua_pop(L, 1);

	lua_getfield(L, p, "wound");
	bool is_wound = lua_toboolean(L, -1);
	if (is_wound)
		output.type = card_type::wound;
	lua_pop(L, 1);

	lua_set_mt_proto_card(L, p);
	lua_pop(L, 1);//pop copy of card
}

card_ref lua_tocard_ref(lua_State* L, int arg)
{
	lua_getfield(L, arg, "_ref_idx");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	return card_ref{ id };
}
card_ref lua_check_card_ref(lua_State * L, int arg)
{
	if (lua_getmetatable(L, arg)) {  /* does it have a metatable? */
		lua_getfield(L, LUA_REGISTRYINDEX, "card.ref");  /* get correct metatable */
		if (lua_rawequal(L, -1, -2)) {  /* does it have the correct mt? */
			lua_pop(L, 2);  /* remove both metatables */
			return lua_tocard_ref(L,arg);
		}
	}
	luaL_typerror(L, arg, "card.ref");  /* else error */
	return card_ref();
}
int lua_card_ref_tostring(lua_State* L)
{
	auto ref = lua_tocard_ref(L, 1);
	lua_getfield(L, 1, "name");
	const char* str=lua_tostring(L, -1);
	lua_pushfstring(L, "card<%d: %s>",ref,str);
	return 1;
}
void lua_push_card_ref(lua_State * L, const card_ref& r)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, r);
}
static int index_to_proto(lua_State* L)
{
	lua_pushstring(L, "_proto");
	lua_rawget(L, 1);
	lua_insert(L, -2);

	lua_gettable(L, -2);

	return 1;
}
void new_lua_card(lua_State * L,int proto_id)
{
	lua_newtable(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, proto_id);
	lua_setfield(L, -2, "_proto");

	if (luaL_newmetatable(L, "card.ref"))
	{
		lua_pushcfunction(L, lua_card_ref_tostring);
		lua_setfield(L, -2, "__tostring");

		

		//FIXME: actually hide the values inside the table... Prob. should just use the userdata way either way...
		//lua_pushvalue(L, -1);
		//lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, index_to_proto); //TODO: @NOTNICE maybe there is more elegant way?
		lua_setfield(L, -2, "__index");

		//lua_setmetatable(L, -2);
	}
	lua_setmetatable(L, -2);
}
int card_registry::new_card(const card & proto, lua_State* L)
{
	//TODO: crazy idea. Store cards in a lua userdata... It's probably a hash table anyways...

	new_lua_card(L,proto.my_id);
	lua_pushvalue(L, -1);
	int current_id=luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, current_id);
	lua_setfield(L, -2, "_ref_idx");
	lua_pop(L, 1);

	auto& new_card = cards[current_id];
	new_card = proto;
	new_card.my_id = current_id;
	new_card.current_state = card_state::destroyed;

	return current_id;
}

void card_registry::unreg_card(int id, lua_State* L)
{
	luaL_unref(L, LUA_REGISTRYINDEX, cards[id].my_id);
	cards.erase(id);
}
