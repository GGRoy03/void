// [API]

internal ui_style_registry *
CreateStyleRegistry(byte_string FileName, memory_arena *OutputArena)
{
    ui_style_registry *Result = 0;

    memory_arena *RoutineArena = {0};
    {
        memory_arena_params Params = {0};
        Params.AllocatedFromLine = __LINE__;
        Params.AllocatedFromFile = __FILE__;
        Params.ReserveSize       = StyleParser_MaximumFileSize + (StyleParser_MaximumTokenPerFile * sizeof(style_token));
        Params.CommitSize        = ArenaDefaultCommitSize;

        RoutineArena = AllocateArena(Params);
    }

    if(!RoutineArena)
    {
        return Result;
    }

    os_handle FileHandle = OSFindFile(FileName);
    if(!OSIsValidHandle(FileHandle))
    {
        return Result;
    }

    u64 FileSize = OSFileSize(FileHandle);
    if(FileSize > StyleParser_MaximumFileSize)
    {
        return Result;
    }

    os_read_file File = OSReadFile(FileHandle, RoutineArena);
    if(!File.FullyRead)
    {
        return Result;
    }

    style_file_debug_info Debug = {0};
    Debug.FileContent = File;
    Debug.FileName    = FileName;

    tokenized_style_file TokenizedFile = TokenizeStyleFile(File, RoutineArena, &Debug);
    if(TokenizedFile.CanBeParsed)
    {
        Result = ParseStyleFile(&TokenizedFile, RoutineArena, OutputArena, &Debug);
    }

    OSReleaseFile(FileHandle);

    return Result;
}

// [Helpers]

internal ui_color
ToNormalizedColor(vec4_unit Vec)
{
    f32      Inverse = 1.f / 255;
    ui_color Result  = UIColor(Vec.X.Float32 * Inverse, Vec.Y.Float32 * Inverse, Vec.Z.Float32 * Inverse, Vec.W.Float32 * Inverse);
    return Result;
}


internal b32
IsValidStyleTokenBuffer(style_token_buffer *Buffer)
{
    b32 Result = (Buffer->Tokens) && (Buffer->At < Buffer->Size);
    return Result;
}

internal b32
IsValidStyleFile(tokenized_style_file *File)
{
    b32 Result = (File->StylesCount > 0) && (IsValidStyleTokenBuffer(&File->Buffer));
    return Result;
}

// [Tokens]

internal b32
StyleTokenMatch(style_token *Token, StyleToken_Type Type)
{
    b32 Result = (Token->Type == Type);
    return Result;
}

internal style_token *
GetStyleToken(style_token_buffer *Buffer, i64 Index)
{
    style_token *Result = 0;

    if (Index < Buffer->Size)
    {
        Result = Buffer->Tokens + Index;
    }

    return Result;
}

internal style_token *
EmitStyleToken(style_token_buffer *Buffer, StyleToken_Type Type, u64 AtLine, u64 AtByte)
{
    style_token *Result = GetStyleToken(Buffer, Buffer->Count++);

    if(Result)
    {
        Result->ByteInFile = AtByte;
        Result->LineInFile = AtLine;
        Result->Type       = Type;
    }

    return Result;
}

internal style_token *
PeekStyleToken(style_token_buffer *Buffer, i32 Offset, style_file_debug_info *Debug)
{
    style_token *Result = GetStyleToken(Buffer, (Buffer->At - 1));

    i64 Index = Buffer->At + Offset;
    if (Index >= 0 && Index < Buffer->Count)
    {
        Result = GetStyleToken(Buffer, Index);

        Debug->CurrentLineInFile = Result->LineInFile;
    }

    return Result;
}

internal void
EatStyleToken(style_token_buffer *Buffer, u32 Count)
{
    if (Buffer->At + Count < Buffer->Count)
    {
        Buffer->At += Count;
    }

    if(Buffer->At > Buffer->Count)
    {
        Buffer->At = Buffer->Count;
    }
}

internal ui_unit
ReadUnit(os_read_file *File, style_file_debug_info *Debug)
{
    ui_unit Result = {0};

    if (IsDigit(PeekFile(File, 0)))
    {
        f64 Number = 0;
        while (IsValidFile(File))
        {
            u8 Char = PeekFile(File, 0);
            if (IsDigit(Char))
            {
                Number = (Number * 10) + (Char - '0');
                AdvanceFile(File, 1);
            }
            else
            {
                break;
            }
        }

        if(IsValidFile(File) && PeekFile(File, 0) == '.')
        {
            AdvanceFile(File, 1);

            f64 C = 1.0 / 10.0;
            while (IsValidFile(File))
            {
                u8 Char = PeekFile(File, 0);
                if (IsDigit(Char))
                {
                    Number = Number + (C * (f64)(Char - '0'));
                    C     *= 1.0f / 10.0f;

                    AdvanceFile(File, 1);
                }
                else
                {
                    break;
                }
            }
        }

        if(IsValidFile(File) && PeekFile(File, 0) == '%')
        {
            Result.Type = UIUnit_Percent;

            if (Number >= 0.f && Number <= 100.f)
            {
                Result.Percent = (f32)Number;
                AdvanceFile(File, 1);
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Percent value must be: 0% <= Value <= 100%"));
            }
        }
        else
        {
            Result.Type    = UIUnit_Float32;
            Result.Float32 = (f32)Number;
        }
    } else
    if (IsAlpha(PeekFile(File, 0)))
    {
        byte_string Identifier = ReadIdentifier(File);
        if(IsValidByteString(Identifier))
        {
            for(u32 Idx = 0; Idx < ArrayCount(StyleUnitKeywordTable); ++Idx)
            {
                style_parser_table_entry Entry = StyleUnitKeywordTable[Idx];
                if(ByteStringMatches(Identifier, Entry.Name, NoFlag))
                {
                    Result.Type = Entry.Value;
                    break;
                }
            }
        }

        if(Result.Type == UIUnit_None)
        {
            ReportStyleFileError(Debug, error_message("Found invalid identifier in file."));
        }
    } else
    {
        ReportStyleFileError(Debug, error_message("Could not parse identifier."));
    }

    return Result;
}

internal byte_string
ReadIdentifier(os_read_file *File)
{
    byte_string Result = ByteString(PeekFilePointer(File), 0);

    while (IsValidFile(File))
    {
        u8 Character = PeekFile(File, 0);
        if (IsAlpha(Character) || Character == '_' || Character == '-')
        {
            ++Result.Size;
            AdvanceFile(File, 1);
        }
        else
        {
            break;
        }
    }

    return Result;
}

internal style_vector
ReadVector(os_read_file *File, style_file_debug_info *Debug)
{
    style_vector Result = {0};

    while(Result.Size < 4)
    {
        SkipWhiteSpaces(File);

        ui_unit Unit = ReadUnit(File, Debug);
        if (Unit.Type != UIUnit_None)
        {
            Result.V.Values[Result.Size++] = Unit;

            SkipWhiteSpaces(File);

            u8 Character = PeekFile(File, 0);
            if (Character == ',')
            {
                AdvanceFile(File, 1);
                continue;
            }
            else if (Character == ']')
            {
                AdvanceFile(File, 1);
                break;
            }
            else
            {
                break;
            }
        }
    }

    return Result;
}

internal tokenized_style_file
TokenizeStyleFile(os_read_file File, memory_arena *Arena, style_file_debug_info *Debug)
{
    tokenized_style_file Result = {0};
    Result.Buffer.Tokens = PushArray(Arena, style_token, StyleParser_MaximumTokenPerFile);
    Result.Buffer.Size   = StyleParser_MaximumTokenPerFile;

    while (IsValidFile(&File))
    {
        SkipWhiteSpaces(&File);

        u8  Char   = PeekFile(&File, 0);
        u64 AtLine = Debug->CurrentLineInFile;
        u64 AtByte = File.At;

        if (IsAlpha(Char))
        {
            byte_string Identifier = ReadIdentifier(&File);
            if(IsValidByteString(Identifier))
            {
                style_token *Token = 0;
                for(u32 Idx = 0; Idx < ArrayCount(StyleKeywordTable); ++Idx)
                {
                    style_parser_table_entry Entry = StyleKeywordTable[Idx];
                    if(ByteStringMatches(Identifier, Entry.Name, StringMatch_NoFlag))
                    {
                        Token = EmitStyleToken(&Result.Buffer, Entry.Value, AtLine, AtByte);
                        break;
                    }
                }

                if(Token == 0)
                {
                    Token = EmitStyleToken(&Result.Buffer, StyleToken_Identifier, AtLine, AtByte);
                    if(Token)
                    {
                        Token->Identifier = Identifier;
                    }
                }
                else if(Token->Type == StyleToken_Style)
                {
                    ++Result.StylesCount;
                }
            }
            else
            {
                // TODO: What do we do here? Not sure what are the failure cases for a string?
                // And what do we even do, given the case?
            }

            continue;
        }

        if (IsDigit(Char))
        {
            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Unit, AtLine, AtByte);
            if (Token)
            {
                ui_unit Unit = ReadUnit(&File, Debug);
                if(Unit.Type != UIUnit_None)
                {
                    Token->Unit = Unit;
                }
            }

            continue;
        }

        if (IsNewLine(Char))
        {
            u8 Next = PeekFile(&File, 1);
            if (Char == '\r' && Next == '\n')
            {
                AdvanceFile(&File, 2);
            }
            else
            {
                AdvanceFile(&File, 1);
            }

            ++Debug->CurrentLineInFile;

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(&File, 1) == '=')
            {
                EmitStyleToken(&Result.Buffer, StyleToken_Assignment, AtLine, AtByte);
                AdvanceFile(&File, 2);
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Stray ':' did you mean ':=' ?"));
            }

            continue;
        }

        if (Char == '[')
        {
            AdvanceFile(&File, 1); // Consumes '['

            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Vector, AtLine, AtByte);
            if (Token)
            {
                 Token->Vector = ReadVector(&File, Debug);
            }

            continue;
        }

        if (Char == '"')
        {
            AdvanceFile(&File, 1); // Skip the first '"'

            byte_string String = ReadIdentifier(&File);
            if(IsValidByteString(String))
            {
                if(PeekFile(&File, 0) == '"')
                {
                    style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_String, AtLine, AtByte);
                    if (Token)
                    {
                        Token->Identifier = String;
                    }

                    AdvanceFile(&File, 1); // Skip the second '"'
                }
                else
                {
                    ReportStyleFileError(Debug, error_message("Found invalid character in string"));
                }
            }

            continue;
        }

        if (Char == '{' || Char == '}' || Char == ';' || Char == '.' || Char == ',' || Char == '%' || Char == '@')
        {
            AdvanceFile(&File, 1);
            EmitStyleToken(&Result.Buffer, (StyleToken_Type)Char, AtLine, AtByte);

            continue;
        }

        if(Char == '#')
        {
            AdvanceFile(&File, 1);

            while(!IsNewLine(PeekFile(&File, 0)))
            {
                AdvanceFile(&File, 1);
            }

            continue;
        }

        // BUG: Break? Seems wrong... Because 1 wrong token = invalid file??

        ReportStyleFileError(Debug, error_message("Found invalid character"));
        break;
    }

    style_token *EndOfFile = EmitStyleToken(&Result.Buffer, StyleToken_EndOfFile, 0, 0);

    Result.CanBeParsed = (EndOfFile && 1);

    return Result;
}

// [Parser]

internal style_effect
ParseStyleState(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    style_effect Effect = {StyleState_None};

    style_token *AtSymbol = PeekStyleToken(Buffer, 0, Debug);
    if(AtSymbol->Type == StyleToken_AtSymbol)
    {
        style_token *Name = PeekStyleToken(Buffer, 1, Debug);
        if(Name->Type == StyleToken_Identifier)
        {
            StyleState_Type EffectType = StyleState_None;
            for(u32 Idx = 0; Idx < ArrayCount(StyleStateTable); ++Idx)
            {
                style_parser_table_entry Entry = StyleStateTable[Idx];
                if(ByteStringMatches(Name->Identifier, Entry.Name, StringMatch_NoFlag))
                {
                    EffectType = Entry.Value;
                    break;
                }
            }

            if(EffectType != StyleState_None)
            {
                Effect.Type = EffectType;
                EatStyleToken(Buffer, 2);
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Found invalid effect. Must be: [base OR hover]"));
            }
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected an identifier after trying to set an effect with @"));
        }
    }

    return Effect;
}

internal style_attribute
ParseStyleAttribute(style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *Debug)
{
    style_attribute Attribute = {0};

    style_token *Name = PeekStyleToken(Buffer, 0, Debug);
    {
        if(Name->Type == StyleToken_Identifier)
        {
            StyleProperty_Type PropertyType = StyleProperty_None;
            for(u32 Idx = 0; Idx < ArrayCount(StylePropertyTable); ++Idx)
            {
                style_parser_table_entry Entry = StylePropertyTable[Idx];
                if(ByteStringMatches(Name->Identifier, Entry.Name, StringMatch_NoFlag))
                {
                    PropertyType = Entry.Value;
                    break;
                }
            }

            if(PropertyType != StyleProperty_None)
            {
                Attribute.LineInFile   = Name->LineInFile;
                Attribute.PropertyType = PropertyType;
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Found an invalid attribute. (Token 1)"));

                Attribute.HadError = 1;
            }

            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected an identifier. (Token 1)"));

            SynchronizeParser(Buffer, StyleToken_SemiColon, Debug);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *Assignment = PeekStyleToken(Buffer, 0, Debug);
    {
        if (Assignment->Type == StyleToken_Assignment)
        {
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected an assignment (:=). (Token 2)"));

            SynchronizeParser(Buffer, StyleToken_SemiColon, Debug);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *Value = PeekStyleToken(Buffer, 0, Debug);
    {
        if(Value->Type == StyleToken_Identifier)
        {
            style_var_hash   Hash  = HashStyleVar(Value->Identifier);
            style_var_entry *Entry = FindStyleVarEntry(Hash, VarTable);
            if(Entry && Entry->ValueIsParsed)
            {
                Value = Entry->ValueToken;
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Found unknown variable. (Token 3)"));
                Attribute.HadError = 1;
            }
        }

        if (Value->Type == StyleToken_Unit || Value->Type == StyleToken_String || Value->Type == StyleToken_Vector)
        {
            Attribute.ParsedAs = Value->Type;
            Attribute.Vector   = Value->Vector; // HACK: Copies the largest union value. Unsure if UB..

            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected a value. (Token 3)"));

            SynchronizeParser(Buffer, StyleToken_SemiColon, Debug);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *End = PeekStyleToken(Buffer, 0, Debug);
    {
        if (End->Type == StyleToken_SemiColon)
        {
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected ';' to terminate the expression. (Token 4)"));

            SynchronizeParser(Buffer, StyleToken_SemiColon, Debug);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    Attribute.IsSet = (!Attribute.HadError);

    return Attribute;
}

internal style_block
ParseStyleBlock(style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *Debug)
{
    style_block Result = {0};

    style_token *Next = PeekStyleToken(Buffer, 0, Debug);
    if(Next->Type == StyleToken_OpenBrace)
    {
        EatStyleToken(Buffer, 1);

        StyleState_Type CurrentEffect = StyleState_None;

        while(1)
        {
            Next = PeekStyleToken(Buffer, 0, Debug);

            if(StyleTokenMatch(Next, StyleToken_EndOfFile))
            {
                break;
            }

            if(StyleTokenMatch(Next, StyleToken_CloseBrace))
            {
                EatStyleToken(Buffer, 1);
                break;
            }

            // Try to parse an @Effect if we find one, set it, otherwise
            // we found an attribute. At least, that's what we expect.

            style_effect Effect = ParseStyleState(Buffer, Debug);
            if(Effect.Type != StyleState_None)
            {
                CurrentEffect = Effect.Type;
            } else 
            if(CurrentEffect != StyleState_None)
            {
                style_attribute Attribute = ParseStyleAttribute(Buffer, VarTable, Debug);
                if(!Attribute.HadError)
                {
                    if(!Result.Attributes[CurrentEffect][Attribute.PropertyType].IsSet)
                    {
                        Result.Attributes[CurrentEffect][Attribute.PropertyType] = Attribute;
                        Result.AttributesCount++;
                    }
                    else
                    {
                        ReportStyleFileError(Debug, warn_message("Attribute has already been set for this effect."));
                    }
                }
            }
            else
            {
                ReportStyleFileError(Debug, warn_message("You must specify an effect such as @Base before declaring your attributes"));
            }
        }
    }
    else
    {
        ReportStyleFileError(Debug, error_message("Expected '{' after header"));
    }

    return Result;
}

internal style_header
ParseStyleHeader(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    style_header Header = {0};

    style_token *Style = PeekStyleToken(Buffer, 0, Debug);
    {
        if (Style->Type == StyleToken_Style)
        {
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected 'Style' as the first token."));

            Header.HadError = 1;
            return Header;
        }
    }

    style_token *Name = PeekStyleToken(Buffer, 0, Debug);
    {
        if (Name->Type == StyleToken_String)
        {
            Header.StyleName = Name->Identifier;
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected 'Name' after 'style'. Must be a string."));

            Header.HadError = 1;
            return Header;
        }
    }

    return Header;
}

internal style_variable
ParseStyleVariable(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    style_variable Variable = {0};

    style_token *Var = PeekStyleToken(Buffer, 0, Debug);
    {
        if(Var->Type == StyleToken_Var)
        {
            EatStyleToken(Buffer, 1);
            Variable.IsValid = 1;
        }
        else
        {
            return Variable;
        }
    }

    style_token *Name = PeekStyleToken(Buffer, 0, Debug);
    {
        if(Name->Type == StyleToken_Identifier)
        {
            Variable.Name = Name->Identifier;
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Expected a name for variable. Name must be [a..z][A..Z][_]"));
            Variable.IsValid = 0;
        }
    }

    style_token *Assignment = PeekStyleToken(Buffer, 0, Debug);
    {
        if(Assignment->Type == StyleToken_Assignment)
        {
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Exptected an assignment (:=) after setting the variable's name"));
            Variable.IsValid = 0;
        }
    }

    style_token *Value = PeekStyleToken(Buffer, 0, Debug);
    {
        if (Value->Type == StyleToken_Unit || Value->Type == StyleToken_String || Value->Type == StyleToken_Vector)
        {
            Variable.ValueToken = Value;
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Exptected a value after the assignment."));
            Variable.IsValid = 0;
        }
    }

    style_token *End = PeekStyleToken(Buffer, 0, Debug);
    {
        if(End->Type == StyleToken_SemiColon)
        {
            EatStyleToken(Buffer, 1);
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Exptected ';' after creating a variable."));
            Variable.IsValid = 0;
        }
    }

    return Variable;
}

internal ui_style_registry *
ParseStyleFile(tokenized_style_file *File, memory_arena *RoutineArena, memory_arena *OutputArena, style_file_debug_info *Debug)
{
    ui_style_registry *Result = PushStruct(OutputArena, ui_style_registry);

    if(Result)
    {
        Result->StylesCount    = 0;
        Result->StylesCapacity = File->StylesCount;
        Result->Styles         = PushArray(OutputArena, ui_cached_style, File->StylesCount);

        style_var_table *VarTable = 0;
        {
            style_var_table_params Params = {0};
            Params.EntryCount = StyleParser_MaximumVarPerFile;
            Params.HashCount  = StyleParser_VarHashEntryPerFile;

            size_t Footprint      = GetStyleVarTableFootprint(Params);
            u8    *VarTableMemory = PushArray(RoutineArena, u8, Footprint);

            VarTable = PlaceStyleVarTableInMemory(Params, VarTableMemory);
        }

        if(VarTable)
        {
            style_token *Next = PeekStyleToken(&File->Buffer, 0, Debug);
            while (Next->Type != StyleToken_EndOfFile)
            {
                style_variable Variable = ParseStyleVariable(&File->Buffer, Debug);
                while (Variable.IsValid) 
                {
                    style_var_hash   Hash  = HashStyleVar(Variable.Name);
                    style_var_entry *Entry = FindStyleVarEntry(Hash, VarTable);
                    if(!Entry->ValueIsParsed)
                    {
                        Entry->ValueToken    = Variable.ValueToken;
                        Entry->ValueIsParsed = 1;
                    }
                    else
                    {
                        ReportStyleFileError(Debug, error_message("Two variables cannot have the same name"));
                    }

                    Variable = ParseStyleVariable(&File->Buffer, Debug);
                };

                style Style = {0};
                Style.Header = ParseStyleHeader(&File->Buffer, Debug);
                Style.Block  = ParseStyleBlock(&File->Buffer, VarTable, Debug);

                CacheStyle(&Style, Result, Debug);

                Next = PeekStyleToken(&File->Buffer, 0, Debug);
            }
        }
        else
        {
            ReportStyleParserError(Debug, error_message("Could not allocate style_var_table."));
        }
    }
    else
    {
        ReportStyleParserError(Debug, error_message("Could not allocate registry."));
    }

    return Result;
}

// [Variables]

internal style_var_table *
PlaceStyleVarTableInMemory(style_var_table_params Params, void *Memory)
{
    Assert(Params.EntryCount > 0);
    Assert(Params.HashCount > 0);
    Assert(IsPowerOfTwo(Params.HashCount));

    style_var_table *Result = 0;

    if (Memory)
    {
        Result = (style_var_table *)Memory;
        Result->HashTable = (u32 *)(Result + 1);
        Result->Entries   = (style_var_entry *)(Result->HashTable + Params.HashCount);

        Result->HashMask   = Params.HashCount - 1;
        Result->HashCount  = Params.HashCount;
        Result->EntryCount = Params.EntryCount;

        MemorySet(Result->HashTable, 0, Result->HashCount * sizeof(Result->HashTable[0]));

        for (u32 Idx = 0; Idx < Params.EntryCount; Idx++)
        {
            style_var_entry *Entry = GetStyleVarEntry(Idx, Result);
            if ((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHash = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHash = 0;
            }

            Entry->ValueIsParsed = 0;
        }
    }

    return Result;
}

internal style_var_entry *
GetStyleVarSentinel(style_var_table *Table)
{
    style_var_entry *Result = Table->Entries;
    return Result;
}

internal style_var_entry *
GetStyleVarEntry(u32 Index, style_var_table *Table)
{
    Assert(Index < Table->EntryCount);

    style_var_entry *Result = Table->Entries + Index;
    return Result;
}

internal u32
PopFreeStyleVarEntry(style_var_table *Table)
{
    style_var_entry *Sentinel = GetStyleVarSentinel(Table);

    if (!Sentinel->NextWithSameHash)
    {
        return 0;
    }

    u32              Result = Sentinel->NextWithSameHash;
    style_var_entry *Entry  = GetStyleVarEntry(Result, Table);

    Sentinel->NextWithSameHash = Entry->NextWithSameHash;
    Entry->NextWithSameHash    = 0;

    return Result;
}

internal style_var_entry *
FindStyleVarEntry(style_var_hash Hash, style_var_table *Table)
{
    u32 HashSlot = Hash.Value & Table->HashMask;
    u32 EntryIndex = Table->HashTable[HashSlot];

    style_var_entry *Result = 0;
    while (EntryIndex)
    {
        style_var_entry *Entry = GetStyleVarEntry(EntryIndex, Table);
        if (Hash.Value == Entry->Hash.Value)
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHash;
    }

    if (!Result)
    {
        EntryIndex = PopFreeStyleVarEntry(Table);
        if (EntryIndex)
        {
            Result = GetStyleVarEntry(EntryIndex, Table);
            Result->NextWithSameHash = Table->HashTable[HashSlot];
            Result->Hash             = Hash;

            Table->HashTable[HashSlot] = EntryIndex;
        }
    }

    return Result;
}

internal style_var_hash
HashStyleVar(byte_string Name)
{
    style_var_hash Result = { HashByteString(Name) };
    return Result;
}

internal size_t
GetStyleVarTableFootprint(style_var_table_params Params)
{
    size_t HashSize  = Params.HashCount * sizeof(u32);
    size_t EntrySize = Params.EntryCount * sizeof(style_var_entry);
    size_t Result    = sizeof(style_var_table) + HashSize + EntrySize;

    return Result;
}

// [CACHING]

internal void
ValidateAttributeFormatting(style_attribute *Attribute, style_file_debug_info *Debug)
{
    Assert(!Attribute->HadError && Attribute->IsSet);

    byte_string ErrorMessage = ByteString(0, 0);

    switch(Attribute->PropertyType)
    {

    case StyleProperty_Size:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->Vector.Size != 2)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y]");
            break;
        }
    } break;

    case StyleProperty_Padding:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->Vector.Size != 4)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y, Z, W]");
            break;
        }

        vec4_unit Vector = Attribute->Vector.V;

        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
           Vector.Z.Type != UIUnit_Float32 || Vector.W.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float, Float, Float]");
            break;
        }
    } break;

    case StyleProperty_Spacing:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->Vector.Size != 2)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y]");
            break;
        }

        vec4_unit Vector = Attribute->Vector.V;

        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float]");
            break;
        }

    } break;

    case StyleProperty_TextColor:
    case StyleProperty_Color:
    case StyleProperty_BorderColor:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->Vector.Size != 4)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y, Z, W]");
            break;
        }

        vec4_unit Vector = Attribute->Vector.V;

        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
           Vector.Z.Type != UIUnit_Float32 || Vector.W.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float, Float, Float]");
            break;
        }

        for(u32 Idx = 0; Idx < 4; ++Idx)
        {
            b32 Valid = 1;
            if(!IsInRangeF32(0.f, 255.f, Vector.Values[Idx].Float32))
            {
                Valid = 0;
            }

            if(!Valid)
            {
                ErrorMessage = byte_string_literal("Invalid Format. All values in the vector must be >= 0.0 AND <= 255.0");
            }
        }

    } break;

    case StyleProperty_BorderWidth:
    case StyleProperty_FontSize:
    case StyleProperty_Softness:
    {
        if(Attribute->ParsedAs != StyleToken_Unit || Attribute->Unit.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Value must be a single scalar.");
            break;
        }

        if(Attribute->Unit.Float32 <= 0)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Value must be > 0.0");
        }
    } break;

    case StyleProperty_CornerRadius:
    {
        vec4_unit Vector = Attribute->Vector.V;

        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
           Vector.Z.Type != UIUnit_Float32 || Vector.W.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float, Float, Float]");
        }
    } break;

    case StyleProperty_Font:
    {
        if(Attribute->ParsedAs != StyleToken_String)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Value must be a \"string\"");
        }
    } break;

    case StyleProperty_Display:
    case StyleProperty_FlexDirection:
    case StyleProperty_JustifyContent:
    case StyleProperty_AlignItems:
    case StyleProperty_SelfAlign:
    {
        if(Attribute->ParsedAs != StyleToken_String)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Value must be a string");
        }
    } break;

    default:
    {
        Assert(!"Should not happen");
    } break;

    }

    if(IsValidByteString(ErrorMessage))
    {
        ReportStyleFileError(Debug, error_message(ErrorMessage.String));

        Attribute->IsSet = 0;
    }
}

internal style_property
ConvertAttributeToProperty(style_attribute Attribute)
{
    style_property Property = {0};

    Property.Type  = Attribute.PropertyType;
    Property.IsSet = (!Attribute.HadError) && (Attribute.PropertyType != StyleProperty_None);

    if (!Property.IsSet) 
    {
        return Property;
    }

    switch (Attribute.PropertyType)
    {
        case StyleProperty_Size:
        {
            Property.Vec2.X = Attribute.Vector.V.X;
            Property.Vec2.Y = Attribute.Vector.V.Y;
        } break;

        case StyleProperty_Color:
        case StyleProperty_BorderColor:
        case StyleProperty_TextColor:
        {
            Property.Color = ToNormalizedColor(Attribute.Vector.V);
        } break;

        case StyleProperty_Padding:
        {
            Property.Padding.Left  = Attribute.Vector.V.X.Float32;
            Property.Padding.Top   = Attribute.Vector.V.Y.Float32;
            Property.Padding.Right = Attribute.Vector.V.Z.Float32;
            Property.Padding.Bot   = Attribute.Vector.V.W.Float32;
        } break;

        case StyleProperty_Spacing:
        {
            Property.Spacing.Horizontal = Attribute.Vector.V.X.Float32;
            Property.Spacing.Vertical   = Attribute.Vector.V.Y.Float32;
        } break;

        case StyleProperty_Softness:
        case StyleProperty_FontSize:
        case StyleProperty_BorderWidth:
        {
            Property.Float32 = Attribute.Unit.Float32;
        } break;

        case StyleProperty_CornerRadius:
        {
            Property.CornerRadius.TopLeft  = Attribute.Vector.V.X.Float32;
            Property.CornerRadius.TopRight = Attribute.Vector.V.Y.Float32;
            Property.CornerRadius.BotRight = Attribute.Vector.V.Z.Float32;
            Property.CornerRadius.BotLeft  = Attribute.Vector.V.W.Float32;
        } break;

        case StyleProperty_Display:
        {
            for(u32 Idx = 0; Idx < ArrayCount(StyleDisplayTable); ++Idx)
            {
                style_parser_table_entry Entry = StyleDisplayTable[Idx];
                if(ByteStringMatches(Entry.Name, Attribute.String, NoFlag))
                {
                    Property.Enum = Entry.Value;
                    break;
                }
            }
        } break;

        case StyleProperty_FlexDirection:
        {
            for(u32 Idx = 0; Idx < ArrayCount(FlexDirectionKeywordTable); ++Idx)
            {
                style_parser_table_entry Entry = FlexDirectionKeywordTable[Idx];
                if(ByteStringMatches(Entry.Name, Attribute.String, NoFlag))
                {
                    Property.Enum = Entry.Value;
                    break;
                }
            }
        } break;

        case StyleProperty_JustifyContent:
        {
            for(u32 Idx = 0; Idx < ArrayCount(JustifyContentKeywordTable); ++Idx)
            {
                style_parser_table_entry Entry = JustifyContentKeywordTable[Idx];
                if(ByteStringMatches(Entry.Name, Attribute.String, NoFlag))
                {
                    Property.Enum = Entry.Value;
                    break;
                }
            }
        } break;

        case StyleProperty_AlignItems:
        {
            for(u32 Idx = 0; Idx < ArrayCount(AlignItemKeywordTable); ++Idx)
            {
                style_parser_table_entry Entry = AlignItemKeywordTable[Idx];
                if(ByteStringMatches(Entry.Name, Attribute.String, NoFlag))
                {
                    Property.Enum = Entry.Value;
                    break;
                }
            }
        } break;

        case StyleProperty_SelfAlign:
        {
            for(u32 Idx = 0; Idx < ArrayCount(SelfAlignItemKeywordTable); ++Idx)
            {
                style_parser_table_entry Entry = SelfAlignItemKeywordTable[Idx];
                if(ByteStringMatches(Entry.Name, Attribute.String, NoFlag))
                {
                    Property.Enum = Entry.Value;
                    break;
                }
            }
        } break;

        default:
        {
        } break;
    }

    return Property;
}

internal b32
IsPropertySet(ui_cached_style *Style, StyleState_Type State, StyleProperty_Type Property)
{
    b32 Result = Style->Properties[State][Property].IsSet;
    return Result;
}

internal void
CacheStyle(style *ParsedStyle, ui_style_registry *Registry, style_file_debug_info *Debug)
{
    Assert(Registry->StylesCount < Registry->StylesCapacity);

    ui_cached_style *CachedStyle = Registry->Styles + Registry->StylesCount;
    style_header    *Header      = &ParsedStyle->Header;
    style_block     *Block       = &ParsedStyle->Block;

    CachedStyle->CachedIndex = ++Registry->StylesCount; // 0 is a reserved spot

    // Prunes every badly formatted attribute from the list.
    // Also, outputs the corresponding error messages.
    // Then if the value is still set, we cache it.

    ForEachEnum(StyleState_Type, StyleState_Count, State)
    {
        ForEachEnum(StyleProperty_Type, StyleProperty_Count, Property)
        {
            style_attribute Attribute = Block->Attributes[State][Property];
            if(Attribute.IsSet)
            {
                ValidateAttributeFormatting(&Attribute, Debug);
                if(Attribute.IsSet)
                {
                    CachedStyle->Properties[State][Property] = ConvertAttributeToProperty(Attribute);
                }
            }
            else
            {
                // NOTE: ?? Isn't it cleared to 0?

                style_property EmptyProperty = {0};
                EmptyProperty.Type = StyleProperty_None;

                CachedStyle->Properties[State][Property] = EmptyProperty;
            }
        }

        // Load Fonts

        // TODO: Need to check if the string is a valid string when trying to
        // prune badly formatted attributes. Same for size, it should already be
        // correctly formed.

        if(IsPropertySet(CachedStyle, State, StyleProperty_Font))
        {
            byte_string Name = Block->Attributes[State][StyleProperty_Font].String;

            if(IsPropertySet(CachedStyle, State, StyleProperty_FontSize))
            {
                style_property *Props = GetCachedProperties(CachedStyle->CachedIndex, StyleState_Basic, Registry);
                f32 Size = UIGetFontSize(Props);

                ui_font *Font = UIQueryFont(Name, Size);
                if(!Font)
                {
                    Font = UILoadFont(Name, Size);
                }

                CachedStyle->Properties[State][StyleProperty_Font].Pointer = Font;
            }
        }
    }

    Useless(Header); // NOTE: Useless at the moment. Unsure where I wanna go with this.
}

// [Error Handling]

internal void
SynchronizeParser(style_token_buffer *Buffer, StyleToken_Type StopAt, style_file_debug_info *Debug)
{
    style_token *Next = PeekStyleToken(Buffer, 0, Debug);
    while(1)
    {
        if(StyleTokenMatch(Next, StopAt) || StyleTokenMatch(Next, StyleToken_EndOfFile))
        {
            break;
        }

        EatStyleToken(Buffer, 1);
        Next = PeekStyleToken(Buffer, 0, Debug);
    }

    EatStyleToken(Buffer, 1);
}

internal void
ReportStyleFileError(style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    u8          Buffer[Kilobyte(4)] = {0};
    byte_string Error               = ByteString(Buffer, 0);

    Error.Size += snprintf((char *)Error.String             , sizeof(Buffer), "[File %s | Line: %u]", Debug->FileName.String, Debug->CurrentLineInFile);
    Error.Size += snprintf((char *)Error.String + Error.Size, sizeof(Buffer), "[%s]", Message.String);

    ReportStyleParserError(Debug, Severity, Error, Console);
}

internal void
ReportStyleParserError(style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    if(Severity == ConsoleMessage_Error)
    {
        Debug->ErrorCount++;
    }
    else if(Severity == ConsoleMessage_Warn)
    {
        Debug->WarningCount++;
    }

    ConsoleWriteMessage(Severity, Message, Console);
}
