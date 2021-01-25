#ifndef _ADD_BOOT_OPTION_H_
#define _ADD_BOOT_OPTION_H_
#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Guid/ConnectConInEvent.h>
#include <Guid/Performance.h>
//#include <Guid/StatusCodeDataTypeVariable.h>
#include <Guid/EventGroup.h>

#include <Protocol/Bds.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/VariableLock.h>
#include <Protocol/DeferredImageLoad.h>

#include <Library/UefiDriverEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/PerformanceLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>

#include <Library/UefiBootManagerLib.h>
#include <Library/PlatformBootManagerLib.h>
#include <Library/DxeServicesLib.h>
#include <Protocol/RamDisk.h>

#include <Protocol/SimpleFileSystem.h>



VOID
EFIAPI
LundyAddRamDisk(
  IN EFI_EVENT                Event,
  IN VOID                     *Context
  );

BOOLEAN
IsVirtualDiskNew (
  IN VOID                 *DevPath
  );
  
  
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
LundyAppendNew (
  IN EFI_DEVICE_PATH_PROTOCOL        *InDevicePath,  
  IN CONST CHAR16                    *FileName
  );

  
UINTN
RegisterRamDiskBootOption (
  EFI_DEVICE_PATH_PROTOCOL         *RamDp,
  CHAR16                           *Description,
  UINTN                            Position,
  BOOLEAN                          IsBootCategory
  );

#endif