dd if=/media/ayu/rootfs/home/ayu/f1c_build/images/flash.bin of=flash_8M.bin bs=1M seek=0 count=8
cpp -I./include -traditional-cpp suniv-f1c100s-licheepi-nano.dts 2> /dev/null | dtc -I dts -O dtb /dev/stdin | dd if=/dev/stdin of=flash_8M.bin bs=1K seek=512 count=8 conv=notrunc
