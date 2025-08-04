mkfs.jffs2 -r rootfs -o rootfs.jffs2 -e 64 -l
~/Downloads/sunxi-tools/sunxi-fel -p spiflash-write 0x590000 rootfs.jffs2
