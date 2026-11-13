| SPDX-License-Identifier: GPL-2.0-or-later
| STUTTER's trig keys. On OS 1.54 the main view's key handler reaches
| 0x40029a50 for a trig key pressed or released on the normal screen (no grid
| recording, no keyboard, no FUNC or TRK held; repeats go elsewhere), with the
| event in d2 and the key reader in a4. mod.json's site jumps here in place of
| its first three instructions. st_key takes trig keys 9-16 (ids 32-39): the
| handler then leaves as for a key it has dealt with (0x40029b26: d4 = 1).
| Any other key runs the three instructions and goes on. In keyboard mode the
| same path plays notes: the firmware's own test (0x40028e1a, the view in a2,
| nonzero with KB On) leaves the keys to it there.

        .section .run, "ax"
        .globl  st_keyhook
st_keyhook:
        move.l  %a2, -(%sp)
        jsr     0x40028e1a              | keyboard mode?
        addq.l  #4, %sp
        tst.b   %d0
        bne.s   2f
        move.l  %d2, -(%sp)
        jsr     st_key
        addq.l  #4, %sp
        tst.l   %d0
        bne.s   1f
2:
        move.l  %d2, -(%sp)             | the stock's three instructions
        jsr     (%a4)
        move.l  148(%a2), -(%sp)
        jmp     0x40029a58
1:      jmp     0x40029b26
