#include "efi.h"
#include "ConsoleControl.h"

EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

EFI_STATUS
EFIAPI
TextModeMain (IN EFI_HANDLE           ImageHandle,
	      IN EFI_SYSTEM_TABLE     *SystemTable)
{
  EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
  int changed = 0;

  if (SystemTable->BootServices->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE currentMode;
    ConsoleControl->GetMode(ConsoleControl, &currentMode, NULL, NULL);
    if (currentMode == EfiConsoleControlScreenGraphics) {
      ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
      changed = 1;
    }
  }

  if (changed) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Welcome to text mode!\r\n\r\n");
  }
  return EFI_SUCCESS;
}
