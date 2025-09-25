// [API IMPLEMENTATION]

// [Constructors]

external byte_string 
ByteString(u8 *String, u64 Size)
{
    byte_string Result = { String, Size };
    return Result;
}

internal wide_string
WideString(u16 *String, u64 Size)
{
    wide_string Result = { String, Size };
    return Result;
}

// [String Utilities]

internal b32
IsValidByteString(byte_string Input)
{
    b32 Result = (Input.String) && (Input.Size);
    return Result;
}
// [Character Utilities]

static b32
IsAlpha(u8 Char)
{
    b32 Result = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z');
    return Result;
}

static b32
IsDigit(u8 Char)
{
    b32 Result = (Char >= '0' && Char <= '9');
    return Result;
}

static b32
IsWhiteSpace(u8 Char)
{
    b32 Result = (Char == ' ') || (Char == '\t');
    return Result;
}

static u8 
ToLowerChar(u8 Char)
{
    Assert(IsAlpha(Char));

    u8 Result = '\0';
    if (Char < 'a')
    {
        Result = Char + ('a' - 'A');
    }
    else
    {
        Result = Char;
    }

    return Result;
}

internal b32 
ByteStringMatches(byte_string Str1, byte_string Str2, bit_field Flags)
{   Assert(Flags >= StringMatch_NoFlag && Flags <= StringMatch_CaseSensitive);

    b32 Result = 0;

    if (Str1.Size == Str2.Size)
    {
        u8 *Pointer1 = Str1.String;
        u8 *Pointer2 = Str2.String;
        u8 *End      = Str1.String + Str1.Size;

        if (Flags & StringMatch_CaseSensitive)
        {
            while (Pointer1 < End && *Pointer1++ == *Pointer2++) {};
        }
        else
        {
            while (Pointer1 < End)
            {
                u8 Char1 = *Pointer1;
                u8 Char2 = *Pointer2;

                if (IsAlpha(Char1)) Char1 = ToLowerChar(Char1);
                if (IsAlpha(Char2)) Char2 = ToLowerChar(Char2);

                if (Char1 == Char2)
                {
                    ++Pointer1;
                    ++Pointer2;
                }
                else
                {
                    break;
                }
            }
        }

        Result = (Pointer1 == End);
    }

    return Result;
}

// [Encoding/Decoding]

read_only global u8 ByteStringClass[32] = 
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
};

// Decoding the byte class
// 0xxx xxxx >> 3 == 0000 xxxx | Range = 0..15  (ASCII)
// 10xx xxxx >> 3 == 0001 0xxx | Range = 16..23 (ContByte/Invalid)
// 110x xxxx >> 3 == 0001 10xx | Range = 24..27 (2 bytes)
// 1110 xxxx >> 3 == 0001 110x | Range = 28..29 (3 bytes)
// 1111 0xxx >> 3 == 0001 1110 | Range = 30     (4 bytes)
// 1111 1xxx >> 3 == 0001 1111 | Range = 32     (Invalid)

// Decoding code point (x represents payload).
// Always mask payload size in byte and shift by sum of cont bytes payload size.
// 1 byte -> 0xxx xxxx                               -> Code Point == (Byte0)
// 2 byte -> 110x xxxx 10yy yyyy                     -> Code Point == (Byte0 & BitMask5) << 6  | (Byte1 & BitMask6)
// 3 byte -> 1110 xxxx 10yy yyyy 10zz zzzz           -> Code Point == (Byte0 & BitMask4) << 12 | (Byte1 & BitMask6) << 6  | (Byte2 & BitMask6)
// 4 byte -> 1111 1xxx 10yy yyyy 10zz zzzz 10ww wwww -> Code Point == (Byte0 & BitMask3) << 18 | (Byte1 & BitMask6) << 12 | (Byte2 & BitMask6) << 6 | (Byte3 & BitMask6)

internal unicode_decode 
DecodeByteString(u8 *ByteString, u64 Maximum)
{
    unicode_decode Result = { 1, _UI32_MAX };

    u8 Byte      = ByteString[0];
    u8 ByteClass = ByteStringClass[Byte >> 3];

    switch (ByteClass)
    {

    case 1:
    {
        Result.Codepoint = Byte;
    } break;

    case 2:
    {
        if (1 < Maximum)
        {
            u8 ContByte = ByteString[1];
            if (ByteStringClass[ContByte >> 3] == 0)
            {
                Result.Codepoint  = (Byte     & 0b00011111) << 6;
                Result.Codepoint |= (ContByte & 0b00111111) << 0;
                Result.Increment  = 2;
            }
        }
    } break;

    case 3:
    {
        if (2 < Maximum)
        {
            u8 ContByte[2] = { ByteString[1], ByteString[2] };
            if (ByteStringClass[ContByte[0] >> 3] == 0 && ByteStringClass[ContByte[1] >> 3] == 0)
            {
                Result.Codepoint  = ((Byte        & 0b00001111) << 12);
                Result.Codepoint |= ((ContByte[0] & 0b00111111) << 6 );
                Result.Codepoint |= ((ContByte[1] & 0b00111111) << 0 );
                Result.Increment  = 3;
            }
        }
    } break;

    case 4:
    {
        if (3 < Maximum)
        {
            u8 ContByte[3] = { ByteString[1], ByteString[2], ByteString[3] };
            if (ByteStringClass[ContByte[0] >> 3] == 0 && ByteStringClass[ContByte[1] >> 3] == 0 && ByteStringClass[ContByte[2] >> 3] == 0)
            {
                Result.Codepoint  = (Byte        & 0b00000111) << 18;
                Result.Codepoint |= (ContByte[0] & 0b00111111) << 12;
                Result.Codepoint |= (ContByte[1] & 0b00111111) << 6;
                Result.Codepoint |= (ContByte[2] & 0b00111111) << 0;
                Result.Increment = 4;
            }
        }
    } break;

    }

    return Result;
}

// Encoding Wide Strings
// May be encoded as a single code unit (16 bits) or two (32 bits surrogate pair)
// Code Point == U32_MAX -> Invalid
// Code Point <  U16_MAX -> Single Code Unit
// Code Point >= U16_MAX -> Two Code Units
// Surrogate Pairs (Supplementary Planes) are stored using 20 bits indices (High range - Low Range + 1) = 2^20
// UTF-16 uses 0XD800..0XDBFF to specify that the lower 10 bits are the payload for the high surrogate (index)
// UTF-16 uses 0XDC00..0XDFFF to specify that the lower 10 bits are the payload for the low  surrogate (index)
// CodeUnit 0: 0XD800 + (SuppIndex >> 10)       (High Bits)
// CodeUnit 1: 0XDC00 + (SuppIndex & BitMask10) (Low  Bits)

internal u32
EncodeWideString(u16 *WideString, u32 CodePoint)
{
    u32 Increment = 1;

    if (CodePoint == _UI32_MAX)
    {
        WideString[0] = (u16)'?';
    }
    else if (CodePoint <= 0xFFFF)
    {
        WideString[0] = (u16)CodePoint;
    }
    else
    {
        u32 SuppIndex = CodePoint - 0x10000;
        WideString[0] = (u16)(0xD800 + (SuppIndex >> 10));
        WideString[1] = (u16)(0XDC00 + (SuppIndex & 0b0000001111111111));
        Increment = 2;
    }

    return Increment;
}

// [Conversion]

external wide_string 
ByteStringToWideString(memory_arena *Arena, byte_string Input)
{
    wide_string Result = WideString(0, 0);

    if (Input.Size)
    {
        u64  Size     = 0;
        u64  Capacity = Input.Size * 2;
        u16 *String   = PushArray(Arena, u16, Capacity);
        u8  *Start    = Input.String;
        u8  *End      = Input.String + Input.Size;

        unicode_decode Consume = {0};
        for (; Start < End; Start += Consume.Increment)
        {
            Consume = DecodeByteString(Start, End - Start);
            Size   += EncodeWideString(String + Size, Consume.Codepoint);
        }

        u64 OverflowAllocation = (Capacity - Size) * 2;
        PopArena(Arena, OverflowAllocation);

        String[Size] = 0;
        Result       = WideString(String, Size);
    }

    return Result;
}