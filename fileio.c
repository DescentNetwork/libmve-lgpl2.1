#include "fileio.h"

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "platform_endian.h"

#if __BYTE_ORDER__ == __BIG_ENDIAN__
#define to16le(x) bswap16(x);
#define to32le(x) bswap32(x);
#define to64le(x) bswap64(x);
#elif __BYTE_ORDER__ == __LITTLE_ENDIAN__
#define to16le(x) x
#define to32le(x) x
#define to64le(x) x
#else
# error Bad endianness
#endif

static const int posix_error = -1;

typedef int fd_t;
static const fd_t invalid_descriptor = 0;

static fd_t current_file = invalid_descriptor;

struct mve_header_t
{
  const char file_signature[20];
  const uint8_t const_data[6];
};

static const struct mve_header_t file_signature =
{
  "Interplay MVE File\x1A",
  { 0x1A, 0x00, 0x00, 0x01, 0x33, 0x11 }
};

static void test_return_value(int rval)
{
  if(rval == posix_error)
    assert(raise(SIGTTIN) != posix_error); /* raise *should* never fail */
}

bool mve_open(const char* fileName)
{
  struct mve_header_t header;

  fd_t rval = posix_error;
  do {
    rval = open(fileName, 0, O_RDONLY);
  } while(rval == posix_error && errno == EINTR);

  if(rval == posix_error)
    return false;

  readptr(&header, sizeof(struct mve_header_t));

  return !memcmp(&header, &file_signature, sizeof(struct mve_header_t));
}

bool mve_close(void)
{
  return close(current_file) != posix_error;
}


void skipnbytes(int count)
{
  off_t rval = posix_error;
  do { rval = lseek(current_file, count, SEEK_CUR);
  } while(rval == posix_error && errno == EINTR);
  test_return_value(rval);
}

void readptr(void* data, size_t count)
{
  int rval = posix_error;
  int total = 0;
  size_t remaining = count;
  do
  {
    rval = read(current_file, data, remaining);
    if(rval != posix_error)
    {
      total += rval;
      remaining -= rval;
    }
  } while(remaining || (rval == posix_error && errno == EINTR));
  test_return_value(rval);
}

uint8_t read8(void)
{
  uint8_t data;
  readptr(&data, 1);
  return data;
}

uint16_t read16(void)
{
  uint16_t data;
  readptr(&data, 2);
  return data;
}

uint32_t read32(void)
{
  uint32_t data;
  readptr(&data, 4);
  return data;
}

uint64_t read64(void)
{
  uint64_t data;
  readptr(&data, 8);
  return data;
}

uint16_t read16LE(void)
  { return to16le(read16()); }

uint32_t read32LE(void)
  { return to32le(read32()); }

uint64_t read64LE(void)
  { return to64le(read64()); }
