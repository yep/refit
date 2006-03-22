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

// images

#ifdef TEXTONLY

DUMMY_IMAGE(image_tool_about)
DUMMY_IMAGE(image_tool_exit)
DUMMY_IMAGE(image_tool_reset)
DUMMY_IMAGE(image_tool_shell)

DUMMY_IMAGE(image_os_mac)
DUMMY_IMAGE(image_os_linux)
DUMMY_IMAGE(image_os_win)
DUMMY_IMAGE(image_os_unknown)

#else

#include "image_tool_about.h"
#include "image_tool_exit.h"
#include "image_tool_reset.h"
#include "image_tool_shell.h"

#include "image_os_mac.h"
#include "image_os_linux.h"
#include "image_os_win.h"
#include "image_os_unknown.h"
#endif

// variables

static EFI_HANDLE SelfImageHandle;
static EFI_LOADED_IMAGE *SelfLoadedImage;

static REFIT_MENU_ENTRY entry_exit    = { L"Exit to built-in Boot Manager", 1, NULL, 1, &image_tool_exit };
static REFIT_MENU_ENTRY entry_reset   = { L"Restart Computer", 2, NULL, 1, &image_tool_reset };
static REFIT_MENU_ENTRY entry_shell   = { L"Start EFI Shell", 3, NULL, 1, &image_tool_shell };
static REFIT_MENU_ENTRY entry_about   = { L"About rEFIt", 4, NULL, 1, &image_tool_about };

static REFIT_MENU_SCREEN main_menu    = { L"rEFIt - Main Menu", 0, 0, NULL, 20, L"Automatic boot" };


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
    BS->StartImage(ShellHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    
bailout:
    FinishExternalScreen();
}

void start_shell(void)
{
    BeginExternalScreen(0, L"rEFIt - EFI Shell");
    run_tool(L"\\apps\\shell.efi");
}

void about_refit(void)
{
    BeginTextScreen(L"rEFIt - About");
    Print(L"rEFIt Version 0.4\n\n");
    Print(L"Copyright (c) 2006 Christoph Pfisterer\n");
    Print(L"Portions Copyright (c) Intel Corporation and others\n");
    FinishTextScreen(TRUE);
}

void chainload(IN UINTN Tag, IN VOID *UserData)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_HANDLE              ChildImageHandle;
    
    BeginExternalScreen((Tag == 8) ? 0 : 1, L"rEFIt - Booting OS");
    
    DevicePath = (EFI_DEVICE_PATH *)UserData;
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ChildImageHandle);
    if (CheckError(Status, L"while loading the OS boot loader"))
        goto bailout;
    
    // turn control over to the image
    // TODO: re-enable the EFI watchdog timer!
    BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    
bailout:
    FinishExternalScreen();
}

void scan_dir(IN EFI_FILE *RootDir, IN CHAR16 *Path, IN EFI_HANDLE DeviceHandle, IN CHAR16 *VolName)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  FileName[256];
    REFIT_MENU_ENTRY        entry_boot = { NULL, 8, NULL, 0, &image_os_unknown };
    
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
        entry_boot.Title = PoolPrint(L"Boot %s from %s", FileName+1, VolName);
        entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
        entry_boot.Tag = 8;
        entry_boot.Image = &image_os_unknown;
        if (StriCmp(DirEntry->FileName, L"e.efi") == 0 ||
	    StriCmp(DirEntry->FileName, L"elilo.efi") == 0)
            entry_boot.Image = &image_os_linux;
        else if (StriCmp(DirEntry->FileName, L"Bootmgfw.efi") == 0)
            entry_boot.Image = &image_os_win;
        else if (StriCmp(DirEntry->FileName, L"xom.efi") == 0) {
            entry_boot.Tag = 9;
            entry_boot.Image = &image_os_win;
        }
        MenuAddEntry(&main_menu, &entry_boot);
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
    REFIT_DIR_ITER          EfiDirIter;
    EFI_FILE_INFO           *EfiDirEntry;
    CHAR16                  FileName[256];
    REFIT_MENU_ENTRY        entry_boot = { NULL, 8, NULL, 0, &image_os_unknown };
    
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
        if (FileExists(RootDir, FileName)) {
            Print(L"  - Mac OS X boot file found\n");
            
            entry_boot.Title = PoolPrint(L"Boot Mac OS X from %s", VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            entry_boot.Tag = 9;
            entry_boot.Image = &image_os_mac;
            MenuAddEntry(&main_menu, &entry_boot);
        }
        
        // check for XOM
        StrCpy(FileName, L"\\System\\Library\\CoreServices\\xom.efi");
        if (FileExists(RootDir, FileName)) {
            entry_boot.Title = PoolPrint(L"Boot Windows XP (XoM) from %s", VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            entry_boot.Tag = 9;
            entry_boot.Image = &image_os_win;
            MenuAddEntry(&main_menu, &entry_boot);
        }
        
        // check for Microsoft boot loader/menu
        StrCpy(FileName, L"\\EFI\\Microsoft\\Boot\\Bootmgfw.efi");
        if (FileExists(RootDir, FileName)) {
            Print(L"  - Microsoft boot menu found\n");
            
            entry_boot.Title = PoolPrint(L"Boot Microsoft boot menu from %s", VolName);
            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
            entry_boot.Tag = 8;
            entry_boot.Image = &image_os_win;
            MenuAddEntry(&main_menu, &entry_boot);
        }
        
        // scan the root directory for EFI executables
        scan_dir(RootDir, NULL, DeviceHandle, VolName);
        // scan the elilo directory (as used on gimli's first Live CD)
        scan_dir(RootDir, L"elilo", DeviceHandle, VolName);
        // scan the boot directory
        scan_dir(RootDir, L"boot", DeviceHandle, VolName);
        
        // scan subdirectories of the EFI directory (as per the standard)
        DirIterOpen(RootDir, L"EFI", &EfiDirIter);
        while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (StriCmp(EfiDirEntry->FileName, L"TOOLS") == 0)
                continue;   // skip this, doesn't contain boot loaders
            if (StriCmp(EfiDirEntry->FileName, L"REFIT") == 0 || StriCmp(EfiDirEntry->FileName, L"REFITL") == 0)
                continue;   // skip ourselves
            Print(L"  - Directory EFI\\%s found\n", EfiDirEntry->FileName);
            
            SPrint(FileName, 255, L"EFI\\%s", EfiDirEntry->FileName);
            scan_dir(RootDir, FileName, DeviceHandle, VolName);
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
    UINTN MenuExit;
    
    InitializeLib(ImageHandle, SystemTable);
    BS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);   // disable EFI watchdog timer
    InitScreen();
    
    SelfImageHandle = ImageHandle;
    Status = BS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID*)&SelfLoadedImage);
    if (CheckFatalError(Status, L"while getting a LoadedImageProtocol handle"))
        return EFI_LOAD_ERROR;
    
    scan_volumes();
    FinishTextScreen(FALSE);   // wait for user ACK when there were errors
    
    MenuAddEntry(&main_menu, &entry_shell);
    MenuAddEntry(&main_menu, &entry_about);
    MenuAddEntry(&main_menu, &entry_exit);
    MenuAddEntry(&main_menu, &entry_reset);
    
    while (mainLoopRunning) {
        MenuExit = MenuRun(1, &main_menu, &chosenEntry);
        
        if (MenuExit == MENU_EXIT_ESCAPE || chosenEntry->Tag == 1)
            break;
        
        switch (chosenEntry->Tag) {
            
            case 2:   // Reboot
                TerminateScreen();
                RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
                mainLoopRunning = FALSE;
                break;
                
            case 3:   // Start Shell
                start_shell();
                break;
                
            case 4:   // About rEFIt
                about_refit();
                break;
                
            case 8:   // Boot OS via .EFI loader, text mode
            case 9:   // Boot OS via .EFI loader, graphics mode
                chainload(chosenEntry->Tag, chosenEntry->UserData);
                break;
                
        }
        
        main_menu.TimeoutSeconds = 0;
    }
    
    // clear screen completely
    TerminateScreen();
    return EFI_SUCCESS;
}
