#pragma once
#include <string>
#include <asset_lib.h>

namespace zec::asset_converter
{
    void load_texture_from_file(const char* file_path, TextureDesc& out_desc, void* out_data);
}