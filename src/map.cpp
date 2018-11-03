#include "map.hpp"
#include "console.hpp"
#include "e_furniture.hpp"
#include <algorithm>
#include <queue>
//enum_helper_define(entity_type, misc, worker);

const float eps = 0.00001f;
const float dist_interact =15.0f;
const float dir_close = 0.2f;
map::map(int w, int h) :static_layer(w,h)
{
}

void map::render(console& trg, int start_x, int start_y, int draw_w, int draw_h, int window_off_x, int window_off_y)
{
    int end_x = std::min(static_layer.w, start_x + draw_w);//in map clamp
    int end_y = std::min(static_layer.h, start_y + draw_h);
    start_x = std::max(0, start_x);
    start_y = std::max(0, start_y);
    for (int x = start_x; x <end_x; x++)
        for (int y = start_y; y < end_y; y++)
        {
            const auto& t = static_layer(x, y);
            int tx = x + window_off_x;
            int ty = y + window_off_y;
            if (tx < 0 || ty < 0 || tx >= trg.w_ || ty >= trg.h_) //inscreen check
            {
                continue;
            }
            trg.set_char(v2i(tx, ty), t.glyph(), t.color_fore(),t.color_back());
        }

	for (const auto& e : furniture)
	{
		if (!e || e->removed) continue;
		v2i wp = e->pos + v2i(window_off_x, window_off_y);
		v2i size = v2i(e->tiles.w, e->tiles.h);
		//TODO: clip early if all offscreen @PERF
		for (int ex = wp.x; ex < wp.x + size.x; ex++)
			for (int ey = wp.y; ey < wp.y + size.y; ey++)
			{
				if (ex < 0 || ey < 0 || ex >= trg.w_ || ey >= trg.h_)
					continue;
				auto& t = e->tiles(ex - wp.x, ey - wp.y);
				if (t.glyph == 0)
					continue;
				trg.set_char(v2i(ex, ey), t.glyph, t.color_fore, t.color_back);
			}
	}

    for (const auto& e : entities)
    {
        if(!e || e->removed) continue;
        int ex = e->x + window_off_x;
        int ey = e->y + window_off_y;
        if (ex < 0 || ey < 0 || ex >= trg.w_ || ey >= trg.h_)
            continue;
        trg.set_char(v2i(ex, ey), e->glyph, e->color_fore,e->color_back);
    }

	
    /*for (int x = 0; x < static_layer.w; x++)
        for (int y = 0; y < static_layer.h; y++)
        {
            auto t = vision_layer(x, y);
            if (t == 1) //see
            {
                trg.set_back(sf::Vector2i(x, y), sf::Color(255, 201,6));
            }
            else if (t == 2) //kill
            {
                trg.set_back(sf::Vector2i(x, y), sf::Color(170, 21, 26));
            }
        }*/
    /*
    for (int x = 0; x < static_layer.w; x++)
        for (int y = 0; y < static_layer.h; y++)
        {
            auto v = !is_supported_any(x,y, DIR8_W | DIR8_SW | DIR8_S | DIR8_SE | DIR8_E);
            trg.set_back(sf::Vector2i(x, y), sf::Color(255 * v, 0, 0));
        }*/
    /* debug openess
    for (int x = 0; x < static_layer.w; x++)
        for (int y = 0; y < static_layer.h; y++)
        {
            auto v = tile_openess(x, y);

            trg.set_back(sf::Vector2i(x, y), sf::Color(255*v, 0, 0));
        }
        */
}

std::vector<std::pair<int, int>> map::pathfind(int x, int y, int tx, int ty)
{
    auto f = [](dyn_array2d<tile>&m, int x, int y){return !test(m(x, y).flags(),tile_flags::block_move); };

    return pathfinding(static_layer, f, x, y, tx, ty);
}
struct plan_node
{
	action* act;
	int next_node;
};
struct fifo_node
{
	std::vector<action_input_output> state;
	action_input_output goal;
	plan_node* node=nullptr;
};
bool map::is_rect_room_empty(recti r)
{
	for (auto& room : rooms)
	{
		auto ov = room.decorated().overlap(r);
		if (ov.width() > 0 && ov.height() > 0)
		{
			return false;
		}
	}
	return true;
}
std::vector<room_overlap> map::get_room_overlaps(recti r, int cur_id)
{
	std::vector<room_overlap> ret;

	for (int i = 0; i<int(rooms.size()); i++)
	{
		if (i == cur_id)
			continue;
		auto ov=rooms[i].decorated().overlap(r);
		if (ov.width() > 0 && ov.height() > 0)
		{
			ret.emplace_back(room_overlap{ ov,&rooms[i] });
		}
	}

	return ret;
}
plan map::formulate_plan(entity * owner, action_input_output goal)
{
	std::queue<fifo_node> fifo;
	std::vector<plan_node> action_graph;

	fifo_node start;
	start.goal = goal;
	//todo: populate state from owner. E.g. owner could already be in some place etc...
	fifo.push(start);

	//iterate:
	while(!fifo.empty())
	{
	//pop from fifo
		auto c=fifo.back();
		fifo.pop();
	//get current goal, find all actions that give that goal
		/*for (const auto& act : common_actions)
		{
			for(const auto& g:act.output)
				if (g == c.goal)
				{
					
				}
		}*/
	//add them to fifo
	//do until finding an action that can be done already (i.e. preq matched) or fifo empty
	}
	return plan(); //empty plan, can't find anyway to reach the goal
}
void map::tick()
{
	for(auto& e: entities)
        {
            if (e && e->removed)
            {
                auto hold = std::move(e);
                e->before_remove(*this,hold);//could put somewhere to "live on"
                //end of scope destroys the entity, if it's still in hold
            }
            else if (e)
            {
                e->ticked = false;
            }
        }
	//compact entities

	entities.erase(std::remove_if(entities.begin(), entities.end(), [](const std::unique_ptr<entity>& a) {return !bool(a); }), entities.end());

	//
    for (auto& ep : entities)
    {
        auto e = ep.get();
        if (e->removed || e->ticked)
            continue;
        if (e->next_tick > 0)
        {
            e->next_tick--;
        }
        e->ticked = true;
        e->next_tick = e->tick(*this);
    } 
}
bool map::is_passible(int x, int y)
{
    if (test(static_layer(x, y).flags(),tile_flags::block_move))
        return false;
    /*auto& e = entities(x, y);
    if (e && test(e->flags, item_flags::blocks_move))
        return false;*/
    return true;
}
bool map::is_supported_all(int x, int y, int dirmask)
{
    for (int i = 0; i < 8; i++)
    {
        if(dirmask & (1u<<i))
        { 
            int tx = x+con8_dx[i];
            int ty = y+con8_dy[i];
            if (tx >= 0 && ty >= 0 && tx < static_layer.w && ty < static_layer.h)
            {
                if (is_passible(tx, ty))
                {
                    return false;
                }
            }
        }
    }
    return true;
}
bool map::is_supported_any(int x, int y, int dirmask)
{
    for (int i = 0; i < 8; i++)
    {
        if (dirmask & (1u << i))
        {
            int tx = x + con8_dx[i];
            int ty = y + con8_dy[i];
            if (tx >= 0 && ty >= 0 && tx < static_layer.w && ty < static_layer.h)
            {
                if (!is_passible(tx, ty))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void map::save(FILE * f)
{
	fprintf(f, "%d %d\n%lld\n", static_layer.w, static_layer.h,rooms.size());
	for (auto& r : rooms)
	{
		r.save(f);
		fprintf(f, "\n");
	}
}

void map::load(FILE * f)
{
	int w, h;
	size_t room_count;
	fscanf(f, "%d %d\n%lld\n", &w, &h, &room_count);
	static_layer.resize(w, h);
	rooms.clear();
	furniture.clear();
	rooms.reserve(room_count);
	for(size_t i=0;i<room_count;i++)
	{
		room r;
		r.load(f);
		r.place(*this);
		rooms.push_back(r);
	}
}
