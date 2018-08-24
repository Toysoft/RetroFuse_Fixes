/* vega.c: Vega+ specific routines
   Copyright (c) 2016-2018 Retro Computers Ltd

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "fuse.h"
#include "display.h"
#include "screenshot.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "settings.h"

int pCs = 32+21;
int pClk = 32+19;
int pMosi = 32+20;
int gpioCs, gpioClk, gpioMosi;

char gamePokes[255] = "";

void setGPIODirection(int gpioNum, int dir)
{
        char buf[64];
        sprintf(&buf[0], "/sys/class/gpio/gpio%d/direction", gpioNum);

//      printf("\nUsing %s", &buf[0]);
        int gpio = open(buf, O_WRONLY);
        if (gpio) {
            if (dir == 1)
                write(gpio, "in", 2);
            else
                write(gpio, "out", 3);
            close(gpio);
        }
        else
            printf("Error writing direction %d %d\n", gpioNum, dir);
}

void spi_write(unsigned int val)
{
    unsigned int mask;

    write(gpioMosi, "0", 1);
    write(gpioClk, "0", 1);
    write(gpioCs, "0", 1);

    for (mask = 0x00800000; mask != 0; mask >>= 1) {
        write(gpioClk, "0", 1);
        if (val & mask)
            write(gpioMosi, "1", 1);
        else
            write(gpioMosi, "0", 1);

        write(gpioClk, "1", 1);
    }

    usleep(10);
    write(gpioMosi, "0", 1);
    write(gpioClk, "0", 1);
    write(gpioCs, "1", 1);
}

int init_pinmux_spi()
{
    int ret = -1;
    char buf[64];
    int gpio;

    gpio = open("/sys/class/gpio/export", O_WRONLY);
    if (gpio > 0) {
        sprintf(buf, "52");
        write(gpio, buf, strlen(buf));
        close(gpio);
    }
    else
        printf("Error exporting gpio52\n");
    setGPIODirection(pMosi, 0);
    gpioMosi = open("/sys/class/gpio/gpio52/value", O_RDWR);
    if (gpioMosi > 0)
        write(gpioMosi, "0", 1);
    else
        printf ("Error opening gpio52\n");

    gpio = open("/sys/class/gpio/export", O_WRONLY);
    if (gpio > 0) {
        sprintf(buf, "51");
        write(gpio, buf, strlen(buf));
        close(gpio);
    }
    else
        printf("Error exporting gpio51\n");
    setGPIODirection(pClk, 0);
    gpioClk = open("/sys/class/gpio/gpio51/value", O_RDWR);
    if (gpioClk > 0)
        write(gpioClk, "0", 1);
    else
        printf ("Error opening gpio51\n");

    gpio = open("/sys/class/gpio/export", O_WRONLY);
    if (gpio > 0) {
        sprintf(buf, "53");
        write(gpio, buf, strlen(buf));
        close(gpio);
    }
    else
        printf("Error exporting gpio53\n");
    setGPIODirection(pCs, 0);
    gpioCs = open("/sys/class/gpio/gpio53/value", O_RDWR);
    if (gpioCs > 0)
        write(gpioCs, "1", 1);
    else
        printf ("Error opening gpio53\n");

    return 0;
}

void write_reg(unsigned short reg, unsigned short val)
{
//    pr_debug("%s: writing %x to %x\n", __func__, reg, val);
    spi_write(0x00700000 | reg);
    spi_write(0x00720000 | val);
}

void init_panel_hw(unsigned short arg)
{
    int i;
    const unsigned short seq[] = {
        0x01, 0x6300,
//      0x01, 0x2100,
        0x02, 0x0200,
        0x03, 0x8286,
        0x04, 0x04c7,
        0x05, 0xa800,
        0x08, 0x06ff,
        0x0a, arg,
        0x0b, 0xd400,
        0x0d, 0x3229,
        0x0E, 0x1200,
        0x0f, 0x0000,
        0x16, 0x9f80,
        0x17, 0x2212,
        0x1e, 0x00fc,
        0x30, 0x0000,
        0x31, 0x0707,
        0x32, 0x0206,
        0x33, 0x0001,
        0x34, 0x0105,
        0x35, 0x0000,
        0x36, 0x0707,
        0x37, 0x0100,
        0x3a, 0x0502,
        0x3b, 0x0502
    };

    for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i += 2)
        write_reg(seq[i], seq[i + 1]);
}

void vegaSetLCD(char brightness, char contrast)
{
    init_pinmux_spi();
    unsigned short r;
    r = brightness;
    r = r << 8;
    r += contrast;
    printf("\nsetLCD %x | %x -> %x", brightness, contrast, r);
    init_panel_hw(r);
}

void vegaSetLed(int greenOrRed, int onOrOff)
{
    char buf[64];
    int gpio;

    gpio = open("/sys/class/gpio/export", O_WRONLY);
    if (gpio > 0) {
        if (greenOrRed == 0)
            sprintf(buf, "16");
        else
            sprintf(buf, "17");
        write(gpio, buf, strlen(buf));
        close(gpio);
    }    
    if (greenOrRed == 0)
        setGPIODirection(16, 0);
    else
        setGPIODirection(17, 0);

    if (greenOrRed == 0)
        gpio = open("/sys/class/gpio/gpio16/value", O_RDWR);
    else
        gpio = open("/sys/class/gpio/gpio17/value", O_RDWR);

    if (gpio > 0) {
        if (onOrOff == 0)
            write(gpio, "0", 1);
        else
            write(gpio, "1", 1);
        close(gpio);
    }
}

void vegaSetBacklight(int offOrOn)
{
    char buf[64];
    int gpio;

    gpio = open("/sys/class/gpio/export", O_WRONLY);
    if (gpio > 0) {
        sprintf(buf, "60");
        write(gpio, buf, strlen(buf));
        close(gpio);
    }
    setGPIODirection(60, 0);
    gpio = open("/sys/class/gpio/gpio60/value", O_RDWR);
    if (gpio > 0) {
        if (offOrOn == 0)
            write(gpio, "0", 1);
        else
            write(gpio, "1", 1);
        close(gpio);
    }
}

#define MAX_VOLUME 7
int audioState = 0;
int currentDACVolume = 7;
int DACvolumeLevel[] = { 0x2000000, 0x29e009e, 0x2ae00ae, 0x2be00be, 0x2ce00ce, 0x2de00de, 0x2ee00ee, 0x2fe00fe };


int vegaGetDACVolume()
{
    return currentDACVolume;
}

void vegaSetDefaultVolume()
{
    audioState = 0;
    currentDACVolume = 4;
    printf("\nDAC Volume default %x %d", DACvolumeLevel[currentDACVolume], currentDACVolume);
#ifdef __arm__
    vegaWriteReg(0x80048000, 0x30, DACvolumeLevel[currentDACVolume]);
    vegaWriteReg(0x80048000, 0x74, 0x1); // Disable HP
    vegaWriteReg(0x80048000, 0x78, 0x1000000); // Enable Speakers
#endif
}

void vegaDACVolumeDown()
{
    if (currentDACVolume > 0)
        currentDACVolume--;
    printf("\nDAC Volume down %x %d", DACvolumeLevel[currentDACVolume], currentDACVolume);
#ifdef __arm__
    vegaWriteReg(0x80048000, 0x30, DACvolumeLevel[currentDACVolume]);
#endif
}

void vegaDACVolumeUp()
{
    if (currentDACVolume < MAX_VOLUME)
        currentDACVolume++;
    printf("\nDAC Volume up %x %d", DACvolumeLevel[currentDACVolume], currentDACVolume);
#ifdef __arm__
    vegaWriteReg(0x80048000, 0x30, DACvolumeLevel[currentDACVolume]);
#endif
}

void vegaSetAudio(int status)
{
#ifdef __arm__
    if (status == 0) {
        vegaWriteReg(0x80048000, 0x74, 0x1); // Disable HP
        vegaWriteReg(0x80048000, 0x78, 0x1000000); // Enable Speakers
    }
    else {
        vegaWriteReg(0x80048000, 0x74, 0x1000000); // Disable Speakers
        vegaWriteReg(0x80048000, 0x78, 0x1); // Enable HP
    }
#endif
    audioState = status;
    printf("\nAudio now %d", audioState);
    fflush(stdout);
}

int vegaGetAudio()
{
//    if (vegaReadReg(0x80048000, 0x70) & 1)
//        return 1;
//    return 0;
    return audioState;
}


#define MAX_BRIGHTNESS_LEVEL 7
#define MAX_CONTRAST_LEVEL 7

char currentBrightnessLevel = MAX_BRIGHTNESS_LEVEL;
int currentContrastLevel = MAX_CONTRAST_LEVEL;

char contrastLevel[] = { 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10 };
char brightnessLevel[] = { 0x8, 0x10, 0x17, 0x20, 0x27, 0x30, 0x37, 0x40 };

int vegaGetBrightnessLevel()
{
    return currentBrightnessLevel;
}

void vegaBrightnessUp()
{
    if (currentBrightnessLevel < MAX_BRIGHTNESS_LEVEL)
        currentBrightnessLevel++;
    printf("\Brightness level %x, %d", brightnessLevel[currentBrightnessLevel], currentBrightnessLevel);
    vegaSetLCD(brightnessLevel[currentBrightnessLevel], contrastLevel[currentContrastLevel]);
}

void vegaBrightnessDown()
{
    if (currentBrightnessLevel > 0)
        currentBrightnessLevel--;
    printf("\Brightness level %x, %d", brightnessLevel[currentBrightnessLevel], currentBrightnessLevel);
    vegaSetLCD(brightnessLevel[currentBrightnessLevel], contrastLevel[currentContrastLevel]);
}

int vegaGetContrastLevel()
{
    return currentContrastLevel;
}

void vegaContrastUp()
{
    if (currentContrastLevel < MAX_CONTRAST_LEVEL)
        currentContrastLevel++;
    printf("\Contrast level %x, %d", contrastLevel[currentContrastLevel], currentContrastLevel);
    vegaSetLCD(brightnessLevel[currentBrightnessLevel], contrastLevel[currentContrastLevel]);
}

void vegaContrastDown()
{
    if (currentContrastLevel > 0)
        currentContrastLevel--;
    printf("\Contrast level %x, %d", contrastLevel[currentContrastLevel], currentContrastLevel);
    vegaSetLCD(brightnessLevel[currentBrightnessLevel], contrastLevel[currentContrastLevel]);
}

void vegaSetPokes(char *str)
{
    if (str)
        strcpy(&gamePokes[0], str);
    else
        gamePokes[0] = 0;
}

void vegaApplyPokes()
{
    if (gamePokes[0] != 0) {
        printf("\nApplying pokes:");
        char poke[24] = "set ";
        int j=4;
        int i=0;
        while (gamePokes[i] != 0) {
            if (gamePokes[i] == ';') {
                poke[j] = 0;
                printf("\n%s", &poke[0]);
                debugger_command_evaluate(&poke[0]);
                j = 4;
            }
            else {
                poke[j] = gamePokes[i];
                if (poke[j] == ',')
                    poke[j] = ' ';
                j++;
            }
            i++;
        }
        if (j != 4) {
            poke[j] = 0;
            printf("\n%s", &poke[0]);
            debugger_command_evaluate(&poke[0]);
        }
    }
    else
        printf("\nNo pokes founds");
}

int vegaWriteReg(int base, int offset, int value)
{
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd < 0) {
        printf("\nCannot access memory.\n");
        return -1;
    }

    int page = getpagesize();
    int length = page * ((0x2000 + page - 1) / page);
    int *regs = (int *)mmap(0, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);

    if (regs  == MAP_FAILED) {
        close(fd);
        printf("regs mmap error\n");
        return -1;
    }
    *(regs + (offset/4)) = value;
//    printf("\nWrite REG %x, %x = %x", base, offset, value);

    munmap(regs, length);
    close(fd);
    return 0;
}

int vegaReadReg(int base, int offset)
{
#ifdef __arm__
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd < 0) {
        printf("\nCannot access memory.\n");
        return -1;
    }

    int page = getpagesize();
    int length = page * ((0x2000 + page - 1) / page);
    int *regs = (int *)mmap(0, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);

    if (regs  == MAP_FAILED) {
        close(fd);
        printf("regs mmap error\n");
        return -1;
    }

    int actual = *(regs + (offset/4));
//    printf("\Read REG %x, %x = %x", base, offset, actual);

    munmap(regs, length);
    close(fd);
    return actual;
#else
    return 0;
#endif
}

int vegaIsTvOUTModeEnabled( void )
{
#ifdef __arm__
    int f = open("/sys/class/graphics/fb0/virtual_size", O_RDONLY);
    if (f) {
        char s[20];
        int r = read(f, &s[0], 10);
        s[r] = 0;
        printf("\nFB Mode found %s", &s[0]);
        close(f);
        if (s[0] == '6') {
            return 1;
        }
    }
    return 0;
#else
    return 0;
#endif
}

int getToken(char *str, char *token, char sep, int maxTokenLength)
{
    int i=0;
    while (str[i] != 0 && i < maxTokenLength) {
        if (str[i] == sep)
            break;
        else {
            token[i] = str[i];
            i++;
        }
    }
    token[i] = 0;

    if (str[i] == 0 && i == 0)
        return 0;

    return 1;
}

short key_button1[10];
short key_button2[10];
short key_button3[10];
short key_button4[10];
short key_left[10];
short key_right[10];
short key_up[10];
short key_down[10];
short key_yellow[10];
short key_green[10];
short key_blue[10];

short startSequence[10];
short alt_key_button1[10];
short alt_key_button2[10];
short alt_key_button3[10];
short alt_key_button4[10];
short alt_key_left[10];
short alt_key_right[10];
short alt_key_up[10];
short alt_key_down[10];
short alt_key_yellow[10];
short alt_key_green[10];
short alt_key_blue[10];
int currentKeyMapping = 0;
int altKeyMappingUsed = 0;

short *current_key_button1;
short *current_key_button2;
short *current_key_button3;
short *current_key_button4;
short *current_key_left;
short *current_key_right;
short *current_key_up;
short *current_key_down;
short *current_key_yellow;
short *current_key_green;
short *current_key_blue;

unsigned short virtualKeys[64] = {'\x00'};
int keyDelay = 0;

short interpretKey(char *key)
{
    if (strcmp(key, "SP") == 0)
        return INPUT_KEY_space;
    if (strcmp(key, "EN") == 0)
        return INPUT_KEY_Return;
    if (strcmp(key, "CS") == 0)
        return INPUT_KEY_Caps_Lock;
    if (strcmp(key, "SS") == 0)
        return keysyms_remap(51);

    if (strcmp(key, "0") == 0)
        return INPUT_KEY_0;
    if (strcmp(key, "1") == 0)
        return INPUT_KEY_1;
    if (strcmp(key, "2") == 0)
        return INPUT_KEY_2;
    if (strcmp(key, "3") == 0)
        return INPUT_KEY_3;
    if (strcmp(key, "4") == 0)
        return INPUT_KEY_4;
    if (strcmp(key, "5") == 0)
        return INPUT_KEY_5;
    if (strcmp(key, "6") == 0)
        return INPUT_KEY_6;
    if (strcmp(key, "7") == 0)
        return INPUT_KEY_7;
    if (strcmp(key, "8") == 0)
        return INPUT_KEY_8;
    if (strcmp(key, "9") == 0)
        return INPUT_KEY_9;

    if (strcmp(key, "A") == 0)
        return INPUT_KEY_a;
    if (strcmp(key, "B") == 0)
        return INPUT_KEY_b;
    if (strcmp(key, "C") == 0)
        return INPUT_KEY_c;
    if (strcmp(key, "D") == 0)
        return INPUT_KEY_d;
    if (strcmp(key, "E") == 0)
        return INPUT_KEY_e;
    if (strcmp(key, "F") == 0)
        return INPUT_KEY_f;
    if (strcmp(key, "G") == 0)
        return INPUT_KEY_g;
    if (strcmp(key, "H") == 0)
        return INPUT_KEY_h;
    if (strcmp(key, "I") == 0)
        return INPUT_KEY_i;
    if (strcmp(key, "J") == 0)
        return INPUT_KEY_j;
    if (strcmp(key, "K") == 0)
        return INPUT_KEY_k;
    if (strcmp(key, "L") == 0)
        return INPUT_KEY_l;
    if (strcmp(key, "M") == 0)
        return INPUT_KEY_m;
    if (strcmp(key, "N") == 0)
        return INPUT_KEY_n;
    if (strcmp(key, "O") == 0)
        return INPUT_KEY_o;
    if (strcmp(key, "P") == 0)
        return INPUT_KEY_p;
    if (strcmp(key, "Q") == 0)
        return INPUT_KEY_q;
    if (strcmp(key, "R") == 0)
        return INPUT_KEY_r;
    if (strcmp(key, "S") == 0)
        return INPUT_KEY_s;
    if (strcmp(key, "T") == 0)
        return INPUT_KEY_t;
    if (strcmp(key, "U") == 0)
        return INPUT_KEY_u;
    if (strcmp(key, "V") == 0)
        return INPUT_KEY_v;
    if (strcmp(key, "W") == 0)
        return INPUT_KEY_w;
    if (strcmp(key, "X") == 0)
        return INPUT_KEY_x;
    if (strcmp(key, "Y") == 0)
        return INPUT_KEY_y;
    if (strcmp(key, "Z") == 0)
        return INPUT_KEY_z;

    printf("\nCannot detect key %s", key);
    return INPUT_KEY_NONE;
}

void mapKeys(int useAltKeyMapping) {
    if (useAltKeyMapping == 1) {
        current_key_button1 = alt_key_button1;
        current_key_button2 = alt_key_button2;
        current_key_button3 = alt_key_button3;
        current_key_button4 = alt_key_button4;
        current_key_left = alt_key_left;
        current_key_right = alt_key_right;
        current_key_up = alt_key_up;
        current_key_down = alt_key_down;
        current_key_blue = alt_key_blue;
        current_key_yellow = alt_key_yellow;
        current_key_green = alt_key_green;
    }
    else {
        current_key_button1 = key_button1;
        current_key_button2 = key_button2;
        current_key_button3 = key_button3;
        current_key_button4 = key_button4;
        current_key_left = key_left;
        current_key_right = key_right;
        current_key_up = key_up;
        current_key_down = key_down;
        current_key_blue = key_blue;
        current_key_yellow = key_yellow;
        current_key_green = key_green;
    }
}

void remapKeys(char *keyDef) {
    key_button1[0] = 0;
    key_button2[0] = 0;
    key_button3[0] = 0;
    key_button4[0] = 0;
    key_left[0] = 0;
    key_right[0] = 0;
    key_up[0] = 0;
    key_down[0] = 0;
    key_yellow[0] = 0;
    key_green[0] = 0;
    key_blue[0] = 0;

    startSequence[0] = 0;
    alt_key_button1[0] = 0;
    alt_key_button2[0] = 0;
    alt_key_button3[0] = 0;
    alt_key_button4[0] = 0;
    alt_key_left[0] = 0;
    alt_key_right[0] = 0;
    alt_key_up[0] = 0;
    alt_key_down[0] = 0;
    alt_key_yellow[0] = 0;
    alt_key_green[0] = 0;
    alt_key_blue[0] = 0;

    if (keyDef) {
        printf("\nRemap Keys: %s", keyDef);
        altKeyMappingUsed = 0;
        short *currentKeys = &startSequence;

        char keySequenceBuffer[32];
        char *keySequence;
        char key[32];

        while (getToken(keyDef, &keySequenceBuffer, ';', 31)) {
            int currentKeysSize = 0;
            printf ("\nKey Sequence is %s", &keySequenceBuffer);
            keySequence = &keySequenceBuffer;
            while (getToken(keySequence, &key, ' ', 31)) {
                printf("\nKey is %s %x", &key, interpretKey(&key));

                if (currentKeys)
                    currentKeys[currentKeysSize++] = interpretKey(&key);

                keySequence += strlen(&key);
                if (strlen(keySequence) > 0)
                    keySequence++;
            }
            if (currentKeys) {
                printf("\nset zero to pos %d", currentKeysSize);
                currentKeys[currentKeysSize] = 0;

                if (currentKeysSize == 2) {
                    int i;
                    for (i=0;currentKeys[i] != 0; i++)
                        printf("CK %x ", currentKeys[i]);
                    for (i=0;key_yellow[i] != 0; i++)
                        printf("Y %x ", key_yellow[i]);
                }
            }

            if (currentKeys == &startSequence)
                currentKeys = &key_up;
            else if (currentKeys == &key_up)
                currentKeys = &key_down;
            else if (currentKeys == &key_down)
                currentKeys = &key_left;
            else if (currentKeys == &key_left)
                currentKeys = &key_right;
            else if (currentKeys == &key_right)
                currentKeys = &key_button1;
            else if (currentKeys == &key_button1)
                currentKeys = &key_button2;
            else if (currentKeys == &key_button2)
                currentKeys = &key_button3;
            else if (currentKeys == &key_button3)
                currentKeys = &key_button4;
            else if (currentKeys == &key_button4)
                currentKeys = &key_yellow;
            else if (currentKeys == &key_yellow)
                currentKeys = &key_green;
            else if (currentKeys == &key_green)
                currentKeys = &key_blue;
            else if (currentKeys == &key_blue)
                currentKeys = &alt_key_up;
            else if (currentKeys == &alt_key_up) {
                altKeyMappingUsed = 1;
                currentKeys = &alt_key_down;
            }
            else if (currentKeys == &alt_key_down)
                currentKeys = &alt_key_left;
            else if (currentKeys == &alt_key_left)
                currentKeys = &alt_key_right;
            else if (currentKeys == &alt_key_right)
                currentKeys = &alt_key_button1;
            else if (currentKeys == &alt_key_button1)
                currentKeys = &alt_key_button2;
            else if (currentKeys == &alt_key_button2)
                currentKeys = &alt_key_button3;
            else if (currentKeys == &alt_key_button3)
                currentKeys = &alt_key_button4;
            else if (currentKeys == &alt_key_button4)
                currentKeys = &alt_key_yellow;
            else if (currentKeys == &alt_key_yellow)
                currentKeys = &alt_key_green;
            else if (currentKeys == &alt_key_green)
                currentKeys = &alt_key_blue;
            else
                currentKeys = NULL;

            keyDef += strlen(&keySequenceBuffer);
            if (strlen(keyDef) > 0)
                keyDef ++;
        }

        int i;
        printf("\nRemapped keys:");
        printf("\nUP: ");
        for (i=0;key_up[i] != 0; i++)
            printf("%x ", key_up[i]);
        printf("\nDOWN: ");
        for (i=0;key_down[i] != 0; i++)
            printf("%x ", key_down[i]);
        printf("\nLEFT: ");
        for (i=0;key_left[i] != 0; i++)
            printf("%x ", key_left[i]);
        printf("\nRIGHT: ");
        for (i=0;key_right[i] != 0; i++)
            printf("%x ", key_right[i]);
        printf("\nButton 1: ");
        for (i=0;key_button1[i] != 0; i++)
            printf("%x ", key_button1[i]);
        printf("\nButton 2: ");
        for (i=0;key_button2[i] != 0; i++)
            printf("%x ", key_button2[i]);
        printf("\nButton 3: ");
        for (i=0;key_button3[i] != 0; i++)
            printf("%x ", key_button3[i]);
        printf("\nButton 4: ");
        for (i=0;key_button4[i] != 0; i++)
            printf("%x ", key_button4[i]);
        printf("\nYellow: ");
        for (i=0;key_yellow[i] != 0; i++)
            printf("%x ", key_yellow[i]);
        printf("\nGreen: ");
        for (i=0;key_green[i] != 0; i++)
            printf("%x ", key_green[i]);
        printf("\nBlue: ");
        for (i=0;key_blue[i] != 0; i++)
            printf("%x ", key_blue[i]);
        if (altKeyMappingUsed == 1) {
            printf("\nALT_UP: ");
            for (i=0;alt_key_up[i] != 0; i++)
                printf("%x ", alt_key_up[i]);
            printf("\nALT_DOWN: ");
            for (i=0;alt_key_down[i] != 0; i++)
                printf("%x ", alt_key_down[i]);
            printf("\nALT_LEFT: ");
            for (i=0;alt_key_left[i] != 0; i++)
                printf("%x ", alt_key_left[i]);
            printf("\nALT_RIGHT: ");
            for (i=0;alt_key_right[i] != 0; i++)
                printf("%x ", alt_key_right[i]);
            printf("\nALT_Button 1: ");
            for (i=0;alt_key_button1[i] != 0; i++)
                printf("%x ", alt_key_button1[i]);
            printf("\nALT_Button 2: ");
            for (i=0;alt_key_button2[i] != 0; i++)
                printf("%x ", alt_key_button2[i]);
            printf("\nALT_Button 3: ");
            for (i=0;alt_key_button3[i] != 0; i++)
                printf("%x ", alt_key_button3[i]);
            printf("\nALT_Button 4: ");
            for (i=0;alt_key_button4[i] != 0; i++)
                printf("%x ", alt_key_button4[i]);
            printf("\nALT_Yellow: ");
            for (i=0;alt_key_yellow[i] != 0; i++)
                printf("%x ", alt_key_yellow[i]);
            printf("\nALT_Green: ");
            for (i=0;alt_key_green[i] != 0; i++)
                printf("%x ", alt_key_green[i]);
            printf("\nALT_Blue: ");
            for (i=0;alt_key_blue[i] != 0; i++)
                printf("%x ", alt_key_blue[i]);
        }
    }
    else
        printf("\nRemap Keys Sequence Empty!");
    mapKeys(0);
}

void setVirtualKeys(unsigned char *keys)
{
  printf("\nSetVirtualKeys");
//  int i;
//  for (i=0;keys[i] != 0; i++)
//      printf("K: %x", keys[i]);
  strncpy((char *) &virtualKeys, (char *) keys, 63);
  virtualKeys[63] = 0;
  keyDelay = 10;
}

