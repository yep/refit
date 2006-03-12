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

static EFI_HANDLE SelfImageHandle;
static EFI_LOADED_IMAGE *SelfLoadedImage;

static REFIT_MENU_ENTRY entry_exit    = { L"Exit to built-in Boot Manager", 1, NULL };
static REFIT_MENU_ENTRY entry_reset   = { L"Restart Computer", 2, NULL };
static REFIT_MENU_ENTRY entry_shell   = { L"Start EFI Shell", 3, NULL };
static REFIT_MENU_ENTRY entry_about   = { L"About rEFIt", 4, NULL };

static REFIT_MENU_SCREEN main_menu    = { L"rEFIt - Main Menu", 0, 0, NULL };


void run_tool(IN CHAR16 *RelativeFilePath)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    CHAR16                  *DevicePathAsString;
    CHAR16                  FileName[256];
    CHAR16                  ErrorInfo[256];
    UINTN                   i;
    EFI_HANDLE              ShellHandle;
    
    // find the current directory
    DevicePathAsString = DevicePathToStr(SelfLoadedImage->FilePath);
    if (DevicePathAsString!=NULL) {
        StrCpy(FileName,DevicePathAsString);
        FreePool(DevicePathAsString);
    }
    for (i = StrLen(FileName) - 1; i > 0 && FileName[i] != '\\'; i--) ;
    FileName[i] = 0;
    
    // append relative path to get the absolute path for the image file
    StrCat(FileName, RelativeFilePath);
    
    // make a full device path for the image file
    DevicePath = FileDevicePath(SelfLoadedImage->DeviceHandle, FileName);
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ShellHandle);
    FreePool(DevicePath);
    SPrint(ErrorInfo, 255, L"while loading %s", FileName);
    if (CheckError(Status, ErrorInfo))
        goto bailout;
    
    // turn control over to the image
    ScreenLeave(0);
    BS->StartImage(ShellHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    ScreenReinit();
    
bailout:
    WaitAfterError();
}

void start_shell(void)
{
    ScreenHeader(L"rEFIt - EFI Shell");
    run_tool(L"\\apps\\shell.efi");
}

void about_refit(void)
{
    ScreenHeader(L"rEFIt - About");
    Print(L"rEFIt Version 0.2\n\n");
    Print(L"Copyright (c) 2006 Christoph Pfisterer\n");
    Print(L"Portions Copyright (c) Intel Corporation and others\n\n");
    ScreenWaitForKey();
}

void chainload(VOID *UserData)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_HANDLE              ChildImageHandle;
    
    ScreenHeader(L"rEFIt - Booting OS");
    
    DevicePath = (EFI_DEVICE_PATH *)UserData;
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ChildImageHandle);
    if (CheckError(Status, L"while loading the OS boot loader"))
        goto bailout;
    
    // turn control over to the image
    ScreenLeave(0);
    // TODO: re-enable the EFI watchdog timer!
    BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    ScreenReinit();
    
bailout:
    WaitAfterError();
}

void scan_volumes(void)
{
    EFI_STATUS              Status;
    UINTN                   HandleCount = 0;
    UINTN                   HandleIndex;
    EFI_HANDLE              *Handles;
    EFI_HANDLE              DeviceHandle;
    EFI_FILE                *RootDir;
    EFI_FILE_SYSTEM_INFO    *FileSystemInfoPtr;
    CHAR16                  *VolName;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    REFIT_DIR_ITER          EfiDirIter;
    EFI_FILE_INFO           *EfiDirEntry;
    EFI_FILE                *BootFile;
    CHAR16                  FileName[256];
    REFIT_MENU_ENTRY        entry_boot = { NULL, 8, NULL };
    
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
        
        // check for Mac OS X boot loader
        StrCpy(FileName, L"\\System\\Library\\CoreServices\\boot.efi");
        if (RootDir->Open(RootDir, &BootFile, FileName, EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
            Print(L"  - Mac OS X boot file found\n");
            BootFile->Close(BootFile);
            
            entry_boot.Title = PoolPrint(L"Boot Mac OS X from %s", VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            MenuAddEntry(&main_menu, &entry_boot);
        }
        
        // check for Microsoft boot loader/menu
        StrCpy(FileName, L"\\EFI\\Microsoft\\Boot\\Bootmgfw.efi");
        if (RootDir->Open(RootDir, &BootFile, FileName, EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
            Print(L"  - Microsoft boot menu found\n");
            BootFile->Close(BootFile);
            
            entry_boot.Title = PoolPrint(L"Boot Microsoft boot menu from %s", VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            MenuAddEntry(&main_menu, &entry_boot);
        }
        
        // scan the root directory for EFI executables
        DirIterOpen(RootDir, NULL, &DirIter);
        while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
            if (StriCmp(DirEntry->FileName, L"TextMode.efi") == 0)
                continue;   // skip this
            if (StriCmp(DirEntry->FileName, L"GraphicsConsole.efi") == 0)
                continue;   // skip this
            
            SPrint(FileName, 255, L"\\%s", DirEntry->FileName);
            entry_boot.Title = PoolPrint(L"Boot %s from %s", FileName+1, VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            MenuAddEntry(&main_menu, &entry_boot);
        }
        Status = DirIterClose(&DirIter);
        CheckError(Status, L"while scanning the root directory");
        
        // scan the elilo directory (as used on gimli's Live CD)
        DirIterOpen(RootDir, L"elilo", &DirIter);
        while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
            if (StriCmp(DirEntry->FileName, L"TextMode.efi") == 0)
                continue;   // skip this
            if (StriCmp(DirEntry->FileName, L"GraphicsConsole.efi") == 0)
                continue;   // skip this
            
            SPrint(FileName, 255, L"\\elilo\\%s", DirEntry->FileName);
            entry_boot.Title = PoolPrint(L"Boot %s from %s", FileName+1, VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            MenuAddEntry(&main_menu, &entry_boot);
        }
        Status = DirIterClose(&DirIter);
        if (Status != EFI_NOT_FOUND)
            CheckError(Status, L"while scanning the elilo directory");
        
        // scan subdirectories of the EFI directory (as per the standard)
        DirIterOpen(RootDir, L"EFI", &EfiDirIter);
        while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (StriCmp(EfiDirEntry->FileName, L"TOOLS") == 0)
                continue;   // skip this, doesn't contain boot loaders
            if (StriCmp(EfiDirEntry->FileName, L"REFIT") == 0)
                continue;   // skip ourselves
            Print(L"  - Directory EFI\\%s found\n", EfiDirEntry->FileName);
            
            // look through contents of that directory
            DirIterOpen(EfiDirIter.DirHandle, EfiDirEntry->FileName, &DirIter);
            while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
	        if (StriCmp(DirEntry->FileName, L"TextMode.efi") == 0)
                    continue;   // skip this
                if (StriCmp(DirEntry->FileName, L"GraphicsConsole.efi") == 0)
                    continue;   // skip this
                
                SPrint(FileName, 255, L"\\EFI\\%s\\%s", EfiDirEntry->FileName, DirEntry->FileName);
                entry_boot.Title = PoolPrint(L"Boot %s from %s", FileName+5, VolName);
                entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
                MenuAddEntry(&main_menu, &entry_boot);
            }
            Status = DirIterClose(&DirIter);
            CheckError(Status, L"while scanning an EFI sub-directory");
        }
        Status = DirIterClose(&EfiDirIter);
        if (Status != EFI_NOT_FOUND)
            CheckError(Status, L"while scanning the EFI directory");
        
        RootDir->Close(RootDir);
        FreePool(VolName);
    }
    
    FreePool(Handles);
}


EFI_STATUS
EFIAPI
RefitMain (IN EFI_HANDLE           ImageHandle,
           IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS Status;
    REFIT_MENU_ENTRY *chosenEntry;
    BOOLEAN mainLoopRunning = TRUE;
    
    InitializeLib(ImageHandle, SystemTable);
    BS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);   // disable EFI watchdog timer
    ScreenInit();
    ScreenHeader(L"rEFIt - Initializing...");
    
    SelfImageHandle = ImageHandle;
    Status = BS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID*)&SelfLoadedImage);
    if (CheckFatalError(Status, L"while getting a LoadedImageProtocol handle"))
        return EFI_LOAD_ERROR;
    
    scan_volumes();
    WaitAfterError();
    
    MenuAddEntry(&main_menu, &entry_shell);
    MenuAddEntry(&main_menu, &entry_about);
    MenuAddEntry(&main_menu, &entry_exit);
    MenuAddEntry(&main_menu, &entry_reset);
    
    while (mainLoopRunning) {
        MenuRun(&main_menu, &chosenEntry);
        
        if (chosenEntry == NULL || chosenEntry->Tag == 1)
            break;
        
        switch (chosenEntry->Tag) {
            
            case 2:   // Reboot
                ScreenLeave(1);
                RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
                mainLoopRunning = FALSE;
                break;
                
            case 3:   // Start Shell
                start_shell();
                break;
                
            case 4:   // About rEFIt
                about_refit();
                break;
                
            case 8:   // Boot OS via .EFI loader
                chainload(chosenEntry->UserData);
                break;
                
        }
    }
    
    // clear screen completely
    ScreenLeave(1);
    return EFI_SUCCESS;
}
