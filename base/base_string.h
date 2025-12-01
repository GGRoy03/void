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
    uint8_t *String;
    uint64_t Size;
} byte_string;

struct compile_byte_string
{
    const char *String;
    uint64_t    Size;
};

typedef struct wide_string
{
    uint16_t *String;
    uint64_t  Size;
} wide_string;

typedef struct unicode_decode
{
    uint32_t Increment;
    uint32_t Codepoint;
} unicode_decode;

// [API]

#define str8_lit(String)  ByteString((uint8_t *)String, sizeof(String) - 1)

#define byte_string_literal(String) ByteString((uint8_t *)String, sizeof(String) - 1)
#define byte_string_compile(String) {(uint8_t *)String, sizeof(String) - 1}

// [Constructors]

static byte_string         ByteString  (uint8_t *String, uint64_t Size);
static compile_byte_string CompileByteString (const char *String, uint64_t Size);
static wide_string         WideString  (uint16_t *String, uint64_t Size);

// [String Utilities]

static bool         IsValidByteString   (byte_string Input);
static bool         IsAsciiString       (byte_string Input);
static bool         ByteStringMatches   (byte_string A, byte_string B, uint32_t Flags);
static byte_string ByteStringCopy      (byte_string String, memory_arena *Arena);
static byte_string ByteStringAppend    (byte_string Target, byte_string Source, uint64_t At,memory_arena *Arena);
static void        ByteStringAppendTo  (byte_string Target, byte_string Source, uint64_t At);

bool         IsValidWideString   (wide_string Input);
static bool         WideStringMatches   (wide_string A, wide_string B, uint32_t Flags);

// [Character Utilities]

static bool IsAlpha       (uint8_t Char);
static bool IsDigit       (uint8_t Char);
static bool IsWhiteSpace  (uint8_t Char);
static bool IsNewLine     (uint8_t Char);
static uint8_t  ToLowerChar   (uint8_t Char);

// [Encoding/Decoding]

unicode_decode DecodeByteString  (uint8_t *String, uint64_t Maximum);

static uint32_t EncodeWideString    (uint16_t *WideString, uint32_t CodePoint);

// [Conversion]

wide_string ByteStringToWideString(memory_arena *Arena, byte_string Input);
static byte_string WideStringToByteString(wide_string Input, memory_arena *Arena);

// [Hashes]

static uint64_t HashByteString(byte_string Input);
