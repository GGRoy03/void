typedef struct ID2D1RenderTarget    ID2D1RenderTarget;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;
typedef struct IDWriteTextFormat    IDWriteTextFormat;
typedef struct IDWriteFontFace      IDWriteFontFace;

typedef struct os_font_context
{
    IDWriteTextFormat    *TextFormat;
    IDWriteFontFace      *FontFace;
    ID2D1RenderTarget    *RenderTarget;
    ID2D1SolidColorBrush *FillBrush;
} os_font_context;

void OSWin32InitializeDWriteFactory  (IDWriteFactory **OutFactory);
