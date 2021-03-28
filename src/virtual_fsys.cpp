#include "virtual_fsys.hpp"
#include "common.hpp"
//assets
#include "asset_cp437_12x12.hpp"
#include "asset_paul_10x10.hpp"
#include "asset_vector_lib.hpp"

struct file_entry
{
    const char fname[200];
    const char* data;
    size_t size;
};

static const file_entry files[] = {
    {"vector_lib.llua",EMB_FILE_vector_lib, EMB_FILE_SIZE_vector_lib},
    {"paul_10x10.png",EMB_FILE_Paul_10x10, EMB_FILE_SIZE_Paul_10x10},
};
bool vfilesys_load_file(const char* asset_dir,const char * filename, char ** buffer, size_t & size, bool & needs_free)
{
    char path_buf[200] = { 0 };
    strcat_s(path_buf, asset_dir);
    strcat_s(path_buf, filename);
    if (read_file_buffer(path_buf, buffer, size))
    {
        needs_free = true;
        return true;
    }
    //TODO: binsearch for perf
    auto count_files = array_size(files);
    for (size_t i = 0; i < count_files; i++)
    {
        if (strcmp(files[i].fname, filename) == 0)
        {
            *buffer = (char*)files[i].data; //TODO: remove this const cast!
            size = files[i].size;
            needs_free = false;
            return true;
        }
    }
    *buffer = nullptr;
    size = 0;
    needs_free = false;
    return false;
}
