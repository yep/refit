/*
 * rEFIt
 *
 * lib.h
 */

#include "efi.h"
#include "efilib.h"

#include "ConsoleControl.h"

/* menu structures */

typedef struct {
    CHAR16 *Title;
    UINTN Tag;
    VOID *UserData;
} REFIT_MENU_ENTRY;

typedef struct {
    CHAR16 *Title;
    UINTN EntryCount;
    UINTN AllocatedEntryCount;
    REFIT_MENU_ENTRY *Entries;
} REFIT_MENU_SCREEN;

/* lib functions */

EFI_STATUS lib_locate_handle(IN EFI_GUID *Protocol, OUT UINTN *HandleCount, OUT EFI_HANDLE **HandleArray);

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode);

/* menu functions */

void menu_init(void);
void menu_reinit(void);
void menu_term(IN UINTN state);

void menu_add_entry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry);
void menu_free(IN REFIT_MENU_SCREEN *Screen);

void menu_clear_screen(IN CHAR16 *Title);
void menu_start_screen(IN CHAR16 *Title);
void menu_run(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry);

void menu_print(IN CHAR16 *String);
void menu_waitforkey(void);

/* EOF */
