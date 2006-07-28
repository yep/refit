/* fsw_base.h - change as required to configure the host environment */

#ifdef HOST_EFI
#include "fsw_efi_base.h"
#endif

#ifdef HOST_POSIX
#include "fsw_posix_base.h"
#endif
