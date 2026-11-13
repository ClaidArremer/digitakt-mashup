/* SPDX-License-Identifier: GPL-2.0-or-later */
/* MASHUP (formerly STUTTER) for the Digitakt mk1 (OS 1.54): performance repeats and effects on the trig keys.
 *
 * An always-on buffer of the master mix (core 4.1's ev_render_master, before GESTALT), 5.4 s of
 * stereo in the recorder's own RAM. On the normal screen (no grid recording, no keyboard), holding
 * trig key 9-14 repeats the last 1/2 bar, 1/4, 1/8, 1/16, 1/32 or 1/8 triplet, from the next 16th;
 * letting go returns to the live mix on the next 16th. Key 15 (REV) plays a repeat backwards, or on
 * its own the recent past backwards; key 16 (HALF) plays a repeat at half speed, or on its own stops
 * the mix like a tape machine. The sequencer runs on underneath, so nothing loses its place.
 *
 * The buffer is the recorder's: STUTTER writes and plays only while the recorder is idle (not armed,
 * not recording), and a take left on the RECORDER page is overwritten by it.
 *
 * Everything here is the render's (interrupt level) except st_key and st_evkey (UI task), which only
 * set the held keys. Firmware addresses are OS 1.54's, found in digiemu (README, "How it works"). */
#include "digitakt-mk1/core3.h"

/* ---- OS 1.54's places ------------------------------------------------------------------------- */
#define REC_BUF16  0x4237ef90u          /* the recorder's 16-bit buffer, 1,584,000 frames (33 s) */
#define REC_STATE  (*(volatile int32_t *)0x4199f114)    /* 0 idle, 1 armed, 2 recording */
#define TIMELINE   (*(volatile uint32_t *)0x80001f4c)   /* +2 x tempo a block; 5,400,000 a 16th */
#define TEMPO      (*(volatile int32_t *)0x4020df10)    /* BPM x 120 */
#define U16TH      5400000

/* ---- the buffer: 1 MB inside the recorder's, stereo 16-bit --------------------------------------- */
#define ST_BUF     ((int16_t *)0x42380000)
#define ST_N       262144               /* frames: 5.46 s */
#define ST_MASK    (ST_N - 1)
#define XF         64                   /* crossfade, frames */

#ifdef ST_HOST                          /* test/host.c gives these */
extern int16_t st_host_buf[];
extern int32_t st_host_state, st_host_tempo;
extern uint32_t st_host_timeline;
#undef ST_BUF
#undef REC_STATE
#undef TIMELINE
#undef TEMPO
#define ST_BUF     st_host_buf
#define REC_STATE  st_host_state
#define TIMELINE   st_host_timeline
#define TEMPO      st_host_tempo
#endif

/* ---- the keys and the mode (UI task) ------------------------------------------------------------ */
#define K_TRIG1    24                   /* trig key 1's id; 9 is 32 */
#define K_FIRST    32
#define K_SAMPLE   7                    /* the SAMPLING key */
#define K_REV      6                    /* trig key 15 */
#define K_HALF     7                    /* trig key 16 */

/* OS 1.54's places for the mode, its pop-up and its lights (README, "How it works") */
#define FW_KB_ON(v)    (((int8_t (*)(void *))0x40028e1a)(v))            /* keyboard mode */
#define FW_GRID_ON(o)  (((int8_t (*)(void *))0x4009359c)(o))            /* grid recording, of view+152 */
#define FW_MESSAGE     ((void (*)(void *, const char *, ...))0x40093ab0) /* the header line's message */
#define FW_PALETTE     ((void (*)(int32_t, int32_t, int32_t, int32_t))0x400e677e) /* entry, r, g, b */
#define FW_PAL_SHADOW  ((volatile uint32_t *)0x4020dcb0)                /* 41 x 0x00RRGGBB, as sent */

static volatile uint8_t st_held;        /* bit k: trig key 9 + k is held */
static volatile uint8_t st_fx;          /* bit k: trig key 1 + k is held (STUTTER 0.2's effects) */
static volatile int8_t st_len = -1;     /* the length key that counts (the last pressed), or -1 */
static volatile int8_t st_len2 = -1;    /* another length key on with it (the two alternate), or -1 */
static volatile uint8_t st_mode;        /* FUNC + SAMPLING: the trig keys are STUTTER's */
static uint8_t st_took_sample;          /* the SAMPLING press was ours, so its release is too */
static void *st_view;                   /* the main view, as it passes */

/* ---- the lights ----
 * LED index (the firmware's: the panel LED + 1): trig keys 1-16 are 1-16, SAMPLING is 36.
 * Colours are palette entries; held keys light white. PINK borrows entry 13 while the mode is on. */
#define L_SAMPLE   36
#define C_WHITE    20
#define C_PINK     13
#define C_STEP     3                    /* red: the normal screen's playing step */
static const uint8_t st_colour[16] = {
    19, 15, 14, 18,                     /* SCRAMBLE light blue, 1/8 blue, +REV bright blue, MIX teal */
    10, 9, 23, 21,                      /* PITCH down dark green, PITCH up green, ROLL yellow, BACKSPIN white (dim) */
    33, 33, 32, 32, 31, 31,             /* the lengths: pale, mid, strong orange */
    27, C_PINK                          /* REV purple, HALF (and HALF-TIME) pink */
};
#define C_MODE     26                   /* SAMPLING while the mode is on: purple */
static uint8_t st_stock[45];            /* the colour each LED was last given by the firmware */
static uint32_t st_pink_was;            /* entry 13 before the mode borrowed it */

/* Pattern or bank selection (PTN held, BANK latched): the main view's selection object (view+256) has
 * byte 204 set. The trig keys are then the firmware's, to choose with, and so are their lights. */
static int32_t st_nav(const void *v)
{
    const uint8_t *sel = *(const uint8_t *const *)((const char *)v + 256);
    return sel && sel[204];
}

static volatile uint8_t st_func;        /* FUNC is held */

int32_t st_lit(void)                    /* the keys show MASHUP: mode on, the normal screen */
{
    void *v = st_view;
    return st_mode && v && !st_func && !st_nav(v)
           && !FW_GRID_ON(*(void **)((char *)v + 152)) && !FW_KB_ON(v);
}

static int32_t st_led_colour(int32_t led, int32_t colour)
{
    if (led == L_SAMPLE)
        return st_mode ? C_MODE : colour;
    if (led < 1 || led > 16 || !st_lit())
        return colour;
    if (led <= 8 ? (st_fx >> (led - 1)) & 1 : (st_held >> (led - 9)) & 1)
        return C_WHITE;
    if (colour == C_STEP)
        return colour;                  /* the sequencer's step, moving across the keys */
    return st_colour[led - 1];
}

/* From ledhook.s, on every LED the firmware sets: a points at the call's (mgr, led, mode, colour), which
 * this may change. Each UI frame the firmware clears the LEDs, the screens set theirs (mode 2), and a
 * last pass gives every LED nobody set mode 1, which the set turns into colour 2, off. While the mode
 * shows, STUTTER's keys keep their colour through that pass too (mode 2), so all sixteen light. */
#define FW_LEDS_MUTE   0x4003618cu      /* the callers that own the trig keys' lights: mute mode and FUNC */
#define FW_LEDS_PATTERN 0x40095772u     /* held, and pattern and bank selection */

void st_led(int32_t *a)
{
    int32_t led = a[1], c;
    uint32_t from = (uint32_t)a[-1];    /* the set's return address */
    if (led != L_SAMPLE && (led < 1 || led > 16))
        return;
    if (from == FW_LEDS_MUTE || from == FW_LEDS_PATTERN)
        return;                         /* their lights, as stock */
    st_stock[led] = (uint8_t)(a[2] == 1 ? 2 : a[3]);
    c = st_led_colour(led, a[3]);
    if (c != a[3] || (a[2] == 1 && (led == L_SAMPLE ? st_mode : st_lit()))) {
        a[3] = c;
        if (a[2] == 1)
            a[2] = 2;
    }
}

/* The keys as STUTTER sees them: held (st_phys) or latched (st_latch), bit k for trig key 9 + k (k 0-7)
 * and trig key 1 + k - 8 (k 8-15). SAMPLING held + a trig key latches it (or unlatches it); a latched
 * key pressed again unlatches; SAMPLING tapped on its own unlatches them all. */
static uint16_t st_phys, st_latch;
static uint8_t st_sample_held, st_sample_used;

static void st_update(int32_t newlen)
{
    uint16_t on = st_phys | st_latch;
    st_held = (uint8_t)on;
    st_fx = (uint8_t)(on >> 8);
    if (newlen >= 0)
        st_len = (int8_t)newlen;
    else if (st_len < 0 || !(on & (1u << st_len))) {  /* fall back to another length still on */
        int32_t j, n = -1;
        for (j = 0; j < K_REV; j++)
            if (on & (1u << j))
                n = j;
        st_len = (int8_t)n;
    }
    {
        int32_t j, n = -1;
        for (j = K_REV - 1; j >= 0; j--)
            if ((on & (1u << j)) && j != st_len)
                n = j;
        st_len2 = (int8_t)(st_len >= 0 ? n : -1);
    }
}

static int32_t st_set(int32_t k, int32_t down)  /* returns 1 if the press latched the key, 2 unlatched */
{
    uint16_t bit = (uint16_t)(1u << k);
    int32_t latched = 0;
    if (!down) {
        st_phys &= (uint16_t)~bit;
        st_update(-1);
        return 0;
    }
    if (st_sample_held) {
        st_sample_used = 1;
        st_latch ^= bit;
        latched = (st_latch & bit) ? 1 : 2;
    } else if (st_latch & bit) {
        st_latch &= (uint16_t)~bit;
        latched = 2;
    }
    st_phys |= bit;
    st_update(k < K_REV ? k : -1);
    return latched;
}

/* ---- the knobs: while a STUTTER key or SAMPLING is held, A-H shape what plays; a push resets ---- */
#define K_PUSH1    40                   /* knob A's push; H is 47 */
static volatile int16_t kn_bend;        /* A: quarter-tones, -96..96 */
static volatile int16_t kn_filt;        /* B: -100 low-pass .. 0 off .. 100 high-pass */
static volatile int16_t kn_decay;       /* (C, DECAY, is gone: always 0) */
static volatile int16_t kn_crush;       /* D: 0..100 */
static volatile int16_t kn_step = 1;    /* E: PITCH's step, 1..12 semitones */
static volatile int16_t kn_gate = 100;  /* F: the part of each pass that plays, 10..100 % */
static volatile int16_t kn_rev;         /* G: the chance a pass plays backwards, 0..100 % */
static volatile int16_t kn_mix = 100;   /* H: the effect against the live mix, 0..100 % */
static volatile int32_t kn_fa = 32767;  /* derived: the filter's coefficient (Q15), crush's bits and hold */
static volatile int16_t kn_bits, kn_hold = 1;
static uint8_t kn_took;                 /* knob pushes taken, so their releases are too */

static void kn_derive(void)
{
    int32_t a = 32767, i, f = kn_filt < 0 ? -kn_filt : kn_filt;
    for (i = 0; i < f; i++)
        a = a * 95 / 100;
    kn_fa = a;
    kn_bits = (int16_t)(kn_crush * 12 / 100);
    kn_hold = (int16_t)(1 + kn_crush / 12);
}

static void st_knobs_reset(void)
{
    kn_bend = kn_filt = kn_decay = kn_crush = kn_rev = 0;
    kn_step = 1;
    kn_gate = kn_mix = 100;
    kn_derive();
}

static int32_t st_knobs_on(void)
{
    return st_mode && (st_phys || st_sample_held) && st_lit();
}

static int16_t kn_clamp(int32_t v, int32_t lo, int32_t hi)
{
    return (int16_t)(v < lo ? lo : v > hi ? hi : v);
}

static void kn_show(int32_t k)
{
    static const char *const q[4] = {"00", "25", "50", "75"};
    void *v = st_view;
    void *m;
    if (!v)
        return;
    m = *(void **)((char *)v + 152);
    switch (k) {
    case 0: {
        int32_t b = kn_bend, a = b < 0 ? -b : b;
        FW_MESSAGE(m, "BEND %s%d.%s", b < 0 ? "-" : "+", a >> 2, q[a & 3]);
        break;
    }
    case 1:
        if (kn_filt)
            FW_MESSAGE(m, "FILTER %s %d", kn_filt < 0 ? "LP" : "HP", kn_filt < 0 ? -kn_filt : kn_filt);
        else
            FW_MESSAGE(m, "FILTER OFF");
        break;
    case 2: FW_MESSAGE(m, "DECAY %d", kn_decay); break;
    case 3: FW_MESSAGE(m, "CRUSH %d", kn_crush); break;
    case 4: FW_MESSAGE(m, "PITCH STEP %d", kn_step); break;
    case 5: FW_MESSAGE(m, "GATE %d", kn_gate); break;
    case 6: FW_MESSAGE(m, "REV CHANCE %d", kn_rev); break;
    default: FW_MESSAGE(m, "MIX %d", kn_mix); break;
    }
}

static void kn_turn(int32_t k, int32_t d)
{
    switch (k) {
    case 0: kn_bend = kn_clamp(kn_bend + d, -96, 96); break;
    case 1: kn_filt = kn_clamp(kn_filt + d, -100, 100); break;
    case 2: kn_decay = kn_clamp(kn_decay + d, 0, 100); break;
    case 3: kn_crush = kn_clamp(kn_crush + d, 0, 100); break;
    case 4: kn_step = kn_clamp(kn_step + d, 1, 12); break;
    case 5: kn_gate = kn_clamp(kn_gate + d, 10, 100); break;
    case 6: kn_rev = kn_clamp(kn_rev + d, 0, 100); break;
    default: kn_mix = kn_clamp(kn_mix + d, 0, 100); break;
    }
    kn_derive();
}

static void kn_reset(int32_t k)
{
    switch (k) {
    case 0: kn_bend = 0; break;
    case 1: kn_filt = 0; break;
    case 2: kn_decay = 0; break;
    case 3: kn_crush = 0; break;
    case 4: kn_step = 1; break;
    case 5: kn_gate = 100; break;
    case 6: kn_rev = 0; break;
    default: kn_mix = 100; break;
    }
    kn_derive();
}

/* ev_enc: a knob turned. ev+12 the knob (1-8, A-H), ev+16 how far */
int32_t st_enc(void *brain, void *ev)
{
    int32_t e = *(int32_t *)((char *)ev + 12), d = *(int32_t *)((char *)ev + 16);
    (void)brain;
    if (e < 1 || e > 8 || e == 3 || !st_knobs_on())
        return 0;                       /* C is the page's */
    if (st_sample_held)
        st_sample_used = 1;
    kn_turn(e - 1, d);
    kn_show(e - 1);
    return 1;
}

static void st_toggle(void)
{
    void *v = st_view;
    st_mode ^= 1;
    st_phys = st_latch = 0;
    st_sample_held = 0;
    st_held = 0;
    st_fx = 0;
    st_len = -1;
    st_len2 = -1;
    st_knobs_reset();
    if (st_mode) {                      /* borrow a palette entry for pink */
        st_pink_was = FW_PAL_SHADOW[C_PINK];
        FW_PALETTE(C_PINK, 15, 2, 6);
    } else {
        uint32_t w = st_pink_was;
        FW_PALETTE(C_PINK, (w >> 16) & 0xff, (w >> 8) & 0xff, w & 0xff);
    }
    if (v)
        FW_MESSAGE(*(void **)((char *)v + 152), "MASHUP %s", st_mode ? "ON" : "OFF");
}

static void st_announce_latch(void);

/* From viewhook.s, every key the main view handles, before it does: FUNC + SAMPLING is the mode's. */
int32_t st_view_key(void *view, void *ev)
{
    int32_t key = *(int32_t *)((char *)ev + 12), fl = *(int32_t *)((char *)ev + 16);
    st_view = view;
    if (key != K_SAMPLE)
        return 0;
    if ((fl & 1) && (fl & 2) && !(fl & 8)) {
        st_took_sample = 1;
        st_toggle();
        return 1;
    }
    if (st_took_sample) {               /* its repeats and its release */
        if (!(fl & 1))
            st_took_sample = 0;
        return 1;
    }
    if (!st_mode)
        return 0;
    if (fl & 8)                         /* SAMPLING in the mode is the latch key */
        return 1;
    if (fl & 1) {
        st_sample_held = 1;
        st_sample_used = 0;
        if (st_phys) {                  /* keys held: latch them as they are, without a break */
            st_latch |= st_phys;
            st_sample_used = 1;
            st_update(-1);
            st_announce_latch();
        }
    } else {
        st_sample_held = 0;
        if (!st_sample_used && st_latch) {
            st_latch = 0;
            st_update(-1);
            if (view)
                FW_MESSAGE(*(void **)((char *)view + 152), "LATCH OFF");
        }
    }
    return 1;
}

/* The top line names what the held keys play, on each press: the same order as st_want (render). */
static void st_announce(const char *pre)
{
    static const char *const len_name[6] = {"1/2 BAR", "1/4", "1/8", "1/16", "1/32", "1/8 TRIPLET"};
    int32_t h = st_held, fx = st_fx, len = st_len, rev = (h >> K_REV) & 1, half = (h >> K_HALF) & 1;
    const char *a = "", *b = "", *c = "", *d = "";
    void *v = st_view;
    if (!v)
        return;
    if (fx & 0x80)
        a = rev ? "TAPE START" : half ? "SLOW SPIN" : "BACKSPIN";
    else if (fx & 0x0f) {
        a = (fx & 1) ? "SCRAMBLE" : (fx & 2) ? "SCRAMBLE 1/8" : (fx & 4) ? "SCRAMBLE REV" : "SCRAMBLE MIX";
        if (len >= 0) {
            a = (fx & 4) ? "SCRAMBLE REV " : (fx & 8) ? "SCRAMBLE MIX " : "SCRAMBLE ";
            b = len_name[len];
        }
    } else if (len >= 0 || (fx & 0x40)) {
        a = len >= 0 ? len_name[len] : "ROLL";
        b = st_len2 >= 0 ? (st_len2 == 0 ? "+1/2" : st_len2 == 1 ? "+1/4" : st_len2 == 2 ? "+1/8"
                            : st_len2 == 3 ? "+1/16" : st_len2 == 4 ? "+1/32" : "+1/8T")
                         : rev ? " REV" : "";
        c = half ? " HALF" : "";
        d = (fx & 0x30) == 0x30 ? " WARBLE" : (fx & 0x20) ? " PITCH UP" : (fx & 0x10) ? " PITCH DOWN" : "";
    } else if (rev) {
        a = "REVERSE";
        b = half ? " HALF" : "";
    } else if (half)
        a = "HALF-TIME";
    else if (fx & 0x30)
        a = (fx & 0x20) ? "PITCH UP: +LENGTH" : "PITCH DN: +LENGTH";
    else
        return;
    FW_MESSAGE(*(void **)((char *)v + 152), "%s%s%s%s%s", pre, a, b, c, d);
}

static void st_announce_latch(void)
{
    st_announce("LATCH ");
}

/* From keyhook.s, on the normal screen's trig key path (press and release, no repeats): nonzero takes
 * the key. Mode off: none. Mode on: all sixteen. */
int32_t st_key(void *ev)
{
    int32_t key = *(int32_t *)((char *)ev + 12), fl = *(int32_t *)((char *)ev + 16);
    if (!st_mode || key < K_TRIG1 || key > K_FIRST + 7 || (st_view && st_nav(st_view)))
        return 0;
    if (fl & 1) {                       /* the event's own FUNC flag decides, and corrects st_func */
        st_func = (uint8_t)((fl & 2) != 0);
        if (st_func)
            return 0;
    }
    {
        int32_t l = st_set(key >= K_FIRST ? key - K_FIRST : key - K_TRIG1 + 8, fl & 1);
        if (fl & 1)
            st_announce(l == 1 ? "LATCH " : l == 2 ? "UNLATCH " : "");
    }
    return 1;
}

/* ev_key, every key on every screen, never taken: a release of a held trig key wherever it lands
 * (after FUNC, a menu or a page change), so no key stays held. */
int32_t st_evkey(void *brain, void *ev)
{
    int32_t key = *(int32_t *)((char *)ev + 12), fl = *(int32_t *)((char *)ev + 16);
    (void)brain;
    if (key == 1) {                     /* FUNC: the keys and lights are the firmware's while it is held */
        if (!(fl & 8))                  /* a repeat says nothing (it can trail the release) */
            st_func = (uint8_t)(fl & 1);
    } else if ((fl & 1) && !(fl & 2))   /* any other key down without the FUNC flag: FUNC is up */
        st_func = 0;
    if (key >= K_PUSH1 && key < K_PUSH1 + 8 && key != K_PUSH1 + 2) {     /* a knob pushed: reset it */
        uint8_t bit = (uint8_t)(1u << (key - K_PUSH1));
        if ((fl & 1) && !(fl & 8) && st_knobs_on()) {
            if (st_sample_held)
                st_sample_used = 1;
            kn_took |= bit;
            kn_reset(key - K_PUSH1);
            kn_show(key - K_PUSH1);
            return 1;
        }
        if (kn_took & bit) {
            if (!(fl & 1))
                kn_took &= (uint8_t)~bit;
            return 1;
        }
        return 0;
    }
    if (key >= K_TRIG1 && key <= K_FIRST + 7 && !(fl & 1) && (fl & 0x10))
        st_set(key >= K_FIRST ? key - K_FIRST : key - K_TRIG1 + 8, 0);
    return 0;
}

/* ---- the voices (render) ------------------------------------------------------------------------ */
enum { V_LIVE, V_LOOP, V_TAPE, V_RUN };

struct voice {
    int32_t type;
    int32_t s, l, l2, plen, o, pass;    /* LOOP: slice start, its lengths (l2 alternates, 0 none), this
                                           pass's length, the place in it, passes played */
    int32_t rev, half, prev;            /* backwards, half speed; this pass backwards (REV CHANCE) */
    int32_t semis, dsemi, warble;       /* PITCH: semitones now, the change a pass, alternating */
    int32_t pi, pf, pg, gend, rate;     /* the read place in the pass and its fraction, the pass's level
                                           (DECAY, Q15), where GATE closes it, the read speed (65536 = 1) */
    int32_t pos, frac, speed, decel;    /* TAPE and RUN: the ring frame, 1/65536 of one, speed 65536 = 1 */
    int32_t gfade, gone;                /* the level follows the speed; RUN: frames travelled */
    int32_t keep;                       /* LOOP: the ring keeps recording (REV's beats: all in the past) */
};

static struct voice cur, old;           /* old: the voice fading out, for xf frames */

static void st_copy(struct voice *d, const struct voice *v)     /* no memcpy in a mod */
{
    int32_t *a = (int32_t *)d, i;
    const int32_t *b = (const int32_t *)v;
    for (i = 0; i < (int32_t)(sizeof *d / 4); i++)
        a[i] = b[i];
}

static void st_zero(struct voice *d)
{
    int32_t *a = (int32_t *)d, i;
    for (i = 0; i < (int32_t)(sizeof *d / 4); i++)
        a[i] = 0;
}

static int32_t xf;                      /* 0: no crossfade; else frames left of it */
static int32_t wp;                      /* the next frame written */
static int32_t applied = 0;             /* the configuration playing (st_want), 0 live */
static uint32_t last_tl;
static int32_t st_l16 = 7347;           /* a 16th, in frames (98 BPM to start) */
static int32_t st_t = 11760;            /* the tempo, BPM x 120 */
static int32_t st_ph = 7347;            /* frames to the next 16th */
static int32_t st_n16;                  /* 16ths since the configuration started (ROLL) */
static uint32_t st_rand = 0x2545f491u;
static int32_t st_left;                 /* 16ths left of this SCRAMBLE slice */
static int32_t st_g16, st_gnext;        /* the pattern's 16th (from the timeline), and the next's */

/* the knobs as this block plays them */
static int32_t b_bend, b_gate, b_rev, b_decay, b_step, b_fa, b_filt, b_mask, b_hold, b_mix;
static int32_t st_w;                    /* the effect's share, 0..XF: ramps in and out */
static int32_t f_l, f_r, h_l, h_r, h_n; /* the filter's state; crush's held sample and its count */

/* the configuration: kind | len << 4 | rev << 8 | half << 9 | pitch << 10 (1 up, 2 down, 3 warble)
 * | scramble (1-4) << 12 | sized << 15 | len2 << 16 (15 none) */
enum { C_LIVE, C_LOOP, C_UNUSED, C_SWOOP, C_HALFTIME, C_BACKSPIN, C_SCRAMBLE };
#define F_TOP_SCR    0x0f               /* trig keys 1-4 */
#define F_DOWN       0x10               /* trig key 5 */
#define F_UP         0x20               /* trig key 6 */
#define F_ROLL       0x40
#define F_BACKSPIN   0x80

static int32_t st_want(void)
{
    int32_t h = st_held, fx = st_fx, len = st_len, len2 = st_len2, rev = (h >> K_REV) & 1,
            half = (h >> K_HALF) & 1, pitch;
    if (fx & F_BACKSPIN)
        return C_BACKSPIN | rev << 8 | half << 9;
    if (fx & F_TOP_SCR) {
        int32_t which = (fx & 1) ? 1 : (fx & 2) ? 2 : (fx & 4) ? 3 : 4;
        return C_SCRAMBLE | which << 12 | (len >= 0 ? 1 << 15 | len << 4 : 0);
    }
    if (len < 0 && (fx & F_ROLL)) {
        static const int8_t roll[4] = {1, 2, 3, 4};         /* 1/4 > 1/8 > 1/16 > 1/32, a beat each */
        len = roll[st_n16 >= 12 ? 3 : st_n16 >> 2];
        len2 = -1;
    }
    pitch = (fx & (F_UP | F_DOWN)) == (F_UP | F_DOWN) ? 3 : (fx & F_UP) ? 1 : (fx & F_DOWN) ? 2 : 0;
    if (len >= 0)
        return C_LOOP | len << 4 | rev << 8 | half << 9 | pitch << 10 | (len2 >= 0 ? len2 : 15) << 16;
    if (rev)
        return C_SWOOP | half << 9;
    if (half)
        return C_HALFTIME;
    return C_LIVE;
}

static int32_t st_frames(int32_t len, int32_t tempo)
{
    static const int32_t num[6] = {691200000, 345600000, 172800000, 86400000, 43200000, 115200000};
    int32_t f = num[len] / tempo;       /* 1/2, 1/4, 1/8, 1/16, 1/32 bar; 1/8 triplet */
    if (f > ST_N / 2 - 4 * XF)
        f = ST_N / 2 - 4 * XF;
    return f < 4 * XF ? 4 * XF : f;
}

static int32_t st_tempo(int32_t delta)
{
    int32_t t = delta ? (int32_t)(delta >> 1) : TEMPO;
    return t < 3000 ? 3000 : t > 36000 ? 36000 : t;     /* 25-300 BPM */
}

/* 65536 x 2^(q/48), q = -96..96 quarter-tones */
static const int32_t st_qratio[193] = {
    16384, 16622, 16864, 17109, 17358, 17611, 17867, 18127, 18390, 18658, 18929, 19205,
    19484, 19767, 20055, 20347, 20643, 20943, 21247, 21556, 21870, 22188, 22511, 22838,
    23170, 23507, 23849, 24196, 24548, 24905, 25268, 25635, 26008, 26386, 26770, 27159,
    27554, 27955, 28362, 28774, 29193, 29618, 30048, 30485, 30929, 31379, 31835, 32298,
    32768, 33245, 33728, 34219, 34716, 35221, 35734, 36254, 36781, 37316, 37859, 38409,
    38968, 39535, 40110, 40693, 41285, 41886, 42495, 43113, 43740, 44376, 45022, 45677,
    46341, 47015, 47699, 48393, 49097, 49811, 50535, 51270, 52016, 52773, 53540, 54319,
    55109, 55911, 56724, 57549, 58386, 59235, 60097, 60971, 61858, 62757, 63670, 64596,
    65536, 66489, 67456, 68438, 69433, 70443, 71468, 72507, 73562, 74632, 75717, 76819,
    77936, 79069, 80220, 81386, 82570, 83771, 84990, 86226, 87480, 88752, 90043, 91353,
    92682, 94030, 95398, 96785, 98193, 99621, 101070, 102540, 104032, 105545, 107080, 108638,
    110218, 111821, 113448, 115098, 116772, 118470, 120194, 121942, 123715, 125515, 127341, 129193,
    131072, 132978, 134913, 136875, 138866, 140886, 142935, 145014, 147123, 149263, 151434, 153637,
    155872, 158139, 160439, 162773, 165140, 167542, 169979, 172452, 174960, 177505, 180087, 182706,
    185364, 188060, 190795, 193571, 196386, 199243, 202141, 205081, 208064, 211090, 214161, 217276,
    220436, 223642, 226895, 230195, 233544, 236941, 240387, 243884, 247431, 251030, 254681, 258386,
    262144};

static int32_t st_qrate(int32_t q)
{
    return st_qratio[(q < -96 ? -96 : q > 96 ? 96 : q) + 96];
}

/* the ring frame i, as 16-bit stereo, read with a linear step frac (0..32767) to the next (dir +-1) */
static void st_read(int32_t i, int32_t frac, int32_t dir, int32_t *l, int32_t *r)
{
    const int16_t *a = ST_BUF + 2 * (i & ST_MASK), *b = ST_BUF + 2 * ((i + dir) & ST_MASK);
    *l = a[0] + (((b[0] - a[0]) * frac) >> 15);
    *r = a[1] + (((b[1] - a[1]) * frac) >> 15);
}

/* a plain LOOP's frame at place o of a pass (0.1's loop, crossfaded over its seam) */
static void st_loop_at(const struct voice *v, int32_t o, int32_t *l, int32_t *r)
{
    int32_t step = v->half ? o >> 1 : o, frac = v->half && (o & 1) ? 16384 : 0;
    if (v->rev)
        st_read(v->s + v->l - 1 - step, frac, -1, l, r);
    else
        st_read(v->s + step, frac, 1, l, r);
}

static uint32_t st_rnd(void)
{
    st_rand = st_rand * 1664525u + 1013904223u;
    return st_rand >> 16;
}

/* a LOOP's pass begins: its length, pitch, direction, level and gate */
static void st_pass(struct voice *v)
{
    v->plen = v->l2 && (v->pass & 1) ? v->l2 : v->l;
    v->o = v->pi = v->pf = 0;
    v->prev = b_rev && (int32_t)(st_rnd() % 100u) < b_rev;
    v->gend = v->plen / 100 * b_gate + (v->plen % 100) * b_gate / 100;
}

static void st_next_pass(struct voice *v)
{
    v->pass++;
    if (v->warble)
        v->semis = (v->pass & 1) ? b_step : -b_step;
    else if (v->dsemi) {
        v->semis += v->dsemi * b_step;
        if (v->semis > 24)
            v->semis = 24;
        if (v->semis < -24)
            v->semis = -24;
    }
    if (b_decay)
        v->pg = v->pg * (120 - b_decay) / 120;
    st_pass(v);
}

static void st_loop_rate(struct voice *v)   /* each block: PITCH and BEND, as quarter-tones */
{
    int32_t r = st_qrate(v->semis * 4 + b_bend);
    v->rate = v->half ? r >> 1 : r;
}

/* one frame of a voice, 16-bit; LIVE reads the input */
static void st_voice(struct voice *v, const int32_t *in, int32_t *l, int32_t *r)
{
    switch (v->type) {
    case V_LIVE:
        *l = in[0] >> 16;
        *r = in[1] >> 16;
        return;
    case V_LOOP: {
        int32_t o = v->o;
        if (v->rate == (v->half ? 32768 : 65536) && !v->semis && !v->dsemi && !v->warble && !v->l2
            && b_gate == 100 && !b_rev && !b_decay && v->pg == 32767) {
            int32_t a, b;               /* plain: the loop crossfaded over its seam */
            st_loop_at(v, o, l, r);
            if (o >= v->l - XF) {
                int32_t k = o - (v->l - XF);
                st_loop_at(v, k, &a, &b);
                *l += ((a - *l) * k) / XF;
                *r += ((b - *r) * k) / XF;
            }
            if (++o >= v->l)
                o = XF;
            v->o = o;
            v->pi = o;
            v->pf = 0;
            v->plen = v->l;
            return;
        }
        {                               /* resampled, gated, faded at its edges */
            int32_t plen = v->plen, q, g, back = v->rev ^ v->prev;
            while (v->pi >= plen)
                v->pi -= plen;
            q = v->pi;
            if (back)
                st_read(v->s + plen - 1 - q, v->pf >> 1, -1, l, r);
            else
                st_read(v->s + q, v->pf >> 1, 1, l, r);
            g = o < plen - 1 - o ? o : plen - 1 - o;
            if (q < g)
                g = q;
            if (plen - 1 - q < g)
                g = plen - 1 - q;
            if (o >= v->gend)
                g = 0;
            else if (v->gend - 1 - o < g)
                g = v->gend - 1 - o;
            if (g < XF) {
                *l = (*l * g) / XF;
                *r = (*r * g) / XF;
            }
            if (v->pg != 32767) {
                *l = (*l * v->pg) >> 15;
                *r = (*r * v->pg) >> 15;
            }
            v->pf += v->rate;
            v->pi += v->pf >> 16;
            v->pf &= 0xffff;
            if (++o >= plen)
                st_next_pass(v);
            else
                v->o = o;
            return;
        }
    }
    case V_TAPE: {                      /* reads behind the writes: half time, or a tape start */
        int32_t sp = v->speed;
        st_read(v->pos, v->frac >> 1, 1, l, r);
        if (v->gfade) {
            int32_t g = sp >> 1;
            *l = (*l * g) >> 15;
            *r = (*r * g) >> 15;
        }
        if (b_bend)
            sp = ((sp >> 4) * (st_qrate(b_bend) >> 4)) >> 8;
        v->frac += sp;
        v->pos = (v->pos + (v->frac >> 16)) & ST_MASK;
        v->frac &= 0xffff;
        v->speed -= v->decel;
        if (v->speed < 0)
            v->speed = 0;
        if (v->speed > 65536)
            v->speed = 65536;
        if (((wp - v->pos) & ST_MASK) > ST_N - 4096)
            v->pos = (wp - 1) & ST_MASK;        /* a bend upward caught up with the writes */
        return;
    }
    default: {                          /* V_RUN: the past, backwards (a swoop, or a backspin slowing) */
        int32_t g = v->gone > ST_N - 4096 ? 0 : 32767, sp = v->speed;
        if (v->gfade && sp < 16384)
            g = sp * 2;
        st_read(v->pos, v->frac >> 1, -1, l, r);
        *l = (*l * g) >> 15;
        *r = (*r * g) >> 15;
        if (b_bend)
            sp = ((sp >> 4) * (st_qrate(b_bend) >> 4)) >> 8;
        v->frac += sp;
        v->pos = (v->pos - (v->frac >> 16)) & ST_MASK;
        v->gone += v->frac >> 16;
        v->frac &= 0xffff;
        v->speed -= v->decel;
        if (v->speed < 0)
            v->speed = 0;
        return;
    }
    }
}

static int32_t st_frozen(const struct voice *v)
{
    return (v->type == V_LOOP && !v->keep) || v->type == V_RUN;
}

static int32_t st_writes(void)          /* the ring keeps recording: not while a slice or the past plays */
{
    return !(st_frozen(&cur) || (xf && st_frozen(&old)));
}

static int32_t st_slice_start(int32_t l)    /* a new LOOP's slice start: keep the old one's if it fits */
{
    if (cur.type == V_LOOP || (xf && old.type == V_LOOP)) {
        const struct voice *p = cur.type == V_LOOP ? &cur : &old;
        int32_t s = p->s;
        if (((wp - s) & ST_MASK) >= (uint32_t)l)
            return s;
    }
    return wp - l;
}

static void st_loop_init(struct voice *v, int32_t s, int32_t l)
{
    v->type = V_LOOP;
    v->s = s & ST_MASK;
    v->l = l;
    v->pg = 32767;
    st_pass(v);
    st_loop_rate(v);
}

/* a SCRAMBLE slice: one of the last bar's 16ths (1), 8ths (2), 16ths a third backwards (3), 16ths and
 * 8ths at random, a third backwards (4); or slices of a held length (len >= 0). From the frozen ring.
 * Returns how many 16ths it lasts. */
static int32_t st_scramble(struct voice *v, int32_t which, int32_t len)
{
    int32_t l, n, j, div;
    if (len >= 0) {
        l = st_frames(len, st_t);
        div = (l + st_l16 / 2) / st_l16;
        if (div < 1)
            div = 1;
    } else {
        div = which == 2 || (which == 4 && (st_rnd() & 1)) ? 2 : 1;
        l = st_l16 * div;
    }
    n = 16 * st_l16 / l;
    while (n > 1 && n * l > ST_N - 8 * XF)
        n--;                            /* a slow tempo: as much of the bar as the ring holds */
    if (n < 1)
        n = 1;
    j = (int32_t)(st_rnd() % (uint32_t)n);
    st_loop_init(v, wp - n * l + j * l, l < 4 * XF ? 4 * XF : l);
    v->rev = which >= 3 && st_rnd() % 3 == 0;
    return div;
}

/* the configuration c starts now (frame wp is the next to be written) */
static void st_start(int32_t c)
{
    struct voice v;
    int32_t kind = c & 15, len = (c >> 4) & 15, rev = (c >> 8) & 1, half = (c >> 9) & 1,
            pitch = (c >> 10) & 3, len2 = (c >> 16) & 15;
    st_zero(&v);
    switch (kind) {
    case C_LOOP: {
        int32_t l = st_frames(len, st_t), l2 = len2 != 15 ? st_frames(len2, st_t) : 0;
        v.rev = rev;
        v.half = half;
        v.dsemi = pitch == 1 ? 1 : pitch == 2 ? -1 : 0;
        v.warble = pitch == 3;
        v.l2 = l2;
        st_loop_init(&v, st_slice_start(l > l2 ? l : l2), l);
        break;
    }
    case C_SCRAMBLE:
        st_left = st_scramble(&v, (c >> 12) & 7, (c >> 15) & 1 ? len : -1);
        break;
    case C_SWOOP: {                     /* REV: the last beat backwards, beat by beat (HALF: over two) */
        int32_t l = (half ? 8 : 4) * st_l16;
        if (l > ST_N / 2)
            l = ST_N / 2;
        v.rev = 1;
        v.half = half;
        v.keep = 1;
        st_loop_init(&v, wp - l, l);
        break;
    }
    case C_BACKSPIN:
        if (rev) {                      /* TAPE START: from a stop up to speed in a beat */
            v.type = V_TAPE;
            v.pos = wp & ST_MASK;
            v.decel = -(65536 / (4 * st_l16) + 1);
            v.gfade = 1;
        } else {                        /* backwards at 3x (HALF: 1.5x), slowing to a stop in a beat (2) */
            v.type = V_RUN;
            v.pos = (wp - 1) & ST_MASK;
            v.speed = half ? 98304 : 196608;
            v.decel = v.speed / ((half ? 8 : 4) * st_l16);
            v.gfade = 1;
        }
        break;
    case C_HALFTIME:                    /* the mix from now, at half speed, as long as it is held */
        v.type = V_TAPE;
        v.pos = wp & ST_MASK;
        v.speed = 32768;
        break;
    default:
        break;
    }
    if (kind != (applied & 15))
        st_n16 = 0;
    st_copy(&old, &cur);
    st_copy(&cur, &v);
    xf = XF;
    applied = c;
}

/* at a 16th: a new configuration, the next SCRAMBLE slice, or nothing */
static void st_sixteenth(int32_t idle)
{
    int32_t want, kind = applied & 15;
    st_n16++;
    st_g16 = st_gnext;
    want = idle ? st_want() : 0;
    if (want != applied) {
        st_start(want);
    } else if ((kind == C_HALFTIME && !(st_g16 & 7))
               || (kind == C_SWOOP && !(st_g16 & ((applied >> 9) & 1 ? 7 : 3)))) {
        st_start(want);                 /* HALF-TIME's half bar, REV's beat: start again, in time */
    } else if ((applied & 15) == C_SCRAMBLE && --st_left <= 0) {
        struct voice v;
        st_zero(&v);
        st_left = st_scramble(&v, (applied >> 12) & 7, (applied >> 15) & 1 ? (applied >> 4) & 15 : -1);
        st_copy(&old, &cur);
        st_copy(&cur, &v);
        xf = XF;
    }
}

static void st_knobs_block(void)        /* the knobs, once a block */
{
    b_bend = kn_bend;
    b_gate = kn_gate;
    b_rev = kn_rev;
    b_decay = kn_decay;
    b_step = kn_step;
    b_filt = kn_filt;
    b_fa = kn_fa;
    b_mask = ~((1 << kn_bits) - 1);
    b_hold = kn_hold;
    b_mix = kn_mix;
    if (cur.type == V_LOOP)
        st_loop_rate(&cur);
    if (xf && old.type == V_LOOP)
        st_loop_rate(&old);
}

/* ev_render_master, order 60: before GESTALT (70), so the repeats go through it */
void st_render(int32_t *pair)
{
    uint32_t tl = TIMELINE, delta = tl - last_tl;
    int32_t i, at = 64, idle = REC_STATE == 0, now = 0;
    last_tl = tl;
    if (delta > 200000)                 /* a jump (PLAY, a reset): no tempo from it */
        delta = 0;
    st_t = st_tempo((int32_t)delta);
    st_l16 = 86400000 / st_t;

    if (delta) {                        /* playing: the next 16th from the timeline */
        uint32_t p = tl % U16TH, f = ((U16TH - p) * 32u + delta - 1) / delta;
        st_ph = p == 0 ? 0 : (int32_t)f;
    } else if (st_ph >= st_l16) {       /* stopped: 16ths of the tempo, from wherever */
        st_ph = st_l16 - 1;
    }
    if (st_ph < 32)
        at = st_ph;
    if (delta) {
        uint32_t p = tl % U16TH;
        st_gnext = (int32_t)(tl / U16TH) + (p ? 1 : 0);
    } else {
        st_gnext = st_g16 + 1;
    }
    if (!idle && applied)
        now = 1;                        /* the recorder wants its buffer: back to live at once */
    else if (!delta && (idle ? st_want() : 0) != applied)
        now = 1;                        /* stopped: changes at once */

    if (!idle && !applied && !xf && !st_w) {
        st_ph = at < 32 ? st_l16 + at - 32 : st_ph - 32;
        return;                         /* the recorder has it, and nothing of ours is sounding */
    }
    if (applied == 0 && !xf && !st_w && !now && (at >= 32 || st_want() == 0)) {
        if (at < 32)
            st_n16++;
        st_ph = at < 32 ? st_l16 + at - 32 : st_ph - 32;
        if (wp <= ST_N - 32) {          /* live: only the recording, the top halves in one word */
            uint32_t *d = (uint32_t *)(ST_BUF + 2 * wp);
            const uint32_t *q = (const uint32_t *)pair;
            for (i = 0; i < 32; i++, q += 2)
                *d++ = (q[0] & 0xffff0000u) | (q[1] >> 16);
            wp = (wp + 32) & ST_MASK;
        } else {
            for (i = 0; i < 32; i++) {
                ST_BUF[2 * wp] = (int16_t)(pair[2 * i] >> 16);
                ST_BUF[2 * wp + 1] = (int16_t)(pair[2 * i + 1] >> 16);
                wp = (wp + 1) & ST_MASK;
            }
        }
        return;
    }
    st_knobs_block();
    for (i = 0; i < 32; i++) {
        int32_t *in = pair + 2 * i, l, r, dl = in[0] >> 16, dr = in[1] >> 16;
        if (now && i == 0)
            st_start(idle ? st_want() : 0);
        if (i == at)
            st_sixteenth(idle);
        if (idle && st_writes()) {
            ST_BUF[2 * wp] = (int16_t)dl;
            ST_BUF[2 * wp + 1] = (int16_t)dr;
        }
        st_voice(&cur, in, &l, &r);
        if (xf) {
            int32_t a, b;
            st_voice(&old, in, &a, &b);
            l = a + ((l - a) * (XF - xf)) / XF;
            r = b + ((r - b) * (XF - xf)) / XF;
            xf--;
        }
        if (idle && st_writes())
            wp = (wp + 1) & ST_MASK;
        if (cur.type != V_LIVE) {
            if (st_w < XF) {
                if (!st_w) {            /* the filter and crush start from here */
                    f_l = l;
                    f_r = r;
                    h_n = 0;
                }
                st_w++;
            }
        } else if (st_w) {
            st_w--;
        }
        if (!st_w) {
            if (cur.type == V_LIVE && !xf)
                continue;               /* the input as it came */
        } else {
            int32_t m = st_w * b_mix;   /* 0..6400 */
            if (b_mask != -1) {         /* CRUSH: fewer bits, held longer */
                if (--h_n <= 0) {
                    h_n = b_hold;
                    h_l = l & b_mask;
                    h_r = r & b_mask;
                }
                l = h_l;
                r = h_r;
            }
            if (b_filt) {               /* FILTER: one pole, low or high */
                f_l += ((l - f_l) * b_fa) >> 15;
                f_r += ((r - f_r) * b_fa) >> 15;
                if (b_filt < 0) {
                    l = f_l;
                    r = f_r;
                } else {
                    l -= f_l;
                    r -= f_r;
                }
            }
            if (m != 6400) {            /* MIX, and the ramp in and out */
                l = dl + ((l - dl) * m) / 6400;
                r = dr + ((r - dr) * m) / 6400;
            }
        }
        if (l > 32767)
            l = 32767;
        if (l < -32768)
            l = -32768;
        if (r > 32767)
            r = 32767;
        if (r < -32768)
            r = -32768;
        in[0] = l << 16;
        in[1] = r << 16;
    }
    st_ph = at < 32 ? st_l16 + at - 32 : st_ph - 32;
}
