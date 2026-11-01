// SPDX-License-Identifier: Apache-2.0
// Finds a way through Bellwick by trying its own rules: node bellwick-solver.mjs [seed] [--write]
//
// What a level can be finished by is not for anyone's eye to judge, so this searches: from each moment it tries what a hand
// could do for the next few steps (a direction, running, jumping, ringing, a slam), and goes on first from what stands
// nearest the level's end by walking through its air. What it finds is a list of presses and releases that the test
// (bellwick.test.mjs) plays from the title to the end of the game, so a level no one could finish cannot be committed.
import {writeFileSync} from 'node:fs';
import * as R from '../games/bellwick/logic/rules.mjs';

const {TILE, MAPS} = R;

const clone = g => JSON.parse(JSON.stringify(g));

// A way a hand could go for a moment: what is held, and whether the jump or the bell is pressed as it starts.
const WAYS = [];
for (const way of ['right', 'left', 'none'])
  for (const run of way === 'none' ? [false] : [true, false])
    for (const jump of [false, 'short', 'long'])
      WAYS.push({way, run, jump, ring: false, slam: false});
for (const way of ['right', 'left', 'none'])
  WAYS.push({way, run: false, jump: false, ring: true, slam: false});
WAYS.push({way: 'none', run: false, jump: false, ring: false, slam: true});
// At Old Muffle, wherever he has got to.
const AT_HIM = [{way: 'him', run: true, jump: false, ring: false, slam: false}, {way: 'him', run: true, jump: 'long', ring: false, slam: false},
                {way: 'him', run: true, jump: false, ring: true, slam: false}, {way: 'from', run: true, jump: 'long', ring: false, slam: false}];

// How many tiles each tile is from `goal`, moving through anything not solid. Hush blocks are air here.
function distances(map, goal) {
  const far = new Int32Array(map.cols * map.rows).fill(-1);
  const stops = t => t !== '.' && t !== '=' && t !== 'T' && t !== 'H';
  const queue = [goal[1] * map.cols + goal[0]];
  far[queue[0]] = 0;
  for (let head = 0; head < queue.length; head++) {
    const at = queue[head], c = at % map.cols, r = (at - c) / map.cols;
    for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const nc = c + dc, nr = r + dr;
      if (nc < 0 || nr < 0 || nc >= map.cols || nr >= map.rows || far[nr * map.cols + nc] >= 0 || stops(map.tiles[nr].charAt(nc)))
        continue;
      far[nr * map.cols + nc] = far[at] + 1;
      queue.push(nr * map.cols + nc);
    }
  }
  return far;
}

// Where a level's way ends: the Great Bell, or Old Muffle.
function ending(map) {
  if (map.goal)
    return [map.goal.c, map.goal.r];
  const muffle = map.spawns.find(s => s.kind === R.MUFFLE);
  return [muffle.c, muffle.r];
}

class Heap {
  constructor() { this.a = []; }
  push(item) {
    const a = this.a;
    a.push(item);
    for (let i = a.length - 1; i > 0;) {
      const up = (i - 1) >> 1;
      if (a[up].score <= a[i].score) break;
      [a[up], a[i]] = [a[i], a[up]];
      i = up;
    }
  }
  pop() {
    const a = this.a, top = a[0], last = a.pop();
    if (a.length > 0) {
      a[0] = last;
      for (let i = 0;;) {
        const l = 2 * i + 1, r = l + 1;
        let least = i;
        if (l < a.length && a[l].score < a[least].score) least = l;
        if (r < a.length && a[r].score < a[least].score) least = r;
        if (least === i) break;
        [a[least], a[i]] = [a[i], a[least]];
        i = least;
      }
    }
    return top;
  }
  get size() { return this.a.length; }
}

// A way's edges from what is held now: pressed and released, in order, for its first step and for the one its jump ends on.
function play(g, way, length) {
  const h = g.hero, first = [], later = new Map();
  const set = (name, on) => {
    if (h.held[name] !== on) {
      (on ? R.press : R.release)(g, name);
      first.push([on ? 'pressed' : 'released', name]);
    }
  };
  const him = g.enemies.find(e => e.kind === R.MUFFLE);
  const toward = way.way === 'him' || way.way === 'from' ? (him && (him.x > h.x) === (way.way === 'him') ? 'right' : 'left') : way.way;
  set('right', toward === 'right');
  set('left', toward === 'left');
  set('run', way.run);
  if (h.held.jump) {
    R.release(g, 'jump');
    first.push(['released', 'jump']);
  }
  if (way.jump) {
    R.press(g, 'jump');
    first.push(['pressed', 'jump']);
    if (way.jump === 'short')
      later.set(Math.min(4, length - 1), [['released', 'jump']]);
  }
  if (way.ring) {
    R.press(g, 'ring');
    R.release(g, 'ring');
    first.push(['pressed', 'ring'], ['released', 'ring']);
  }
  if (way.slam) {
    R.press(g, 'down');
    R.release(g, 'down');
    first.push(['pressed', 'down'], ['released', 'down']);
  }
  const edges = [];
  if (first.length > 0)
    edges.push([g.steps, first]);
  const level = g.level;
  for (let n = 0; n < length; n++) {
    if (later.has(n) && g.hero.held.jump) {
      R.release(g, 'jump');
      edges.push([g.steps, later.get(n)]);
    }
    R.step(g);
    if (g.level !== level || g.phase !== 'playing' || g.hero.dying > 0 || g.clearing > 0)
      break;
  }
  return edges;
}

// A level searched from `start`: the game when its level is done, and the edges there, or null when `most` looks find none.
export function solveLevel(start, target, most = 400000) {
  const level = start.level, map = MAPS[level];
  const far = distances(map, target || ending(map));
  const fits = g => {
    const h = g.hero, c = Math.floor(h.x / TILE), r = Math.floor((h.y - 1) / TILE);
    const at = r >= 0 && r < map.rows ? far[r * map.cols + c] : -1;
    return at >= 0 ? at : 4000;
  };
  const heap = new Heap(), seen = new Set();
  heap.push({score: 0, g: start, from: null, edges: []});
  let looks = 0, best = Infinity, nearest = start;
  while (heap.size > 0 && looks < most) {
    const node = heap.pop();
    looks += 1;
    const him = node.g.enemies.find(e => e.kind === R.MUFFLE && Math.abs(e.x - node.g.hero.x) < 22 * TILE && Math.abs(e.y - node.g.hero.y) < 8 * TILE);
    for (const way of him ? AT_HIM : WAYS) {
      if (way.slam && node.g.hero.ground) continue;
      const g = clone(node.g);
      const length = way.jump === 'long' ? 14 : way.ring || way.slam ? 4 : 8;
      const edges = play(g, way, length);
      const hero = g.hero;
      const done = target
        ? g.silverGot[map.silverAt[target[1] * map.cols + target[0]]] === 1
        : g.level !== level || g.phase === 'over' || g.clearing > 0;
      if (done)
        return {g, trail: [...walk(node), ...edges], looks};
      if (hero.dying > 0 || hero.hurt > 0 || g.phase !== 'playing')
        continue;
      const anyMuffle = g.enemies.find(e => e.kind === R.MUFFLE);
      // What is near decides how much the clock matters: a platform or a fight must be waited for, and elsewhere only a little.
      const moving = g.platforms.some(p => Math.abs(p.x - hero.x) < 9 * TILE && Math.abs(p.y - hero.y) < 8 * TILE);
      const fighting = anyMuffle && Math.abs(anyMuffle.x - hero.x) < 22 * TILE && Math.abs(anyMuffle.y - hero.y) < 8 * TILE;
      const clock = fighting ? Math.floor(g.steps / 12) % 64 : moving ? Math.floor(g.steps / 24) % 64 : Math.floor(g.steps / 120) % 8;
      const key = [Math.floor(hero.x / 8), Math.floor(hero.y / 8), Math.floor(hero.vx), Math.floor(hero.vy / 2), hero.ground ? 1 : 0,
                   clock, hero.on, anyMuffle ? anyMuffle.hits : 0, anyMuffle ? anyMuffle.act : 0,
                   Object.keys(g.awake).length > 0 ? 1 : 0].join();
      if (seen.has(key)) continue;
      seen.add(key);
      const d = fits(g);
      if (d < best) {
        best = d;
        nearest = g;
      }
      const score = d * 20 + g.steps * 0.02 - (anyMuffle ? (anyMuffle.most - anyMuffle.hits) * 400 : 0);
      heap.push({score, g, from: node, edges});
    }
  }
  return {g: null, trail: null, looks, best, nearest};
}

function walk(node) {
  const out = [];
  for (let n = node; n; n = n.from)
    out.unshift(...n.edges);
  return out;
}

// The whole game from the title: the presses and releases of a run that finishes it.
export function solve(seed, log = () => {}) {
  let g = R.create(seed);
  R.click(g, 'Play');
  const edges = [];
  while (g.phase === 'playing') {
    const level = g.level, at = Date.now();
    const found = solveLevel(g);
    if (!found.g) {
      log(`level ${level} ${MAPS[level].name}: no way found in ${found.looks} looks, nearest ${found.best} tiles off, at column ${Math.floor(found.nearest.hero.x / TILE)}, row ${Math.floor(found.nearest.hero.y / TILE)}`);
      return null;
    }
    edges.push(...found.trail);
    g = found.g;
    // Stand and let it ring out.
    const held = g.hero.held;
    const let_go = [];
    for (const name of ['right', 'left', 'run', 'jump'])
      if (held[name]) {
        R.release(g, name);
        let_go.push(['released', name]);
      }
    if (let_go.length > 0)
      edges.push([g.steps, let_go]);
    while (g.phase === 'playing' && g.level === level)
      R.step(g);
    log(`level ${level} ${MAPS[level].name}: ${found.looks} looks, ${Date.now() - at} ms, score ${g.score}`);
  }
  return {seed, edges, outcome: R.outcome(g)};
}

if (process.argv[1] && process.argv[1].endsWith('bellwick-solver.mjs')) {
  const seed = Number(process.argv[2] ?? 7);
  const route = solve(seed, line => console.log(line));
  console.log(route ? JSON.stringify(route.outcome) : 'no route');
  if (route && process.argv.includes('--write'))
    writeFileSync(new URL('./fixtures/bellwick-route.json', import.meta.url), JSON.stringify(route) + '\n');
}
