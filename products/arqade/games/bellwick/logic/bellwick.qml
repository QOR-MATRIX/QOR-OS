// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// Bellwick (QOR OS's QOR-0017): a platformer on islands in the sky. Tam runs, jumps and rings across eight levels to bring
// back the bells the Hush has muffled.
//
// The rules are logic/rules.mjs, the one place they live: ARQADE's service runs the same file to replay a run. This logic
// hands them every event as QQ's tick delivers it (a screen button's click, an action's press and release, a step) and
// draws what they say: Tam, what walks and flies, blocks, notes, bells and platforms where the rules put them, and the
// screens for the rules' phase. So a run QQ records is the rules' input exactly, and its outcome is theirs (`outcome`).
// Nothing drawn here decides anything, which is why it may use trigonometry the rules may not.
//
// The level is a world seen from its side, turned to face where the eye looks from and tipped a little toward it, and it
// moves under the eye so Tam stays in view. Its ground is one QQ prop for each level, from make_assets.py, split into sectors
// so only what is near is drawn; the far country is another, slid slowly as the eye moves. What the rules hold is drawn from
// pools made as play starts, each thing keeping its piece while it lasts, and an idle piece is hidden, so it costs nothing.
// The figures' clips run on the game's clock. With less motion asked for (Look), the eye keeps exactly to Tam, nothing turns
// or swells, and Tam does not squash.
//
// What happens somewhere is heard from there, through QQ's spatial audio (`at`). A mid-way bell, the Great Bell, a life, a
// fall, the run's end and the music are heard everywhere.

import QQ
import "rules.mjs" as Rules

Logic {
    id: bellwick

    /// The rules' game: their state, which only they change.
    property var game: null
    /// The rules themselves, for ARQADE's games suite to hand its pilot.
    readonly property var rules: Rules
    /// The level as the rules read it, set as each is drawn: the rules' game says nothing when it changes.
    property var map: null

    /// The rules' units to the level's: a tile of 32 units is one.
    readonly property real k: 1 / Rules.TILE
    readonly property var away: Qt.vector3d(0, -80, 0)
    /// How far from the eye's middle anything is drawn, in tiles.
    readonly property real drawReach: 15
    /// Half the level the eye takes in, in tiles.
    readonly property real halfWide: 8.2
    readonly property real halfHigh: 4.2

    property var pools: ({})
    /// Each pool's pieces by the number of the thing each shows, so a thing keeps its piece while it lasts.
    property var assigned: ({})
    /// Where the eye's middle is, in tiles, along the level and up from its foot; and where it wants to be up.
    property real eyeX: 0
    property real eyeY: 0
    property real wantY: 0
    property real lookAhead: 0
    /// The level drawn, and what the sectors were last shown for.
    property int drawn: -1
    property var sectors: []
    property string sectorsFor: ""
    /// The level's blocks by kind, and what stands in it for the eye.
    property var blocks: []
    /// A block just struck, by its tile's number, and how far through rising and falling it is.
    property var struck: ({})
    /// Bits flying and dust settling: only looked at.
    property var bits: []
    /// Tam's squash, turned toward the way it faces, and flying up from a fall.
    property real squash: 0
    property real turned: 62
    property real fallLift: 0
    property real spin: 0
    /// What the screens last showed, so nothing is asked of them until it changes.
    property string showing: ""
    /// The Great Bell's and each mid-way bell's swing.
    property bool goalRung: false
    property var checksRung: []

    function at(x, y, z) {
        return Qt.vector3d(x * k, map.rows - y * k, z === undefined ? 0 : z)
    }

    /// Where a sound at a place in the level is heard from, in the scene.
    function heardAt(x, y) {
        const level = entity("Level")
        return level ? level.mapPositionToScene(at(x, y, 0)) : null
    }

    /// Something only looked at, made here, hidden until it shows something.
    function piece(kind, fields) {
        fields.position = away
        if (kind === "Shape")
            fields.body = "none"
        const made = spawn(kind, fields, entity("Level"))
        if (made)
            made.visible = false
        return made
    }

    function shapes(name, count, fields) {
        const made = []
        for (let i = 0; i < count; i++) {
            const f = Object.assign({}, fields)
            f.name = name + " " + i
            made.push(piece("Shape", f))
        }
        return made
    }

    function props(name, source, count, size) {
        const made = []
        for (let i = 0; i < count; i++)
            made.push(piece("Prop", { name: name + " " + i, source: "../assets/" + source + ".glb",
                                      scale: Qt.vector3d(size, size, size) }))
        return made
    }

    function text(name, words) {
        const shown = entity(name)
        if (shown)
            shown.text = words
    }

    function button(name) {
        const b = entity(name)
        if (b)
            b.clicked.connect(() => {
                Rules.click(game, name)
                show()
            })
    }

    function counted(n) {
        return n.toLocaleString(Qt.locale("en_GB"), "f", 0)
    }

    /// Words across the middle of the screen for a moment: a level's name, a silver bell, a life.
    function say(words, colour) {
        const message = entity("Message")
        if (!message)
            return
        message.text = words
        message.colour = colour
        if (!Look.reducedMotion) {
            message.size = 44
            tween(message, "size", 34, 0.5, "outCubic")
        }
        after(2.4, () => {
            if (message.text === words)
                message.text = ""
        })
    }

    /// The screens and the words, for the rules' phase; and what the run came to.
    function show() {
        if (game.level !== drawn)
            drawLevel()
        outcome = Rules.outcome(game)
        const phase = game.phase, h = game.hero
        if (phase === "paused" && !paused)
            pause()
        else if (phase !== "paused" && paused)
            resume()
        const muffle = game.enemies.find((e) => e.kind === Rules.MUFFLE && e.act !== "hang")
        const showFoe = muffle && Math.abs(muffle.x - h.x) < 22 * Rules.TILE && Math.abs(muffle.y - h.y) < 10 * Rules.TILE
                        && (phase === "playing" || phase === "paused")
        const hurting = h.hurt > Rules.HURT_HOLD - 10 && phase === "playing"
        const key = [phase, showFoe, hurting].join()
        if (key !== showing) {
            showing = key
            onlyScreen(phase === "title" ? "title" : phase === "over" ? "result" : "hud")
            if (phase === "paused")
                showScreen("pause")
            if (showFoe)
                showScreen("foe")
            if (hurting)
                showScreen("hurt")
        }
        text("Score", counted(game.score))
        text("Notes", "NOTES " + game.notes)
        text("Lives", "LIVES " + game.lives)
        text("Silver", "SILVER " + game.silverGot.reduce((a, b) => a + b, 0) + " OF " + map.silver.length)
        text("Level name", (game.level + 1) + " OF " + Rules.MAPS.length + "  ·  " + map.name.toUpperCase())
        text("Time", "TIME " + Math.floor(game.time / 60))
        if (showFoe) {
            const foe = entity("Foe health")
            if (foe) {
                foe.maximum = muffle.most
                foe.value = muffle.hits
            }
        }
        if (phase === "over") {
            text("Result", game.won ? "THE BELLS RING AGAIN" : "THE HUSH HOLDS")
            text("Final", "SCORE " + counted(game.score))
            text("Details", game.levels + " of " + Rules.MAPS.length + " levels  ·  " + game.notes + " notes  ·  " + game.silver + " silver bells  ·  "
                 + game.kills + " stomped")
        }
    }

    /// The level's own look, its blocks and its bells, when it changes.
    function drawLevel() {
        map = Rules.MAPS[game.level]
        const world = map.world
        drawn = game.level
        const ground = entity("Ground")
        if (ground)
            ground.source = SceneIO.resolvedUrl(scene, "../assets/level-" + game.level + ".glb")
        const far = entity("Far")
        const farSource = SceneIO.resolvedUrl(scene, "../assets/backdrop-" + world + ".glb")
        if (far && String(far.source) !== String(farSource))
            far.source = farSource
        if (scene) {
            scene.sky = "../assets/sky-" + world + ".hdr"
            scene.skyLight = [0.95, 0.82, 0.92, 0.72][world]
            scene.exposure = [1, 1, 1, 1.1][world]
        }
        const sun = entity("Sun")
        if (sun) {
            sun.color = ["#fff2d8", "#ffb07a", "#eaf4ff", "#b4a0ff"][world]
            sun.brightness = [1.15, 1.05, 0.95, 0.62][world]
        }
        const music = entity("Music")
        if (music)
            music.source = "../assets/world-" + world + ".wav"
        sectors = []
        sectorsFor = ""
        const list = []
        map.tiles.forEach((row, r) => {
            for (let c = 0; c < row.length; c++)
                if ("BQRPLH".includes(row.charAt(c)))
                    list.push({ key: r * map.cols + c, c: c, r: r, t: row.charAt(c) })
        })
        blocks = list
        struck = {}
        goalRung = false
        checksRung = map.checks.map(() => false)
        bits = []
        const great = entity("Great bell")
        if (great) {
            if (map.goal) {
                great.position = Qt.vector3d(map.goal.c - 0.05, map.rows - map.goal.floor * k + 3.72, -0.8)
                great.visible = true
                great.clip = "hang"
            } else {
                great.position = away
                great.visible = false
            }
        }
        eyeX = camX()
        wantY = eyeUp()
        eyeY = wantY
        lookAhead = 0
    }

    /// Where the eye wants to stand along the level. A secret room lies past the wall behind the Great Bell, and the eye stays
    /// on its own side of that wall.
    function camX() {
        const x = game.hero.x * k + lookAhead
        let low = halfWide, high = map.cols - halfWide
        if (map.goal) {
            const wall = map.goal.c + Rules.WALL_AFTER_BELL
            if (game.hero.x * k < wall)
                high = Math.min(high, wall + 1 - halfWide)
            else
                low = Math.max(low, wall + 2 + halfWide)
        }
        return map.cols < 2 * halfWide ? map.cols / 2 : Math.max(low, Math.min(Math.max(low, high), x))
    }

    /// Where the eye wants to stand up the level: over the ground Tam stands on, kept inside the level.
    function eyeUp() {
        const y = map.rows - game.hero.y * k + 1.9
        return Math.max(halfHigh, Math.min(Math.max(halfHigh, map.rows - halfHigh + 1), y))
    }

    /// Only the ground the eye can see.
    function showSectors() {
        const size = 14
        if (sectors.length === 0) {
            const ground = entity("Ground")
            if (!ground || ground.status !== 1)
                return
            const found = []
            for (let c = 0; c <= map.cols / size; c++) {
                for (let r = 0; r <= map.rows / size; r++) {
                    const part = ground.part("Sector_" + c + "_" + r)
                    if (part)
                        found.push({ part: part, x: (c + 0.5) * size, y: map.rows - (r + 0.5) * size })
                }
            }
            sectors = found
            sectorsFor = ""
        }
        const key = Math.floor(eyeX / 2) + ":" + Math.floor(eyeY / 2)
        if (key === sectorsFor)
            return
        sectorsFor = key
        for (const s of sectors)
            s.part.visible = Math.abs(s.x - eyeX) < 20 + size / 2 && Math.abs(s.y - eyeY) < 11 + size / 2
    }

    /// Draw `things` from pool `name`, each keeping its piece while it lasts, at `place(thing)`, then `look(piece, thing)`.
    function draw(name, things, place, look) {
        const list = pools[name]
        const was = assigned[name] || {}
        const now = {}
        const taken = list.map(() => false)
        for (const t of things) {
            if (was[t.id] !== undefined) {
                now[t.id] = was[t.id]
                taken[was[t.id]] = true
            }
        }
        let free = 0
        for (const t of things) {
            if (now[t.id] === undefined) {
                while (free < list.length && taken[free])
                    free += 1
                if (free >= list.length)
                    continue
                now[t.id] = free
                taken[free] = true
            }
            const p = list[now[t.id]]
            p.position = place(t)
            p.visible = true
            if (look)
                look(p, t)
        }
        for (const id in was) {
            if (now[id] === undefined) {
                const p = list[was[id]]
                p.position = away
                p.visible = false
                if (p.clip !== undefined)
                    p.clip = ""
            }
        }
        assigned[name] = now
    }

    /// A figure's clip, played at `speed` (looping or not), changed only when it changes.
    function plays(figure, clip, speed, loops) {
        if (figure.clip !== clip || figure.clipLoops !== loops) {
            figure.clipLoops = loops
            figure.clipSpeed = speed
            figure.clip = clip
        } else {
            figure.clipSpeed = speed
        }
    }

    function nearEye(x, y) {
        return Math.abs(x * k - eyeX) < drawReach && Math.abs(map.rows - y * k - eyeY) < 11
    }

    /// Bits thrown from a place: only looked at, up and out, and gone.
    function throwBits(x, y, count, colour, speed, size) {
        if (Look.reducedMotion)
            count = Math.min(count, 2)
        for (let n = 0; n < count; n++) {
            const a = (n + 0.5) / count * Math.PI
            bits.push({ x: x * k, y: map.rows - y * k, vx: Math.cos(a) * speed, vy: Math.sin(a) * speed + 1, left: 0.55, of: 0.55,
                        colour: colour, size: size, id: "b" + game.steps + "-" + n + "-" + bits.length })
        }
    }

    onStarted: {
        game = Rules.create(scene.playSeed)
        const sun = entity("Sun")
        if (sun) {
            sun.shadowMapFar = 30
            sun.shadowMapQuality = 2
            sun.pcfFactor = 1
        }
        const p = {}
        p.mumble = props("Mumble", "mumble", 12, 1)
        p.hushling = props("Hushling", "hushling", 10, 1)
        p.thistle = props("Thistle", "thistle", 10, 1)
        p.flit = props("Flit", "flit", 8, 1)
        p.boing = props("Boing", "boing", 8, 1)
        p.gourd = props("Gourd", "gourd", 6, 1)
        p.muffle = props("Old Muffle", "muffle", 1, 1)
        p.notes = props("Note", "note", 44, 1)
        p.silver = props("Silver bell", "silver", 3, 1)
        p.brick = props("Brick", "brick", 16, 1)
        p.blockQ = props("Note block", "block-note", 10, 1)
        p.blockR = props("Rich block", "block-rich", 5, 1)
        p.blockP = props("Power block", "block-power", 5, 1)
        p.blockL = props("Life block", "block-life", 2, 1)
        p.used = props("Used block", "block-used", 12, 1)
        p.hush = props("Hush block", "block-hush", 26, 1)
        p.spring = props("Spring", "spring", 3, 1)
        p.mid = props("Mid-way bell", "mid-bell", 2, 1)
        p.mover = props("Mover", "mover", 4, 1)
        p.faller = props("Faller", "faller", 6, 1)
        p.brass = props("Brass", "brass", 2, 1)
        p.kite = props("Kite", "kite", 2, 1)
        p.life = props("Life", "life", 2, 1)
        p.wave = props("Wave", "wave", 8, 1)
        p.seeds = shapes("Seed", 8, { form: "sphere", scale: Qt.vector3d(0.2, 0.2, 0.2), colour: "#5c3c1e", metalness: 0,
                                      roughness: 0.4, emissive: "#ff8a4a", emissivePower: 0.3 })
        p.icicles = shapes("Icicle", 8, { form: "cone", scale: Qt.vector3d(0.32, 0.86, 0.32), eulerRotation: Qt.vector3d(180, 0, 0), colour: "#a8d4e8",
                                          metalness: 0.1, roughness: 0.15, emissive: "#7ad0e8", emissivePower: 0.2 })
        p.bits = shapes("Bit", 20, { form: "cube", scale: Qt.vector3d(0.14, 0.14, 0.14), colour: "#f3c84b", metalness: 0, roughness: 0.6 })
        pools = p
        const wave = entity("Ring wave")
        if (wave)
            wave.visible = false
        for (const name of ["Play", "Resume", "Play again"])
            button(name)
        drawLevel()
        show()
    }

    // A and D, the arrows, the pad's d-pad or left stick, or the buttons under a finger move; Space, the pad's A or Jump jumps;
    // Shift, L, the pad's X or Run runs; K, E, the pad's B or Ring rings; S or down, in the air, slams; W or up, at a door, goes
    // through it; P or Start pauses.
    onPressed: (action) => {
        Rules.press(game, action)
        show()
    }
    onReleased: (action) => Rules.release(game, action)

    onStep: {
        const before = game.phase, level = game.level, score = game.score, notes = game.notes, lives = game.lives, form = game.hero.form
        Rules.step(game)
        const h = game.hero
        const heroAt = heardAt(h.x, h.y)
        const heard = new Set()
        for (const s of game.said) {
            const there = heardAt(s.x, s.y)
            if (s.what === "note") {
                // Every note, and each a step up the scale: that is the point of them.
                play("../assets/note" + s.pitch + ".wav", { volume: 0.5, at: there })
                throwBits(s.x, s.y, 3, "#f3c84b", 2.2, 0.08)
                continue
            }
            // One sound of each other kind a step, however many happened, from the first place it happened.
            if (heard.has(s.what))
                continue
            heard.add(s.what)
            if (s.what === "jump") {
                play("../assets/jump.wav", { volume: 0.4, at: heroAt })
                squash = -0.2
                throwBits(s.x, s.y, 3, "#d8cfae", 1.4, 0.09)
            } else if (s.what === "land") {
                play("../assets/land.wav", { volume: 0.35, at: heroAt })
                squash = 0.28
                throwBits(s.x, s.y, 4, "#d8cfae", 1.8, 0.09)
            } else if (s.what === "kill") {
                play(s.kind === Rules.MUFFLE ? "../assets/thump.wav" : "../assets/stomp.wav", { volume: 0.6, at: there })
                throwBits(s.x, s.y, 7, "#fff6dc", 3.2, 0.1)
                if (s.kind === Rules.MUFFLE)
                    say(counted(s.worth + game.bonus) + "  ·  OLD MUFFLE IS ENDED", "#ffe9a8")
            } else if (s.what === "bump") {
                play("../assets/bump.wav", { volume: 0.5, at: there })
                struck[s.r * map.cols + s.c] = 0.18
            } else if (s.what === "knock") {
                play("../assets/bump.wav", { volume: 0.3, at: there })
            } else if (s.what === "item") {
                play("../assets/item.wav", { volume: 0.55, at: there })
                struck[s.r * map.cols + s.c] = 0.18
            } else if (s.what === "break") {
                play("../assets/break.wav", { volume: 0.55, at: there })
                throwBits(s.x, s.y, 9, "#c9543a", 4, 0.17)
            } else if (s.what === "power") {
                play("../assets/power.wav", { volume: 0.65, at: heroAt })
                squash = 0.28
                say(s.what === "power" && h.form === Rules.KITE ? "KITE  ·  HOLD JUMP TO GLIDE" : "BRASS  ·  BREAK BRICK WITH YOUR HEAD", "#ffe9a8")
            } else if (s.what === "shrink") {
                play("../assets/shrink.wav", { volume: 0.6, at: heroAt })
            } else if (s.what === "ring") {
                play("../assets/ring.wav", { volume: 0.65, at: heroAt })
                const figure = entity("Tam " + h.form)
                if (figure)
                    figure.clipTime = 0
                const wave = entity("Ring wave")
                if (wave) {
                    wave.position = at(s.x, s.y, 0.2)
                    wave.emissivePower = 0.9
                    wave.scale = Qt.vector3d(0.4, 0.02, 0.4)
                    wave.visible = true
                }
            } else if (s.what === "hover") {
                play("../assets/hover.wav", { volume: 0.4, at: heroAt })
            } else if (s.what === "slam") {
                play("../assets/slam.wav", { volume: 0.8, at: heroAt })
                squash = 0.36
                throwBits(s.x, s.y, 8, "#d8cfae", 3.4, 0.12)
            } else if (s.what === "spring") {
                play("../assets/spring.wav", { volume: 0.6, at: heroAt })
                squash = -0.3
            } else if (s.what === "silver") {
                play("../assets/silver.wav", { volume: 0.75, at: there })
                say("SILVER BELL", "#e8f4fa")
                throwBits(s.x, s.y, 8, "#e8f4fa", 3.4, 0.1)
            } else if (s.what === "life") {
                play("../assets/life.wav", { volume: 0.7 })
                say("ONE MORE LIFE", "#ffe9a8")
            } else if (s.what === "check") {
                play("../assets/check.wav", { volume: 0.7 })
                checksRung[s.which] = true
                say("MID-WAY BELL", "#ffe9a8")
            } else if (s.what === "goal") {
                play("../assets/goal.wav", { volume: 0.9 })
                goalRung = true
                const great = entity("Great bell")
                if (great) {
                    great.clipLoops = false
                    great.clip = "swing"
                }
                say(counted(s.bonus + game.bonus) + "  ·  THE GREAT BELL RINGS", "#ffe9a8")
            } else if (s.what === "door") {
                play("../assets/door.wav", { volume: 0.55 })
                snapEye()
            } else if (s.what === "die") {
                play("../assets/die.wav", { volume: 0.75 })
                fallLift = 0
            } else if (s.what === "late") {
                play("../assets/late.wav", { volume: 0.7 })
                say("THE HUSH CAUGHT UP", "#ff9a8a")
            } else if (s.what === "wake") {
                snapEye()
            } else if (s.what === "level") {
                play("../assets/level.wav", { volume: 0.6 })
            } else if (s.what === "spit" || s.what === "boing" || s.what === "creak" || s.what === "shatter"
                       || s.what === "thump" || s.what === "leap" || s.what === "struck" || s.what === "muffled") {
                play("../assets/" + s.what + ".wav", { volume: 0.5, at: there })
                if (s.what === "thump" || s.what === "struck")
                    throwBits(s.x, s.y, 8, "#b4a0ff", 3.4, 0.14)
            }
        }
        if (before !== "over" && game.phase === "over")
            play(game.won ? "../assets/won.wav" : "../assets/lost.wav", { volume: 0.9 })
        if (game.level !== drawn) {
            drawLevel()
            say(map.name.toUpperCase(), "#fff6dc")
        }
        outcome = Rules.outcome(game)
        if (before !== game.phase || score !== game.score || notes !== game.notes || lives !== game.lives || form !== h.form
                || game.steps % 15 === 0 || h.hurt === Rules.HURT_HOLD - 10)
            show()
    }

    function snapEye() {
        lookAhead = 0
        eyeX = camX()
        wantY = eyeUp()
        eyeY = wantY
        sectorsFor = ""
    }

    // Only looked at: everything where the rules put it.
    onFrame: (dt) => {
        const h = game.hero
        if (!Look.reducedMotion)
            spin = (spin + 140 * dt) % 360
        const turning = Look.reducedMotion ? 0 : spin
        // The eye leads the way Tam faces, and comes up or down only for the ground Tam stands on, or when Tam nears its edge.
        const lead = h.facing * 2 * Math.min(1, Math.abs(h.vx) / Rules.WALK)
        lookAhead += (lead - lookAhead) * (Look.reducedMotion ? 1 : Math.min(1, dt * 1.6))
        const follow = Look.reducedMotion ? 1 : Math.min(1, dt * 7)
        eyeX += (camX() - eyeX) * follow
        const heroUp = map.rows - h.y * k
        if (h.ground || heroUp < eyeY - 3.2 || heroUp > eyeY + 2.6)
            wantY = eyeUp()
        eyeY += (wantY - eyeY) * (Look.reducedMotion ? 1 : Math.min(1, dt * 4))
        showSectors()
        const level = entity("Level")
        if (level)
            level.position = Qt.vector3d(-eyeX, -eyeY, 0)
        const far = entity("Far")
        if (far)
            far.position = Qt.vector3d(-(eyeX - 60) * 0.1, -eyeY * 0.14 - 1, 0)

        // Tam, in the form it has.
        for (let n = 0; n < 3; n++) {
            const figure = entity("Tam " + n)
            if (figure && n !== h.form) {
                figure.position = away
                figure.visible = false
                figure.clip = ""
            }
        }
        const figure = entity("Tam " + h.form)
        if (figure) {
            const way = h.facing > 0 ? 62 : -62
            turned += (way - turned) * (Look.reducedMotion ? 1 : Math.min(1, dt * 16))
            let lift = 0
            if (h.dying > 0) {
                const t = (Rules.DYING_STEPS - h.dying) / 60
                lift = 5 * t - 9 * t * t
                plays(figure, "die", 1, true)
            } else if (game.clearing > 0) {
                plays(figure, "win", 1, true)
            } else if (h.hurt > Rules.HURT_HOLD - 18) {
                plays(figure, "hurt", 1, false)
            } else if (h.slam > 0) {
                plays(figure, "hover", 1, false)
            } else if (h.slam < 0) {
                plays(figure, "slam", 1, true)
            } else if (h.ringing > 0) {
                plays(figure, "ring", 1, false)
            } else if (h.gliding) {
                plays(figure, "glide", 1, true)
            } else if (!h.ground) {
                plays(figure, h.vy < 0 ? "jump" : "fall", 1, true)
            } else if (h.skid) {
                plays(figure, "skid", 1, true)
            } else if (Math.abs(h.vx) > 0.25) {
                plays(figure, "run", 0.6 + Math.abs(h.vx) * 0.32, true)
            } else {
                plays(figure, "idle", 1, true)
            }
            figure.position = Qt.vector3d(h.x * k, map.rows - h.y * k + lift, 0.05)
            figure.eulerRotation = Qt.vector3d(0, h.dying > 0 ? 0 : game.clearing > 0 ? 8 : turned, 0)
            // Hurt, it flickers: that says nothing can hurt it for now.
            figure.visible = !(h.hurt > 0 && h.dying === 0) || (Look.reducedMotion ? true : Math.floor(h.hurt / 5) % 2 === 0)
            squash += (0 - squash) * Math.min(1, dt * 12)
            const s = Look.reducedMotion ? 0 : squash
            figure.scale = Qt.vector3d(1 + s * 0.6, 1 - s, 1 + s * 0.6)
        }

        const nearby = game.enemies.filter((e) => nearEye(e.x, e.y))
        const of = (n) => nearby.filter((e) => e.kind === n)
        const faces = (e) => e.face > 0 ? 62 : -62
        const walking = (figure, e) => {
            figure.eulerRotation = Qt.vector3d(0, faces(e), 0)
            plays(figure, e.stun > 0 ? "stun" : "walk", e.stun > 0 ? 1 : 1, true)
        }
        const place = (e) => at(e.x, e.y, 0)
        draw("mumble", of(Rules.MUMBLE), place, walking)
        draw("hushling", of(Rules.HUSHLING), place, walking)
        draw("thistle", of(Rules.THISTLE), place, walking)
        draw("flit", of(Rules.FLIT), (e) => at(e.x, e.y - 9, 0), walking)
        draw("boing", of(Rules.BOING), place, (figure, e) => {
            figure.eulerRotation = Qt.vector3d(0, faces(e), 0)
            plays(figure, e.stun > 0 ? "stun" : e.ground ? "idle" : "walk", 1, true)
        })
        draw("gourd", of(Rules.GOURD), place, (figure, e) => {
            figure.eulerRotation = Qt.vector3d(0, faces(e) * 0.5, 0)
            if (e.stun > 0)
                plays(figure, "stun", 1, true)
            else if (e.timer < 20)
                plays(figure, "spit", 1, false)
            else
                plays(figure, "idle", 1, true)
        })
        draw("muffle", of(Rules.MUFFLE), place, (figure, e) => {
            figure.eulerRotation = Qt.vector3d(0, e.face > 0 ? 28 : -28, 0)
            if (e.act === "reeling")
                plays(figure, "reel", 1, true)
            else if (e.stun > 0 || e.act === "open")
                plays(figure, "open", 1, false)
            else if (e.act === "air")
                plays(figure, "leap", 1, false)
            else
                plays(figure, "walk", 1 + (e.most - e.hits) * 0.22, true)
        })
        draw("icicles", of(Rules.ICICLE), (e) => {
            const shake = e.act === "shake" && !Look.reducedMotion ? Math.sin(game.steps * 2.4) * 1.5 : 0
            return at(e.x + shake, e.y - Rules.KINDS[Rules.ICICLE].h / 2, 0)
        })
        draw("seeds", game.shots.filter((s) => s.what === "seed" && nearEye(s.x, s.y)), (s) => at(s.x, s.y, 0))
        draw("wave", game.shots.filter((s) => s.what === "wave" && nearEye(s.x, s.y)), (s) => at(s.x, s.y + 10, 0))

        // Blocks, each as it stands now; a block struck rises and falls.
        const near = blocks.filter((b) => Math.abs(b.c - eyeX) < drawReach && Math.abs(map.rows - b.r - eyeY) < 11)
        const named = (b) => {
            const changed = game.changed[b.key]
            return changed !== undefined ? changed : b.t
        }
        for (const key in struck) {
            struck[key] -= dt
            if (struck[key] <= 0)
                delete struck[key]
        }
        const block = (b) => {
            const rise = struck[b.key] !== undefined ? Math.sin((1 - struck[b.key] / 0.18) * Math.PI) * 0.28 : 0
            return Qt.vector3d(b.c + 0.5, map.rows - b.r - 0.5 + rise, 0)
        }
        const withId = (b) => { b.id = b.key; return b }
        const kinded = (t) => near.filter((b) => named(b) === t).map(withId)
        draw("brick", kinded("B"), block)
        draw("blockQ", kinded("Q"), block)
        draw("blockR", kinded("R"), block)
        draw("blockP", kinded("P"), block)
        draw("blockL", kinded("L"), block)
        draw("used", kinded("U"), block)
        draw("hush", kinded("H"), block, (piece, b) => {
            const left = game.awake[b.key]
            // Asleep it is a ghost; awake, there; and about to go, it flickers.
            piece.opacity = left === undefined ? 0.22 : (left < 60 && !Look.reducedMotion && Math.floor(left / 6) % 2 === 0 ? 0.5 : 1)
        })

        // Notes and silver bells, turning where they wait.
        draw("notes", map.notes.map((n, i) => ({ id: i, c: n[0], r: n[1] })).filter((n) => game.got[n.id] === 0 && Math.abs(n.c - eyeX) < drawReach),
             (n) => Qt.vector3d(n.c + 0.5, map.rows - n.r - 0.5 + (Look.reducedMotion ? 0 : Math.sin((spin + n.c * 40) * Math.PI / 180) * 0.05), 0),
             (piece) => piece.eulerRotation = Qt.vector3d(0, turning, 0))
        draw("silver", map.silver.map((n, i) => ({ id: i, c: n[0], r: n[1] })).filter((n) => game.silverGot[n.id] === 0 && Math.abs(n.c - eyeX) < drawReach),
             (n) => Qt.vector3d(n.c + 0.5, map.rows - n.r - 0.5, 0), (piece) => piece.eulerRotation = Qt.vector3d(0, turning, 0))
        draw("spring", map.springs.map((n, i) => ({ id: i, c: n[0], r: n[1] })).filter((n) => Math.abs(n.c - eyeX) < drawReach),
             (n) => Qt.vector3d(n.c + 0.5, map.rows - n.r - 1, 0))
        draw("mid", map.checks.map((n, i) => ({ id: i, c: n[0], r: n[1] })).filter((n) => Math.abs(n.c - eyeX) < drawReach),
             (n) => Qt.vector3d(n.c + 0.5, map.rows - n.r - 1, -0.3),
             (piece, n) => {
                 if (checksRung[n.id] || game.check >= n.id) {
                     piece.clipLoops = true
                     if (piece.clip !== "swing")
                         piece.clip = "swing"
                 } else if (piece.clip !== "hang") {
                     piece.clip = "hang"
                 }
             })
        draw("mover", game.platforms.filter((p) => p.what === "mover" && nearEye(p.x, p.y)), (p) => at(p.x, p.y, 0))
        draw("faller", game.platforms.filter((p) => p.what === "faller" && p.act !== "gone" && nearEye(p.x, p.y)),
             (p) => at(p.x + (p.act === "shaking" && !Look.reducedMotion ? Math.sin(game.steps * 2.2) * 1.5 : 0), p.y, 0))
        draw("brass", game.items.filter((i) => i.what === "brass"), (i) => at(i.x, i.y, 0), (piece) => piece.eulerRotation = Qt.vector3d(0, turning, 0))
        draw("kite", game.items.filter((i) => i.what === "kite"), (i) => at(i.x, i.y - 14, 0), (piece) => piece.eulerRotation = Qt.vector3d(0, 18, Look.reducedMotion ? 0 : Math.sin(spin * Math.PI / 90) * 12))
        draw("life", game.items.filter((i) => i.what === "life"), (i) => at(i.x, i.y, 0), (piece) => piece.eulerRotation = Qt.vector3d(0, turning, 0))

        // The hand bell's ring going out, as far as it reaches.
        const wave = entity("Ring wave")
        if (wave && wave.visible) {
            const across = wave.scale.x + dt * 22
            const most = 2 * Rules.RING_REACH * k
            if (across >= most) {
                wave.visible = false
                wave.position = away
            } else {
                wave.position = at(h.x, h.y - Rules.HERO_H / 2, 0.2)
                wave.scale = Qt.vector3d(across, 0.02, across)
                wave.emissivePower = 0.9 * (1 - across / most)
                wave.opacity = 0.6 * (1 - across / most)
            }
        }

        // Bits fly, and fall away.
        for (const b of bits) {
            b.left -= dt
            b.vy -= 12 * dt
            b.x += b.vx * dt
            b.y += b.vy * dt
        }
        bits = bits.filter((b) => b.left > 0).slice(-20)
        draw("bits", bits, (b) => Qt.vector3d(b.x, b.y, 0.5), (piece, b) => {
            const s = b.size * Math.max(0.2, b.left / b.of)
            piece.scale = Qt.vector3d(s, s, s)
            piece.colour = b.colour
            piece.emissive = b.colour
            piece.emissivePower = 0.35
        })
    }
}
