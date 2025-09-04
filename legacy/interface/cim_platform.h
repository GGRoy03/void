#pragma once

// [Enums & Constants]

#define CimLog_Info(...)  PlatformLogMessage(CimLog_Info   , __FILE__, __LINE__, __VA_ARGS__);
#define CimLog_Warn(...)  PlatformLogMessage(CimLog_Warning, __FILE__, __LINE__, __VA_ARGS__)
#define CimLog_Error(...) PlatformLogMessage(CimLog_Error  , __FILE__, __LINE__, __VA_ARGS__)
#define CimLog_Fatal(...) PlatformLogMessage(CimLog_Fatal  , __FILE__, __LINE__, __VA_ARGS__)

#define CimIO_MaxPath 256
#define CimIO_KeyboardKeyCount 256
#define CimIO_KeybordEventByufferSize 128

#define UIText_ExtendedASCIITableSize 256
#define UIText_ValidateLRU 0

typedef enum CimLog_Severity
{
    CimLog_Info    = 0,
    CimLog_Warning = 1,
    CimLog_Error   = 2,
    CimLog_Fatal   = 3,
} CimLog_Severity;

typedef enum CimMouse_Button
{
    CimMouse_Left        = 0,
    CimMouse_Right       = 1,
    CimMouse_Middle      = 2,
    CimMouse_ButtonCount = 3,
} CimMouse_Button;

typedef enum UIFont_Mode
{
    UIFont_ExtendedASCIIDirectMapping = 0,
} UIFont_Mode;

// Forward Declarations

typedef struct os_font_backend os_font_backend;
typedef struct gpu_font_backend gpu_font_backend;
typedef struct glyph_entry glyph_entry;
typedef struct glyph_table glyph_table;
typedef struct direct_glyph_table direct_glyph_table;

// [Types]

typedef struct cim_button_state
{
    bool    EndedDown;
    cim_u32 HalfTransitionCount;
} cim_button_state;

typedef struct cim_keyboard_event
{
    cim_u8 VKCode;
    bool   IsDown;
} cim_keyboard_event;

typedef struct cim_inputs
{
    cim_button_state Buttons[CimIO_KeyboardKeyCount];
    cim_f32          ScrollDelta;
    cim_i32          MouseX, MouseY;
    cim_i32          MouseDeltaX, MouseDeltaY;
    cim_button_state MouseButtons[5];
} cim_inputs;

typedef struct cim_watched_file
{
    char FileName[CimIO_MaxPath];
    char FullPath[CimIO_MaxPath];
} cim_watched_file;

typedef struct dir_watcher_context
{
    char              Directory[CimIO_MaxPath];
    cim_watched_file *Files;
    cim_u32           FileCount;
    cim_u32 volatile  RefCount;  // NOTE: Does this need volatile?
} dir_watcher_context;

typedef struct ui_texture_coord
{
    cim_f32 u0, v0, u1, v1;
} ui_texture_coord;

typedef struct glyph_hash
{
    cim_u32 Value;
} glyph_hash;

typedef struct glyph_size
{
    cim_u16 Width;
    cim_u16 Height;
}  glyph_size;

typedef struct glyph_table_params
{
    cim_u32 HashCount;
    cim_u32 EntryCount;
} glyph_table_params;

typedef struct glyph_table_stats
{
    size_t HitCount;
    size_t MissCount;
    size_t RecycleCount;
} glyph_table_stats;

typedef struct glyph_info
{
    cim_u32          MapId;
    bool             IsInAtlas;
    ui_texture_coord TexCoord;
    glyph_size       Size;
    cim_vector2      Offsets;
    cim_f32          AdvanceX;
} glyph_info;

typedef struct glyph_atlas
{
    cim_u32          TableId;
    bool             IsInAtlas;
    ui_texture_coord TexCoord;
} glyph_atlas;

typedef struct glyph_layout
{
    glyph_size  Size;
    cim_vector2 Offsets;
    cim_f32     AdvanceX;
} glyph_layout;

typedef struct ui_font
{
    bool        IsValid;
    UIFont_Mode Mode;

    // Layout data
    cim_f32 LineHeight;
    cim_f32 Size;

    // 2D Allocator
    stbrp_context AtlasContext;
    stbrp_node   *AtlasNodes;
    cim_u32       AtlasHeight;
    cim_u32       AtlasWidth;

    // Backend objects
    gpu_font_backend *GPUBackend;
    os_font_backend  *OSBackend;
    void *FreePointer;

    // Tables
    union
    {
        direct_glyph_table *Direct;
        glyph_table        *LRU;
    } Table;
} ui_font;

// [Public API]

// IO/Misc
static bool   PlatformInit        (const char *StyleDir);
static buffer PlatformReadFile    (char *FileName);
static void   PlatformLogMessage  (CimLog_Severity Level, const char *File, cim_i32 Line, const char *Format, ...);

// Font/Text
static bool         OSAcquireFontBackend  (const char *FontName, cim_f32 FontSize, void *TransferSurface, ui_font *Font);
static void         OSReleaseFontBackend  (os_font_backend *Backend);
static void         OSRasterizeGlyph      (char Character, stbrp_rect Rect, ui_font *Font);
static glyph_layout OSGetGlyphLayout      (char Character, ui_font *Font);