#include "mve.h"

#include <stdint.h>
#include <unistd.h>

#include <stddef.h>
#include <stdlib.h>

#include <string.h>


#include "fileio.h"


extern void decode_block(uint8_t op_id, intptr_t offset);

#define CHUNK_PREAMBLE_SIZE 4
#define OPCODE_PREAMBLE_SIZE 4

#define CHUNK_INIT_AUDIO   0x0000
#define CHUNK_AUDIO_ONLY   0x0001
#define CHUNK_INIT_VIDEO   0x0002
#define CHUNK_VIDEO        0x0003
#define CHUNK_SHUTDOWN     0x0004
#define CHUNK_END          0x0005
/* these last types are used internally */
#define CHUNK_DONE         0xFFFC
#define CHUNK_NOMEM        0xFFFD
#define CHUNK_EOF          0xFFFE
#define CHUNK_BAD          0xFFFF

#define OPCODE_END_OF_STREAM           0x00
#define OPCODE_END_OF_CHUNK            0x01
#define OPCODE_CREATE_TIMER            0x02
#define OPCODE_INIT_AUDIO_BUFFERS      0x03
#define OPCODE_START_STOP_AUDIO        0x04
#define OPCODE_INIT_VIDEO_BUFFERS      0x05
#define OPCODE_UNKNOWN_06              0x06
#define OPCODE_SEND_BUFFER             0x07
#define OPCODE_AUDIO_FRAME             0x08
#define OPCODE_SILENCE_FRAME           0x09
#define OPCODE_INIT_VIDEO_MODE         0x0A
#define OPCODE_CREATE_GRADIENT         0x0B
#define OPCODE_SET_PALETTE             0x0C
#define OPCODE_SET_PALETTE_COMPRESSED  0x0D
#define OPCODE_UNKNOWN_0E              0x0E
#define OPCODE_SET_DECODING_MAP        0x0F
#define OPCODE_UNKNOWN_10              0x10
#define OPCODE_VIDEO_DATA              0x11
#define OPCODE_UNKNOWN_12              0x12
#define OPCODE_UNKNOWN_13              0x13
#define OPCODE_UNKNOWN_14              0x14
#define OPCODE_UNKNOWN_15              0x15

#define PALETTE_COUNT 256

#define kMVETile    (MAXTILES-1)
#define kMVEPal     5
#define kMVETile    (MAXTILES-1)
#define kMVEPal     5

#define kAudioBlocks    20 // alloc a lot of blocks - need to store lots of audio data before video frames start.

struct AudioBlock
{
    int16_t buf[6000];
    uint32_t size;
};

struct AudioData
{
    int hFx;
    int nChannels;
    uint16_t nSampleRate;
    uint8_t nBitDepth;

    struct AudioBlock block[kAudioBlocks];
    int nWrite;
    int nRead;
};

struct DecodeMap
{
    uint8_t* pData;
    uint32_t nSize;
};

struct Palette
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static const int16_t delta_table[] = {
         0,      1,      2,      3,      4,      5,      6,      7,
         8,      9,     10,     11,     12,     13,     14,     15,
        16,     17,     18,     19,     20,     21,     22,     23,
        24,     25,     26,     27,     28,     29,     30,     31,
        32,     33,     34,     35,     36,     37,     38,     39,
        40,     41,     42,     43,     47,     51,     56,     61,
        66,     72,     79,     86,     94,    102,    112,    122,
       133,    145,    158,    173,    189,    206,    225,    245,
       267,    292,    318,    348,    379,    414,    452,    493,
       538,    587,    640,    699,    763,    832,    908,    991,
      1081,   1180,   1288,   1405,   1534,   1673,   1826,   1993,
      2175,   2373,   2590,   2826,   3084,   3365,   3672,   4008,
      4373,   4772,   5208,   5683,   6202,   6767,   7385,   8059,
      8794,   9597,  10472,  11428,  12471,  13609,  14851,  16206,
     17685,  19298,  21060,  22981,  25078,  27367,  29864,  32589,
    -29973, -26728, -23186, -19322, -15105, -10503,  -5481,     -1,
         1,      1,   5481,  10503,  15105,  19322,  23186,  26728,
     29973, -32589, -29864, -27367, -25078, -22981, -21060, -19298,
    -17685, -16206, -14851, -13609, -12471, -11428, -10472,  -9597,
     -8794,  -8059,  -7385,  -6767,  -6202,  -5683,  -5208,  -4772,
     -4373,  -4008,  -3672,  -3365,  -3084,  -2826,  -2590,  -2373,
     -2175,  -1993,  -1826,  -1673,  -1534,  -1405,  -1288,  -1180,
     -1081,   -991,   -908,   -832,   -763,   -699,   -640,   -587,
      -538,   -493,   -452,   -414,   -379,   -348,   -318,   -292,
      -267,   -245,   -225,   -206,   -189,   -173,   -158,   -145,
      -133,   -122,   -112,   -102,    -94,    -86,    -79,    -72,
       -66,    -61,    -56,    -51,    -47,    -43,    -42,    -41,
       -40,    -39,    -38,    -37,    -36,    -35,    -34,    -33,
       -32,    -31,    -30,    -29,    -28,    -27,    -26,    -25,
       -24,    -23,    -22,    -21,    -20,    -19,    -18,    -17,
       -16,    -15,    -14,    -13,    -12,    -11,    -10,     -9,
        -8,     -7,     -6,     -5,     -4,     -3,     -2,     -1
};

intptr_t videoStride = 0;
uint8_t* current_frame_buffer = NULL;
uint8_t* previous_frame_buffer = NULL;
struct AudioData audio;
struct DecodeMap decodeMap;
struct Palette palette[256];

uint32_t nTimerRate, nTimerDiv;
uint32_t nWidth, nHeight, nFrame;
double nFps;
uint32_t nFrameDuration;
bool bIsPlaying, bAudioStarted;


void buffer_swap(void)
{
    uint8_t* t = previous_frame_buffer;
    previous_frame_buffer = current_frame_buffer;
    current_frame_buffer = t;
}


// macro to fetch 16-bit little-endian words from a bytestream
#define LE_16(x)  ((*x) | ((*(x+1)) << 8))

int ClipRange(int val, int min, int max)
{
    if (val < min)
        return min;
    else if (val > max)
        return max;
    else
        return val;
}

void mve_init(void)
{
    bIsPlaying = false;
    bAudioStarted = false;

    nWidth  = 0;
    nHeight = 0;
    nFrame  = 0;

    memset(palette, 0, sizeof(palette));

    for (int i = 0; i < kAudioBlocks; i++) {
        memset(audio.block[i].buf, 0, sizeof(audio.block[i].buf));
        audio.block[i].size = 0;
    }

    nFps = 0.0;
    nFrameDuration = 0;
    nTimerRate = 0;
    nTimerDiv  = 0;

    audio.nChannels   = 0;
    audio.nSampleRate = 0;
    audio.nBitDepth   = 0;
    audio.nRead  = 0;
    audio.nWrite = 0;
    audio.hFx = 0;

    current_frame_buffer = NULL;
    previous_frame_buffer = NULL;

    decodeMap.pData = NULL;
    decodeMap.nSize = 0;

    videoStride = 0;
}

void mve_deinit(void)
{
    mve_close();

    if (decodeMap.pData != NULL)
      free(decodeMap.pData);

    current_frame_buffer = NULL;
    previous_frame_buffer = NULL;
}

bool mve_play(void)
{
    uint8_t chunkPreamble[CHUNK_PREAMBLE_SIZE];
    uint8_t opcodePreamble[OPCODE_PREAMBLE_SIZE];
    uint8_t opcodeType;
    uint8_t opcodeVersion;
    int opcodeSize, chunkSize;
    int chunkType = 0;

    auto const oyxaspect = yxaspect;

    int nScale = tabledivide32(scale(65536, ydim << 2, xdim * 3), ((max(nHeight, 240 + 1u) + 239) / 240));
    int nStat = 2|4|8|64|1024;
    renderSetAspect(viewingrange, 65536);

    uint32_t nNextFrameTime = (uint32_t)totalclock + nFrameDuration;

    bIsPlaying = true;

    // iterate through the chunks in the file
    while (chunkType != CHUNK_END && bIsPlaying)
    {

        // handle timing - wait until we're ready to process the next frame.
        if (nNextFrameTime > (uint32_t)totalclock) {
            continue;
        }
        else {
            nNextFrameTime = (uint32_t)totalclock + nFrameDuration;
        }

        readptr(&chunkPreamble, CHUNK_PREAMBLE_SIZE);

        chunkSize = LE_16(&chunkPreamble[0]);
        chunkType = LE_16(&chunkPreamble[2]);

        switch (chunkType)
        {
            case CHUNK_INIT_AUDIO:
                break;
            case CHUNK_AUDIO_ONLY:
                break;
            case CHUNK_INIT_VIDEO:
                break;
            case CHUNK_VIDEO:
                break;
            case CHUNK_SHUTDOWN:
                break;
            case CHUNK_END:
                bIsPlaying = false;
                break;
            default:
                break;
        }

        // iterate through individual opcodes
        while (chunkSize > 0)
        {
            handleevents();

            if (KB_KeyWaiting() || BUTTON(gamefunc_Fire)
                || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_A))
                || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_START)))
            {
                renderSetAspect(viewingrange, oyxaspect);
                Close();
                return true;
            }

            if (file.ReadBytes(opcodePreamble, OPCODE_PREAMBLE_SIZE) != OPCODE_PREAMBLE_SIZE)
            {
                initprintf("InterplayDecoder: could not read from file (EOF?)\n");
                return false;
            }

            opcodeSize = LE_16(&opcodePreamble[0]);
            opcodeType = opcodePreamble[2];
            opcodeVersion = opcodePreamble[3];

            chunkSize -= OPCODE_PREAMBLE_SIZE;
            chunkSize -= opcodeSize;

            switch (opcodeType)
            {
            case OPCODE_END_OF_STREAM:
            {
                file.Skip(opcodeSize);
                break;
            }

            case OPCODE_END_OF_CHUNK:
            {
                file.Skip(opcodeSize);
                break;
            }

            case OPCODE_CREATE_TIMER:
            {
                nTimerRate = read32LE();
                nTimerDiv  = read16LE();
                nFps = 1000000.0f / ((double)nTimerRate * nTimerDiv);
                nFrameDuration = 120.0f / nFps;
                break;
            }

            case OPCODE_INIT_AUDIO_BUFFERS:
            {
                file.Skip(2);
                uint16_t flags = read16LE();
                audio.nSampleRate = read16LE();

                uint32_t nBufferBytes;

                if (opcodeVersion == 0) {
                    nBufferBytes = read16LE();
                }
                else {
                    nBufferBytes = read32LE();
                }

                if (flags & 0x1) {
                    audio.nChannels = 2;
                }
                else {
                    audio.nChannels = 1;
                }
                if (flags & 0x2) {
                    audio.nBitDepth = 16;
                }
                else {
                    audio.nBitDepth = 8;
                }
                break;
            }

            case OPCODE_START_STOP_AUDIO:
            {
                if (!bAudioStarted)
                {
                    // start audio playback
                    audio.hFx = FX_StartDemandFeedPlayback(ServeAudioSample, audio.nBitDepth, audio.nChannels, audio.nSampleRate, 0, 128, 128, 128, FX_MUSIC_PRIORITY, fix16_one, -1, this);
                    bAudioStarted = true;
                }

                file.Skip(opcodeSize);
                break;
            }

            case OPCODE_INIT_VIDEO_BUFFERS:
            {
                assert(opcodeSize == 8);
                nWidth  = read16LE() * 8;
                nHeight = read16LE() * 8;

                int count = read16LE();
                int truecolour = read16LE();
                assert(truecolour == 0);

                current_frame_buffer = new uint8_t[nWidth * nHeight];
                previous_frame_buffer = new uint8_t[nWidth * nHeight];

                videoStride = nWidth;

                uint8_t* pFrame = (uint8_t*)Xmalloc(nWidth * nHeight);
                memset(pFrame, 0, nWidth * nHeight);
                walock[kMVETile] = CACHE1D_PERMANENT;
                waloff[kMVETile] = (intptr_t)pFrame;
                tileSetSize(kMVETile, nHeight, nWidth);
                break;
            }

            case OPCODE_UNKNOWN_06:
            case OPCODE_UNKNOWN_0E:
            case OPCODE_UNKNOWN_10:
            case OPCODE_UNKNOWN_12:
            case OPCODE_UNKNOWN_13:
            case OPCODE_UNKNOWN_14:
            case OPCODE_UNKNOWN_15:
            {
                file.Skip(opcodeSize);
                break;
            }

            case OPCODE_SEND_BUFFER:
            {
                int nPalStart = read16LE();
                int nPalCount = read16LE();

                memcpy((char*)waloff[kMVETile], current_frame_buffer, nWidth * nHeight);
                tileInvalidate(kMVETile, -1, -1);

                nFrame++;
                buffer_swap();

                file.Skip(opcodeSize - 4);
                break;
            }

            case OPCODE_AUDIO_FRAME:
            {
                int nStart = file.GetPosition();
                uint16_t seqIndex   = read16LE();
                uint16_t streamMask = read16LE();
                uint16_t nSamples   = read16LE(); // number of samples this chunk

                int predictor[2];
                int i = 0;


                int16_t* pBuf = audio.block[audio.nWrite].buf;

                for (int ch = 0; ch < audio.nChannels; ch++)
                {
                    predictor[ch] = read16LE();
                    i++;

                    if (predictor[ch] & 0x8000) {
                        predictor[ch] |= 0xFFFF0000; // sign extend
                    }

                    *pBuf++ = predictor[ch];
                }

                int ch = 0;
                for (; i < (nSamples / 2); i++)
                {
                    predictor[ch] += delta_table[read8()];
                    predictor[ch] = ClipRange(predictor[ch], -32768, 32768);

                    *pBuf++ = predictor[ch];

                    // toggle channel
                    ch ^= audio.nChannels - 1;
                }

                audio.block[audio.nWrite].size = nSamples / 2;
                audio.nWrite++;

                if (audio.nWrite >= kAudioBlocks)
                    audio.nWrite = 0;

                int nEnd = file.GetPosition();
                int nRead = nEnd - nStart;
                assert(opcodeSize == nRead);

                mutex_unlock(&mutex);
                break;
            }

            case OPCODE_SILENCE_FRAME:
            {
                uint16_t seqIndex = read16LE();
                uint16_t streamMask = read16LE();
                uint16_t nStreamLen = read16LE();
                break;
            }

            case OPCODE_INIT_VIDEO_MODE:
            {
                file.Skip(opcodeSize);
                break;
            }

            case OPCODE_CREATE_GRADIENT:
            {
                file.Skip(opcodeSize);
                initprintf("InterplayDecoder: Create gradient not supported.\n");
                break;
            }

            case OPCODE_SET_PALETTE:
            {
                if (opcodeSize > 0x304 || opcodeSize < 4) {
                    printf("set_palette opcode with invalid size\n");
                    chunkType = CHUNK_BAD;
                    break;
                }

                int nPalStart = read16LE();
                int nPalCount = read16LE();

                for (int i = nPalStart; i <= nPalCount; i++)
                {
                    palette[i].r = read8() << 2;
                    palette[i].g = read8() << 2;
                    palette[i].b = read8() << 2;
                }

                paletteSetColorTable(kMVEPal, (uint8_t*)palette);
                videoSetPalette(0, kMVEPal, 0);
                break;
            }

            case OPCODE_SET_PALETTE_COMPRESSED:
            {
                file.Skip(opcodeSize);
                initprintf("InterplayDecoder: Set palette compressed not supported.\n");
                break;
            }

            case OPCODE_SET_DECODING_MAP:
            {
                if (!decodeMap.pData)
                  free(decodeMap.pData);
                decodeMap.pData = malloc(opcodeSize);
                decodeMap.nSize = opcodeSize;
                readptr(decodeMap.pData, decodeMap.nSize);
                break;
            }

            case OPCODE_VIDEO_DATA:
            {
                int nStart = file.GetPosition();

                // need to skip 14 bytes
                skipnbytes(14);

                if (decodeMap.nSize)
                {
                    int i = 0;
    
                    for (uint32_t y = 0; y < nHeight; y += 8)
                    {
                        for (uint32_t x = 0; x < nWidth; x += 8)
                        {
                            uint32_t opcode;

                            // alternate between getting low and high 4 bits
                            if (i & 1) {
                                opcode = decodeMap.pData[i >> 1] >> 4;
                            }
                            else {
                                opcode = decodeMap.pData[i >> 1] & 0x0F;
                            }
                            i++;

                            int32_t offset = x + (y * videoStride);
                            if(opcode > 16)
                              break;

                            decode_block(opcode, offset);
                        }
                    }
                }

                int nEnd = file.GetPosition();
                int nSkipBytes = opcodeSize - (nEnd - nStart); // we can end up with 1 byte left we need to skip
                assert(nSkipBytes <= 1);

                file.Skip(nSkipBytes);
                break;
            }

            default:
                break;
            }
        }

        videoClearScreen(0);

        rotatesprite_fs(160 << 16, 100 << 16, nScale, 512, kMVETile, 0, 0, nStat);
        videoNextPage();
    }

    renderSetAspect(viewingrange, oyxaspect);

    bIsPlaying = false;

    return true;
}
