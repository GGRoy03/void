#pragma once

// TODO: Remove dependencies on these
#include <stdint.h>
#include <stdbool.h> 
#include <string.h>  // memcpy, memset, ...
#include <stdlib.h>  // malloc, free, ...
#include <intrin.h>  // SIMD
#include <float.h>   // Numeric Limits
#include <math.h>    // Min/Max
#include <stdio.h>   // Reading files 

typedef float unused; // Visual Studio parsing bug.

// Third party stuff.
#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

typedef float    cim_f32;
typedef double   cim_f64;
typedef uint8_t  cim_u8;
typedef uint16_t cim_u16;
typedef uint32_t cim_u32;
typedef uint64_t cim_u64;
typedef int      cim_i32;
typedef cim_u32  cim_bit_field;

#define Cim_Unused(Arg) (void*)Arg
#define Cim_Assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)
#define Cim_ArrayCount(Arr) (sizeof(Arr) / sizeof((Arr)[0]))
#define Cim_IsPowerOfTwo(Value) (((Value) & ((Value) - 1)) == 0)
#define Cim_SafeRatio(A, B) ((B) ? ((A)/(B)) : (A))

typedef struct cim_vector2
{
    cim_f32 x, y;
} cim_vector2;

typedef struct cim_vector4
{
    cim_f32 x, y, z, w;
} cim_vector4;

typedef struct cim_rect
{
    cim_u32 MinX;
    cim_u32 MinY;
    cim_u32 MaxX;
    cim_u32 MaxY;
} cim_rect;

typedef enum UIPass_Type
{
    UIPass_Layout = 0,
    UIPass_Event  = 1,
    UIPass_Draw   = 2,
    UIPass_Ended  = 3,
} UIPass_Type;

// Header files
#include "interface/cim_helpers.h"  // Depends on [nothing] type-wise.
#include "interface/cim_style.h"    // Depends on [nothing] type-wise.
#include "interface/cim_platform.h" // Depends on [helpers] type-wise.
#include "interface/cim_renderer.h" // Depends on [helpers & platform] type-wise.
#include "interface/cim_layout.h"   // Depends on [style & renderer] type-wise.

typedef struct cim_context
{
    // Global Systems
    cim_layout           Layout;
    cim_renderer         Renderer;
    cim_inputs           Inputs;
    ui_draw_batch_buffer Batches;

    // Current State
    ui_font *CurrentFont;

    void *Backend;
} cim_context;

static cim_context *CimCurrent;

#define UI_STATE      (CimCurrent->State)
#define UI_LAYOUT     (CimCurrent->Layout)
#define UIP_LAYOUT   &(CimCurrent->Layout)
#define UI_INPUT      (CimCurrent->Inputs)
#define UIP_INPUT    &(CimCurrent->Inputs)
#define UI_BATCHES    (CimCurrent->Batches)
#define UIP_BATCHES  &(CimCurrent->Batches)
#define UI_RENDERER   (CimCurrent->Renderer)
#define UIP_RENDERER &(CimCurrent->Renderer)

// TODO: Figure out dependencies.

// Implementation files
#include "implementation/cim_helpers.cpp"
#include "implementation/cim_styles.cpp"
#include "implementation/cim_platform.cpp"
#include "implementation/cim_text.cpp"
#include "implementation/cim_layout.cpp"
#include "implementation/cim_renderer.cpp"
#include "implementation/cim_component.cpp"

// [COMPONENTS]

// [Layout]

#define UIRowBlock() for (int UNIQUE_UI_ONCE = (UIRow(), 1); UNIQUE_UI_ONCE; UIEndRow(), UNIQUE_UI_ONCE = 0)

// [Window]
static bool 
UIWindow (const char *Id, const char *ThemeId, ui_component_state *State);

// [Buttons]
#define UIButton(Id, ThemeId, Text, State)           ButtonComponent(Id, ThemeId, Text, State, 0)
#define UIIndexedButton(ThemeId, Text, State, Index) ButtonComponent(NULL, ThemeId, Text, State, Index)

static void 
ButtonComponent(const char *Id, const char *ThemeId, const char *Text, ui_component_state *State, cim_u32 Index);

// [Text]
static void 
#define UIText(Text) TextComponent(Text, NULL, CimCurrent->CurrentFont)
TextComponent(const char *Text, ui_layout_node *LayoutNode, ui_font *Font);

// [CONTEXT MANAGEMENT]
#define UIBeginContext() for (UIPass_Type Pass = UIPass_Layout; Pass !=  UIPass_Ended; Pass = (UIPass_Type)((cim_u32)Pass + 1))
#define UIEndContext()   if (Pass == UIPass_Layout) {UIEndLayout();} else if (Pass == UIPass_Draw) {UIEndDraw(UIP_LAYOUT.Tree.DrawList);}

static void
UIBeginFrame()
{
    // Obviously wouldn't really do this but prototyping.
    cim_layout *Layout = UIP_LAYOUT;
    Layout->Tree.NodeCount = 0;
    Layout->Tree.AtParent = 0;
    Layout->Tree.ParentStack[0] = CimLayout_InvalidNode; // Temp.
}

// TODO: Make this call the renderer draw function.
static void
UIEndFrame()
{
    cim_inputs *Inputs = UIP_INPUT;
    Inputs->ScrollDelta = 0;
    Inputs->MouseDeltaX = 0;
    Inputs->MouseDeltaY = 0;

    for (cim_u32 Idx = 0; Idx < CimIO_KeyboardKeyCount; Idx++)
    {
        Inputs->Buttons[Idx].HalfTransitionCount = 0;
    }

    for (cim_u32 Idx = 0; Idx < CimMouse_ButtonCount; Idx++)
    {
        Inputs->MouseButtons[Idx].HalfTransitionCount = 0;
    }
}

static bool
UIInitializeContext(cim_context *UserContext)
{
    if (!UserContext)
    {
        return false;
    }

    memset(UserContext, 0, sizeof(cim_context));
    CimCurrent = UserContext;

    return true;
}

// [TEXT]

static ui_font * UILoadFont    (const char *FileName, cim_f32 FontSize, UIFont_Mode Mode);
static void      UIUnloadFont  (ui_font *Font);
static void      UISetFont     (ui_font *Font);