#pragma once

namespace StyleParser
{

static ui_cached_style_list
LoadStyles(os_read_file *Files, uint32_t FileCount, memory_arena *OutputArena);

}
