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
	player,
	enemy,
};
enum_helper_decl(entity_type, misc, enemy);

struct entity:public tile_attr
{
    v2i pos;
    bool removed = false;
    entity_type type=entity_type::misc;

    virtual ~entity() {}
};

typedef std::unique_ptr<entity> entity_ptr;

struct console;
struct map
{
    map(int w, int h);
	recti view_rect;
	v2i   view_pos;

    std::mt19937_64 rand;

    dyn_array2d<tile_attr> static_layer;
	dyn_array2d<float> pathfinding_field_result;

    std::vector<entity_ptr> entities;
	void compact_entities();

    void render(console& trg);
    std::vector<std::pair<int, int>> pathfind(int x, int y, int tx, int ty);
	void pathfind_field(v2i target,float range);
	void render_reachable(console& trg,const v3f& color);
	std::vector<v2i> get_path(v2i target, bool ignore_target,int max_len=-1);
	void render_path(console& trg, const std::vector<v2i>& path, const v3f& color_ok);
	std::vector<v2i> raycast(v2i pos, v2f dir);
	std::vector<v2i> raycast_target(v2i pos, v2i target);
    //helper functions
    inline bool is_valid_coord(int x, int y) const { return x >= 0 && y >= 0 && x < static_layer.w && y < static_layer.h; }
	inline bool is_valid_coord(v2i p) const { return p.x >= 0 && p.y >= 0 && p.x < static_layer.w && p.y < static_layer.h; }
    bool is_passible(int x, int y);

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