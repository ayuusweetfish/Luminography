// /media/ayu/rootfs/home/ayu/f1c_build/host/bin/arm-linux-gcc blink.c -o rootfs/root/blink -O2
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

int main()
{
  int fd = open("/dev/gpiochip0", O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  int line = 134;   // PE6 = 4 * 32 + 6
  struct gpiohandle_request req = {
    .lineoffsets = { line },
    .flags = GPIOHANDLE_REQUEST_OUTPUT,
    .default_values = { 0 },
    .consumer_label = "blink",
    .lines = 1,
  };
  if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
    perror("GPIO_GET_LINEHANDLE_IOCTL");
    close(fd);
    return 1;
  }

  close(fd);

  printf("Blinking LED at GPIO line %d\n", line);
  int parity = 0;
  while (1) {
    parity ^= 1;
    puts(parity ? "On" : "Off");
    struct gpiohandle_data data = { .values = { parity } };
    if (ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
      perror("GPIOHANDLE_SET_LINE_VALUES_IOCTL");
      break;
    }
    usleep(500000);
  }

  close(req.fd);

  return 0;
}
