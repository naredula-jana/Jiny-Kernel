#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

enum {
  SET_KB_LEDS,
  SET_KB_TYPEMATIC,
};

#define KBD_IOCTL 'K'
#define KIOC_SETLEDS _IOW(KBD_IOCTL, SET_KB_LEDS, uint8_t)
#define KIOC_SET_TYPEMATIC _IOW(KBD_IOCTL, SET_KB_TYPEMATIC, uint8_t)

#define TM_DELAY(x) (((x) & 3)<<5)
#define TM_RATE(x) ((x) & 0x1F)

#define SCROLL_MASK  0x01
#define NUM_MASK     0x02
#define CAPS_MASK    0x04

#define LSHIFT_MASK  0x01
#define RSHIFT_MASK  0x02
#define LALT_MASK    0x04
#define RALT_MASK    0x08
#define LCTRL_MASK   0x10
#define RCTRL_MASK   0x20
 
#define SHIFT_MASK   (LSHIFT_MASK | RSHIFT_MASK)
#define ALT_MASK     (LALT_MASK | RALT_MASK)
#define CTRL_MASK    (LCTRL_MASK | RCTRL_MASK)

#define DEF_TM_RATE  0
#define DEF_TM_DELAY 0

#define EXT1_VAL     0xE0
#define EXT2_VAL     0xE1

#define ENTER_VAL    0x1C

#define RELEASE_MASK 0x80


#define KC_ESC            1
#define KC_1              2
#define KC_2              3
#define KC_3              4
#define KC_4              5
#define KC_5              6
#define KC_6              7
#define KC_7              8
#define KC_8              9
#define KC_9              10
#define KC_0              11
#define KC_MINUS          12
#define KC_EQ             13
#define KC_BS             14
#define KC_TAB            15
#define KC_Q              16
#define KC_W              17
#define KC_E              18
#define KC_R              19
#define KC_T              20
#define KC_Y              21
#define KC_U              22
#define KC_I              23
#define KC_O              24
#define KC_P              25
#define KC_LBRACE         26
#define KC_RBRACE         27
#define KC_ENTER          28
#define KC_LCTRL          29
#define KC_A              30
#define KC_S              31
#define KC_D              32
#define KC_F              33
#define KC_G              34
#define KC_H              35
#define KC_J              36
#define KC_K              37
#define KC_L              38
#define KC_SEMICOLON      39
#define KC_QUOTE          40
#define KC_BQUOTE         41
#define KC_LSHIFT         42
#define KC_BSLASH         43
#define KC_Z              44
#define KC_X              45
#define KC_C              46
#define KC_V              47
#define KC_B              48
#define KC_N              49
#define KC_M              50
#define KC_COMMA          51
#define KC_DOT            52
#define KC_SLASH          53
#define KC_RSHIFT         54
#define KC_KP_ASTERISK    55
#define KC_LALT           56
#define KC_SPACE          57
#define KC_CAPSLOCK       58
#define KC_F1             59
#define KC_F2             60
#define KC_F3             61
#define KC_F4             62
#define KC_F5             63
#define KC_F6             64
#define KC_F7             65
#define KC_F8             66
#define KC_F9             67
#define KC_F10            68
#define KC_NUMLOCK        69
#define KC_SCROLLLOCK     70
#define KC_KP_7           71
#define KC_UP_8           72
#define KC_KP_9           73
#define KC_KP_MINUS       74
#define KC_KP_4           75
#define KC_KP_5           76
#define KC_KP_6           77
#define KC_KP_PLUS        78
#define KC_KP_1           79
#define KC_KP_2           80
#define KC_KP_3           81
#define KC_INS            82
#define KC_KP_DOT         83
#define KC_F11            87
#define KC_F12            88

#define KC_RENTER         96
#define KC_RCTRL          97
#define KC_KP_SLASH       98
#define KC_PRTSCR         99
#define KC_RALT           100
#define KC_HOME           102
#define KC_UP             103
#define KC_PGUP           104
#define KC_LEFT           105
#define KC_RIGHT          106
#define KC_END            107
#define KC_DOWN           108
#define KC_PGDN           109
#define KC_INSERT         110
#define KC_DEL            111
#define KC_PAUSE          118

#endif
