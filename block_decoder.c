#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "fileio.h"

extern intptr_t videoStride;
extern uint8_t* current_frame_buffer;
extern uint8_t* previous_frame_buffer;

#define pixel_select1(data) \
  pixel[data & 1]; data >>= 1

#define pixel_select2(data) \
  pixel[data & 3]; data >>= 2

#define copy_row(dest, src) \
  memcpy(dest, src, 8); \
  src += videoStride; \
  dest += videoStride

static inline void copy_block(uint8_t* dest, uint8_t* src)
{
#ifdef MANUAL_UNROLL
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
  copy_row(dest, src);
#else
  for (register uint8_t i = 0; i < 8; i++)
    { copy_row(dest, src); } /* braces required */
#endif
}

void decode_block(uint8_t op_id, intptr_t offset)
{
  static uint8_t pixel[8];

  switch(op_id)
  {
    case 0x00:
      copy_block(current_frame_buffer + offset,
                 previous_frame_buffer + offset);
    break;

  case 0x01:
    break;

  case 0x02:
  {
    register intptr_t x, y;
    register uint8_t buf8 = read8();
    if (buf8 < 56)
    {
        x = (buf8 % 7) + 8;
        y = buf8 / 7;
    }
    else
    {
        x = ((buf8 - 56) % 29) - 14;
        y = ((buf8 - 56) / 29) + 8;
    }

    copy_block(current_frame_buffer + offset,
               current_frame_buffer + offset + x + (y * videoStride));
    break;
  } /* /case */

  case 0x03:
  {
    // copy 8x8 block from current frame from an up/left block
    register intptr_t x, y;
    register uint8_t buf8 = read8(); // need 1 more byte for motion
    if (buf8 < 56)
    {
        x = -((buf8 % 7) + 8);
        y = -(buf8 / 7);
    }
    else
    {
        x = -(((buf8 - 56) % 29) - 14);
        y = -(((buf8 - 56) / 29) + 8);
    }
    copy_block(current_frame_buffer + offset,
               current_frame_buffer + offset + x + (y * videoStride));
    break;
  } /* /case */

  case 0x04:
  {
    // copy a block from the previous frame; need 1 more byte
    register uint8_t buf8 = read8();
    register intptr_t x = (buf8 & 0x0F) - 8;
    register intptr_t y = (buf8 >> 4) - 8;

    copy_block(current_frame_buffer + offset,
               previous_frame_buffer + offset + x + (y * videoStride));
    break;
  } /* /case */

  case 0x05:
  {
    // copy a block from the previous frame using an expanded range; need 2 more bytes
    register intptr_t x = read8();
    register intptr_t y = read8();
    copy_block(current_frame_buffer + offset,
               previous_frame_buffer + offset + x + (y * videoStride));
    break;
  } /* /case */

  case 0x06:
    assert(0);
    break;

  case 0x07:
  {
    readptr(pixel, 2);
    register uint8_t* ptr8 = current_frame_buffer + offset;

    // 2-color encoding
    if (pixel[1] & 0x80)
    {
      // need 8 more bytes from the stream
      for (register uint8_t row = 0; row < 8; row++)
      {
        register uint8_t buf8 = read8();

#ifdef MANUAL_UNROLL
        ptr8[0] = pixel_select1(buf8);
        ptr8[1] = pixel_select1(buf8);
        ptr8[2] = pixel_select1(buf8);
        ptr8[3] = pixel_select1(buf8);
        ptr8[4] = pixel_select1(buf8);
        ptr8[5] = pixel_select1(buf8);
        ptr8[6] = pixel_select1(buf8);
        ptr8[7] = pixel_select1(buf8);
#else
        for (register uint8_t i = 0; i < 8; i++)
          { ptr8[i] = pixel_select1(buf8); } /* braces required */
#endif

        ptr8 += videoStride;
      }
    }
    else
    {
      register uint16_t buf16 = read16LE();
      for(register uint8_t row = 0; row < 16; ++row)
      {
        register uint16_t col = (row & 3) << 1;
        ptr8[col + 0] =
        ptr8[col + 1] =
        ptr8[col + 0 + videoStride] =
        ptr8[col + 1 + videoStride] = pixel_select1(buf16);
        if((row & 3) == 3)
          ptr8 += videoStride << 1;
      }
    }
    break;
  } /* /case */

  case 0x08:
  {
    readptr(pixel, 2);
    register uint8_t* ptr8 = current_frame_buffer + offset;

    // 2-color encoding for each 4x4 quadrant, or 2-color encoding on either top and bottom or left and right halves
    if (pixel[1] & 0x80)
    {
      register uint16_t buf16 = UINT16_MAX;
      for(register uint8_t row = 0; row < 16; ++row)
      {
        // switch to right half
        if (row == 8)
          ptr8 -= 8 * videoStride;

        // new values for each 4x4 block
        if (!(row & 3))
        {
          if (row)
            readptr(pixel, 2);
          buf16 = read16LE();
        }

#ifdef MANUAL_UNROLL
        ptr8[0] = pixel_select1(buf16);
        ptr8[1] = pixel_select1(buf16);
        ptr8[2] = pixel_select1(buf16);
        ptr8[3] = pixel_select1(buf16);
#else
          for (register uint8_t i = 0; i < 4; i++)
            { ptr8[i] = pixel_select1(buf16); } /* braces required */
#endif

        ptr8 += videoStride;
      }
    }
    else
    {
      readptr(pixel + 2, 2);
      register uint32_t buf32 = read32LE();

      if (pixel[3] & 0x80)
      {
        // vertical split; left & right halves are 2-color encoded
        for (register uint8_t row = 0; row < 16; row++)
        {
          // switch to right half
          if (row == 8)
          {
            ptr8 -= 8 * videoStride - 4;
            ((uint16_t*)pixel)[0] = ((uint16_t*)pixel)[1];
            buf32 = read32LE();
          }

#ifdef MANUAL_UNROLL
          ptr8[0] = pixel_select1(buf32);
          ptr8[1] = pixel_select1(buf32);
          ptr8[2] = pixel_select1(buf32);
          ptr8[3] = pixel_select1(buf32);
#else
          for (register uint8_t i = 0; i < 4; i++)
            { ptr8[i] = pixel_select1(buf32); } /* braces required */
#endif
          ptr8 += videoStride;
        }
      }
      else
      {
        // horizontal split; top & bottom halves are 2-color encoded
        for (register uint8_t row = 0; row < 8; row++)
        {
          if (row == 4)
          {
            ((uint16_t*)pixel)[0] = ((uint16_t*)pixel)[1];
            buf32 = read32LE();
          }

#ifdef MANUAL_UNROLL
          ptr8[0] = pixel_select1(buf32);
          ptr8[1] = pixel_select1(buf32);
          ptr8[2] = pixel_select1(buf32);
          ptr8[3] = pixel_select1(buf32);
          ptr8[4] = pixel_select1(buf32);
          ptr8[5] = pixel_select1(buf32);
          ptr8[6] = pixel_select1(buf32);
          ptr8[7] = pixel_select1(buf32);
#else
          for (register uint8_t i = 0; i < 8; i++)
            { ptr8[i] = pixel_select1(buf32); } /* braces required */
#endif
          ptr8 += videoStride;
        }
      }
    }
    break;
  } /* /case */

  case 0x09:
  {
    readptr(pixel, 4);
    register uint8_t* ptr8 = current_frame_buffer + offset;

    if (pixel[1] & 0x80)
    {
      if (pixel[3] & 0x80)
      {
        register uint8_t buf16;
        // 1 of 4 colors for each pixel, need 16 more bytes
        for (register uint8_t y = 0; y < 8; y++)
        {
            // get the next set of 8 2-bit flags
            buf16 = read16LE();
#ifdef MANUAL_UNROLL
            ptr8[0] = pixel_select2(buf16);
            ptr8[1] = pixel_select2(buf16);
            ptr8[2] = pixel_select2(buf16);
            ptr8[3] = pixel_select2(buf16);
            ptr8[4] = pixel_select2(buf16);
            ptr8[5] = pixel_select2(buf16);
            ptr8[6] = pixel_select2(buf16);
            ptr8[7] = pixel_select2(buf16);
#else
          for (register uint8_t i = 0; i < 8; i++)
            { ptr8[0] = pixel_select2(buf16); } /* braces required */
#endif
            ptr8 += videoStride;
        }
      }
      else
      {
        // 1 of 4 colors for each 2x2 block, need 4 more bytes
        register uint32_t buf32 = read32LE();
        register uint16_t col;
        for (register uint8_t i = 0; i < 16; i++)
        {
          col = (i & 3) << 1;
          ptr8[col + 0] =
          ptr8[col + 1] =
          ptr8[col + 0 + videoStride] =
          ptr8[col + 1 + videoStride] = pixel_select2(buf32);
          if((i & 3) == 3)
            ptr8 += videoStride << 1;
        }
      }
    }
    else
    {
      // 1 of 4 colors for each 2x1 or 1x2 block, need 8 more bytes
      register uint64_t buf64 = read64LE();
      register uint8_t row;
      if (pixel[3] & 0x80)
      {
        for (row = 0; row < 8; row++)
        {
#ifdef MANUAL_UNROLL
          ptr8[0] =
          ptr8[1] = pixel_select2(buf64);
          ptr8[2] =
          ptr8[3] = pixel_select2(buf64);
          ptr8[4] =
          ptr8[5] = pixel_select2(buf64);
          ptr8[6] =
          ptr8[7] = pixel_select2(buf64);
#else
          for (register uint8_t i = 0; i < 8; i += 2)
          {
            ptr8[i] =
            ptr8[i + 1] = pixel_select2(buf64);
          }
#endif
          ptr8 += videoStride;
        }
      }
      else
      {
        for (row = 0; row < 4; row++)
        {
#ifdef MANUAL_UNROLL
          ptr8[0] = pixel_select2(buf64);
          ptr8[1] = pixel_select2(buf64);
          ptr8[2] = pixel_select2(buf64);
          ptr8[3] = pixel_select2(buf64);
          ptr8[4] = pixel_select2(buf64);
          ptr8[5] = pixel_select2(buf64);
          ptr8[6] = pixel_select2(buf64);
          ptr8[7] = pixel_select2(buf64);
#else
          for (register uint8_t i = 0; i < 8; i++)
            { ptr8[i] = pixel_select2(buf64); } /* braces required */
#endif
          memcpy(ptr8, ptr8 + videoStride, 8);
          ptr8 += videoStride << 1;
        }
      }
    }
    break;
  } /* /case */

  case 0x0A: // 10
  {
    readptr(pixel, 4);
    register uint8_t* ptr8 = current_frame_buffer + offset;

    // 4-color encoding for each 4x4 quadrant, or 4-color encoding on either top and bottom or left and right halves
    if (pixel[1] & 0x80)
    {
      register uint32_t buf32 = 0;
      // 4-color encoding for each quadrant; need 32 bytes
      for (register uint8_t i = 0; i < 16; i++)
      {
        // switch to right half
        if (i == 8)
          ptr8 -= 8 * videoStride;

        // new values for each 4x4 block
        if (!(i & 3))
        {
          if (i)
            readptr(pixel, 4);
          buf32 = read32LE();
        }

#ifdef MANUAL_UNROLL
        ptr8[0] = pixel_select2(buf32);
        ptr8[1] = pixel_select2(buf32);
        ptr8[2] = pixel_select2(buf32);
        ptr8[3] = pixel_select2(buf32);
#else
          for (register uint8_t i = 0; i < 4; i++)
            { ptr8[i] = pixel_select2(buf32); } /* braces required */
#endif
        ptr8 += videoStride;
      }
    }
    else
    {
      readptr(pixel + 4, 4);
      // vertical split?
      register uint8_t vert = pixel[5] & 0x80;
      register uint64_t buf64 = read64LE();

      // 4-color encoding for either left and right or top and bottom halves
      for (register uint8_t i = 0; i < 16; i++)
      {
        if (vert || !(i & 1))
          ptr8 += videoStride;

        // load values for second half
        if (i == 8)
        {
          // switch to right half
          if (vert)
            ptr8 -= videoStride << 3;
          memcpy(pixel, pixel + 4, 4);
          buf64 = read64LE();
        }

#ifdef MANUAL_UNROLL
        ptr8[0] = pixel_select2(buf64);
        ptr8[1] = pixel_select2(buf64);
        ptr8[2] = pixel_select2(buf64);
        ptr8[3] = pixel_select2(buf64);
#else
        for (register uint8_t i = 0; i < 4; i++)
          { ptr8[i] = pixel_select2(buf64); } /* braces required */
#endif
      }
    }
    break;
  } /* /case */

  case 0x0B: // 11
  {
    // 64-color encoding (each pixel in block is a different color)
    register uint8_t* ptr8 = current_frame_buffer + offset;
#ifdef MANUAL_UNROLL
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
    readptr(ptr8, 8); ptr8 += videoStride;
#else
    for (register uint8_t i = 0; i < 8; i++)
    {
      readptr(ptr8, 8);
      ptr8 += videoStride;
    }
#endif
    break;
  } /* /case */

  case 0x0C: // 12
  {
    // 16-color block encoding: each 2x2 block is a different color
    register uint8_t* ptr8 = current_frame_buffer + offset;
    register uint8_t x, y;
    for (y = 0; y < 8; y += 2)
    {
      for (x = 0; x < 8; x += 2)
      {
        ptr8[x + 0] =
        ptr8[x + 1] =
        ptr8[x + 0 + videoStride] =
        ptr8[x + 1 + videoStride] = read8();
      }
      ptr8 += videoStride << 1;
    }
    break;
  } /* /case */

  case 0x0D: // 13
  {
    // 4-color block encoding: each 4x4 block is a different color
    register uint8_t* ptr8 = current_frame_buffer + offset;
#ifdef MANUAL_UNROLL
    readptr(pixel, 2);

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    readptr(pixel, 2);

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;

    memset(ptr8 + 0, pixel[0], 4);
    memset(ptr8 + 4, pixel[1], 4);
    ptr8 += videoStride;
#else
    for (register uint8_t i = 0; i < 8; i++)
    {
      if (!(i & 3))
        readptr(pixel, 2);

      memset(ptr8 + 0, pixel[0], 4);
      memset(ptr8 + 4, pixel[1], 4);
      ptr8 += videoStride;
    }
#endif
    break;
  } /* /case */

  case 0x0E: // 14
  {
    // 1-color encoding : the whole block is 1 solid color
    register uint8_t* ptr8 = current_frame_buffer + offset;
    register uint8_t buf8 = read8();
#ifdef MANUAL_UNROLL
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
    memset(ptr8, buf8, 8); pBuffer += videoStride;
#else
    for (register uint8_t i = 0; i < 8; i++)
    {
      memset(ptr8, buf8, 8);
      ptr8 += videoStride;
    }
#endif
    break;
  } /* /case */

  case 0x0F: // 15
  {
    // dithered encoding
    readptr(pixel, 2);
    register uint8_t* ptr8 = current_frame_buffer + offset;

    for (register uint8_t row = 0; row < 8; row++)
    {
#ifdef MANUAL_UNROLL
      ptr8[0] = pixel_select2(row); row = ~row;
      ptr8[1] = pixel_select2(row); row = ~row;
      ptr8[2] = pixel_select2(row); row = ~row;
      ptr8[3] = pixel_select2(row); row = ~row;
      ptr8[4] = pixel_select2(row); row = ~row;
      ptr8[5] = pixel_select2(row); row = ~row;
      ptr8[6] = pixel_select2(row); row = ~row;
      ptr8[7] = pixel_select2(row); row = ~row;
#else
      for (register uint8_t col = 0; col < 8; col++)
      {
        ptr8[col] = pixel_select2(row);
        row = ~row;
      }
#endif
      ptr8 += videoStride;
    }
    break;
  } /* /case */
  } /* /switch */
}
