// /media/ayu/rootfs/home/ayu/f1c_build/host/bin/arm-linux-gcc blink.c -o rootfs/root/blink -O2

// Reference: https://docs.kernel.org/userspace-api/gpio/chardev.html
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>   // exit
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

static int req_fd = 0;
static void sigint_handler(int n)
{
  if (req_fd != 0) {
    printf("Received SIGINT, turning off LED and exiting\n");
    struct gpio_v2_line_values values = { .bits = 0, .mask = 1 };
    if (ioctl(req_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
      perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
    }
    close(req_fd);
  }
  exit(0);
}

int main()
{
  int fd = open("/dev/gpiochip0", O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  int line = 134;   // PE6 = 4 * 32 + 6
  struct gpio_v2_line_request req = {
    .offsets = { line },
    .consumer = "blink",
    .config.flags = GPIO_V2_LINE_FLAG_OUTPUT,
    .num_lines = 1,
  };
  if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
    perror("GPIO_V2_GET_LINE_IOCTL");
    close(fd);
    return 1;
  }

  close(fd);

  req_fd = req.fd;
  signal(SIGINT, sigint_handler);

  printf("Blinking LED at GPIO line %d\n", line);
  int parity = 0;
  while (1) {
    parity ^= 1;
    puts(parity ? "On" : "Off");
    struct gpio_v2_line_values values = { .bits = parity, .mask = 1 };
    if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
      perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
      break;
    }
    usleep(500000);
  }

  assert(0);
}
