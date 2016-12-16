enum InputAction {
    RELEASE,
    PRESS,
    REPEAT,
};

enum GLFWOption {
    FALSE = 0,
    TRUE = 1,
    THREE = 3,

    FOCUSED                = 0x00020001,
    ICONIFIED              = 0x00020002,
    RESIZABLE              = 0x00020003,
    VISIBLE                = 0x00020004,
    DECORATED              = 0x00020005,
    AUTO_ICONIFY           = 0x00020006,
    FLOATING               = 0x00020007,

    RED_BITS               = 0x00021001,
    GREEN_BITS             = 0x00021002,
    BLUE_BITS              = 0x00021003,
    ALPHA_BITS             = 0x00021004,
    DEPTH_BITS             = 0x00021005,
    STENCIL_BITS           = 0x00021006,
    ACCUM_RED_BITS         = 0x00021007,
    ACCUM_GREEN_BITS       = 0x00021008,
    ACCUM_BLUE_BITS        = 0x00021009,
    ACCUM_ALPHA_BITS       = 0x0002100A,
    AUX_BUFFERS            = 0x0002100B,
    STEREO                 = 0x0002100C,
    SAMPLES                = 0x0002100D,
    SRGB_CAPABLE           = 0x0002100E,
    REFRESH_RATE           = 0x0002100F,
    DOUBLEBUFFER           = 0x00021010,

    CLIENT_API               = 0x00022001,
    CONTEXT_VERSION_MAJOR    = 0x00022002,
    CONTEXT_VERSION_MINOR    = 0x00022003,
    CONTEXT_REVISION         = 0x00022004,
    CONTEXT_ROBUSTNESS       = 0x00022005,
    OPENGL_FORWARD_COMPAT    = 0x00022006,
    OPENGL_DEBUG_CONTEXT     = 0x00022007,
    OPENGL_PROFILE           = 0x00022008,
    CONTEXT_RELEASE_BEHAVIOR = 0x00022009,

    OPENGL_API             = 0x00030001,
    OPENGL_ES_API          = 0x00030002,

    NO_ROBUSTNESS          =          0,
    NO_RESET_NOTIFICATION  = 0x00031001,
    LOSE_CONTEXT_ON_RESET  = 0x00031002,

    OPENGL_ANY_PROFILE     =          0,
    OPENGL_CORE_PROFILE    = 0x00032001,
    OPENGL_COMPAT_PROFILE  = 0x00032002,

    CURSOR                 = 0x00033001,
    STICKY_KEYS            = 0x00033002,
    STICKY_MOUSE_BUTTONS   = 0x00033003,

    CURSOR_NORMAL          = 0x00034001,
    CURSOR_HIDDEN          = 0x00034002,
    CURSOR_DISABLED        = 0x00034003,

    ANY_RELEASE_BEHAVIOR   =          0,
    RELEASE_BEHAVIOR_FLUSH = 0x00035001,
    RELEASE_BEHAVIOR_NONE  = 0x00035002,
};

enum KeyCode {
    KEY_UNKNOWN            = -1,

    KEY_SPACE              = 32,
    KEY_APOSTROPHE         = 39, /* ' */
    KEY_COMMA              = 44, /* , */
    KEY_MINUS              = 45, /* - */
    KEY_PERIOD             = 46, /* . */
    KEY_SLASH              = 47, /* / */
    KEY_0                  = 48,
    KEY_1                  = 49,
    KEY_2                  = 50,
    KEY_3                  = 51,
    KEY_4                  = 52,
    KEY_5                  = 53,
    KEY_6                  = 54,
    KEY_7                  = 55,
    KEY_8                  = 56,
    KEY_9                  = 57,
    KEY_SEMICOLON          = 59, /* ; */
    KEY_EQUAL              = 61, /* = */
    KEY_A                  = 65,
    KEY_B                  = 66,
    KEY_C                  = 67,
    KEY_D                  = 68,
    KEY_E                  = 69,
    KEY_F                  = 70,
    KEY_G                  = 71,
    KEY_H                  = 72,
    KEY_I                  = 73,
    KEY_J                  = 74,
    KEY_K                  = 75,
    KEY_L                  = 76,
    KEY_M                  = 77,
    KEY_N                  = 78,
    KEY_O                  = 79,
    KEY_P                  = 80,
    KEY_Q                  = 81,
    KEY_R                  = 82,
    KEY_S                  = 83,
    KEY_T                  = 84,
    KEY_U                  = 85,
    KEY_V                  = 86,
    KEY_W                  = 87,
    KEY_X                  = 88,
    KEY_Y                  = 89,
    KEY_Z                  = 90,
    KEY_LEFT_BRACKET       = 91, /* [ */
    KEY_BACKSLASH          = 92, /* \ */
    KEY_RIGHT_BRACKET      = 93, /* ] */
    KEY_GRAVE_ACCENT       = 96, /* ` */
    KEY_WORLD_1            = 161,/* non-US #1 */
    KEY_WORLD_2            = 162,/* non-US #2 */

    KEY_ESCAPE             = 256,
    KEY_ENTER              = 257,
    KEY_TAB                = 258,
    KEY_BACKSPACE          = 259,
    KEY_INSERT             = 260,
    KEY_DELETE             = 261,
    KEY_RIGHT              = 262,
    KEY_LEFT               = 263,
    KEY_DOWN               = 264,
    KEY_UP                 = 265,
    KEY_PAGE_UP            = 266,
    KEY_PAGE_DOWN          = 267,
    KEY_HOME               = 268,
    KEY_END                = 269,
    KEY_CAPS_LOCK          = 280,
    KEY_SCROLL_LOCK        = 281,
    KEY_NUM_LOCK           = 282,
    KEY_PRINT_SCREEN       = 283,
    KEY_PAUSE              = 284,
    KEY_F1                 = 290,
    KEY_F2                 = 291,
    KEY_F3                 = 292,
    KEY_F4                 = 293,
    KEY_F5                 = 294,
    KEY_F6                 = 295,
    KEY_F7                 = 296,
    KEY_F8                 = 297,
    KEY_F9                 = 298,
    KEY_F10                = 299,
    KEY_F11                = 300,
    KEY_F12                = 301,
    KEY_F13                = 302,
    KEY_F14                = 303,
    KEY_F15                = 304,
    KEY_F16                = 305,
    KEY_F17                = 306,
    KEY_F18                = 307,
    KEY_F19                = 308,
    KEY_F20                = 309,
    KEY_F21                = 310,
    KEY_F22                = 311,
    KEY_F23                = 312,
    KEY_F24                = 313,
    KEY_F25                = 314,
    KEY_KP_0               = 320,
    KEY_KP_1               = 321,
    KEY_KP_2               = 322,
    KEY_KP_3               = 323,
    KEY_KP_4               = 324,
    KEY_KP_5               = 325,
    KEY_KP_6               = 326,
    KEY_KP_7               = 327,
    KEY_KP_8               = 328,
    KEY_KP_9               = 329,
    KEY_KP_DECIMAL         = 330,
    KEY_KP_DIVIDE          = 331,
    KEY_KP_MULTIPLY        = 332,
    KEY_KP_SUBTRACT        = 333,
    KEY_KP_ADD             = 334,
    KEY_KP_ENTER           = 335,
    KEY_KP_EQUAL           = 336,
    KEY_LEFT_SHIFT         = 340,
    KEY_LEFT_CONTROL       = 341,
    KEY_LEFT_ALT           = 342,
    KEY_LEFT_SUPER         = 343,
    KEY_RIGHT_SHIFT        = 344,
    KEY_RIGHT_CONTROL      = 345,
    KEY_RIGHT_ALT          = 346,
    KEY_RIGHT_SUPER        = 347,
    KEY_MENU               = 348,
    KEY_LAST               = 348,
};

enum InputMods {
    NONE    = 0x0000,
    SHIFT   = 0x0001,
    CONTROL = 0x0002,
    ALT     = 0x0004,
    SUPER   = 0x0008,
};

GL_CURRENT_BIT := 0x00000001;
GL_POINT_BIT := 0x00000002;
GL_LINE_BIT := 0x00000004;
GL_POLYGON_BIT := 0x00000008;
GL_POLYGON_STIPPLE_BIT := 0x00000010;
GL_PIXEL_MODE_BIT := 0x00000020;
GL_LIGHTING_BIT := 0x00000040;
GL_FOG_BIT := 0x00000080;
GL_DEPTH_BUFFER_BIT := 0x00000100;
GL_ACCUM_BUFFER_BIT := 0x00000200;
GL_STENCIL_BUFFER_BIT := 0x00000400;
GL_VIEWPORT_BIT := 0x00000800;
GL_TRANSFORM_BIT := 0x00001000;
GL_ENABLE_BIT := 0x00002000;
GL_COLOR_BUFFER_BIT := 0x00004000;
GL_HINT_BIT := 0x00008000;
GL_EVAL_BIT := 0x00010000;
GL_LIST_BIT := 0x00020000;
GL_TEXTURE_BIT := 0x00040000;
GL_SCISSOR_BIT := 0x00080000;
GL_ALL_ATTRIB_BITS := 0xFFFFFFFF;
