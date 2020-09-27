#!/bin/bash
rm ./data.img
export EFI_BOOT_MEDIA=./data.img
echo "Start to create file boot disk ..."
dd bs=1024 count=1440 if=/dev/zero of=$EFI_BOOT_MEDIA
mkfs.msdos -F 12 $EFI_BOOT_MEDIA
mcopy -i $EFI_BOOT_MEDIA -/ ./Source/* ::/
mdir -i $EFI_BOOT_MEDIA -s ::
## Linux version of GenBootSector has not pass build yet.
./GnuGenBootSector -i $EFI_BOOT_MEDIA -o $EFI_BOOT_MEDIA.bs0
cp ./bootsect.com $EFI_BOOT_MEDIA.bs1
./BootSectImage -g $EFI_BOOT_MEDIA.bs0 $EFI_BOOT_MEDIA.bs1
./GnuGenBootSector -o $EFI_BOOT_MEDIA -i $EFI_BOOT_MEDIA.bs1
rm $EFI_BOOT_MEDIA.bs[0-1]
rm IMG.c
xxd -i $EFI_BOOT_MEDIA > IMG.c

echo Done.

