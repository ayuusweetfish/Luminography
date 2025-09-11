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

static int gpio_line;
static int req_fd = 0;
static void sigint_handler(int n)
{
  printf("Received SIGINT, exiting\n");
  exit(0);

  if (req_fd != 0) {
    printf("Received SIGINT, setting pin output to low and exiting\n");
    struct gpio_v2_line_values values = { .bits = 0, .mask = 1 };
    if (ioctl(req_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
      perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
    }
    close(req_fd);
  }
  exit(0);
}

int main(int argc, char *argv[])
{
  int fd = open("/dev/gpiochip0", O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  int gpio_line = 134;  // PE6 = 4 * 32 + 6
  if (argc >= 2) {
    const char *p = argv[1];
    if (p[0] == 'P' || p[0] == 'p') p++;
    if ((p[0] >= 'A' && p[0] <= 'Z') || p[0] >= 'a' && p[0] <= 'z') {
      gpio_line = ((p[0] - 'A') % 32) * 32 + (int)strtol(p + 1, NULL, 0);
    } else {
      gpio_line = (int)strtol(argv[1], NULL, 0);
    }
  }
  struct gpio_v2_line_request req = {
    .offsets = { gpio_line },
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

  printf("Blinking GPIO line %d (P%c%d)\n", gpio_line, 'A' + (gpio_line / 32), gpio_line % 32);
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
