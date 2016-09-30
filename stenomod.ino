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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

For LGPL information:   http://www.gnu.org/copyleft/lesser.txt
*/

#define REPEAT_ENABLED true
#define REPEAT_TIMEOUT  300
#define REPEAT_START_DELAY 800
#define REPEAT_DELAY 45
#define STICKY_ENABLED true
#define STICKY_DELAY 500

// Useful way to refer to the Stroke objects
typedef uint8_t* Stroke;

typedef enum { NONE, REPEAT, STICKY, STICKY_SEND, NO_MODE } Mode;

typedef struct {
   Mode mode;
   uint8_t current_stroke[4];
   uint8_t current_keys[4];
   uint8_t previous_stroke[4];
   uint8_t sticky_stroke[4];
   uint8_t last_stroke[4];
   uint8_t last_keys[4];
   uint32_t last_stroke_change;
   uint32_t last_key_change;
   uint32_t last_stroke_send;
   uint32_t last_key_up;
   bool stroke_sent;

   bool repeat_enabled;
   bool sticky_enabled;
   uint16_t repeat_timeout;
   uint16_t repeat_start_delay;
   uint16_t repeat_delay;
   uint16_t sticky_delay;
} State;

uint8_t pin[4] = {11, 10, 9, 8};
uint8_t LED = 13;
uint64_t last_key_up = 0;

State state;

void default_settings();
void reset_state(bool set_prev);

// Setup ports and serial
void setup() {
   DDRB = DDRC = DDRD = 0;
   PORTC = PORTD = 0xff;
   PORTB = 0xf0;
   led(false);
   Serial.begin(9600);
   reset_state(false);
   default_settings();
}

void default_settings() {
   state.repeat_enabled = REPEAT_ENABLED;
   state.sticky_enabled = STICKY_ENABLED;
   state.repeat_timeout = REPEAT_TIMEOUT;
   state.repeat_start_delay = REPEAT_START_DELAY;
   state.repeat_delay = REPEAT_DELAY;
   state.sticky_delay = STICKY_DELAY;
}

/*** Stroke Manipulation Functions */
// Reset a stroke to 0
Stroke clear_stroke(Stroke s) {
   return memset(s, 0, 4);
}

// Copy one stroke to another
Stroke copy_stroke(Stroke d, Stroke s) {
   return memcpy(d, s, 4);
}

// Compare two stokes
Stroke compare_stroke(Stroke a, Stroke b) {
   return memcmp(a, b, 4);
}

// Merge two strokes
Stroke merge_stroke(Stroke d, Stroke s) {
   for (int i = 0; i < 4; i++)
     d[i] = d[i] | s[i];
   return d;
}

/* Hardware Functions */
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

/* State Manipulation Functions */
// Resets state to "as new" settings.
// optinally sets previous_stroke to
// current_stroke
void reset_state(bool set_prev) {
   if (set_prev)
     copy_stroke(state.previous_stroke, state.current_stroke);
   else
     clear_stroke(state.previous_stroke);
   clear_stroke(state.current_stroke);
   clear_stroke(state.last_stroke);
   clear_stroke(state.last_keys);
   state.stroke_sent = false;
   state.mode = NONE;
}

/* Key Scanning Functions */
// Check all keys
// Stroke s is set to currently pressed keys,
//   overwriting existing value
// Stroke c is accumulative, or'ing with
//   existing value
// Return true if any key is currently pressed.

bool look(Stroke s, Stroke c) {
   bool ret = false;
   for (int i = 0; i < 4; i++) {
     uint8_t r = read_column(i);
     ret |= r;
     if (c)
       c[i] |= r;
     if (s)
       s[i] = r;
   }
   return ret;
}

typedef struct {
   void (*func)();
   bool enabled;
   uint32_t time_set;
   uint32_t length;
} Timer;

void m_repeat_send();
void m_sticky_start();

Timer timers[] = {
    { m_repeat_send, false, 0 },
    { m_sticky_start, false, 0 }
};
#define M_REPEAT_SEND 0
#define M_STICKY_START 1
#define NUM_TIMERS 2

void set_timer(uint8_t t, uint32_t length) {
   timers[t].enabled = true;
   timers[t].length = length;
   timers[t].time_set = millis();
}

void unset_timer(uint8_t t) {
   timers[t].enabled = false;
}

void check_timers() {
  uint32_t now = millis();
  for (int i = 0; i < NUM_TIMERS; i++) {
    if (timers[i].enabled &&
         now - timers[i].time_set > timers[i].length) {
      timers[i].enabled = false;
      timers[i].func();
    }
  }
}

// Continuously poll for keys until
// Keys are pressed and then released
void scan_keys() {
   reset_state(true);

   // Wait until a key is pressed
   do {
     while (look(NULL, state.current_stroke) == false);
     // De-bounce
     delay(20);
   } while (look(NULL, state.current_stroke) == false);

   // Loop until all keys are lifted
   while (look(state.current_keys, state.current_stroke) == true) {

     // Notify listeners of changes in keys that are currently pressed
     if (compare_stroke(state.current_keys, state.last_keys) != 0) {
       m_repeat_on_key_change();
       m_sticky_on_key_change();
     }

     // Notify listeners if the stroke has changed
     if (compare_stroke(state.current_stroke, state.last_stroke) != 0) {
       m_repeat_on_stroke_change();
     }

     // Check if any timers have run out
     check_timers();

     // Prepare for next iteration
     copy_stroke(state.last_keys, state.current_keys);
     copy_stroke(state.last_stroke, state.current_stroke);
   }
   state.last_key_up = millis();

   unset_timer(M_REPEAT_SEND);
   unset_timer(M_STICKY_START);

   // If we haven't sent a key yet, send as normal
   if (state.stroke_sent == false)
     send_stroke(state.current_stroke);

   led(false);
}

/* Repeat Code */

void m_repeat_on_stroke_change() {
   if (state.repeat_enabled == false) return;

   /* Don't perform repeat if we're alreay in a mode */
   if (state.mode != NONE)
     return;

   /* Don't perform repeat if we've waited pass the timout */
   if (millis() - state.last_key_up > state.repeat_timeout)
     return;

   /* Don't perform repeat if the current stroke does not match previous
      stroke */
   if (compare_stroke(state.current_stroke, state.previous_stroke) != 0)
     return;

   /* Otherwise, we can start the repeat code */
   state.mode = REPEAT;

   /* Set a timer to initiate repeat after the appropriate delay */
   set_timer(M_REPEAT_SEND, state.repeat_start_delay);
}

void m_repeat_on_key_change() {
   /* Cancel repeat if key press change whilst repeat is on */
   if (state.mode == REPEAT) {
     state.mode = NO_MODE;
     unset_timer(M_REPEAT_SEND);
   }
}

void m_repeat_send() {
   if (state.mode != REPEAT) return;

   led(true);

   /* Send the current stroke */
   send_stroke(state.current_stroke);

   /* Add a timer to send the stroke again */
   set_timer(M_REPEAT_SEND, state.repeat_delay);
}


/* Sticky Code */

/* Called when sticky keys pressed for long enough */
void m_sticky_start() {
  if (state.mode != NONE) return;

  /* Save the key combination that is the base for the sticky keys */
  copy_stroke(state.sticky_stroke, state.current_stroke);

  /* Set mode to sticky */
  state.mode = STICKY;
  led(true);
}

void m_sticky_on_key_change() {
  if (state.sticky_enabled == false) return;

  if (state.mode == NONE) {
    /* Set a timer to initiate sicky mode */
    set_timer(M_STICKY_START, state.sticky_delay);
  }
  /* If additional keys have been pressed, track that we need to send */
  else if (state.mode == STICKY &&
         compare_stroke(state.sticky_stroke, state.current_stroke) != 0) {
    state.mode = STICKY_SEND;
  }
  /* once we get back to the initial set of keys, we can send the stroke */
  else if (state.mode == STICKY_SEND &&
         compare_stroke(state.sticky_stroke, state.current_keys) == 0) {
    send_stroke(state.current_stroke);
    copy_stroke(state.current_stroke, state.sticky_stroke);
    state.mode = STICKY;
  }
}

/* TXBOLT Serial Functions */
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

   state.last_stroke_send = millis();
   state.stroke_sent = true;
}

/* Main Loop */
void loop() {
   while (true) {
     scan_keys();
   }
}
