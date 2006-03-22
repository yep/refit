/*
 * refit/lib.h
 * General header file
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

#include "efi.h"
#include "efilib.h"

#include "ConsoleControl.h"

/* defines */

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR (EFI_YELLOW | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

#define LAYOUT_TEXT_WIDTH (512)
#define LAYOUT_TOTAL_HEIGHT (368)

#define FONT_CELL_WIDTH (7)
#define FONT_CELL_HEIGHT (12)

/* menu structures */

typedef struct {
    const UINT8 *PixelData;
    UINTN Width, Height;
} REFIT_IMAGE;

#define DUMMY_IMAGE(name) static REFIT_IMAGE name = { NULL, 0, 0 };

typedef struct {
    CHAR16 *Title;
    UINTN Tag;
    VOID *UserData;
    UINTN Row;
    REFIT_IMAGE *Image;
} REFIT_MENU_ENTRY;

typedef struct {
    CHAR16 *Title;
    UINTN EntryCount;
    UINTN AllocatedEntryCount;
    REFIT_MENU_ENTRY *Entries;
    UINTN TimeoutSeconds;
    CHAR16 *TimeoutText;
} REFIT_MENU_SCREEN;

/* dir scan structure */

typedef struct {
    EFI_STATUS LastStatus;
    EFI_FILE *DirHandle;
    BOOLEAN CloseDirHandle;
    EFI_FILE_INFO *LastFileInfo;
} REFIT_DIR_ITER;

/* lib functions */

BOOLEAN FileExists(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath);

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode);

VOID DirIterOpen(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath OPTIONAL, OUT REFIT_DIR_ITER *DirIter);
BOOLEAN DirIterNext(IN OUT REFIT_DIR_ITER *DirIter, IN UINTN FilterMode, IN CHAR16 *FilePattern OPTIONAL, OUT EFI_FILE_INFO **DirEntry);
EFI_STATUS DirIterClose(IN OUT REFIT_DIR_ITER *DirIter);

/* screen functions */

extern UINTN ConWidth;
extern UINTN ConHeight;
extern CHAR16 *BlankLine;

extern UINTN UGAWidth;
extern UINTN UGAHeight;
extern BOOLEAN AllowGraphicsMode;

VOID InitScreen(VOID);
VOID BeginTextScreen(IN CHAR16 *Title);
VOID FinishTextScreen(IN BOOLEAN WaitAlways);
VOID BeginExternalScreen(IN UINTN Mode, IN CHAR16 *Title);
VOID FinishExternalScreen(VOID);
VOID TerminateScreen(VOID);

VOID SwitchToGraphicsAndClear(VOID);

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where);
BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where);

#ifndef TEXTONLY
VOID BltClearScreen(VOID);
VOID BltImage(IN REFIT_IMAGE *Image, IN UINTN XPos, IN UINTN YPos);
VOID BltImageComposite(IN REFIT_IMAGE *BaseImage, IN REFIT_IMAGE *TopImage, IN UINTN XPos, IN UINTN YPos);
VOID RenderText(IN CHAR16 *Text, IN OUT REFIT_IMAGE *BackBuffer);
#endif  /* !TEXTONLY */

/* menu functions */

VOID MenuAddEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry);
VOID MenuFree(IN REFIT_MENU_SCREEN *Screen);
VOID MenuRun(IN BOOLEAN HasGraphics, IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry);

/* EOF */
