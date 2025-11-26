// [Constructors]

static byte_string 
ByteString(uint8_t *String, uint64_t Size)
{
    byte_string Result = { String, Size };
    return Result;
}

static wide_string
WideString(uint16_t *String, uint64_t Size)
{
    wide_string Result = { String, Size };
    return Result;
}

// [String Utilities]

static bool
IsValidByteString(byte_string Input)
{
    bool Result = (Input.String) && (Input.Size);
    return Result;
}

static bool
IsAsciiString(byte_string Input)
{
    bool Result = 1;

    for(uint32_t Idx = 0; Idx < Input.Size; ++Idx)
    {
        if(Input.String[Idx] > 0x7F)
        {
            Result = 0;
            break;
        }
    }

    return Result;
}

static bool 
ByteStringMatches(byte_string Str1, byte_string Str2, uint32_t Flags)
{
    VOID_ASSERT(Flags >= StringMatch_NoFlag && Flags <= StringMatch_CaseSensitive);

    bool Result = 0;

    if (Str1.Size == Str2.Size)
    {
        uint8_t *Pointer1 = Str1.String;
        uint8_t *Pointer2 = Str2.String;
        uint8_t *End      = Str1.String + Str1.Size;

        if (Flags & StringMatch_CaseSensitive)
        {
            while (Pointer1 < End && *Pointer1++ == *Pointer2++) {};
        }
        else
        {
            while (Pointer1 < End)
            {
                uint8_t Char1 = ToLowerChar(*Pointer1);
                uint8_t Char2 = ToLowerChar(*Pointer2);

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

static byte_string 
ByteStringCopy(byte_string Input, memory_arena *Arena)
{
    byte_string Result = ByteString(PushArray(Arena, uint8_t, Input.Size), Input.Size);
    memcpy(Result.String, Input.String, Input.Size);

    return Result;
}

static byte_string
ByteStringAppend(byte_string Target, byte_string Source, uint64_t At, memory_arena *Arena)
{
    byte_string Result = ByteString(0, 0);

    if(At < Target.Size)
    {
        uint64_t Size = Target.Size + Source.Size;
        uint8_t *Str  = PushArray(Arena, uint8_t, Size);

        Result = ByteString(Str, Size);

        // Copy from target[0]..At into the result buffer.
        for(uint32_t Idx = 0; Idx < At; ++Idx)
        {
            Result.String[Idx] = Target.String[Idx];
        }

        // Append the source string into the result buffer.
        uint32_t SourceIdx = 0;
        for(uint32_t Idx = At; SourceIdx < Source.Size; ++Idx)
        {
            Result.String[Idx] = Source.String[SourceIdx++];
        }

        // Copy the rest of target into the result buffer.
        uint32_t TargetIdx = At;
        for(uint32_t Idx = At + Source.Size; TargetIdx < Target.Size; ++Idx)
        {
            Result.String[Idx] = Target.String[TargetIdx++];
        }
    }

    return Result;
}

static void
ByteStringAppendTo(byte_string Target, byte_string Source, uint64_t At)
{
    VOID_ASSERT(IsValidByteString(Target));
    VOID_ASSERT(IsValidByteString(Source));

    if(At + Source.Size < Target.Size)
    {
        // [Target.String || Source.String || Target.String]
        //  DO NOT TOUCH
        //                                     COPY PAST APPEND
        //                       APPEND

        uint32_t TargetIdx = At;
        uint32_t WriteIdx  = At + Source.Size;
        while (TargetIdx < Target.Size && Target.String[TargetIdx] != '\0')
        {
            Target.String[WriteIdx++] = Target.String[TargetIdx++];
        }

        uint32_t SourceIdx = 0;
        for(uint32_t Idx = At; SourceIdx < Source.Size; ++Idx)
        {
            Target.String[Idx] = Source.String[SourceIdx++];
        }
    }
}

static bool
IsValidWideString(wide_string Input)
{
    bool Result = (Input.String) && (Input.Size);
    return Result;
}

static wide_string
WideStringAppendBefore(wide_string Pre, wide_string Post, memory_arena *Arena)
{
    uint64_t         Size   = Pre.Size + Post.Size;
    wide_string Result = WideString(PushArray(Arena, uint16_t, Size), Size);

    memcpy(Result.String           , Pre.String , Pre.Size  * sizeof(uint16_t));
    memcpy(Result.String + Pre.Size, Post.String, Post.Size * sizeof(uint16_t));

    return Result;
}

// [Character Utilities]

static bool
IsAlpha(uint8_t Char)
{
    bool Result = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z');
    return Result;
}

static bool
IsDigit(uint8_t Char)
{
    bool Result = (Char >= '0' && Char <= '9');
    return Result;
}

static bool
IsWhiteSpace(uint8_t Char)
{
    bool Result = (Char == ' ') || (Char == '\t') || (Char == '\0');
    return Result;
}

static bool
IsNewLine(uint8_t Char)
{
    bool Result = (Char == '\n' || Char == '\r');
    return Result;
}

static uint8_t
ToLowerChar(uint8_t Char)
{
    uint8_t Result = Char;

    if(IsAlpha(Char))
    {
        Result = '\0';
        if (Char < 'a')
        {
            Result = Char + ('a' - 'A');
        }
        else
        {
            Result = Char;
        }
    }

    return Result;
}

// [Encoding/Decoding]

const static uint8_t ByteStringClass[32] = 
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

static unicode_decode 
DecodeByteString(uint8_t *ByteString, uint64_t Maximum)
{
    unicode_decode Result = { 1, _UI32_MAX};

    uint8_t Byte      = ByteString[0];
    uint8_t ByteClass = ByteStringClass[Byte >> 3];

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
            uint8_t ContByte = ByteString[1];
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
            uint8_t ContByte[2] = { ByteString[1], ByteString[2] };
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
            uint8_t ContByte[3] = { ByteString[1], ByteString[2], ByteString[3] };
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

static unicode_decode
DecodeWideString(uint16_t *String, uint64_t Max)
{
    unicode_decode Result = {1, _UI32_MAX};

    Result.Codepoint = String[0];
    Result.Increment = 1;

    if(Max > 1 && 0xD800 <= String[0] && String[0] < 0xDC00 && 0xDC00 <= String[1] && String[1] < 0xE000)
    {
        Result.Codepoint = ((String[0] - 0xD800) << 10) | ((String[1] - 0xDC00) + 0x10000);
        Result.Increment = 2;
  }

  return Result;
}

// Encoding Byte Strings

static uint32_t
EncodeByteString(uint8_t *String, uint32_t CodePoint)
{
    uint32_t Increment = 0;
    if(CodePoint <= 0x7F)
    {
        String[0] = (uint8_t)CodePoint;
        Increment = 1;
    }
    else if(CodePoint <= 0x7FF)
    {
        String[0] = (VOID_BITMASK(2) << 6) | (CodePoint >> 6 & VOID_BITMASK(5));
        String[1] = (VOID_BIT(8)         ) | (CodePoint >> 0 & VOID_BITMASK(6));
        Increment = 2;
    }
    else if(CodePoint <= 0xFFFF)
    {
        String[0] = (VOID_BITMASK(3) << 5) | ((CodePoint >> 12) & VOID_BITMASK(4));
        String[1] = (VOID_BIT(8)         ) | ((CodePoint >> 6 ) & VOID_BITMASK(6));
        String[2] = (VOID_BIT(8)         ) | ((CodePoint >> 0 ) & VOID_BITMASK(6));
        Increment = 3;
    }
    else if(CodePoint <= 0x10FFFF)
    {
        String[0] = (VOID_BITMASK(4) << 4) | ((CodePoint >> 18) & VOID_BITMASK(3));
        String[1] = (VOID_BIT(8)         ) | ((CodePoint >> 12) & VOID_BITMASK(6));
        String[2] = (VOID_BIT(8)         ) | ((CodePoint >> 6 ) & VOID_BITMASK(6));
        String[3] = (VOID_BIT(8)         ) | ((CodePoint >> 0 ) & VOID_BITMASK(6));
        Increment = 4;
    }
    else
    {
        String[0] = '?';
        Increment = 1;
    }

    return Increment;
}

// Encoding Wide Strings
// May be encoded as a single code unit (16 bits) or two (32 bits surrogate pair)
// Code Point == uint32_t_MAX -> Invalid
// Code Point <  uint16_t_MAX -> Single Code Unit
// Code Point >= uint16_t_MAX -> Two Code Units
// Surrogate Pairs (Supplementary Planes) are stored using 20 bits indices (High range - Low Range + 1) = 2^20
// UTF-16 uses 0XD800..0XDBFF to specify that the lower 10 bits are the payload for the high surrogate (index)
// UTF-16 uses 0XDC00..0XDFFF to specify that the lower 10 bits are the payload for the low  surrogate (index)
// CodeUnit 0: 0XD800 + (SuppIndex >> 10)       (High Bits)
// CodeUnit 1: 0XDC00 + (SuppIndex & BitMask10) (Low  Bits)

static uint32_t
EncodeWideString(uint16_t *WideString, uint32_t CodePoint)
{
    uint32_t Increment = 1;

    if (CodePoint == _UI32_MAX)
    {
        WideString[0] = (uint16_t)'?';
    }
    else if (CodePoint <= 0xFFFF)
    {
        WideString[0] = (uint16_t)CodePoint;
    }
    else
    {
        uint32_t SuppIndex = CodePoint - 0x10000;
        WideString[0] = (uint16_t)(0xD800 + (SuppIndex >> 10));
        WideString[1] = (uint16_t)(0XDC00 + (SuppIndex & 0b0000001111111111));
        Increment = 2;
    }

    return Increment;
}

// [Conversion]

static wide_string 
ByteStringToWideString(memory_arena *Arena, byte_string Input)
{
    wide_string Result = WideString(0, 0);

    if (Input.Size)
    {
        uint64_t  Size     = 0;
        uint64_t  Capacity = Input.Size * 2;
        uint16_t *String   = PushArray(Arena, uint16_t, Capacity);
        uint8_t  *Start    = Input.String;
        uint8_t  *End      = Input.String + Input.Size;

        unicode_decode Consume = {0};
        for (; Start < End; Start += Consume.Increment)
        {
            Consume = DecodeByteString(Start, End - Start);
            Size   += EncodeWideString(String + Size, Consume.Codepoint);
        }

        uint64_t Overflow = (Capacity - Size) * 2;
        PopArena(Arena, Overflow);

        String[Size] = 0;
        Result       = WideString(String, Size);
    }

    return Result;
}

static byte_string
WideStringToByteString(wide_string Input, memory_arena *Arena)
{
    byte_string Result = ByteString(0, 0);

    if(Input.Size)
    {
        uint64_t Size     = 0;
        uint64_t Capacity = Input.Size * 3;
        uint8_t *String   = PushArray(Arena, uint8_t, Capacity + 1);

        uint16_t *Start = Input.String;
        uint16_t *End   = Start + Input.Size;

        unicode_decode Consume = {0};
        for(; Start < End; Start += Consume.Increment)
        {
            Consume = DecodeWideString(Start, End - Start);
            Size   += EncodeByteString(String + Size, Consume.Codepoint);
        }

        String[Size] = '\0';

        uint64_t Overflow = (Capacity - Size);
        PopArena(Arena, Overflow);

        Result = ByteString(String, Size);
    }

    return Result;
}

// [Hashes]

static uint64_t
HashByteString(byte_string Input)
{
    uint64_t Result = XXH3_64bits(Input.String, Input.Size);
    return Result;
}
