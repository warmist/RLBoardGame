#include "common.hpp"
#ifdef LUA_TOOLS
#include "lua_tools.hpp"
#endif
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <codecvt>

// convert UTF-8 string to wstring
std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}
const int mask_dir = 1;
const int mask_file = 2;
vector<string> enum_fs_items(string path, int mask)
{
    vector<string> ret;

    WIN32_FIND_DATA ffd;
    auto wpath = utf8_to_wstring(path);
    auto hFind = FindFirstFile(wpath.c_str(), &ffd);

    if (INVALID_HANDLE_VALUE == hFind) //TODO: error handling
    {
        return ret;
    }
    do
    {
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (mask & mask_dir))
        {
            ret.push_back(wstring_to_utf8(ffd.cFileName));
        }
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (mask & mask_file))
        {
            ret.push_back(wstring_to_utf8(ffd.cFileName));
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    auto error = GetLastError();
    if (error != ERROR_NO_MORE_FILES)
    {
        //TODO: log error?
        return ret;
    }

    FindClose(hFind);
    return ret;
}
vector<string> enum_files(string path)
{
    return enum_fs_items(path, mask_file);
}
vector<string> enum_dirs(string path)
{
    return enum_fs_items(path, mask_dir);
}
#ifdef LUA_TOOLS
static int lua_enum_files(lua_State* L)
{
    const char* path=luaL_checkstring(L, 1);
    auto ret = enum_files(path);
    lua_newtable(L);
    for (size_t i = 0; i < ret.size(); i++)
    {
        lua::set_field(L, -1,int(i + 1), ret[i].c_str());
    }
    return 1;
}
static int lua_enum_dirs(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    auto ret = enum_dirs(path);
    lua_newtable(L);
    for (size_t i = 0; i < ret.size(); i++)
    {
        lua::set_field(L, -1,int(i + 1), ret[i].c_str());
    }
    return 1;
}
void lua_load_fs_lib(lua_State* L)
{
    lua_newtable(L);
    lua::set_field(L, -1, "enum_files", &lua_enum_files);
    lua::set_field(L, -1, "enum_dirs", &lua_enum_dirs);
    lua_setglobal(L, "fs");
}

#endif
int con4_to_con8(int c4)
{
    return c4 * 2;
}
int dxdy_to_dir(int dx, int dy, bool con8)
{
    double d = sqrt(dx*dx + dy*dy);
    double ddx = dx/d;
    double ddy = dy/d;
    double angle = atan2(ddy, ddx);
    return angle_to_dir(angle, con8);
}
int angle_to_dir(double angle, bool con8)
{
    if (con8)
    {
        angle /= (M_PI / 4.0); //turns M_PI into 4
        return mod(int(angle), 8);
    }
    else
    {
        angle /= (M_PI / 2.0); //turns M_PI into 4
        return mod(int(angle), 4);
    }
}
float angle_diff(float a1, float a2)
{
    float diff=a1- a2;
	if (diff < -M_PI )diff += float(2 * M_PI);
	if (diff > M_PI)diff -= float(2 * M_PI);
    return diff;
}
bool angle_inside(int dx, int dy,float angle_start,float angle_end)
{
    float a = float(atan2(dy, dx)); //TODO: costly op
    float as = angle_start;
    float ae = angle_end;
    bool invert = false;
    if (as < -M_PI)
    {
        as += 2 * float(M_PI);
        std::swap(as, ae);
        invert = true;
    }
    else if (ae > M_PI)
    {
        ae -= 2 * float(M_PI);
        std::swap(as, ae);
        invert = true;
    }
    //else
    {
        if (a > as && a < ae)
            return !invert;
        return invert;
    }

}
bool read_file_buffer(const char * fname, char ** buffer, size_t & size)
{
    auto f = fopen(fname, "rb");

    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buffer = new char[size];

    bool ok = fread(*buffer, size, 1, f) == 1;

    fclose(f);
    return ok;
}
int str_find(const std::string & v, char f, int start_offset)
{
	for (int i = start_offset; i < v.size(); i++)
	{
		if (v[i] == f)
			return i;
	}
	return int(v.size());
}

