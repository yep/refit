/*
 * rEFIt
 *
 * menu.c
 */

#include "lib.h"

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)

static UINTN screen_width;
static UINTN screen_height;

EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

void menu_init(void)
{
    EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
    EFI_CONSOLE_CONTROL_SCREEN_MODE currentMode;
    
    /* switch console to text mode if necessary */
    if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
        ConsoleControl->GetMode(ConsoleControl, &currentMode, NULL, NULL);
        if (currentMode == EfiConsoleControlScreenGraphics) {
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
        }
    }
    
    /* get size of display */
    if (ST->ConOut->QueryMode(ST->ConOut, ST->ConOut->Mode->Mode, &screen_width, &screen_height) != EFI_SUCCESS) {
        screen_width = 80;
        screen_height = 25;
    }
    
    /* disable cursor */
    ST->ConOut->EnableCursor(ST->ConOut, FALSE);
}

void menu_reinit(void)
{
    menu_init();
}

void menu_term(IN UINTN state)
{
    /* states:
        0 text, as is
        1 text, blank screen
        2 graphics */
    
    /* clear text screen if requested */
    if (state > 0) {
        ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
        ST->ConOut->ClearScreen(ST->ConOut);
    }

    /* enable cursor */
    ST->ConOut->EnableCursor(ST->ConOut, TRUE);
    
    /* switch to graphics if requested */
    if (state == 2) {
        EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
        if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenGraphics);
        }
    }
}

void menu_add_entry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry)
{
    REFIT_MENU_ENTRY *NewEntries;
    
    if (Screen->EntryCount >= Screen->AllocatedEntryCount) {
        Screen->AllocatedEntryCount += 8;
        NewEntries = AllocatePool(sizeof(REFIT_MENU_ENTRY) * Screen->AllocatedEntryCount);
        if (Screen->EntryCount > 0)
            CopyMem(NewEntries, Screen->Entries, sizeof(REFIT_MENU_ENTRY) * Screen->EntryCount);
        if (Screen->Entries)
            FreePool(Screen->Entries);
        Screen->Entries = NewEntries;
    }
    
    CopyMem(Screen->Entries + Screen->EntryCount, Entry, sizeof(REFIT_MENU_ENTRY));
    Screen->EntryCount++;
}

void menu_free(IN REFIT_MENU_SCREEN *Screen)
{
    if (Screen->Entries)
        FreePool(Screen->Entries);
}

void menu_clear_screen(IN CHAR16 *Title)
{
    UINTN x, y;
    
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->ClearScreen(ST->ConOut);
    
    /* banner background */
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BANNER);
    for (y = 0; y < 3; y++) {
        ST->ConOut->SetCursorPosition(ST->ConOut, 0, y);
        for (x = 0; x < screen_width; x++)
            ST->ConOut->OutputString(ST->ConOut, L" ");
    }
    
    menu_start_screen(Title);
}

void menu_start_screen(IN CHAR16 *Title)
{
    UINTN x;
    
    /* banner */
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BANNER);
    ST->ConOut->SetCursorPosition(ST->ConOut, 0, 1);
    for (x = 0; x < screen_width; x++)
        ST->ConOut->OutputString(ST->ConOut, L" ");
    ST->ConOut->SetCursorPosition(ST->ConOut, 3, 1);
    ST->ConOut->OutputString(ST->ConOut, Title);
    
    /* reposition cursor */
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
}

void menu_run(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN index;
    INTN i, chosenIndex, newIndex, maxIndex;
    EFI_STATUS status;
    EFI_INPUT_KEY key;
    BOOLEAN running;
    
    chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    running = TRUE;
    
    /* disable cursor */
    ST->ConOut->EnableCursor(ST->ConOut, FALSE);
    
    menu_clear_screen(Screen->Title);
    for (i = 0; i <= maxIndex; i++) {
        ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4+i);
        if (i == chosenIndex)
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
        else
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
        ST->ConOut->OutputString(ST->ConOut, L" ");
        ST->ConOut->OutputString(ST->ConOut, Screen->Entries[i].Title);
        ST->ConOut->OutputString(ST->ConOut, L" ");
    }
    
    while (running) {
        status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (status == EFI_NOT_READY) {
            BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
            continue;
        }
        
        newIndex = 0x3fff;
        switch (key.ScanCode) {
            case 0x01:  /* Arrow Up */
                newIndex = chosenIndex - 1;
                break;
            case 0x02:  /* Arrow Down */
                newIndex = chosenIndex + 1;
                break;
            case 0x05:  /* Home */
            case 0x09:  /* PageUp */
                newIndex = 0;
                break;
            case 0x06:  /* End */
            case 0x0a:  /* PageDown */
                newIndex = maxIndex;
                break;
            case 0x17:  /* Escape */
                /* exit menu screen without selection */
                if (ChosenEntry)
                    *ChosenEntry = NULL;
                return;
        }
        switch (key.UnicodeChar) {
            case 0x0a:
            case 0x0d:
            case 0x20:
                running = FALSE;
                break;
        }
        
        if (newIndex != 0x3fff) {
            if (newIndex < 0)
                newIndex = 0;
            if (newIndex > maxIndex)
                newIndex = maxIndex;
            if (newIndex != chosenIndex) {
                /* redraw selection */
                ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4+chosenIndex);
                ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
                ST->ConOut->OutputString(ST->ConOut, L" ");
                ST->ConOut->OutputString(ST->ConOut, Screen->Entries[chosenIndex].Title);
                ST->ConOut->OutputString(ST->ConOut, L" ");
                chosenIndex = newIndex;
                ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4+chosenIndex);
                ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
                ST->ConOut->OutputString(ST->ConOut, L" ");
                ST->ConOut->OutputString(ST->ConOut, Screen->Entries[chosenIndex].Title);
                ST->ConOut->OutputString(ST->ConOut, L" ");
            }
        }
    }
    
    if (ChosenEntry)
        *ChosenEntry = Screen->Entries + chosenIndex;
    return;
}

void menu_print(IN CHAR16 *String)
{
    ST->ConOut->OutputString(ST->ConOut, String);
}

void menu_waitforkey(void)
{
    UINTN index;
    EFI_INPUT_KEY key;
    
    menu_print(L"* Hit any key to continue *");
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
    menu_print(L"\r\n");
}
