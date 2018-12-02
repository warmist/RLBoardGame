#include "map.hpp"
#include "console.hpp"
#include <algorithm>
#include <queue>

map::map(int w, int h) :static_layer(w,h)
{
}

void map::render(console& trg, const recti& view_rect,const v2i& view_pos)
{
	v2i view_size = view_rect.size;
    for (int x = view_rect.x(); x <view_size.x; x++)
        for (int y = view_rect.y(); y <view_size.y; y++)
        {
			v2i tpos=v2i(x, y)+view_pos;
			if (tpos.x >= static_layer.w || tpos.x < 0 ||
				tpos.y >= static_layer.h || tpos.y < 0)
				continue;
            const auto& t = static_layer(tpos);
            trg.set_char(v2i(x,y), t.glyph, t.color_fore,t.color_back);
        }

	
    for (const auto& e : entities)
    {
        if(!e || e->removed) continue;
		v2i tpos(e->x, e->y);
        trg.set_char(tpos-view_pos, e->glyph, e->color_fore,e->color_back);
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
    auto f = [](dyn_array2d<tile_attr>&m, int x, int y){return !test(m(x, y).flags,tile_flags::block_move); };

    return pathfinding(static_layer, f, x, y, tx, ty);
}
const int path_blocked = std::numeric_limits<int>::max();//blocked but processed
void map::pathfind_field(v2i target,float range) //TODO: maybe con8 would be fun but FIXME needs work
{
	auto& pfr = pathfinding_field_result;
	pfr.data.clear();
	pfr.resize(static_layer.w, static_layer.h,v2i(0,0));

	pfr(target) = { 0,0 };

	std::queue<v3i> processing_front;
	int irange = int(range);
	for (int i = 0; i < 4; i++)
	{
		int dx = con4_dx[i];
		int dy = con4_dy[i];
		v2i t = target + v2i(dx, dy);
		if (is_valid_coord(t))
		{
			if (is_passible(t.x, t.y))
			{
				pfr(t) = v2i(-dx, -dy);
				processing_front.push(v3i(t.x,t.y, 1));
			}
			else
			{
				pfr(t) = v2i(path_blocked, path_blocked);
			}
		}
	}

	while (!processing_front.empty())
	{
		v3i c = processing_front.front();
		processing_front.pop();

		for (int i = 0; i < 4; i++)
		{
			int dx = con4_dx[i];
			int dy = con4_dy[i];
			v2i t = v2i(int(c.x + dx), int(c.y + dy));
			int dist = c.z + 1;
			
			if (is_valid_coord(t))
			{
				if (pfr(t) != v2i(0, 0) || t == target) //using 0,0 as unprocessed, also the target coord is 0,0
				{
					continue;
				}
				if (is_passible(t.x, t.y))
				{
					pfr(t) = v2i(-dx, -dy);
					if (dist >= range)
						continue;
					processing_front.push(v3i(t.x, t.y, dist));
				}
				else
				{
					pfr(t) = v2i(path_blocked, path_blocked);
				}
			}
		}
	}
}

void map::render_reachable(console & trg, const recti & view_rect, const v2i & view_pos,const v3f& color)
{

	v2i view_size = view_rect.size;
	for (int x = view_rect.x(); x <view_size.x; x++)
		for (int y = view_rect.y(); y <view_size.y; y++)
		{
			v2i tpos = v2i(x, y) + view_pos;
			if (tpos.x >= pathfinding_field_result.w || tpos.x < 0 ||
				tpos.y >= pathfinding_field_result.h || tpos.y < 0)
				continue;
			const auto& t = pathfinding_field_result(tpos);
			if((t.x!=0 || t.y!=0) && (t.x!= path_blocked))
				trg.set_back(v2i(x, y), color);
		}
}

void map::render_path(console & trg, v2i target, const recti & view_rect, const v2i & view_pos, const v3f & color_ok, const v3f & color_fail)
{
	auto& pfr = pathfinding_field_result;
	v2i map_coord = target + view_pos;
	
	if (map_coord.x >= pfr.w || map_coord.x < 0 || map_coord.y >= pfr.h || map_coord.y < 0)
	{
		trg.set_back(target, color_fail);
		return;
	}
	v2i cdelta = pfr(map_coord);
	if ((cdelta.x == 0 && cdelta.y == 0) ||(cdelta.x== path_blocked))
	{
		trg.set_back(target, color_fail);
		return;
	}

	while (true)
	{
		v2i view_coord = map_coord - view_pos;
		if (view_rect.is_inside(view_coord))
			trg.set_back(view_coord, color_ok);
		map_coord +=cdelta;
		if (map_coord.x >= pfr.w || map_coord.x < 0 || map_coord.y >= pfr.h || map_coord.y < 0)
		{
			//TODO: error here?
			return;
		}
		cdelta = pfr(map_coord);
		if (cdelta.x == 0 && cdelta.y == 0)
		{
			return; //done
		}
	}
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
    if (test(static_layer(x, y).flags,tile_flags::block_move))
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
	fprintf(f, "%d %d\n", static_layer.w, static_layer.h);
}

void map::load(FILE * f)
{
	int w, h;
	fscanf(f, "%d %d\n", &w, &h);
	static_layer.resize(w, h);
}
