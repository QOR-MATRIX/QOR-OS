// SPDX-License-Identifier: Apache-2.0
// Bellwick's pilot: what a person would do next, read from the rules' game as it stands. Node's test (bellwick.test.mjs) and
// ARQADE's games suite (qt/tests/tst_games.cpp) both evaluate this and call the function it gives with the rules, the game and
// whether to play calm. The suite, in the game's own engine, plays what it says by the keys, the pad and the screen's buttons.
//
// It runs right, and jumps for what it sees coming: a pit, a wall, thorns, or something walking. It looks far enough ahead to
// act in time when the game draws few frames, because then it looks only every few steps. It is no searcher, and plays the first
// levels well enough to reach their bells, not the whole game: bellwick-solver.mjs proves that. Calm, it runs right, jumping only
// when a wall has stopped it, into whatever is there, until the lives are spent.
(function () {
    function ahead(R, g, reach) {
        const h = g.hero, T = R.TILE, r = Math.floor((h.y - 1) / T)
        const c = Math.floor(h.x / T)
        // A wall or thorns can be jumped from a way off; a gap only from near its edge, or the jump falls short of the far side.
        const edge = (c + 1) * T - h.x
        for (let n = 1; n <= reach; n++) {
            const col = c + n
            if (R.solidAt(g, col, r) || R.solidAt(g, col, r - 1) || R.tileAt(g, col, r) === "T")
                return n
            let ground = false
            for (let d = 1; d <= 4 && !ground; d++)
                ground = R.solidAt(g, col, r + d) || R.tileAt(g, col, r + d) === "="
            if (!ground && (n > 1 || edge < 34))
                return n === 1 ? 1 : 0
        }
        return 0
    }

    function threat(R, g) {
        const h = g.hero, T = R.TILE
        for (const e of g.enemies) {
            if (e.stun > 0 || e.kind === R.ICICLE)
                continue
            const dx = e.x - h.x, dy = e.y - h.y
            if (dx > 0 && dx < 3.4 * T && Math.abs(dy) < 2.2 * T)
                return true
        }
        for (const s of g.shots) {
            const dx = s.x - h.x
            if (dx > -T && dx < 3 * T && Math.abs(s.y - h.y) < 1.5 * T)
                return true
        }
        return false
    }

    return function (R, g, calm) {
        const h = g.hero
        const choice = { right: true, left: false, run: true, jump: false, ring: false }
        if (calm) {
            // A wall would hold it there until the clock ran out, every life.
            choice.jump = h.ground ? h.vx === 0 && R.solidAt(g, Math.floor((h.x + h.held.right * R.HERO_W + 1) / R.TILE), Math.floor((h.y - 1) / R.TILE)) : h.vy < 0 && h.held.jump
            return choice
        }
        // A jump holds until it starts to come down; a hop over a small thing would do, but a person presses and holds.
        if (h.ground) {
            const near = ahead(R, g, 2)
            if (near > 0 || threat(R, g))
                choice.jump = true
        } else if (h.vy < 0) {
            choice.jump = h.held.jump
        }
        return choice
    }
})()
