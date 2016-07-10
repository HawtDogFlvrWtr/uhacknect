/*
=================================================================================
 Name        : PCD8544.c
 Version     : 0.2

 Copyright (C) 2010 Limor Fried, Adafruit Industries
 CORTEX-M3 version by Le Dang Dung, 2011 LeeDangDung@gmail.com (tested on LPC1769)
 Raspberry Pi version by Andre Wussow, 2012, desk@binerry.de

 Font Fixed and a couple of missing functions added (text colour / size)
    by Andrew Johnson, 2015, andrew.johnson@sappsys.co.uk

 Description :
     A simple PCD8544 LCD (Nokia3310/5110) driver. Target board is Raspberry Pi.
     This driver uses 5 GPIOs on target board with a bit-bang SPI implementation
     (hence, may not be as fast).
	 Makes use of WiringPI-library of Gordon Henderson (https://projects.drogon.net/raspberry-pi/wiringpi/)

	 Recommended connection (http://www.raspberrypi.org/archives/384):
	 LCD pins      Raspberry Pi
	 LCD1 - GND    P06  - GND
	 LCD2 - VCC    P01 - 3.3V
	 LCD3 - CLK    P11 - GPIO0
	 LCD4 - Din    P12 - GPIO1
	 LCD5 - D/C    P13 - GPIO2
	 LCD6 - CS     P15 - GPIO3
	 LCD7 - RST    P16 - GPIO4
	 LCD8 - LED    P01 - 3.3V 

 References  :
 http://www.arduino.cc/playground/Code/PCD8544
 http://ladyada.net/products/nokia5110/
 http://code.google.com/p/meshphone/

================================================================================
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
================================================================================
 */
#include <wiringPi.h>
#include "PCD8544.h"

// An abs() :)
#define abs(a) (((a) < 0) ? -(a) : (a))

// bit set
#define _BV(bit) (0x1 << (bit))

// LCD port variables
static uint8_t cursor_x, cursor_y, textsize, textcolor;
static int8_t _din, _sclk, _dc, _rst, _cs;

// font bitmap

static unsigned char  font[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, // 00
		0x3E, 0x5B, 0x4F, 0x5B, 0x3E, // 01
		0x3E, 0x6B, 0x4F, 0x6B, 0x3E, // 02
		0x1C, 0x3E, 0x7C, 0x3E, 0x1C, // 03 Heart
		0x18, 0x3C, 0x7E, 0x3C, 0x18, // 04 Diamond
		0x1C, 0x57, 0x7D, 0x57, 0x1C, // 05 Club
		0x1C, 0x5E, 0x7F, 0x5E, 0x1C, // 06 Spade
		0x00, 0x18, 0x3C, 0x18, 0x00, // 07
		0xFF, 0xE7, 0xC3, 0xE7, 0xFF, // 08
		0x00, 0x18, 0x24, 0x18, 0x00, // 09
		0xFF, 0xE7, 0xDB, 0xE7, 0xFF, // 0a
		0x30, 0x48, 0x3A, 0x06, 0x0E, // 0b Male
		0x26, 0x29, 0x79, 0x29, 0x26, // 0c Female
		0x40, 0x7F, 0x05, 0x05, 0x07, // 0d Quaver
		0x40, 0x7F, 0x05, 0x25, 0x3F, // 0e Double Quaver
		0x5A, 0x3C, 0xE7, 0x3C, 0x5A, // 0f 
		0x7F, 0x3E, 0x1C, 0x1C, 0x08, // 10 Wedge Right
		0x08, 0x1C, 0x1C, 0x3E, 0x7F, // 11 Wedge Left
		0x14, 0x22, 0x7F, 0x22, 0x14, // 12 Arrow Up-Down
		0x5F, 0x5F, 0x00, 0x5F, 0x5F, // 13
		0x06, 0x09, 0x7F, 0x01, 0x7F, // 14
		0x00, 0x66, 0x89, 0x95, 0x6A, // 15
		0x60, 0x60, 0x60, 0x60, 0x60, // 16
		0x94, 0xA2, 0xFF, 0xA2, 0x94, // 17
		0x08, 0x04, 0x7E, 0x04, 0x08, // 18 Arrow Up
		0x10, 0x20, 0x7E, 0x20, 0x10, // 19 Arrow Down
		0x08, 0x08, 0x2A, 0x1C, 0x08, // 1a Arrow Right
		0x08, 0x1C, 0x2A, 0x08, 0x08, // 1b Arrow Left
		0x1E, 0x10, 0x10, 0x10, 0x10, // 1c
		0x0C, 0x1E, 0x0C, 0x1E, 0x0C, // 1d Arrow Left-Right
		0x30, 0x38, 0x3E, 0x38, 0x30, // 1e Wedge Up
		0x06, 0x0E, 0x3E, 0x0E, 0x06, // 1f Wedge Down
		0x00, 0x00, 0x00, 0x00, 0x00, // 20 space
		0x00, 0x00, 0x5F, 0x00, 0x00, // 21 !
		0x00, 0x07, 0x00, 0x07, 0x00, // 22 "
		0x14, 0x7F, 0x14, 0x7F, 0x14, // 23 #
		0x24, 0x2A, 0x7F, 0x2A, 0x12, // 24 $
		0x23, 0x13, 0x08, 0x64, 0x62, // 25 %
		0x36, 0x49, 0x56, 0x20, 0x50, // 26 &
		0x00, 0x08, 0x07, 0x03, 0x00, // 27 '
		0x00, 0x1C, 0x22, 0x41, 0x00, // 28 (
		0x00, 0x41, 0x22, 0x1C, 0x00, // 29 )
		0x2A, 0x1C, 0x7F, 0x1C, 0x2A, // 2a *
		0x08, 0x08, 0x3E, 0x08, 0x08, // 2b +
		0x00, 0x80, 0x70, 0x30, 0x00, // 2c ,
		0x08, 0x08, 0x08, 0x08, 0x08, // 2d -
		0x00, 0x00, 0x60, 0x60, 0x00, // 2e .
		0x20, 0x10, 0x08, 0x04, 0x02, // 2f /
		0x3E, 0x51, 0x49, 0x45, 0x3E, // 30 0
		0x00, 0x42, 0x7F, 0x40, 0x00, // 31 1
		0x72, 0x49, 0x49, 0x49, 0x46, // 32 2
		0x21, 0x41, 0x49, 0x4D, 0x33, // 33 3
		0x18, 0x14, 0x12, 0x7F, 0x10, // 34 4
		0x27, 0x45, 0x45, 0x45, 0x39, // 35 5
		0x3C, 0x4A, 0x49, 0x49, 0x31, // 36 6
		0x41, 0x21, 0x11, 0x09, 0x07, // 37 7
		0x36, 0x49, 0x49, 0x49, 0x36, // 38 8
		0x46, 0x49, 0x49, 0x29, 0x1E, // 39 9
		0x00, 0x00, 0x14, 0x00, 0x00, // 3a :
		0x00, 0x40, 0x34, 0x00, 0x00, // 3b ;
		0x00, 0x08, 0x14, 0x22, 0x41, // 3c <
		0x14, 0x14, 0x14, 0x14, 0x14, // 3d =
		0x00, 0x41, 0x22, 0x14, 0x08, // 3e >
		0x02, 0x01, 0x59, 0x09, 0x06, // 3f ?
		0x3E, 0x41, 0x5D, 0x59, 0x4E, // 40 @
		0x7C, 0x12, 0x11, 0x12, 0x7C, // 41 A
		0x7F, 0x49, 0x49, 0x49, 0x36, // 42 B
		0x3E, 0x41, 0x41, 0x41, 0x22, // 43 C
		0x7F, 0x41, 0x41, 0x41, 0x3E, // 44 D
		0x7F, 0x49, 0x49, 0x49, 0x41, // 45 E
		0x7F, 0x09, 0x09, 0x09, 0x01, // 46 F
		0x3E, 0x41, 0x41, 0x51, 0x73, // 47 G
		0x7F, 0x08, 0x08, 0x08, 0x7F, // 48 H
		0x00, 0x41, 0x7F, 0x41, 0x00, // 49 I
		0x20, 0x40, 0x41, 0x3F, 0x01, // 4a J
		0x7F, 0x08, 0x14, 0x22, 0x41, // 4b K
		0x7F, 0x40, 0x40, 0x40, 0x40, // 4c L
		0x7F, 0x02, 0x1C, 0x02, 0x7F, // 4d M
		0x7F, 0x04, 0x08, 0x10, 0x7F, // 4e N
		0x3E, 0x41, 0x41, 0x41, 0x3E, // 4f O
		0x7F, 0x09, 0x09, 0x09, 0x06, // 50 P
		0x3E, 0x41, 0x51, 0x21, 0x5E, // 51 Q
		0x7F, 0x09, 0x19, 0x29, 0x46, // 52 R
		0x26, 0x49, 0x49, 0x49, 0x32, // 53 S
		0x03, 0x01, 0x7F, 0x01, 0x03, // 54 T
		0x3F, 0x40, 0x40, 0x40, 0x3F, // 55 U
		0x1F, 0x20, 0x40, 0x20, 0x1F, // 56 V
		0x3F, 0x40, 0x38, 0x40, 0x3F, // 57 W
		0x63, 0x14, 0x08, 0x14, 0x63, // 58 X
		0x03, 0x04, 0x78, 0x04, 0x03, // 59 Y
		0x61, 0x59, 0x49, 0x4D, 0x43, // 5a Z
		0x00, 0x7F, 0x41, 0x41, 0x41, // 5b [
		0x02, 0x04, 0x08, 0x10, 0x20, // 5c backslash
		0x00, 0x41, 0x41, 0x41, 0x7F, // 5d ]
		0x04, 0x02, 0x01, 0x02, 0x04, // 5e ^
		0x40, 0x40, 0x40, 0x40, 0x40, // 5f _
		0x00, 0x03, 0x07, 0x08, 0x00, // 60 `
		0x20, 0x54, 0x54, 0x78, 0x40, // 61 a
		0x7F, 0x28, 0x44, 0x44, 0x38, // 62 b
		0x38, 0x44, 0x44, 0x44, 0x28, // 63 c
		0x38, 0x44, 0x44, 0x28, 0x7F, // 64 d
		0x38, 0x54, 0x54, 0x54, 0x18, // 65 e
		0x00, 0x08, 0x7E, 0x09, 0x02, // 66 f
		0x18, 0xA4, 0xA4, 0x9C, 0x78, // 67 g
		0x7F, 0x08, 0x04, 0x04, 0x78, // 68 h
		0x00, 0x44, 0x7D, 0x40, 0x00, // 69 i
		0x20, 0x40, 0x40, 0x3D, 0x00, // 6a j
		0x7F, 0x10, 0x28, 0x44, 0x00, // 6b k
		0x00, 0x41, 0x7F, 0x40, 0x00, // 6c l
		0x7C, 0x04, 0x78, 0x04, 0x78, // 6d m
		0x7C, 0x08, 0x04, 0x04, 0x78, // 6e n
		0x38, 0x44, 0x44, 0x44, 0x38, // 6f o
		0xFC, 0x18, 0x24, 0x24, 0x18, // 70 p
		0x18, 0x24, 0x24, 0x18, 0xFC, // 71 q
		0x7C, 0x08, 0x04, 0x04, 0x08, // 72 r
		0x48, 0x54, 0x54, 0x54, 0x24, // 73 s
		0x04, 0x04, 0x3F, 0x44, 0x24, // 74 t
		0x3C, 0x40, 0x40, 0x20, 0x7C, // 75 u
		0x1C, 0x20, 0x40, 0x20, 0x1C, // 76 v
		0x3C, 0x40, 0x30, 0x40, 0x3C, // 77 w
		0x44, 0x28, 0x10, 0x28, 0x44, // 78 x
		0x4C, 0x90, 0x90, 0x90, 0x7C, // 79 y
		0x44, 0x64, 0x54, 0x4C, 0x44, // 7a z
		0x00, 0x08, 0x36, 0x41, 0x00, // 7b {
		0x00, 0x00, 0x77, 0x00, 0x00, // 7c |
		0x00, 0x41, 0x36, 0x08, 0x00, // 7d }
		0x02, 0x01, 0x02, 0x04, 0x02, // 7e ~
		0x3C, 0x26, 0x23, 0x26, 0x3C, // 7f House
		0x1E, 0xA1, 0xA1, 0x61, 0x12, // 80
		0x3A, 0x40, 0x40, 0x20, 0x7A, // 81
		0x38, 0x54, 0x54, 0x55, 0x59, // 82
		0x21, 0x55, 0x55, 0x79, 0x41, // 83
		0x21, 0x54, 0x54, 0x78, 0x41, // 84
		0x21, 0x55, 0x54, 0x78, 0x40, // 85
		0x20, 0x54, 0x55, 0x79, 0x40, // 86
		0x0C, 0x1E, 0x52, 0x72, 0x12, // 87
		0x39, 0x55, 0x55, 0x55, 0x59, // 88
		0x39, 0x54, 0x54, 0x54, 0x59, // 89
		0x39, 0x55, 0x54, 0x54, 0x58, // 8a
		0x00, 0x00, 0x45, 0x7C, 0x41, // 8b
		0x00, 0x02, 0x45, 0x7D, 0x42, // 8c
		0x00, 0x01, 0x45, 0x7C, 0x40, // 8d
		0xF0, 0x29, 0x24, 0x29, 0xF0, // 8e
		0xF0, 0x28, 0x25, 0x28, 0xF0, // 8f
		0x7C, 0x54, 0x55, 0x45, 0x00, // 90
		0x20, 0x54, 0x54, 0x7C, 0x54, // 91
		0x7C, 0x0A, 0x09, 0x7F, 0x49, // 92
		0x32, 0x49, 0x49, 0x49, 0x32, // 93
		0x32, 0x48, 0x48, 0x48, 0x32, // 94
		0x32, 0x4A, 0x48, 0x48, 0x30, // 95
		0x3A, 0x41, 0x41, 0x21, 0x7A, // 96
		0x3A, 0x42, 0x40, 0x20, 0x78, // 97
		0x00, 0x9D, 0xA0, 0xA0, 0x7D, // 98
		0x39, 0x44, 0x44, 0x44, 0x39, // 99
		0x3D, 0x40, 0x40, 0x40, 0x3D, // 9a
		0x3C, 0x24, 0xFF, 0x24, 0x24, // 9b
		0x48, 0x7E, 0x49, 0x43, 0x66, // 9c Pound
		0x2B, 0x2F, 0xFC, 0x2F, 0x2B, // 9d
		0xFF, 0x09, 0x29, 0xF6, 0x20, // 9e
		0xC0, 0x88, 0x7E, 0x09, 0x03, // 9f Function
		0x20, 0x54, 0x54, 0x79, 0x41, // a0
		0x00, 0x00, 0x44, 0x7D, 0x41, // a1
		0x30, 0x48, 0x48, 0x4A, 0x32, // a2
		0x38, 0x40, 0x40, 0x22, 0x7A, // a3
		0x00, 0x7A, 0x0A, 0x0A, 0x72, // a4
		0x7D, 0x0D, 0x19, 0x31, 0x7D, // a5
		0x26, 0x29, 0x29, 0x2F, 0x28, // a6
		0x26, 0x29, 0x29, 0x29, 0x26, // a7
		0x30, 0x48, 0x4D, 0x40, 0x20, // a8
		0x38, 0x08, 0x08, 0x08, 0x08, // a9
		0x08, 0x08, 0x08, 0x08, 0x38, // aa
		0x2F, 0x10, 0xC8, 0xAC, 0xBA, // ab Half
		0x2F, 0x10, 0x28, 0x34, 0xFA, // ac Quarter
		0x00, 0x00, 0x7B, 0x00, 0x00, // ad
		0x08, 0x14, 0x2A, 0x14, 0x22, // ae Chevron Left
		0x22, 0x14, 0x2A, 0x14, 0x08, // af Chevron Right
		0xAA, 0x00, 0x55, 0x00, 0xAA, // b0 Light Bloc
		0xAA, 0xAA, 0x55, 0xAA, 0xAA, // b1 Medium Block
		0xAA, 0x55, 0xAA, 0x55, 0xAA, // b2 Heavy Block
		0x00, 0x00, 0x00, 0xFF, 0x00, // b3
		0x10, 0x10, 0x10, 0xFF, 0x00, // b4
		0x14, 0x14, 0x14, 0xFF, 0x00, // b5
		0x10, 0x10, 0xFF, 0x00, 0xFF, // b6
		0x10, 0x10, 0xF0, 0x10, 0xF0, // b7
		0x14, 0x14, 0x14, 0xFC, 0x00, // b8
		0x14, 0x14, 0xF7, 0x00, 0xFF, // b9
		0x00, 0x00, 0xFF, 0x00, 0xFF, // ba
		0x14, 0x14, 0xF4, 0x04, 0xFC, // bb
		0x14, 0x14, 0x17, 0x10, 0x1F, // bc
		0x10, 0x10, 0x1F, 0x10, 0x1F, // bd
		0x14, 0x14, 0x14, 0x1F, 0x00, // be
		0x10, 0x10, 0x10, 0xF0, 0x00, // bf
		0x00, 0x00, 0x00, 0x1F, 0x10, // c0
		0x10, 0x10, 0x10, 0x1F, 0x10, // c1
		0x10, 0x10, 0x10, 0xF0, 0x10, // c2
		0x00, 0x00, 0x00, 0xFF, 0x10, // c3
		0x10, 0x10, 0x10, 0x10, 0x10, // c4
		0x10, 0x10, 0x10, 0xFF, 0x10, // c5
		0x00, 0x00, 0x00, 0xFF, 0x14, // c6
		0x00, 0x00, 0xFF, 0x00, 0xFF, // c7
		0x00, 0x00, 0x1F, 0x10, 0x17, // c8
		0x00, 0x00, 0xFC, 0x04, 0xF4, // c9
		0x14, 0x14, 0x17, 0x10, 0x17, // ca
		0x14, 0x14, 0xF4, 0x04, 0xF4, // cb
		0x00, 0x00, 0xFF, 0x00, 0xF7, // cc
		0x14, 0x14, 0x14, 0x14, 0x14, // cd
		0x14, 0x14, 0xF7, 0x00, 0xF7, // ce
		0x14, 0x14, 0x14, 0x17, 0x14, // cf
		0x10, 0x10, 0x1F, 0x10, 0x1F, // df
		0x14, 0x14, 0x14, 0xF4, 0x14, // d1
		0x10, 0x10, 0xF0, 0x10, 0xF0, // d2
		0x00, 0x00, 0x1F, 0x10, 0x1F, // d3
		0x00, 0x00, 0x00, 0x1F, 0x14, // d4
		0x00, 0x00, 0x00, 0xFC, 0x14, // d5
		0x00, 0x00, 0xF0, 0x10, 0xF0, // d6
		0x10, 0x10, 0xFF, 0x10, 0xFF, // d7
		0x14, 0x14, 0x14, 0xFF, 0x14, // d8
		0x10, 0x10, 0x10, 0x1F, 0x00, // d9
		0x00, 0x00, 0x00, 0xF0, 0x10, // da
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // db
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // dc
		0xFF, 0xFF, 0xFF, 0x00, 0x00, // dd
		0x00, 0x00, 0x00, 0xFF, 0xFF, // de
		0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // df
		0x38, 0x44, 0x44, 0x38, 0x44, // e0 Alpha
		0x7C, 0x2A, 0x2A, 0x3E, 0x14, // e1 Beta
		0x7E, 0x02, 0x02, 0x06, 0x06, // e2
		0x02, 0x7E, 0x02, 0x7E, 0x02, // e3 Pi
		0x63, 0x55, 0x49, 0x41, 0x63, // e4 E
		0x38, 0x44, 0x44, 0x3C, 0x04, // e5
		0x40, 0x7E, 0x20, 0x1E, 0x20, // e6
		0x06, 0x02, 0x7E, 0x02, 0x02, // e7
		0x99, 0xA5, 0xE7, 0xA5, 0x99, // e8
		0x1C, 0x2A, 0x49, 0x2A, 0x1C, // e9
		0x4C, 0x72, 0x01, 0x72, 0x4C, // ea Ohm
		0x30, 0x4A, 0x4D, 0x4D, 0x30, // eb
		0x30, 0x48, 0x78, 0x48, 0x30, // ec Infinity
		0xBC, 0x62, 0x5A, 0x46, 0x3D, // ed
		0x3E, 0x49, 0x49, 0x49, 0x00, // ee
		0x7E, 0x01, 0x01, 0x01, 0x7E, // ef
		0x2A, 0x2A, 0x2A, 0x2A, 0x2A, // f0
		0x44, 0x44, 0x5F, 0x44, 0x44, // f1
		0x40, 0x51, 0x4A, 0x44, 0x40, // f2 Greater/Equal
		0x40, 0x44, 0x4A, 0x51, 0x40, // f3 Less/Equal
		0x00, 0x00, 0xFF, 0x01, 0x03, // f4
		0xE0, 0x80, 0xFF, 0x00, 0x00, // f5
		0x08, 0x08, 0x6B, 0x6B, 0x08, // f6 Divide
		0x36, 0x12, 0x36, 0x24, 0x36, // f7
		0x06, 0x0F, 0x09, 0x0F, 0x06, // f8
		0x00, 0x00, 0x18, 0x18, 0x00, // f9
		0x00, 0x00, 0x10, 0x10, 0x00, // fa
		0x30, 0x40, 0xFF, 0x01, 0x01, // fb Sqr Root
//		0x00, 0x1F, 0x01, 0x01, 0x1E, // fc
//		0x00, 0x19, 0x1D, 0x17, 0x12, // fd
		0x88, 0x50, 0x20, 0x50, 0x88, // fc Cross
		0x20, 0x40, 0x20, 0x10, 0x08, // fd Tick
		0x00, 0x3C, 0x3C, 0x3C, 0x3C, // fe
		0x00, 0x00, 0x00, 0x00, 0x00, // ff NULL
};
// 0x88, 0x50, 0x20, 0x50, 0x88 Cross
// 0x20, 0x40, 0x20, 0x10, 0x08 Tick

// the memory buffer for the LCD
uint8_t pcd8544_buffer[LCDWIDTH * LCDHEIGHT / 8] = {0,};

// Le: get the bitmap assistance here! : http://en.radzio.dxp.pl/bitmap_converter/
// Andre: or here! : http://www.henningkarlsen.com/electronics/t_imageconverter_mono.php
const uint8_t pi_logo [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0010 (16) pixels

0x00, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x0020 (32) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF,   // 0x0030 (48) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x0040 (64) pixels

0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0050 (80) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0060 (96) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x0070 (112) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0080 (128) pixels

0x00, 0x01, 0x0F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x0090 (144) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x00A0 (160) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x00B0 (176) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x00C0 (192) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00,   // 0x00D0 (208) pixels

0xC0, 0xFC, 0xFC, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x00E0 (224) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x00F0 (240) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0100 (256) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,   // 0x0110 (272) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x0F, 0x03, 0x00, 0x00, 0x00,   // 0x0120 (288) pixels

0x00, 0x00, 0x00, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,   // 0x0130 (304) pixels

0x0F, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,   // 0x0140 (320) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0150 (336) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x0160 (352) pixels

0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x8F, 0x81, 0x80, 0x80,   // 0x0170 (368) pixels

0x80, 0x80, 0x80, 0xE0, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC,   // 0x0180 (384) pixels

0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x87, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x0190 (400) pixels

0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x01A0 (416) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x01B0 (432) pixels

0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x01C0 (448) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x01D0 (464) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   // 0x01E0 (480) pixels

0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0x01F0 (496) pixels
};

// reduces how much is refreshed, which speeds it up!
// originally derived from Steve Evans/JCW's mod but cleaned up and optimized
//#define enablePartialUpdate

static void my_setpixel(uint8_t x, uint8_t y, uint8_t color)
{
	if ((x >= LCDWIDTH) || (y >= LCDHEIGHT))
		return;
	// x is which column
	if (color)
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] |= _BV(y%8);
	else
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] &= ~_BV(y%8);
}

// Set the text colour. 1 is Black on White, 0 is White on Black
void LCDsetTextColor(uint8_t color)
{
        textcolor = color;
}
// Set the text spacing
void LCDsetTextSize(uint8_t siz)
{
        textsize = siz;
}

void LCDshowLogo()
{
	uint32_t i;
	for (i = 0; i < LCDWIDTH * LCDHEIGHT / 8; i++  )
	{
		pcd8544_buffer[i] = pi_logo[i];
	}
	LCDdisplay();
}

#ifdef enablePartialUpdate
static uint8_t xUpdateMin, xUpdateMax, yUpdateMin, yUpdateMax;
#endif

static void updateBoundingBox(uint8_t xmin, uint8_t ymin, uint8_t xmax, uint8_t ymax) {
#ifdef enablePartialUpdate
	if (xmin < xUpdateMin) xUpdateMin = xmin;
	if (xmax > xUpdateMax) xUpdateMax = xmax;
	if (ymin < yUpdateMin) yUpdateMin = ymin;
	if (ymax > yUpdateMax) yUpdateMax = ymax;
#endif
}

void LCDInit(uint8_t SCLK, uint8_t DIN, uint8_t DC, uint8_t CS, uint8_t RST, uint8_t contrast)
{
	_din = DIN;
	_sclk = SCLK;
	_dc = DC;
	_rst = RST;
	_cs = CS;
	cursor_x = cursor_y = 0;
	textsize = 1;
	textcolor = BLACK;

	// set pin directions
	pinMode(_din, OUTPUT);
	pinMode(_sclk, OUTPUT);
	pinMode(_dc, OUTPUT);
	pinMode(_rst, OUTPUT);
	pinMode(_cs, OUTPUT);

	// toggle RST low to reset; CS low so it'll listen to us
	if (_cs > 0)
		digitalWrite(_cs, LOW);

	digitalWrite(_rst, LOW);
	_delay_ms(500);
	digitalWrite(_rst, HIGH);

	// get into the EXTENDED mode!
	LCDcommand(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );

	// LCD bias select (4 is optimal?)
	LCDcommand(PCD8544_SETBIAS | 0x4);

	// set VOP
	if (contrast > 0x7f)
		contrast = 0x7f;

	LCDcommand( PCD8544_SETVOP | contrast); // Experimentally determined

	// normal mode
	LCDcommand(PCD8544_FUNCTIONSET);

	// Set display to Normal
	LCDcommand(PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);

	// set up a bounding box for screen updates
	updateBoundingBox(0, 0, LCDWIDTH-1, LCDHEIGHT-1);

}

void LCDdrawbitmap(uint8_t x, uint8_t y,const uint8_t *bitmap, uint8_t w, uint8_t h,uint8_t color)
{
	uint8_t j,i;
	for ( j=0; j<h; j++)
	{
		for ( i=0; i<w; i++ )
		{
			if (*(bitmap + i + (j/8)*w) & _BV(j%8))
			{
				my_setpixel(x+i, y+j, color);
			}
		}
	}
	updateBoundingBox(x, y, x+w, y+h);
}

void LCDdrawstring(uint8_t x, uint8_t y, char *c)
{
	cursor_x = x;
	cursor_y = y;
	while (*c)
	{
		LCDwrite(*c++);
	}
}

void LCDdrawstring_P(uint8_t x, uint8_t y, const char *str)
{
	cursor_x = x;
	cursor_y = y;
	while (1)
	{
		char c = (*str++);
		if (! c)
			return;
		LCDwrite(c);
	}
}

void LCDdrawchar(uint8_t x, uint8_t y, char c)
{
	if (y >= LCDHEIGHT) return;
	if ((x+5) >= LCDWIDTH) return;
	uint8_t i,j;
	for ( i =0; i<5; i++ )
	{
		uint8_t d = *(font+(c*5)+i);
		uint8_t j;
		for (j = 0; j<8; j++)
		{
			if (d & _BV(j))
			{
				my_setpixel(x+i, y+j, textcolor);
			}
			else
			{
				my_setpixel(x+i, y+j, !textcolor);
			}
		}
	}

	for ( j = 0; j<8; j++)
	{
		my_setpixel(x+5, y+j, !textcolor);
	}
	updateBoundingBox(x, y, x+5, y + 8);
}

void LCDwrite(uint8_t c)
{
	if (c == '\n')
	{
		cursor_y += textsize*8;
		cursor_x = 0;
	}
	else if (c == '\r')
	{
		// skip em
	}
	else
	{
		LCDdrawchar(cursor_x, cursor_y, c);
		cursor_x += textsize*6;
		if (cursor_x >= (LCDWIDTH-5))
		{
			cursor_x = 0;
			cursor_y+=8;
		}
		if (cursor_y >= LCDHEIGHT)
			cursor_y = 0;
	}
}

void LCDsetCursor(uint8_t x, uint8_t y)
{
	cursor_x = x;
	cursor_y = y;
}

// bresenham's algorithm - thx wikpedia
void LCDdrawline(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
	uint8_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep)
	{
		swap(x0, y0);
		swap(x1, y1);
	}

	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}

	// much faster to put the test here, since we've already sorted the points
	updateBoundingBox(x0, y0, x1, y1);

	uint8_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int8_t err = dx / 2;
	int8_t ystep;

	if (y0 < y1)
	{
		ystep = 1;
	} else
	{
		ystep = -1;
	}

	for (; x0<=x1; x0++)
	{
		if (steep)
		{
			my_setpixel(y0, x0, color);
		}
		else
		{
			my_setpixel(x0, y0, color);
		}
		err -= dy;
		if (err < 0)
		{
			y0 += ystep;
			err += dx;
		}
	}
}

// filled rectangle
void LCDfillrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,  uint8_t color)
{
	// stupidest version - just pixels - but fast with internal buffer!
	uint8_t i,j;
	for ( i=x; i<x+w; i++)
	{
		for ( j=y; j<y+h; j++)
		{
			my_setpixel(i, j, color);
		}
	}
	updateBoundingBox(x, y, x+w, y+h);
}

// draw a rectangle
void LCDdrawrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
	// stupidest version - just pixels - but fast with internal buffer!
	uint8_t i;
	for ( i=x; i<x+w; i++) {
		my_setpixel(i, y, color);
		my_setpixel(i, y+h-1, color);
	}
	for ( i=y; i<y+h; i++) {
		my_setpixel(x, i, color);
		my_setpixel(x+w-1, i, color);
	}

	updateBoundingBox(x, y, x+w, y+h);
}

// draw a circle outline
void LCDdrawcircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
	updateBoundingBox(x0-r, y0-r, x0+r, y0+r);

	int8_t f = 1 - r;
	int8_t ddF_x = 1;
	int8_t ddF_y = -2 * r;
	int8_t x = 0;
	int8_t y = r;

	my_setpixel(x0, y0+r, color);
	my_setpixel(x0, y0-r, color);
	my_setpixel(x0+r, y0, color);
	my_setpixel(x0-r, y0, color);

	while (x<y)
	{
		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		my_setpixel(x0 + x, y0 + y, color);
		my_setpixel(x0 - x, y0 + y, color);
		my_setpixel(x0 + x, y0 - y, color);
		my_setpixel(x0 - x, y0 - y, color);

		my_setpixel(x0 + y, y0 + x, color);
		my_setpixel(x0 - y, y0 + x, color);
		my_setpixel(x0 + y, y0 - x, color);
		my_setpixel(x0 - y, y0 - x, color);

	}
}

void LCDfillcircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
	updateBoundingBox(x0-r, y0-r, x0+r, y0+r);
	int8_t f = 1 - r;
	int8_t ddF_x = 1;
	int8_t ddF_y = -2 * r;
	int8_t x = 0;
	int8_t y = r;
	uint8_t i;

	for (i=y0-r; i<=y0+r; i++)
	{
		my_setpixel(x0, i, color);
	}

	while (x<y)
	{
		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		for ( i=y0-y; i<=y0+y; i++)
		{
			my_setpixel(x0+x, i, color);
			my_setpixel(x0-x, i, color);
		}
		for ( i=y0-x; i<=y0+x; i++)
		{
			my_setpixel(x0+y, i, color);
			my_setpixel(x0-y, i, color);
		}
	}
}

// the most basic function, set a single pixel
void LCDsetPixel(uint8_t x, uint8_t y, uint8_t color)
{
	if ((x >= LCDWIDTH) || (y >= LCDHEIGHT))
		return;

	// x is which column
	if (color)
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] |= _BV(y%8);
	else
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] &= ~_BV(y%8);
	updateBoundingBox(x,y,x,y);
}

// the most basic function, get a single pixel
uint8_t LCDgetPixel(uint8_t x, uint8_t y)
{
	if ((x >= LCDWIDTH) || (y >= LCDHEIGHT))
		return 0;

	return (pcd8544_buffer[x+ (y/8)*LCDWIDTH] >> (7-(y%8))) & 0x1;
}

void LCDspiwrite(uint8_t c)
{
	digitalWrite(_cs, LOW);  //bugfix

	shiftOut(_din, _sclk, MSBFIRST, c);

	digitalWrite(_cs, HIGH); //bugfix


}

void LCDcommand(uint8_t c)
{
	digitalWrite( _dc, LOW);
	LCDspiwrite(c);
}

void LCDdata(uint8_t c)
{
	digitalWrite(_dc, HIGH);
	LCDspiwrite(c);
}

void LCDsetContrast(uint8_t val)
{
	if (val > 0x7f) {
		val = 0x7f;
	}
	LCDcommand(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );
	LCDcommand( PCD8544_SETVOP | val);
	LCDcommand(PCD8544_FUNCTIONSET);
}

void LCDdisplay(void)
{
	uint8_t col, maxcol, p;

	for(p = 0; p < 6; p++)
	{
#ifdef enablePartialUpdate
		// check if this page is part of update
		if ( yUpdateMin >= ((p+1)*8) )
		{
			continue;   // nope, skip it!
		}
		if (yUpdateMax < p*8)
		{
			break;
		}
#endif

		LCDcommand(PCD8544_SETYADDR | p);


#ifdef enablePartialUpdate
		col = xUpdateMin;
		maxcol = xUpdateMax;
#else
		// start at the beginning of the row
		col = 0;
		maxcol = LCDWIDTH-1;
#endif

		LCDcommand(PCD8544_SETXADDR | col);

		for(; col <= maxcol; col++) {
			//uart_putw_dec(col);
			//uart_putchar(' ');
			LCDdata(pcd8544_buffer[(LCDWIDTH*p)+col]);
		}
	}

	LCDcommand(PCD8544_SETYADDR );  // no idea why this is necessary but it is to finish the last byte?
#ifdef enablePartialUpdate
	xUpdateMin = LCDWIDTH - 1;
	xUpdateMax = 0;
	yUpdateMin = LCDHEIGHT-1;
	yUpdateMax = 0;
#endif

}

// clear everything
void LCDclear(void) {
	//memset(pcd8544_buffer, 0, LCDWIDTH*LCDHEIGHT/8);
	uint32_t i;
	for ( i = 0; i < LCDWIDTH*LCDHEIGHT/8 ; i++)
		pcd8544_buffer[i] = 0;
	updateBoundingBox(0, 0, LCDWIDTH-1, LCDHEIGHT-1);
	cursor_y = cursor_x = 0;
}

// bitbang serial shift out on select GPIO pin. Data rate is defined by CPU clk speed and CLKCONST_2. 
// Calibrate these value for your need on target platform.
void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
	uint8_t i;
	uint32_t j;

	for (i = 0; i < 8; i++)  {
		if (bitOrder == LSBFIRST)
			digitalWrite(dataPin, !!(val & (1 << i)));
		else
			digitalWrite(dataPin, !!(val & (1 << (7 - i))));

		digitalWrite(clockPin, HIGH);
		for (j = CLKCONST_2; j > 0; j--); // clock speed, anyone? (LCD Max CLK input: 4MHz)
		digitalWrite(clockPin, LOW);
	}
}

// roughly calibrated spin delay
void _delay_ms(uint32_t t)
{
	uint32_t nCount = 0;
	while (t != 0)
	{
		nCount = CLKCONST_1;
		while(nCount != 0)
			nCount--;
		t--;
	}
}