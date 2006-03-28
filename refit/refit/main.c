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

// types

typedef struct {
    REFIT_MENU_ENTRY me;
    CHAR16           *LoaderPath;
    CHAR16           *VolName;
    EFI_DEVICE_PATH  *DevicePath;
    BOOLEAN          UseGraphicsMode;
} LOADER_ENTRY;

// variables

#define TAG_EXIT   (1)
#define TAG_RESET  (2)
#define TAG_ABOUT  (3)
#define TAG_LOADER (4)
#define TAG_TOOL   (5)

static REFIT_MENU_ENTRY entry_exit    = { L"Exit to built-in Boot Manager", TAG_EXIT, 1, NULL, NULL };
static REFIT_MENU_ENTRY entry_reset   = { L"Restart Computer", TAG_RESET, 1, NULL, NULL };
static REFIT_MENU_ENTRY entry_about   = { L"About rEFIt", TAG_ABOUT, 1, NULL, NULL };
static REFIT_MENU_SCREEN main_menu    = { L"rEFIt - Main Menu", NULL, 0, NULL, 0, NULL, 20, L"Automatic boot" };

static REFIT_MENU_ENTRY about_exit_entry = { L"Return to Main Menu", 0, 0, NULL, NULL };
static REFIT_MENU_SCREEN about_menu      = { L"rEFIt - About", NULL, 0, NULL, 0, NULL, 0, NULL };

#define MACOSX_LOADER_PATH L"\\System\\Library\\CoreServices\\boot.efi"


static void about_refit(void)
{
    if (about_menu.TitleImage == NULL) {
        about_menu.TitleImage = BuiltinIcon(4);
        AddMenuInfoLine(&about_menu, L"rEFIt Version 0.4");
        AddMenuInfoLine(&about_menu, L"");
        AddMenuInfoLine(&about_menu, L"Copyright (c) 2006 Christoph Pfisterer");
        AddMenuInfoLine(&about_menu, L"Portions Copyright (c) Intel Corporation and others");
        AddMenuEntry(&about_menu, &about_exit_entry);
    }
    
    RunMenu(&about_menu, NULL);
}


static void start_loader(IN LOADER_ENTRY *Entry)
{
    EFI_STATUS              Status;
    EFI_HANDLE              ChildImageHandle;
    CHAR16                  ErrorInfo[256];
    
    BeginExternalScreen(Entry->UseGraphicsMode ? 1 : 0, L"rEFIt - Booting OS");
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, Entry->DevicePath, NULL, 0, &ChildImageHandle);
    SPrint(ErrorInfo, 255, L"while loading %s on %s", Entry->LoaderPath, Entry->VolName);
    if (CheckError(Status, ErrorInfo))
        goto bailout;
    
    // turn control over to the image
    // TODO: re-enable the EFI watchdog timer!
    BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    
bailout:
    FinishExternalScreen();
}

static void add_loader_entry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN EFI_HANDLE DeviceHandle,
                             IN EFI_FILE *RootDir, IN CHAR16 *VolName, IN REFIT_IMAGE *VolBadgeImage)
{
    CHAR16          *FileName;
    CHAR16          IconFileName[256];
    UINTN           i;
    LOADER_ENTRY    *Entry;
    
    Entry = AllocatePool(sizeof(LOADER_ENTRY));
    
    // find the file name
    FileName = LoaderPath;
    for (i = StrLen(LoaderPath) - 1; i >= 0; i--) {
        if (LoaderPath[i] == '\\') {
            FileName = LoaderPath + i + 1;
            break;
        }
    }
    
    // prepare the menu entry
    if (LoaderTitle == NULL)
        LoaderTitle = LoaderPath + 1;
    Entry->me.Title = PoolPrint(L"Boot %s from %s", LoaderTitle, VolName);
    Entry->me.Tag = TAG_LOADER;
    Entry->me.Row = 0;
    Entry->me.Image = NULL;
    Entry->me.BadgeImage = VolBadgeImage;
    Entry->LoaderPath = StrDuplicate(LoaderPath);
    Entry->VolName = StrDuplicate(VolName);
    Entry->DevicePath = FileDevicePath(DeviceHandle, Entry->LoaderPath);
    Entry->UseGraphicsMode = FALSE;
    
#ifndef TEXTONLY
    // locate a custom icon for the loader
    StrCpy(IconFileName, LoaderPath);
    for (i = StrLen(IconFileName) - 1; i >= 0; i--) {
        if (IconFileName[i] == '.') {
            IconFileName[i] = 0;
            break;
        }
        if (IconFileName[i] == '\\')
            break;
    }
    StrCat(IconFileName, L".icns");
    if (FileExists(RootDir, IconFileName))
        Entry->me.Image = LoadIcns(RootDir, IconFileName, 128);
#endif  /* !TEXTONLY */
    
    // determine default icon and graphics mode setting
    if (StriCmp(LoaderPath, MACOSX_LOADER_PATH) == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(0);  // os_mac
        Entry->UseGraphicsMode = TRUE;
    } else if (StriCmp(FileName, L"e.efi") == 0 ||
               StriCmp(FileName, L"elilo.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(1);  // os_linux
    } else if (StriCmp(FileName, L"Bootmgfw.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(2);  // os_win
    } else if (StriCmp(FileName, L"xom.efi") == 0) {
        if (Entry->me.Image == NULL)
            Entry->me.Image = BuiltinIcon(2);  // os_win
        Entry->UseGraphicsMode = TRUE;
    }
    if (Entry->me.Image == NULL) {
        Entry->me.Image = BuiltinIcon(3);  // os_unknown
    }
    
    AddMenuEntry(&main_menu, (REFIT_MENU_ENTRY *)Entry);
}

static void free_loader_entry(IN LOADER_ENTRY *Entry)
{
    FreePool(Entry->me.Title);
    FreePool(Entry->LoaderPath);
    FreePool(Entry->VolName);
    FreePool(Entry->DevicePath);
}

static void loader_scan_dir(IN EFI_FILE *RootDir, IN CHAR16 *Path, IN EFI_HANDLE DeviceHandle,
                            IN CHAR16 *VolName, IN REFIT_IMAGE *VolBadgeImage)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  FileName[256];
    
    // look through contents of the directory
    DirIterOpen(RootDir, Path, &DirIter);
    while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
        if (StriCmp(DirEntry->FileName, L"TextMode.efi") == 0 ||
            StriCmp(DirEntry->FileName, L"ebounce.efi") == 0 ||
            StriCmp(DirEntry->FileName, L"GraphicsConsole.efi") == 0)
            continue;   // skip this
        
        if (Path)
            SPrint(FileName, 255, L"\\%s\\%s", Path, DirEntry->FileName);
        else
            SPrint(FileName, 255, L"\\%s", DirEntry->FileName);
        add_loader_entry(FileName, NULL, DeviceHandle, RootDir, VolName, VolBadgeImage);
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

static void loader_scan(void)
{
    EFI_STATUS              Status;
    UINTN                   HandleCount = 0;
    UINTN                   HandleIndex;
    EFI_HANDLE              *Handles;
    EFI_HANDLE              DeviceHandle;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_FILE                *RootDir;
    EFI_FILE_SYSTEM_INFO    *FileSystemInfoPtr;
    CHAR16                  *VolName;
    REFIT_IMAGE             *VolBadgeImage;
    UINTN                   VolumeKind;
    REFIT_DIR_ITER          EfiDirIter;
    EFI_FILE_INFO           *EfiDirEntry;
    CHAR16                  FileName[256];
    
    Print(L"Scanning for boot loaders...\n");
    
    // get all filesystem handles
    Status = LibLocateHandle(ByProtocol, &FileSystemProtocol, NULL, &HandleCount, &Handles);
    if (Status == EFI_NOT_FOUND)
        return;  // no filesystems. strange, but true...
    if (CheckError(Status, L"while listing all file systems"))
        return;
    // iterate over the filesystem handles
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        DeviceHandle = Handles[HandleIndex];
        
        RootDir = LibOpenRoot(DeviceHandle);
        if (RootDir == NULL) {
            Print(L"Error: Can't open volume.\n");
            // TODO: signal that we had an error
            continue;
        }
        
        // get volume name
        FileSystemInfoPtr = LibFileSystemInfo(RootDir);
        if (FileSystemInfoPtr != NULL) {
            Print(L"  Volume %s\n", FileSystemInfoPtr->VolumeLabel);
            VolName = StrDuplicate(FileSystemInfoPtr->VolumeLabel);
            FreePool(FileSystemInfoPtr);
        } else {
            Print(L"  GetInfo failed\n");
            VolName = StrDuplicate(L"Unnamed Volume");
        }
        
        // get volume icon
        VolBadgeImage = NULL;
        if (FileExists(RootDir, L".VolumeIcon.icns"))
            VolBadgeImage = LoadIcns(RootDir, L".VolumeIcon.icns", 32);
        if (VolBadgeImage == NULL) {
            // use a generic icon, try to determine what kind of disk this is
            
            VolumeKind = 8;   // default: internal disk
            DevicePath = DevicePathFromHandle(DeviceHandle);
            //if (DevicePath != NULL)
            //    Print(L"  * %s\n", DevicePathToStr(DevicePath));
            while (DevicePath != NULL && !IsDevicePathEndType(DevicePath)) {
                //Print(L"  + type %d sub-type %d length %d\n", DevicePathType(DevicePath), DevicePathSubType(DevicePath), DevicePathNodeLength(DevicePath));
                if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH &&
                    (DevicePathSubType(DevicePath) == MSG_USB_DP ||
                     DevicePathSubType(DevicePath) == MSG_USB_CLASS_DP ||
                     DevicePathSubType(DevicePath) == MSG_1394_DP ||
                     DevicePathSubType(DevicePath) == MSG_FIBRECHANNEL_DP))
                    VolumeKind = 9;   // external disk
                if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH &&
                    DevicePathSubType(DevicePath) == MEDIA_CDROM_DP)
                    VolumeKind = 10;   // optical disk
                // TODO: the following is a bad, hardware-dependent hack!
                if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH &&
                    DevicePathSubType(DevicePath) == MSG_ATAPI_DP) {
                    ATAPI_DEVICE_PATH *AtapiPath = (ATAPI_DEVICE_PATH *)DevicePath;
                    if (AtapiPath->PrimarySecondary == 0 && AtapiPath->SlaveMaster == 0)
                        VolumeKind = 10;   // optical disk
                }
                DevicePath = NextDevicePathNode(DevicePath);
            }
            //CheckError(EFI_LOAD_ERROR, L"FOR DISLPAY ONLY");
            
            VolBadgeImage = BuiltinIcon(VolumeKind);
        }
        
        // check for Mac OS X boot loader
        StrCpy(FileName, MACOSX_LOADER_PATH);
        if (FileExists(RootDir, FileName)) {
            Print(L"  - Mac OS X boot file found\n");
            add_loader_entry(FileName, L"Mac OS X", DeviceHandle, RootDir, VolName, VolBadgeImage);
        }
        
        // check for XOM
        StrCpy(FileName, L"\\System\\Library\\CoreServices\\xom.efi");
        if (FileExists(RootDir, FileName)) {
            add_loader_entry(FileName, L"Windows XP (XoM)", DeviceHandle, RootDir, VolName, VolBadgeImage);
        }
        
        // check for Microsoft boot loader/menu
        StrCpy(FileName, L"\\EFI\\Microsoft\\Boot\\Bootmgfw.efi");
        if (FileExists(RootDir, FileName)) {
            Print(L"  - Microsoft boot menu found\n");
            add_loader_entry(FileName, L"Microsoft boot menu", DeviceHandle, RootDir, VolName, VolBadgeImage);
        }
        
        // scan the root directory for EFI executables
        loader_scan_dir(RootDir, NULL, DeviceHandle, VolName, VolBadgeImage);
        // scan the elilo directory (as used on gimli's first Live CD)
        loader_scan_dir(RootDir, L"elilo", DeviceHandle, VolName, VolBadgeImage);
        // scan the boot directory
        loader_scan_dir(RootDir, L"boot", DeviceHandle, VolName, VolBadgeImage);
        
        // scan subdirectories of the EFI directory (as per the standard)
        DirIterOpen(RootDir, L"EFI", &EfiDirIter);
        while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (StriCmp(EfiDirEntry->FileName, L"TOOLS") == 0)
                continue;   // skip this, doesn't contain boot loaders
            if (StriCmp(EfiDirEntry->FileName, L"REFIT") == 0 || StriCmp(EfiDirEntry->FileName, L"REFITL") == 0)
                continue;   // skip ourselves
            Print(L"  - Directory EFI\\%s found\n", EfiDirEntry->FileName);
            
            SPrint(FileName, 255, L"EFI\\%s", EfiDirEntry->FileName);
            loader_scan_dir(RootDir, FileName, DeviceHandle, VolName, VolBadgeImage);
        }
        Status = DirIterClose(&EfiDirIter);
        if (Status != EFI_NOT_FOUND)
            CheckError(Status, L"while scanning the EFI directory");
        
        RootDir->Close(RootDir);
        FreePool(VolName);
    }
    
    FreePool(Handles);
}


static void start_tool(IN LOADER_ENTRY *Entry)
{
    EFI_STATUS              Status;
    CHAR16                  ScreenTitle[256];
    EFI_HANDLE              ChildImageHandle;
    CHAR16                  ErrorInfo[256];
    
    SPrint(ScreenTitle, 255, L"rEFIt - %s", Entry->me.Title + 6);
    BeginExternalScreen(Entry->UseGraphicsMode ? 1 : 0, ScreenTitle);
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, Entry->DevicePath, NULL, 0, &ChildImageHandle);
    SPrint(ErrorInfo, 255, L"while loading %s", Entry->LoaderPath);
    if (CheckError(Status, ErrorInfo))
        goto bailout;
    
    // turn control over to the image
    BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    
bailout:
    FinishExternalScreen();
}

static void add_tool_entry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, REFIT_IMAGE *Image, BOOLEAN UseGraphicsMode)
{
    LOADER_ENTRY *Entry;
    
    Entry = AllocatePool(sizeof(LOADER_ENTRY));
    
    Entry->me.Title = PoolPrint(L"Start %s", LoaderTitle);
    Entry->me.Tag = TAG_TOOL;
    Entry->me.Row = 1;
    Entry->me.Image = Image;
    Entry->me.BadgeImage = NULL;
    Entry->LoaderPath = StrDuplicate(LoaderPath);
    Entry->VolName = NULL;
    Entry->DevicePath = FileDevicePath(SelfLoadedImage->DeviceHandle, Entry->LoaderPath);
    Entry->UseGraphicsMode = UseGraphicsMode;
    
    AddMenuEntry(&main_menu, (REFIT_MENU_ENTRY *)Entry);
}

static void free_tool_entry(IN LOADER_ENTRY *Entry)
{
    FreePool(Entry->me.Title);
    FreePool(Entry->LoaderPath);
    FreePool(Entry->DevicePath);
}

static void tool_scan(void)
{
    //EFI_STATUS              Status;
    CHAR16                  FileName[256];
    
    Print(L"Scanning for tools...\n");
    
    // look for the EFI shell
    SPrint(FileName, 255, L"%s\\apps\\shell.efi", SelfDirPath);
    if (FileExists(SelfRootDir, FileName)) {
        add_tool_entry(FileName, L"EFI Shell", BuiltinIcon(7), FALSE);
    }
}


EFI_STATUS
EFIAPI
RefitMain (IN EFI_HANDLE           ImageHandle,
           IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS Status;
    BOOLEAN mainLoopRunning = TRUE;
    REFIT_MENU_ENTRY *chosenEntry;
    UINTN MenuExit;
    UINTN i;
    
    InitializeLib(ImageHandle, SystemTable);
    Status = InitRefitLib(ImageHandle);
    if (EFI_ERROR(Status))
        return Status;
    InitScreen();
    
    BS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);   // disable EFI watchdog timer
    
    // scan for loaders and tools, add them to the menu
    loader_scan();
    tool_scan();
    
    // fixed other menu entries
    entry_about.Image = BuiltinIcon(4);
    AddMenuEntry(&main_menu, &entry_about);
    entry_exit.Image = BuiltinIcon(5);
    AddMenuEntry(&main_menu, &entry_exit);
    entry_reset.Image = BuiltinIcon(6);
    AddMenuEntry(&main_menu, &entry_reset);
    
    // wait for user ACK when there were errors
    FinishTextScreen(FALSE);
    
    while (mainLoopRunning) {
        MenuExit = RunMainMenu(&main_menu, &chosenEntry);
        
        if (MenuExit == MENU_EXIT_ESCAPE || chosenEntry->Tag == TAG_EXIT)
            break;
        
        switch (chosenEntry->Tag) {
            
            case TAG_RESET:    // Reboot
                TerminateScreen();
                RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
                mainLoopRunning = FALSE;   // just in case we get this far
                break;
                
            case TAG_ABOUT:    // About rEFIt
                about_refit();
                break;
                
            case TAG_LOADER:   // Boot OS via .EFI loader
                start_loader((LOADER_ENTRY *)chosenEntry);
                break;
                
            case TAG_TOOL:     // Start a EFI tool
                start_tool((LOADER_ENTRY *)chosenEntry);
                break;
                
        }
        
        main_menu.TimeoutSeconds = 0;   // no timeout on the second run
    }
    
    for (i = 0; i < main_menu.EntryCount; i++) {
        if (main_menu.Entries[i]->Tag == TAG_LOADER) {
            free_loader_entry((LOADER_ENTRY *)(main_menu.Entries[i]));
            FreePool(main_menu.Entries[i]);
        } else if (main_menu.Entries[i]->Tag == TAG_TOOL) {
            free_tool_entry((LOADER_ENTRY *)(main_menu.Entries[i]));
            FreePool(main_menu.Entries[i]);
        }
    }
    FreePool(main_menu.Entries);
    
    // clear screen completely
    TerminateScreen();
    return EFI_SUCCESS;
}
