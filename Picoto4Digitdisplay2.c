#include <stdio.h>

#include "pico/stdlib.h"

/*
 * FourDigitLEDTest.c
 * Adapted from Arduino C code
 * Created: 4/20/2020 5:07:40 PM
 * Author : dframe (david.frame@uvu.edu)
 * Modified: Oliver Rivera
 */

// Hardware connections (Pimoroni Tiny2040 / RP2040):
#define LATCH 5      // -> 74HC595 pin 12 (RCLK / latch)
#define CLK 6        // -> 74HC595 pin 11 (SRCLK / shift clock)
#define DATA 7       // -> 74HC595 pin 14 (SER / serial data in)
#define THOUSANDS 0  // -> resistor -> display pin 12 (digit 1 common); drive LOW to enable
#define HUNDREDS 1   // -> resistor -> display pin 9 (digit 2 common); drive LOW to enable
#define TENS 2       // -> resistor -> display pin 8 (digit 3 common); drive LOW to enable
#define ONES 3       // -> resistor -> display pin 6 (digit 4 common); drive LOW to enable

// Oscilloscope test pin only, not wired to the display; toggles once per
// count increment so the loop's actual timing can be verified on a scope
#define SCOPEOUT 4

#define ON_TIME 2   // ms each digit stays lit while multiplexing through all 4 digits
#define OFF_TIME 0  // extra blanking delay after a full 4-digit refresh cycle

#define ELEMENTS(x) (sizeof(x) / sizeof((x)[0]))  // number of elements in a fixed-size array

// Configure every GPIO used above as an output and set its initial (inactive) state.
void setup() {
    const uint outputs[] = {LATCH, CLK, DATA, THOUSANDS, HUNDREDS, TENS, ONES, SCOPEOUT};
    const uint initval[] = {0, 0, 0, 1, 1, 1, 1, 1};  // digit-enable pins start HIGH (off)
    stdio_init_all();
    for (int i = 0; i < ELEMENTS(outputs); i++) {
        gpio_init(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
        gpio_put(outputs[i], initval[i]);
    }
}

// Shift one byte out MSB-first into the 74HC595, then pulse LATCH to push it to the outputs.
void static inline shiftOut(char num) {
    gpio_put(LATCH, false);  // hold latch low while shifting; display keeps showing old data
    for (char i = 0; i < 8; i++) {
        gpio_put(CLK, false);        // clock low before setting the next bit
        if ((num & 0x80) == 0x80) {  // is the current MSB a 1?
            gpio_put(DATA, true);    // data pin HIGH
        } else {
            gpio_put(DATA, false);  // data pin LOW
        }
        // Short NOP delay so the data pin is stable before the clock's rising edge
        // (74HC595 needs ~100ns setup time).
        asm("NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \\
            NOP; NOP; NOP; NOP; NOP; NOP;");
        gpio_put(CLK, true);  // clock high: shifts this bit into the register
        // Short NOP delay to hold the clock high for the ~100ns minimum pulse width.
        asm("NOP; NOP; NOP; NOP; NOP; NOP; NOP; \\
            NOP;");
        num = num << 1;  // shift left so the next bit to send becomes the new MSB
    }
    asm("NOP; NOP; NOP;");  // brief delay before latching so the last bit is stable
    gpio_put(LATCH, true);  // latch high: copies the shifted bits out to the segments
    sleep_ms(1);
}

// Split a 4-digit number into its digits and multiplex them across the display, showing one
// digit at a time for ON_TIME ms so persistence of vision makes all 4 look lit at once.
void static inline Display(unsigned int num) {
    uint8_t digitEnable[] = {THOUSANDS, HUNDREDS, TENS, ONES};
    uint16_t digitSelect[] = {1000, 100, 10, 1};
    // 7-segment patterns for digits 0-9, then A-F, then blank (index 16), one byte per digit.
    unsigned char table[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f,
                             0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x00};
    int digit;
    char output;
    for (uint8_t i = 0; i < 4; i++) {
        digit = num / digitSelect[i];  // pull out this position's digit (e.g. thousands, tens)
        num -= digit * digitSelect[i];

        gpio_put(digitEnable[i], false);  // enable only this digit's common pin
        if (i == 1) {
            // Index 1 is the 2nd digit from the left; light its decimal point segment
            // (bit 0x80) so the 4-digit count reads as "XX.XX".
            output = table[digit] | 0x80;
        } else {
            output = table[digit];
        }
        shiftOut(output);  // push this digit's segment pattern out to the 74HC595
        sleep_ms(ON_TIME);
        gpio_put(digitEnable[i], 1);  // disable this digit before moving to the next one
    }
    sleep_ms(OFF_TIME);
}

int main(void) {
    uint count = 0;
    uint32_t t1, t2;
    setup();
    t1 = time_us_32();
    while (1) {
        if (count < 9999) {
            t2 = time_us_32();
            if ((t2 - t1) > 9999) {  // ~10ms elapsed (time_us_32 returns microseconds)
                count++;
                t1 = t2;
                gpio_put(SCOPEOUT, !(gpio_get(SCOPEOUT)));  // toggle scope pin every tick
            }
        } else {
            count = 0;  // wrap back to 0 after reaching 9999
        }

        Display(count);  // continuously re-draw the current count (also drives multiplexing)
    }
}
