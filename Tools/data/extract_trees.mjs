// One-off importer: plain-text export of the design doc's "Skill trees" tab -> Data/paths/<id>.json.
// Usage: node Tools/data/extract_trees.mjs <tab2.txt> <outDir>
// Re-run whenever Raven edits the trees; hand edits to the JSON outside "grants"/"impl" get overwritten.
import fs from 'node:fs';
import path from 'node:path';

const [, , src, outDir] = process.argv;
const lines = fs.readFileSync(src, 'utf8').split(/\r?\n/);

const PATHS = {
  Fire: { id: 'fire', biome: 'volcanic', resource: { id: 'heat', name: 'Heat' } },
  Frost: { id: 'frost', biome: 'tundra', resource: { id: 'chill', name: 'Chill' } },
};

const slug = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '');

for (const [title, meta] of Object.entries(PATHS)) {
  const start = lines.findIndex(l => l.trim() === `## ${title}`);
  if (start < 0) throw new Error(`no section ${title}`);
  let end = lines.findIndex((l, i) => i > start && l.startsWith('## '));
  if (end < 0) end = lines.length;
  const section = lines.slice(start + 1, end);

  const rootLine = section.find(l => l.startsWith('Root passive'));
  const rootMatch = rootLine.match(/:\s*([^.]+)\.\s*(.*)$/);
  const cells = section.filter(l => l.trim().endsWith('|')).map(l => l.replace(/\|\s*$/, '').trim());
  // Header row is the first 4 cells.
  const rows = [];
  for (let i = 4; i + 3 < cells.length; i += 4) rows.push(cells.slice(i, i + 4));

  const tree = {
    id: meta.id,
    name: title,
    biome: meta.biome,
    resource: meta.resource,
    rootPassive: { id: slug(rootMatch[1]), name: rootMatch[1].trim(), text: rootMatch[2].trim() },
    trunk: [],
    branches: [],
  };

  const gateFor = spent => Math.min(20, spent);
  const branchMap = new Map();
  for (const [branch, name, cost, effect] of rows) {
    const ranks = cost.match(/(\d+)\s*ranks?/);
    const pts = cost.match(/(\d+)\s*pts?/);
    const isCap = /capstone/i.test(effect) || /capstone/i.test(cost);
    const node = {
      id: slug(name),
      name,
      kind: ranks ? 'stat' : isCap ? 'capstone' : 'ability',
      ...(ranks ? { ranks: +ranks[1] } : { cost: +(pts ? pts[1] : 10) }),
      text: effect.replace(/^\s*Capstone[.:]?\s*/i, ''),
    };
    if (branch === 'Trunk') {
      tree.trunk.push(node);
    } else {
      if (!branchMap.has(branch)) branchMap.set(branch, { id: slug(branch), name: branch, nodes: [], spent: 0 });
      const b = branchMap.get(branch);
      node.gate = isCap ? 20 : gateFor(b.spent);
      b.spent += node.kind === 'stat' ? node.ranks : node.cost;
      b.nodes.push(node);
    }
  }
  tree.branches = [...branchMap.values()].map(({ spent, ...b }) => b);

  // Validate the tree shape the design doc promises.
  const sum = ns => ns.reduce((a, n) => a + (n.kind === 'stat' ? n.ranks : n.cost), 0);
  const problems = [];
  if (sum(tree.trunk) !== 10) problems.push(`trunk sums to ${sum(tree.trunk)}`);
  for (const b of tree.branches) if (sum(b.nodes) !== 30) problems.push(`${b.name} sums to ${sum(b.nodes)}`);
  if (tree.branches.length !== 3) problems.push(`${tree.branches.length} branches`);

  fs.mkdirSync(outDir, { recursive: true });
  fs.writeFileSync(path.join(outDir, `${meta.id}.json`), JSON.stringify(tree, null, 2) + '\n');
  console.log(`${title}: trunk ${tree.trunk.length} nodes, branches ${tree.branches.map(b => b.name).join('/')}`,
    problems.length ? `PROBLEMS: ${problems.join('; ')}` : 'OK');
}
