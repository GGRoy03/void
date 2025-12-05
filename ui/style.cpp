// NOTE:
// This code is so small. Almost feels like we can move it?

// ------------------------------------------------------------------------------------
// Style Manipulation Public API

// NOTE:
// Passing pipeline here is suspicious. This knows too much? Like...
// This should just be on the fucking... Layout tree right? And helpers
// moved to paint? Then, also move, uuuhh, style.h into paint.h?

// [Helpers]

static bool
IsVisibleColor(ui_color Color)
{
    bool Result = (Color.A > 0.f);
    return Result;
}

static ui_color
NormalizeColor(ui_color Color)
{
    float Inverse = 1.f / 255.f;

    ui_color Result =
    {
        .R = Color.R * Inverse,
        .G = Color.G * Inverse,
        .B = Color.B * Inverse,
        .A = Color.A * Inverse,
    };

    return Result;
}
