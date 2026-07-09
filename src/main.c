#include <vectrex/bios.h>
#include <vectrex/stdlib.h>

#pragma vx_copyright "PROK"
#pragma vx_title_pos 40, -110
#pragma vx_title_size -8, 70
#pragma vx_title "P A R A M E C I U M"

/* BIOS default text height/width in system RAM. wait_recal does not restore
   these, so save/restore them around any temporary set_text_size(). */
#define VEC_TEXT_HEIGHT (*((uint8_t *)0xC82A))
#define VEC_TEXT_WIDTH  (*((uint8_t *)0xC82B))

#define STATE_TITLE 0
#define STATE_PLAY  1
#define STATE_WIN   2
#define STATE_FAIL  3
#define STATE_DEATH 4
#define STATE_INTRO 5
#define STATE_OUTRO 6
#define MODE_ORBIT  0
#define MODE_JUMP   1
#define MODE_SPRING 2
#define MODE_SETTLE 3

#define MAX_SPHERES 6
#define MAX_EDGES   36
#define MAX_LEVELS  50
#define MAX_AUTHORED 3
#define ORBIT_STEPS 64
#define CIRCLE_SEGS 8
#define ORBIT_RADIUS 20
#define ANIM_MAX 11
#define SPIN_SPEED 2
#define ABORT_TRI_R 6
#define ORBIT_ADVANCE  2
#define JUMP_SPEED_MAX 9
#define JUMP_SPEED_MIN 3
#define LAUNCH_RAMP    24
#define LANDING_RAMP   36
#define SETTLE_SNAP    2
#define SPRING_SPEED   9
#define JUMP_HALF_DIST 50
#define SCREEN_LIMIT 115
#define CAPTURE_PAD  12
#define AST_R_MIN 4
#define AST_R_MAX 24
#define ORBIT_MARGIN 6
#define PLAYER_SIZE_BASE 6
#define PLAYER_SIZE_MAX  14
#define JUMP_GROW_DIST   20
#define MAX_ABORTS     2
#define DEATH_PARTS    12
#define DEATH_FRAMES   50
#define DEATH_MIN      35
#define PART_FADE      2
#define TITLE_DELAY  250
#define WIN_DELAY    150
#define FAIL_TIMEOUT 750    /* ~15s at 50Hz: TRY AGAIN screen auto-resets */
#define SIZE_SHRINK_DIST 48
/* text_center_x under-estimates glyph width, so centered text sits a little
   right; nudge these screens back left to look centered. */
#define TEXT_SHIFT_MENU  19
#define TEXT_SHIFT_TITLE 26

/* Animated "eye" obstacle: opens on a random sphere, stays open ~2s, closes.
   Landing on a sphere while its eye is open bursts the player. */
#define EYE_NONE      0
#define EYE_OPENING   1
#define EYE_OPEN      2
#define EYE_CLOSING   3
#define EYE_ANIM_FRAMES 22   /* open/close transition (~0.45s)  */
#define EYE_OPEN_FRAMES 100  /* stays fully open ~2s            */
#define EYE_GAP_FRAMES  90   /* idle gap before the next eye    */

/* Free-swimming "bacteria" enemy: wanders the screen on a wiggly random
   path. If the player collides with it mid-jump, the jump is deflected
   (reflected) off the body and continues in a new direction. */
#define BAC_STEP        2    /* swim speed (px/frame, via orbit unit)  */
#define BAC_TURN_FRAMES 22   /* frames between random heading nudges     */
#define BAC_TURN_AT     74   /* start smoothly turning inward past this  */
#define BAC_TURN_RATE   2    /* heading steps/frame while turning inward */
#define BAC_HARD       100   /* hard clamp on center; keeps body on-screen */
#define BAC_WID         4    /* body half-width (cilia attach point)     */
#define BAC_HIT         8    /* collision half-box vs the player         */

/* Amoeba enemy: a slow morphing blob (curves only, plus freckle dots) that
   drifts in the background. Jumping too close electrocutes the player with
   a jolt of lightning. */
#define AMO_PTS         12   /* outline points -- denser = more blob-like */
#define AMO_STEP        1    /* drift speed (also moves every other frame) */
#define AMO_TURN_FRAMES 40   /* frames between random heading nudges       */
#define AMO_TURN_AT     70   /* start banking inward past this            */
#define AMO_HARD        94   /* hard clamp on center                       */
#define AMO_BASE_R      10   /* nominal blob radius (25% smaller)          */
#define AMO_ZAP_R       16   /* electrocute range (center-to-player box)   */
#define AMO_REBUILD     7    /* rebuild cached outline every (N+1) frames  */

static const int8_t orbit_x[ORBIT_STEPS] = {
    20, 20, 20, 19, 18, 18, 17, 15, 14, 13, 11, 9, 8, 6, 4, 2,
     0, -2, -4, -6, -8, -9, -11, -13, -14, -15, -17, -18, -18, -19, -20, -20,
    -20, -20, -20, -19, -18, -18, -17, -15, -14, -13, -11, -9, -8, -6, -4, -2,
     0, 2, 4, 6, 8, 9, 11, 13, 14, 15, 17, 18, 18, 19, 20, 20
};

static const int8_t orbit_y[ORBIT_STEPS] = {
     0, 2, 4, 6, 8, 9, 11, 13, 14, 15, 17, 18, 18, 19, 20, 20,
    20, 20, 20, 19, 18, 18, 17, 15, 14, 13, 11, 9, 8, 6, 4, 2,
     0, -2, -4, -6, -8, -9, -11, -13, -14, -15, -17, -18, -18, -19, -20, -20,
    -20, -20, -20, -19, -18, -18, -17, -15, -14, -13, -11, -9, -8, -6, -4, -2
};

/* Precomputed sample indices into the 64-step orbit table so the draw
   loop never divides. 8 evenly spaced samples (i*8) -- enough for a
   readable circle on Vectrex, ~33% fewer lines than the old 12-seg path. */
static const uint8_t seg12[CIRCLE_SEGS] = {0, 8, 16, 24, 32, 40, 48, 56};

/* Denser sample set for the amoeba blob (round(i*64/12)). */
static const uint8_t seg_amo[AMO_PTS] = {0, 5, 11, 16, 21, 27, 32, 37, 43, 48, 53, 59};

static const int8_t l1_x[] = {-60, 0, 62};
static const int8_t l1_y[] = {-40, 50, -38};
static const int8_t l2_x[] = {-68, 72, -38, 43};
static const int8_t l2_y[] = {-50, -49, 61, 62};
static const int8_t l3_x[] = {-76, 81, 0, -48, 53};
static const int8_t l3_y[] = {-60, -59, 68, 32, 33};

typedef struct {
    int8_t count;
    const int8_t *x;
    const int8_t *y;
} LevelDef;

static const LevelDef levels[MAX_AUTHORED] = {
    {3, l1_x, l1_y},
    {4, l2_x, l2_y},
    {5, l3_x, l3_y}
};

/* Difficulty model. Levels 1..MAX_AUTHORED use hand-authored layouts; beyond
   that, boards are generated procedurally. Every difficulty knob is derived
   from the level number here so new layers (drift, obstacles, power-ups) can
   be switched on per level without touching the game loop. */
typedef struct {
    uint8_t count;          /* number of asteroids on the board            */
    int8_t  orbit_advance;  /* orbit-aim speed in steps/frame              */
    int8_t  jump_speed;     /* max jump travel speed in px/frame           */
    int8_t  size_mid;       /* asteroid size window center (radius)        */
    int8_t  size_half;      /* asteroid size window half-width             */
    uint8_t drift_speed;    /* asteroids drift per frame (0 = static) TODO */
    uint8_t obstacle_count; /* hazards on the board (0 = none)             */
    uint8_t powerup_kind;   /* power-up type (0 = none)               TODO */
} LevelParams;

static LevelParams cur_params;


static uint8_t state = STATE_TITLE;
static uint8_t level_num = 1;
static uint8_t mode = MODE_ORBIT;
static uint16_t title_t = 0;

static int8_t sphere_count = 3;
static int8_t current = 0;
static int8_t orbit_idx = 0;
static int8_t jump_dx = 0;
static int8_t jump_dy = 0;
static int16_t jump_fx = 0;      /* jump position sub-pixel accumulator (x*16) */
static int16_t jump_fy = 0;      /* jump position sub-pixel accumulator (y*16) */
static int8_t jump_cx = 0;
static int8_t jump_cy = 0;
static int8_t jump_sx = 0;
static int8_t jump_sy = 0;
static int8_t jump_orbit_idx = 0;
static int8_t jump_orbit_r = ORBIT_MARGIN;
static int16_t jump_travel = 0;   /* total px travelled this jump; 16-bit so a
                                     long miss can't overflow and collapse the
                                     jump speed (the "stall then drift off"
                                     bug) */
static int8_t jump_land_dist = 127; /* nearest_landing_dist cached once/frame */
static uint8_t jump_clear = 0;
static uint8_t jump_committed = 0;
static uint8_t jump_pump = 0;
static uint8_t jump_bounced = 0; /* 1 after first wall bounce this jump */
static uint8_t jump_armed = 0;
static int8_t settle_sphere = 0;
static int8_t settle_tx = 0;
static int8_t settle_ty = 0;
static int8_t visited_count = 1;
static int8_t edge_count = 0;
static int8_t abort_count = 0;
static int8_t player_size_cur = PLAYER_SIZE_BASE;
static uint8_t fail_reason = 0;

/* --- TEMP death-cause telemetry (remove once the mid-flight death is found).
   dbg_site identifies which trigger_death() fired; the rest snapshot state at
   the instant of death so it can be read straight off the screen. --- */
static uint8_t dbg_site = 0;
static int16_t dbg_jt = 0;
static int8_t dbg_ax = 0;
static int8_t dbg_ay = 0;
static uint8_t dbg_ab = 0;
static uint8_t dbg_mode = 0;
static uint8_t dbg_minsp = 99;   /* slowest jump speed seen this jump */
static int8_t dbg_ldmin = 127;   /* nearest_landing_dist when slowest */
static int8_t dbg_dx = 0;        /* jump direction captured at launch */
static int8_t dbg_dy = 0;
static int8_t dbg_sx = 0;        /* launch pixel position */
static int8_t dbg_sy = 0;
static uint8_t dbg_frames = 0;   /* frames spent in this jump */
static uint16_t fail_t = 0;
static uint16_t death_t = 0;
static uint8_t anim_t = 0;
static uint8_t win_t = 0;
static uint8_t spin = 0;
static int8_t eye_sphere = -1;
static uint8_t eye_phase = EYE_NONE;
static uint8_t eye_amt = 0;
static uint16_t eye_t = 0;
static uint8_t eye_scan = 16;    /* pupil left/right scan phase (orbit index) */

static uint8_t bac_active = 0;
static int8_t bac_x = 0;
static int8_t bac_y = 0;
static int8_t bac_head = 0;      /* swim heading, index into orbit table */
static uint8_t bac_wiggle = 0;
static uint8_t bac_turn_t = 0;
static int8_t bac_body_sx[6];    /* cached body screen offsets (heading-only) */
static int8_t bac_body_sy[6];
static int8_t bac_cache_head = -1; /* last heading the body cache was built for */
static int8_t pl_vx[CIRCLE_SEGS]; /* cached player-circle offsets for current r */
static int8_t pl_vy[CIRCLE_SEGS];
static int8_t pl_cache_r = -1;

static uint8_t amo_active = 0;
static int8_t amo_x = 0;
static int8_t amo_y = 0;
static int8_t amo_head = 0;
static uint8_t amo_turn_t = 0;
static uint8_t amo_morph = 0;
static int8_t zap_ax = 0;        /* lightning endpoints captured at death */
static int8_t zap_ay = 0;
static int8_t zap_px = 0;
static int8_t zap_py = 0;
static int8_t amo_pts_x[AMO_PTS];  /* cached outline offsets from amo center */
static int8_t amo_pts_y[AMO_PTS];
static uint8_t amo_cache_valid = 0;

static int8_t scr_x[MAX_SPHERES];
static int8_t scr_y[MAX_SPHERES];
static int8_t sp_r[MAX_SPHERES];
static int16_t sp_sx[MAX_SPHERES];   /* drift sub-pixel position (x * 16) */
static int16_t sp_sy[MAX_SPHERES];   /* drift sub-pixel position (y * 16) */
static int8_t sp_dvx[MAX_SPHERES];   /* drift velocity, 1/16 px per frame */
static int8_t sp_dvy[MAX_SPHERES];
static int8_t sp_vx[MAX_SPHERES][CIRCLE_SEGS];
static int8_t sp_vy[MAX_SPHERES][CIRCLE_SEGS];
static int8_t visited[MAX_SPHERES];
static int8_t edge_use[MAX_EDGES];
static int8_t ln_a[MAX_EDGES];
static int8_t ln_b[MAX_EDGES];
static int8_t part_x[DEATH_PARTS];
static int8_t part_y[DEATH_PARTS];
static int8_t part_dx[DEATH_PARTS];
static int8_t part_dy[DEATH_PARTS];
static uint8_t part_life[DEATH_PARTS];

static const int8_t burst_dx[DEATH_PARTS] = {6, 4, 0, -4, -6, -4, 0, 4, 5, 2, -2, -5};
static const int8_t burst_dy[DEATH_PARTS] = {0, 4, 6, 4, 0, -4, -6, -4, 2, 5, -5, -2};

/* AY-3-8912 register dumps for play_sound() (reg,value pairs, 0xff-terminated).
   Tuned for an "old lab" feel: a glassy bubble/blip on connect, a high tick
   when an eye opens, and a buzzy electric jolt when the amoeba zaps you.
   Each uses the hardware envelope (amp reg bit4 set) with a one-shot decay
   (env shape 0x00) so the note fades on its own. */
static const uint8_t snd_bubble[] = {
    0x00, 0x80, 0x01, 0x01,       /* tone A period ~0x0180 (glassy blip) */
    0x07, 0x3e,                   /* mixer: tone A on, everything else off */
    0x08, 0x10,                   /* amp A follows envelope               */
    0x0b, 0x00, 0x0c, 0x03,       /* short envelope period                */
    0x0d, 0x00, 0xff              /* env shape 0 = decay, then terminator */
};
static const uint8_t snd_eye[] = {
    0x00, 0x40, 0x01, 0x00,       /* tone A high-pitched tick             */
    0x07, 0x3e, 0x08, 0x10,
    0x0b, 0x00, 0x0c, 0x02,
    0x0d, 0x00, 0xff
};
static const uint8_t snd_zap[] = {
    0x00, 0x00, 0x01, 0x02,       /* low buzzy tone A                     */
    0x06, 0x04,                   /* fast noise (electric crackle)        */
    0x07, 0x36,                   /* mixer: tone A + noise A on           */
    0x08, 0x10,                   /* amp A follows envelope               */
    0x0b, 0x00, 0x0c, 0x06,       /* medium envelope period               */
    0x0d, 0x00, 0xff
};

static int8_t player_x = 0;
static int8_t player_y = 0;
static uint8_t btn_state = 0;
static uint8_t prev_ui_state = 0xff;
static uint8_t menu_btn_ready = 0;

static void poll_buttons(void);
static void draw_connections(void);
static void draw_spheres(void);
static void draw_lightning(void);

/* Parametric circle: x = cx + r*cos(t), y = cy + r*sin(t).
   orbit_x/orbit_y hold cos/sin at t = 0..63 for r = ORBIT_RADIUS.
   Division only happens here, at level start (round-to-nearest). */
static int8_t scale_radial(int8_t unit, int8_t radius)
{
    int16_t v = (int16_t)unit * radius;
    if (v >= 0) {
        v += ORBIT_RADIUS / 2;
    } else {
        v -= ORBIT_RADIUS / 2;
    }
    return (int8_t)(v / ORBIT_RADIUS);
}

/* Player orbits just outside the surface of the current asteroid, so a
   wide range of asteroid sizes all feel consistent to fly around. */
static int8_t orbit_r_of(int8_t i)
{
    return (int8_t)(sp_r[i] + ORBIT_MARGIN);
}

/* Draw a closed outline from precomputed per-point offsets (no division). */
static void draw_outline(int8_t cy, int8_t cx, const int8_t *vx, const int8_t *vy,
                         uint8_t count, uint8_t intensity)
{
    uint8_t i;
    int8_t px;
    int8_t py;
    int8_t nx;
    int8_t ny;

    intensity_a(intensity);
    reset0ref();

    px = (int8_t)(cx + vx[0]);
    py = (int8_t)(cy + vy[0]);
    moveto_d(py, px);

    for (i = 1; i < count; ++i) {
        nx = (int8_t)(cx + vx[i]);
        ny = (int8_t)(cy + vy[i]);
        draw_line_d((int8_t)(ny - py), (int8_t)(nx - px));
        px = nx;
        py = ny;
    }

    draw_line_d((int8_t)((cy + vy[0]) - py), (int8_t)((cx + vx[0]) - px));
}

static void draw_dot_abs(int8_t y, int8_t x, uint8_t intensity)
{
    intensity_a(intensity);
    reset0ref();
    moveto_d(y, x);
    draw_line_d(0, 2);
    draw_line_d(2, 0);
    draw_line_d(0, -2);
    draw_line_d(-2, 0);
}

static void begin_frame(void)
{
    wait_recal();
    do_sound();
    intensity_a(0x7f);
    set_scale(0x7f);
    poll_buttons();
}

static void poll_buttons(void)
{
    controller_check_buttons();
    btn_state = read_btns();
}

static uint8_t button1_held(void)
{
    if (controller_button_1_1_held()) {
        return 1;
    }
    if ((controller_buttons_held() & JOY1_BTN1_MASK) != 0) {
        return 1;
    }
    return (uint8_t)((btn_state & JOY1_BTN1_MASK) != 0);
}

static uint8_t button1_pressed(void)
{
    static uint8_t prev = 0;
    uint8_t now;
    uint8_t edge;

    if (controller_button_1_1_pressed()) {
        return 1;
    }

    now = (uint8_t)(btn_state & JOY1_BTN1_MASK);
    edge = (uint8_t)(now & (uint8_t)~prev);
    prev = now;
    return edge != 0;
}

static void reset_menu_button(void)
{
    menu_btn_ready = 0;
}

static void update_menu_button_arm(uint8_t ui_state)
{
    if (ui_state != prev_ui_state) {
        prev_ui_state = ui_state;
        menu_btn_ready = 0;
    }
    if (!menu_btn_ready && !button1_held()) {
        menu_btn_ready = 1;
    }
}

static uint8_t menu_button_fire(void)
{
    return (uint8_t)(menu_btn_ready && (button1_held() || button1_pressed()));
}

static void draw_label(int8_t y, int8_t x, char *str)
{
    zero_beam();
    print_str_c(y, x, str);
}

static int8_t text_center_x(char *str)
{
    uint8_t len = 0;

    while (str[len] != 0) {
        len++;
    }
    return (int8_t)(-((int8_t)((len * 10) / 2)));
}

/* Derive every difficulty knob from the level number. This is the single
   place to tune the 50-level ramp and to enable future layers. */
static void get_level_params(uint8_t lvl, LevelParams *p)
{
    int8_t mid;
    int8_t half;
    int8_t adv;
    uint8_t count;

    if (lvl == 1) {
        count = 3;
    } else if (lvl == 2) {
        count = 4;
    } else {
        count = (uint8_t)(5 + (lvl - 3) / 6);
        if (count > MAX_SPHERES) {
            count = MAX_SPHERES;
        }
    }
    p->count = count;

    /* Orbit aim and jump speed both climb gradually with the level. */
    adv = (int8_t)(ORBIT_ADVANCE + (int8_t)((lvl - 1) / 5));
    if (adv > 6) {
        adv = 6;
    }
    p->orbit_advance = adv;

    p->jump_speed = (int8_t)(JUMP_SPEED_MAX + (int8_t)((lvl - 1) / 4));
    if (p->jump_speed > 16) {
        p->jump_speed = 16;
    }

    /* Size window tightens and shrinks with level (see randomize). */
    mid = (int8_t)(15 - (int8_t)((lvl - 1) * 2));
    half = (int8_t)(9 - (int8_t)((lvl - 1) * 2));
    if (mid < 7) {
        mid = 7;
    }
    if (half < 1) {
        half = 1;
    }
    p->size_mid = mid;
    p->size_half = half;

    /* Hazard ramp: one enemy type at a time. Eye on L2 only; bacteria and
       amoeba are activated in reset_round for L3 / L4+ respectively. */
    p->obstacle_count = (uint8_t)(lvl == 2 ? 1 : 0);

    if (lvl >= 2) {
        uint8_t d = (uint8_t)(3 + (lvl - 2));
        if (d > 10) {
            d = 10;
        }
        p->drift_speed = d;
    } else {
        p->drift_speed = 0;
    }

    p->powerup_kind = 0;
}

/* Procedural board for levels past the authored ones: spread the asteroids
   around two concentric rings so they stay on-screen and non-overlapping. */
static void gen_ring_positions(uint8_t count)
{
    uint8_t i;
    uint8_t idx;
    int8_t r;

    for (i = 0; i < count; ++i) {
        idx = (uint8_t)(((uint16_t)i * ORBIT_STEPS) / count);
        r = (int8_t)((i & 1) ? 58 : 80);
        scr_x[i] = scale_radial(orbit_x[idx], r);
        scr_y[i] = scale_radial(orbit_y[idx], r);
    }
}

static void init_spheres(uint8_t lvl)
{
    uint8_t i;
    const LevelDef *def;

    level_num = lvl;
    sphere_count = (int8_t)cur_params.count;

    if (lvl <= MAX_AUTHORED) {
        def = &levels[lvl - 1];
        for (i = 0; i < (uint8_t)sphere_count; ++i) {
            scr_x[i] = def->x[i];
            scr_y[i] = def->y[i];
        }
    } else {
        gen_ring_positions((uint8_t)sphere_count);
    }
}

/* Self-contained 16-bit xorshift PRNG. The Vectrex BIOS random() did not
   give usable spread between successive calls, so we roll our own. */
static uint16_t rng_state = 0xA17C;

static uint8_t next_rng(void)
{
    rng_state ^= (uint16_t)(rng_state << 7);
    rng_state ^= (uint16_t)(rng_state >> 9);
    rng_state ^= (uint16_t)(rng_state << 8);
    return (uint8_t)(rng_state >> 8);
}

/* Precompute the scaled outline for one asteroid (division happens here,
   once per level, never per frame). */
static void build_asteroid_pts(uint8_t si)
{
    uint8_t j;

    for (j = 0; j < CIRCLE_SEGS; ++j) {
        sp_vx[si][j] = scale_radial(orbit_x[seg12[j]], sp_r[si]);
        sp_vy[si][j] = scale_radial(orbit_y[seg12[j]], sp_r[si]);
    }
}

/* Per-level size window. Early levels span a very wide range (big and
   small asteroids mixed on the same board); the window tightens and drifts
   smaller each level, so later boards are more uniform and harder.
   Diameters are spread evenly across the window and then shuffled onto the
   asteroids, so every asteroid on a board has a distinct size. */
static void randomize_sphere_looks(uint8_t count, uint8_t lvl)
{
    uint8_t i;
    uint8_t j;
    int8_t tmp;
    int8_t lo;
    int8_t hi;
    int8_t range;

    lo = (int8_t)(cur_params.size_mid - cur_params.size_half);
    hi = (int8_t)(cur_params.size_mid + cur_params.size_half);
    if (lo < AST_R_MIN) {
        lo = AST_R_MIN;
    }
    if (hi > AST_R_MAX) {
        hi = AST_R_MAX;
    }
    range = (int8_t)(hi - lo);

    /* Evenly spaced diameters across [lo, hi] -> guaranteed all different. */
    for (i = 0; i < count; ++i) {
        if (count <= 1) {
            sp_r[i] = (int8_t)((lo + hi) / 2);
        } else {
            sp_r[i] = (int8_t)(lo + (int8_t)(((int16_t)range * i) / (count - 1)));
        }
    }

    /* Shuffle which asteroid gets which size (Fisher-Yates). */
    rng_state = (uint16_t)(0x1234u + (uint16_t)lvl * 0x9E37u);
    for (i = count; i > 1; --i) {
        j = (uint8_t)(next_rng() % i);
        tmp = sp_r[i - 1];
        sp_r[i - 1] = sp_r[j];
        sp_r[j] = tmp;
    }

    for (i = 0; i < count; ++i) {
        build_asteroid_pts(i);
    }
}

/* Seed each nucleus with a sub-pixel position and a slow random drift
   velocity (magnitude ~ drift_speed, in 1/16-px units per frame). */
static void init_drift(uint8_t count)
{
    uint8_t i;
    uint8_t idx;

    for (i = 0; i < count; ++i) {
        sp_sx[i] = (int16_t)((int16_t)scr_x[i] * 16);
        sp_sy[i] = (int16_t)((int16_t)scr_y[i] * 16);
        if (cur_params.drift_speed == 0) {
            sp_dvx[i] = 0;
            sp_dvy[i] = 0;
        } else {
            idx = (uint8_t)(next_rng() & (ORBIT_STEPS - 1));
            sp_dvx[i] = scale_radial(orbit_x[idx], (int8_t)cur_params.drift_speed);
            sp_dvy[i] = scale_radial(orbit_y[idx], (int8_t)cur_params.drift_speed);
        }
    }
}

static int8_t sphere_capture_r(int8_t i)
{
    return (int8_t)(sp_r[i] + CAPTURE_PAD);
}

static void clear_visited(void)
{
    uint8_t i;
    for (i = 0; i < MAX_SPHERES; ++i) {
        visited[i] = 0;
    }
}

static void clear_edges(void)
{
    uint8_t i;
    for (i = 0; i < MAX_EDGES; ++i) {
        edge_use[i] = 0;
        ln_a[i] = 0;
        ln_b[i] = 0;
    }
}

static uint8_t edge_index(int8_t a, int8_t b)
{
    int8_t tmp;
    if (a > b) {
        tmp = a;
        a = b;
        b = tmp;
    }
    return (uint8_t)(a * MAX_SPHERES + b);
}

static uint8_t edge_used(int8_t a, int8_t b)
{
    return edge_use[edge_index(a, b)];
}

static void mark_edge(int8_t a, int8_t b)
{
    uint8_t idx = edge_index(a, b);
    edge_use[idx] = 1;
    ln_a[edge_count] = a;
    ln_b[edge_count] = b;
    edge_count++;
}

static void update_player_orbit(void)
{
    int8_t cx = scr_x[current];
    int8_t cy = scr_y[current];
    int8_t r = orbit_r_of(current);

    player_x = (int8_t)(cx + scale_radial(orbit_x[orbit_idx], r));
    player_y = (int8_t)(cy + scale_radial(orbit_y[orbit_idx], r));
}

/* Choose a spawn point for a free-roaming enemy that is clear (Manhattan
   distance >= clear_r) of the currently occupied nucleus, so nothing ever
   materialises on top of the player. Spawning near an *unoccupied* nucleus
   is fine (and can steer the player's next jump). */
static void pick_spawn_away(int8_t *ox, int8_t *oy, int8_t clear_r)
{
    uint8_t tries;
    int8_t x = 0;
    int8_t y = 0;

    for (tries = 0; tries < 8; ++tries) {
        x = (int8_t)((int8_t)(next_rng() % 132) - 66);
        y = (int8_t)((int8_t)(next_rng() % 132) - 66);
        if ((int8_t)(abs(x - scr_x[current]) + abs(y - scr_y[current])) >= clear_r) {
            break;
        }
    }
    *ox = x;
    *oy = y;
}

static void reset_round(void)
{
    current = 0;
    orbit_idx = 0;
    mode = MODE_ORBIT;
    jump_clear = 0;
    jump_committed = 0;
    jump_travel = 0;
    jump_pump = 0;
    jump_armed = 0;
    settle_sphere = 0;
    visited_count = 1;
    edge_count = 0;
    abort_count = 0;
    player_size_cur = PLAYER_SIZE_BASE;
    fail_reason = 0;
    /* One enemy on screen at a time (keeps level 3+ drawable on the 6809):
       L2 = eye, L3 = bacteria, L4+ = amoeba. */
    eye_phase = EYE_NONE;
    eye_sphere = -1;
    eye_amt = 0;
    eye_t = EYE_GAP_FRAMES;
    eye_scan = 16;
    bac_active = 0;
    amo_active = 0;
    bac_cache_head = -1;
    amo_cache_valid = 0;

    if (level_num == 2) {
        /* eye enabled via cur_params.obstacle_count */
    } else if (level_num == 3) {
        bac_active = 1;
        pick_spawn_away(&bac_x, &bac_y, 34);
        bac_head = (int8_t)(next_rng() & (ORBIT_STEPS - 1));
        bac_wiggle = 0;
        bac_turn_t = BAC_TURN_FRAMES;
    } else if (level_num >= 4) {
        amo_active = 1;
        pick_spawn_away(&amo_x, &amo_y, 46);
        amo_head = (int8_t)(next_rng() & (ORBIT_STEPS - 1));
        amo_turn_t = AMO_TURN_FRAMES;
        amo_morph = 0;
    }
    clear_visited();
    clear_edges();
    visited[0] = 1;
    update_player_orbit();
}

static void start_level(uint8_t lvl)
{
    title_t = 0;
    get_level_params(lvl, &cur_params);
    init_spheres(lvl);
    randomize_sphere_looks((uint8_t)sphere_count, lvl);
    init_drift((uint8_t)sphere_count);
    reset_round();
    anim_t = 0;
    state = STATE_INTRO;
}

static int8_t orbit_idx_from_vector(int8_t ai, int8_t dx, int8_t dy)
{
    uint8_t i;
    uint8_t best = 0;
    int8_t best_dist = 127;
    int8_t dist;

    (void)ai;
    for (i = 0; i < ORBIT_STEPS; ++i) {
        dist = (int8_t)(abs(dx - orbit_x[i]) + abs(dy - orbit_y[i]));
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return (int8_t)best;
}

static void begin_jump(void)
{
    jump_cx = scr_x[current];
    jump_cy = scr_y[current];
    jump_sx = player_x;
    jump_sy = player_y;
    jump_orbit_idx = orbit_idx;
    jump_orbit_r = orbit_r_of(current);
    jump_dx = (int8_t)(player_x - jump_cx);
    jump_dy = (int8_t)(player_y - jump_cy);
    jump_fx = (int16_t)((int16_t)player_x * 16);
    jump_fy = (int16_t)((int16_t)player_y * 16);
    jump_travel = 0;
    jump_clear = 0;
    jump_committed = 0;
    jump_pump = 1;
    jump_bounced = 0;
    mode = MODE_JUMP;

    dbg_minsp = 99;
    dbg_ldmin = 127;
    dbg_dx = jump_dx;
    dbg_dy = jump_dy;
    dbg_sx = jump_sx;
    dbg_sy = jump_sy;
    dbg_frames = 0;
}

static void cancel_jump_spring(void)
{
    mode = MODE_SPRING;
}

static void finish_spring_back(void)
{
    mode = MODE_ORBIT;
    jump_clear = 0;
    jump_committed = 0;
    jump_travel = 0;
    jump_pump = 0;
    orbit_idx = jump_orbit_idx;
    abort_count++;
    update_player_orbit();
}

static void trigger_death(uint8_t reason)
{
    uint8_t i;

    dbg_jt = jump_travel;
    dbg_ax = player_x;
    dbg_ay = player_y;
    dbg_ab = (uint8_t)abort_count;
    dbg_mode = mode;

    mode = MODE_ORBIT;
    for (i = 0; i < DEATH_PARTS; ++i) {
        part_x[i] = player_x;
        part_y[i] = player_y;
        part_dx[i] = burst_dx[i];
        part_dy[i] = burst_dy[i];
        part_life[i] = 127;
    }
    death_t = 0;
    fail_reason = reason;
    state = STATE_DEATH;
    if (reason == 2) {
        play_sound((void *)snd_zap);
    } else {
        explosion_sound();
    }
}

static void update_death(void)
{
    uint8_t i;
    uint8_t alive = 0;

    death_t++;
    for (i = 0; i < DEATH_PARTS; ++i) {
        if (part_life[i] == 0) {
            continue;
        }
        alive = 1;
        part_x[i] = (int8_t)(part_x[i] + part_dx[i]);
        part_y[i] = (int8_t)(part_y[i] + part_dy[i]);
        if (part_life[i] > PART_FADE) {
            part_life[i] = (uint8_t)(part_life[i] - PART_FADE);
        } else {
            part_life[i] = 0;
        }
    }

    if (death_t >= DEATH_FRAMES || (!alive && death_t >= DEATH_MIN)) {
        fail_t = 0;
        state = STATE_FAIL;
    }
}

static void draw_death(void)
{
    uint8_t i;

    draw_connections();
    draw_spheres();
    if (fail_reason == 2 && death_t < 16) {
        draw_lightning();
    }
    for (i = 0; i < DEATH_PARTS; ++i) {
        if (part_life[i] > 0) {
            draw_dot_abs(part_y[i], part_x[i], part_life[i]);
        }
    }
}

static uint8_t player_off_screen(void)
{
    return (uint8_t)(player_x < -SCREEN_LIMIT || player_x > SCREEN_LIMIT ||
                     player_y < -SCREEN_LIMIT || player_y > SCREEN_LIMIT);
}

static uint8_t player_near_sphere(int8_t i)
{
    int8_t cap = sphere_capture_r(i);
    int16_t dx = (int16_t)player_x - (int16_t)scr_x[i];
    int16_t dy = (int16_t)player_y - (int16_t)scr_y[i];

    if (dx < 0) {
        dx = (int16_t)(-dx);
    }
    if (dy < 0) {
        dy = (int16_t)(-dy);
    }
    return (uint8_t)(dx < cap && dy < cap);
}

static int8_t sphere_travel_dist(int8_t i)
{
    int16_t dx = (int16_t)player_x - (int16_t)scr_x[i];
    int16_t dy = (int16_t)player_y - (int16_t)scr_y[i];
    int16_t d;

    if (dx < 0) {
        dx = (int16_t)(-dx);
    }
    if (dy < 0) {
        dy = (int16_t)(-dy);
    }
    d = (int16_t)(dx + dy);
    if (d > 127) {
        d = 127;
    }
    return (int8_t)d;
}

static int8_t nearest_landing_dist(void)
{
    int8_t i;
    int8_t best = 127;
    int8_t d;

    for (i = 0; i < sphere_count; ++i) {
        if (i == current) {
            continue;
        }
        d = sphere_travel_dist(i);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

static int8_t jump_step_speed(void)
{
    int8_t speed;
    int8_t vmax = cur_params.jump_speed;

    if (vmax < JUMP_SPEED_MIN) {
        vmax = JUMP_SPEED_MIN;
    }

    /* Pure straight-line flight: brief launch ramp up to full speed, then
       constant speed for the rest of the jump. No "landing brake" -- the
       trajectory is a straight line and the only thing that ends a jump is a
       collision (land) or leaving the screen (miss). Slowing down near a
       sphere the player wasn't going to capture is what made near-misses
       crawl to a stop and drift off the edge. */
    if (jump_travel < LAUNCH_RAMP) {
        speed = (int8_t)(JUMP_SPEED_MIN +
            ((int16_t)jump_travel * (vmax - JUMP_SPEED_MIN)) / LAUNCH_RAMP);
    } else {
        speed = vmax;
    }

    if (speed < JUMP_SPEED_MIN) {
        speed = JUMP_SPEED_MIN;
    }
    return speed;
}

/* Advance along the jump direction. Position is accumulated in 1/16-px
   sub-pixel units so the trajectory is a true straight radial line at every
   speed. Returns 1 only when the step would leave the screen AND the jump
   has already used its one wall bounce -- first edge hit reflects instead. */
static uint8_t move_along_jump_dir(int8_t speed)
{
    int16_t sx;
    int16_t sy;
    int16_t nfx;
    int16_t nfy;
    int16_t px;
    int16_t py;
    uint8_t hit_x;
    uint8_t hit_y;

    /* Cast BOTH operands to int16 BEFORE multiplying. CMOC does 8-bit
       multiply for int8*int8, so |dx|*speed > 127 wraps and reverses the
       jump direction mid-flight. */
    sx = (int16_t)(((int16_t)jump_dx * (int16_t)speed * 16) / jump_orbit_r);
    sy = (int16_t)(((int16_t)jump_dy * (int16_t)speed * 16) / jump_orbit_r);

    nfx = (int16_t)(jump_fx + sx);
    nfy = (int16_t)(jump_fy + sy);

    px = (int16_t)(nfx / 16);
    py = (int16_t)(nfy / 16);
    hit_x = (uint8_t)(px < -SCREEN_LIMIT || px > SCREEN_LIMIT);
    hit_y = (uint8_t)(py < -SCREEN_LIMIT || py > SCREEN_LIMIT);

    if (hit_x || hit_y) {
        if (jump_bounced) {
            return 1;   /* second edge hit = real miss / death */
        }

        /* One free bounce: reflect the axis that hit, clamp onto the rim,
           and keep flying so a lined-up sphere can still be landed on. */
        if (hit_x) {
            jump_dx = (int8_t)(-jump_dx);
            if (px > SCREEN_LIMIT) {
                px = SCREEN_LIMIT;
            } else {
                px = (int16_t)(-SCREEN_LIMIT);
            }
        }
        if (hit_y) {
            jump_dy = (int8_t)(-jump_dy);
            if (py > SCREEN_LIMIT) {
                py = SCREEN_LIMIT;
            } else {
                py = (int16_t)(-SCREEN_LIMIT);
            }
        }
        jump_fx = (int16_t)(px * 16);
        jump_fy = (int16_t)(py * 16);
        player_x = (int8_t)px;
        player_y = (int8_t)py;
        jump_bounced = 1;
        dbg_dx = jump_dx;
        dbg_dy = jump_dy;
        return 0;
    }

    jump_fx = nfx;
    jump_fy = nfy;
    player_x = (int8_t)px;
    player_y = (int8_t)py;
    return 0;
}

static void begin_settle(int8_t target)
{
    int8_t dx;
    int8_t dy;
    int8_t settle_r;

    /* Landing on a sphere whose eye is open bursts the player. */
    if (target == eye_sphere && eye_phase == EYE_OPEN) {
        dbg_site = 1;
        trigger_death(1);
        return;
    }

    settle_sphere = target;
    dx = (int8_t)(player_x - scr_x[target]);
    dy = (int8_t)(player_y - scr_y[target]);
    orbit_idx = orbit_idx_from_vector(target, dx, dy);
    settle_r = orbit_r_of(target);
    settle_tx = (int8_t)(scr_x[target] + scale_radial(orbit_x[orbit_idx], settle_r));
    settle_ty = (int8_t)(scr_y[target] + scale_radial(orbit_y[orbit_idx], settle_r));
    mode = MODE_SETTLE;
}

static void finish_settle(void)
{
    /* Record the tether for drawing, but never fail on a repeat visit --
       reusing a link is allowed. */
    if (!edge_used(current, settle_sphere)) {
        mark_edge(current, settle_sphere);
    }
    current = settle_sphere;
    player_x = settle_tx;
    player_y = settle_ty;
    mode = MODE_ORBIT;
    jump_clear = 0;
    jump_committed = 0;
    jump_travel = 0;
    jump_pump = 0;
    jump_armed = 0;
    abort_count = 0;

    if (!visited[settle_sphere]) {
        visited[settle_sphere] = 1;
        visited_count++;
    }

    play_sound((void *)snd_bubble);

    if (visited_count >= sphere_count) {
        anim_t = ANIM_MAX;
        state = STATE_OUTRO;
    }
}

static int8_t settle_step(int16_t delta, int16_t ad)
{
    int8_t step;

    if (ad <= SETTLE_SNAP) {
        return (int8_t)delta;
    }

    step = (int8_t)(ad / 4);
    if (step < 1) {
        step = 1;
    }
    if (step > 5) {
        step = 5;
    }
    if (delta < 0) {
        return (int8_t)-step;
    }
    return step;
}

static void update_settle(void)
{
    int16_t dx;
    int16_t dy;
    int16_t adx;
    int16_t ady;

    dx = (int16_t)((int16_t)settle_tx - (int16_t)player_x);
    dy = (int16_t)((int16_t)settle_ty - (int16_t)player_y);
    adx = (dx < 0) ? (int16_t)(-dx) : dx;
    ady = (dy < 0) ? (int16_t)(-dy) : dy;

    if (adx <= SETTLE_SNAP && ady <= SETTLE_SNAP) {
        finish_settle();
        return;
    }

    if (adx > SETTLE_SNAP) {
        player_x = (int8_t)(player_x + settle_step(dx, adx));
    }
    if (ady > SETTLE_SNAP) {
        player_y = (int8_t)(player_y + settle_step(dy, ady));
    }
}

static void land_on_sphere(int8_t target)
{
    begin_settle(target);
}

static uint8_t move_player_outward(void)
{
    int8_t speed = jump_step_speed();
    uint8_t off = move_along_jump_dir(speed);

    dbg_frames++;
    if (jump_travel >= LAUNCH_RAMP && (uint8_t)speed < dbg_minsp) {
        dbg_minsp = (uint8_t)speed;
        dbg_ldmin = jump_land_dist;
    }

    jump_travel = (int16_t)(jump_travel + speed);
    return off;
}

static void move_player_spring_back(void)
{
    int8_t tx;
    int8_t ty;
    int16_t dx;
    int16_t dy;
    int16_t adx;
    int16_t ady;
    int8_t step;

    tx = (int8_t)(scr_x[current] + scale_radial(orbit_x[jump_orbit_idx], jump_orbit_r));
    ty = (int8_t)(scr_y[current] + scale_radial(orbit_y[jump_orbit_idx], jump_orbit_r));
    dx = (int16_t)((int16_t)tx - (int16_t)player_x);
    dy = (int16_t)((int16_t)ty - (int16_t)player_y);
    adx = (dx < 0) ? (int16_t)(-dx) : dx;
    ady = (dy < 0) ? (int16_t)(-dy) : dy;

    if (adx <= SPRING_SPEED && ady <= SPRING_SPEED) {
        finish_spring_back();
        return;
    }

    step = SPRING_SPEED;
    if (adx > 0) {
        if (dx > 0) {
            player_x = (int8_t)(player_x + step);
        } else {
            player_x = (int8_t)(player_x - step);
        }
    }
    if (ady > 0) {
        if (dy > 0) {
            player_y = (int8_t)(player_y + step);
        } else {
            player_y = (int8_t)(player_y - step);
        }
    }
}

static void check_jump_collisions(void)
{
    int8_t i;
    int8_t dx;
    int8_t dy;

    if (!jump_clear) {
        int8_t cap = sphere_capture_r(current);
        dx = (int8_t)abs(player_x - jump_cx);
        dy = (int8_t)abs(player_y - jump_cy);
        if (dx < cap && dy < cap) {
            return;
        }
        jump_clear = 1;
    }

    for (i = 0; i < sphere_count; ++i) {
        if (i == current) {
            continue;
        }
        if (player_near_sphere(i)) {
            land_on_sphere(i);
            return;
        }
    }
}

static void update_spring(void)
{
    move_player_spring_back();
}

/* Touching the bacteria mid-jump bursts the player. It has no other effect
   on the jump: the trajectory is computed identically whether or not the
   bacteria happens to sit in the path. */
static uint8_t check_bacteria_collision(void)
{
    int8_t adx;
    int8_t ady;

    if (!bac_active) {
        return 0;
    }
    adx = (int8_t)abs((int8_t)(player_x - bac_x));
    ady = (int8_t)abs((int8_t)(player_y - bac_y));
    if (adx < BAC_HIT && ady < BAC_HIT) {
        dbg_site = 2;
        trigger_death(1);
        return 1;
    }
    return 0;
}

/* Jumping within the amoeba's field electrocutes the player: capture the
   lightning endpoints and trigger an electric death. */
static uint8_t check_amoeba_collision(void)
{
    int8_t adx;
    int8_t ady;

    if (!amo_active) {
        return 0;
    }
    adx = (int8_t)abs((int8_t)(player_x - amo_x));
    ady = (int8_t)abs((int8_t)(player_y - amo_y));
    if (adx < AMO_ZAP_R && ady < AMO_ZAP_R) {
        zap_ax = amo_x;
        zap_ay = amo_y;
        zap_px = player_x;
        zap_py = player_y;
        dbg_site = 3;
        trigger_death(2);
        return 1;
    }
    return 0;
}

static void update_jump(void)
{
    /* One landing-distance scan per jump frame -- shared by size easing
       and (debug) stall telemetry. */
    jump_land_dist = nearest_landing_dist();

    if (!jump_committed) {
        if (!button1_held() && !jump_pump) {
            if (jump_travel < JUMP_HALF_DIST) {
                /* Releasing before the commit distance always reverts to the
                   launch sphere -- never a death. (Previously a 3rd early
                   release burst the player, which was not an intended rule and
                   fired spuriously when a held button momentarily read as
                   released.) */
                if (jump_travel > 0) {
                    cancel_jump_spring();
                } else {
                    mode = MODE_ORBIT;
                    update_player_orbit();
                }
                return;
            }
            jump_committed = 1;
        }
    }

    if (button1_held() || jump_committed || jump_pump) {
        if (move_player_outward()) {
            dbg_site = 5;
            trigger_death(0);
            return;
        }
        jump_pump = 0;
    } else {
        return;
    }

    if (player_off_screen()) {
        dbg_site = 6;
        trigger_death(0);
        return;
    }

    if (check_amoeba_collision()) {
        return;
    }

    if (check_bacteria_collision()) {
        return;
    }

    check_jump_collisions();
}

/* Smoothstep over 0..16 -> 0..16: 16 * (3u^2 - 2u^3), u = t/16.
   Equivalent integer form: (24*t*t - t*t*t) / 128. Symmetric ease in/out. */
static int8_t size_ease(int8_t t)
{
    int16_t tt;

    if (t <= 0) {
        return 0;
    }
    if (t >= 16) {
        return 16;
    }
    tt = (int16_t)t * t;
    return (int8_t)((24 * tt - tt * t) / 128);
}

static int8_t player_size_target(void)
{
    int8_t range;
    int8_t grow;
    int8_t eased;
    int8_t tx;
    int8_t ty;
    int8_t dx;
    int8_t dy;

    range = (int8_t)(PLAYER_SIZE_MAX - PLAYER_SIZE_BASE);

    if (mode == MODE_JUMP) {
        int8_t up;
        int8_t down;

        /* Ramp up quickly with distance travelled off the launch pad...
           (clamp in 16-bit before narrowing so a long jump can't wrap grow) */
        if (jump_travel > JUMP_GROW_DIST) {
            grow = JUMP_GROW_DIST;
        } else {
            grow = (int8_t)jump_travel;
        }
        up = size_ease((int8_t)((int16_t)grow * 16 / JUMP_GROW_DIST));

        /* ...then ease back down as we close in on the next asteroid. */
        grow = jump_land_dist;
        if (grow > SIZE_SHRINK_DIST) {
            grow = SIZE_SHRINK_DIST;
        }
        down = size_ease((int8_t)((int16_t)grow * 16 / SIZE_SHRINK_DIST));

        eased = (up < down) ? up : down;
        return (int8_t)(PLAYER_SIZE_BASE + ((int16_t)range * eased) / 16);
    }

    if (mode == MODE_SPRING) {
        tx = (int8_t)(scr_x[current] + scale_radial(orbit_x[jump_orbit_idx], jump_orbit_r));
        ty = (int8_t)(scr_y[current] + scale_radial(orbit_y[jump_orbit_idx], jump_orbit_r));
        dx = (int8_t)abs(tx - player_x);
        dy = (int8_t)abs(ty - player_y);
        grow = (int8_t)(dx + dy);
        if (grow > JUMP_GROW_DIST) {
            grow = JUMP_GROW_DIST;
        }
        eased = size_ease((int8_t)((int16_t)grow * 16 / JUMP_GROW_DIST));
        return (int8_t)(PLAYER_SIZE_BASE + ((int16_t)range * eased) / 16);
    }

    if (mode == MODE_SETTLE) {
        dx = (int8_t)abs(settle_tx - player_x);
        dy = (int8_t)abs(settle_ty - player_y);
        grow = (int8_t)(dx + dy);
        if (grow > SIZE_SHRINK_DIST) {
            grow = SIZE_SHRINK_DIST;
        }
        eased = size_ease((int8_t)((int16_t)grow * 16 / SIZE_SHRINK_DIST));
        return (int8_t)(PLAYER_SIZE_BASE + ((int16_t)range * eased) / 16);
    }

    return PLAYER_SIZE_BASE;
}

static void update_player_size(void)
{
    int8_t target;
    int8_t delta;

    target = player_size_target();
    if (player_size_cur == target) {
        return;
    }

    delta = (int8_t)(target - player_size_cur);
    if (delta > 2) {
        delta = 2;
    } else if (delta < -2) {
        delta = -2;
    }
    player_size_cur = (int8_t)(player_size_cur + delta);
}

/* Advance the eye obstacle: idle gap -> open -> hold ~2s -> close -> repeat,
   each time choosing a fresh random sphere (never the one being orbited). */
static void update_eye(void)
{
    uint8_t r;

    if (cur_params.obstacle_count == 0) {
        eye_phase = EYE_NONE;
        eye_amt = 0;
        return;
    }

    /* Pupil sweeps side to side (smoothly, via the cos table) while visible. */
    if (eye_phase != EYE_NONE) {
        eye_scan = (uint8_t)(eye_scan + 2);
    }

    switch (eye_phase) {
    case EYE_NONE:
        if (eye_t > 0) {
            eye_t--;
        } else if (sphere_count > 1) {
            do {
                r = (uint8_t)(next_rng() % (uint8_t)sphere_count);
            } while (r == (uint8_t)current);
            eye_sphere = (int8_t)r;
            eye_phase = EYE_OPENING;
            eye_amt = 0;
            eye_scan = 16;
            play_sound((void *)snd_eye);
        }
        break;
    case EYE_OPENING:
        if (eye_amt < EYE_ANIM_FRAMES) {
            eye_amt++;
        } else {
            eye_phase = EYE_OPEN;
            eye_t = EYE_OPEN_FRAMES;
        }
        break;
    case EYE_OPEN:
        if (eye_t > 0) {
            eye_t--;
        } else {
            eye_phase = EYE_CLOSING;
        }
        break;
    case EYE_CLOSING:
        if (eye_amt > 0) {
            eye_amt--;
        } else {
            eye_phase = EYE_NONE;
            eye_sphere = -1;
            eye_t = EYE_GAP_FRAMES;
        }
        break;
    default:
        break;
    }
}

/* Wander the bacteria on a wiggly random path. Near the screen edge it
   overrides the wander and banks smoothly back toward the middle; a hard
   clamp on the center guarantees the body never crosses the edge.

   Steering is O(1): the sign of the 2D cross product of the current heading
   with the "toward-center" vector tells us which way to rotate, so it turns
   the short way around a couple of steps per frame (no table search). */
static void update_bacteria(void)
{
    int8_t t;
    int8_t eff;
    int8_t wig;
    uint8_t near_edge;

    if (!bac_active) {
        return;
    }

    bac_wiggle = (uint8_t)(bac_wiggle + 5);

    near_edge = (uint8_t)(bac_x > BAC_TURN_AT || bac_x < -BAC_TURN_AT ||
                          bac_y > BAC_TURN_AT || bac_y < -BAC_TURN_AT);

    if (near_edge) {
        uint8_t hn = (uint8_t)(bac_head & (ORBIT_STEPS - 1));
        int16_t cross = (int16_t)((int16_t)orbit_x[hn] * (int16_t)(-bac_y) -
                                  (int16_t)orbit_y[hn] * (int16_t)(-bac_x));
        if (cross < 0) {
            bac_head = (int8_t)((bac_head - BAC_TURN_RATE) & (ORBIT_STEPS - 1));
        } else {
            bac_head = (int8_t)((bac_head + BAC_TURN_RATE) & (ORBIT_STEPS - 1));
        }
        bac_turn_t = BAC_TURN_FRAMES;
    } else if (bac_turn_t > 0) {
        bac_turn_t--;
    } else {
        t = (int8_t)((int8_t)(next_rng() % 7) - 3);      /* -3..+3 */
        bac_head = (int8_t)((bac_head + t) & (ORBIT_STEPS - 1));
        bac_turn_t = BAC_TURN_FRAMES;
    }

    /* small continuous sway on top of the heading -> swimming wiggle */
    wig = (int8_t)((int8_t)((bac_wiggle >> 3) & 3) - 1);
    eff = (int8_t)((bac_head + wig) & (ORBIT_STEPS - 1));

    bac_x = (int8_t)(bac_x + scale_radial(orbit_x[(uint8_t)eff], BAC_STEP));
    bac_y = (int8_t)(bac_y + scale_radial(orbit_y[(uint8_t)eff], BAC_STEP));

    if (bac_x > BAC_HARD) {
        bac_x = BAC_HARD;
    } else if (bac_x < -BAC_HARD) {
        bac_x = -BAC_HARD;
    }
    if (bac_y > BAC_HARD) {
        bac_y = BAC_HARD;
    } else if (bac_y < -BAC_HARD) {
        bac_y = -BAC_HARD;
    }
}

/* Slowly drift the nuclei, bouncing each one off a soft boundary that keeps
   its whole body (and the player orbiting it) comfortably on screen. Works
   in 1/16-px sub-pixel units so very slow speeds still move smoothly.
   After the wall bounce, spheres that overlap each other also bounce apart
   so they never pass through one another. */
static void update_drift(void)
{
    int8_t i;
    int8_t j;
    int8_t lim;
    int8_t min_sep;
    int16_t nx;
    int16_t ny;
    int16_t dx;
    int16_t dy;
    int16_t adx;
    int16_t ady;
    int8_t tmp;

    if (cur_params.drift_speed == 0) {
        return;
    }

    for (i = 0; i < sphere_count; ++i) {
        sp_sx[i] = (int16_t)(sp_sx[i] + sp_dvx[i]);
        sp_sy[i] = (int16_t)(sp_sy[i] + sp_dvy[i]);
        nx = (int16_t)(sp_sx[i] >> 4);
        ny = (int16_t)(sp_sy[i] >> 4);

        lim = (int8_t)(84 - sp_r[i]);
        if (lim < 40) {
            lim = 40;
        }

        if (nx > lim) {
            nx = lim;
            sp_sx[i] = (int16_t)((int16_t)lim * 16);
            sp_dvx[i] = (int8_t)(-sp_dvx[i]);
        } else if (nx < -lim) {
            nx = (int16_t)(-lim);
            sp_sx[i] = (int16_t)((int16_t)(-lim) * 16);
            sp_dvx[i] = (int8_t)(-sp_dvx[i]);
        }
        if (ny > lim) {
            ny = lim;
            sp_sy[i] = (int16_t)((int16_t)lim * 16);
            sp_dvy[i] = (int8_t)(-sp_dvy[i]);
        } else if (ny < -lim) {
            ny = (int16_t)(-lim);
            sp_sy[i] = (int16_t)((int16_t)(-lim) * 16);
            sp_dvy[i] = (int8_t)(-sp_dvy[i]);
        }

        scr_x[i] = (int8_t)nx;
        scr_y[i] = (int8_t)ny;
    }

    /* Pairwise bounce: if two nuclei overlap (Manhattan approx of radii
       sum), swap their velocity components along the separation axis and
       nudge them apart one pixel so they don't stick. Cheap O(n^2) is fine
       at MAX_SPHERES=6. */
    for (i = 0; i < sphere_count; ++i) {
        for (j = (int8_t)(i + 1); j < sphere_count; ++j) {
            dx = (int16_t)((int16_t)scr_x[i] - (int16_t)scr_x[j]);
            dy = (int16_t)((int16_t)scr_y[i] - (int16_t)scr_y[j]);
            adx = (dx < 0) ? (int16_t)(-dx) : dx;
            ady = (dy < 0) ? (int16_t)(-dy) : dy;
            min_sep = (int8_t)(sp_r[i] + sp_r[j] + 2);
            if ((adx + ady) >= (int16_t)min_sep) {
                continue;
            }

            /* Reflect: swap velocities so they bounce off each other. */
            tmp = sp_dvx[i];
            sp_dvx[i] = sp_dvx[j];
            sp_dvx[j] = tmp;
            tmp = sp_dvy[i];
            sp_dvy[i] = sp_dvy[j];
            sp_dvy[j] = tmp;

            /* Nudge apart along the dominant axis so they don't re-collide
               next frame while still overlapping. */
            if (adx >= ady) {
                if (dx == 0) {
                    dx = 1;
                }
                if (dx > 0) {
                    scr_x[i] = (int8_t)(scr_x[i] + 1);
                    scr_x[j] = (int8_t)(scr_x[j] - 1);
                } else {
                    scr_x[i] = (int8_t)(scr_x[i] - 1);
                    scr_x[j] = (int8_t)(scr_x[j] + 1);
                }
            } else {
                if (dy == 0) {
                    dy = 1;
                }
                if (dy > 0) {
                    scr_y[i] = (int8_t)(scr_y[i] + 1);
                    scr_y[j] = (int8_t)(scr_y[j] - 1);
                } else {
                    scr_y[i] = (int8_t)(scr_y[i] - 1);
                    scr_y[j] = (int8_t)(scr_y[j] + 1);
                }
            }
            sp_sx[i] = (int16_t)((int16_t)scr_x[i] * 16);
            sp_sy[i] = (int16_t)((int16_t)scr_y[i] * 16);
            sp_sx[j] = (int16_t)((int16_t)scr_x[j] * 16);
            sp_sy[j] = (int16_t)((int16_t)scr_y[j] * 16);
        }
    }
}

/* Slowly drift the amoeba (even slower than the bacteria: it only steps on
   even frames), banking inward near the edge. amo_morph also drives the
   body's shape animation. */
static void update_amoeba(void)
{
    int8_t t;
    uint8_t near_edge;
    uint8_t hn;

    if (!amo_active) {
        return;
    }

    amo_morph = (uint8_t)(amo_morph + 1);

    near_edge = (uint8_t)(amo_x > AMO_TURN_AT || amo_x < -AMO_TURN_AT ||
                          amo_y > AMO_TURN_AT || amo_y < -AMO_TURN_AT);

    hn = (uint8_t)(amo_head & (ORBIT_STEPS - 1));
    if (near_edge) {
        int16_t cross = (int16_t)((int16_t)orbit_x[hn] * (int16_t)(-amo_y) -
                                  (int16_t)orbit_y[hn] * (int16_t)(-amo_x));
        if (cross < 0) {
            amo_head = (int8_t)((amo_head - 1) & (ORBIT_STEPS - 1));
        } else {
            amo_head = (int8_t)((amo_head + 1) & (ORBIT_STEPS - 1));
        }
        amo_turn_t = AMO_TURN_FRAMES;
    } else if (amo_turn_t > 0) {
        amo_turn_t--;
    } else {
        t = (int8_t)((int8_t)(next_rng() % 5) - 2);      /* -2..+2 */
        amo_head = (int8_t)((amo_head + t) & (ORBIT_STEPS - 1));
        amo_turn_t = AMO_TURN_FRAMES;
    }

    if ((amo_morph & 1) == 0) {
        hn = (uint8_t)(amo_head & (ORBIT_STEPS - 1));
        amo_x = (int8_t)(amo_x + scale_radial(orbit_x[hn], AMO_STEP));
        amo_y = (int8_t)(amo_y + scale_radial(orbit_y[hn], AMO_STEP));
    }

    if (amo_x > AMO_HARD) {
        amo_x = AMO_HARD;
    } else if (amo_x < -AMO_HARD) {
        amo_x = -AMO_HARD;
    }
    if (amo_y > AMO_HARD) {
        amo_y = AMO_HARD;
    } else if (amo_y < -AMO_HARD) {
        amo_y = -AMO_HARD;
    }
}

static void update_play(void)
{
    if (state != STATE_PLAY) {
        return;
    }

    update_player_size();
    update_eye();
    update_bacteria();
    update_amoeba();
    update_drift();
    spin = (uint8_t)(spin + SPIN_SPEED);

    if (mode == MODE_ORBIT) {
        if (jump_armed && button1_held()) {
            update_player_orbit();
            begin_jump();
            update_jump();
        } else {
            if (!button1_held()) {
                jump_armed = 1;
            }
            orbit_idx = (int8_t)(orbit_idx + cur_params.orbit_advance);
            if (orbit_idx >= ORBIT_STEPS) {
                orbit_idx = (int8_t)(orbit_idx - ORBIT_STEPS);
            }
            update_player_orbit();
        }
    } else if (mode == MODE_SPRING) {
        update_spring();
    } else if (mode == MODE_SETTLE) {
        update_settle();
    } else {
        update_jump();
    }

    /* Hard rule: leaving the screen in ANY mode is instant death. */
    if (state == STATE_PLAY && player_off_screen()) {
        dbg_site = 7;
        trigger_death(0);
    }
}

/* The visited spheres form a single path (each edge's endpoint is the next
   edge's start), so draw the whole chain as one continuous polyline: one
   beam-reset + one moveto, then a relative line per hop. */
static void draw_connections(void)
{
    uint8_t i;
    int16_t dx;
    int16_t dy;
    int8_t hx;
    int8_t hy;

    if (edge_count == 0) {
        return;
    }

    intensity_a(70);
    reset0ref();
    moveto_d(scr_y[ln_a[0]], scr_x[ln_a[0]]);
    for (i = 0; i < edge_count; ++i) {
        /* Deltas must be computed in 16-bit: two nuclei (esp. once drift has
           pushed them apart) can be >127 px apart, which overflows an int8
           and flips the line's direction. Draw each hop as two halves so
           every draw_line_d delta stays safely within int8 range. */
        dx = (int16_t)((int16_t)scr_x[ln_b[i]] - (int16_t)scr_x[ln_a[i]]);
        dy = (int16_t)((int16_t)scr_y[ln_b[i]] - (int16_t)scr_y[ln_a[i]]);
        hx = (int8_t)(dx / 2);
        hy = (int8_t)(dy / 2);
        draw_line_d(hy, hx);
        draw_line_d((int8_t)(dy - hy), (int8_t)(dx - hx));
    }
}

/* Animated eye on its host sphere: a lens (two corners + eyelids) that opens
   vertically, with a pupil dot once it's mostly open. */
static void draw_eye(void)
{
    int8_t cx;
    int8_t cy;
    int8_t w;
    int8_t h;
    int8_t hh;

    if (eye_phase == EYE_NONE || eye_sphere < 0) {
        return;
    }

    cx = scr_x[eye_sphere];
    cy = scr_y[eye_sphere];
    w = (int8_t)(sp_r[eye_sphere] - 1);
    if (w < 3) {
        w = 3;
    }
    if (w > 12) {
        w = 12;
    }
    h = (int8_t)((w * 3) / 5);
    hh = (int8_t)(((int16_t)h * eye_amt) / EYE_ANIM_FRAMES);
    if (hh < 1) {
        hh = 1;
    }

    intensity_a(115);
    reset0ref();
    moveto_d(cy, (int8_t)(cx - w));
    draw_line_d(hh, w);
    draw_line_d((int8_t)(-hh), w);
    draw_line_d((int8_t)(-hh), (int8_t)(-w));
    draw_line_d(hh, (int8_t)(-w));

    /* Pupil scans left/right as if searching for the player. */
    if (eye_amt > (EYE_ANIM_FRAMES / 2)) {
        int8_t poff = scale_radial(orbit_x[(uint8_t)(eye_scan & (ORBIT_STEPS - 1))],
                                   (int8_t)(w - 3));
        draw_dot_abs(cy, (int8_t)(cx + poff), 127);
    }
}

static void draw_spheres(void)
{
    int8_t i;

    for (i = 0; i < sphere_count; ++i) {
        if (visited[i]) {
            draw_outline(scr_y[i], scr_x[i], sp_vx[i], sp_vy[i], CIRCLE_SEGS, 90);
        } else {
            draw_outline(scr_y[i], scr_x[i], sp_vx[i], sp_vy[i], CIRCLE_SEGS, 50);
        }
    }
}

/* Spinning triangle centered on an asteroid. `base` is a rotation offset
   into the 64-step table; the 3 vertices are ~120 deg apart. */
static void draw_spin_tri(int8_t cy, int8_t cx, uint8_t base, int8_t tri_r, uint8_t intensity)
{
    uint8_t ia = (uint8_t)(base % ORBIT_STEPS);
    uint8_t ib = (uint8_t)((base + 21) % ORBIT_STEPS);
    uint8_t ic = (uint8_t)((base + 43) % ORBIT_STEPS);
    int8_t ax = (int8_t)(cx + scale_radial(orbit_x[ia], tri_r));
    int8_t ay = (int8_t)(cy + scale_radial(orbit_y[ia], tri_r));
    int8_t bx = (int8_t)(cx + scale_radial(orbit_x[ib], tri_r));
    int8_t by = (int8_t)(cy + scale_radial(orbit_y[ib], tri_r));
    int8_t dx = (int8_t)(cx + scale_radial(orbit_x[ic], tri_r));
    int8_t dy = (int8_t)(cy + scale_radial(orbit_y[ic], tri_r));

    intensity_a(intensity);
    reset0ref();
    moveto_d(ay, ax);
    draw_line_d((int8_t)(by - ay), (int8_t)(bx - ax));
    draw_line_d((int8_t)(dy - by), (int8_t)(dx - bx));
    draw_line_d((int8_t)(ay - dy), (int8_t)(ax - dx));
}

/* Abort-warning triangles on the current asteroid: one per used attempt.
   First spins right, second spins left; both persist until a landing. */
static void draw_abort_marks(void)
{
    int8_t cx = scr_x[current];
    int8_t cy = scr_y[current];
    int8_t tri_r = (int8_t)((sp_r[current] >> 1) + 1);

    if (tri_r > ABORT_TRI_R) {
        tri_r = ABORT_TRI_R;
    }
    if (abort_count >= 1) {
        draw_spin_tri(cy, cx, spin, tri_r, 95);
    }
    if (abort_count >= 2) {
        draw_spin_tri(cy, cx, (uint8_t)(ORBIT_STEPS - (spin % ORBIT_STEPS)), tri_r, 95);
    }
}

/* Both center position and radius scale by k/ANIM_MAX, so each asteroid
   emanates from screen center (k=0) to its full size and spot (k=ANIM_MAX). */
static void draw_asteroid_scaled(uint8_t si, uint8_t k, uint8_t intensity)
{
    uint8_t j;
    int8_t cx;
    int8_t cy;
    int8_t vx0;
    int8_t vy0;
    int8_t px;
    int8_t py;
    int8_t nx;
    int8_t ny;

    cx = (int8_t)(((int16_t)scr_x[si] * k) / ANIM_MAX);
    cy = (int8_t)(((int16_t)scr_y[si] * k) / ANIM_MAX);
    vx0 = (int8_t)(((int16_t)sp_vx[si][0] * k) / ANIM_MAX);
    vy0 = (int8_t)(((int16_t)sp_vy[si][0] * k) / ANIM_MAX);

    intensity_a(intensity);
    reset0ref();
    px = (int8_t)(cx + vx0);
    py = (int8_t)(cy + vy0);
    moveto_d(py, px);

    for (j = 1; j < CIRCLE_SEGS; ++j) {
        nx = (int8_t)(cx + (int8_t)(((int16_t)sp_vx[si][j] * k) / ANIM_MAX));
        ny = (int8_t)(cy + (int8_t)(((int16_t)sp_vy[si][j] * k) / ANIM_MAX));
        draw_line_d((int8_t)(ny - py), (int8_t)(nx - px));
        px = nx;
        py = ny;
    }

    draw_line_d((int8_t)((cy + vy0) - py), (int8_t)((cx + vx0) - px));
}

static void draw_level_anim(uint8_t k)
{
    int8_t i;

    for (i = 0; i < sphere_count; ++i) {
        draw_asteroid_scaled((uint8_t)i, k, 80);
    }
}

/* Player as a small circle. Outline offsets are cached by radius so we only
   pay scale_radial when size changes -- draw is then just add + line. */
static void draw_player(void)
{
    int8_t size = player_size_cur;
    int8_t r;
    uint8_t j;
    int8_t px, py, nx, ny, x0, y0;

    if (size < PLAYER_SIZE_BASE) {
        size = PLAYER_SIZE_BASE;
    }
    /* Map size 6..14 -> radius ~3..7 so the circle stays readable but small. */
    r = (int8_t)(size >> 1);
    if (r < 3) {
        r = 3;
    }

    if (pl_cache_r != r) {
        for (j = 0; j < CIRCLE_SEGS; ++j) {
            pl_vx[j] = scale_radial(orbit_x[seg12[j]], r);
            pl_vy[j] = scale_radial(orbit_y[seg12[j]], r);
        }
        pl_cache_r = r;
    }

    intensity_a(127);
    reset0ref();
    x0 = (int8_t)(player_x + pl_vx[0]);
    y0 = (int8_t)(player_y + pl_vy[0]);
    px = x0;
    py = y0;
    moveto_d(py, px);
    for (j = 1; j < CIRCLE_SEGS; ++j) {
        nx = (int8_t)(player_x + pl_vx[j]);
        ny = (int8_t)(player_y + pl_vy[j]);
        draw_line_d((int8_t)(ny - py), (int8_t)(nx - px));
        px = nx;
        py = ny;
    }
    draw_line_d((int8_t)(y0 - py), (int8_t)(x0 - px));
}

/* Local frame for the bacteria, oriented along its swim heading.
   fwd = (bfx, bfy) from the orbit table (magnitude ORBIT_RADIUS),
   side = perpendicular (-bfy, bfx). A body point (a along fwd, b along
   side) maps to screen offset (a*fwd + b*side)/ORBIT_RADIUS from center. */
static int8_t bfx, bfy;

static int8_t bac_ox(int8_t a, int8_t b)
{
    int16_t o = (int16_t)((int16_t)a * bfx - (int16_t)b * bfy);
    return (int8_t)(o / ORBIT_RADIUS);
}

static int8_t bac_oy(int8_t a, int8_t b)
{
    int16_t o = (int16_t)((int16_t)a * bfy + (int16_t)b * bfx);
    return (int8_t)(o / ORBIT_RADIUS);
}

/* Compact oval body (6 pts) with a slight belly notch so it still reads as
   a microbe, not a plain ellipse. +a is the long (swim) axis. */
#define BAC_BODY_N 6
static const int8_t bac_body_a[BAC_BODY_N] = {-7, -2, 5, 7, 2, -3};
static const int8_t bac_body_b[BAC_BODY_N] = { 0,  4, 3, 0,-3, -2};

static void rebuild_bac_body(void)
{
    uint8_t j;
    uint8_t h = (uint8_t)(bac_head & (ORBIT_STEPS - 1));

    bfx = orbit_x[h];
    bfy = orbit_y[h];
    for (j = 0; j < BAC_BODY_N; ++j) {
        bac_body_sx[j] = bac_ox(bac_body_a[j], bac_body_b[j]);
        bac_body_sy[j] = bac_oy(bac_body_a[j], bac_body_b[j]);
    }
    bac_cache_head = bac_head;
}

/* Bacterium: 6-point oval + two flickering cilia drawn as true BIOS dots.
   One beam-reset for the body; two cheap Dot_d calls for the hairs. Still
   reads as a swimming microbe, ~half the line work of the old 10-pt bean. */
static void draw_bacteria(void)
{
    uint8_t j;
    int8_t px, py, nx, ny;
    int8_t x0, y0;
    int8_t hair;
    int8_t sway;
    uint8_t h;

    if (!bac_active) {
        return;
    }

    if (bac_cache_head != bac_head) {
        rebuild_bac_body();
    }

    intensity_a(95);
    reset0ref();
    x0 = (int8_t)(bac_x + bac_body_sx[0]);
    y0 = (int8_t)(bac_y + bac_body_sy[0]);
    px = x0;
    py = y0;
    moveto_d(py, px);
    for (j = 1; j < BAC_BODY_N; ++j) {
        nx = (int8_t)(bac_x + bac_body_sx[j]);
        ny = (int8_t)(bac_y + bac_body_sy[j]);
        draw_line_d((int8_t)(ny - py), (int8_t)(nx - px));
        px = nx;
        py = ny;
    }
    draw_line_d((int8_t)(y0 - py), (int8_t)(x0 - px));

    /* Cilia as single points that sway with the wiggle -- still "hairy",
       but Dot_d is far cheaper than oriented line segments. */
    h = (uint8_t)(bac_head & (ORBIT_STEPS - 1));
    bfx = orbit_x[h];
    bfy = orbit_y[h];
    hair = (int8_t)(2 + (int8_t)((bac_wiggle >> 3) & 3));
    sway = (int8_t)((int8_t)((bac_wiggle >> 2) & 3) - 1);

    intensity_a(80);
    reset0ref();
    dot_d((int8_t)(bac_y + bac_oy((int8_t)(4 + sway), (int8_t)(BAC_WID + hair))),
          (int8_t)(bac_x + bac_ox((int8_t)(4 + sway), (int8_t)(BAC_WID + hair))));
    reset0ref();
    dot_d((int8_t)(bac_y + bac_oy((int8_t)(-4 - sway), (int8_t)(-(BAC_WID + hair)))),
          (int8_t)(bac_x + bac_ox((int8_t)(-4 - sway), (int8_t)(-(BAC_WID + hair)))));
}

/* Recompute the amoeba outline offsets (relative to its center). Each point
   sits at a radius that wobbles with a wave travelling around the body
   (amo_morph) whose amplitude slowly breathes, so the shape morphs
   organically. All the per-point division lives here; it is called only
   once every AMO_REBUILD+1 frames (the blob morphs slowly, so a cache this
   coarse is imperceptible) instead of every frame. */
static void build_amoeba_pts(void)
{
    uint8_t j;
    uint8_t idx;
    uint8_t phase;
    int8_t amp;
    int8_t rj;

    amp = (int8_t)(2 + (int8_t)((amo_morph >> 3) & 3));    /* 2..5, breathing */

    for (j = 0; j < AMO_PTS; ++j) {
        idx = seg_amo[j];
        phase = (uint8_t)((j * 5 + amo_morph) & (ORBIT_STEPS - 1));
        rj = (int8_t)(AMO_BASE_R + scale_radial(orbit_x[phase], amp));
        amo_pts_x[j] = scale_radial(orbit_x[idx], rj);
        amo_pts_y[j] = scale_radial(orbit_y[idx], rj);
    }
}

/* Amoeba: a dim background blob drawn from the cached outline (add-only, no
   per-frame division). Two freckle dots ride along inside. */
static void draw_amoeba(void)
{
    uint8_t j;
    int8_t px, py, nx, ny, x0, y0;

    if (!amo_active) {
        return;
    }

    if (!amo_cache_valid || (amo_morph & AMO_REBUILD) == 0) {
        build_amoeba_pts();
        amo_cache_valid = 1;
    }

    intensity_a(45);
    reset0ref();
    x0 = (int8_t)(amo_x + amo_pts_x[0]);
    y0 = (int8_t)(amo_y + amo_pts_y[0]);
    px = x0;
    py = y0;
    moveto_d(py, px);
    for (j = 1; j < AMO_PTS; ++j) {
        nx = (int8_t)(amo_x + amo_pts_x[j]);
        ny = (int8_t)(amo_y + amo_pts_y[j]);
        draw_line_d((int8_t)(ny - py), (int8_t)(nx - px));
        px = nx;
        py = ny;
    }
    draw_line_d((int8_t)(y0 - py), (int8_t)(x0 - px));

    /* True single-point freckles via BIOS Dot_d -- no square ticks. */
    intensity_a(55);
    reset0ref();
    dot_d((int8_t)(amo_y + 2), (int8_t)(amo_x + 3));
    reset0ref();
    dot_d((int8_t)(amo_y - 3), (int8_t)(amo_x - 4));
}

/* Jagged lightning bolt between two captured points, flickering each frame
   for an electric look. Drawn as a single 4-segment polyline. */
static void draw_lightning(void)
{
    int8_t dx = (int8_t)(zap_px - zap_ax);
    int8_t dy = (int8_t)(zap_py - zap_ay);
    int8_t pxd = (int8_t)(-dy / 4);
    int8_t pyd = (int8_t)(dx / 4);
    int8_t s = (int8_t)((death_t & 2) ? 1 : -1);
    int8_t ax1 = (int8_t)(zap_ax + dx / 4 + (int8_t)(pxd * s));
    int8_t ay1 = (int8_t)(zap_ay + dy / 4 + (int8_t)(pyd * s));
    int8_t ax2 = (int8_t)(zap_ax + dx / 2 - (int8_t)(pxd * s));
    int8_t ay2 = (int8_t)(zap_ay + dy / 2 - (int8_t)(pyd * s));
    int8_t ax3 = (int8_t)(zap_ax + (int8_t)((dx * 3) / 4) + (int8_t)(pxd * s));
    int8_t ay3 = (int8_t)(zap_ay + (int8_t)((dy * 3) / 4) + (int8_t)(pyd * s));

    intensity_a(127);
    reset0ref();
    moveto_d(zap_ay, zap_ax);
    draw_line_d((int8_t)(ay1 - zap_ay), (int8_t)(ax1 - zap_ax));
    draw_line_d((int8_t)(ay2 - ay1), (int8_t)(ax2 - ax1));
    draw_line_d((int8_t)(ay3 - ay2), (int8_t)(ax3 - ax2));
    draw_line_d((int8_t)(zap_py - ay3), (int8_t)(zap_px - ax3));
}

static void draw_play(void)
{
    draw_amoeba();
    draw_connections();
    draw_spheres();
    draw_eye();
    draw_abort_marks();
    draw_bacteria();
    draw_player();
    /* No HUD text during play -- print_str is one of the heaviest BIOS
       calls on the Vectrex, and level 3+ already spends the frame budget
       on spheres + enemies. */
}

static void draw_title(void)
{
    /* Spaced letterforms; centered from string length (no fixed left-nudge
       that was tuned for the old unspaced 10-char name and pulled it off). */
    char *line0 = (char *)"P A R A M E C I U M";
    char *line1 = (char *)"HOLD TO JUMP";
    char *line2 = (char *)"LAND ON NUCLEI";
    char *line3 = (char *)"TETHER TO SURVIVE";
    char *line4 = (char *)"TO JUMP IS LIFE";
    char *credit = (char *)"BRIAN JONES";
    uint8_t save_h;
    uint8_t save_w;

    /* Decorative lab microbe: reuses the in-game bacteria wander/draw with
       no collision. Spawned once when the title screen opens; reset_round()
       clears bac_active when play starts. Drawn before text so the copy
       stays readable. */
    if (title_t == 0) {
        bac_active = 1;
        bac_x = -55;
        bac_y = 35;
        bac_head = 10;
        bac_wiggle = 0;
        bac_turn_t = BAC_TURN_FRAMES;
        bac_cache_head = -1;
    }
    update_bacteria();
    draw_bacteria();

    /* Game name brighter (bold); rest dimmed. */
    zero_beam();
    intensity_a(0x7f);
    print_str_c(62, (int8_t)(text_center_x(line0) - 5), line0);
    intensity_a(0x50);
    /* How-to lines: two glyphs (~20) right of the old title-shift nudge. */
    print_str_c(15, (int8_t)(text_center_x(line1) - (TEXT_SHIFT_TITLE - 20)), line1);
    print_str_c(-5, (int8_t)(text_center_x(line2) - (TEXT_SHIFT_TITLE - 20)), line2);
    print_str_c(-25, (int8_t)(text_center_x(line3) - (TEXT_SHIFT_TITLE - 20)), line3);
    print_str_c(-50, (int8_t)(text_center_x(line4) - (TEXT_SHIFT_TITLE - 20)), line4);

    /* Credit line, centered at the bottom in a smaller font. */
    save_h = VEC_TEXT_HEIGHT;
    save_w = VEC_TEXT_WIDTH;
    intensity_a(0x40);
    set_text_size(-6, 50);
    print_str_c(-100, text_center_x(credit), credit);
    VEC_TEXT_HEIGHT = save_h;
    VEC_TEXT_WIDTH = save_w;

    update_menu_button_arm(STATE_TITLE);
    title_t++;
    if (title_t > 30 && menu_button_fire()) {
        reset_menu_button();
        start_level(level_num);
    }
}

static void draw_win(void)
{
    char *connected_line = (char *)"CONNECTED!";
    char *done_line = (char *)"ALL LEVELS DONE";
    char *btn_line = (char *)"PRESS BTN1";

    update_menu_button_arm(STATE_WIN);

    if (level_num >= MAX_LEVELS) {
        draw_label(20, (int8_t)(text_center_x(done_line) - TEXT_SHIFT_MENU), done_line);
        draw_label(-10, (int8_t)(text_center_x(btn_line) - TEXT_SHIFT_MENU), btn_line);
        if (menu_button_fire()) {
            reset_menu_button();
            level_num = 1;
            title_t = 0;
            state = STATE_TITLE;
        }
        return;
    }

    draw_label(0, (int8_t)(text_center_x(connected_line) - TEXT_SHIFT_MENU), connected_line);
    win_t++;
    if (win_t >= WIN_DELAY) {
        start_level((uint8_t)(level_num + 1));
    }
}

/* TEMP: append a signed decimal to buf (no stdio on Vectrex). */
static void dbg_append_num(char *buf, uint8_t *pn, int16_t v)
{
    uint8_t n = *pn;
    uint8_t d[6];
    uint8_t k = 0;
    uint16_t u;

    if (v < 0) {
        buf[n++] = '-';
        u = (uint16_t)(-v);
    } else {
        u = (uint16_t)v;
    }
    do {
        d[k++] = (uint8_t)(u % 10);
        u = (uint16_t)(u / 10);
    } while (u > 0);
    while (k > 0) {
        buf[n++] = (char)('0' + d[--k]);
    }
    *pn = n;
}

static void draw_fail(void)
{
    char *retry_line = (char *)"TRY AGAIN?";
    char *msg;
    char dbuf[32];
    uint8_t n;

    update_menu_button_arm(STATE_FAIL);
    if (fail_reason == 1) {
        msg = (char *)"BURST!";
    } else if (fail_reason == 2) {
        msg = (char *)"ZAPPED!";
    } else {
        msg = (char *)"FELL OFF SLIDE";
    }
    draw_label(25, (int8_t)(text_center_x(msg) - TEXT_SHIFT_MENU), msg);
    draw_label(0, (int8_t)(text_center_x(retry_line) - TEXT_SHIFT_MENU), retry_line);

    /* TEMP death telemetry: site + state snapshot at the instant of death. */
    n = 0;
    dbuf[n++] = 'S'; dbg_append_num(dbuf, &n, (int16_t)dbg_site);
    dbuf[n++] = ' '; dbuf[n++] = 'T'; dbg_append_num(dbuf, &n, dbg_jt);
    dbuf[n++] = ' '; dbuf[n++] = 'M'; dbg_append_num(dbuf, &n, (int16_t)dbg_mode);
    dbuf[n++] = ' '; dbuf[n++] = 'A'; dbg_append_num(dbuf, &n, (int16_t)dbg_ab);
    dbuf[n] = 0;
    draw_label(-35, (int8_t)(text_center_x(dbuf) - TEXT_SHIFT_MENU), dbuf);
    n = 0;
    dbuf[n++] = 'X'; dbg_append_num(dbuf, &n, (int16_t)dbg_ax);
    dbuf[n++] = ' '; dbuf[n++] = 'Y'; dbg_append_num(dbuf, &n, (int16_t)dbg_ay);
    dbuf[n] = 0;
    draw_label(-55, (int8_t)(text_center_x(dbuf) - TEXT_SHIFT_MENU), dbuf);
    n = 0;
    dbuf[n++] = 'V'; dbg_append_num(dbuf, &n, (int16_t)dbg_minsp);
    dbuf[n++] = ' '; dbuf[n++] = 'L'; dbg_append_num(dbuf, &n, (int16_t)dbg_ldmin);
    dbuf[n++] = ' '; dbuf[n++] = 'F'; dbg_append_num(dbuf, &n, (int16_t)dbg_frames);
    dbuf[n] = 0;
    draw_label(-75, (int8_t)(text_center_x(dbuf) - TEXT_SHIFT_MENU), dbuf);
    n = 0;
    dbuf[n++] = 'F'; dbuf[n++] = 'R'; dbuf[n++] = 'M';
    dbg_append_num(dbuf, &n, (int16_t)dbg_sx);
    dbuf[n++] = ' '; dbg_append_num(dbuf, &n, (int16_t)dbg_sy);
    dbuf[n++] = ' '; dbuf[n++] = 'D'; dbg_append_num(dbuf, &n, (int16_t)dbg_dx);
    dbuf[n++] = ' '; dbg_append_num(dbuf, &n, (int16_t)dbg_dy);
    dbuf[n] = 0;
    draw_label(-95, (int8_t)(text_center_x(dbuf) - TEXT_SHIFT_MENU), dbuf);

    if (menu_button_fire()) {
        reset_menu_button();
        start_level(level_num);
        return;
    }

    /* Idle too long: reset the game and return to the intro screen. */
    fail_t++;
    if (fail_t >= FAIL_TIMEOUT) {
        reset_menu_button();
        level_num = 1;
        title_t = 0;
        state = STATE_TITLE;
    }
}

int main(void)
{
    state = STATE_TITLE;
    title_t = 0;
    level_num = 1;
    random_seed(0x4a, 0x7c, 0x2f);

    while (1) {
        begin_frame();

        switch (state) {
        case STATE_TITLE:
            draw_title();
            break;
        case STATE_PLAY:
            update_play();
            if (state == STATE_PLAY) {
                draw_play();
            }
            break;
        case STATE_DEATH:
            draw_death();
            update_death();
            break;
        case STATE_INTRO:
            draw_level_anim(anim_t);
            if (anim_t >= ANIM_MAX) {
                state = STATE_PLAY;
            } else {
                anim_t++;
            }
            break;
        case STATE_OUTRO:
            draw_level_anim(anim_t);
            if (anim_t == 0) {
                win_t = 0;
                state = STATE_WIN;
            } else {
                anim_t--;
            }
            break;
        case STATE_WIN:
            draw_win();
            break;
        case STATE_FAIL:
            draw_fail();
            break;
        default:
            break;
        }
    }

    return 0;
}
