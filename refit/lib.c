/*
 * rEFIt
 *
 * lib.c
 */

#include "lib.h"

EFI_STATUS lib_locate_handle(IN EFI_GUID *Protocol, OUT UINTN *HandleCount, OUT EFI_HANDLE **HandleArray)
{
    EFI_STATUS status;
    UINTN bufferSize = 16 * sizeof(EFI_HANDLE);
    EFI_HANDLE *buffer;
    
    buffer = AllocatePool(bufferSize);
    status = BS->LocateHandle(ByProtocol, Protocol, NULL, &bufferSize, buffer);
    if (status == EFI_BUFFER_TOO_SMALL) {
        FreePool(buffer);
        buffer = AllocatePool(bufferSize);  /* was changed by the call to an appropriate size */
        status = BS->LocateHandle(ByProtocol, Protocol, NULL, &bufferSize, buffer);
    }
    
    if (status == EFI_SUCCESS) {
        *HandleCount = bufferSize / sizeof(EFI_HANDLE);
        *HandleArray = buffer;
    } else if (status == EFI_NOT_FOUND) {
        *HandleCount = 0;
        *HandleArray = buffer;
        status = EFI_SUCCESS;
    } else {
        FreePool(buffer);
    }
    return status;
}

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode)
{
    EFI_STATUS Status;
    VOID *Buffer;
    UINTN InitialBufferSize, BufferSize;
    
    for (;;) {
        
        // free pointer from last call
        if (*DirEntry != NULL) {
            FreePool(*DirEntry);
            *DirEntry = NULL;
        }
        
        // read next directory entry
        BufferSize = InitialBufferSize = 256;
        Buffer = AllocatePool(BufferSize);
        Status = Directory->Read(Directory, &BufferSize, Buffer);
        if (Status == EFI_BUFFER_TOO_SMALL) {
            Buffer = ReallocatePool(Buffer, InitialBufferSize, BufferSize);
            Status = Directory->Read(Directory, &BufferSize, Buffer);
        }
        if (EFI_ERROR(Status)) {
            FreePool(Buffer);
            break;
        }
        
        // check for end of listing
        if (BufferSize == 0) {    // end of directory listing
            FreePool(Buffer);
            break;
        }
        
        // entry is ready to be returned
        *DirEntry = (EFI_FILE_INFO *)Buffer;
        
        // filter results
        if (FilterMode == 1) {   // only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY))
                break;
        } else                   // no filter or unknown filter -> return everything
            break;
        
    }
    return Status;
}
