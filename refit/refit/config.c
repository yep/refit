/*
 * refit/config.c
 * Configuration file functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "lib.h"

// constants

#define MAXCONFIGFILESIZE (64*1024)

#define ENCODING_ISO8859_1  (0)
#define ENCODING_UTF8       (1)
#define ENCODING_UTF16_LE   (2)

// global configuration with default values

REFIT_CONFIG        GlobalConfig = { 20, 0, FALSE };

//
// read a file into a buffer
//

static EFI_STATUS ReadFile(REFIT_VOLUME *Volume, CHAR16 *FilePath, REFIT_FILE *File)
{
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO   *FileInfo;
    UINT64          ReadSize;
    
    File->Buffer = NULL;
    File->BufferSize = 0;
    
    // read the file, allocating a buffer on the woy
    Status = Volume->RootDir->Open(Volume->RootDir, &FileHandle, FilePath, EFI_FILE_MODE_READ, 0);
    if (CheckError(Status, L"while loading the configuration file"))
        return Status;
    
    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        // TODO: print and register the error
        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    if (ReadSize > MAXCONFIGFILESIZE)
        ReadSize = MAXCONFIGFILESIZE;
    FreePool(FileInfo);
    
    File->BufferSize = (UINTN)ReadSize;   // was limited to a few K before, so this is safe
    File->Buffer = AllocatePool(File->BufferSize);
    Status = FileHandle->Read(FileHandle, &File->BufferSize, File->Buffer);
    if (CheckError(Status, L"while loading the configuration file")) {
        FreePool(File->Buffer);
        File->Buffer = NULL;
        return Status;
    }
    Status = FileHandle->Close(FileHandle);
    
    // setup for reading
    File->Current8Ptr  = (CHAR8 *)File->Buffer;
    File->End8Ptr      = File->Current8Ptr + File->BufferSize;
    File->Current16Ptr = (CHAR16 *)File->Buffer;
    File->End16Ptr     = File->Current16Ptr + (File->BufferSize >> 1);
    
    // detect encoding
    File->Encoding = ENCODING_ISO8859_1;   // default: 1:1 translation of CHAR8 to CHAR16
    if (File->BufferSize >= 4) {
        if (File->Buffer[0] == 0xFF && File->Buffer[1] == 0xFE) {
            // BOM in UTF-16 little endian (or UTF-32 little endian)
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
            File->Current16Ptr++;
        } else if (File->Buffer[0] == 0xEF && File->Buffer[1] == 0xBB && File->Buffer[2] == 0xBF) {
            // BOM in UTF-8
            File->Encoding = ENCODING_UTF8;       // translate from UTF-8 to UTF-16
            File->Current8Ptr += 3;
        } else if (File->Buffer[1] == 0 && File->Buffer[3] == 0) {
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
        }
        // TODO: detect other encodings as they are implemented
    }
    
    return EFI_SUCCESS;
}

//
// get a single line of text from a file
//

static CHAR16 *ReadLine(REFIT_FILE *File)
{
    CHAR16  *Line, *q;
    UINTN   LineLength;
    
    if (File->Buffer == NULL)
        return NULL;
    
    if (File->Encoding == ENCODING_ISO8859_1 || File->Encoding == ENCODING_UTF8) {
        
        CHAR8 *p, *LineStart, *LineEnd;
        
        p = File->Current8Ptr;
        if (p >= File->End8Ptr)
            return NULL;
        
        LineStart = p;
        for (; p < File->End8Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End8Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current8Ptr = p;
        
        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;
        
        if (File->Encoding == ENCODING_ISO8859_1) {
            for (p = LineStart, q = Line; p < LineEnd; )
                *q++ = *p++;
        } else if (File->Encoding == ENCODING_UTF8) {
            // TODO: actually handle UTF-8
            for (p = LineStart, q = Line; p < LineEnd; )
                *q++ = *p++;
        }
        *q = 0;
        
    } else if (File->Encoding == ENCODING_UTF16_LE) {
        
        CHAR16 *p, *LineStart, *LineEnd;
        
        p = File->Current16Ptr;
        if (p >= File->End16Ptr)
            return NULL;
        
        LineStart = p;
        for (; p < File->End16Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End16Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current16Ptr = p;
        
        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;
        
        for (p = LineStart, q = Line; p < LineEnd; )
            *q++ = *p++;
        *q = 0;
        
    } else
        return NULL;   // unsupported encoding
    
    return Line;
}

//
// get a line of tokens from a file
//

static VOID ReadTokenLine(IN REFIT_FILE *File, OUT CHAR16 ***TokenList, OUT UINTN *TokenCount)
{
    BOOLEAN         LineFinished;
    CHAR16          *Line, *Token, *p;
    
    *TokenCount = 0;
    *TokenList = NULL;
    
    while (*TokenCount == 0) {
        Line = ReadLine(File);
        if (Line == NULL)
            return;
        
        p = Line;
        LineFinished = FALSE;
        while (!LineFinished) {
            // skip whitespace
            while (*p == ' ' || *p == '\t' || *p == '=')
                p++;
            if (*p == 0 || *p == '#')
                break;
            
            Token = p;
            
            // find end of token
            while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '#')
                p++;
            if (*p == 0 || *p == '#')
                LineFinished = TRUE;
            *p++ = 0;
            
            AddListElement((VOID ***)TokenList, TokenCount, (VOID *)StrDuplicate(Token));
        }
        
        FreePool(Line);
    }
}

static VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount)
{
    // TODO: also free the items
    FreeList((VOID ***)TokenList, TokenCount);
}

//
// handle a parameter with a single integer argument
//

static VOID HandleInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT UINTN *Value)
{
    if (TokenCount < 2) {
        return;
    }
    if (TokenCount > 2) {
        return;
    }
    *Value = Atoi(TokenList[1]);
}

//
// handle an enumeration parameter
//

static VOID HandleEnum(IN CHAR16 **TokenList, IN UINTN TokenCount, IN CHAR16 **EnumList, IN UINTN EnumCount, OUT UINTN *Value)
{
    UINTN i;
    
    if (TokenCount < 2) {
        return;
    }
    if (TokenCount > 2) {
        return;
    }
    // look for the enum value
    for (i = 0; i < EnumCount; i++)
        if (StriCmp(EnumList[i], TokenList[1]) == 0) {
            *Value = i;
            return;
        }
    // try to handle an int instead
    *Value = Atoi(TokenList[1]);
}

//
// read config file
//

static CHAR16 *HideBadgesEnum[3] = { L"none", L"internal", L"all" };

VOID ReadConfig(VOID)
{
    EFI_STATUS      Status;
    REFIT_FILE      File;
    CHAR16          *FilePath;
    CHAR16          **TokenList;
    UINTN           TokenCount;
    
    if (SelfVolume == NULL)
        return;
    FilePath = PoolPrint(L"%s\\refit.conf", SelfDirPath);
    if (!FileExists(SelfVolume->RootDir, FilePath)) {
        FreePool(FilePath);
        return;
    }
    
    Print(L"Reading configuration file...\n");
    Status = ReadFile(SelfVolume, FilePath, &File);
    FreePool(FilePath);
    if (EFI_ERROR(Status))
        return;
    
    for (;;) {
        ReadTokenLine(&File, &TokenList, &TokenCount);
        if (TokenCount == 0)
            break;
        
        if (StriCmp(TokenList[0], L"timeout") == 0) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.Timeout));
        } else if (StriCmp(TokenList[0], L"hidebadges") == 0) {
            HandleEnum(TokenList, TokenCount, HideBadgesEnum, 3, &(GlobalConfig.HideBadges));
        } else if (StriCmp(TokenList[0], L"textonly") == 0) {
            SetTextOnly();
        } else if (StriCmp(TokenList[0], L"legacyfirst") == 0) {
            GlobalConfig.LegacyFirst = TRUE;
        } else {
            Print(L" unknown configuration command: '%s'\n", TokenList[0]);
        }
        
        FreeTokenLine(&TokenList, &TokenCount);
    }
    
    FreePool(File.Buffer);
}
