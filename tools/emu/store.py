# SPDX-License-Identifier: GPL-3.0-or-later
"""game.exe's vendor stock fill (FUN_00576980) as the oracle.

    uv run python store.py <first>-<last>

A job per index: a proxy seed {s, 666} (s = index * 0x9e3779b1, game
+0x1d24 +8, FUN_00536b40), each vendor 0..16 with its candidate lists
(FUN_00536d50: 12-byte {min, max, magicMin, magicMax, code, magicLvl}, the
permanent codes), an act byte (the proxy record +0x22, FUN_00576890's cap
in Normal), a difficulty and clvl (FUN_00625480 stat 12, hooked).
FUN_00535f10 (the NPC's proxy record) is hooked to give the record; the
item maker FUN_00576330 is hooked to record what's asked
(code:quality@ilvl) and answer a fake item; a permanent quiver's fill
(FUN_006295b0 / FUN_00627260) is recorded as `=max`. One line per job:
`s vendor act diff clvl: asks... | seed after`; drlg-dump ... store prints
rules::store_asks' the same.
"""
import struct
import sys

from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_ESP

import drlg

JOBS = [(v, act, diff, clvl) for v in range(17) for act, diff, clvl in ((0, 0, 1), (1, 0, 18), (2, 0, 40), (1, 1, 30), (3, 2, 70), (4, 2, 90))]


def code_of(n):
    return struct.pack("<I", n).decode("latin1").strip()


def lists(e, vendor):
    e.call(0x536d50, vendor)
    cand, count = e.r32(0x883d78 + 16 * vendor), e.r32(0x883d7c + 16 * vendor)
    perm, perms = e.r32(0x883d80 + 16 * vendor), e.r32(0x883d84 + 16 * vendor)
    return cand, count, perm, perms


def dump(e, index):
    s = index * 0x9e3779b1 & 0xffffffff
    g, proxy, rec, npc, player = (e.alloc(n) for n in (0x2000, 0x40, 0x80, 0x200, 0x200))
    e.w32(g + 0x1d24, proxy)
    e.mu.mem_write(g + 0x78, struct.pack("<H", 101)); e.mu.mem_write(npc + 0x78, struct.pack("<H", 101))
    e.w32(g + 0x70, 1); e.w32(npc + 0x70, 1)
    out, asks, clvl = [], [], [1]
    def make(_e):
        sp = e.mu.reg_read(UC_X86_REG_ESP)
        asks.append(f"{code_of(e.mu.reg_read(UC_X86_REG_EDX))}:{e.r32(sp + 12)}@{e.r32(sp + 16)}")
        return 1
    def stack_max(_e): return 7
    def set_stat(_e):
        sp = e.mu.reg_read(UC_X86_REG_ESP); asks.append(f"={e.r32(sp + 12)}"); return 0
    hooks = ((0x535f10, lambda _e: rec, 1), (0x625480, lambda _e: clvl[0], 1), (0x576330, make, 5), (0x6295b0, stack_max, 1), (0x627260, set_stat, 4))
    saved = {a: bytes(e.mu.mem_read(a, 5)) for a, _, _ in hooks}
    for a, fn, n in hooks: e.hook(a, fn, n)
    try:
        for vendor, act, diff, level in JOBS:
            cand, count, perm, perms = lists(e, vendor)
            e.mu.mem_write(rec, bytes(0x80))
            e.mu.mem_write(rec + 0x22, bytes([act]))
            e.w32(rec + 0x2c, cand); e.w32(rec + 0x30, count); e.w32(rec + 0x34, perm); e.w32(rec + 0x38, perms)
            e.mu.mem_write(g + 0x6d, bytes([diff]))
            e.w32(proxy + 8, s); e.w32(proxy + 12, 666)
            clvl[0] = level
            asks.clear()
            e.call(0x576980, npc, rec + 0x20, ecx=g, edx=player)
            out.append(f"{s:08x} {vendor} {act} {diff} {level}: {' '.join(asks)} | {e.r32(proxy + 8):08x}")
    finally:
        for a, code in saved.items():
            e.mu.mem_write(a, code)
            e.mu.ctl_remove_cache(a, a + 5)
    return "\n".join(out)


if __name__ == "__main__":
    first, last = (int(v, 0) for v in sys.argv[1].split("-"))
    e = drlg.boot()
    print("\n".join(dump(e, i) for i in range(first, last + 1)))
