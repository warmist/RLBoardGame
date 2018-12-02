#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <random>
#include <queue>
//#include <cereal/types/vector.hpp>
#include <tuple>
#include <memory>

#define _USE_MATH_DEFINES //can't live without all that math
#include <math.h>
using std::string;
using std::vector;

//strings here are utf8
vector<string> enum_files(string path); //list all files in path. Accepts wildcards
vector<string> enum_dirs(string path); //list all dirs in path. Accepts wildcards

#ifdef LUA_TOOLS
struct lua_State;
void lua_load_fs_lib(lua_State* L); //add enum files and enum dirs to lua (under fs)
#endif
template<typename T, size_t sz>
constexpr size_t array_size(T(&)[sz])
{
    return sz;
}
//TODO: move to math module
template <typename T>
struct v2
{
#define MY_OP_2D_S(op) v2 operator op (T s) const { return v2(x op s, y op s); }
#define MY_OP_2D_OTHER(op) template<typename L> v2 operator op (const v2<L>& other) const { return v2(x op other.x, y op other.y); }
#define MY_OP_2D_S_SELF(op) template<typename L> v2& operator op (L s) { x op s; y op s; return *this;}
#define MY_OP_2D_OTHER_SELF(op) template<typename L> v2& operator op (const v2<L>& other) { x op other.x; y op other.y; return *this;}

    T x = 0;
    T y = 0;

    v2() = default;
	template <typename U>
	v2(v2<U>& other) :x(other.x), y(other.y) {}
    v2(T x, T y) :x(x), y(y) {}

    MY_OP_2D_S(*)
    MY_OP_2D_S(/)
    
    MY_OP_2D_OTHER(+)
    MY_OP_2D_OTHER(-)
    MY_OP_2D_OTHER(*)
    MY_OP_2D_OTHER(/)

    MY_OP_2D_OTHER_SELF( += )
    MY_OP_2D_OTHER_SELF( -= )
    MY_OP_2D_OTHER_SELF( *= )
    MY_OP_2D_OTHER_SELF( /= )
    MY_OP_2D_S_SELF( *= )
    MY_OP_2D_S_SELF( /= )
    
    v2& operator =(const v2& other) = default;

    bool operator ==(const v2& other) const { return x == other.x && y == other.y; }
    bool operator !=(const v2& other) const { return x != other.x || y != other.y; }

    T dot(const v2& other) const { return other.x*x + other.y*y; }

    template<typename A>
    void serialize(A& archive)
    {
        archive & x & y;
    }
#undef MY_OP_2D_S
#undef MY_OP_2D_OTHER
#undef MY_OP_2D_S_SELF
#undef MY_OP_2D_OTHER_SELF
};

typedef v2<float> v2f;
typedef v2<double> v2d;
typedef v2<int> v2i;
typedef v2<char> v2c;

template <typename T>
struct v3
{
#define MY_OP_3D_S(op) v3 operator op (T s) const { return v3(x op s, y op s,z op s); }
#define MY_OP_3D_OTHER(op) template<typename L> v3 operator op (const v3<L>& other) const { return v3(x op other.x, y op other.y,z op other.z); }
#define MY_OP_3D_S_SELF(op) template<typename L> v3& operator op (L s) { x op s; y op s; z op s; return *this;}
#define MY_OP_3D_OTHER_SELF(op) template<typename L> v3& operator op (const v3<L>& other) { x op other.x; y op other.y; z op other.z; return *this;}

    T x = 0;
    T y = 0;
    T z = 0;

    v3() = default;
	template <typename U>
	v3(v3<U>& other) :x(other.x), y(other.y),z(other.z) {}
    v3(T x, T y,T z) :x(x), y(y),z(z) {}

    MY_OP_3D_S(*)
    MY_OP_3D_S(/ )

    MY_OP_3D_OTHER(+)
    MY_OP_3D_OTHER(-)
    MY_OP_3D_OTHER(*)
    MY_OP_3D_OTHER(/ )

    MY_OP_3D_OTHER_SELF(+= )
    MY_OP_3D_OTHER_SELF(-= )
    MY_OP_3D_OTHER_SELF(*= )
    MY_OP_3D_OTHER_SELF(/= )
    MY_OP_3D_S_SELF(*= )
    MY_OP_3D_S_SELF(/= )

    v3& operator =(const v3& other) = default;

    bool operator ==(const v3& other)const { return x == other.x && y == other.y && z==other.z; }
    bool operator !=(const v3& other)const { return x != other.x || y != other.y || z!=other.z; }

    T dot(const v3& other) const { return other.x*x + other.y*y + other.z*z; }

    template<typename A>
    void serialize(A& archive)
    {
        archive & x & y & z;
    }
#undef MY_OP_3D_S
#undef MY_OP_3D_OTHER
#undef MY_OP_3D_S_SELF
#undef MY_OP_3D_OTHER_SELF
};

typedef v3<float> v3f;
typedef v3<double> v3d;
typedef v3<int> v3i;
typedef v3<char> v3c;

template<typename T> constexpr int enum_size();

#define enum_helper_decl(type_name,first,last) inline type_name operator++(type_name & x){return x = type_name(std::underlying_type<type_name>::type(x) + 1); }\
	inline type_name operator--(type_name & x){return x = type_name(std::underlying_type<type_name>::type(x) - 1); }\
    type_name operator*(type_name c);\
    constexpr type_name begin(type_name r){return type_name:: first;};\
    constexpr type_name end(type_name r){return type_name:: last;};\
    constexpr bool enum_valid_value(type_name x);\
    template<> constexpr int enum_size<type_name>(){return std::underlying_type<type_name>::type(type_name::last) - std::underlying_type<type_name>::type(type_name::first)+1;}

template<typename ENUM_T>
ENUM_T next_looped(ENUM_T v) {
	if (v != end(v))
		++v;
	else
		v = begin(v);
	return v;
}
template<typename ENUM_T>
ENUM_T prev_looped(ENUM_T v) {
	if (v != begin(v))
		--v;
	else
		v = end(v);
	return v;
}
#define enum_sized_array(enum_type,array_elem_type,array_type) static_assert(array_size(array_type) == enum_size<enum_type>(), #array_type " size must match " #enum_type);\
	inline const array_elem_type& get_ ## array_elem_type (enum_type t) {return array_type[std::underlying_type<enum_type>::type(t)];}


#define enum_helper_define(type_name,first,last) type_name operator++(type_name & x) {return x = type_name(std::underlying_type<type_name>::type(x) + 1); }\
	type_name operator--(type_name & x){return x = type_name(std::underlying_type<type_name>::type(x) - 1); }\
    type_name operator*(type_name c) {return c;}\
    constexpr bool enum_valid_value(type_name x) {return (std::underlying_type<type_name>::type(x)>=std::underlying_type<type_name>::type(type_name::first)) && (std::underlying_type<type_name>::type(x)<std::underlying_type<type_name>::type(type_name::last));}
#define enum_helper_flags(type_name) inline type_name operator |(type_name lhs, type_name rhs)\
	{	using T= std::underlying_type_t<type_name>;\
		return (type_name)( static_cast<T>(lhs)|static_cast<T>(rhs)); }\
inline type_name& operator |=(type_name& lhs,const type_name& rhs)\
	{	using T= std::underlying_type_t<type_name>;\
		lhs= (type_name)( static_cast<T>(lhs)|static_cast<T>(rhs)); return lhs; }\
inline type_name operator &(type_name lhs, type_name rhs)\
	{	using T= std::underlying_type_t<type_name>;\
		return (type_name)( static_cast<T>(lhs)&static_cast<T>(rhs)); }\
inline bool test(type_name lhs, type_name rhs) { return (lhs&rhs) != static_cast<type_name>(0); }
		

template <typename T>
struct rect{
    v2<T> pos;//aka tl_corner
    v2<T> size;//TODO: maybe this could be different type? size!=vector

    rect(){};
    rect(v2<T> pos, v2<T> size = v2<T>()) :pos(pos), size(size){};
    rect(T x, T y, T w = 0, T h = 0) :pos(x, y), size(w, h){};

    T& width() { return size.x; }
    const T& width() const { return size.x; }
    T& height() { return size.y; }
    const T& height() const { return size.y; }
    T& x() { return pos.x; }
    const T& x() const { return pos.x; }
    T& y() { return pos.y; }
    const T& y() const { return pos.y; }
	v2<T> tr_corner() const
	{
		return pos + v2i(size.x, 0);
	}
	v2<T> bl_corner() const
	{
		return pos + v2i(0, size.y);
	}
    v2<T> max_corner() const //aka br_corner
    {
        return pos + size;
    }
    template<typename U>
    bool is_inside(v2<U> p) const
    {
        auto end = max_corner();
        return p.x >= pos.x && p.x < end.x && p.y >= pos.y && p.y < end.y;
    }
    //bool intersects(const rect<U>& o);
    template <typename U>
    rect<T> overlap(const rect<U>& o) const
    {
        rect<T> ret;
        ret.x() = std::max(pos.x, T(o.x()));
        ret.y() = std::max(pos.y, T(o.y()));
        auto m1 = max_corner();
        auto m2 = o.max_corner();
        ret.width() = std::min(m1.x, T(m2.x)) - ret.x();
        ret.height() = std::min(m1.y, T(m2.y)) - ret.y();
        return ret;
    }
	rect<T> grown(T value) const
	{
		rect<T> ret;
		ret.pos = pos - v2<T>(value, value);
		ret.size = size + v2<T>(value, value)*2;
		return ret;
	}
};

typedef rect<float> rectf;
typedef rect<double> rectd;
typedef rect<int> recti;

template <typename T>
struct dyn_array2d
{
    std::vector<T> data;
    int w=0;
    int h=0;

    dyn_array2d(){};
    dyn_array2d(int w, int h) :w(w), h(h)
    {
        resize(w, h);
    }
    
    void resize(int nw, int nh,const T& v=T())
    {
        w = nw; h = nh;
        data.resize(w*h);
    }
	void resize(v2i size, const T& v = T())
	{
		resize(size.x, size.y);
	}
    const T& operator()(int x, int y) const
    {
        return data[x + y*w];
    }
    
    T& operator()(int x, int y)
    {
        return data[x + y*w];
    }
	const T& operator()(v2i pos) const
	{
		return data[pos.x + pos.y*w];
	}

	T& operator()(v2i pos)
	{
		return data[pos.x + pos.y*w];
	}
    template <class A>
    void serialize(A& archive)
    {
        archive & w & h & data;
    }
};

const int con4_dx[] = {1,0,-1,0};
const int con4_dy[] = {0,1,0,-1};
const std::uniform_int<int> rand_con4(0, 3);

const int con8_dx[] = {1,1,0,-1,-1,-1, 0, 1};
const int con8_dy[] = {0,1,1, 1, 0,-1,-1,-1};
const std::uniform_int<int> rand_con8(0, 7);

int con4_to_con8(int c4);
//change dx dy pair into direction index
int dxdy_to_dir(int dx, int dy, bool con8 = true);
//change angle to direction index
int angle_to_dir(double angle, bool con8 = true);

enum DIR8_MASK
{
    DIR8_E  = (1u << 0),
    DIR8_SE = (1u << 1),
    DIR8_S  = (1u << 2),
    DIR8_SW = (1u << 3),
    DIR8_W  = (1u << 4),
    DIR8_NW = (1u << 5),
    DIR8_N  = (1u << 6),
    DIR8_NE = (1u << 7),
};

template<typename T>
T mod(T a, T b)
{
    return (a%b + b) % b;
}

const std::uniform_real<double> rand_one;
inline void hash_combine(std::size_t& seed) { }


template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}

#define MAKE_HASHABLE(type, ...) \
    namespace std {\
        template<> struct hash<type> {\
            std::size_t operator()(const type &t) const {\
                std::size_t ret = 0;\
                hash_combine(ret, __VA_ARGS__);\
                return ret;\
            }\
        };\
    }

MAKE_HASHABLE(v2i, t.x, t.y);

template <typename callback_t,typename T>
void depth_first_walk(dyn_array2d<T>& m, callback_t f,int x,int y)
{
    std::vector<std::pair<int, int>> stack;
    std::unordered_set<v2i> visited;
    if(f(m, x, y))
        stack.push_back(std::make_pair(x, y));

    while (stack.size() > 0)
    {
        auto pos = stack.back();
        stack.pop_back();

            for (int i = 0; i < 4; i++)
            {
                int dx = con4_dx[i];
                int dy = con4_dy[i];
                int tx = dx + pos.first;
                int ty = dy + pos.second;
                if (tx>=0 && tx<m.w && ty>=0 && ty<m.h && visited.count(v2i(tx,ty))==0)
                if (f(m, tx, ty))
                {
                    visited.insert(v2i(tx, ty));
                    stack.push_back(std::make_pair(tx, ty));
                }
            }
    }
}
template <typename callback_t, typename T>
void breadth_first_walk(dyn_array2d<T>& m, callback_t f, int x, int y)
{
    std::queue<std::pair<int, int>> bob;// words without consonants are fake and nobody likes them
    std::unordered_set<v2i> visited;

    if (f(m,x,y))
        bob.push(std::make_pair(x, y));

    while (bob.size() > 0)
    {
        auto pos = bob.front();
        bob.pop();
        
        for (int i = 0; i < 4; i++)
        {
            int dx = con4_dx[i];
            int dy = con4_dy[i];
            int tx = dx + pos.first;
            int ty = dy + pos.second;
            if (tx >= 0 && tx<m.w && ty >= 0 && ty<m.h && visited.count(v2i(tx, ty)) == 0)
                if (f(m, tx, ty))
                {
                    visited.insert(v2i(tx, ty));
                    bob.push(std::make_pair(tx, ty));
                }
        }
    }
}
template <typename callback_t, typename T>
std::vector<std::pair<int, int>> pathfinding(dyn_array2d<T>& m, callback_t f, int x, int y, int ttx, int tty)
{
    std::queue<std::pair<int, int>> bob;// words without consonants are fake and nobody likes them
    std::unordered_map<v2i,v2i> visited;

    std::vector<std::pair<int, int>> ret;
    if (x == ttx && y == tty)
        return ret;

    if (!f(m, ttx, tty)) //Tried to reach unreachable target
        return ret;

    if (f(m, x, y))
    {
        //visited[v2i(x, y)] = v2i(-1, -1);
        bob.push(std::make_pair(x, y));
    }
    while (bob.size() > 0)
    {
        auto pos = bob.front();
        bob.pop();
        bool done = false;
        for (int i = 0; i < 8; i++)
        {
            int dx = con8_dx[i];
            int dy = con8_dy[i];
            int tx = dx + pos.first;
            int ty = dy + pos.second;
            if (tx >= 0 && tx<m.w && ty >= 0 && ty<m.h && visited.count(v2i(tx, ty)) == 0)
                if (f(m, tx, ty))
                {
                    visited[v2i(tx, ty)]=v2i(pos.first,pos.second);
                    bob.push(std::make_pair(tx, ty));
                    if (tx == ttx && ty == tty)
                    {
                        done = true;
                        break;
                    }
                }
        }
        if (done)
        {
            ret.push_back(std::make_pair(ttx, tty));
            v2i cp = visited[v2i(ttx, tty)];
            while (visited.count(cp) != 0)
            {
                ret.push_back(std::make_pair(cp.x, cp.y));
                cp = visited[cp];
                if (cp.x == x && cp.y == y)
                    break;
            }
            return ret;
        }
    }
    return ret;
}
template <typename callback_t, typename callback_w_t, typename T>
std::vector<std::pair<int, int>> pathfinding_weighted(dyn_array2d<T>& m, callback_w_t wfunc, callback_t f, int x, int y, int ttx, int tty)
{
    using w_point_t=std::tuple<int, int, float>;//point with weight
    auto compare_pts = [](const w_point_t& a, const w_point_t& b) {return std::get<2>(a) < std::get<2>(b); };

    std::priority_queue<w_point_t, std::vector<w_point_t>,decltype(compare_pts)> bob(compare_pts);// words without consonants are fake and nobody likes them
    std::unordered_map<v2i, v2i> visited;

    std::vector<std::pair<int, int>> ret;
    if (x == ttx && y == tty)
        return ret;

    if (!f(m, ttx, tty)) //Tried to reach unreachable target
        return ret;

    if (f(m, x, y))
    {
        //visited[v2i(x, y)] = v2i(-1, -1);
        bob.push(std::make_tuple(x, y, wfunc(x,y)));
    }
    while (bob.size() > 0)
    {
        auto pos = bob.top();
        bob.pop();
        bool done = false;
        for (int i = 0; i < 8; i++)
        {
            int dx = con8_dx[i];
            int dy = con8_dy[i];
            int tx = dx + std::get<0>(pos);
            int ty = dy + std::get<1>(pos);
            if (tx >= 0 && tx<m.w && ty >= 0 && ty<m.h && visited.count(v2i(tx, ty)) == 0)
                if (f(m, tx, ty))
                {
                    visited[v2i(tx, ty)] = v2i(std::get<0>(pos), std::get<1>(pos));
                    bob.push(std::make_tuple(tx, ty, wfunc(tx, ty)));
                    if (tx == ttx && ty == tty)
                    {
                        done = true;
                        break;
                    }
                }
        }
        if (done)
        {
            ret.push_back(std::make_pair(ttx, tty));
            v2i cp = visited[v2i(ttx, tty)];
            while (visited.count(cp) != 0)
            {
                ret.push_back(std::make_pair(cp.x, cp.y));
                cp = visited[cp];
                if (cp.x == x && cp.y == y)
                    break;
            }
            return ret;
        }
    }
    return ret;
}
#define RULE_OF_ZERO(classname) classname(const classname&) = default;\
    classname(classname&&) = default;\
    classname& operator=(const classname&) = default;\
    classname& operator=(classname&&) = default;\
    virtual ~classname() = default

#define RULE_OF_ZERO_NO_COPY(classname) classname(const classname&) = delete;\
    classname& operator=(const classname&) = delete;\
    virtual ~classname() = default

#define RULE_OF_ZERO2(classname) classname()=default;\
    classname(const classname&) = delete;\
    classname(classname&&) = default;\
    classname& operator=(const classname&) = delete;\
    classname& operator=(classname&&) = default;\
    virtual ~classname() = default


template<typename ENUM,typename T>
struct enum_vector {
    T data[enum_size<ENUM>()];
    T& operator[](ENUM key)
    {
        size_t index = static_cast<size_t>(static_cast<size_t>(key) - static_cast<size_t>(begin(ENUM())));
        return data[index];
    }
    const T& operator[](ENUM key) const
    {
        size_t index = static_cast<size_t>(static_cast<size_t>(key) - static_cast<size_t>(begin(ENUM())));
        return data[index];
    }
    template <class A>
    void serialize(A& archive)
    {
        for (size_t i = 0; i < enum_size<ENUM>(); i++)
            archive & data[i];
    }
};
//TODO make one for std::array?
template <typename T, typename ENG>
T& pick_random(std::vector<T>& vec, ENG& engine)
{
    std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
    return vec[dist(engine)];
}
template <typename T, typename ENG>
const T& pick_random(const std::vector<T>& vec, ENG& engine)
{
    std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
    return vec[dist(engine)];
}
template<typename T,size_t max_size>
class ring_buffer {
    int top_ = 0;
    int size_ = 0;
public:
    std::array<T, max_size> data;

    bool push_top(T v)
    {
        if (size_ == max_size)
            return false;

        if (is_empty())
        {
            top_ = 0;
            data[top_] = v;
        }
        else
        {
            top_--;
            if (top_ < 0)
            {
                top_ = size - 1;
            }
            data[top_] = v;
        }
        size_++;
        return true;
    }
    bool push_back(T v)
    {
        if (size_ == max_size)
            return false;

        if (is_empty())
        {
            top_ = 0;
        }
        size_++;
        int end_pt = top_ + size_-1;
        end_pt %= max_size;
        data[end_pt] = v;

        return true;
    }
    bool is_empty() { return size_ == 0; }
    const T& front() const { return data[top_]; }
    T& front() { return data[top_]; }
    const T& back() const { return data[(top_+size_-1)%max_size]; }
    T& back() { return data[(top_ + size_-1) % max_size]; }

    bool pop_top()
    {
        if (size_ == 0)
            return false;
        top_--;
        size_--;
        if (top_ < 0)
            top_ = max_size - 1;
        return true;
    }
    bool pop_back()
    {
        if (size_ == 0)
            return false;
        size_--;
        return true;
    }
    int size() const { return size_; }
};
float angle_diff(float a1, float a2);

template<typename D, typename B>
std::unique_ptr<D> static_cast_ptr(std::unique_ptr<B>& base)
{
    return std::unique_ptr<D>(static_cast<D*>(base.release()));
}

template<typename D, typename B>
std::unique_ptr<D> static_cast_ptr(std::unique_ptr<B>&& base)
{
    return std::unique_ptr<D>(static_cast<D*>(base.release()));
}

bool angle_inside(int dx, int dy,float angle_start, float angle_end);

template <typename T>
struct boolean_statement //in form (1 && 2 && ... n) || (n+1 && ...) || ... (k && ... k+j)
{
	std::vector<T> entries;
	std::vector<int> or_places;
	void add_and_term(T v)
	{
		entries.push_back(v);
	}
	void start_or_term()
	{
		or_places.push_back(entries.size());
	}
	bool matches(const std::vector<T>& input)
	{
		int last_p = 0;
		for (int cur_or = 0; cur_or <= or_places.size(); cur_or++)
		{
			int or_p = 0;
			if (cur_or == or_places.size())
				or_p = entries.size();
			else
				or_p = or_places[cur_or];
			
			bool all_good = true;
			for (int j = last_p; j < or_p; j++)
			{
				bool found = false;
				for (const auto& v : input)
				{
					if (entries[j] == v)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					all_good = false;
					break;
				}
			}
			if (all_good)
				return true;
			last_p = or_p;
		}
		return false;
	}
};

template<typename T>
bool read_file(const char* fname,std::vector<T>& ret)
{
	auto f = fopen(fname, "rb");

	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	auto pos = ftell(f);
	fseek(f, 0, SEEK_SET);

	ret.resize(pos);
	size_t count = ret.size() / sizeof(T);
	bool ok = fread(ret.data(), sizeof(T), count, f) == count;

	fclose(f);
	return ok;
}