#pragma once

#include "common.hpp"
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <memory>
#include <random>
#include <array>
#include <assert.h>

#include "knowledge.hpp"
#include "action.hpp"
#include "room.hpp"

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

enum class tile_type //NOTE: DO NOT FORGET TO UPDATE THE HELPER DECL!
{
	nothing,
	floor,
	wall,
	glass,
	entry, //temp way for units to enter map
};
enum_helper_decl(tile_type, nothing, entry);

struct tile_attr
{
	v3f color_back;
	v3f color_fore;
	int glyph;
	tile_flags flags;
};

const tile_attr tile_attrs[] = {
	{ v3f(0.05f, 0.05f, 0.05f)	,v3f(0, 0, 0),	   '.',tile_flags::block_move | tile_flags::block_sight, },
	{ v3f(0, 0, 0)			,v3f(0.2f, 0.2f, 0.2f),'+',tile_flags::none, },
	{ v3f(0, 0, 0)			,v3f(0.8f, 0.8f, 0.8f),'#',tile_flags::block_move | tile_flags::block_sight, },
	{ v3f(0.8f, 0.8f, 0.8f)	,v3f(0.2f, 0.2f, 0.2f),'_',tile_flags::block_move, },
	{ v3f(0, 0, 0)			,v3f(0.8f, 0.8f, 0.8f),'<',tile_flags::none },
};

static_assert(array_size(tile_attrs) == enum_size<tile_type>(), "Tile attrs must be same size as tile_types");

struct tile
{
	tile_type t;
	inline int glyph() const
	{
		return tile_attrs[static_cast<int>(t)].glyph;
	}
	inline v3f color_fore() const
	{
		return tile_attrs[static_cast<int>(t)].color_fore;
	}
	inline v3f color_back() const
	{
		return tile_attrs[static_cast<int>(t)].color_back;
	}
	inline tile_flags flags() const
	{
		return tile_attrs[static_cast<int>(t)].flags;
	}
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
struct room_overlap {
	recti overlap;
	room *other_room;
};
struct console;
struct map
{
    map(int w, int h);

    std::mt19937_64 rand;

    dyn_array2d<tile> static_layer;

    std::vector<entity_ptr> entities;
	std::vector<furniture_ptr> furniture;

	std::vector<room> rooms;
	
	bool is_rect_room_empty(recti r);
	std::vector<room_overlap> get_room_overlaps(recti r, int cur_id = -1);

	plan formulate_plan(entity* owner, action_input_output goal);

	void tick();
    
    void render(console& trg,int start_x,int start_y,int draw_w,int draw_h,int window_off_x,int window_off_y);
    
	
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