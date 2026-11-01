// SPDX-License-Identifier: Apache-2.0
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import * as R from '../games/bellwick/logic/rules.mjs';

// Bellwick's rules (QOR OS's QOR-0017): the same module the QQ game runs, here as ARQADE's service will run it to replay a run.
// A run QQ itself recorded is replayed below to the outcome QQ reached, and a run the solver found goes through the whole game.
const {TILE, MAPS, KINDS, MUMBLE, HUSHLING, THISTLE, FLIT, BOING, GOURD, ICICLE, MUFFLE, SMALL, BRASS, KITE} = R;
const newPilot = () => (0, eval)(readFileSync(new URL('./bellwick-pilot.js', import.meta.url), 'utf8'));

const started = (seed = 1, level = 0) => {
  const g = R.create(seed);
  R.click(g, 'Play');
  if (level !== 0) R.enter(g, level);
  return g;
};
const steps = (g, n) => { for (let i = 0; i < n; i++) R.step(g); return g; };
const hold = (g, ...names) => { for (const name of names) R.press(g, name); };
const letGo = (g, ...names) => { for (const name of names) R.release(g, name); };
const tap = (g, name) => { R.press(g, name); R.release(g, name); };
const key = (g, c, r) => r * MAPS[g.level].cols + c;
const said = (g, what, run) => {
  const heard = [];
  const before = g.said;
  run(() => { heard.push(...g.said.filter(s => s.what === what)); });
  g.said = before;
  return heard;
};
// Steps `n`, keeping everything said on the way.
const listen = (g, n) => {
  const out = [];
  for (let i = 0; i < n; i++) { R.step(g); out.push(...g.said); }
  return out;
};
// Windmill Green has a clear floor from column 13 to 29. Tam stands on it, at `c`, with nothing else about.
const FLOOR_ROW = MAPS[0].start[1] + 1;
const clear = g => { g.enemies = []; g.platforms = []; g.shots = []; g.items = []; };
const standing = (seed = 1, c = 16) => {
  const g = started(seed);
  clear(g);
  put(g, c, FLOOR_ROW);
  return g;
};
const put = (g, c, floorRow) => {
  const h = g.hero;
  h.x = (c + 0.5) * TILE; h.y = floorRow * TILE; h.before = h.y; h.vx = 0; h.vy = 0; h.ground = true; h.coyote = 0; h.buffer = 0;
  h.on = -1; h.slam = 0;
};
const like = (g, kind, x, y, more = {}) => {
  const e = {id: 900 + g.enemies.length, kind, x, y, vx: 0, vy: 0, face: -1, act: 'go', timer: 9999, stun: 0, ground: true, homeX: x, homeY: y, phase: 0, ...more};
  g.enemies.push(e);
  return e;
};
const heightOf = g => g.hero.y;
const peak = (g, n) => {
  let top = g.hero.y;
  for (let i = 0; i < n; i++) { R.step(g); top = Math.min(top, g.hero.y); }
  return top;
};

test('a seed plays the same run every time, and another seed moves things otherwise', () => {
  const run = seed => {
    const g = started(seed, 1);
    hold(g, 'right');
    return JSON.stringify(steps(g, 600));
  };
  assert.equal(run(4), run(4));
  assert.notEqual(run(4), run(5));
  // What a level is laid out as is the same for every seed.
  assert.deepEqual(started(4, 2).enemies.map(e => [e.kind, e.x, e.y]), started(5, 2).enemies.map(e => [e.kind, e.x, e.y]));
});

test('every level holds together: one start, an ending, three silver bells, doors in pairs, and ways that go somewhere', () => {
  assert.equal(MAPS.length, 8);
  assert.deepEqual(MAPS.map(m => m.world), [0, 0, 1, 1, 2, 2, 3, 3]);
  MAPS.forEach((map, n) => {
    assert.ok(map.start, map.name);
    assert.equal(map.silver.length, 3, map.name);
    assert.ok(map.goal || map.spawns.some(s => s.kind === MUFFLE), map.name);
    assert.ok(map.seconds >= 200, map.name);
    assert.ok(map.notes.length >= 6, map.name);
    for (const door of map.doors) assert.ok(door.a && door.b, map.name);
    for (const p of map.platforms) if (p.what === 'mover') assert.ok(p.to[0] !== 0 || p.to[1] > -1 || p.to[1] < 0, map.name);
    assert.equal(R.LEVELS[n].rows.length, map.rows);
    for (const row of map.tiles) assert.equal(row.length, map.cols);
  });
  // Old Muffle waits at the top of the Belfry and in the Last Belfry, and nowhere else.
  assert.deepEqual(MAPS.map(m => m.spawns.filter(s => s.kind === MUFFLE).length), [0, 0, 0, 1, 0, 0, 0, 1]);
});

test('nothing moves on the title or while paused, and what was pressed on a screen does nothing once play starts', () => {
  const g = R.create(1);
  const at = JSON.stringify(g.hero);
  hold(g, 'jump');
  steps(g, 60);
  assert.equal(g.phase, 'title');
  assert.equal(g.steps, 0);
  R.click(g, 'Play');
  assert.equal(g.hero.buffer, 0);
  letGo(g, 'jump');
  tap(g, 'pause');
  assert.equal(g.phase, 'paused');
  const y = g.hero.y, time = g.time;
  steps(g, 60);
  assert.equal(g.hero.y, y);
  assert.equal(g.time, time);
  R.click(g, 'Resume');
  assert.equal(g.phase, 'playing');
  tap(g, 'pause');
  tap(g, 'pause');
  assert.equal(g.phase, 'playing');
  assert.ok(at);
});

test('Tam walks to a walk, runs to a run, stops short on the ground, and is stopped by walls', () => {
  const g = standing();
  hold(g, 'right');
  steps(g, 60);
  assert.equal(g.hero.vx, R.WALK);
  hold(g, 'run');
  steps(g, 40);
  assert.equal(g.hero.vx, R.RUN);
  assert.equal(g.hero.facing, 1);
  letGo(g, 'run');
  steps(g, 30);
  assert.equal(g.hero.vx, R.WALK);
  letGo(g, 'right');
  steps(g, 20);
  assert.equal(g.hero.vx, 0);
  // Turning skids.
  put(g, 16, FLOOR_ROW);
  hold(g, 'right', 'run');
  steps(g, 30);
  letGo(g, 'right');
  hold(g, 'left');
  R.step(g);
  assert.equal(g.hero.skid, true);
  assert.equal(g.hero.facing, -1);
  letGo(g, 'left', 'run');
  // A stump stands at columns 11 and 12: walked into, Tam stops at its side.
  put(g, 9, FLOOR_ROW);
  hold(g, 'right');
  steps(g, 60);
  assert.equal(g.hero.x, 11 * TILE - R.HERO_W);
  assert.equal(g.hero.vx, 0);
  letGo(g, 'right');
  // And the level's own side is a wall.
  put(g, 1, FLOOR_ROW);
  hold(g, 'left');
  steps(g, 60);
  assert.equal(g.hero.x, R.HERO_W);
});

test('on ice it takes room to get going, and more to stop', () => {
  const slide = level => {
    const g = started(1, level);
    clear(g);
    const floor = MAPS[level].start[1] + 1;
    put(g, 3, floor);
    hold(g, 'right');
    steps(g, 30);
    const fast = g.hero.vx;
    steps(g, 30);
    letGo(g, 'right');
    const from = g.hero.x;
    steps(g, 90);
    return {fast, stop: g.hero.x - from};
  };
  const grass = slide(0), ice = slide(4);
  assert.ok(ice.fast < grass.fast, 'slower off the mark on ice');
  assert.ok(ice.stop > grass.stop * 3, `a longer stop on ice: ${ice.stop} and ${grass.stop}`);
});

test('a jump held goes higher than one let go, and a run jumps higher than a stand', () => {
  const from = (held, run) => {
    const g = standing();
    if (run) {
      put(g, 13, FLOOR_ROW);
      hold(g, 'right', 'run');
      steps(g, 28);
    }
    const y = g.hero.y;
    R.press(g, 'jump');
    if (!held) { R.step(g); R.release(g, 'jump'); }
    return y - peak(g, 60);
  };
  const tapped = from(false, false), held = from(true, false), running = from(true, true);
  assert.ok(tapped < TILE * 2.5, `a tap is a hop: ${tapped}`);
  assert.ok(held > TILE * 3.2 && held < TILE * 4, `a full jump: ${held}`);
  assert.ok(running > held + TILE * 0.5, `a running jump: ${running}`);
});

test('a jump is still allowed a moment after leaving an edge, and kept a moment before landing', () => {
  // The floor of Windmill Green ends at a gap: walk off it, then jump late.
  const off = late => {
    const g = started();
    clear(g);
    const map = MAPS[0];
    let edge = 40;
    while (map.tiles[FLOOR_ROW].charAt(edge) === '#') edge += 1;
    put(g, edge - 1, FLOOR_ROW);
    g.hero.x = edge * TILE + R.HERO_W + 2;
    hold(g, 'right');
    R.step(g);
    assert.equal(g.hero.ground, false);
    assert.equal(g.hero.coyote, R.COYOTE - 1);
    steps(g, late);
    const vy = g.hero.vy;
    tap(g, 'jump');
    R.step(g);
    return {vy, jumped: g.hero.vy < vy};
  };
  assert.equal(off(2).jumped, true);
  assert.equal(off(R.COYOTE + 2).jumped, false);
  // Pressed just before the ground, it jumps on landing; pressed too soon, it is forgotten.
  const before = soon => {
    const g = standing();
    g.hero.y -= TILE * 3;
    g.hero.ground = false;
    while (g.hero.y + g.hero.vy + R.FALL_PULL < FLOOR_ROW * TILE - soon * R.FALL_MOST) R.step(g);
    tap(g, 'jump');
    const events = listen(g, soon + R.BUFFER + 3);
    return events.some(s => s.what === 'jump');
  };
  assert.equal(before(1), true);
  const late = standing();
  late.hero.y -= TILE * 7;
  late.hero.ground = false;
  tap(late, 'jump');
  steps(late, R.BUFFER + 2);
  assert.equal(late.hero.buffer, 0);
  assert.equal(listen(late, 60).some(s => s.what === 'jump'), false);
});

test('a head that catches a corner slips past it; a head that hits a block squarely stops', () => {
  const g = standing();
  const r = FLOOR_ROW - 3;
  g.changed[key(g, 20, r)] = 'U';
  put(g, 20, FLOOR_ROW);
  g.hero.x = 20 * TILE - R.HERO_W + R.CORNER - 2;
  hold(g, 'jump');
  const top = peak(g, 40);
  assert.ok(top < (r + 1) * TILE, 'past the corner, the jump goes on above the block');
  letGo(g, 'jump');
  const square = standing();
  square.changed[key(square, 20, r)] = 'U';
  put(square, 20, FLOOR_ROW);
  hold(square, 'jump');
  const stopped = peak(square, 40);
  assert.equal(stopped, (r + 1) * TILE + R.HERO_H);
});

test('a plank is stood on from above and jumped through from below', () => {
  const g = standing();
  const r = FLOOR_ROW - 2;
  g.changed[key(g, 20, r)] = '=';
  put(g, 20, FLOOR_ROW);
  hold(g, 'jump');
  const top = peak(g, 26);
  assert.ok(top < r * TILE, 'through it from under');
  letGo(g, 'jump');
  steps(g, 60);
  assert.equal(g.hero.y, r * TILE);
  assert.equal(g.hero.ground, true);
});

test('a stomp ends what walks and bounces higher with the jump held; each in one bounce is worth more; from the side it hurts', () => {
  const stomp = (held, kind = MUMBLE) => {
    const g = standing();
    const x = g.hero.x, e = like(g, kind, x, FLOOR_ROW * TILE);
    g.hero.y = e.y - KINDS[kind].h - 14;
    g.hero.before = g.hero.y;
    g.hero.ground = false;
    g.hero.vy = 2;
    if (held) hold(g, 'jump');
    g.hero.buffer = 0;
    const events = listen(g, 8);
    return {g, events};
  };
  const {g, events} = stomp(false);
  assert.equal(g.enemies.length, 0);
  assert.equal(g.kills, 1);
  assert.equal(g.score, KINDS[MUMBLE].worth);
  assert.ok(events.some(s => s.what === 'kill'));
  const low = stomp(false).g.hero.y;
  const high = stomp(true).g.hero.y;
  assert.ok(high < low);

  // Two in one bounce: the second is worth twice as much.
  const two = stomp(false).g;
  const worth = two.score;
  two.hero.y = FLOOR_ROW * TILE - KINDS[MUMBLE].h - 14;
  two.hero.before = two.hero.y;
  two.hero.vy = 2;
  like(two, MUMBLE, two.hero.x, FLOOR_ROW * TILE);
  assert.equal(two.hero.chain, 1);
  steps(two, 8);
  assert.equal(two.score, worth * 3);

  // From the side, small: a fall.
  const side = standing();
  like(side, MUMBLE, side.hero.x + 16, FLOOR_ROW * TILE, {face: -1});
  const said_ = listen(side, 4);
  assert.ok(said_.some(s => s.what === 'die'));
  assert.equal(side.falls, 1);
  assert.ok(side.hero.dying > 0);
});

test('a thistle hurts a stomp until it is rung; a slam ends it; what is rung stands stunned and harmless', () => {
  const g = standing();
  g.hero.form = BRASS;
  const e = like(g, THISTLE, g.hero.x, FLOOR_ROW * TILE);
  g.hero.y = e.y - KINDS[THISTLE].h - 14;
  g.hero.before = g.hero.y;
  g.hero.ground = false;
  g.hero.vy = 2;
  steps(g, 6);
  assert.equal(g.enemies.length, 1, 'spines');
  assert.equal(g.hero.form, SMALL);
  assert.ok(g.hero.hurt > 0);

  const rung = standing();
  const t = like(rung, THISTLE, rung.hero.x + 60, FLOOR_ROW * TILE);
  tap(rung, 'ring');
  R.step(rung);
  assert.equal(t.stun, R.STUN_STEPS - 1);
  assert.equal(rung.hero.ringCool, R.RING_COOL);
  assert.equal(rung.hero.ringing, R.RING_STEPS);
  // Walked through, it does nothing; stomped, it ends.
  t.x = rung.hero.x;
  steps(rung, 3);
  assert.equal(rung.hero.dying, 0);
  rung.hero.y = t.y - KINDS[THISTLE].h - 14;
  rung.hero.before = rung.hero.y;
  rung.hero.ground = false;
  rung.hero.vy = 2;
  steps(rung, 6);
  assert.equal(rung.enemies.length, 0);

  const slam = standing();
  const under = like(slam, THISTLE, slam.hero.x, FLOOR_ROW * TILE);
  slam.hero.y -= TILE * 5;
  slam.hero.before = slam.hero.y;
  slam.hero.ground = false;
  tap(slam, 'down');
  const events = listen(slam, R.SLAM_HOLD + 20);
  assert.ok(events.some(s => s.what === 'hover') && events.some(s => s.what === 'kill'));
  assert.ok(!slam.enemies.includes(under));
  assert.equal(slam.hero.form, SMALL);
  assert.equal(slam.hero.dying, 0);
});

test('the bell reaches so far and no further, and waits before it rings again', () => {
  const g = standing();
  const near = like(g, MUMBLE, g.hero.x + R.RING_REACH - 10, FLOOR_ROW * TILE, {timer: 0});
  const far = like(g, MUMBLE, g.hero.x + R.RING_REACH + 40, FLOOR_ROW * TILE);
  tap(g, 'ring');
  R.step(g);
  assert.ok(near.stun > 0);
  assert.equal(far.stun, 0);
  near.stun = 0;
  tap(g, 'ring');
  R.step(g);
  assert.equal(near.stun, 0, 'too soon');
  steps(g, R.RING_COOL);
  tap(g, 'ring');
  R.step(g);
  assert.ok(near.stun > 0);
});

test('a hush block is nothing until rung, there while it lasts, and not woken round Tam', () => {
  const g = standing();
  const r = FLOOR_ROW - 1, c = 22;
  g.changed[key(g, c, r)] = 'H';
  assert.equal(R.solidAt(g, c, r), false);
  // Walked into asleep, Tam goes through.
  put(g, c - 1, FLOOR_ROW);
  hold(g, 'right');
  steps(g, 30);
  assert.ok(g.hero.x > (c + 1) * TILE);
  letGo(g, 'right');
  steps(g, 40);
  // Rung from beside it, it stands.
  put(g, c - 2, FLOOR_ROW);
  tap(g, 'ring');
  R.step(g);
  assert.equal(R.solidAt(g, c, r), true);
  hold(g, 'right');
  steps(g, 40);
  assert.equal(g.hero.x, c * TILE - R.HERO_W);
  letGo(g, 'right');
  steps(g, R.HUSH_STEPS);
  assert.equal(R.solidAt(g, c, r), false, 'it goes back to sleep');
  // Rung while inside it, it stays asleep and nobody is shut in.
  put(g, c, FLOOR_ROW);
  steps(g, R.RING_COOL);
  tap(g, 'ring');
  R.step(g);
  assert.equal(R.solidAt(g, c, r), false);
});

test('blocks give: a note, several notes, a power by the form Tam has, a life; a brick breaks for brass and not for small', () => {
  const struck = (tile, form = SMALL) => {
    const g = standing();
    const r = FLOOR_ROW - 3;
    g.changed[key(g, 24, r)] = tile;
    g.hero.form = form;
    put(g, 24, FLOOR_ROW);
    hold(g, 'jump');
    steps(g, 30);
    letGo(g, 'jump');
    return {g, r, tile: R.tileAt(g, 24, r)};
  };
  const note = struck('Q');
  assert.equal(note.tile, 'U');
  assert.equal(note.g.notes, 1);
  const again = note.g;
  put(again, 24, FLOOR_ROW);
  hold(again, 'jump');
  steps(again, 30);
  letGo(again, 'jump');
  assert.equal(again.notes, 1, 'once only');

  const rich = standing();
  const r = FLOOR_ROW - 3;
  rich.changed[key(rich, 24, r)] = 'R';
  let bumps = 0;
  while (R.tileAt(rich, 24, r) === 'R' && bumps < 20) {
    put(rich, 24, FLOOR_ROW);
    hold(rich, 'jump');
    steps(rich, 30);
    letGo(rich, 'jump');
    bumps += 1;
  }
  assert.ok(rich.notes >= 3 && rich.notes <= 7, `a rich block gave ${rich.notes}`);
  assert.equal(R.tileAt(rich, 24, r), 'U');

  assert.equal(struck('P').g.items[0].what, 'brass');
  assert.equal(struck('P', BRASS).g.items[0].what, 'kite');
  assert.equal(struck('L').g.items[0].what, 'life');

  assert.equal(struck('B').tile, 'B');
  assert.equal(struck('B', BRASS).tile, '.');
  assert.equal(struck('B', KITE).g.score, R.BRICK_WORTH);

  // What stands on a block struck falls off it.
  const under = standing();
  under.changed[key(under, 24, r)] = 'Q';
  like(under, HUSHLING, 24.5 * TILE, r * TILE, {ground: true, face: 1});
  put(under, 24, FLOOR_ROW);
  hold(under, 'jump');
  steps(under, 30);
  assert.equal(under.enemies.length, 0);
  assert.equal(under.kills, 1);
});

test('what Tam picks up changes its form: brass takes a blow, the kite glides, and small falls', () => {
  const pickUp = what => {
    const g = standing();
    g.items.push({id: 1, what, x: g.hero.x, y: g.hero.y, vx: 0, vy: 0, rising: 0, face: 1});
    const events = listen(g, 2);
    return {g, events};
  };
  assert.equal(pickUp('brass').g.hero.form, BRASS);
  assert.equal(pickUp('kite').g.hero.form, KITE);
  assert.equal(pickUp('life').g.lives, R.LIVES + 1);
  // The kite falls slower with the jump held, and at the usual rate without it.
  const fall = (form, held) => {
    const g = standing();
    g.hero.form = form;
    g.hero.y -= TILE * 6;
    g.hero.before = g.hero.y;
    g.hero.ground = false;
    if (held) hold(g, 'jump');
    g.hero.buffer = 0;
    const y = g.hero.y;
    steps(g, 20);
    return g.hero.y - y;
  };
  assert.ok(fall(KITE, true) < fall(KITE, false) / 2);
  assert.equal(fall(BRASS, true) > fall(KITE, true), true);
  // A blow takes the form, and holds off the next; a second, after the hold, is a fall.
  const g = pickUp('brass').g;
  like(g, MUMBLE, g.hero.x, FLOOR_ROW * TILE);
  R.step(g);
  assert.equal(g.hero.form, SMALL);
  assert.equal(g.hero.dying, 0);
  assert.ok(g.hero.hurt > 0);
  steps(g, R.HURT_HOLD - 2);
  assert.equal(g.hero.dying, 0);
  steps(g, 3);
  g.enemies = [];
  like(g, MUMBLE, g.hero.x, FLOOR_ROW * TILE);
  R.step(g);
  assert.ok(g.hero.dying > 0);
});

test('notes climb the scale while they come quickly; a hundred are a life; and a fall does not put them back', () => {
  const g = standing();
  const map = MAPS[0];
  const first = map.notes[0];
  const note = n => { const [c, r] = map.notes[n]; g.hero.x = (c + 0.5) * TILE; g.hero.y = (r + 1) * TILE; g.hero.before = g.hero.y; g.hero.vy = 0; };
  note(0);
  let events = listen(g, 1);
  assert.equal(events.find(s => s.what === 'note').pitch, 0);
  assert.equal(g.notes, 1);
  note(1);
  events = listen(g, 1);
  assert.equal(events.find(s => s.what === 'note').pitch, 1);
  g.lastNote = -1000;
  note(2);
  events = listen(g, 1);
  assert.equal(events.find(s => s.what === 'note').pitch, 0);
  // A hundred.
  g.toLife = R.NOTES_FOR_LIFE - 1;
  const lives = g.lives;
  note(3);
  events = listen(g, 1);
  assert.equal(g.lives, lives + 1);
  assert.ok(events.some(s => s.what === 'life'));
  // After a fall, what was picked up is gone, and what was struck stays struck.
  put(g, 16, FLOOR_ROW);
  g.hero.y = (map.rows + 4) * TILE;
  g.hero.before = g.hero.y;
  const notes = g.notes, got = g.got.slice();
  steps(g, R.DYING_STEPS + 2);
  assert.equal(g.notes, notes);
  assert.deepEqual(g.got, got);
  assert.ok(first);
});

test('a moving platform carries Tam; one that falls waits, falls, and comes back; a spring throws Tam high', () => {
  const g = started(3, 1);
  g.enemies = [];
  const p = g.platforms.find(q => q.what === 'mover');
  p.t = 0;
  const h = g.hero;
  R.step(g);
  h.x = p.x + 40; h.y = p.y - 2; h.before = h.y; h.vy = 0; h.ground = false;
  steps(g, 4);
  assert.equal(h.on, p.id);
  const x = h.x, px = p.x;
  steps(g, 30);
  assert.ok(Math.abs((h.x - x) - (p.x - px)) < 0.01, 'carried as far as the platform went');
  assert.ok(p.x > px);
  assert.equal(h.y, p.y);

  const f = started(1, 1);
  f.enemies = [];
  const fall = f.platforms.find(q => q.what === 'faller');
  f.hero.x = fall.x + 30; f.hero.y = fall.y - 1; f.hero.before = f.hero.y; f.hero.vy = 0; f.hero.ground = false;
  steps(f, 2);
  assert.equal(fall.act, 'shaking');
  const y = fall.y;
  steps(f, R.FALLER_WAIT - 3);
  assert.equal(fall.y, y);
  steps(f, 30);
  assert.equal(fall.act, 'falling');
  assert.ok(fall.y > y);
  f.hero.x = 10 * TILE; f.hero.y = MAPS[1].start[1] * TILE; f.hero.on = -1; f.hero.before = f.hero.y; f.hero.vy = 0;
  let waited = 0;
  while (fall.act !== 'still' && waited++ < 900) { f.hero.y = (MAPS[1].start[1] + 1) * TILE; f.hero.dying = 0; R.step(f); }
  assert.equal(fall.act, 'still');
  assert.equal(fall.y, y);

  const s = standing();
  const [c, r] = MAPS[0].springs[0];
  s.hero.x = (c + 0.5) * TILE; s.hero.y = r * TILE; s.hero.before = s.hero.y; s.hero.vy = 2; s.hero.ground = false;
  const top = s.hero.y;
  steps(s, 6);
  assert.ok(s.hero.vy < -8);
  const high = peak(s, 60);
  assert.ok(top - high > TILE * 6, `a spring threw Tam ${(top - high) / TILE} tiles`);
});

test('what flies keeps its beat, what hops comes on, a gourd spits, a seed hurts, and icicles fall on who stands under them', () => {
  const g = standing();
  g.hero.x = 16.5 * TILE;
  const f = like(g, FLIT, 20 * TILE, 8 * TILE);
  let low = Infinity, high = -Infinity, up = Infinity, down = -Infinity;
  for (let i = 0; i < 300; i++) {
    R.step(g);
    low = Math.min(low, f.x); high = Math.max(high, f.x); up = Math.min(up, f.y); down = Math.max(down, f.y);
  }
  assert.ok(high - low > 100 && high - low < 170, `its beat: ${high - low}`);
  assert.ok(down - up > 10 && down - up < 18, `its bob: ${down - up}`);

  const b = standing();
  const hop = like(b, BOING, b.hero.x + 6 * TILE, FLOOR_ROW * TILE, {timer: 1, phase: 40});
  const heard = listen(b, 3);
  assert.ok(heard.some(s => s.what === 'boing'));
  assert.equal(hop.face, -1);
  assert.ok(hop.vy < 0 && hop.vx < 0);

  const s = standing();
  const gourd = like(s, GOURD, s.hero.x + 8 * TILE, FLOOR_ROW * TILE, {timer: 1});
  listen(s, 2);
  assert.equal(s.shots.length, 1);
  assert.equal(s.shots[0].vx, -R.SEED_SPEED);
  assert.ok(gourd);
  s.shots[0].x = s.hero.x + 4;
  s.shots[0].y = s.hero.y - 10;
  R.step(s);
  assert.ok(s.hero.dying > 0, 'a seed is a blow');

  const i = standing();
  const ice = like(i, ICICLE, i.hero.x, 3 * TILE + KINDS[ICICLE].h, {act: 'hang'});
  const creaks = listen(i, 2);
  assert.ok(creaks.some(c => c.what === 'creak'));
  assert.equal(ice.act, 'shake');
  i.hero.x += 100;
  const shatters = listen(i, 80);
  assert.ok(shatters.some(c => c.what === 'shatter'));
  assert.equal(i.enemies.length, 0);
  assert.equal(i.hero.dying, 0);
  // One not moved away is struck by it.
  const j = standing();
  const held = like(j, ICICLE, j.hero.x, FLOOR_ROW * TILE - 20, {act: 'fall'});
  R.step(j);
  assert.ok(j.hero.dying > 0);
  assert.ok(held);
});

test('a fall spends a life and wakes Tam at the last mid-way bell, its clock whole; the clock running out is a fall; none left ends the run', () => {
  const g = started();
  clear(g);
  const map = MAPS[0], [c, r] = map.checks[0];
  g.hero.x = (c + 0.5) * TILE; g.hero.y = (r + 1) * TILE; g.hero.before = g.hero.y; g.hero.vy = 0;
  const events = listen(g, 1);
  assert.ok(events.some(s => s.what === 'check'));
  assert.equal(g.check, 0);
  g.hero.y = (map.rows + 4) * TILE;
  g.hero.before = g.hero.y;
  g.time = 500;
  steps(g, R.DYING_STEPS + 2);
  assert.equal(g.lives, R.LIVES - 1);
  assert.equal(g.falls, 1);
  assert.equal(Math.floor(g.hero.x / TILE), c);
  assert.ok(g.time > 500);
  // What walked is back.
  assert.equal(g.enemies.length, map.spawns.length);

  // The clock.
  g.time = 3;
  const late = listen(g, 4);
  assert.ok(late.some(s => s.what === 'late') && late.some(s => s.what === 'die'));
  steps(g, R.DYING_STEPS + 2);
  assert.equal(g.lives, R.LIVES - 2);
  g.hero.y = (map.rows + 4) * TILE;
  g.hero.before = g.hero.y;
  steps(g, R.DYING_STEPS + 3);
  assert.equal(g.lives, 0);
  assert.equal(g.phase, 'over');
  assert.equal(R.outcome(g).over, true);
  assert.equal(R.outcome(g).won, false);
  R.click(g, 'Play again');
  assert.equal(g.phase, 'playing');
  assert.equal(g.lives, R.LIVES);
  assert.equal(g.score, 0);
});

test('up at a door goes through it and back, and a door is no use from the air', () => {
  const g = started();
  clear(g);
  const door = MAPS[0].doors[0];
  put(g, door.a[0], door.a[1] + 1);
  R.step(g);
  assert.equal(g.hero.ground, true);
  const events = (tap(g, 'up'), listen(g, 1));
  assert.ok(events.some(s => s.what === 'door'));
  assert.equal(Math.floor(g.hero.x / TILE), door.b[0]);
  assert.ok(g.hero.hold > 0);
  steps(g, 30);
  tap(g, 'up');
  R.step(g);
  assert.equal(Math.floor(g.hero.x / TILE), door.a[0]);
  steps(g, 30);
  hold(g, 'jump');
  steps(g, 6);
  letGo(g, 'jump');
  const x = g.hero.x;
  tap(g, 'up');
  R.step(g);
  assert.equal(g.hero.x, x);
});

test('the Great Bell ends a level: higher on its post is worth more, the clock left scores, and the next level begins with the form Tam has', () => {
  const reach = high => {
    const g = started();
    clear(g);
    const goal = MAPS[0].goal;
    g.hero.form = BRASS;
    g.hero.x = (goal.c - 1) * TILE;
    g.hero.y = goal.floor - high * TILE;
    g.hero.before = g.hero.y;
    g.hero.vy = 0;
    g.hero.ground = false;
    g.time = 60 * 100;
    const before = g.score;
    hold(g, 'right', 'run');
    g.hero.vx = R.RUN;
    const events = listen(g, 12);
    return {g, gained: g.score - before, goal: events.find(s => s.what === 'goal')};
  };
  const low = reach(0), up = reach(5);
  assert.ok(low.goal && up.goal);
  assert.equal(low.goal.high, 0);
  assert.ok(up.goal.high >= 4);
  assert.ok(up.gained > low.gained);
  assert.ok(low.gained >= 100 + 99 * R.SECOND_WORTH);
  const {g} = up;
  assert.ok(g.clearing > 0);
  const x = g.hero.x;
  letGo(g, 'right', 'run');
  steps(g, R.CLEARING_STEPS - 20);
  assert.equal(g.hero.x, x, 'no one moves while it rings');
  assert.equal(g.level, 0);
  const on = listen(g, 40);
  assert.equal(g.level, 1);
  assert.equal(g.levels, 1);
  assert.equal(g.hero.form, BRASS);
  assert.ok(on.some(s => s.what === 'level'));
  assert.deepEqual(g.hero.held, {left: false, right: false, up: false, down: false, jump: false, run: false, ring: false});
});

test('Old Muffle hurts a stomp while he is shut, takes one when he stands open or is rung, grows quicker, and ended he ends the level', () => {
  const fight = () => {
    const g = started(1, 3);
    const e = g.enemies.find(x => x.kind === MUFFLE);
    g.enemies = [e];
    g.platforms = [];
    const h = g.hero;
    h.x = e.x - 200; h.y = e.y; h.before = h.y; h.vy = 0; h.ground = true;
    return {g, e, h};
  };
  const drop = (g, e, h) => {
    h.x = e.x;
    h.y = e.y - KINDS[MUFFLE].h - 14;
    h.before = h.y;
    h.vy = 3;
    h.ground = false;
    h.dying = 0;
  };
  const shut = fight();
  assert.equal(shut.e.hits, 3);
  shut.e.act = 'prowl';
  shut.e.timer = 100;
  shut.h.form = BRASS;
  drop(shut.g, shut.e, shut.h);
  steps(shut.g, 4);
  assert.equal(shut.e.hits, 3);
  assert.equal(shut.h.form, SMALL);

  const open = fight();
  open.e.act = 'open';
  open.e.timer = R.OPEN_STEPS;
  drop(open.g, open.e, open.h);
  steps(open.g, 4);
  assert.equal(open.e.hits, 2);
  assert.equal(open.e.act, 'reeling');
  // Reeling, nothing touches him.
  drop(open.g, open.e, open.h);
  steps(open.g, 4);
  assert.equal(open.e.hits, 2);

  // Rung, he stands stunned, and takes a stomp; he will not hear the bell twice in a row.
  const rung = fight();
  rung.h.x = rung.e.x - 90;
  rung.h.y = rung.e.y;
  tap(rung.g, 'ring');
  R.step(rung.g);
  assert.equal(R.muffleOpen(rung.e), true);
  assert.equal(rung.e.deaf, R.MUFFLE_DEAF - 1);
  drop(rung.g, rung.e, rung.h);
  steps(rung.g, 4);
  assert.equal(rung.e.hits, 2);
  rung.e.stun = 0;
  rung.e.deaf = 200;
  rung.e.act = 'prowl';
  rung.h.x = rung.e.x - 90;
  rung.h.ringCool = 0;
  tap(rung.g, 'ring');
  R.step(rung.g);
  assert.equal(rung.e.stun, 0);

  // He leaps, lands, sends two waves along the ground, and stands open.
  const leap = fight();
  leap.h.x = leap.e.x - 300;
  leap.e.act = 'prowl';
  leap.e.timer = 1;
  leap.e.ground = true;
  const events = listen(leap.g, 200);
  assert.ok(events.some(s => s.what === 'leap'));
  assert.ok(events.some(s => s.what === 'thump'));

  // The last hit: the level is done.
  const last = fight();
  last.e.hits = 1;
  last.e.act = 'open';
  drop(last.g, last.e, last.h);
  const end = listen(last.g, 4);
  assert.ok(end.some(s => s.what === 'kill' && s.kind === MUFFLE));
  assert.ok(last.g.clearing > 0);
  assert.equal(last.g.enemies.length, 0);
  assert.equal(last.g.kills, 1);
  assert.ok(last.g.score >= KINDS[MUFFLE].worth);
  steps(last.g, R.CLEARING_STEPS + 2);
  assert.equal(last.g.level, 4);

  // At the Last Belfry he takes five, and ended the run is won.
  const final = started(1, 7);
  const him = final.enemies.find(x => x.kind === MUFFLE);
  assert.equal(him.hits, 5);
  final.enemies = [him];
  him.hits = 1;
  him.act = 'open';
  drop(final, him, final.hero);
  steps(final, 4);
  steps(final, R.CLEARING_STEPS + 2);
  assert.equal(final.phase, 'over');
  assert.equal(R.outcome(final).won, true);
  assert.equal(R.outcome(final).over, true);
  assert.equal(R.outcome(final).levels, 1);
});

test('the pilot runs the first level to its bell, whether it looks each step or only every few; one that jumps only at walls spends its lives', () => {
  for (const every of [1, 4, 8]) {
    for (const seed of [1, 2, 9]) {
      const g = started(seed), pilot = newPilot();
      for (let i = 0; i < 40000 && g.phase === 'playing' && g.level === 0; i++) {
        if (i % every === 0) {
          const choice = pilot(R, g, false);
          for (const a of ['right', 'left', 'run', 'jump'])
            if (choice[a] !== g.hero.held[a]) (choice[a] ? R.press : R.release)(g, a);
        }
        R.step(g);
      }
      assert.equal(g.level, 1, `looking every ${every}, seed ${seed}`);
      assert.ok(g.notes >= 3);
    }
  }
  const g = started(1), pilot = newPilot();
  for (let i = 0; i < 40000 && g.phase === 'playing'; i++) {
    const choice = pilot(R, g, true);
    for (const a of ['right', 'left', 'run', 'jump'])
      if (choice[a] !== g.hero.held[a]) (choice[a] ? R.press : R.release)(g, a);
    R.step(g);
  }
  assert.equal(g.phase, 'over');
  assert.equal(g.lives, 0);
  assert.equal(R.outcome(g).won, false);
  // Quickly: ARQADE's games suite plays this, and a run held up by the clock would outlast what a case is allowed.
  assert.ok(g.steps < 60 * 90, `it took ${g.steps} steps`);
});

test('a still Tam is caught by the clock, every life, and the run ends', () => {
  const g = started();
  clear(g);
  let spent = 0;
  while (g.phase === 'playing' && spent++ < 200000) R.step(g);
  assert.equal(g.phase, 'over');
  assert.equal(g.falls, R.LIVES);
  assert.ok(g.steps >= R.LIVES * MAPS[0].seconds * 60);
});

const route = new URL('./fixtures/bellwick-route.json', import.meta.url);
test('every level can be finished: a run the solver found by these rules goes from the title to the last bell', () => {
  // Written by bellwick-solver.mjs, which tries what a hand could do. A level no one could finish cannot be committed.
  const {seed, edges, outcome} = JSON.parse(readFileSync(route, 'utf8'));
  const at = new Map();
  for (const [step, list] of edges) at.set(step, [...(at.get(step) || []), ...list]);
  const g = R.create(seed);
  R.click(g, 'Play');
  const seen = new Set();
  for (let i = 0; i < 400000 && g.phase === 'playing'; i++) {
    seen.add(g.level);
    for (const [what, action] of at.get(g.steps) || []) (what === 'pressed' ? R.press : R.release)(g, action);
    R.step(g);
  }
  assert.equal(g.phase, 'over');
  assert.equal(g.won, true);
  assert.equal(seen.size, 8);
  assert.equal(g.levels, 8);
  assert.deepEqual(R.outcome(g), outcome);
});

test('a run replays to the outcome of the same events played live', () => {
  const g = R.create(21), pilot = newPilot(), edges = [], clicks = [[0, ['Play']]];
  let tick = 0;
  R.click(g, 'Play');
  const hands = [];
  for (; tick < 4000 && g.phase === 'playing'; tick++) {
    const choice = pilot(R, g, tick > 2500);
    hands.length = 0;
    for (const a of ['right', 'left', 'run', 'jump']) {
      if (choice[a] !== g.hero.held[a]) {
        (choice[a] ? R.press : R.release)(g, a);
        hands.push([choice[a] ? 'pressed' : 'released', a]);
      }
    }
    if (hands.length > 0) edges.push([tick, hands.slice()]);
    R.step(g);
  }
  assert.deepEqual(R.replay({qqRun: 1, seed: 21, ticks: tick, edges, clicks}), R.outcome(g));
  assert.throws(() => R.replay({qqRun: 2, seed: 1, ticks: 0}));
});

const fixture = new URL('./fixtures/bellwick-run.json', import.meta.url);
test('the run QQ kept as ARQADE played it replays here to the outcome QQ reached, and an altered one does not', () => {
  // Kept by the Qt app's games suite (qt/tests/tst_games.cpp), which plays Bellwick in ARQADE's window through its first level
  // and on until its lives are spent, and replays this same file with QQ's own replay.
  const run = JSON.parse(readFileSync(fixture, 'utf8'));
  assert.equal(run.qqRun, 1);
  assert.equal(run.scene, 'bellwick');
  assert.deepEqual(R.replay(run), run.outcome['Bellwick']);
  assert.equal(run.outcome['Bellwick'].levels, 1);
  // The same run never jumping is another run with another outcome.
  const calm = run.edges.map(([tick, list]) => [tick, list.filter(([, action]) => action !== 'jump')]);
  assert.notDeepEqual(R.replay({...run, edges: calm}), run.outcome['Bellwick']);
});

test('the rules\' whole state at the end of the kept run is pinned, so no change to the rules passes unseen', () => {
  const run = JSON.parse(readFileSync(fixture, 'utf8'));
  const edges = new Map(run.edges.map(([t, l]) => [t, l])), clicks = new Map(run.clicks.map(([t, l]) => [t, l])), g = R.create(run.seed);
  for (let t = 0; t < run.ticks; t++) {
    for (const b of clicks.get(t) || []) R.click(g, b);
    for (const [w, a] of edges.get(t) || []) (w === 'pressed' ? R.press : R.release)(g, a);
    R.step(g);
  }
  assert.equal(createHash('sha256').update(JSON.stringify(g)).digest('hex'), 'dc0c729d91b71e0c70beb58d8ce0b3f550c064902621e684fb07bb34493bf66e');
});

test('the rules keep to arithmetic every machine computes alike: a square root at most, and no sine, cosine, power or logarithm', () => {
  const source = readFileSync(new URL('../games/bellwick/logic/rules.mjs', import.meta.url), 'utf8').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
  assert.doesNotMatch(source, /Math\.(sin|cos|tan|asin|acos|atan2?|sinh|cosh|tanh|exp|expm1|log\w*|pow|cbrt|hypot|random)\b/);
  assert.doesNotMatch(source, /\*\*/);
  assert.doesNotMatch(source, /\b(Date|performance)\b/);
});
