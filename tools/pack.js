#!/usr/bin/env node
// Pack a directory tree into a single text file.
// Usage: node tools/pack.js <src-dir> <out-file> [--include-binary]
//
// Format per entry:
//   ===FILE=== <relative/path>\n
//   ===SIZE=== <bytes>\n
//   ===ENC===  utf8|base64\n
//   ===BODY===\n
//   <content>\n
//   ===END===\n
//
// Skips VCS/internal dirs by default and common build/cache folders.
// Pass --all to include optional ignore buckets, but VCS/internal dirs are always skipped.

const fs = require('fs');
const path = require('path');

const ALWAYS_IGNORES = [
  '.git', '.hg', '.svn',
];

const DEFAULT_IGNORES = [
  'node_modules', 'build', 'dist', 'out',
  'tools/portable', '.vscode', '.idea',
  '.cache', '.sccache', '_vcpkg_cache',
];

const TEXT_EXT = new Set([
  '.md', '.txt', '.json', '.yml', '.yaml', '.toml', '.ini', '.cfg',
  '.c', '.h', '.cpp', '.hpp', '.cc', '.cxx', '.cppm', '.inl',
  '.js', '.mjs', '.cjs', '.ts', '.tsx', '.jsx',
  '.py', '.rb', '.rs', '.go', '.java', '.kt',
  '.sh', '.ps1', '.bat', '.cmd',
  '.html', '.css', '.scss', '.xml', '.wgsl', '.glsl', '.hlsl',
  '.gitignore', '.gitattributes', '.editorconfig', '.clang-format', '.clang-tidy',
]);

function isText(file) {
  const base = path.basename(file);
  if (base.startsWith('.')) return TEXT_EXT.has(base);
  return TEXT_EXT.has(path.extname(file).toLowerCase());
}

function isIgnored(rel, ignores) {
  const norm = rel.replace(/\\/g, '/');
  return ignores.some(ig => norm === ig || norm.startsWith(ig + '/'));
}

function walk(dir, root, ignores, out) {
  for (const name of fs.readdirSync(dir).sort()) {
    const full = path.join(dir, name);
    const rel = path.relative(root, full);
    if (isIgnored(rel, ignores)) continue;
    const stat = fs.lstatSync(full);
    if (stat.isSymbolicLink()) continue;
    if (stat.isDirectory()) walk(full, root, ignores, out);
    else if (stat.isFile()) out.push(rel);
  }
}

function main() {
  const args = process.argv.slice(2);
  const includeBinary = args.includes('--include-binary');
  const all = args.includes('--all');
  const positional = args.filter(a => !a.startsWith('--'));
  if (positional.length !== 2) {
    console.error('usage: node tools/pack.js <src-dir> <out-file> [--include-binary] [--all]');
    process.exit(1);
  }
  const [src, outFile] = positional;
  const root = path.resolve(src);
  const outAbs = path.resolve(outFile);
  if (!fs.statSync(root).isDirectory()) {
    console.error(`not a directory: ${root}`);
    process.exit(1);
  }
  const ignores = [
    ...ALWAYS_IGNORES,
    ...(all ? [] : DEFAULT_IGNORES),
  ];

  // Prevent recursive/self-pack when output is inside source root.
  const outRel = path.relative(root, outAbs);
  if (outRel && !outRel.startsWith('..') && !path.isAbsolute(outRel)) {
    ignores.push(outRel.replace(/\\/g, '/'));
  }

  const files = [];
  walk(root, root, ignores, files);

  const chunks = [];
  chunks.push(`===PACK=== v1 root=${path.basename(root)} count=${files.length}\n`);
  let textCount = 0, binCount = 0, skipCount = 0;

  for (const rel of files) {
    const full = path.join(root, rel);
    const text = isText(rel);
    if (!text && !includeBinary) { skipCount++; continue; }
    const buf = fs.readFileSync(full);
    const enc = text ? 'utf8' : 'base64';
    const body = text ? buf.toString('utf8') : buf.toString('base64');
    const relPosix = rel.replace(/\\/g, '/');
    chunks.push(`===FILE=== ${relPosix}\n`);
    chunks.push(`===SIZE=== ${buf.length}\n`);
    chunks.push(`===ENC=== ${enc}\n`);
    chunks.push(`===BODY===\n`);
    chunks.push(body);
    if (!body.endsWith('\n')) chunks.push('\n');
    chunks.push(`===END===\n`);
    if (text) textCount++; else binCount++;
  }

  fs.mkdirSync(path.dirname(path.resolve(outFile)), { recursive: true });
  fs.writeFileSync(outFile, chunks.join(''));
  console.log(`packed ${textCount + binCount} files -> ${outFile}`);
  console.log(`  text: ${textCount}, binary: ${binCount}, skipped: ${skipCount}`);
}

main();
