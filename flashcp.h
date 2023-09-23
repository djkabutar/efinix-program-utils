#include <errno.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/version.h>
#include <unistd.h>

#define PROGRAM_NAME "flashcp"
#define VERSION "1.0"

/* error levels */
#define LOG_NORMAL 1
#define LOG_ERROR 2

/* for tagging functions that always exit */
#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

#define RESET_GPIO "509"
#define CONDONE_GPIO "510"

void set_verbose(int v);
int get_verbose(void);
NORETURN void log_failure(const char *fmt, ...);
void log_verbose(const char *fmt, ...);
int safe_open(const char *pathname, int flags);
void safe_read(int fd, const char *filename, void *buf, size_t count);
void safe_write(int fd, const void *buf, size_t count, size_t written,
		       unsigned long long to_write, const char *device);
off_t safe_lseek(int fd, off_t offset, int whence, const char *filename);

void safe_rewind(int fd, const char *filename);

void safe_memerase(int fd, const char *device,
			  struct erase_info_user *erase);

int gpio_export(const char *pin);
int gpio_unexport(const char *pin);
void gpio_set_direction(const char *pin, const char *direction);
void gpio_set_value(const char *pin, const char *value);

void flash_access_to_processor(void);
void flash_access_to_fpga(void);

