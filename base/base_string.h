#pragma once

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

internal byte_string ByteString  (u8  *String, u64 Size);
internal wide_string WideString  (u16 *String, u64 Size);

// [Character Utilities]

internal b32  IsAlpha            (u8 Char);
internal b32  IsNumber           (u8 Char);
internal b32  IsWhiteSpace       (u8 Char);
internal u8   ToLowerChar        (u8 Char);
internal b32  ByteStringMatches  (byte_string Str1, byte_string Str2);

// [Encoding/Decoding]

internal unicode_decode DecodeByteString  (u8 *String, u64 Maximum);
internal u32            EncodeWideString  (u16 *WideString, u32 CodePoint);

// [Conversion]

internal wide_string ByteStringToWideString(memory_arena *Arena, byte_string Input);