/*
 * refit/lib.h
 * General header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
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

#include "libeg.h"

#define REFIT_DEBUG (0)

//
// lib module
//

typedef struct {
    EFI_STATUS          LastStatus;
    EFI_FILE            *DirHandle;
    BOOLEAN             CloseDirHandle;
    EFI_FILE_INFO       *LastFileInfo;
} REFIT_DIR_ITER;

typedef struct {
    UINT8 Flags;
    UINT8 StartCHS1;
    UINT8 StartCHS2;
    UINT8 StartCHS3;
    UINT8 Type;
    UINT8 EndCHS1;
    UINT8 EndCHS2;
    UINT8 EndCHS3;
    UINT32 StartLBA;
    UINT32 Size;
} MBR_PARTITION_INFO;

#define DISK_KIND_INTERNAL  (0)
#define DISK_KIND_EXTERNAL  (1)
#define DISK_KIND_OPTICAL   (2)

#define IS_EXTENDED_PART_TYPE(type) ((type) == 0x05 || (type) == 0x0f || (type) == 0x85)

typedef struct {
    EFI_DEVICE_PATH     *DevicePath;
    EFI_HANDLE          DeviceHandle;
    EFI_FILE            *RootDir;
    CHAR16              *VolName;
    EG_IMAGE            *VolBadgeImage;
    UINTN               DiskKind;
    BOOLEAN             IsAppleLegacy;
    BOOLEAN             HasBootCode;
    CHAR16              *OSIconName;
    CHAR16              *OSName;
    BOOLEAN             IsMbrPartition;
    UINTN               MbrPartitionIndex;
    EFI_BLOCK_IO        *BlockIO;
    UINT64              BlockIOOffset;
    EFI_BLOCK_IO        *WholeDiskBlockIO;
    EFI_DEVICE_PATH     *WholeDiskDevicePath;
    MBR_PARTITION_INFO  *MbrPartitionTable;
} REFIT_VOLUME;

extern EFI_HANDLE       SelfImageHandle;
extern EFI_LOADED_IMAGE *SelfLoadedImage;
extern EFI_FILE         *SelfRootDir;
extern EFI_FILE         *SelfDir;
extern CHAR16           *SelfDirPath;

extern REFIT_VOLUME     *SelfVolume;
extern REFIT_VOLUME     **Volumes;
extern UINTN            VolumesCount;

EFI_STATUS InitRefitLib(IN EFI_HANDLE ImageHandle);
VOID UninitRefitLib(VOID);
EFI_STATUS ReinitRefitLib(VOID);

VOID CreateList(OUT VOID ***ListPtr, OUT UINTN *ElementCount, IN UINTN InitialElementCount);
VOID AddListElement(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement);
VOID FreeList(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount /*, IN Callback*/);

VOID ExtractLegacyLoaderPaths(EFI_DEVICE_PATH **PathList, UINTN MaxPaths, EFI_DEVICE_PATH **HardcodedPathList);

VOID ScanVolumes(VOID);

BOOLEAN FileExists(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath);

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode);

VOID DirIterOpen(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath OPTIONAL, OUT REFIT_DIR_ITER *DirIter);
BOOLEAN DirIterNext(IN OUT REFIT_DIR_ITER *DirIter, IN UINTN FilterMode, IN CHAR16 *FilePattern OPTIONAL, OUT EFI_FILE_INFO **DirEntry);
EFI_STATUS DirIterClose(IN OUT REFIT_DIR_ITER *DirIter);

CHAR16 * Basename(IN CHAR16 *Path);
VOID ReplaceExtension(IN OUT CHAR16 *Path, IN CHAR16 *Extension);

INTN FindMem(IN VOID *Buffer, IN UINTN BufferLength, IN VOID *SearchString, IN UINTN SearchStringLength);

//
// screen module
//

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR (EFI_YELLOW | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

#define LAYOUT_TEXT_WIDTH (512)
#define LAYOUT_TOTAL_HEIGHT (368)
#define LAYOUT_BANNER_HEIGHT (32)
#define LAYOUT_BANNER_YOFFSET (LAYOUT_BANNER_HEIGHT + 32)

#define FONT_CELL_WIDTH (7)
#define FONT_CELL_HEIGHT (12)

extern UINTN ConWidth;
extern UINTN ConHeight;
extern CHAR16 *BlankLine;

extern UINTN UGAWidth;
extern UINTN UGAHeight;
extern BOOLEAN AllowGraphicsMode;

extern EG_PIXEL StdBackgroundPixel;
extern EG_PIXEL MenuBackgroundPixel;

VOID InitScreen(VOID);
VOID SetupScreen(VOID);
VOID BeginTextScreen(IN CHAR16 *Title);
VOID FinishTextScreen(IN BOOLEAN WaitAlways);
VOID BeginExternalScreen(IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title);
VOID FinishExternalScreen(VOID);
VOID TerminateScreen(VOID);
#if REFIT_DEBUG > 0
VOID DebugPause(VOID);
#else
#define DebugPause()
#endif
VOID EndlessIdleLoop(VOID);

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where);
BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where);

VOID SwitchToGraphicsAndClear(VOID);
VOID BltClearScreen(IN BOOLEAN ShowBanner);
VOID BltImage(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos);
VOID BltImageAlpha(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos, IN EG_PIXEL *BackgroundPixel);
VOID BltImageComposite(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN UINTN XPos, IN UINTN YPos);
VOID BltImageCompositeBadge(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN EG_IMAGE *BadgeImage, IN UINTN XPos, IN UINTN YPos);

//
// icns loader module
//

EG_IMAGE * LoadOSIcon(IN CHAR16 *OSIconName OPTIONAL, IN CHAR16 *FallbackIconName, BOOLEAN BootLogo);

EG_IMAGE * LoadIcns(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize);
EG_IMAGE * LoadIcnsFallback(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize);
EG_IMAGE * DummyImage(IN UINTN PixelSize);

EG_IMAGE * BuiltinIcon(IN UINTN Id);

#define BUILTIN_ICON_FUNC_ABOUT     (0)
#define BUILTIN_ICON_FUNC_RESET     (1)
#define BUILTIN_ICON_FUNC_SHUTDOWN  (2)
#define BUILTIN_ICON_TOOL_SHELL     (3)
#define BUILTIN_ICON_TOOL_PART      (4)
#define BUILTIN_ICON_TOOL_RESCUE    (5)
#define BUILTIN_ICON_VOL_INTERNAL   (6)
#define BUILTIN_ICON_VOL_EXTERNAL   (7)
#define BUILTIN_ICON_VOL_OPTICAL    (8)
#define BUILTIN_ICON_COUNT          (9)

//
// menu module
//

#define MENU_EXIT_ENTER   (1)
#define MENU_EXIT_ESCAPE  (2)
#define MENU_EXIT_DETAILS (3)
#define MENU_EXIT_TIMEOUT (4)

#define TAG_RETURN       (99)

struct _refit_menu_screen;

typedef struct _refit_menu_entry {
    CHAR16      *Title;
    UINTN       Tag;
    UINTN       Row;
    CHAR16      ShortcutDigit;
    CHAR16      ShortcutLetter;
    EG_IMAGE    *Image;
    EG_IMAGE    *BadgeImage;
    struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
    CHAR16      *Title;
    EG_IMAGE    *TitleImage;
    UINTN       InfoLineCount;
    CHAR16      **InfoLines;
    UINTN       EntryCount;
    REFIT_MENU_ENTRY **Entries;
    UINTN       TimeoutSeconds;
    CHAR16      *TimeoutText;
} REFIT_MENU_SCREEN;

VOID AddMenuInfoLine(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine);
VOID AddMenuEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry);
VOID FreeMenu(IN REFIT_MENU_SCREEN *Screen);
UINTN RunMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry);
UINTN RunMainMenu(IN REFIT_MENU_SCREEN *Screen, IN CHAR16* DefaultSelection, OUT REFIT_MENU_ENTRY **ChosenEntry);

//
// config module
//

typedef struct {
    UINT8   *Buffer;
    UINTN   BufferSize;
    UINTN   Encoding;
    CHAR8   *Current8Ptr;
    CHAR8   *End8Ptr;
    CHAR16  *Current16Ptr;
    CHAR16  *End16Ptr;
} REFIT_FILE;

#define DISABLE_FLAG_SHELL      (0x0001)
#define DISABLE_FLAG_TOOLS      (0x0002)
#define DISABLE_FLAG_OPTICAL    (0x0004)
#define DISABLE_FLAG_EXTERNAL   (0x0008)
#define DISABLE_FLAG_INTERNAL   (0x0010)
#define DISABLE_FLAG_SINGLEUSER (0x0020)
#define DISABLE_FLAG_HWTEST     (0x0040)
#define DISABLE_ALL             ((0xffff) & (~DISABLE_FLAG_INTERNAL))

#define HIDEUI_FLAG_BANNER      (0x0001)
#define HIDEUI_FLAG_FUNCS       (0x0002)
#define HIDEUI_FLAG_LABEL       (0x0004)
#define HIDEUI_ALL              (0xffff)

typedef struct {
    BOOLEAN     TextOnly;
    UINTN       Timeout;
    UINTN       DisableFlags;
    UINTN       HideBadges;
    UINTN       HideUIFlags;
    BOOLEAN     LegacyFirst;
    CHAR16      *BannerFileName;
    CHAR16      *SelectionSmallFileName;
    CHAR16      *SelectionBigFileName;
    CHAR16      *DefaultSelection;
} REFIT_CONFIG;

extern REFIT_CONFIG GlobalConfig;

VOID ReadConfig(VOID);

/* EOF */
