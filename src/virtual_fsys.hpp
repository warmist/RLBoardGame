#pragma once


bool vfilesys_load_file(const char* asset_dir,const char* filename,char** buffer,size_t& size,bool& needs_free);