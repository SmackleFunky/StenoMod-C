/*
Copyright (C) 2016 by Jason Green.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

For LGPL information:   http://www.gnu.org/copyleft/lesser.txt
*/

#define REPEAT_TIMEOUT  300
#define REPEAT_START_DELAY 800
#define REPEAT_DELAY 30
#define HOLD_DELAY 500

uint8_t b[4] = {0, 0, 0, 0}; // Current Stroke
uint8_t p[4] = {0, 0, 0, 0}; // Previous Stroke
uint8_t c[4] = {0, 0, 0, 0}; // Copy of current
uint8_t t[4] = {0, 0, 0, 0}; // Temporary
uint8_t pin[4] = {11, 10, 9, 8};
uint8_t LED = 13;
uint64_t last_key_up = 0;

#define NONE 0
#define REPEAT 1
#define HOLD 2
#define HOLD_SEND 3

// Setup ports and serial
void setup() {
  DDRB = DDRC = DDRD = 0;
  PORTC = PORTD = 0xff;
  PORTB = 0xf0;
  led(false);
  Serial.begin(9600);
}

uint8_t* clear_stroke(uint8_t* s) {
  return memset(s, 0, 4);
}

uint8_t* copy_stroke(uint8_t* d, uint8_t* s) {
  return memcpy(d, s, 4);
}

uint8_t compare_stroke(uint8_t* a, uint8_t* b) {
  return memcmp(a, b, 4);
}

// Turn LED on are or off
void led(bool on) {
  digitalWrite(LED, on ? HIGH : LOW);
}

// Set to output mode, and pull low
// to measure a row.
void set_output(uint8_t p) {
  pinMode(pin[p], OUTPUT);
  digitalWrite(pin[p], LOW);
}

// Set to input mode
void set_input(uint8_t p) {
  pinMode(pin[p], INPUT);
}

// Read the current byte, where
// bit value of 1 means key is
// pressed
uint8_t read_byte() {
  return PINC ^ 0x3f;
}

// Send byte over serial
void send_byte(uint8_t b) {
  Serial.write(b);
}

// Read a column of keys
uint8_t read_column(uint8_t p) {
  uint8_t ret;
  set_output(p);
  delayMicroseconds(10);
  ret = read_byte();
  set_input(p);
  return ret;
}

// Check all keys, and modify
// b array for new presses
// Return true if any key is pressed
bool look(uint8_t* b) {
  bool ret = false;
  for (int i = 0; i < 4; i++) {
    uint8_t r = read_column(i);
    ret |= r;
    b[i] |= r;
  }
  return ret;
}

// Continuously poll for keys until
// Keys are pressed and then released
void scan_keys() {
  uint8_t key_pressed = false;
  uint32_t last_send = 0;
  uint8_t mode = NONE;
  uint32_t resend_millis = REPEAT_START_DELAY;
  bool hold_check = false;
  bool need_send = true;
  copy_stroke(p, b);
  b[0] = b[1] = b[2] = b[3] = 0;

  while (key_pressed == false) {
    while (look(b) == false);
    delay(20);
    key_pressed = look(b);
  }
  while (look(b) == true) {
    if ((mode == NONE || mode == REPEAT) &&
        compare_stroke(b, p) == 0) {
      if (mode == NONE && millis() - last_key_up < REPEAT_TIMEOUT) {
        mode = REPEAT;
        last_send = millis();
      }
      else if (mode == REPEAT) {
        if (millis() - last_send > resend_millis) {
          send_stroke(b);
          need_send = false;
          led(true);
          last_send = millis();
          resend_millis = REPEAT_DELAY;
        }
      }
    }
    if ((mode == NONE || mode == HOLD) &&
        compare_stroke(c, b) != 0) {
      if (mode == NONE) {
        hold_check = true;
        copy_stroke(c, b);
        last_send = millis();
        resend_millis = HOLD_DELAY;
      }
      else if (mode == HOLD) {
        mode = HOLD_SEND;

      }
    }
    if (mode == NONE && hold_check == true) {
      if (millis() - last_send > resend_millis) {
        mode = HOLD;
        led(true);
      }
    }
    else if (mode == HOLD_SEND) {
      look(t);
      delayMicroseconds(20);
      look(t);
      if (compare_stroke(t, c) == 0) {
        send_stroke(b);
        need_send = false;
        copy_stroke(b, c);
        mode = HOLD;
      }
      clear_stroke(t);
    }
  }
  if (need_send == true) {
    send_stroke(b);
  }
  if (mode != NONE && mode != REPEAT)
    clear_stroke(b);
  clear_stroke(c);
  last_key_up = millis();
  led(false);
}

// Send the current stroke stored in
// b array
void send_stroke(uint8_t* b) {
  if (b[0])
    send_byte(b[0]);
  if (b[1])
    send_byte(b[1] | 0x40);
  if (b[2])
    send_byte(b[2] | 0x80);
  if (b[3])
    send_byte(b[3] | 0xc0);
  else
    send_byte(0);
}

// Main loop.
void loop() {
  while (true) {
    scan_keys();
  }
}

