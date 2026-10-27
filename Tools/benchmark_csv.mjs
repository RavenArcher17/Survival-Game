// Summarises one Unreal CSV profile (csvprofile) for Tools/benchmark.ps1 and prints it as one line of JSON.
// Frames are cut by elapsed seconds since the capture started (the sum of FrameTime), so the benchmark's fixed route
// (camp, walk, long move) can be split into phases and a server capture can be cut to the window a player was there.
//   node benchmark_csv.mjs <file.csv> [--from <s>] [--to <s>] [--phases camp:0:20,walk:20:110,move:110:140] [--cols <csv column>,...]
// Per frame set: frame time average / p50 / p95 / p99 / max, game / draw (render thread) / RHI / GPU averages,
// hitches over 50 and 100 ms, garbage collection (frames that ran it, longest), level streaming (seconds until
// nothing is left loading or becoming visible), process memory, GPU use by other programs (machine load), and the
// game thread's work per frame (the sum of its exclusive timings: on a 30 Hz dedicated server the frame time is
// mostly sleep, so this is its real cost) with the biggest categories.
import fs from 'fs';

const args = process.argv.slice(2);
const file = args[0];
const opt = (name, def) => { const i = args.indexOf('--' + name); return i >= 0 ? args[i + 1] : def; };
const from = parseFloat(opt('from', '0'));
const to = parseFloat(opt('to', '1e9'));
const extraCols = opt('cols', '').split(',').filter(Boolean);
const phases = opt('phases', '').split(',').filter(Boolean).map(p => { const [name, a, b] = p.split(':'); return { name, a: parseFloat(a), b: parseFloat(b) }; });

const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/);
// Stats that first show up mid-capture add columns at the end, so the full header is the one written again at the
// end, just before the metadata row; rows from before a column existed are shorter (a prefix of it).
let end = lines.findIndex(l => l.startsWith('[HasHeaderRowAtEnd]'));
const header = (end > 1 ? lines[end - 1] : lines[0]).split(',');
if (end < 0) end = lines.length;
else end -= 1;
const col = name => header.indexOf(name);
const rows = [];
for (let i = 1; i < end; i++) {
	if (!lines[i]) continue;
	let cells = lines[i].split(',');
	// The first column is EVENTS (free text): an event with commas in it spills into extra cells.
	if (cells.length > header.length) cells = [cells.slice(0, cells.length - header.length + 1).join(','), ...cells.slice(cells.length - header.length + 1)];
	rows.push(cells);
}
const ft = col('FrameTime');
let t = 0;
const frames = rows.map(r => { const f = { r, t0: t, ms: parseFloat(r[ft]) || 0 }; t += f.ms / 1000; return f; });

const gtCols = header.map((h, i) => [h, i]).filter(([h]) => h.startsWith('Exclusive/GameThread/'));
const num = (f, name) => { const k = col(name); return k < 0 ? NaN : parseFloat(f.r[k]); };
const pct = (sorted, p) => sorted.length ? sorted[Math.min(sorted.length - 1, Math.floor(p / 100 * sorted.length))] : NaN;
const round = (x, d = 2) => (Number.isFinite(x) ? Math.round(x * 10 ** d) / 10 ** d : null);
function avgOf(set, name) {
	if (col(name) < 0) return null;
	let s = 0, n = 0;
	for (const f of set) { const v = num(f, name); if (Number.isFinite(v)) { s += v; n++; } }
	return n ? round(s / n) : null;
}
function stats(set, start) {
	if (!set.length) return { frames: 0 };
	const ms = set.map(f => f.ms).sort((a, b) => a - b);
	const sum = ms.reduce((a, b) => a + b, 0);
	const work = set.map(f => gtCols.reduce((a, [, k]) => a + (parseFloat(f.r[k]) || 0), 0)).sort((a, b) => a - b);
	const gcCols = ['Exclusive/GameThread/GarbageCollection', 'Exclusive/GameThread/GarbageCollectionFull'].map(col).filter(k => k >= 0);
	const gc = set.map(f => gcCols.reduce((a, k) => a + (parseFloat(f.r[k]) || 0), 0));
	const busy = name => set.filter(f => num(f, name) > 0);
	const streamingCols = ['LevelStreaming/NumLevelsLoading', 'LevelStreaming/NumLevelsMakingVisible'];
	const streaming = set.filter(f => streamingCols.some(c => num(f, c) > 0));
	const lastStreaming = streaming.length ? streaming[streaming.length - 1].t0 + streaming[streaming.length - 1].ms / 1000 - start : 0;
	const top = gtCols.map(([h, k]) => [h.replace('Exclusive/GameThread/', ''), set.reduce((a, f) => a + (parseFloat(f.r[k]) || 0), 0) / set.length])
		.sort((a, b) => b[1] - a[1]).slice(0, 6).map(([h, v]) => `${h} ${round(v)}`);
	const mem = set.map(f => num(f, 'PhysicalUsedMB')).filter(Number.isFinite);
	const tickCount = name => avgOf(set, 'Ticks/' + name);
	return {
		frames: set.length, seconds: round(sum / 1000, 1),
		avg: round(sum / set.length), fps: round(1000 / (sum / set.length), 1),
		p50: round(pct(ms, 50)), p95: round(pct(ms, 95)), p99: round(pct(ms, 99)), max: round(ms[ms.length - 1]),
		game: avgOf(set, 'GameThreadTime'), draw: avgOf(set, 'RenderThreadTime'), rhi: avgOf(set, 'RHIThreadTime'), gpu: avgOf(set, 'GPUTime'),
		hitch50: set.filter(f => f.ms > 50).length, hitch100: set.filter(f => f.ms > 100).length,
		gcFrames: gc.filter(v => v > 0.5).length, gcMax: round(Math.max(0, ...gc)),
		streamSettle: round(lastStreaming, 1), streamFrames: streaming.length,
		memMB: mem.length ? Math.round(Math.max(...mem)) : null,
		gpuOther: avgOf(set, 'GPUUsage/ExternalUsage'), gpuMine: avgOf(set, 'GPUUsage/Usage'),
		work: round(work.reduce((a, b) => a + b, 0) / work.length), workP95: round(pct(work, 95)), workP99: round(pct(work, 99)), workMax: round(work[work.length - 1]),
		top: top.join(', '),
		ticks: { total: tickCount('Total'), creature: tickCount('WRCreature'), cmc: tickCount('WRCharacterMovementComponent'), springArm: tickCount('SpringArmComponent'), skel: tickCount('SkeletalMeshComponent'), ai: tickCount('WRCreatureAI') },
		actors: avgOf(set, 'ActorCount/TotalActorCount'),
		cols: Object.fromEntries(extraCols.map(c => [c, avgOf(set, c)])),
	};
}
const pick = (a, b) => frames.filter(f => f.t0 >= a && f.t0 < b);
const out = { file, totalSeconds: round(t, 1), all: stats(pick(from, to), from) };
for (const p of phases) out[p.name] = stats(pick(p.a, p.b), p.a);
console.log(JSON.stringify(out));
