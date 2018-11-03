#ifndef REX_LOADER_INCLUDED
#define REX_LOADER_INCLUDED

#include "common.hpp"
#include <vector>
struct loaded_rex_pixel {
	int tile;
	v3c fore_color;
	v3c back_color;
};
using loaded_rex_layer = dyn_array2d<loaded_rex_pixel>;
using loaded_rex_image = std::vector<loaded_rex_layer>;

bool load_rex_image_file(const char* path, loaded_rex_image& layers);
bool load_rex_image(const unsigned char* data, size_t size, loaded_rex_image& layers);
#endif

#ifdef REX_LOADER_IMPL

#include <stdint.h>

#pragma pack(push,1)
struct rex_header {
	int32_t version;
	int32_t layer_count;
};

struct rex_layer_header {
	uint32_t width;
	uint32_t height;
};
struct rex_pixel {
	uint32_t tile_image;
	uint8_t fore[3];
	uint8_t back[3];
};
struct gzip_header {
	uint16_t magic;//0x1f 0x8b
	uint8_t method;
	uint8_t flags;
	uint32_t mod_time;
	uint8_t extra_flags;
	uint8_t os_type;
};
#pragma pack(pop)

#include "miniz.h"



bool load_rex_image_file(const char* path, loaded_rex_image& layers)
{
	std::vector<unsigned char> zipped_buffer;
	if (!read_file(path, zipped_buffer))
		return false;
	return load_rex_image(zipped_buffer.data(), zipped_buffer.size(), layers);
}
bool load_rex_image(const unsigned char* data, size_t size, loaded_rex_image& layers)
{
	auto gheader = *reinterpret_cast<const gzip_header*>(data);
	if (gheader.method != 0x8 || gheader.magic != 0x8b1f || gheader.flags != 0) //note: method and magic MUST be this way for this to work but if flags !=0 that means there is custom shit in this gzip thing
		return false;
	uint32_t unzipped_size= *reinterpret_cast<const uint32_t*>(data+size-4);

	std::vector<unsigned char> buffer;
	buffer.resize(unzipped_size);
	mz_ulong dest_size = mz_ulong(buffer.size());

	mz_stream stream = { 0 };
	auto init_ret=mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS);
	stream.next_in = data + sizeof(gzip_header);

	stream.next_out = buffer.data();
	stream.avail_out = dest_size;
	size_t bytes_left = size-sizeof(gzip_header)-8;
	for(;;)
	{
		auto bread = (1024 < bytes_left) ? (1024) : (bytes_left);
		stream.avail_in +=int(bread);
		auto ret=mz_inflate(&stream, MZ_SYNC_FLUSH);
		bytes_left -= bread;

		if (bytes_left != 0 && stream.avail_out == 0)
			return false;

		if (ret == MZ_PARAM_ERROR || ret == MZ_DATA_ERROR)
			return false;

		if (ret == MZ_STREAM_END)
			break;
	}
	mz_inflateEnd(&stream);

	auto cptr = buffer.data();
	rex_header h = *reinterpret_cast<rex_header*>(cptr);
	cptr += sizeof(rex_header);

	//TODO: check header version
	layers.resize(h.layer_count);

	for (size_t i = 0; i < h.layer_count; i++)
	{
		auto& l = layers[i];
		auto l_h = *reinterpret_cast<rex_layer_header*>(cptr);
		cptr += sizeof(rex_layer_header);
		l.resize(l_h.width, l_h.height);
		for (auto t = 0; t < l.data.size(); t++)
		{
			auto p = *reinterpret_cast<rex_pixel*>(cptr);
			cptr += sizeof(rex_pixel);
			l.data[t].tile = p.tile_image;
			l.data[t].fore_color = v3c(p.fore[0], p.fore[1], p.fore[2]);
			l.data[t].back_color= v3c(p.back[0], p.back[1], p.back[2]);
		}
	}
	return true;
}
#endif
