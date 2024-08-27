cat >debug/gdbinit <<EOF
define hook-quit
  set confirm off
end
target extended-remote localhost:3333

b swv_trap_line
commands
  silent
  printf "%s\n", swv_buf
  c
end

b main.c:862
commands
  silent
  p mag
  c
end

r
EOF

~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gdb .pio/build/dev/firmware.elf -x debug/gdbinit
rm debug/gdbinit
