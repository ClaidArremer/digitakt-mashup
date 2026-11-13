| SPDX-License-Identifier: GPL-2.0-or-later
| STUTTER's lights and its mode key (OS 1.54).
|
| 0x400083a0 is the LED manager's set (mgr, led, mode, colour): every key LED
| the firmware lights goes through it. The site jumps here in place of its
| first two instructions; st_led may change the call's mode and colour (to
| STUTTER's while its mode is on), and the set goes on with them.
|
| 0x40029876 is the main view's key handler (view, event). st_view_key sees
| every key first: nonzero means it took the key (FUNC + SAMPLING), and the
| handler returns at once, as for a key it dealt with.

        .section .run, "ax"
        .globl  st_ledhook
st_ledhook:
        pea     4(%sp)                  | the call's (mgr, led, mode, colour)
        jsr     st_led
        addq.l  #4, %sp
        lea     -16(%sp), %sp           | the stock's two instructions
        movem.l %d2-%d4/%a2, (%sp)
        jmp     0x400083a8

        .globl  st_viewhook
st_viewhook:
        move.l  8(%sp), -(%sp)          | the event
        move.l  8(%sp), -(%sp)          | the view
        jsr     st_view_key
        addq.l  #8, %sp
        tst.l   %d0
        beq.s   1f
        moveq   #1, %d0
        rts
1:      lea     -140(%sp), %sp          | the stock's two instructions
        movem.l %d2-%d7/%a2-%a6, (%sp)
        jmp     0x4002987e

| 0x40033b6c is the normal screen's track-activity lights: each UI frame it
| lights the trig keys of the tracks that are sounding, straight to the LEDs.
| While STUTTER's colours show (st_lit), it returns at once.
        .globl  st_acthook
st_acthook:
        jsr     st_lit
        tst.l   %d0
        beq.s   1f
        rts
1:      lea     -24(%sp), %sp           | the stock's two instructions
        movem.l %d2-%d4/%a2-%a4, (%sp)
        jmp     0x40033b74
