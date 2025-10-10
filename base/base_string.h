#pragma once

// [CONSTANTS]

typedef enum StringMatch_Flag
{
    StringMatch_NoFlag        = 0,
    StringMatch_CaseSensitive = 1,
} StringMatch_Flag;

// [CORE TYPES]

typedef struct byte_string
{
    u8 *String;
    u64 Size;
} byte_string;

typedef struct wide_string
{
    u16 *String;
    u64  Size;
} wide_string;

typedef struct unicode_decode
{
    u32 Increment;
    u32 Codepoint;
} unicode_decode;

// [API]

#define byte_string_literal(String) ByteString((u8 *)String, sizeof(String) - 1)
#define byte_string_compile(String) {(u8 *)String, sizeof(String) - 1}
#define wide_string_literal(String) WideString((u16 *)String, (sizeof(String) - 2) / 2) // NOTE: Might be broken?

// [Constructors]

external byte_string ByteString  (u8 *String, u64 Size);
internal wide_string WideString  (u16 *String, u64 Size);

// [String Utilities]

internal b32         IsValidByteString       (byte_string Input);
internal b32         IsAsciiString           (byte_string Input);
internal b32         ByteStringMatches       (byte_string Str1, byte_string Str2, bit_field Flags);
internal byte_string ByteStringCopy          (byte_string String, memory_arena *Arena);
internal byte_string ByteStringAppendBefore  (byte_string Pre, byte_string Post, memory_arena *Arena);

external b32         IsValidWideString       (wide_string Input);
internal b32         WideStringMatches       (wide_string A, wide_string B, bit_field Flags);
internal wide_string WideStringAppendBefore  (wide_string Pre, wide_string Post, memory_arena *Arena);

// [Character Utilities]

internal b32  IsAlpha       (u8 Char);
internal b32  IsDigit       (u8 Char);
internal b32  IsWhiteSpace  (u8 Char);
internal u8   ToLowerChar   (u8 Char);

// [Encoding/Decoding]

external unicode_decode DecodeByteString  (u8 *String, u64 Maximum);
internal u32            EncodeWideString  (u16 *WideString, u32 CodePoint);

// [Conversion]

external wide_string ByteStringToWideString(memory_arena *Arena, byte_string Input);

// [Hashes]

internal u64 HashByteString(byte_string Input);
