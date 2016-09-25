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

uint8_t b[4];
uint8_t pin[4] = {11, 10, 9, 8};
uint8_t LED = 13;

// Setup ports and serial
void setup() {
  DDRB = DDRC = DDRD = 0;
  PORTC = PORTD = 0xff;
  PORTB = 0xf0;
  led(false);
  Serial.begin(9600);
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
bool look() {
  bool ret = false;
  for(int i = 0; i < 4; i++) {
     uint8_t r = read_pin(i);
     ret |= r;
     b[i] |= r;
  }
  return ret;
}

// Continuously poll for keys until
// Keys are pressed and then released
void scan_keys() {
  uint8_t key_pressed = false;
  b[0] = b[1] = b[2] = b[3] = 0;
  
  while(key_pressed == false) {
    while(look() == false);
    delay(20);
    if(look() == true)
      key_pressed = true;
  }
  led(true);
  while(look() == true);
  led(false);
}

// Send the current stroke stored in
// b array
void send_stroke() {
  if(b[0]) send_byte(b[0]);
  if(b[1]) send_byte(b[1] | 0x40);
  if(b[2]) send_byte(b[2] | 0x80);
  if(b[3]) send_byte(b[3] | 0xc0);
}

// Main loop.
void loop() {
  while (true) {
    scan_keys();
    send_stroke();
  }
}

