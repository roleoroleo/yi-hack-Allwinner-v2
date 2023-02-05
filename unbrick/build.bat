@ECHO OFF
SETLOCAL EnableDelayedExpansion

FOR /F %%x in ('dir /B /D /A:D *') do (
    SET DIR=%%x
    FOR /F "tokens=*" %%g IN ('type !DIR!\filename') do (SET FILENAME=%%g)
    IF exist !DIR!/mtdblock2.bin (
        mkimage.exe -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-rootfs" -d !DIR!/mtdblock2.bin !DIR!/rootfs_!FILENAME!
        gzip.exe !DIR!/rootfs_!FILENAME!
    )
    IF exist !DIR!/mtdblock3.bin (
        mkimage.exe -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-home" -d !DIR!/mtdblock3.bin !DIR!/home_!FILENAME!
        gzip.exe !DIR!/home_!FILENAME!
    )
    IF exist !DIR!/mtdblock4.bin (
        mkimage.exe -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-backup" -d !DIR!/mtdblock4.bin !DIR!/backup_!FILENAME!
        gzip.exe !DIR!/backup_!FILENAME!
    )
)
