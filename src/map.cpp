#include "map.hpp"
#include "console.hpp"
#include <algorithm>
#include <queue>

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
            trg.set_char(v2i(tx, ty), t.glyph, t.color_fore,t.color_back);
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
    auto f = [](dyn_array2d<tile_attr>&m, int x, int y){return !test(m(x, y).flags,tile_flags::block_move); };

    return pathfinding(static_layer, f, x, y, tx, ty);
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
