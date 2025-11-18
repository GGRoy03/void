// [General Notes]
// 1) Functions marked as 'internal' are only used from within files included in this file.
// 2) Functions marked as 'external' are both used from within files included in this file AND used outside of this file.

// [Third-Party Libraries]

#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

// [Headers]

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "render/render_inc.h"
#include "ui/ui_inc.h"

// [Source Files]

#include "base/base_inc.c"
#include "os/os_inc.c"
#include "render/render_inc.c"
#include "ui/ui_inc.c"
