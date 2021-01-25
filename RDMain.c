#include "RD.h"
//Add Start By Lundy
#include "AddBootOption.h"
#define EFI_VIRTUAL_DISK_FILE_NAME L"\\EFI\\OC\\OpenCore.efi"

#define NEW_BOOT_OPTION_NAME L"Scorpion"
//Add End By Lundy
extern unsigned char __data_img[];

extern unsigned int __data_img_len;
//
// Handle for the EFI_RAM_DISK_PROTOCOL instance
//
EFI_HANDLE  mRamDiskHandle = NULL;

//
// RamDiskDxe driver maintains a list of registered RAM disks.
//
LIST_ENTRY  RegisteredRamDisks;


RAM_DISK_PRIVATE_DATA mRamDiskPrivateDataTemplate = {
  RAM_DISK_PRIVATE_DATA_SIGNATURE,
  NULL
};

MEDIA_RAM_DISK_DEVICE_PATH  mRamDiskDeviceNodeTemplate = {
  {
    MEDIA_DEVICE_PATH,
    MEDIA_RAM_DISK_DP,
    {
      (UINT8) (sizeof (MEDIA_RAM_DISK_DEVICE_PATH)),
      (UINT8) ((sizeof (MEDIA_RAM_DISK_DEVICE_PATH)) >> 8)
    }
  }
};


/**
  Initialize the RAM disk device node.

  @param[in]      PrivateData     Points to RAM disk private data.
  @param[in, out] RamDiskDevNode  Points to the RAM disk device node.

**/
VOID
RamDiskInitDeviceNode (
  IN     RAM_DISK_PRIVATE_DATA         *PrivateData,
  IN OUT MEDIA_RAM_DISK_DEVICE_PATH    *RamDiskDevNode
  )
{
  WriteUnaligned64 (
    (UINT64 *) &(RamDiskDevNode->StartingAddr[0]),
    (UINT64) PrivateData->StartingAddr
    );
  WriteUnaligned64 (
    (UINT64 *) &(RamDiskDevNode->EndingAddr[0]),
    (UINT64) PrivateData->StartingAddr + PrivateData->Size - 1
    );
  CopyGuid (&RamDiskDevNode->TypeGuid, &PrivateData->TypeGuid);
  RamDiskDevNode->Instance = PrivateData->InstanceNumber;
}


/**
  Register a RAM disk with specified address, size and type.

  @param[in]  RamDiskBase    The base address of registered RAM disk.
  @param[in]  RamDiskSize    The size of registered RAM disk.
  @param[in]  RamDiskType    The type of registered RAM disk. The GUID can be
                             any of the values defined in section 9.3.6.9, or a
                             vendor defined GUID.
  @param[in]  ParentDevicePath
                             Pointer to the parent device path. If there is no
                             parent device path then ParentDevicePath is NULL.
  @param[out] DevicePath     On return, points to a pointer to the device path
                             of the RAM disk device.
                             If ParentDevicePath is not NULL, the returned
                             DevicePath is created by appending a RAM disk node
                             to the parent device path. If ParentDevicePath is
                             NULL, the returned DevicePath is a RAM disk device
                             path without appending. This function is
                             responsible for allocating the buffer DevicePath
                             with the boot service AllocatePool().

  @retval EFI_SUCCESS             The RAM disk is registered successfully.
  @retval EFI_INVALID_PARAMETER   DevicePath or RamDiskType is NULL.
                                  RamDiskSize is 0.
  @retval EFI_ALREADY_STARTED     A Device Path Protocol instance to be created
                                  is already present in the handle database.
  @retval EFI_OUT_OF_RESOURCES    The RAM disk register operation fails due to
                                  resource limitation.

**/
EFI_STATUS
EFIAPI
RamDiskRegister (
  IN UINT64                       RamDiskBase,
  IN UINT64                       RamDiskSize,
  IN EFI_GUID                     *RamDiskType,
  IN EFI_DEVICE_PATH              *ParentDevicePath     OPTIONAL,
  OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath
  )
{
  EFI_STATUS                      Status;
  RAM_DISK_PRIVATE_DATA           *PrivateData;
  RAM_DISK_PRIVATE_DATA           *RegisteredPrivateData;
  MEDIA_RAM_DISK_DEVICE_PATH      *RamDiskDevNode;
  UINTN                           DevicePathSize;
  LIST_ENTRY                      *Entry;

  if ((0 == RamDiskSize) || (NULL == RamDiskType) || (NULL == DevicePath)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Add check to prevent data read across the memory boundary
  //
  if (RamDiskBase + RamDiskSize > ((UINTN) -1) - RAM_DISK_BLOCK_SIZE + 1) {
    return EFI_INVALID_PARAMETER;
  }

  RamDiskDevNode = NULL;

  //
  // Create a new RAM disk instance and initialize its private data
  //
  PrivateData = AllocateCopyPool (
                  sizeof (RAM_DISK_PRIVATE_DATA),
                  &mRamDiskPrivateDataTemplate
                  );
  if (NULL == PrivateData) {
    return EFI_OUT_OF_RESOURCES;
  }

  PrivateData->StartingAddr = RamDiskBase;
  PrivateData->Size         = RamDiskSize;
  CopyGuid (&PrivateData->TypeGuid, RamDiskType);
  InitializeListHead (&PrivateData->ThisInstance);

  //
  // Generate device path information for the registered RAM disk
  //
  RamDiskDevNode = AllocateCopyPool (
                     sizeof (MEDIA_RAM_DISK_DEVICE_PATH),
                     &mRamDiskDeviceNodeTemplate
                     );
  if (NULL == RamDiskDevNode) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  RamDiskInitDeviceNode (PrivateData, RamDiskDevNode);

  *DevicePath = AppendDevicePathNode (
                  ParentDevicePath,
                  (EFI_DEVICE_PATH_PROTOCOL *) RamDiskDevNode
                  );
  if (NULL == *DevicePath) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  PrivateData->DevicePath = *DevicePath;

  //
  // Check whether the created device path is already present in the handle
  // database
  //
  if (!IsListEmpty(&RegisteredRamDisks)) {
    DevicePathSize = GetDevicePathSize (PrivateData->DevicePath);

    EFI_LIST_FOR_EACH (Entry, &RegisteredRamDisks) {
      RegisteredPrivateData = RAM_DISK_PRIVATE_FROM_THIS (Entry);
      if (DevicePathSize == GetDevicePathSize (RegisteredPrivateData->DevicePath)) {
        //
        // Compare device path
        //
        if ((CompareMem (
               PrivateData->DevicePath,
               RegisteredPrivateData->DevicePath,
               DevicePathSize)) == 0) {
          *DevicePath = NULL;
          Status      = EFI_ALREADY_STARTED;
          goto ErrorExit;
        }
      }
    }
  }

  //
  // Fill Block IO protocol informations for the RAM disk
  //
  RamDiskInitBlockIo (PrivateData);

  //
  // Install EFI_DEVICE_PATH_PROTOCOL & EFI_BLOCK_IO(2)_PROTOCOL on a new
  // handle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &PrivateData->Handle,
                  &gEfiBlockIoProtocolGuid,
                  &PrivateData->BlockIo,
                  &gEfiBlockIo2ProtocolGuid,
                  &PrivateData->BlockIo2,
                  &gEfiDevicePathProtocolGuid,
                  PrivateData->DevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  //
  // Insert the newly created one to the registered RAM disk list
  //
  InsertTailList (&RegisteredRamDisks, &PrivateData->ThisInstance);

  gBS->ConnectController (PrivateData->Handle, NULL, NULL, TRUE);

  FreePool (RamDiskDevNode);

  return EFI_SUCCESS;

ErrorExit:
  if (RamDiskDevNode != NULL) {
    FreePool (RamDiskDevNode);
  }

  if (PrivateData != NULL) {
    if (PrivateData->DevicePath) {
      FreePool (PrivateData->DevicePath);
    }

    FreePool (PrivateData);
  }

  return Status;
}


//**************************************************************************************
//Add Start By Lundy
VOID
EFIAPI
LundyAddRamDisk(
  IN EFI_EVENT                Event,
  IN VOID                     *Context
  )
{
  EFI_HANDLE                    *FileSystemHandles;
  UINTN                         NumberFileSystemHandles;
  UINTN                         Index;
  EFI_DEVICE_PATH_PROTOCOL      *InDevicePath;
  BOOLEAN                       flag;
  //CHAR16                        *DevPathStr;

  UINTN                        BootOptionCount;
  EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions;
  //UINTN                        Index;
  //UINTN                        OptionNumber;

  //Print(L"LundyAddRamDisk Start\n");

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

  for (Index = 0; Index < BootOptionCount; Index++) {
	if(!StrCmp(NEW_BOOT_OPTION_NAME,BootOptions[Index].Description)){
      EfiBootManagerDeleteLoadOptionVariable (BootOptions[Index].OptionNumber, LoadOptionTypeBoot);;
    }
  }


  
  gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        &NumberFileSystemHandles,
        &FileSystemHandles
        );
  // for (Index = 0; Index < NumberFileSystemHandles; Index++) {

  //   InDevicePath = DevicePathFromHandle (FileSystemHandles[Index]);
  //   if(IsVirtualDiskNew(InDevicePath)){
  //     DevPathStr = ConvertDevicePathToText (InDevicePath, FALSE, FALSE);
  //     DEBUG ((DEBUG_INFO , "Lundy %s\n", DevPathStr));
  //     if(StrlenNew(DevPathStr)>60)
  //       return;
  //   }
  // }

  for (Index = 0; Index < NumberFileSystemHandles; Index++) {

  	InDevicePath = DevicePathFromHandle (FileSystemHandles[Index]);
  	flag = IsVirtualDiskNew(InDevicePath);
    //Print(L"66666 %d\n",flag);
  	if(flag){
		RegisterRamDiskBootOption(InDevicePath,NEW_BOOT_OPTION_NAME, (UINTN) -1, TRUE);
  		break;
  	}
  }
}


BOOLEAN
IsVirtualDiskNew (
  IN VOID                 *DevPath
  )
{
  MEDIA_RAM_DISK_DEVICE_PATH *RamDisk;

  RamDisk = DevPath;

  if (CompareGuid (&RamDisk->TypeGuid, &gEfiVirtualDiskGuid)) {
    return TRUE;
  }
  return FALSE;
}



EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
LundyAppendNew (
  IN EFI_DEVICE_PATH_PROTOCOL        *InDevicePath,  
  IN CONST CHAR16                    *FileName
  )
{
  UINTN                     Size;
  FILEPATH_DEVICE_PATH      *FilePath;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *FileDevicePath;

  DevicePath = NULL;

  Size = StrSize (FileName);
  FileDevicePath = AllocatePool (Size + SIZE_OF_FILEPATH_DEVICE_PATH + END_DEVICE_PATH_LENGTH);
  if (FileDevicePath != NULL) {
    FilePath = (FILEPATH_DEVICE_PATH *) FileDevicePath;
    FilePath->Header.Type    = MEDIA_DEVICE_PATH;
    FilePath->Header.SubType = MEDIA_FILEPATH_DP;
    CopyMem (&FilePath->PathName, FileName, Size);
    SetDevicePathNodeLength (&FilePath->Header, Size + SIZE_OF_FILEPATH_DEVICE_PATH);
    SetDevicePathEndNode (NextDevicePathNode (&FilePath->Header));

    //if (Device != NULL) {
    //  DevicePath = DevicePathFromHandle (Device);
    //}

    DevicePath = AppendDevicePath (InDevicePath, FileDevicePath);
    FreePool (FileDevicePath);
  }

  return DevicePath;
}

  
UINTN
RegisterRamDiskBootOption (
  EFI_DEVICE_PATH_PROTOCOL         *RamDp,
  CHAR16                           *Description,
  UINTN                            Position,
  BOOLEAN                          IsBootCategory
  )
{
  EFI_STATUS                       Status;
  EFI_BOOT_MANAGER_LOAD_OPTION     NewOption;
  EFI_DEVICE_PATH_PROTOCOL         *DevicePath;
  UINTN                            OptionNumber;

  DevicePath = LundyAppendNew(RamDp,EFI_VIRTUAL_DISK_FILE_NAME);
  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             IsBootCategory ? LOAD_OPTION_ACTIVE : LOAD_OPTION_CATEGORY_APP,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  Status = EfiBootManagerAddLoadOptionVariable (&NewOption, Position);
  ASSERT_EFI_ERROR (Status);

  OptionNumber = NewOption.OptionNumber;

  EfiBootManagerFreeLoadOption (&NewOption);

  return OptionNumber;
}
//Add End By Lundy
//**************************************************************************************





/**
  The entry point for RamDisk

  @param[in] ImageHandle     The image handle of the driver.
  @param[in] SystemTable     The system table.

  @retval EFI_SUCCES              All the related protocols are installed on
                                  the driver.

**/
EFI_STATUS
EFIAPI
RDEntryPoint (
  IN EFI_HANDLE                   ImageHandle,
  IN EFI_SYSTEM_TABLE             *SystemTable
  )
{
  EFI_STATUS                      Status;
  UINT64                          *StartingAddr;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  //Add Start By Lundy
  EFI_EVENT   ReadyToBootEvent;
  //Add End By Lundy
  //
  // Initialize the list of registered RAM disks maintained by the driver
  //
  InitializeListHead (&RegisteredRamDisks);
  
  //
  // Allocate boot service data for this
  //
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  (UINTN)__data_img_len,
                  (VOID**)&StartingAddr
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Allocate Memory Failed\n"));
  }

  //
  // regesiter ramdisk by data.img.h
  //
  CopyMem ((UINT8 *)(UINTN)StartingAddr, __data_img, __data_img_len);
  Status = RamDiskRegister (
             ((UINT64)(UINTN) StartingAddr),
             __data_img_len,
             &gEfiVirtualDiskGuid,
             NULL,
             &DevicePath
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Register ramdisk failed\n"));
  }
//Add Start By Lundy
 Status = gBS->CreateEventEx (
                 EVT_NOTIFY_SIGNAL,
                 TPL_NOTIFY,
                 LundyAddRamDisk,
                 NULL,
                 &gEfiEventReadyToBootGuid,
                 &ReadyToBootEvent
                 );
  if (EFI_ERROR (Status)) {
     DEBUG ((EFI_D_ERROR, "Create ramdisk failed\n"));
  }
  //Add End By Lundy
  
  return EFI_SUCCESS;

}








