// [Third-Party Libraries]

#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define XXH_NO_STREAM
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "third_party/xxhash.h"

#include "third_party/nxi_log.h"

// [Headers]

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "render/render_inc.h"
#include "ui/ui_inc.h"

// [Source Files]

#include "base/base_inc.cpp"
#include "os/os_inc.cpp"
#include "render/render_inc.cpp"
#include "ui/ui_inc.cpp"
