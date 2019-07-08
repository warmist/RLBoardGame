#include "map.hpp"
#include "console.hpp"
#include <algorithm>
#include <queue>

map::map(int w, int h) :static_layer(w,h)
{
}

void map::render(console& trg)
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
        trg.set_char(e->pos-view_pos, e->glyph, e->color_fore,e->color_back);
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
const float path_blocked = std::numeric_limits<float>::infinity();
void map::pathfind_field(v2i target,float range)
{
	dyn_array2d<float>& distances = pathfinding_field_result;
	distances.data.clear();
	distances.resize(static_layer.w, static_layer.h,path_blocked);
	distances(target) =  0;

	std::queue<v2i> processing_front;
	int irange = int(range);
	for (int i = 0; i < 8; i++)
	{
		int dx = con8_dx[i];
		int dy = con8_dy[i];
		v2i t = target + v2i(dx, dy);
		if (is_valid_coord(t))
		{
			if (is_passible(t.x, t.y))
			{
				processing_front.push(t);
				distances(t) = dist_con8[i];
			}
			else
			{
				distances(t) = path_blocked;
			}
		}
	}

	while (!processing_front.empty())
	{
		v2i c = processing_front.front();
		processing_front.pop();

		for (int i = 0; i < 8; i++)
		{
			int dx = con8_dx[i];
			int dy = con8_dy[i];
			v2i t = v2i(int(c.x + dx), int(c.y + dy));
			float cur_dist = distances(c);
			float dist =dist_con8[i]+cur_dist;
			
			if (is_valid_coord(t))
			{
				float& trg_dist = distances(t);
				if ((trg_dist <= dist) || t == target)//if we dont improve the distance we don't touch it. But we use 0 as unvisited
				{
					continue;
				}
				if (is_passible(t.x, t.y))
				{
					trg_dist = dist;
					if (dist >= range)
						continue;
					processing_front.push(v2i(t.x, t.y));
				}
				else
				{
					trg_dist = path_blocked;
				}
			}
		}
	}
}

void map::render_reachable(console & trg, const v3f& color)
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
			if(t!=0 && t!= path_blocked)
				trg.set_back(v2i(x, y), color);
		}
}
std::vector<v2i> map::get_path(v2i target, bool ignore_target,int max_len)
{
	auto& pfr = pathfinding_field_result;

	std::vector<v2i> ret;

	//don't pathfind if target is inside a wall or sth
	if(!ignore_target && (pfr(target) == path_blocked))
		return ret;

	while (true)
	{
		//find direction in which we would step in minimized distance
		float min_dist = path_blocked;
		v2i cdelta = v2i(0, 0);
		for (int i = 0; i < 8; i++)
		{
			int dx = con8_dx[i];
			int dy = con8_dy[i];
			v2i t = target + v2i(dx, dy);
			if (t.x < 0 || t.y < 0 || t.x >= pfr.w || t.y >= pfr.h)
				continue;
			float cd = pfr(t);
			if (cd < min_dist)
			{
				cdelta = v2i(dx, dy);
				min_dist = cd;
			}
		}
		//can't step anywhere?
		if (cdelta == v2i(0, 0))
		{
			return std::vector<v2i>();
		}
		//perform a step
		ret.push_back(target);
		if (max_len > 0 && ret.size() >= max_len)
		{
			return ret; //quit early as walked MAX_LEN of path
		}
		target += cdelta;
		//somehow stepped out of bounds. Totally an error
		if (target.x >= pfr.w || target.x < 0 || target.y >= pfr.h || target.y < 0)
		{
			assert(false);
			//TODO: error here?
			return ret;
		}
		//reached the end
		if (min_dist == 0)
		{
			return ret;//done
		}
	}
	
}

void map::render_path(console & trg, const std::vector<v2i>& path, const v3f & color_ok)
{
	for (auto& p : path)
	{
		v2i pv = p - view_pos;
		if(view_rect.is_inside(pv))
			trg.set_back(pv, color_ok);
	}
}
/*
void line(v2i p0, v2i p1)
{
	v2i delta = p1 - p0;
	if (p0 == p1)
		return;
	bool swap_xy = false;
	if (abs(delta.y) > abs(delta.x))
	{
		swap_xy = true;
		std::swap(delta.x, delta.y);
		std::swap(p0.x, p0.y);
		std::swap(p1.x, p1.y);
	}
	float deltaerr = abs(float(delta.y) / float(delta.x)); // Assume deltax != 0 (line is not vertical),
	float error = 0; // No error at start
	int y = p0.y;
	for (int x = p0.x; x < p1.x; x++)
	{
		if(swap_xy)
			plot(y, x);
		else
			plot(x, y);
		error += deltaerr;
		if (error >= 0.5)
		{
			y += sign(delta.y) * 1;
			error -= 1;
		}
	}
}
*/

//FIXME: this sucks! e.g. push, with start+dir*dist pushes too far (on diagonal) and in wrong direction (e.g. -1,-1)
std::vector<v2i> map::raycast_target(v2i spos, v2i target, bool ignore_start, bool ignore_passible,bool ignore_target, float& path_distance)
{
	auto passible_logic = [&](v2i p) {
		if (ignore_passible)
			return true;
		if (ignore_start && (v2i(p.x,p.y) == spos))
			return true;
		if (ignore_target && (v2i(p.x, p.y) == target))
			return true;
		return is_passible(p.x,p.y);
	};
	std::vector<v2i> ret;
	v2i delta = target - spos;
	if (target == spos)
		return ret;
	v2i step = v2i(sign(delta.x), 0);
	bool step_x = true;
	float deltaerr = abs(float(delta.y) / float(delta.x)); 
	if (abs(delta.y) > abs(delta.x))
	{
		step_x = false;
		step = v2i(0, sign(delta.y));
		deltaerr= abs(float(delta.x) / float(delta.y));
	}

	float error = 0;
	
	path_distance = 0;
	bool first = true;
	bool step_alt = false;
	for (v2i pos = spos; ; pos+=step)
	{
		
		if (!is_valid_coord(pos) || !passible_logic(pos))
			return ret;
		if (!first) //first step has 0 dx/dy
		{
			int dirv = 0;
			if (step_alt)
			{
				dirv=dxdy_to_dir(sign(delta.x), sign(delta.y));
			}
			else
			{
				if(step_x)
					dirv=dxdy_to_dir(sign(delta.x), 0);
				else
					dirv=dxdy_to_dir(0, sign(delta.y));
			}
			path_distance += dist_con8[dirv];
		}
		first = false;
		ret.push_back(pos);
		if (pos == target)
			return ret;
		error += deltaerr;
		if (error >= 0.5)
		{
			if (step_x)
				pos.y += sign(delta.y);
			else
				pos.x += sign(delta.x);

			error -= 1;
			step_alt = true;
		}
		else
		{
			step_alt = false;
		}
	}
	return ret;
}

void map::compact_entities()
{
	entities.erase(std::remove_if(entities.begin(), entities.end(), [](const std::unique_ptr<entity>& a) {
		return !bool(a) || a->removed; 
	}), entities.end());
}
bool map::is_passible(int x, int y)
{
    if (test(static_layer(x, y).flags,tile_flags::block_move))
        return false;
	for (auto& e : entities)
	{
		if (e->pos == v2i(x, y))
		{
			return false;
		}
	}
    return true;
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
