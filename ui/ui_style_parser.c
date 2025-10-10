// [API]

internal ui_style_subregistry *
CreateStyleSubregistry(byte_string FileName, memory_arena *Arena)
{
    ui_style_subregistry *Result = 0;

    // NOTE: This seems weird?? How do relative paths even work?
    memory_region Region          = EnterMemoryRegion(Arena);
    byte_string   CorrectFileName = OSAppendToLaunchDirectory(FileName, Region.Arena);
    LeaveMemoryRegion(Region);

    os_handle FileHandle = OSFindFile(CorrectFileName);
    if (OSIsValidHandle(FileHandle))
    {
        u64 FileSize = OSFileSize(FileHandle);
        if (FileSize <= StyleParser_MaximumFileSize)
        {
            os_read_file File = OSReadFile(FileHandle, Region.Arena);
            if(File.FullyRead)
            {
                style_file StyleFile = TokenizeStyleFile(FileName, File, Region.Arena);
                if (IsValidStyleFile(&StyleFile))
                {
                    Result = ParseStyleFile(&StyleFile, Region.Arena);
                }
            }
            else
            {
                // TODO: Report Error
            }
        }
        else
        {
            // TODO: Report Error
        }

        OSReleaseFile(FileHandle);
    }
    else
    {
        // TODO: Report Error
    }

    return Result;
}

internal ui_style_registry *
CreateStyleRegistry(byte_string *FileNames, u32 Count, memory_arena *OutputArena)
{
    Assert(FileNames && Count > 0);

    ui_style_registry *Result = PushArray(OutputArena, ui_style_registry, 1);

    for (u32 FileIdx = 0; FileIdx < Count; FileIdx++)
    {
        // TODO: Check if this can return NULL.

        ui_style_subregistry *Sub = CreateStyleSubregistry(FileNames[FileIdx], OutputArena);
        if (Sub)
        {
            AppendToLinkedList(Result, Sub, Result->Count);
        }
    }

    // OSListenToRegistry(Result);

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
    b32 Result = (Buffer->Tokens) && (Buffer->At) && (Buffer->At < Buffer->Size);
    return Result;
}

internal b32
IsValidStyleFile(style_file *File)
{
    b32 Result = (File->StyleCount > 0) && (IsValidByteString(File->Name)) && (IsValidStyleTokenBuffer(&File->Buffer));
    return Result;
}

// [Tokenizer]

internal style_token *
GetStyleToken(style_token_buffer *Buffer, u64 Index)
{
    style_token *Result = 0;

    if (Index < Buffer->Size)
    {
        Result = Buffer->Tokens + Index;
    }

    return Result;
}

internal style_token *
EmitStyleToken(style_token_buffer *Buffer, StyleToken_Type Type, u32 AtLine, u8 *InFile)
{
    style_token *Result = GetStyleToken(Buffer, Buffer->At);

    if(Result)
    {
        Result->FilePointer = InFile;
        Result->LineInFile  = AtLine;
        Result->Type        = Type;

        ++Buffer->At;
    }

    return Result;
}

internal style_token *
PeekStyleToken(style_token_buffer *Buffer, i32 Offset)
{
    style_token *Result = GetStyleToken(Buffer, (Buffer->At - 1));

    if (Buffer->At + Offset < Buffer->Size)
    {
        Result = GetStyleToken(Buffer, Buffer->At + Offset);
    }

    return Result;
}

internal void
ConsumeStyleTokens(style_token_buffer *Buffer, u32 Count)
{
    if (Buffer->At + Count < Buffer->Size)
    {
        Buffer->At += Count;
    }
}

internal ui_unit
ReadUnit(os_read_file *File, byte_string FileName, u32 LineInFile)
{
    ui_unit Result = {0};
    f64     Number = 0;

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

    if (IsValidFile(File))
    {
        if (PeekFile(File, 0) == StyleToken_Period)
        {
            AdvanceFile(File, 1); // Consumes '.'

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

            if (IsValidFile(File))
            {
                if (PeekFile(File, 0) == StyleToken_Percent)
                {
                    Result.Type = UIUnit_Percent;

                    if (Number >= 0.f && Number <= 100.f)
                    {
                        Result.Percent = (f32)Number;
                        AdvanceFile(File, 1);
                    }
                    else
                    {
                        byte_string Message = byte_string_literal("Percent value must be: 0% <= Value <= 100%");
                        ReportStyleFileError(FileName, LineInFile, ConsoleMessage_Error, Message);
                    }
                }
                else
                {
                    Result.Type    = UIUnit_Float32;
                    Result.Float32 = (f32)Number;
                }
            }
        }
        else if (PeekFile(File, 0) == StyleToken_Percent)
        {
            Result.Type = UIUnit_Percent;

            if (Number >= 0.f && Number <= 100.f)
            {
                Result.Percent = (f32)Number;
                AdvanceFile(File, 1);
            }
            else
            {
                byte_string Message = byte_string_literal("Percent value must be: 0% <= Value <= 100%");
                ReportStyleFileError(FileName, LineInFile, ConsoleMessage_Error, Message);
            }
        }
        else
        {
            Result.Type    = UIUnit_Float32;
            Result.Float32 = (f32)Number;
        }
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
        if (IsAlpha(Character) || Character == '_')
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

internal vec4_unit
ReadVector(os_read_file *File, byte_string FileName, u32 LineInFile)
{
    vec4_unit Result = (vec4_unit){0};

    if(PeekFile(File, 0) == '[')
    {
        AdvanceFile(File, 1);

        u32 VectorSize = 0;
        while(VectorSize < 4)
        {
            SkipWhiteSpaces(File);

            u8 Character = PeekFile(File, 0);
            if(IsDigit(Character))
            {
                ui_unit Unit = ReadUnit(File, FileName, LineInFile);
                if(Unit.Type != UIUnit_None)
                {
                    Result.Values[VectorSize++] = Unit;

                    SkipWhiteSpaces(File);

                    Character = PeekFile(File, 0);
                    if(Character == ',')
                    {
                        AdvanceFile(File, 1);
                        continue;
                    }
                    else if(Character == ']')
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
            else
            {
                byte_string Message = byte_string_literal("A vector must only contain numerical values.");
                ReportStyleFileError(FileName, LineInFile, ConsoleMessage_Error, Message);
                break;
            }
        }
    }
    else
    {
        byte_string Message = byte_string_literal("Expected '[' when calling ReadVector");
        ReportStyleFileError(FileName, LineInFile, ConsoleMessage_Error, Message);
    }

    return Result;
}

internal style_file
TokenizeStyleFile(byte_string FileName, os_read_file File, memory_arena *Arena)
{
    style_file Result = {0};
    Result.Buffer.Tokens = PushArray(Arena, style_token, StyleParser_MaximumTokenPerFile);
    Result.Buffer.Size   = StyleParser_MaximumTokenPerFile;

    u64 AtLine = 0;

    while (IsValidFile(&File))
    {
        SkipWhiteSpaces(&File);

        u8  Char   = PeekFile(&File, 0);
        u8 *InFile = PeekFilePointer(&File);

        if (IsAlpha(Char))
        {
            byte_string Identifier = ReadIdentifier(&File);
            if(IsValidByteString(Identifier))
            {
                style_token *Token = 0;
                for(u32 Idx = 0; Idx < ArrayCount(StyleKeywordTable); ++Idx)
                {
                    style_keyword_table_entry Entry = StyleKeywordTable[Idx];
                    if(ByteStringMatches(Identifier, Entry.Name, StringMatch_NoFlag))
                    {
                        Token = EmitStyleToken(&Result.Buffer, Entry.TokenType, AtLine, InFile);
                        break;
                    }
                }

                if(Token == 0)
                {
                    Token = EmitStyleToken(&Result.Buffer, StyleToken_Identifier, AtLine, InFile);
                    if(Token)
                    {
                        Token->Identifier = Identifier;
                    }
                }
                else if(Token->Type == StyleToken_Style)
                {
                    ++Result.StyleCount;
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
            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Unit, AtLine, InFile);
            if (Token)
            {
                ui_unit Unit = ReadUnit(&File, FileName, AtLine);
                if(Unit.Type != UIUnit_None)
                {
                    Token->Unit = Unit;
                }
            }

            continue;
        }

        if (Char == '\r' || Char == '\n')
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

            AtLine += 1;

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(&File, 1) == '=')
            {
                EmitStyleToken(&Result.Buffer, StyleToken_Assignment, AtLine, InFile);
                AdvanceFile(&File, 2);
            }
            else
            {
                byte_string Message = byte_string_literal("Stray ':' did you mean ':=' ?");
                ReportStyleFileError(FileName, AtLine, ConsoleMessage_Error, Message);
            }

            continue;
        }

        if (Char == '[')
        {
            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Vector, AtLine, InFile);
            if (Token)
            {
                Token->Vector = ReadVector(&File, FileName, AtLine);
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

                    style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_String, AtLine, InFile);
                    if (Token)
                    {
                        Token->Identifier = String;
                    }

                    AdvanceFile(&File, 1); // Skip the second '"'
                }
                else
                {
                    byte_string Message = byte_string_literal("Found invalid character in string");
                    ReportStyleFileError(FileName, AtLine, ConsoleMessage_Error, Message);
                }
            }

            continue;
        }

        if (Char == '{' || Char == '}' || Char == ';' || Char == '.' || Char == ',' || Char == '%' || Char == '@')
        {
            AdvanceFile(&File, 1);
            EmitStyleToken(&Result.Buffer, (StyleToken_Type)Char, AtLine, InFile);

            continue;
        }

        // NOTE: Break? Seems wrong... Because 1 wrong token = invalid file??

        ReportStyleFileError(FileName, AtLine, ConsoleMessage_Error, byte_string_literal("Found invalid character"));
        break;
    }

    EmitStyleToken(&Result.Buffer, StyleToken_EndOfFile, AtLine, 0);

    return Result;
}

// [Parser]

internal style_effect
ParseStyleEffect(byte_string FileName, style_token_buffer *Buffer)
{
    style_effect Effect = {0};

    style_token *AtSymbol = PeekStyleToken(Buffer, 0);
    if(AtSymbol->Type == StyleToken_AtSymbol)
    {
        style_token *Name = PeekStyleToken(Buffer, 1);
        if(Name->Type == StyleToken_Identifier)
        {
            StyleEffect_Type EffectType = StyleEffect_None;
            for(u32 Idx = 0; Idx < ArrayCount(StyleEffectTable); ++Idx)
            {
                style_effect_table_entry Entry = StyleEffectTable[Idx];
                if(ByteStringMatches(Name->Identifier, Entry.Name, StringMatch_NoFlag))
                {
                    EffectType = Entry.EffectType;
                    break;
                }
            }

            if(EffectType != StyleEffect_None)
            {
                Effect.Type = EffectType;
                ConsumeStyleTokens(Buffer, 2);
            }
            else
            {
                byte_string Message = byte_string_literal("Found invalid effect. Must be: [base OR hover OR click]");
                ReportStyleFileError(FileName, AtSymbol->LineInFile, ConsoleMessage_Error, Message);
            }
        }
        else
        {
            byte_string Message = byte_string_literal("Expected an identifier after trying to set an effect with @");
            ReportStyleFileError(FileName, AtSymbol->LineInFile, ConsoleMessage_Error, Message);
        }
    }

    return Effect;
}

internal style_attribute
ParseStyleAttribute(byte_string FileName, style_token_buffer *Buffer, style_var_table *VarTable)
{
    style_attribute Attribute = {0};

    style_token *Name = PeekStyleToken(Buffer, 0);
    {
        if(Name->Type == StyleToken_Identifier)
        {
            StyleProperty_Type PropertyType = StyleProperty_None;
            for(u32 Idx = 0; Idx < ArrayCount(StylePropertyTable); ++Idx)
            {
                style_property_table_entry Entry = StylePropertyTable[Idx];
                if(ByteStringMatches(Name->Identifier, Entry.Name, StringMatch_NoFlag))
                {
                    PropertyType = Entry.PropertyType;
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
                byte_string Message = byte_string_literal("Found an invalid attribute. (Token 1)");
                ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

                Attribute.HadError = 1;
            }

            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Expected an identifier. (Token 1)");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

            SynchronizeParser(Buffer, StyleToken_SemiColon);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *Assignment = PeekStyleToken(Buffer, 0);
    {
        if (Assignment->Type == StyleToken_Assignment)
        {
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Expected an assignment (:=). (Token 2)");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

            SynchronizeParser(Buffer, StyleToken_SemiColon);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *Value = PeekStyleToken(Buffer, 0);
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
                byte_string Message = byte_string_literal("Found unknown variable. (Token 3)");
                ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

                Attribute.HadError = 1;
            }
        }

        if (Value->Type == StyleToken_Unit || Value->Type == StyleToken_String || Value->Type == StyleToken_Vector)
        {
            Attribute.ParsedAs = Value->Type;
            Attribute.Unit     = Value->Unit; // NOTE: Is this fine? The unions are the same.

            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Expected a value. (Token 3)");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

            SynchronizeParser(Buffer, StyleToken_SemiColon);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    style_token *End = PeekStyleToken(Buffer, 0);
    {
        if (End->Type == StyleToken_SemiColon)
        {
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Expected ';' to terminate the expression. (Token 4)");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

            SynchronizeParser(Buffer, StyleToken_SemiColon);
            Attribute.HadError = 1;
            return Attribute;
        }
    }

    return Attribute;
}

internal style_block
ParseStyleBlock(byte_string FileName, style_token_buffer *Buffer, style_var_table *VarTable)
{
    style_block Result = {0};

    style_token *Next = PeekStyleToken(Buffer, 0);
    if(Next->Type == StyleToken_OpenBrace)
    {
        ConsumeStyleTokens(Buffer, 1);

        style_effect CurrentEffect = {StyleEffect_None};
        while(Next->Type != StyleToken_CloseBrace && Next->Type != StyleToken_EndOfFile)
        {
            style_effect Effect = ParseStyleEffect(FileName, Buffer);
            while(Effect.Type != StyleEffect_None)
            {
                if(Effect.Type != CurrentEffect.Type)
                {
                    CurrentEffect = Effect;
                }

                Effect = ParseStyleEffect(FileName, Buffer);
            }

            if(Effect.Type != StyleEffect_None)
            {
                Next = PeekStyleToken(Buffer, 0);
                while(Next->Type != StyleToken_CloseBrace)
                {
                    style_attribute Attribute = ParseStyleAttribute(FileName, Buffer, VarTable);
                    if(!Attribute.HadError)
                    {
                        if(!Result.Attributes[CurrentEffect.Type][Attribute.PropertyType].IsSet)
                        {
                            Result.Attributes[CurrentEffect.Type][Attribute.PropertyType] = Attribute;
                            Result.AttributeCount++;
                        }
                        else
                        {
                            byte_string Message = byte_string_literal("Attribute has already been set for this effect.");
                            ReportStyleFileError(FileName, Attribute.LineInFile, ConsoleMessage_Warn, Message);
                        }
                    }

                    Next = PeekStyleToken(Buffer, 0);
                }
            }
        }

        if(Next->Type == StyleToken_EndOfFile)
        {
            ReportStyleFileError(FileName, Next->LineInFile, ConsoleMessage_Error, byte_string_literal("Unexpected EOF"));
        }
        else
        {
            ConsumeStyleTokens(Buffer, 1);
        }
    }
    else
    {
        ReportStyleFileError(FileName, Next->LineInFile, ConsoleMessage_Error, byte_string_literal("Expected '{' after header"));
    }

    return Result;
}

internal style_header
ParseStyleHeader(byte_string FileName, style_token_buffer *Buffer)
{
    style_header Header = {0};

    style_token *Style = PeekStyleToken(Buffer, 0);
    {
        if (Style->Type == StyleToken_Style)
        {
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Unknown token when declaring a header. Expected 'Style'.");
            ReportStyleFileError(FileName, Style->LineInFile, ConsoleMessage_Error, Message);

            Header.HadError = 1;
            return Header;
        }
    }

    style_token *Name = PeekStyleToken(Buffer, 0);
    {
        if (Name->Type == StyleToken_String)
        {
            Header.StyleName = Name->Identifier;
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Unknown token when declaring a header. Expected 'string' (style name).");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);

            Header.HadError = 1;
            return Header;
        }
    }

    return Header;
}

internal style_variable
ParseStyleVariable(byte_string FileName, style_token_buffer *Buffer)
{
    style_variable Variable = {0};

    style_token *Var = PeekStyleToken(Buffer, 0);
    {
        if(Var->Type == StyleToken_Var)
        {
            ConsumeStyleTokens(Buffer, 1);
            Variable.IsValid = 1;
        }
        else
        {
            return Variable;
        }
    }

    style_token *Name = PeekStyleToken(Buffer, 0);
    {
        if(Name->Type == StyleToken_Identifier)
        {
            Variable.Name = Name->Identifier;
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Exptected an identifier when setting the variable's name.");
            ReportStyleFileError(FileName, Name->LineInFile, ConsoleMessage_Error, Message);
            Variable.IsValid = 0;
        }
    }

    style_token *Assignment = PeekStyleToken(Buffer, 0);
    {
        if(Assignment->Type == StyleToken_Assignment)
        {
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Exptected an assignment (:=) after setting the variable's name");
            ReportStyleFileError(FileName, Assignment->LineInFile, ConsoleMessage_Error, Message);
            Variable.IsValid = 0;
        }
    }

    style_token *Value = PeekStyleToken(Buffer, 0);
    {
        if (Value->Type == StyleToken_Unit || Value->Type == StyleToken_String || Value->Type == StyleToken_Vector)
        {
            Variable.ValueToken = Value;
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Exptected a value after the assignment.");
            ReportStyleFileError(FileName, Value->LineInFile, ConsoleMessage_Error, Message);
            Variable.IsValid = 0;
        }
    }

    style_token *End = PeekStyleToken(Buffer, 0);
    {
        if(End->Type == StyleToken_SemiColon)
        {
            ConsumeStyleTokens(Buffer, 1);
        }
        else
        {
            byte_string Message = byte_string_literal("Exptected ';' after creating a variable.");
            ReportStyleFileError(FileName, End->LineInFile, ConsoleMessage_Error, Message);
            Variable.IsValid = 0;
        }
    }

    if(!Variable.IsValid)
    {
        u32 Line          = Var->LineInFile;
        u8 *VariableStart = Var->FilePointer;
        u8 *VariableEnd   = End->FilePointer;

        // TODO: Push this to front?
        ReportStyleFileError(FileName, Line, ConsoleMessage_Info, ByteString(VariableStart, (VariableEnd - VariableStart)));

        Variable.Name       = ByteString(0, 0);
        Variable.ValueToken = 0;
    }

    return Variable;
}

internal ui_style_subregistry *
ParseStyleFile(style_file *File, memory_arena *Arena)
{
    ui_style_subregistry *Result = PushArray(Arena, ui_style_subregistry, 1);

    if(Result)
    {
        Result->Styles = PushArray(Arena, ui_cached_style, File->StyleCount);

        memory_region TransientData = EnterMemoryRegion(Arena);

        style_var_table *VarTable = 0;
        {
            style_var_table_params Params = {0};
            Params.EntryCount = 0;
            Params.HashCount  = 0;

            size_t Footprint      = GetStyleVarTableFootprint(Params);
            u8    *VarTableMemory = PushArray(Arena, u8, Footprint);

            VarTable = PlaceStyleVarTableInMemory(Params, VarTableMemory);
        }

        if(VarTable)
        {
            style_token *Next = PeekStyleToken(&File->Buffer, 0);
            while (Next->Type != StyleToken_EndOfFile)
            {
                style_variable Variable = ParseStyleVariable(File->Name, &File->Buffer);
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
                        byte_string Message = byte_string_literal("Two variables cannot have the same name");
                        ReportStyleFileError(File->Name, Variable.LineInFile, ConsoleMessage_Error, Message);
                    }

                    Variable = ParseStyleVariable(File->Name, &File->Buffer);
                };

                style Style = {0};
                Style.Header = ParseStyleHeader(File->Name, &File->Buffer);
                Style.Block  = ParseStyleBlock(File->Name , &File->Buffer, VarTable);

                // TODO: Kinda have to create an empty style...

                if(!Style.Header.HadError && Style.Block.AttributeCount > 0)
                {
                    CacheStyle(File->Name, &Style, Result);
                }

                Next = PeekStyleToken(&File->Buffer, 0);
            }
        }
        else
        {
            ReportStyleParserError(ConsoleMessage_Error, byte_string_literal("Could not allocate style_var_table."));
        }

        LeaveMemoryRegion(TransientData);
    }
    else
    {
        ReportStyleParserError(ConsoleMessage_Error, byte_string_literal("Could not allocate subregistry."));
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
        ReportStyleParserError(ConsoleMessage_Error, byte_string_literal("Maximum amount of variables exceeded for file."));
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

    Assert(Result);

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
ValidateAttributeFormatting(byte_string FileName, style_attribute *Attribute)
{
    Assert(!Attribute->HadError && Attribute->IsSet);

    byte_string ErrorMessage = ByteString(0, 0);

    switch(Attribute->PropertyType)
    {

    case StyleProperty_Size:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->VectorSize != 2)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y]");
            break;
        }

        if(!((Attribute->Vector.X.Type == UIUnit_Float32 || Attribute->Vector.X.Type == UIUnit_Percent) &&
             (Attribute->Vector.Y.Type == UIUnit_Float32 || Attribute->Vector.Y.Type == UIUnit_Percent)))
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [(Float OR Percent), (Float OR Percent)]");
            break;
        }
    } break;

    case StyleProperty_Padding:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->VectorSize != 4)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y, Z, W]");
            break;
        }

        vec4_unit Vector = Attribute->Vector;

        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
           Vector.Z.Type != UIUnit_Float32 || Vector.W.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float, Float, Float]");
            break;
        }
    } break;

    case StyleProperty_Spacing:
    {
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->VectorSize != 2)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y]");
            break;
        }

        vec4_unit Vector = Attribute->Vector;

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
        if(Attribute->ParsedAs != StyleToken_Vector || Attribute->VectorSize != 4)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be [Vector][X, Y, Z, W]");
            break;
        }

        vec4_unit Vector = Attribute->Vector;

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
                break;
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
            break;
        }
    } break;

    case StyleProperty_CornerRadius:
    {
        vec4_unit Vector = Attribute->Vector;
        if(Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
           Vector.Z.Type != UIUnit_Float32 || Vector.Z.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Should be: [Float, Float, Float, Float]");
            break;
        }
    } break;

    case StyleProperty_Font:
    {
        if(Attribute->ParsedAs != StyleToken_String)
        {
            ErrorMessage = byte_string_literal("Invalid Format. Value must be a \"string\"");
            break;
        }
    } break;

    default:
    {
        Assert(!"Should not happen");
    } break;

    }

    if(IsValidByteString(ErrorMessage))
    {
        ReportStyleFileError(FileName, Attribute->LineInFile, ConsoleMessage_Error, ErrorMessage);

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
            Property.Vec2.X = Attribute.Vector.X;
            Property.Vec2.Y = Attribute.Vector.Y;
        } break;

        case StyleProperty_Color:
        case StyleProperty_BorderColor:
        case StyleProperty_TextColor:
        {
            Property.Color = ToNormalizedColor(Attribute.Vector);
        } break;

        case StyleProperty_Padding:
        {
            Property.Padding.Left  = Attribute.Vector.X.Float32;
            Property.Padding.Top   = Attribute.Vector.Y.Float32;
            Property.Padding.Right = Attribute.Vector.Z.Float32;
            Property.Padding.Bot   = Attribute.Vector.W.Float32;
        } break;

        case StyleProperty_Spacing:
        {
            Property.Spacing.Horizontal = Attribute.Vector.X.Float32;
            Property.Spacing.Vertical   = Attribute.Vector.Y.Float32;
        } break;

        case StyleProperty_FontSize:
        case StyleProperty_Softness:
        case StyleProperty_BorderWidth:
        {
            Property.Float32 = Attribute.Unit.Float32;
        } break;

        case StyleProperty_CornerRadius:
        {
            Property.CornerRadius.TopLeft  = Attribute.Vector.X.Float32;
            Property.CornerRadius.TopRight = Attribute.Vector.Y.Float32;
            Property.CornerRadius.BotRight = Attribute.Vector.Z.Float32;
            Property.CornerRadius.BotLeft  = Attribute.Vector.W.Float32;
        } break;

        case StyleProperty_Font:
        {
            // TODO: Figure out what to do here.
        } break;

        default:
        {
            Property.IsSet = 0;
        } break;
    }

    return Property;
}

internal void
CacheStyle(byte_string FileName, style *ParsedStyle, ui_style_subregistry *Registry)
{
    Assert(Registry->StyleCount < Registry->StyleCapacity);

    ui_cached_style *CachedStyle = Registry->Styles + Registry->StyleCount++;
    style_header    *Header      = &ParsedStyle->Header;
    style_block     *Block       = &ParsedStyle->Block;

    // Prunes every badly formatted attribute from the list.
    // Also, outputs the corresponding error messages.
    // Then if the value is still set, we cache it.

    ForEachEnum(StyleEffect_Type, StyleEffect_Count, Effect)
    {
        ForEachEnum(StyleProperty_Type, StyleProperty_Count, Property)
        {
            style_attribute Attribute = Block->Attributes[Effect][Property];
            if(Attribute.IsSet)
            {
                ValidateAttributeFormatting(FileName, &Attribute);
                if(Attribute.IsSet)
                {
                    CachedStyle->Properties[Effect][Property] = ConvertAttributeToProperty(Attribute);
                }
            }
        }
    }

    Useless(Header); // NOTE: Useless at the moment. Unsure where I wanna go with this.
}

// [Error Handling]

internal void
SynchronizeParser(style_token_buffer *Buffer, StyleToken_Type StopAt)
{
    while(PeekStyleToken(Buffer, 0)->Type != StopAt)
    {
        ConsumeStyleTokens(Buffer, 1);
    }

    ConsumeStyleTokens(Buffer, 1);
}

internal void
ReportStyleFileError(byte_string FileName, u32 Line, ConsoleMessage_Severity Severity, byte_string Message)
{
    u8          Buffer[Kilobyte(4)] = {0};
    byte_string Error               = ByteString(Buffer, 0);

    Error.Size += snprintf((char *)Error.String             , sizeof(Buffer), "[File %s | Line: %u]", FileName.String, Line);
    Error.Size += snprintf((char *)Error.String + Error.Size, sizeof(Buffer), "[%s]", Message.String);

    ReportStyleParserError(Severity, Error);
}

internal void
ReportStyleParserError(ConsoleMessage_Severity Severity, byte_string Message)
{
    ConsoleWriteMessage(Message, Severity, &Console);
}
