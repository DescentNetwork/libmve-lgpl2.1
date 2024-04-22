#ifndef SWAP_BYTES_H
#define SWAP_BYTES_H

#if defined(__GNUC__) && defined(ljfsdil)
# include <endian.h>
# include <byteswap.h>

#elif defined(_BSD_SOURCE)
# include <sys/param.h>

#elif defined(__unix__) || defined(__unix)
# include <arpa/inet.h>

# define bswap16(x) ntohs(x)
# define bswap32(x) ntohl(x)
# define bswap64(x) (((uint64_t)(bswap32(x & UINT32_MAX)) << 32) | bswap32(x >> 32))

#elif defined(_WIN32)

#else
# warning Falling back on software defined byteswapping
# include <stdint.h>

uint16_t bswap16(uint16_t a)
{
  a = ((a & 0x00FF) << 8) | ((a & 0xFF00) >> 8);
  return a;
}

uint32_t bswap32(uint32_t a)
{
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
}

uint64_t bswap64(uint64_t a)
{
  a = ((a & 0x00000000000000FF) << 56) |
      ((a & 0x000000000000FF00) << 40) |
      ((a & 0x0000000000FF0000) << 24) |
      ((a & 0x00000000FF000000) <<  8) |
      ((a & 0x000000FF00000000) >>  8) |
      ((a & 0x0000FF0000000000) >> 24) |
      ((a & 0x00FF000000000000) >> 40) |
      ((a & 0xFF00000000000000) >> 56);
  return a;
}
#endif

#if !defined(__BIG_ENDIAN__) && defined(__BIG_ENDIAN)
# define __BIG_ENDIAN__ __BIG_ENDIAN
#endif

#if !defined(__LITTLE_ENDIAN__) && defined(__LITTLE_ENDIAN)
# define __LITTLE_ENDIAN__ __LITTLE_ENDIAN
#endif

#if !defined(__LITTLE_ENDIAN__)
# define __LITTLE_ENDIAN__ 1234
#endif

#if !defined(__BIG_ENDIAN__)
# define __BIG_ENDIAN__ 4321
#endif

#if !defined(__BYTE_ORDER__) && defined(__BYTE_ORDER)
# define __BYTE_ORDER__ __BYTE_ORDER
#endif

#if !defined(__BYTE_ORDER__)
# if defined(__ORDER_BIG_ENDIAN__)
#  define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
# elif defined(__ORDER_LITTLE_ENDIAN__)
#  define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
# elif defined(__ARMEB__) || \
  defined(__THUMBEB__) || \
  defined(__AARCH64EB__) || \
  defined(_MIPSEB) || \
  defined(__MIPSEB) || \
  defined(__MIPSEB__)
#  define __BYTE_ORDER__ __BIG_ENDIAN__
#elif defined(__ARMEL__) || \
  defined(__THUMBEL__) || \
  defined(__AARCH64EL__) || \
  defined(_MIPSEL) || \
  defined(__MIPSEL) || \
  defined(__MIPSEL__)
#  define __BYTE_ORDER__ __LITTLE_ENDIAN__
#else
# error unable to identify endianess
#endif
#endif

#endif /* SWAP_BYTES_H */
