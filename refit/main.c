/*
 * rEFIt
 *
 * main.c
 */

#include "lib.h"

static EFI_HANDLE SelfImageHandle;
static EFI_LOADED_IMAGE *SelfLoadedImage;

static REFIT_MENU_ENTRY entry_shell   = { L"Start EFI Shell", 3, NULL };
static REFIT_MENU_ENTRY entry_volumes = { L"List Volumes", 5, NULL };
static REFIT_MENU_ENTRY entry_exit    = { L"Exit to EFI Boot Manager Menu", 1, NULL };
static REFIT_MENU_ENTRY entry_reset   = { L"Restart Computer", 2, NULL };

static REFIT_MENU_SCREEN main_menu    = { L"rEFIt - Main Menu", 0, 0, NULL };


void run_tool(IN CHAR16 *RelativeFilePath)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    CHAR16                  *DevicePathAsString;
    CHAR16                  FileName[256];
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
    if (EFI_ERROR(Status)) {
        Print(L"Can not load the file %s\n", FileName);
        goto bailout;
    }
    
    // turn control over to the image
    menu_term(0);
    BS->StartImage(ShellHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    menu_reinit();
    return;
    
bailout:
    menu_waitforkey();
}



#define BUFFERSIZE 4096
char buffer[BUFFERSIZE];
UINTN bufferSize;

void list_devices(void)
{
    EFI_STATUS Status;
    UINTN handleCount, handleIndex;
    EFI_HANDLE *handles;
    EFI_FILE_IO_INTERFACE *fileSystem;
    
    EFI_FILE *rootDir;
    EFI_FILE_SYSTEM_INFO *fileSystemInfo;
    EFI_FILE *efiDir, /* *testDir,*/ *testFile;
    EFI_FILE_INFO *dirEntry;
    
    menu_clear_screen(L"rEFIt - Volume Listing");
    
    // SIMPLE_FILE_SYSTEM_PROTOCOL
    Status = lib_locate_handle(&FileSystemProtocol, &handleCount, &handles);
    if (EFI_ERROR(Status)) {
        menu_print(L"An error occured.");
        goto bailout;
    }
    
    for (handleIndex = 0; handleIndex < handleCount; handleIndex++) {
        menu_print(L"Looking at a handle\r\n");
        if (BS->HandleProtocol(handles[handleIndex], &FileSystemProtocol, &fileSystem) != EFI_SUCCESS) {
            menu_print(L"  not a SimpleFileSystem\r\n");
            continue;
        }
        
        if (fileSystem->OpenVolume(fileSystem, &rootDir) == EFI_SUCCESS) {
            bufferSize = BUFFERSIZE;
            if (rootDir->GetInfo(rootDir, &FileSystemInfo, &bufferSize, buffer) == EFI_SUCCESS) {
                fileSystemInfo = (EFI_FILE_SYSTEM_INFO *)buffer;
                menu_print(L"Volume found: ");
                menu_print(fileSystemInfo->VolumeLabel);
                menu_print(L"\r\n");
            } else {
                menu_print(L"  GetInfo failed\r\n");
            }
            
            if (rootDir->Open(rootDir, &testFile, L"\\System\\Library\\CoreServices\\boot.efi", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                menu_print(L"  Mac OS X boot file found\r\n");
                testFile->Close(testFile);
            }
            
            if (rootDir->Open(rootDir, &efiDir, L"EFI", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                menu_print(L"  looking at EFI directory\r\n");
                for(;;) {
                    bufferSize = BUFFERSIZE;
                    if (efiDir->Read(efiDir, &bufferSize, buffer) != EFI_SUCCESS) {
                        menu_print(L"   ...error during dir scan\r\n");
                        break;
                    }
                    if (bufferSize == 0)   // end of listing
                        break;
                    dirEntry = (EFI_FILE_INFO *)buffer;
                    if ((dirEntry->Attribute & EFI_FILE_DIRECTORY)) {
                        menu_print(L"  found directory ");
                        menu_print(dirEntry->FileName);
                        menu_print(L"\r\n");
                    }
                }
                
                efiDir->Close(efiDir);
            }
            
            rootDir->Close(rootDir);
            
        } else {
            menu_print(L"  OpenVolume failed\r\n");
        }
    }
    
    FreePool(handles);
bailout:
    menu_waitforkey();
}

void start_shell(void)
{
    menu_clear_screen(L"rEFIt - EFI Shell");
    run_tool(L"\\apps\\shell.efi");
}

void chainload(VOID *UserData)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_HANDLE              ChildImageHandle;
    
    menu_clear_screen(L"rEFIt - Booting OS");
    
    DevicePath = (EFI_DEVICE_PATH *)UserData;
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ChildImageHandle);
    if (EFI_ERROR(Status)) {
        Print(L"Can not load the boot loader image\n");
        goto bailout;
    }
    
    // turn control over to the image
    menu_term(0);
    BS->StartImage(ChildImageHandle, NULL, NULL);
    // control returns here when the child image calls Exit()
    menu_reinit();
    return;
    
bailout:
    menu_waitforkey();
}

void scan_volumes(void)
{
    EFI_STATUS              Status;
    UINTN                   handleCount, handleIndex;
    EFI_HANDLE              *handles;
    EFI_HANDLE              DeviceHandle;
    EFI_FILE_IO_INTERFACE   *FileSystem;
    EFI_FILE                *RootDir;
    EFI_FILE_SYSTEM_INFO    *FileSystemInfoPtr;
    CHAR16                  *VolName;
    EFI_FILE                *EfiDir;
    EFI_FILE_INFO           *DirEntry;
    EFI_FILE                *SubDir;
    EFI_FILE_INFO           *SubDirEntry;
    EFI_FILE                *BootFile;
    CHAR16                  FileName[256];
    REFIT_MENU_ENTRY        entry_boot = { NULL, 8, NULL };
    
    Print(L"Scanning for boot loaders...\n");
    
    // get all filesystem handles
    Status = lib_locate_handle(&FileSystemProtocol, &handleCount, &handles);
    if (EFI_ERROR(Status)) {
        Print(L"Can't list file system handles.\n");
        return;
    }
    // iterate over the filesystem handles
    for (handleIndex = 0; handleIndex < handleCount; handleIndex++) {
        DeviceHandle = handles[handleIndex];
        
        // get the file system protocol
        Status = BS->HandleProtocol(DeviceHandle, &FileSystemProtocol, &FileSystem);
        if (EFI_ERROR(Status)) {
            Print(L"Can't get file system protocol for handle.\n");
            continue;
        }
        
        // open the file system (results in dir handle for the root dir)
        Status = FileSystem->OpenVolume(FileSystem, &RootDir);
        if (EFI_ERROR(Status)) {
            Print(L"Can't open volume.\n");
            continue;
        }
        
        // get volume name
        bufferSize = BUFFERSIZE;
        if (RootDir->GetInfo(RootDir, &FileSystemInfo, &bufferSize, buffer) == EFI_SUCCESS) {
            FileSystemInfoPtr = (EFI_FILE_SYSTEM_INFO *)buffer;
            Print(L"  Volume %s\n", FileSystemInfoPtr->VolumeLabel);
            VolName = StrDuplicate(FileSystemInfoPtr->VolumeLabel);
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
            menu_add_entry(&main_menu, &entry_boot);
        }
        
        // check for other boot loaders (EFI directory)
        Status = RootDir->Open(RootDir, &EfiDir, L"EFI", EFI_FILE_MODE_READ, 0);
        if (Status == EFI_SUCCESS) {
            DirEntry = NULL;
            for (;;) {
                Status = DirNextEntry(EfiDir, &DirEntry, 1);
                if (EFI_ERROR(Status)) {
                    Print(L"   ...error during dir scan\n");
                    break;
                }
                if (DirEntry == NULL)  // end of listing
                    break;
                
                if (StriCmp(DirEntry->FileName, L"TOOLS") == 0)
                    continue;   // skip this, doesn't contain boot loaders
                if (StriCmp(DirEntry->FileName, L"REFIT") == 0)
                    continue;   // skip ourselves
                Print(L"  - Directory EFI\\%s found\n", DirEntry->FileName);
                
                // look through contents of that directory
                Status = EfiDir->Open(EfiDir, &SubDir, DirEntry->FileName, EFI_FILE_MODE_READ, 0);
                if (Status == EFI_SUCCESS) {
                    SubDirEntry = NULL;
                    for (;;) {
                        Status = DirNextEntry(SubDir, &SubDirEntry, 0);
                        if (EFI_ERROR(Status)) {
                            Print(L"   ...error during dir scan\n");
                            break;
                        }
                        if (SubDirEntry == NULL)  // end of listing
                            break;
                        if (StriCmp(SubDirEntry->FileName + (StrLen(SubDirEntry->FileName) - 4), L".EFI") == 0) {
                            SPrint(FileName, 255, L"\\EFI\\%s\\%s", DirEntry->FileName, SubDirEntry->FileName);
                            entry_boot.Title = PoolPrint(L"Boot %s from %s", FileName+5, VolName);
                            entry_boot.UserData = FileDevicePath(DeviceHandle, FileName);
                            menu_add_entry(&main_menu, &entry_boot);
                        }
                        
                    }
                    SubDir->Close(SubDir);
                }
            }
            
            EfiDir->Close(EfiDir);
        }
        
        RootDir->Close(RootDir);
        if (VolName != NULL)
            FreePool(VolName);
    }
    
    FreePool(handles);
}


/*    
    
    EFI_STATUS              Status;
    EFI_LOADED_IMAGE        *LoadedImage;
    EFI_DEVICE_PATH         *DevicePath;
    CHAR16                  *DevicePathAsString;
    EFI_FILE_IO_INTERFACE   *Vol;
    EFI_FILE_HANDLE         RootFs;
    EFI_FILE_HANDLE         CurDir;
    //EFI_FILE_HANDLE         FileHandle;
    CHAR16                  FileName[100];
    UINTN                   i;
    EFI_HANDLE              ShellHandle;
    
    menu_clear_screen(L"rEFIt - TianoCore EFI Shell");
    
    
    //
    // Get the device handle and file path to the EFI OS Loader itself.
    //
    
    Status = BS->HandleProtocol (ImageHandle, 
                                 &LoadedImageProtocol, 
                                 (VOID*)&LoadedImage
                                 );
    
    if (EFI_ERROR(Status)) {
        Print(L"Can not retrieve a LoadedImageProtocol handle for ImageHandle\n");
        goto bailout;
    }
    
    Status = BS->HandleProtocol (LoadedImage->DeviceHandle, 
                                 &DevicePathProtocol, 
                                 (VOID*)&DevicePath
                                 );
    
    if (EFI_ERROR(Status) || DevicePath==NULL) {
        Print(L"Can not find a DevicePath handle for LoadedImage->DeviceHandle\n");
        goto bailout;
    }
    
    DevicePathAsString = DevicePathToStr(DevicePath);
    if (DevicePathAsString != NULL) {
        Print (L"Image device : %s\n", DevicePathAsString);
        FreePool(DevicePathAsString);
    }
    
    DevicePathAsString = DevicePathToStr(LoadedImage->FilePath);
    if (DevicePathAsString != NULL) {
        Print (L"Image file   : %s\n", DevicePathToStr (LoadedImage->FilePath));
        FreePool(DevicePathAsString);
    }
    
    Print (L"Image Base   : %X\n", LoadedImage->ImageBase);
    Print (L"Image Size   : %X\n", LoadedImage->ImageSize);
    
    //
    // Open the volume for the device where the EFI OS Loader was loaded from.
    //
    
    Status = BS->HandleProtocol (LoadedImage->DeviceHandle,
                                 &FileSystemProtocol,
                                 (VOID*)&Vol
                                 );
    
    if (EFI_ERROR(Status)) {
        Print(L"Can not get a FileSystem handle for LoadedImage->DeviceHandle\n");
        goto bailout;
    }
    
    Status = Vol->OpenVolume (Vol, &RootFs);
    
    if (EFI_ERROR(Status)) {
        Print(L"Can not open the volume for the file system\n");
        goto bailout;
    }
    
    CurDir = RootFs;
    
    //
    // Open the file OSKERNEL.BIN in the same path as the EFI OS Loader.
    //
    
    DevicePathAsString = DevicePathToStr(LoadedImage->FilePath);
    if (DevicePathAsString!=NULL) {
        StrCpy(FileName,DevicePathAsString);
        FreePool(DevicePathAsString);
    }
    for (i = StrLen(FileName); i > 0 && FileName[i-1] != '\\'; i--) ;
    FileName[i] = 0;
    StrCat(FileName, L"\\apps\\Shell.efi");
    
    
    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, FileName);
    DevicePathAsString = DevicePathToStr(DevicePath);
    if (DevicePathAsString != NULL) {
        Print (L"Shell file   : %s\n", DevicePathAsString);
        FreePool(DevicePathAsString);
    }
    
    if (BS->LoadImage(FALSE, ImageHandle, DevicePath, NULL, 0, &ShellHandle) == EFI_SUCCESS) {
        Print(L"Shell loaded, press any key to start it...\n");
        menu_waitforkey();
        BS->StartImage(ShellHandle, NULL, NULL);
    } else {
        Print(L"Failed to load.\n");
    }
    
    FreePool(DevicePath);
    
bailout:
    menu_waitforkey();
    */


EFI_STATUS
EFIAPI
RefitMain (IN EFI_HANDLE           ImageHandle,
           IN EFI_SYSTEM_TABLE     *SystemTable)
{
    REFIT_MENU_ENTRY *chosenEntry;
    BOOLEAN mainLoopRunning = TRUE;
    
    InitializeLib(ImageHandle, SystemTable);
    menu_init();
    menu_clear_screen(L"rEFIt - Initializing...");
    
    SelfImageHandle = ImageHandle;
    if (BS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID*)&SelfLoadedImage) != EFI_SUCCESS) {
        Print(L"Can not retrieve a LoadedImageProtocol handle for ImageHandle\n");
        return EFI_SUCCESS;  /* TODO: appropriate error code */
    }
    
    scan_volumes();
    
    menu_add_entry(&main_menu, &entry_shell);
    //menu_add_entry(&main_menu, &entry_volumes);
    menu_add_entry(&main_menu, &entry_exit);
    menu_add_entry(&main_menu, &entry_reset);
    
    while (mainLoopRunning) {
        menu_run(&main_menu, &chosenEntry);
        
        if (chosenEntry == NULL || chosenEntry->Tag == 1)
            break;
        
        switch (chosenEntry->Tag) {
            
            case 2:   // Reboot
                menu_term(1);
                RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
                mainLoopRunning = FALSE;
                break;
                
            case 3:   // Start Shell
                start_shell();
                break;
                
            case 5:   // List Devices
                list_devices();
                break;
                
            case 8:   // Boot OS via .EFI loader
                chainload(chosenEntry->UserData);
                break;
                
        }
    }
    
    //menu_clear_screen(L"rEFIt - Good Bye!");
    menu_term(1);
    return EFI_SUCCESS;
}
