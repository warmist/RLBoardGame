#pragma once

#include "common.hpp"
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <memory>
#include <random>
#include <array>
#include <assert.h>


struct entity;
struct map;
enum class tile_flags
{
	none = 0,
	block_move = 1 << 0,
	block_sight = 1 << 1,
	crowded_move = 1 << 2, //e.g. ventilation is hard to move in
};
enum_helper_flags(tile_flags)


struct tile_attr
{
	v3f color_back;
	v3f color_fore;
	int glyph;
	tile_flags flags;
};


enum class entity_type
{
	misc,
    worker,
	furniture,
};
enum_helper_decl(entity_type, misc, furniture);

struct entity:public tile_attr
{
    int x, y;
    int next_tick = 0;
    bool ticked = false;
    bool removed = false;
    entity_type type=entity_type::misc;

    virtual ~entity() {}

	virtual const char* name() const { return "misc"; }

    //general tick logic. Return is how many ticks to wait until next call
    virtual int tick(map& m) 
    {
        return 0;
    }
    //something bumped into you. Return true if it can swap with you
    virtual bool bump(entity& other, map& m)
    {
        return true;
    }
    //gets called before removal from map
    virtual void before_remove(map& m,std::unique_ptr<entity>& temp_hold)
    {

    }
	virtual bool is_same(const entity& other) const
	{
		return other.type == type;
	}
};
struct furniture;
typedef std::unique_ptr<furniture> furniture_ptr;

typedef std::unique_ptr<entity> entity_ptr;

struct console;
struct map
{
    map(int w, int h);

    std::mt19937_64 rand;

    dyn_array2d<tile_attr> static_layer;

    std::vector<entity_ptr> entities;

	void tick();
    
    void render(console& trg, const recti& view_rect, const v2i& view_pos);
    std::vector<std::pair<int, int>> pathfind(int x, int y, int tx, int ty);

    //helper functions
    inline bool is_valid_coord(int x, int y) const { return x >= 0 && y >= 0 && x < static_layer.w && y < static_layer.h; }
	inline bool is_valid_coord(v2i p) const { return p.x >= 0 && p.y >= 0 && p.x < static_layer.w && p.y < static_layer.h; }
    bool is_passible(int x, int y);
    bool is_supported_all(int x, int y, int dirmask); //returns true IFF all (x+dx,y+dy) is not passible (checking con8 with dirmask)
    bool is_supported_any(int x, int y, int dirmask); //returns true IFF any (x+dx,y+dy) is not passible (checking con8 with dirmask)

	void save(FILE* f);
	void load(FILE* f);
};

template <typename T>
T sign(T v){
    return (v >= 0) ? ((v==0)?(0):(1)) : (-1);
}
template <typename T>
T sign_no_zero(T v) {
	return (v >= 0) ? (1) : (-1);
}