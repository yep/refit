/*
 * refit/main.c
 * Main code for the boot menu
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

#include "syslinux_mbr.h"

// types

typedef struct {
    REFIT_MENU_ENTRY me;
    CHAR16           *LoaderPath;
    CHAR16           *VolName;
    EFI_DEVICE_PATH  *DevicePath;
    BOOLEAN          UseGraphicsMode;
    CHAR16           *LoadOptions;
} LOADER_ENTRY;

typedef struct {
    REFIT_MENU_ENTRY me;
    REFIT_VOLUME     *Volume;
    CHAR16           *LoadOptions;
} LEGACY_ENTRY;

// variables

#define MACOSX_LOADER_PATH      L"\\System\\Library\\CoreServices\\boot.efi"
#define MACOSX_HIBERNATE_PATH   L"\\var\\vm\\sleepimage"

#define TAG_RESET  (1)
#define TAG_ABOUT  (2)
#define TAG_LOADER (3)
#define TAG_LEGACY (4)
#define TAG_TOOL   (5)

static REFIT_MENU_ENTRY MenuEntryReset  = { L"Restart Computer", TAG_RESET, 1, NULL, NULL, NULL };
static REFIT_MENU_ENTRY MenuEntryAbout  = { L"About rEFIt", TAG_ABOUT, 1, NULL, NULL, NULL };
static REFIT_MENU_ENTRY MenuEntryReturn = { L"Return to Main Menu", TAG_RETURN, 0, NULL, NULL, NULL };

static REFIT_MENU_SCREEN MainMenu       = { L"Main Menu", NULL, 0, NULL, 0, NULL, 0, L"Automatic boot" };
static REFIT_MENU_SCREEN AboutMenu      = { L"About", NULL, 0, NULL, 0, NULL, 0, NULL };

//
// misc functions
//

static VOID AboutRefit(VOID)
{
    if (AboutMenu.EntryCount == 0) {
        AboutMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
        AddMenuInfoLine(&AboutMenu, L"rEFIt Version 0.7");
        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2006 Christoph Pfisterer");
        AddMenuInfoLine(&AboutMenu, L"Portions Copyright (c) Intel Corporation and others");
        AddMenuEntry(&AboutMenu, &MenuEntryReturn);
    }
    
    RunMenu(&AboutMenu, NULL);
}

static VOID StartEFIImage(IN EFI_DEVICE_PATH *DevicePath,
                          IN CHAR16 *LoadOptions, IN CHAR16 *LoadOptionsPrefix,
                          IN CHAR16 *ImageTitle)
{
    EFI_STATUS              Status;
    EFI_HANDLE              ChildImageHandle;
    EFI_LOADED_IMAGE        *ChildLoadedImage;
    CHAR16                  ErrorInfo[256];
    CHAR16                  *FullLoadOptions = NULL;
    
    Print(L"Starting %s\n", ImageTitle);
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ChildImageHandle);
    SPrint(ErrorInfo, 255, L"while loading %s", ImageTitle);
    if (CheckError(Status, ErrorInfo))
        goto bailout;
    
    // set load options
    if (LoadOptions != NULL) {
        Status = BS->HandleProtocol(ChildImageHandle, &LoadedImageProtocol, (VOID **) &ChildLoadedImage);
        if (CheckError(Status, L"while getting a LoadedImageProtocol handle"))
            goto bailout_unload;
        
        if (LoadOptionsPrefix != NULL) {
            FullLoadOptions = PoolPrint(L"%s %s ", LoadOptionsPrefix, LoadOptions);
            // NOTE: That last space is also added by the EFI shell and seems to be significant
            //  when passing options to Apple's boot.efi...
            LoadOptions = FullLoadOptions;
        }
        // NOTE: We also include the terminating null in the length for safety.
        ChildLoadedImage->LoadOptions = (VOID *)LoadOptions;
        ChildLoadedImage->LoadOptionsSize = (StrLen(LoadOptions) + 1) * sizeof(CHAR16);
        Print(L"Using load options '%s'\n", LoadOptions);
    }
    
    // turn control over to the image
    // TODO: (optionally) re-enable the EFI watchdog timer!
    Status = BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    SPrint(ErrorInfo, 255, L"returned from %s", ImageTitle);
    CheckError(Status, ErrorInfo);
    
bailout_unload:
    // unload the image, we don't care if it works or not...
    Status = BS->UnloadImage(ChildImageHandle);
bailout:
    if (FullLoadOptions != NULL)
        FreePool(FullLoadOptions);
    FinishExternalScreen();
}

//
// EFI OS loader functions
//

static VOID StartLoader(IN LOADER_ENTRY *Entry)
{
    BeginExternalScreen(Entry->UseGraphicsMode, L"Booting OS");
    StartEFIImage(Entry->DevicePath, Entry->LoadOptions, Basename(Entry->LoaderPath), Basename(Entry->LoaderPath));
}

static LOADER_ENTRY * AddLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume)
{
    CHAR16          *FileName;
    CHAR16          IconFileName[256];
    UINTN           LoaderKind;
    LOADER_ENTRY    *Entry, *SubEntry;
    REFIT_MENU_SCREEN *SubScreen;
    
    FileName = Basename(LoaderPath);
    
    // prepare the menu entry
    Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    Entry->me.Title        = PoolPrint(L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : LoaderPath + 1, Volume->VolName);
    Entry->me.Tag          = TAG_LOADER;
    Entry->me.Row          = 0;
    if (GlobalConfig.HideBadges == 0 ||
        (GlobalConfig.HideBadges == 1 && Volume->DiskKind != DISK_KIND_INTERNAL))
        Entry->me.BadgeImage   = Volume->VolBadgeImage;
    Entry->LoaderPath      = StrDuplicate(LoaderPath);
    Entry->VolName         = Volume->VolName;
    Entry->DevicePath      = FileDevicePath(Volume->DeviceHandle, Entry->LoaderPath);
    Entry->UseGraphicsMode = FALSE;
    
    // locate a custom icon for the loader
    StrCpy(IconFileName, LoaderPath);
    ReplaceExtension(IconFileName, L".icns");
    if (FileExists(Volume->RootDir, IconFileName))
        Entry->me.Image = LoadIcns(Volume->RootDir, IconFileName, 128);
    
    // detect specific loaders
    LoaderKind = 0;
    if (StriCmp(LoaderPath, MACOSX_LOADER_PATH) == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_MAC);
        Entry->UseGraphicsMode = TRUE;
        LoaderKind = 1;
    } else if (StriCmp(FileName, L"diags.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_HWTEST);
    } else if (StriCmp(FileName, L"e.efi") == 0 ||
               StriCmp(FileName, L"elilo.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_LINUX);
        LoaderKind = 2;
    } else if (StriCmp(FileName, L"Bootmgfw.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_WIN);
    } else if (StriCmp(FileName, L"xom.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_WIN);
        Entry->UseGraphicsMode = TRUE;
        LoaderKind = 3;
    }
    if (Entry->me.Image == NULL)
        Entry->me.Image = BuiltinIcon(BUILTIN_ICON_OS_UNKNOWN);
    
    // create the submenu
    SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
    SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", (LoaderTitle != NULL) ? LoaderTitle : FileName, Volume->VolName);
    SubScreen->TitleImage = Entry->me.Image;
    
    // default entry
    SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    SubEntry->me.Title        = (LoaderKind == 1) ? L"Boot Mac OS X" : PoolPrint(L"Run %s", FileName);
    SubEntry->me.Tag          = TAG_LOADER;
    SubEntry->LoaderPath      = Entry->LoaderPath;
    SubEntry->VolName         = Entry->VolName;
    SubEntry->DevicePath      = Entry->DevicePath;
    SubEntry->UseGraphicsMode = Entry->UseGraphicsMode;
    AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    
    // loader-specific submenu entries
    if (LoaderKind == 1) {          // entries for Mac OS X
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Mac OS X in verbose mode";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = FALSE;
        SubEntry->LoadOptions     = L"-v";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Mac OS X in single user mode";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = FALSE;
        SubEntry->LoadOptions     = L"-v -s";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
    } else if (LoaderKind == 2) {   // entries for elilo
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = PoolPrint(L"Run %s in interactive mode", FileName);
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = Entry->UseGraphicsMode;
        SubEntry->LoadOptions     = L"-p";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Linux for a 17\" iMac or a 15\" MacBook Pro (*)";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = TRUE;
        SubEntry->LoadOptions     = L"-d 0 i17";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Linux for a 20\" iMac (*)";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = TRUE;
        SubEntry->LoadOptions     = L"-d 0 i20";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Linux for a Mac Mini (*)";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = TRUE;
        SubEntry->LoadOptions     = L"-d 0 mini";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        AddMenuInfoLine(SubScreen, L"NOTE: This is an example. Entries");
        AddMenuInfoLine(SubScreen, L"marked with (*) may not work.");
        
    } else if (LoaderKind == 3) {   // entries for xom.efi
        // by default, skip the built-in selection and boot from hard disk only
        Entry->LoadOptions = L"-s -h";
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Windows from Hard Disk";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = Entry->UseGraphicsMode;
        SubEntry->LoadOptions     = L"-s -h";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = L"Boot Windows from CD-ROM";
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = Entry->UseGraphicsMode;
        SubEntry->LoadOptions     = L"-s -c";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
        SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        SubEntry->me.Title        = PoolPrint(L"Run %s in text mode", FileName);
        SubEntry->me.Tag          = TAG_LOADER;
        SubEntry->LoaderPath      = Entry->LoaderPath;
        SubEntry->VolName         = Entry->VolName;
        SubEntry->DevicePath      = Entry->DevicePath;
        SubEntry->UseGraphicsMode = FALSE;
        SubEntry->LoadOptions     = L"-v";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        
    }
    
    AddMenuEntry(SubScreen, &MenuEntryReturn);
    Entry->me.SubScreen = SubScreen;
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return Entry;
}

static VOID FreeLoaderEntry(IN LOADER_ENTRY *Entry)
{
    FreePool(Entry->me.Title);
    FreePool(Entry->LoaderPath);
    FreePool(Entry->DevicePath);
}

static VOID ScanLoaderDir(IN REFIT_VOLUME *Volume, IN CHAR16 *Path)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  FileName[256];
    
    // look through contents of the directory
    DirIterOpen(Volume->RootDir, Path, &DirIter);
    while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
        if (StriCmp(DirEntry->FileName, L"TextMode.efi") == 0 ||
            StriCmp(DirEntry->FileName, L"ebounce.efi") == 0 ||
            StriCmp(DirEntry->FileName, L"GraphicsConsole.efi") == 0)
            continue;   // skip this
        
        if (Path)
            SPrint(FileName, 255, L"\\%s\\%s", Path, DirEntry->FileName);
        else
            SPrint(FileName, 255, L"\\%s", DirEntry->FileName);
        AddLoaderEntry(FileName, NULL, Volume);
    }
    Status = DirIterClose(&DirIter);
    if (Status != EFI_NOT_FOUND) {
        if (Path)
            SPrint(FileName, 255, L"while scanning the %s directory", Path);
        else
            StrCpy(FileName, L"while scanning the root directory");
        CheckError(Status, FileName);
    }
}

static VOID ScanLoader(VOID)
{
    EFI_STATUS              Status;
    UINTN                   VolumeIndex;
    REFIT_VOLUME            *Volume;
    REFIT_DIR_ITER          EfiDirIter;
    EFI_FILE_INFO           *EfiDirEntry;
    CHAR16                  FileName[256];
    LOADER_ENTRY            *Entry;
    
    Print(L"Scanning for boot loaders...\n");
    
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];
        if (Volume->RootDir == NULL || Volume->VolName == NULL)
            continue;
        
        // check for Mac OS X boot loader
        StrCpy(FileName, MACOSX_LOADER_PATH);
        if (FileExists(Volume->RootDir, FileName)) {
            Print(L"  - Mac OS X boot file found\n");
            Entry = AddLoaderEntry(FileName, L"Mac OS X", Volume);
            /*
            if (FileExists(Volume->RootDir, MACOSX_HIBERNATE_PATH)) {
                // system is suspended in Safe Sleep, skip the menu
                StartLoader(Entry);
            }
             */
        }
        
        // check for XOM
        StrCpy(FileName, L"\\System\\Library\\CoreServices\\xom.efi");
        if (FileExists(Volume->RootDir, FileName)) {
            AddLoaderEntry(FileName, L"Windows XP (XoM)", Volume);
        }
        
        // check for Microsoft boot loader/menu
        StrCpy(FileName, L"\\EFI\\Microsoft\\Boot\\Bootmgfw.efi");
        if (FileExists(Volume->RootDir, FileName)) {
            Print(L"  - Microsoft boot menu found\n");
            AddLoaderEntry(FileName, L"Microsoft boot menu", Volume);
        }
        
        // scan the root directory for EFI executables
        ScanLoaderDir(Volume, NULL);
        // scan the elilo directory (as used on gimli's first Live CD)
        ScanLoaderDir(Volume, L"elilo");
        // scan the boot directory
        ScanLoaderDir(Volume, L"boot");
        
        // scan subdirectories of the EFI directory (as per the standard)
        DirIterOpen(Volume->RootDir, L"EFI", &EfiDirIter);
        while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (StriCmp(EfiDirEntry->FileName, L"TOOLS") == 0 || EfiDirEntry->FileName[0] == '.')
                continue;   // skip this, doesn't contain boot loaders
            if (StriCmp(EfiDirEntry->FileName, L"REFIT") == 0 || StriCmp(EfiDirEntry->FileName, L"REFITL") == 0)
                continue;   // skip ourselves
            Print(L"  - Directory EFI\\%s found\n", EfiDirEntry->FileName);
            
            SPrint(FileName, 255, L"EFI\\%s", EfiDirEntry->FileName);
            ScanLoaderDir(Volume, FileName);
        }
        Status = DirIterClose(&EfiDirIter);
        if (Status != EFI_NOT_FOUND)
            CheckError(Status, L"while scanning the EFI directory");
        
        // check for Apple hardware diagnostics
        StrCpy(FileName, L"\\System\\Library\\CoreServices\\.diagnostics\\diags.efi");
        if (FileExists(Volume->RootDir, FileName)) {
            Print(L"  - Apple Hardware Test found\n");
            AddLoaderEntry(FileName, L"Apple Hardware Test", Volume);
        }
    }
}

//
// legacy boot functions
//

static UINT8 LegacyLoaderDevicePathData[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF9, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};

static EFI_STATUS ActivateMbrPartition(IN EFI_BLOCK_IO *BlockIO, IN UINTN PartitionIndex)
{
    EFI_STATUS          Status;
    UINT8               SectorBuffer[512];
    MBR_PARTITION_INFO  *MbrTable;
    UINTN               i;
    BOOLEAN             HaveBootCode;
    
    // read MBR
    Status = BlockIO->ReadBlocks(BlockIO, BlockIO->Media->MediaId, 0, 512, SectorBuffer);
    if (EFI_ERROR(Status))
        return Status;
    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55)
        return EFI_NOT_FOUND;  // safety measure #1
    
    // add boot code if necessary
    HaveBootCode = FALSE;
    for (i = 0; i < MBR_BOOTCODE_SIZE; i++) {
        if (SectorBuffer[i] != 0) {
            HaveBootCode = TRUE;
            break;
        }
    }
    if (!HaveBootCode) {
        // no boot code found in the MBR, add the syslinux MBR code
        SetMem(SectorBuffer, MBR_BOOTCODE_SIZE, 0);
        CopyMem(SectorBuffer, syslinux_mbr, SYSLINUX_MBR_SIZE);
    }
    
    // set the partition active
    MbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80)
            return EFI_NOT_FOUND;   // safety measure #2
        MbrTable[i].Flags = (i == PartitionIndex) ? 0x80 : 0x00;
    }
    
    // write MBR
    Status = BlockIO->WriteBlocks(BlockIO, BlockIO->Media->MediaId, 0, 512, SectorBuffer);
    if (EFI_ERROR(Status))
        return Status;
    
    return EFI_SUCCESS;
}

static VOID StartLegacy(IN LEGACY_ENTRY *Entry)
{
    BeginExternalScreen(TRUE, L"Booting Legacy OS");
    
    if (Entry->Volume->IsMbrPartition)
        ActivateMbrPartition(Entry->Volume->WholeDiskBlockIO, Entry->Volume->MbrPartitionIndex);
    
    StartEFIImage((EFI_DEVICE_PATH *)LegacyLoaderDevicePathData,
                  Entry->LoadOptions, NULL, L"legacy loader");
}

static LEGACY_ENTRY * AddLegacyEntry(IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume)
{
    LEGACY_ENTRY            *Entry, *SubEntry;
    REFIT_MENU_SCREEN       *SubScreen;
    CHAR16                  *VolDesc;
    
    if (LoaderTitle == NULL) {
        if (Volume->BootCodeDetected == BOOTCODE_WINDOWS)
            LoaderTitle = L"Windows";
        else if (Volume->BootCodeDetected == BOOTCODE_LINUX)
            LoaderTitle = L"Linux";
        else
            LoaderTitle = L"Legacy OS";
    }
    if (Volume->VolName != NULL)
        VolDesc = Volume->VolName;
    else
        VolDesc = (Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" : L"HD";
    
    // prepare the menu entry
    Entry = AllocateZeroPool(sizeof(LEGACY_ENTRY));
    Entry->me.Title        = PoolPrint(L"Boot %s from %s", LoaderTitle, VolDesc);
    Entry->me.Tag          = TAG_LEGACY;
    Entry->me.Row          = 0;
    if (Volume->BootCodeDetected == BOOTCODE_WINDOWS)
        Entry->me.Image    = BuiltinIcon(BUILTIN_ICON_OS_WIN);
    else if (Volume->BootCodeDetected == BOOTCODE_LINUX)
        Entry->me.Image    = BuiltinIcon(BUILTIN_ICON_OS_LINUX);
    else
        Entry->me.Image    = BuiltinIcon(BUILTIN_ICON_OS_LEGACY);
    if (GlobalConfig.HideBadges == 0 ||
        (GlobalConfig.HideBadges == 1 && Volume->DiskKind != DISK_KIND_INTERNAL))
        Entry->me.BadgeImage   = Volume->VolBadgeImage;
    Entry->Volume          = Volume;
    Entry->LoadOptions     = (Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" : L"HD";
    
    // create the submenu
    SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
    SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", LoaderTitle, VolDesc);
    SubScreen->TitleImage = Entry->me.Image;
    
    // default entry
    SubEntry = AllocateZeroPool(sizeof(LEGACY_ENTRY));
    SubEntry->me.Title        = PoolPrint(L"Boot %s", LoaderTitle);
    SubEntry->me.Tag          = TAG_LEGACY;
    SubEntry->Volume          = Entry->Volume;
    SubEntry->LoadOptions     = Entry->LoadOptions;
    AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    
    AddMenuEntry(SubScreen, &MenuEntryReturn);
    Entry->me.SubScreen = SubScreen;
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return Entry;
}

static VOID ScanLegacy(VOID)
{
    UINTN                   VolumeIndex, VolumeIndex2;
    BOOLEAN                 ShowVolume, HideIfOthersFound;
    REFIT_VOLUME            *Volume;
    
    Print(L"Scanning for legacy boot volumes...\n");
    
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];
#if REFIT_DEBUG > 0
        Print(L" %d %s\n  %d %d %s %d %s\n",
              VolumeIndex, DevicePathToStr(Volume->DevicePath),
              Volume->DiskKind, Volume->MbrPartitionIndex,
              Volume->IsAppleLegacy ? L"AL" : L"--", Volume->BootCodeDetected,
               Volume->VolName ? Volume->VolName : L"(no name)");
#endif
        
        ShowVolume = FALSE;
        HideIfOthersFound = FALSE;
        if (Volume->IsAppleLegacy) {
            ShowVolume = TRUE;
            HideIfOthersFound = TRUE;
        } else if (Volume->BootCodeDetected) {
            ShowVolume = TRUE;
            if (Volume->BlockIO == Volume->WholeDiskBlockIO &&
                Volume->BootCodeDetected == BOOTCODE_UNKNOWN)
                // this is a whole disk entry; hide if we have entries for partitions
                HideIfOthersFound = TRUE;
        }
        if (HideIfOthersFound) {
            // check for other bootable entries on the same disk
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (VolumeIndex2 != VolumeIndex && Volumes[VolumeIndex2]->BootCodeDetected &&
                    Volumes[VolumeIndex2]->WholeDiskBlockIO == Volume->WholeDiskBlockIO)
                    ShowVolume = FALSE;
            }
        }
        
        if (ShowVolume)
            AddLegacyEntry(NULL, Volume);
    }
}

//
// pre-boot tool functions
//

static VOID StartTool(IN LOADER_ENTRY *Entry)
{
    BeginExternalScreen(Entry->UseGraphicsMode, Entry->me.Title + 6);  // assumes "Start <title>" as assigned below
    StartEFIImage(Entry->DevicePath, Entry->LoadOptions, Basename(Entry->LoaderPath),
                  Basename(Entry->LoaderPath));
}

static LOADER_ENTRY * AddToolEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, EG_IMAGE *Image, BOOLEAN UseGraphicsMode)
{
    LOADER_ENTRY *Entry;
    
    Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    
    Entry->me.Title = PoolPrint(L"Start %s", LoaderTitle);
    Entry->me.Tag = TAG_TOOL;
    Entry->me.Row = 1;
    Entry->me.Image = Image;
    Entry->LoaderPath = StrDuplicate(LoaderPath);
    Entry->DevicePath = FileDevicePath(SelfLoadedImage->DeviceHandle, Entry->LoaderPath);
    Entry->UseGraphicsMode = UseGraphicsMode;
    
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return Entry;
}

static VOID FreeToolEntry(IN LOADER_ENTRY *Entry)
{
    FreePool(Entry->me.Title);
    FreePool(Entry->LoaderPath);
    FreePool(Entry->DevicePath);
}

static VOID ScanTool(VOID)
{
    //EFI_STATUS              Status;
    CHAR16                  FileName[256];
    
    Print(L"Scanning for tools...\n");
    
    // look for the EFI shell
    if (!(GlobalConfig.HideUIFlags & (HIDEUI_FLAG_SHELL | HIDEUI_FLAG_TOOLS))) {
        SPrint(FileName, 255, L"%s\\apps\\shell.efi", SelfDirPath);
        if (FileExists(SelfRootDir, FileName)) {
            AddToolEntry(FileName, L"EFI Shell", BuiltinIcon(BUILTIN_ICON_TOOL_SHELL), FALSE);
        } else {
            StrCpy(FileName, L"\\efi\\tools\\shell.efi");
            if (FileExists(SelfRootDir, FileName)) {
                AddToolEntry(FileName, L"EFI Shell", BuiltinIcon(BUILTIN_ICON_TOOL_SHELL), FALSE);
            }
        }
    }
}

//
// main entry point
//

#ifdef __GNUC__
#define RefitMain efi_main
#endif

EFI_STATUS
EFIAPI
RefitMain (IN EFI_HANDLE           ImageHandle,
           IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS Status;
    BOOLEAN MainLoopRunning = TRUE;
    REFIT_MENU_ENTRY *ChosenEntry;
    UINTN MenuExit;
    UINTN i;
    
    // bootstrap
    InitializeLib(ImageHandle, SystemTable);
    InitScreen();
    Status = InitRefitLib(ImageHandle);
    if (EFI_ERROR(Status))
        return Status;
    
    // read configuration
    ReadConfig();
    MainMenu.TimeoutSeconds = GlobalConfig.Timeout;
    
    // disable EFI watchdog timer
    BS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);
    
    // further bootstrap (now with config available)
    SetupScreen();
    ScanVolumes();
    DebugPause();
    
    // scan for loaders and tools, add them to the menu
    if (GlobalConfig.LegacyFirst)
        ScanLegacy();
    ScanLoader();
    if (!GlobalConfig.LegacyFirst)
        ScanLegacy();
    ScanTool();
    DebugPause();
    
    // fixed other menu entries
    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_FUNCS)) {
        MenuEntryAbout.Image = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
        AddMenuEntry(&MainMenu, &MenuEntryAbout);
        MenuEntryReset.Image = BuiltinIcon(BUILTIN_ICON_FUNC_RESET);
        AddMenuEntry(&MainMenu, &MenuEntryReset);
    }
    
    // wait for user ACK when there were errors
    FinishTextScreen(FALSE);
    
    while (MainLoopRunning) {
        MenuExit = RunMainMenu(&MainMenu, &ChosenEntry);
        
        if (MenuExit == MENU_EXIT_ESCAPE)
            break;
        
        switch (ChosenEntry->Tag) {
            
            case TAG_RESET:    // Reboot
                TerminateScreen();
                RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
                MainLoopRunning = FALSE;   // just in case we get this far
                break;
                
            case TAG_ABOUT:    // About rEFIt
                AboutRefit();
                break;
                
            case TAG_LOADER:   // Boot OS via .EFI loader
                StartLoader((LOADER_ENTRY *)ChosenEntry);
                break;
                
            case TAG_LEGACY:   // Boot legacy OS
                StartLegacy((LEGACY_ENTRY *)ChosenEntry);
                break;
                
            case TAG_TOOL:     // Start a EFI tool
                StartTool((LOADER_ENTRY *)ChosenEntry);
                break;
                
        }
    }
    
    for (i = 0; i < MainMenu.EntryCount; i++) {
        if (MainMenu.Entries[i]->Tag == TAG_LOADER) {
            FreeLoaderEntry((LOADER_ENTRY *)(MainMenu.Entries[i]));
            FreePool(MainMenu.Entries[i]);
        } else if (MainMenu.Entries[i]->Tag == TAG_TOOL) {
            FreeToolEntry((LOADER_ENTRY *)(MainMenu.Entries[i]));
            FreePool(MainMenu.Entries[i]);
        }
    }
    FreePool(MainMenu.Entries);
    
    // clear screen completely
    TerminateScreen();
    return EFI_SUCCESS;
}
