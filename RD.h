#ifndef _RAM_DISK_MAIN_H_
#define _RAM_DISK_MAIN_H_
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/PcdLib.h>
#include <Library/DxeServicesLib.h>
#include <Protocol/RamDisk.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/SimpleFileSystem.h>

#ifndef RAM_DISK_BLOCK_SIZE
#define RAM_DISK_BLOCK_SIZE 512
#endif
//
// maintains a list of registered RAM disks.
// The struct contains the list entry and the information of each RAM
// disk
//
typedef struct {
  UINTN                           Signature;

  EFI_HANDLE                      Handle;

  EFI_BLOCK_IO_PROTOCOL           BlockIo;
  EFI_BLOCK_IO2_PROTOCOL          BlockIo2;
  EFI_BLOCK_IO_MEDIA              Media;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;

  UINT64                          StartingAddr;
  UINT64                          Size;
  EFI_GUID                        TypeGuid;
  UINT16                          InstanceNumber;

  LIST_ENTRY                      ThisInstance;
} RAM_DISK_PRIVATE_DATA;

#define RAM_DISK_PRIVATE_DATA_SIGNATURE     SIGNATURE_32 ('R', 'D', 'S', 'K')
#define RAM_DISK_PRIVATE_FROM_BLKIO(a)      CR (a, RAM_DISK_PRIVATE_DATA, BlockIo, RAM_DISK_PRIVATE_DATA_SIGNATURE)
#define RAM_DISK_PRIVATE_FROM_BLKIO2(a)     CR (a, RAM_DISK_PRIVATE_DATA, BlockIo2, RAM_DISK_PRIVATE_DATA_SIGNATURE)
#define RAM_DISK_PRIVATE_FROM_THIS(a)       CR (a, RAM_DISK_PRIVATE_DATA, ThisInstance, RAM_DISK_PRIVATE_DATA_SIGNATURE)
#define EFI_LIST_FOR_EACH(Entry, ListHead)    \
  for(Entry = (ListHead)->ForwardLink; Entry != (ListHead); Entry = Entry->ForwardLink)

//
// Iterate through the double linked list. This is delete-safe.
// Do not touch NextEntry
//
#define EFI_LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead)            \
  for(Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink;\
      Entry != (ListHead); Entry = NextEntry, NextEntry = Entry->ForwardLink)

/**
  Initialize the BlockIO protocol of a RAM disk device.

  @param[in] PrivateData     Points to RAM disk private data.

**/
VOID
RamDiskInitBlockIo (
  IN     RAM_DISK_PRIVATE_DATA    *PrivateData
  );

/**
  Reset the Block Device.

  @param[in] This            Indicates a pointer to the calling context.
  @param[in] ExtendedVerification
                             Driver may perform diagnostics on reset.

  @retval EFI_SUCCESS             The device was reset.
  @retval EFI_DEVICE_ERROR        The device is not functioning properly and
                                  could not be reset.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIoReset (
  IN EFI_BLOCK_IO_PROTOCOL        *This,
  IN BOOLEAN                      ExtendedVerification
  );

/**
  Read BufferSize bytes from Lba into Buffer.

  @param[in]  This           Indicates a pointer to the calling context.
  @param[in]  MediaId        Id of the media, changes every time the media is
                             replaced.
  @param[in]  Lba            The starting Logical Block Address to read from.
  @param[in]  BufferSize     Size of Buffer, must be a multiple of device block
                             size.
  @param[out] Buffer         A pointer to the destination buffer for the data.
                             The caller is responsible for either having
                             implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while performing
                                  the read.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_MEDIA_CHANGED       The MediaId does not matched the current
                                  device.
  @retval EFI_BAD_BUFFER_SIZE     The Buffer was not a multiple of the block
                                  size of the device.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL        *This,
  IN UINT32                       MediaId,
  IN EFI_LBA                      Lba,
  IN UINTN                        BufferSize,
  OUT VOID                        *Buffer
  );

/**
  Write BufferSize bytes from Lba into Buffer.

  @param[in] This            Indicates a pointer to the calling context.
  @param[in] MediaId         The media ID that the write request is for.
  @param[in] Lba             The starting logical block address to be written.
                             The caller is responsible for writing to only
                             legitimate locations.
  @param[in] BufferSize      Size of Buffer, must be a multiple of device block
                             size.
  @param[in] Buffer          A pointer to the source buffer for the data.

  @retval EFI_SUCCESS             The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED     The device can not be written to.
  @retval EFI_DEVICE_ERROR        The device reported an error while performing
                                  the write.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_MEDIA_CHNAGED       The MediaId does not matched the current
                                  device.
  @retval EFI_BAD_BUFFER_SIZE     The Buffer was not a multiple of the block
                                  size of the device.
  @retval EFI_INVALID_PARAMETER   The write request contains LBAs that are not
                                  valid, or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIoWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL        *This,
  IN UINT32                       MediaId,
  IN EFI_LBA                      Lba,
  IN UINTN                        BufferSize,
  IN VOID                         *Buffer
  );

/**
  Flush the Block Device.

  @param[in] This            Indicates a pointer to the calling context.

  @retval EFI_SUCCESS             All outstanding data was written to the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while writting
                                  back the data
  @retval EFI_NO_MEDIA            There is no media in the device.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL        *This
  );

/**
  Resets the block device hardware.

  @param[in] This                 The pointer of EFI_BLOCK_IO2_PROTOCOL.
  @param[in] ExtendedVerification The flag about if extend verificate.

  @retval EFI_SUCCESS             The device was reset.
  @retval EFI_DEVICE_ERROR        The block device is not functioning correctly
                                  and could not be reset.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIo2Reset (
  IN EFI_BLOCK_IO2_PROTOCOL       *This,
  IN BOOLEAN                      ExtendedVerification
  );

/**
  Reads the requested number of blocks from the device.

  @param[in]      This            Indicates a pointer to the calling context.
  @param[in]      MediaId         The media ID that the read request is for.
  @param[in]      Lba             The starting logical block address to read
                                  from on the device.
  @param[in, out] Token           A pointer to the token associated with the
                                  transaction.
  @param[in]      BufferSize      The size of the Buffer in bytes. This must be
                                  a multiple of the intrinsic block size of the
                                  device.
  @param[out]     Buffer          A pointer to the destination buffer for the
                                  data. The caller is responsible for either
                                  having implicit or explicit ownership of the
                                  buffer.

  @retval EFI_SUCCESS             The read request was queued if Token->Event
                                  is not NULL. The data was read correctly from
                                  the device if the Token->Event is NULL.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the read operation.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_MEDIA_CHANGED       The MediaId is not for the current media.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not on proper
                                  alignment.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a
                                  lack of resources.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIo2ReadBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL   *This,
  IN     UINT32                   MediaId,
  IN     EFI_LBA                  Lba,
  IN OUT EFI_BLOCK_IO2_TOKEN      *Token,
  IN     UINTN                    BufferSize,
     OUT VOID                     *Buffer
  );

/**
  Writes a specified number of blocks to the device.

  @param[in]      This            Indicates a pointer to the calling context.
  @param[in]      MediaId         The media ID that the write request is for.
  @param[in]      Lba             The starting logical block address to be
                                  written. The caller is responsible for
                                  writing to only legitimate locations.
  @param[in, out] Token           A pointer to the token associated with the
                                  transaction.
  @param[in]      BufferSize      The size in bytes of Buffer. This must be a
                                  multiple of the intrinsic block size of the
                                  device.
  @param[in]      Buffer          A pointer to the source buffer for the data.

  @retval EFI_SUCCESS             The write request was queued if Event is not
                                  NULL. The data was written correctly to the
                                  device if the Event is NULL.
  @retval EFI_WRITE_PROTECTED     The device cannot be written to.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_MEDIA_CHANGED       The MediaId is not for the current media.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the write operation.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.
  @retval EFI_INVALID_PARAMETER   The write request contains LBAs that are not
                                  valid, or the buffer is not on proper
                                  alignment.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a
                                  lack of resources.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIo2WriteBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL   *This,
  IN     UINT32                   MediaId,
  IN     EFI_LBA                  Lba,
  IN OUT EFI_BLOCK_IO2_TOKEN      *Token,
  IN     UINTN                    BufferSize,
  IN     VOID                     *Buffer
  );

/**
  Flushes all modified data to a physical block device.

  @param[in]      This            Indicates a pointer to the calling context.
  @param[in, out] Token           A pointer to the token associated with the
                                  transaction.

  @retval EFI_SUCCESS             The flush request was queued if Event is not
                                  NULL. All outstanding data was written
                                  correctly to the device if the Event is NULL.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to write data.
  @retval EFI_WRITE_PROTECTED     The device cannot be written to.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_MEDIA_CHANGED       The MediaId is not for the current media.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a
                                  lack of resources.

**/
EFI_STATUS
EFIAPI
RamDiskBlkIo2FlushBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL   *This,
  IN OUT EFI_BLOCK_IO2_TOKEN      *Token
  );
#endif
