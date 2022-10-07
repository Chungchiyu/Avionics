// I2Cdev library collection - MPU6050 I2C device class, 6-axis MotionApps 2.0
// implementation Based on InvenSense MPU-6050 register map document rev. 2.0,
// 5/19/2011 (RM-MPU-6000A-00) 5/20/2013 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at
// https://github.com/jrowberg/i2cdevlib
//
// Changelog:
// 2019/07/08 - merged all DMP Firmware configuration items into the dmpMemory
// array
//            - Simplified dmpInitialize() to accomidate the dmpmemory array
//            alterations
//     ... - ongoing debug release

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2012 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#ifndef _MPU6050_6AXIS_MOTIONAPPS20_H_
#define _MPU6050_6AXIS_MOTIONAPPS20_H_

#include "I2Cdev.h"
#include "helper_3dmath.h"

// MotionApps 2.0 DMP implementation, built using the MPU-6050EVB evaluation
// board
#define MPU6050_INCLUDE_DMP_MOTIONAPPS20

#include "MPU6050_dev.h"

// Tom Carpenter's conditional PROGMEM code
// http://forum.arduino.cc/index.php?topic=129407.0
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
// Teensy 3.0 library conditional PROGMEM code from Paul Stoffregen
#ifndef __PGMSPACE_H_
#define __PGMSPACE_H_ 1
#include <inttypes.h>

#define PROGMEM
#define PGM_P const char *
#define PSTR(str) (str)
#define F(x) x

typedef void prog_void;
typedef char prog_char;
typedef unsigned char prog_uchar;
typedef int8_t prog_int8_t;
typedef uint8_t prog_uint8_t;
typedef int16_t prog_int16_t;
typedef uint16_t prog_uint16_t;
typedef int32_t prog_int32_t;
typedef uint32_t prog_uint32_t;

#define strcpy_P(dest, src) strcpy((dest), (src))
#define strcat_P(dest, src) strcat((dest), (src))
#define strcmp_P(a, b) strcmp((a), (b))

#define pgm_read_byte(addr) (*(const unsigned char *) (addr))
#define pgm_read_word(addr) (*(const unsigned short *) (addr))
#define pgm_read_dword(addr) (*(const unsigned long *) (addr))
#define pgm_read_float(addr) (*(const float *) (addr))

#define pgm_read_byte_near(addr) pgm_read_byte(addr)
#define pgm_read_word_near(addr) pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_float_near(addr) pgm_read_float(addr)
#define pgm_read_byte_far(addr) pgm_read_byte(addr)
#define pgm_read_word_far(addr) pgm_read_word(addr)
#define pgm_read_dword_far(addr) pgm_read_dword(addr)
#define pgm_read_float_far(addr) pgm_read_float(addr)
#endif
#endif

/* Source is from the InvenSense MotionApps v2 demo code. Original source is
 * unavailable, unless you happen to be amazing as decompiling binary by
 * hand (in which case, please contact me, and I'm totally serious).
 *
 * Also, I'd like to offer many, many thanks to Noah Zerkin for all of the
 * DMP reverse-engineering he did to help make this bit of wizardry
 * possible.
 */

// NOTE! Enabling DEBUG adds about 3.3kB to the flash program size.
// Debug output is now working even on ATMega328P MCUs (e.g. Arduino Uno)
// after moving string constants to flash memory storage using the F()
// compiler macro (Arduino IDE 1.0+ required).

//#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTF(x, y) Serial.print(x, y)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLNF(x, y) Serial.println(x, y)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTF(x, y)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLNF(x, y)
#endif

#define MPU6050_DMP_CODE_SIZE 1929   // dmpMemory[]
#define MPU6050_DMP_CONFIG_SIZE 192  // dmpConfig[]
#define MPU6050_DMP_UPDATES_SIZE 47  // dmpUpdates[]

/* ================================================================================================
 * | Default MotionApps v2.0 42-byte FIFO packet structure: | | | | [QUAT W][
 ][QUAT X][      ][QUAT Y][      ][QUAT Z][      ][GYRO X][      ][GYRO Y][ ] |
 |   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18
 19  20  21  22  23  | | | | [GYRO Z][      ][ACC X ][      ][ACC Y ][ ][ACC Z
 ][      ][      ]                         | |  24  25  26  27  28  29  30  31
 32  33  34  35  36  37  38  39  40  41                          |
 * ================================================================================================ */

// this block of memory gets written to the MPU on start-up, and it seems
// to be volatile memory, so it has to be done each time (it only takes ~1
// second though)

// I Only Changed this by applying all the configuration data and capturing it
// before startup:
// *** this is a capture of the DMP Firmware after all the messy changes were
// made so we can just load it
const unsigned char dmpMemory[MPU6050_DMP_CODE_SIZE] PROGMEM = {
    /* bank # 0 */
    0xFB,
    0x00,
    0x00,
    0x3E,
    0x00,
    0x0B,
    0x00,
    0x36,
    0x00,
    0x01,
    0x00,
    0x02,
    0x00,
    0x03,
    0x00,
    0x00,
    0x00,
    0x65,
    0x00,
    0x54,
    0xFF,
    0xEF,
    0x00,
    0x00,
    0xFA,
    0x80,
    0x00,
    0x0B,
    0x12,
    0x82,
    0x00,
    0x01,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x00,
    0xFF,
    0xFF,
    0x45,
    0x81,
    0xFF,
    0xFF,
    0xFA,
    0x72,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0xE8,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x7F,
    0xFF,
    0xFF,
    0xFE,
    0x80,
    0x01,
    0x00,
    0x1B,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x02,
    0xCB,
    0x47,
    0xA2,
    0x20,
    0x00,
    0x00,
    0x00,
    0x20,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x60,
    0x00,
    0x00,
    0x00,
    0x41,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0B,
    0x2A,
    0x00,
    0x00,
    0x16,
    0x55,
    0x00,
    0x00,
    0x21,
    0x82,
    0xFD,
    0x87,
    0x26,
    0x50,
    0xFD,
    0x80,
    0x00,
    0x00,
    0x00,
    0x1F,
    0x00,
    0x00,
    0x00,
    0x05,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x04,
    0x6F,
    0x00,
    0x02,
    0x65,
    0x32,
    0x00,
    0x00,
    0x5E,
    0xC0,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xFB,
    0x8C,
    0x6F,
    0x5D,
    0xFD,
    0x5D,
    0x08,
    0xD9,
    0x00,
    0x7C,
    0x73,
    0x3B,
    0x00,
    0x6C,
    0x12,
    0xCC,
    0x32,
    0x00,
    0x13,
    0x9D,
    0x32,
    0x00,
    0xD0,
    0xD6,
    0x32,
    0x00,
    0x08,
    0x00,
    0x40,
    0x00,
    0x01,
    0xF4,
    0xFF,
    0xE6,
    0x80,
    0x79,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD0,
    0xD6,
    0x00,
    0x00,
    0x27,
    0x10,
    /* bank # 1 */
    0xFB,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xFA,
    0x36,
    0xFF,
    0xBC,
    0x30,
    0x8E,
    0x00,
    0x05,
    0xFB,
    0xF0,
    0xFF,
    0xD9,
    0x5B,
    0xC8,
    0xFF,
    0xD0,
    0x9A,
    0xBE,
    0x00,
    0x00,
    0x10,
    0xA9,
    0xFF,
    0xF4,
    0x1E,
    0xB2,
    0x00,
    0xCE,
    0xBB,
    0xF7,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x04,
    0x00,
    0x02,
    0x00,
    0x02,
    0x02,
    0x00,
    0x00,
    0x0C,
    0xFF,
    0xC2,
    0x80,
    0x00,
    0x00,
    0x01,
    0x80,
    0x00,
    0x00,
    0xCF,
    0x80,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x14,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x09,
    0x23,
    0xA1,
    0x35,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x3F,
    0x68,
    0xB6,
    0x79,
    0x35,
    0x28,
    0xBC,
    0xC6,
    0x7E,
    0xD1,
    0x6C,
    0x80,
    0x00,
    0xFF,
    0xFF,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB2,
    0x6A,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3F,
    0xF0,
    0x00,
    0x00,
    0x00,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x25,
    0x4D,
    0x00,
    0x2F,
    0x70,
    0x6D,
    0x00,
    0x00,
    0x05,
    0xAE,
    0x00,
    0x0C,
    0x02,
    0xD0,
    /* bank # 2 */
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x65,
    0x00,
    0x54,
    0xFF,
    0xEF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x44,
    0x00,
    0x01,
    0x00,
    0x05,
    0x8B,
    0xC1,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x65,
    0x00,
    0x00,
    0x00,
    0x54,
    0x00,
    0x00,
    0xFF,
    0xEF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1B,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1B,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    /* bank # 3 */
    0xD8,
    0xDC,
    0xBA,
    0xA2,
    0xF1,
    0xDE,
    0xB2,
    0xB8,
    0xB4,
    0xA8,
    0x81,
    0x91,
    0xF7,
    0x4A,
    0x90,
    0x7F,
    0x91,
    0x6A,
    0xF3,
    0xF9,
    0xDB,
    0xA8,
    0xF9,
    0xB0,
    0xBA,
    0xA0,
    0x80,
    0xF2,
    0xCE,
    0x81,
    0xF3,
    0xC2,
    0xF1,
    0xC1,
    0xF2,
    0xC3,
    0xF3,
    0xCC,
    0xA2,
    0xB2,
    0x80,
    0xF1,
    0xC6,
    0xD8,
    0x80,
    0xBA,
    0xA7,
    0xDF,
    0xDF,
    0xDF,
    0xF2,
    0xA7,
    0xC3,
    0xCB,
    0xC5,
    0xB6,
    0xF0,
    0x87,
    0xA2,
    0x94,
    0x24,
    0x48,
    0x70,
    0x3C,
    0x95,
    0x40,
    0x68,
    0x34,
    0x58,
    0x9B,
    0x78,
    0xA2,
    0xF1,
    0x83,
    0x92,
    0x2D,
    0x55,
    0x7D,
    0xD8,
    0xB1,
    0xB4,
    0xB8,
    0xA1,
    0xD0,
    0x91,
    0x80,
    0xF2,
    0x70,
    0xF3,
    0x70,
    0xF2,
    0x7C,
    0x80,
    0xA8,
    0xF1,
    0x01,
    0xB0,
    0x98,
    0x87,
    0xD9,
    0x43,
    0xD8,
    0x86,
    0xC9,
    0x88,
    0xBA,
    0xA1,
    0xF2,
    0x0E,
    0xB8,
    0x97,
    0x80,
    0xF1,
    0xA9,
    0xDF,
    0xDF,
    0xDF,
    0xAA,
    0xDF,
    0xDF,
    0xDF,
    0xF2,
    0xAA,
    0x4C,
    0xCD,
    0x6C,
    0xA9,
    0x0C,
    0xC9,
    0x2C,
    0x97,
    0x97,
    0x97,
    0x97,
    0xF1,
    0xA9,
    0x89,
    0x26,
    0x46,
    0x66,
    0xB0,
    0xB4,
    0xBA,
    0x80,
    0xAC,
    0xDE,
    0xF2,
    0xCA,
    0xF1,
    0xB2,
    0x8C,
    0x02,
    0xA9,
    0xB6,
    0x98,
    0x00,
    0x89,
    0x0E,
    0x16,
    0x1E,
    0xB8,
    0xA9,
    0xB4,
    0x99,
    0x2C,
    0x54,
    0x7C,
    0xB0,
    0x8A,
    0xA8,
    0x96,
    0x36,
    0x56,
    0x76,
    0xF1,
    0xB9,
    0xAF,
    0xB4,
    0xB0,
    0x83,
    0xC0,
    0xB8,
    0xA8,
    0x97,
    0x11,
    0xB1,
    0x8F,
    0x98,
    0xB9,
    0xAF,
    0xF0,
    0x24,
    0x08,
    0x44,
    0x10,
    0x64,
    0x18,
    0xF1,
    0xA3,
    0x29,
    0x55,
    0x7D,
    0xAF,
    0x83,
    0xB5,
    0x93,
    0xAF,
    0xF0,
    0x00,
    0x28,
    0x50,
    0xF1,
    0xA3,
    0x86,
    0x9F,
    0x61,
    0xA6,
    0xDA,
    0xDE,
    0xDF,
    0xD9,
    0xFA,
    0xA3,
    0x86,
    0x96,
    0xDB,
    0x31,
    0xA6,
    0xD9,
    0xF8,
    0xDF,
    0xBA,
    0xA6,
    0x8F,
    0xC2,
    0xC5,
    0xC7,
    0xB2,
    0x8C,
    0xC1,
    0xB8,
    0xA2,
    0xDF,
    0xDF,
    0xDF,
    0xA3,
    0xDF,
    0xDF,
    0xDF,
    0xD8,
    0xD8,
    0xF1,
    0xB8,
    0xA8,
    0xB2,
    0x86,
    /* bank # 4 */
    0xB4,
    0x98,
    0x0D,
    0x35,
    0x5D,
    0xB8,
    0xAA,
    0x98,
    0xB0,
    0x87,
    0x2D,
    0x35,
    0x3D,
    0xB2,
    0xB6,
    0xBA,
    0xAF,
    0x8C,
    0x96,
    0x19,
    0x8F,
    0x9F,
    0xA7,
    0x0E,
    0x16,
    0x1E,
    0xB4,
    0x9A,
    0xB8,
    0xAA,
    0x87,
    0x2C,
    0x54,
    0x7C,
    0xB9,
    0xA3,
    0xDE,
    0xDF,
    0xDF,
    0xA3,
    0xB1,
    0x80,
    0xF2,
    0xC4,
    0xCD,
    0xC9,
    0xF1,
    0xB8,
    0xA9,
    0xB4,
    0x99,
    0x83,
    0x0D,
    0x35,
    0x5D,
    0x89,
    0xB9,
    0xA3,
    0x2D,
    0x55,
    0x7D,
    0xB5,
    0x93,
    0xA3,
    0x0E,
    0x16,
    0x1E,
    0xA9,
    0x2C,
    0x54,
    0x7C,
    0xB8,
    0xB4,
    0xB0,
    0xF1,
    0x97,
    0x83,
    0xA8,
    0x11,
    0x84,
    0xA5,
    0x09,
    0x98,
    0xA3,
    0x83,
    0xF0,
    0xDA,
    0x24,
    0x08,
    0x44,
    0x10,
    0x64,
    0x18,
    0xD8,
    0xF1,
    0xA5,
    0x29,
    0x55,
    0x7D,
    0xA5,
    0x85,
    0x95,
    0x02,
    0x1A,
    0x2E,
    0x3A,
    0x56,
    0x5A,
    0x40,
    0x48,
    0xF9,
    0xF3,
    0xA3,
    0xD9,
    0xF8,
    0xF0,
    0x98,
    0x83,
    0x24,
    0x08,
    0x44,
    0x10,
    0x64,
    0x18,
    0x97,
    0x82,
    0xA8,
    0xF1,
    0x11,
    0xF0,
    0x98,
    0xA2,
    0x24,
    0x08,
    0x44,
    0x10,
    0x64,
    0x18,
    0xDA,
    0xF3,
    0xDE,
    0xD8,
    0x83,
    0xA5,
    0x94,
    0x01,
    0xD9,
    0xA3,
    0x02,
    0xF1,
    0xA2,
    0xC3,
    0xC5,
    0xC7,
    0xD8,
    0xF1,
    0x84,
    0x92,
    0xA2,
    0x4D,
    0xDA,
    0x2A,
    0xD8,
    0x48,
    0x69,
    0xD9,
    0x2A,
    0xD8,
    0x68,
    0x55,
    0xDA,
    0x32,
    0xD8,
    0x50,
    0x71,
    0xD9,
    0x32,
    0xD8,
    0x70,
    0x5D,
    0xDA,
    0x3A,
    0xD8,
    0x58,
    0x79,
    0xD9,
    0x3A,
    0xD8,
    0x78,
    0x93,
    0xA3,
    0x4D,
    0xDA,
    0x2A,
    0xD8,
    0x48,
    0x69,
    0xD9,
    0x2A,
    0xD8,
    0x68,
    0x55,
    0xDA,
    0x32,
    0xD8,
    0x50,
    0x71,
    0xD9,
    0x32,
    0xD8,
    0x70,
    0x5D,
    0xDA,
    0x3A,
    0xD8,
    0x58,
    0x79,
    0xD9,
    0x3A,
    0xD8,
    0x78,
    0xA8,
    0x8A,
    0x9A,
    0xF0,
    0x28,
    0x50,
    0x78,
    0x9E,
    0xF3,
    0x88,
    0x18,
    0xF1,
    0x9F,
    0x1D,
    0x98,
    0xA8,
    0xD9,
    0x08,
    0xD8,
    0xC8,
    0x9F,
    0x12,
    0x9E,
    0xF3,
    0x15,
    0xA8,
    0xDA,
    0x12,
    0x10,
    0xD8,
    0xF1,
    0xAF,
    0xC8,
    0x97,
    0x87,
    /* bank # 5 */
    0x34,
    0xB5,
    0xB9,
    0x94,
    0xA4,
    0x21,
    0xF3,
    0xD9,
    0x22,
    0xD8,
    0xF2,
    0x2D,
    0xF3,
    0xD9,
    0x2A,
    0xD8,
    0xF2,
    0x35,
    0xF3,
    0xD9,
    0x32,
    0xD8,
    0x81,
    0xA4,
    0x60,
    0x60,
    0x61,
    0xD9,
    0x61,
    0xD8,
    0x6C,
    0x68,
    0x69,
    0xD9,
    0x69,
    0xD8,
    0x74,
    0x70,
    0x71,
    0xD9,
    0x71,
    0xD8,
    0xB1,
    0xA3,
    0x84,
    0x19,
    0x3D,
    0x5D,
    0xA3,
    0x83,
    0x1A,
    0x3E,
    0x5E,
    0x93,
    0x10,
    0x30,
    0x81,
    0x10,
    0x11,
    0xB8,
    0xB0,
    0xAF,
    0x8F,
    0x94,
    0xF2,
    0xDA,
    0x3E,
    0xD8,
    0xB4,
    0x9A,
    0xA8,
    0x87,
    0x29,
    0xDA,
    0xF8,
    0xD8,
    0x87,
    0x9A,
    0x35,
    0xDA,
    0xF8,
    0xD8,
    0x87,
    0x9A,
    0x3D,
    0xDA,
    0xF8,
    0xD8,
    0xB1,
    0xB9,
    0xA4,
    0x98,
    0x85,
    0x02,
    0x2E,
    0x56,
    0xA5,
    0x81,
    0x00,
    0x0C,
    0x14,
    0xA3,
    0x97,
    0xB0,
    0x8A,
    0xF1,
    0x2D,
    0xD9,
    0x28,
    0xD8,
    0x4D,
    0xD9,
    0x48,
    0xD8,
    0x6D,
    0xD9,
    0x68,
    0xD8,
    0xB1,
    0x84,
    0x0D,
    0xDA,
    0x0E,
    0xD8,
    0xA3,
    0x29,
    0x83,
    0xDA,
    0x2C,
    0x0E,
    0xD8,
    0xA3,
    0x84,
    0x49,
    0x83,
    0xDA,
    0x2C,
    0x4C,
    0x0E,
    0xD8,
    0xB8,
    0xB0,
    0xA8,
    0x8A,
    0x9A,
    0xF5,
    0x20,
    0xAA,
    0xDA,
    0xDF,
    0xD8,
    0xA8,
    0x40,
    0xAA,
    0xD0,
    0xDA,
    0xDE,
    0xD8,
    0xA8,
    0x60,
    0xAA,
    0xDA,
    0xD0,
    0xDF,
    0xD8,
    0xF1,
    0x97,
    0x86,
    0xA8,
    0x31,
    0x9B,
    0x06,
    0x99,
    0x07,
    0xAB,
    0x97,
    0x28,
    0x88,
    0x9B,
    0xF0,
    0x0C,
    0x20,
    0x14,
    0x40,
    0xB8,
    0xB0,
    0xB4,
    0xA8,
    0x8C,
    0x9C,
    0xF0,
    0x04,
    0x28,
    0x51,
    0x79,
    0x1D,
    0x30,
    0x14,
    0x38,
    0xB2,
    0x82,
    0xAB,
    0xD0,
    0x98,
    0x2C,
    0x50,
    0x50,
    0x78,
    0x78,
    0x9B,
    0xF1,
    0x1A,
    0xB0,
    0xF0,
    0x8A,
    0x9C,
    0xA8,
    0x29,
    0x51,
    0x79,
    0x8B,
    0x29,
    0x51,
    0x79,
    0x8A,
    0x24,
    0x70,
    0x59,
    0x8B,
    0x20,
    0x58,
    0x71,
    0x8A,
    0x44,
    0x69,
    0x38,
    0x8B,
    0x39,
    0x40,
    0x68,
    0x8A,
    0x64,
    0x48,
    0x31,
    0x8B,
    0x30,
    0x49,
    0x60,
    0xA5,
    0x88,
    0x20,
    0x09,
    0x71,
    0x58,
    0x44,
    0x68,
    /* bank # 6 */
    0x11,
    0x39,
    0x64,
    0x49,
    0x30,
    0x19,
    0xF1,
    0xAC,
    0x00,
    0x2C,
    0x54,
    0x7C,
    0xF0,
    0x8C,
    0xA8,
    0x04,
    0x28,
    0x50,
    0x78,
    0xF1,
    0x88,
    0x97,
    0x26,
    0xA8,
    0x59,
    0x98,
    0xAC,
    0x8C,
    0x02,
    0x26,
    0x46,
    0x66,
    0xF0,
    0x89,
    0x9C,
    0xA8,
    0x29,
    0x51,
    0x79,
    0x24,
    0x70,
    0x59,
    0x44,
    0x69,
    0x38,
    0x64,
    0x48,
    0x31,
    0xA9,
    0x88,
    0x09,
    0x20,
    0x59,
    0x70,
    0xAB,
    0x11,
    0x38,
    0x40,
    0x69,
    0xA8,
    0x19,
    0x31,
    0x48,
    0x60,
    0x8C,
    0xA8,
    0x3C,
    0x41,
    0x5C,
    0x20,
    0x7C,
    0x00,
    0xF1,
    0x87,
    0x98,
    0x19,
    0x86,
    0xA8,
    0x6E,
    0x76,
    0x7E,
    0xA9,
    0x99,
    0x88,
    0x2D,
    0x55,
    0x7D,
    0x9E,
    0xB9,
    0xA3,
    0x8A,
    0x22,
    0x8A,
    0x6E,
    0x8A,
    0x56,
    0x8A,
    0x5E,
    0x9F,
    0xB1,
    0x83,
    0x06,
    0x26,
    0x46,
    0x66,
    0x0E,
    0x2E,
    0x4E,
    0x6E,
    0x9D,
    0xB8,
    0xAD,
    0x00,
    0x2C,
    0x54,
    0x7C,
    0xF2,
    0xB1,
    0x8C,
    0xB4,
    0x99,
    0xB9,
    0xA3,
    0x2D,
    0x55,
    0x7D,
    0x81,
    0x91,
    0xAC,
    0x38,
    0xAD,
    0x3A,
    0xB5,
    0x83,
    0x91,
    0xAC,
    0x2D,
    0xD9,
    0x28,
    0xD8,
    0x4D,
    0xD9,
    0x48,
    0xD8,
    0x6D,
    0xD9,
    0x68,
    0xD8,
    0x8C,
    0x9D,
    0xAE,
    0x29,
    0xD9,
    0x04,
    0xAE,
    0xD8,
    0x51,
    0xD9,
    0x04,
    0xAE,
    0xD8,
    0x79,
    0xD9,
    0x04,
    0xD8,
    0x81,
    0xF3,
    0x9D,
    0xAD,
    0x00,
    0x8D,
    0xAE,
    0x19,
    0x81,
    0xAD,
    0xD9,
    0x01,
    0xD8,
    0xF2,
    0xAE,
    0xDA,
    0x26,
    0xD8,
    0x8E,
    0x91,
    0x29,
    0x83,
    0xA7,
    0xD9,
    0xAD,
    0xAD,
    0xAD,
    0xAD,
    0xF3,
    0x2A,
    0xD8,
    0xD8,
    0xF1,
    0xB0,
    0xAC,
    0x89,
    0x91,
    0x3E,
    0x5E,
    0x76,
    0xF3,
    0xAC,
    0x2E,
    0x2E,
    0xF1,
    0xB1,
    0x8C,
    0x5A,
    0x9C,
    0xAC,
    0x2C,
    0x28,
    0x28,
    0x28,
    0x9C,
    0xAC,
    0x30,
    0x18,
    0xA8,
    0x98,
    0x81,
    0x28,
    0x34,
    0x3C,
    0x97,
    0x24,
    0xA7,
    0x28,
    0x34,
    0x3C,
    0x9C,
    0x24,
    0xF2,
    0xB0,
    0x89,
    0xAC,
    0x91,
    0x2C,
    0x4C,
    0x6C,
    0x8A,
    0x9B,
    0x2D,
    0xD9,
    0xD8,
    0xD8,
    0x51,
    0xD9,
    0xD8,
    0xD8,
    0x79,
    /* bank # 7 */
    0xD9,
    0xD8,
    0xD8,
    0xF1,
    0x9E,
    0x88,
    0xA3,
    0x31,
    0xDA,
    0xD8,
    0xD8,
    0x91,
    0x2D,
    0xD9,
    0x28,
    0xD8,
    0x4D,
    0xD9,
    0x48,
    0xD8,
    0x6D,
    0xD9,
    0x68,
    0xD8,
    0xB1,
    0x83,
    0x93,
    0x35,
    0x3D,
    0x80,
    0x25,
    0xDA,
    0xD8,
    0xD8,
    0x85,
    0x69,
    0xDA,
    0xD8,
    0xD8,
    0xB4,
    0x93,
    0x81,
    0xA3,
    0x28,
    0x34,
    0x3C,
    0xF3,
    0xAB,
    0x8B,
    0xF8,
    0xA3,
    0x91,
    0xB6,
    0x09,
    0xB4,
    0xD9,
    0xAB,
    0xDE,
    0xFA,
    0xB0,
    0x87,
    0x9C,
    0xB9,
    0xA3,
    0xDD,
    0xF1,
    0x20,
    0x28,
    0x30,
    0x38,
    0x9A,
    0xF1,
    0x28,
    0x30,
    0x38,
    0x9D,
    0xF1,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xF2,
    0xA3,
    0xB4,
    0x90,
    0x80,
    0xF2,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xB2,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xB0,
    0x87,
    0xB5,
    0x99,
    0xF1,
    0x28,
    0x30,
    0x38,
    0x98,
    0xF1,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0x97,
    0xA3,
    0xA3,
    0xA3,
    0xA3,
    0xF3,
    0x9B,
    0xA3,
    0x30,
    0xDC,
    0xB9,
    0xA7,
    0xF1,
    0x26,
    0x26,
    0x26,
    0xFE,
    0xD8,
    0xFF,

};

#ifndef MPU6050_DMP_FIFO_RATE_DIVISOR
#define MPU6050_DMP_FIFO_RATE_DIVISOR \
    0x01  // The New instance of the Firmware has this as the default
#endif



#endif /* _MPU6050_6AXIS_MOTIONAPPS20_H_ */
