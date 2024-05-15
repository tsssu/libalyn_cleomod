#include <mod/amlmod.h>
#include <mod/logger.h>
#include <cleohelpers.h>
#include <cleo4scmfunc.h>

// There wont be that much opcodes, because some of them are in their own plugins

std::deque<PausedScriptInfo> pausedScripts;
extern void (*UpdateCompareFlag)(void*, uint8_t);
extern GTAScript **pActiveScripts;
void CleoReturnGeneric(void* handle, bool returnArgs, int returnArgCount);


// Game vars
bool *m_CodePause;
uint16_t *NumberOfIntroTextLinesThisFrame;
char *IntroTextLines;

// Debug plugin (need additional work)
CLEO_Fn(DEBUG_ON)
{
    GetAddonInfo(handle).debugMode = true;
}
CLEO_Fn(DEBUG_OFF)
{
    GetAddonInfo(handle).debugMode = false;
}
CLEO_Fn(BREAKPOINT)
{
    if(!GetAddonInfo(handle).debugMode)
    {
        SkipUnusedParameters(handle);
        return;
    }

    char fmt[MAX_STR_LEN], buf[MAX_STR_LEN];
    bool blocking = true;
    if(Read1Byte_NoSkip(handle) == SCRIPT_PARAM_STATIC_INT_8BITS)
    {
        blocking = !!cleo->ReadParam(handle)->i;
    }
    if(Read1Byte_NoSkip(handle) == SCRIPT_PARAM_END_OF_ARGUMENTS)
    {
        Skip1Byte(handle);
        buf[0] = 0;
    }
    else
    {
        CLEO_ReadStringEx(handle, fmt, sizeof(fmt));
        CLEO_FormatString(handle, buf, sizeof(buf), fmt);
    }

    pausedScripts.emplace_back(handle, buf);
    snprintf(fmt, sizeof(fmt), "Script breakpoint '%s' captured in '%s'", buf, ((GTAScript*)handle)->name);
    cleo->PrintToCleoLog(fmt);

    if(blocking)
    {
        cleo->PrintToCleoLog("Game paused");
        *m_CodePause = true;
    }
}
CLEO_Fn(TRACE)
{
    if(!GetAddonInfo(handle).debugMode)
    {
        SkipUnusedParameters(handle);
        return;
    }

    char text[MAX_STR_LEN], fmt[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, fmt, sizeof(fmt));
    CLEO_FormatString(handle, text, sizeof(text), fmt);
    cleo->PrintToCleoLog(text);
}
CLEO_Fn(LOG_TO_FILE)
{
    char to[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, to, sizeof(to));
    std::string tostr = ResolvePath(handle, to);

    bool bTimestamp = cleo->ReadParam(handle)->i;
    if(bTimestamp)
    {

    }

    char fmt[MAX_STR_LEN], buf[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, fmt, sizeof(fmt));
    CLEO_FormatString(handle, buf, sizeof(buf), fmt);
    
    // Need additional work
}

// MemOps plugin
  // A scripter should use 0DD7 on mobile (get image base) if he works with GTASA address.
  // Not adding "add_ib" argument here for compatibility
CLEO_Fn(COPY_MEMORY)
{
    uint32_t source = cleo->ReadParam(handle)->u;
    uint32_t target = cleo->ReadParam(handle)->u;
    int size = cleo->ReadParam(handle)->i;
    aml->Write(source, target, size);
}
CLEO_Fn(READ_MEMORY_WITH_OFFSET)
{
    uint32_t source = cleo->ReadParam(handle)->u;
    uint32_t offset = cleo->ReadParam(handle)->u;
    int size = cleo->ReadParam(handle)->i;

    if(IsParamString(handle))
    {
        std::string str(std::string_view((char*)source + offset, size)); // null terminated
        CLEO_WriteStringEx(handle, str.c_str());
    }
    else
    {
        int result = 0;
        if(size > 0 && size <= sizeof(result))
        {
            memcpy(&result, (void*)(source + offset), size);
        }
        cleo->GetPointerToScriptVar(handle)->i = result;
    }
}
CLEO_Fn(WRITE_MEMORY_WITH_OFFSET)
{
    uint32_t source = cleo->ReadParam(handle)->u;
    uint32_t offset = cleo->ReadParam(handle)->u;
    int size = cleo->ReadParam(handle)->i;

    if(size <= 0)
    {
        SkipOpcodeParameters(handle, 1);
        return;
    }

    if(IsParamString(handle))
    {
        char buf[MAX_STR_LEN];
        CLEO_ReadStringEx(handle, buf, sizeof(buf));
        int len = strlen(buf);
        memcpy((void*)(source + offset), buf, (size < len) ? size : len);
        if (size > len) memset((void*)(source + offset + len), 0, size - len);
    }
    else
    {
        int value = cleo->ReadParam(handle)->i;
        if(size > sizeof(value)) size = sizeof(value);

        memcpy((void*)(source + offset), &value, size);
    }
}
CLEO_Fn(FORGET_MEMORY)
{
    FreeMem((void*)cleo->ReadParam(handle)->u);
}
CLEO_Fn(GET_SCRIPT_STRUCT_JUST_CREATED)
{
    GTAScript* head = (GTAScript*)handle;
    while(head->prev != NULL)
    {
        head = head->prev;
    }
    cleo->GetPointerToScriptVar(handle)->i = (int)head;
}
CLEO_Fn(IS_SCRIPT_RUNNING)
{
    GTAScript* scriptStruct = (GTAScript*)cleo->ReadParam(handle)->i;
    for (GTAScript* script = *pActiveScripts; script != NULL; script = script->next)
    {
        if(scriptStruct == script)
        {
            UpdateCompareFlag(handle, true);
            return;
        }
    }
    UpdateCompareFlag(handle, false);
}
CLEO_Fn(GET_SCRIPT_STRUCT_FROM_FILENAME)
{
    char buf[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, buf, sizeof(buf));
    void* script = CLEO_GetScriptFromFilename(buf);
    cleo->GetPointerToScriptVar(handle)->i = (int)script;
    UpdateCompareFlag(handle, script != NULL);
}
CLEO_Fn(IS_MEMORY_EQUAL)
{
    void* source = (void*)cleo->ReadParam(handle)->u;
    void* target = (void*)cleo->ReadParam(handle)->u;
    int size = cleo->ReadParam(handle)->i;
    UpdateCompareFlag(handle, (memcmp(source, target, size)) == 0);
}

// Text plugin
CLEO_Fn(IS_TEXT_EMPTY)
{
    char buf[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, buf, sizeof(buf));
    UpdateCompareFlag(handle, buf[0] == 0);
}
CLEO_Fn(IS_TEXT_EQUAL)
{
    char bufa[MAX_STR_LEN], bufb[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, bufa, sizeof(bufa));
    CLEO_ReadStringEx(handle, bufb, sizeof(bufb));
    bool ignoreCase = !!cleo->ReadParam(handle)->i;

    UpdateCompareFlag(handle, (ignoreCase ? strcasecmp(bufa, bufb) : strcmp(bufa, bufb)) == 0);
}
CLEO_Fn(IS_TEXT_IN_TEXT)
{
    char bufa[MAX_STR_LEN], bufb[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, bufa, sizeof(bufa));
    CLEO_ReadStringEx(handle, bufb, sizeof(bufb));
    bool ignoreCase = !!cleo->ReadParam(handle)->i;

    UpdateCompareFlag(handle, (ignoreCase ? strcasestr(bufa, bufb) : strstr(bufa, bufb)) != NULL);
}
CLEO_Fn(IS_TEXT_PREFIX)
{
    char bufa[MAX_STR_LEN], bufb[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, bufa, sizeof(bufa));
    CLEO_ReadStringEx(handle, bufb, sizeof(bufb));
    bool ignoreCase = !!cleo->ReadParam(handle)->i;
    int prefixlen = strlen(bufb);

    UpdateCompareFlag(handle, (ignoreCase ? strncasecmp(bufa, bufb, prefixlen) : strncmp(bufa, bufb, prefixlen)) == 0);
}
CLEO_Fn(IS_TEXT_SUFFIX)
{
    char bufa[MAX_STR_LEN], bufb[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, bufa, sizeof(bufa));
    CLEO_ReadStringEx(handle, bufb, sizeof(bufb));
    bool ignoreCase = !!cleo->ReadParam(handle)->i;
    int suffixOff = strlen(bufa) - strlen(bufb);

    UpdateCompareFlag(handle, (ignoreCase ? strcasecmp(bufa + suffixOff, bufb) : strcmp(bufa + suffixOff, bufb)) == 0);
}
CLEO_Fn(DISPLAY_TEXT_FORMATTED)
{
    float posX = cleo->ReadParam(handle)->f;
    float posY = cleo->ReadParam(handle)->f;

    char fmt[MAX_STR_LEN], buf[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, fmt, sizeof(fmt));
    CLEO_FormatString(handle, buf, sizeof(buf), fmt);

    char* introTxtLine = IntroTextLines + *NumberOfIntroTextLinesThisFrame * ValueForGame(0xF4, 0xF4, 0x44);
    if(*nGameIdent == GTASA)
    {
        // new GXT label
        // includes unprintable character, to ensure there will be no collision with user GXT lables
        char gxt[8] = { 0x01, 'C', 'L', 'E', 'O', '_', 0x01, 0x00 };
        gxt[6] += *NumberOfIntroTextLinesThisFrame; // unique label for each possible entry

        AddGXTLabel(gxt, buf);

        *(float*)(introTxtLine + 0x2C) = posX;
        *(float*)(introTxtLine + 0x30) = posY;
        strncpy(introTxtLine + 0x34, gxt, sizeof(gxt));
    }
    else
    {
        uint16_t gxt[256];
        AsciiToGXTChar(buf, gxt);
        memcpy(introTxtLine + 0x2C, &gxt[0], 200);
        *(float*)(introTxtLine + 0x24) = posX;
        *(float*)(introTxtLine + 0x28) = posY;
    }
    ++(*NumberOfIntroTextLinesThisFrame);
}

// FileSystemOperations
CLEO_Fn(GET_FILE_POSITION)
{
    FILE* file = (FILE*)cleo->ReadParam(handle)->i;
    cleo->GetPointerToScriptVar(handle)->i = file ? ftell(file) : 0;
}
CLEO_Fn(READ_BLOCK_FROM_FILE)
{
    FILE* file = (FILE*)cleo->ReadParam(handle)->i;
    int size = cleo->ReadParam(handle)->i;
    char* buffer = (char*)cleo->ReadParam(handle)->i;
    int didRead = fread(buffer, 1, size, file);
    fseek(file, 0, SEEK_CUR); // required for RW streams (https://en.wikibooks.org/wiki/C_Programming/stdio.h/fopen)
    UpdateCompareFlag(handle, didRead == size);
}
CLEO_Fn(WRITE_BLOCK_TO_FILE)
{
    FILE* file = (FILE*)cleo->ReadParam(handle)->i;
    int size = cleo->ReadParam(handle)->i;
    char* buffer = (char*)cleo->ReadParam(handle)->i;
    int didWrite = fwrite(buffer, 1, size, file);
    fseek(file, 0, SEEK_CUR); // required for RW streams (https://en.wikibooks.org/wiki/C_Programming/stdio.h/fopen)
    UpdateCompareFlag(handle, didWrite == size);
}
CLEO_Fn(RESOLVE_FILEPATH)
{
    char path[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, path, sizeof(path));

    std::string resolved = ResolvePath(handle, path);
    CLEO_WriteStringEx(handle, resolved.c_str());
}
CLEO_Fn(GET_SCRIPT_FILENAME)
{
    void* script = (void*)cleo->ReadParam(handle)->i;
    bool fullPath = cleo->ReadParam(handle)->i;

    if(script == (void*)-1) script = handle;

    if(GetAddonInfo(script).isCustom)
    {
        const char* filename = CLEO_GetScriptFilename(script);
        if(!filename)
        {
            SkipOpcodeParameters(handle, 1);
            UpdateCompareFlag(handle, false);
            return;
        }

        if(fullPath)
        {
            std::string fullfilename = cleo->GetCleoStorageDir();
            if(fullfilename.back() != '/' && fullfilename.back() != '\\') fullfilename += '/';
            fullfilename += filename;
            CLEO_WriteStringEx(handle, fullfilename.c_str());
        }
        else
        {
            CLEO_WriteStringEx(handle, filename);
        }
    }
    else
    {
        if(fullPath)
        {
            std::string fullfilename = aml->GetAndroidDataPath();
            if(fullfilename.back() != '/' && fullfilename.back() != '\\') fullfilename += '/';
            fullfilename += "data/script/";
            fullfilename += ((GTAScript*)script)->name;
            CLEO_WriteStringEx(handle, fullfilename.c_str()); // stupid but ok...
        }
        else
        {
            CLEO_WriteStringEx(handle, ((GTAScript*)script)->name);
        }
    }
}
CLEO_Fn(DELETE_FILE)
{
    char path[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, path, sizeof(path));
    std::string str = ResolvePath(handle, path);
    int removeStatus = remove(str.c_str());
    UpdateCompareFlag(handle, removeStatus == 0);
}
CLEO_Fn(DELETE_DIRECTORY)
{
    char path[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, path, sizeof(path));
    std::string str = ResolvePath(handle, path);
    bool deleteContents = cleo->ReadParam(handle)->i;
    bool removed = deleteContents ? fs::remove(str.c_str()) : fs::remove_all(str.c_str());
    UpdateCompareFlag(handle, removed);
}
CLEO_Fn(MOVE_FILE)
{
    char from[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, from, sizeof(from));
    std::string fromstr = ResolvePath(handle, from);

    char to[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, to, sizeof(to));
    std::string tostr = ResolvePath(handle, to);

    std::error_code ec; ec.clear();
    fs::rename(from, to, ec);
    UpdateCompareFlag(handle, ec.value() == 0);
}
CLEO_Fn(MOVE_DIRECTORY)
{
    char from[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, from, sizeof(from));
    std::string fromstr = ResolvePath(handle, from);

    char to[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, to, sizeof(to));
    std::string tostr = ResolvePath(handle, to);

    std::error_code ec; ec.clear();
    fs::rename(from, to, ec);
    UpdateCompareFlag(handle, ec.value() == 0);
}
CLEO_Fn(COPY_FILE)
{
    char from[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, from, sizeof(from));
    std::string fromstr = ResolvePath(handle, from);

    char to[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, to, sizeof(to));
    std::string tostr = ResolvePath(handle, to);

    bool copied = fs::copy_file(from, to, fs::copy_options::skip_existing);
    UpdateCompareFlag(handle, copied);
}
CLEO_Fn(COPY_DIRECTORY)
{
    char from[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, from, sizeof(from));
    std::string fromstr = ResolvePath(handle, from);

    char to[MAX_STR_LEN];
    CLEO_ReadStringEx(handle, to, sizeof(to));
    std::string tostr = ResolvePath(handle, to);

    std::error_code ec; ec.clear();
    fs::copy(from, to, fs::copy_options::skip_existing | fs::copy_options::directories_only, ec);
    UpdateCompareFlag(handle, ec.value() == 0);
}

// CLEO 5
CLEO_Fn(GET_CLEO_ARG_COUNT)
{
    ScmFunction *scmFunc = ScmFunction::Store[GetScmFunc(handle)];
    cleo->GetPointerToScriptVar(handle)->i = scmFunc->callArgCount;
}
CLEO_Fn(CLEO_RETURN_WITH)
{
    int argCount = GetVarArgCount(handle);
    int result = cleo->ReadParam(handle)->i;
    UpdateCompareFlag(handle, result != 0);
    CleoReturnGeneric(handle, true, argCount - 1);
}
CLEO_Fn(CLEO_RETURN_FAIL)
{
    int argCount = GetVarArgCount(handle);
    UpdateCompareFlag(handle, false);
    CleoReturnGeneric(handle, false, 0);
}

void Init5Opcodes()
{
    SET_TO(m_CodePause,                     cleo->GetMainLibrarySymbol("_ZN6CTimer11m_CodePauseE"));
    SET_TO(NumberOfIntroTextLinesThisFrame, cleo->GetMainLibrarySymbol("_ZN11CTheScripts31NumberOfIntroTextLinesThisFrameE"));
    SET_TO(IntroTextLines,                  cleo->GetMainLibrarySymbol("_ZN11CTheScripts14IntroTextLinesE"));

    // Debug plugin
    CLEO_RegisterOpcode(0x00C3, DEBUG_ON); // 00C3=0, debug_on
    CLEO_RegisterOpcode(0x00C4, DEBUG_OFF); // 00C4=0, debug_off
    CLEO_RegisterOpcode(0x2100, BREAKPOINT); // 2100=-1, breakpoint ...
    CLEO_RegisterOpcode(0x2101, TRACE); // 2101=-1, trace %1s% ...
    CLEO_RegisterOpcode(0x2102, LOG_TO_FILE); // 2102=-1, log_to_file %1s% timestamp %2d% text %3s% ...

    // MemOps plugin (early CLEO **always** had it in themselves. What's the purpose of removing it?!)
    // Literally brainless move... #1
    CLEO_RegisterOpcode(0x2400, COPY_MEMORY); // 2400=3, copy_memory %1d% to %2d% size %3d%
    CLEO_RegisterOpcode(0x2401, READ_MEMORY_WITH_OFFSET); // 2401=4, read_memory_with_offset %1d% offset %2d% size %3d% store_to %4d%
    CLEO_RegisterOpcode(0x2402, WRITE_MEMORY_WITH_OFFSET); // 2402=4, write_memory_with_offset %1d% offset %2d% size %3d% value %4d%
    CLEO_RegisterOpcode(0x2403, FORGET_MEMORY); // 2403=1, forget_memory %1d%
    CLEO_RegisterOpcode(0x2404, GET_SCRIPT_STRUCT_JUST_CREATED); // 2404=1, get_script_struct_just_created %1d%
    CLEO_RegisterOpcode(0x2405, IS_SCRIPT_RUNNING); // 2405=1, is_script_running %1d%
    CLEO_RegisterOpcode(0x2406, GET_SCRIPT_STRUCT_FROM_FILENAME); // 2406=1, get_script_struct_from_filename %1s%
    CLEO_RegisterOpcode(0x2407, IS_MEMORY_EQUAL); // 2407=3, is_memory_equal address_a %1d% address_b %2d% size %d3%

    // Text plugin
    // Literally brainless move... #2
    CLEO_RegisterOpcode(0x2600, IS_TEXT_EMPTY); // 2600=1, is_text_empty %1s%
    CLEO_RegisterOpcode(0x2601, IS_TEXT_EQUAL); // 2601=3, is_text_equal %1s% another %2s% ignore_case %3d%
    CLEO_RegisterOpcode(0x2602, IS_TEXT_IN_TEXT); // 2602=3, is_text_in_text %1s% sub_text %2s% ignore_case %3d%
    CLEO_RegisterOpcode(0x2603, IS_TEXT_PREFIX); // 2603=3, is_text_prefix %1s% prefix %2s% ignore_case %3d%
    CLEO_RegisterOpcode(0x2604, IS_TEXT_SUFFIX); // 2604=3, is_text_suffix %1s% suffix %2s% ignore_case %3d% // originally it's sufix *facepalm*
    CLEO_RegisterOpcode(0x2605, DISPLAY_TEXT_FORMATTED); // 2605=-1, display_text_formatted offset_left %1d% offset_top %2d% format %3d% args

    // FileSystemOperations
    // Literally brainless move... #3
    CLEO_RegisterOpcode(0x2300, GET_FILE_POSITION); // 2300=2, get_file_position %1d% store_to %2d%
    CLEO_RegisterOpcode(0x2301, READ_BLOCK_FROM_FILE); // 2301=3, read_block_from_file %1d% size %2d% buffer %3d% // IF and SET
    CLEO_RegisterOpcode(0x2302, WRITE_BLOCK_TO_FILE); // 2302=3, write_block_to_file %1d% size %2d% address %3d% // IF and SET
    CLEO_RegisterOpcode(0x2303, RESOLVE_FILEPATH); // 2303=2, %2s% = resolve_filepath %1s%
    CLEO_RegisterOpcode(0x2304, GET_SCRIPT_FILENAME); // 2304=3, %3s% = get_script_filename %1d% full_path %2d% // IF and SET
    CLEO_RegisterOpcode(0x0B00, DELETE_FILE); // 0B00=1, delete_file %1s% //IF and SET
    CLEO_RegisterOpcode(0x0B01, DELETE_DIRECTORY); // 0B01=1, delete_directory %1s% with_all_files_and_subdirectories %2d% //IF and SET
    CLEO_RegisterOpcode(0x0B02, MOVE_FILE); // 0B02=2, move_file %1s% to %2s% //IF and SET
    CLEO_RegisterOpcode(0x0B03, MOVE_DIRECTORY); // 0B03=2, move_directory %1s% to %2s% //IF and SET
    CLEO_RegisterOpcode(0x0B04, COPY_FILE); // 0B04=2, copy_file %1s% to %2s% //IF and SET
    CLEO_RegisterOpcode(0x0B05, COPY_DIRECTORY); // 0B05=2, copy_directory %1d% to %2d% //IF and SET

    // CLEO 5
    CLEO_RegisterOpcode(0x2000, GET_CLEO_ARG_COUNT); // 2000=1, %1d% = get_cleo_arg_count
    CLEO_RegisterOpcode(0x2002, CLEO_RETURN_WITH); // 2002=-1, cleo_return_with ...
    CLEO_RegisterOpcode(0x2003, CLEO_RETURN_FAIL); // 2003=-1, cleo_return_fail
}