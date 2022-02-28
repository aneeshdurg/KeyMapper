// Simple USB Keyboard Forwarder
//
// This example is in the public domain

#include "USBHost_t36.h"

#include "./mouse_defs.h"

USBHost myusb;
USBHub hub1(myusb);
KeyboardController keyboard1(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);

#ifdef KEYBOARD_INTERFACE
uint8_t keyboard_last_leds = 0;
uint8_t keyboard_modifiers = 0;  // try to keep a reasonable value
#elif !defined(SHOW_KEYBOARD_DATA)
#Warning: "USB type does not have Serial, so turning on SHOW_KEYBOARD_DATA"
#define SHOW_KEYBOARD_DATA
#endif

#include <SD.h>
#include <SPI.h>

#define X(mouseaction)  + 1
const size_t NUM_MOUSE_ACTIONS = 0
#include "./mouse_xlist.h"
;
#undef X

#define X(keycode)  + 1
const size_t NUM_KEYS = 0
#include "./keys_xlist.h"
;
#undef X

File myFile;
const int chipSelect = BUILTIN_SDCARD;

uint8_t current_layer = 0;

uint32_t layer0[0xff + 1];
bool layer1_trigger[0xff + 1];
bool layer2_trigger[0xff + 1];
uint32_t layer1[0xff + 1];
uint32_t layer2[0xff + 1];
uint32_t layer3[0xff + 1];

bool MOUSE_STATE[NUM_MOUSE_ACTIONS];
int move_mul_h = 100000;
int move_mul_v = 100000;
int default_mul = 100000;

int btn1_click = 0;
int btn2_click = 0;
int btn3_click = 0;
void PressMouseAction(uint32_t mouse_action) {
  size_t mouse_idx = mouse_action & ~0xFF00;
  MOUSE_STATE[mouse_idx] = true;

  bool set_clicked = false;
  if (mouse_action == MOUSE_BTN1) {
    set_clicked = true;
    btn1_click = 1;
  } else if (mouse_action == MOUSE_BTN2) {
    set_clicked = true;
    btn2_click = 1;
  }  else if (mouse_action == MOUSE_BTN3) {
    set_clicked = true;
    btn3_click = 1;
  }

  if (set_clicked) {
    Mouse.set_buttons(btn1_click, btn3_click, btn2_click);
  }
}

void ReleaseMouseAction(uint32_t mouse_action) {
  size_t mouse_idx = mouse_action & ~0xFF00;
  MOUSE_STATE[mouse_idx] = false;

  bool set_clicked = false;
  if (mouse_action == MOUSE_BTN1) {
    set_clicked = true;
    btn1_click = 0;
  } else if (mouse_action == MOUSE_BTN2) {
    set_clicked = true;
    btn2_click = 0;
  }  else if (mouse_action == MOUSE_BTN3) {
    set_clicked = true;
    btn3_click = 0;
  }

  if (mouse_action == MOUSE_M_UP || mouse_action == MOUSE_M_DOWN) {
    move_mul_v = default_mul;
  }

  if (mouse_action == MOUSE_M_LEFT || mouse_action == MOUSE_M_RIGHT) {
    move_mul_h = default_mul;
  }

  if (set_clicked) {
    Mouse.set_buttons(btn1_click, btn3_click, btn2_click);
  }
}

bool IsMouseAction(uint32_t code) {
  switch (code) {
#define X(mouseaction) case mouseaction: return true;
#include "./mouse_xlist.h"
#undef X
    default:
      return false;
  }
}

char KeyCodeToLinearID(uint32_t keycode) {
  const size_t initial_value = __COUNTER__;
  switch(keycode) {
#define X(keyname) case keyname: return __COUNTER__ - initial_value - 1;
#include "./keys_xlist.h"
#undef X
  }

  return 0xff;
}

uint32_t StringToKeyCode(String s) {
#define X(mouseaction) if (s == #mouseaction) return mouseaction;
#include "./mouse_xlist.h"
#undef X

#define X(keyname) if (s == #keyname) return keyname;
#include "./keys_xlist.h"
#undef X

  return 0xffff;
}

uint32_t ModToKeyCode(uint8_t keymod) {
  switch(keymod) {
    case 0x01: return MODIFIERKEY_LEFT_CTRL   ;
    case 0x02: return MODIFIERKEY_LEFT_SHIFT  ;
    case 0x04: return MODIFIERKEY_LEFT_ALT    ;
    case 0x08: return MODIFIERKEY_LEFT_GUI    ;
    case 0x10: return MODIFIERKEY_RIGHT_CTRL  ;
    case 0x20: return MODIFIERKEY_RIGHT_SHIFT ;
    case 0x40: return MODIFIERKEY_RIGHT_ALT   ;
    case 0x80: return MODIFIERKEY_RIGHT_GUI   ;
  }

  return 0xffff;
}

uint8_t KeyCodeToMod(uint32_t keymod) {
  switch(keymod) {
    case MODIFIERKEY_LEFT_CTRL   : return 0x01;
    case MODIFIERKEY_LEFT_SHIFT  : return 0x02;
    case MODIFIERKEY_LEFT_ALT    : return 0x04;
    case MODIFIERKEY_LEFT_GUI    : return 0x08;
    case MODIFIERKEY_RIGHT_CTRL  : return 0x10;
    case MODIFIERKEY_RIGHT_SHIFT : return 0x20;
    case MODIFIERKEY_RIGHT_ALT   : return 0x40;
    case MODIFIERKEY_RIGHT_GUI   : return 0x80;
  }

  return 0xff;
}

bool IsMod(uint32_t keycode) {
  switch(keycode) {
    case MODIFIERKEY_LEFT_CTRL   :
    case MODIFIERKEY_LEFT_SHIFT  :
    case MODIFIERKEY_LEFT_ALT    :
    case MODIFIERKEY_LEFT_GUI    :
    case MODIFIERKEY_RIGHT_CTRL  :
    case MODIFIERKEY_RIGHT_SHIFT :
    case MODIFIERKEY_RIGHT_ALT   :
    case MODIFIERKEY_RIGHT_GUI   :
      return true;
  }

  return false;
}

void setup()
{
  for (size_t i = 0; i < 0xff; i++) {
    layer0[i] = 0;
    layer1[i] = 0;
    layer2[i] = 0;
    layer3[i] = 0;
    layer1_trigger[i] = false;
    layer2_trigger[i] = false;
  }

  for (size_t i = 0; i < NUM_MOUSE_ACTIONS; i++) {
    MOUSE_STATE[i] = false;
  }

  // Open serial communications and wait for port to open:
  // Serial.begin(9600);
  //  while (!Serial) {
  //   ; // wait for serial port to connect.
  // }

  // Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    // Serial.println("initialization failed!");
    return;
  }
  // Serial.println("initialization done.");

  for(size_t layer = 0; layer < 4; layer++) {
    String filename = String("layer") + String(layer) + ".txt";
    // re-open the file for reading:
    myFile = SD.open(filename.c_str());
    if (myFile) {
      // Serial.write("opening ");
      // Serial.println(filename.c_str());

      // read from the file until there's nothing else in it:
      while (myFile.available()) {
        String config_line = myFile.readStringUntil('\n');

        // Serial.write(":");
        // Serial.println(config_line.c_str());
        int split = config_line.indexOf(' ');
        if (split < 0) {
          continue;
        }

        String src = config_line.substring(0, split);
        uint32_t src_key = StringToKeyCode(src);
        char src_idx = KeyCodeToLinearID(src_key);

        String dst = config_line.substring(split + 1);
        if (dst == "layer_1") {
          layer1_trigger[src_idx] = true;
        } else if (dst == "layer_2") {
          layer2_trigger[src_idx] = true;
        } else {
          uint32_t dst_key = StringToKeyCode(dst);
          if (layer == 0) {
            layer0[src_idx] = dst_key;
          } else if (layer == 1) {
            layer1[src_idx] = dst_key;
          } else if (layer == 2) {
            layer2[src_idx] = dst_key;
          } else if (layer == 3) {
            layer3[src_idx] = dst_key;
          }
        }
      }
      // close the file:
      myFile.close();
    } else {
      // if the file didn't open, print an error:
      // Serial.write("error opening ");
      // Serial.println(filename.c_str());
    }
  }

  // Setup keyboard
  myusb.begin();
  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);
  keyboard1.attachExtrasPress(OnHIDExtrasPress);
  keyboard1.attachExtrasRelease(OnHIDExtrasRelease);
}


void loop()
{
  myusb.Task();
  ProcessMouseKeys();
}

void OnHIDExtrasPress(uint32_t top, uint16_t key)
{
#ifdef KEYBOARD_INTERFACE
  if (top == 0xc0000) {
    Keyboard.press(0XE400 | key);
#ifndef KEYMEDIA_INTERFACE
#error "KEYMEDIA_INTERFACE is Not defined"
#endif
  }
#endif
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key)
{
#ifdef KEYBOARD_INTERFACE
  if (top == 0xc0000) {
    Keyboard.release(0XE400 | key);
  }
#endif
}

uint32_t MapKeyThroughLayers(uint32_t key) {
  size_t key_idx = KeyCodeToLinearID(key);
  uint8_t layer = current_layer;
  uint32_t final_key = key;
  while (true) {
    // Check if the bit for this layer is set.
    uint8_t test = layer & current_layer;
    if (test == layer) {
      if (layer == 0 && layer0[key_idx]) {
        final_key = layer0[key_idx];
        break;
      } else if (layer == 1 && layer1[key_idx]) {
        final_key = layer1[key_idx];
        break;
      } else if (layer == 2 && layer2[key_idx]) {
        final_key = layer2[key_idx];
        break;
      } else if (layer == 3 && layer3[key_idx]) {
        final_key = layer3[key_idx];
        break;
      }
    }

    if (layer == 0) {
      break;
    }
    layer--;
  }

  return final_key;
}

void OnRawPress(uint8_t keycode) {
#ifdef KEYBOARD_INTERFACE
  if (keyboard_leds != keyboard_last_leds) {
    keyboard_last_leds = keyboard_leds;
    keyboard1.LEDS(keyboard_leds);
  }

  uint32_t key = 0;
  if (keycode >= 103 && keycode < 111) {
    uint8_t keybit = 1 << (keycode - 103);
    key = ModToKeyCode(keybit);
  } else {

    key = 0xF000 | keycode;
  }

  size_t key_idx = KeyCodeToLinearID(key);
  if (layer1_trigger[key_idx]) {
    current_layer |= 0x01;
  } else if (layer2_trigger[key_idx]) {
    current_layer |= 0x02;
  } else {
    key = MapKeyThroughLayers(key);
    if (IsMouseAction(key)) {
      PressMouseAction(key);
    } else if (IsMod(key)) {
      uint8_t keybit = KeyCodeToMod(key);
      keyboard_modifiers |= keybit;
      Keyboard.set_modifier(keyboard_modifiers);
      Keyboard.send_now();
    } else {
      Keyboard.press(key);
    }
  }
#endif
}
void OnRawRelease(uint8_t keycode) {
#ifdef KEYBOARD_INTERFACE
  uint32_t key = 0;
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    uint8_t keybit = 1 << (keycode - 103);
    key = ModToKeyCode(keybit);
  } else {
    key = 0xF000 | keycode;
  }

  size_t key_idx = KeyCodeToLinearID(key);
  if (layer1_trigger[key_idx]) {
    current_layer &= ~0x01;
  } else if (layer2_trigger[key_idx]) {
    current_layer &= ~0x02;
  } else {
    key = MapKeyThroughLayers(key);
    if (IsMouseAction(key)) {
      ReleaseMouseAction(key);
    } else if (IsMod(key)) {
      uint8_t keybit = KeyCodeToMod(key);
      keyboard_modifiers &= ~keybit;
      Keyboard.set_modifier(keyboard_modifiers);
      Keyboard.send_now();
    } else {
      Keyboard.release(key);
    }
  }
#endif
}

size_t last_movement = 0;
size_t last_scroll = 0;
#define delay_motion_time 5
#define delay_scroll_time 100
void ProcessMouseKeys() {
  int vert_motion = 0;
  if (MOUSE_STATE[0]) {
    vert_motion--;
  }
  if (MOUSE_STATE[1]) {
    vert_motion++;
  }


  int horzt_motion = 0;
  if (MOUSE_STATE[2]) {
    horzt_motion--;
  }
  if (MOUSE_STATE[3]) {
    horzt_motion++;
  }

  int vert_scroll = 0;
  if (MOUSE_STATE[7]) {
    vert_scroll++;
  }
  if (MOUSE_STATE[8]) {
    vert_scroll--;
  }

  int horzt_scroll = 0;
  if (MOUSE_STATE[9]) {
    horzt_scroll++;
  }
  if (MOUSE_STATE[10]) {
    horzt_scroll--;
  }

  size_t current_time = millis();
  size_t delta = current_time - last_movement;
  if (delta > delay_motion_time) {
    Mouse.move(horzt_motion * move_mul_h / default_mul, vert_motion * move_mul_v / default_mul);
    last_movement = current_time;

    if (horzt_motion) {
      move_mul_h += 1;
      if (move_mul_h > (3 * default_mul)) {
        move_mul_h = (3 * default_mul);
      }
    }

    if (vert_motion) {
      move_mul_v += 1;
      if (move_mul_v > (3 * default_mul)) {
        move_mul_v = (3 * default_mul);
      }
    }
  }

  delta = current_time - last_scroll;
  if (delta > delay_scroll_time) {
    Mouse.scroll(vert_scroll, horzt_scroll);
    last_scroll = current_time;
  }
}
