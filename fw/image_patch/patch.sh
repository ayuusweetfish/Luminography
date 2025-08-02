cp /media/ayu/rootfs/home/ayu/f1c_build/images/flash.bin flash.bin
cpp -I./include -traditional-cpp suniv-f1c100s-licheepi-nano.dts 2> /dev/null | dtc -I dts -O dtb /dev/stdin | dd if=/dev/stdin of=flash.bin bs=1K seek=512 count=8 conv=notrunc
