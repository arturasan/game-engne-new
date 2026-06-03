#!/usr/bin/env node
// Unpack a file produced by tools/pack.js into a directory tree.
// Usage: node tools/unpack.js <pack-file> <dest-dir> [--force]
//
// Refuses to overwrite existing files unless --force is given.
// Refuses to write outside <dest-dir> (path traversal guard).

const fs = require('fs');
const path = require('path');

function readLine(buf, pos) {
  const nl = buf.indexOf(0x0a, pos);
  if (nl === -1) {
    const line = buf.subarray(pos).toString('utf8').replace(/\r$/, '');
    return { line, next: buf.length };
  }
  const line = buf.subarray(pos, nl).toString('utf8').replace(/\r$/, '');
  return { line, next: nl + 1 };
}

function main() {
  const args = process.argv.slice(2);
  const force = args.includes('--force');
  const positional = args.filter(a => !a.startsWith('--'));
  if (positional.length !== 2) {
    console.error('usage: node tools/unpack.js <pack-file> <dest-dir> [--force]');
    process.exit(1);
  }
  const [packFile, dest] = positional;
  const destRoot = path.resolve(dest);
  fs.mkdirSync(destRoot, { recursive: true });

  const pack = fs.readFileSync(packFile);
  let pos = 0;

  let header = readLine(pack, pos);
  if (!header.line || !header.line.startsWith('===PACK===')) {
    console.error(`not a pack file (missing ===PACK=== header): ${packFile}`);
    process.exit(1);
  }
  pos = header.next;

  let written = 0, skipped = 0;

  while (pos < pack.length) {
    let lineRec = readLine(pack, pos);
    let line = lineRec.line;
    pos = lineRec.next;

    if (line.trim() === '') {
      if (pos >= pack.length) break;
      continue;
    }

    if (!line.startsWith('===FILE===')) {
      console.error(`expected ===FILE===, got: ${line.slice(0, 60)}`);
      process.exit(1);
    }
    const relPath = line.slice('===FILE==='.length).trim();

    lineRec = readLine(pack, pos);
    line = lineRec.line;
    pos = lineRec.next;
    if (!line.startsWith('===SIZE===')) { console.error(`expected ===SIZE=== for ${relPath}`); process.exit(1); }
    const size = parseInt(line.slice('===SIZE==='.length).trim(), 10);
    if (!Number.isFinite(size) || size < 0) {
      console.error(`invalid size for ${relPath}: ${line}`);
      process.exit(1);
    }

    lineRec = readLine(pack, pos);
    line = lineRec.line;
    pos = lineRec.next;
    if (!line.startsWith('===ENC===')) { console.error(`expected ===ENC=== for ${relPath}`); process.exit(1); }
    const enc = line.slice('===ENC==='.length).trim();

    lineRec = readLine(pack, pos);
    line = lineRec.line;
    pos = lineRec.next;
    if (line !== '===BODY===') { console.error(`expected ===BODY=== for ${relPath}`); process.exit(1); }

    let buf;
    if (enc === 'utf8') {
      if (pos + size > pack.length) {
        console.error(`unexpected EOF in ${relPath}`);
        process.exit(1);
      }
      buf = Buffer.from(pack.subarray(pos, pos + size));
      pos += size;

      // pack.js guarantees a newline delimiter before ===END===.
      // Accept either LF (\n) or CRLF (\r\n) after transport/conversion.
      if (pos < pack.length && pack[pos] === 0x0d) pos++;
      if (pos < pack.length && pack[pos] === 0x0a) pos++;

      lineRec = readLine(pack, pos);
      line = lineRec.line;
      pos = lineRec.next;
      if (line !== '===END===') {
        console.error(`expected ===END=== for ${relPath}`);
        console.error('hint: bundle may be newline-converted or corrupted; copy engine-bundle.txt in binary mode and do not re-save it in an editor');
        process.exit(1);
      }
    } else if (enc === 'base64') {
      const bodyLines = [];
      while (pos < pack.length) {
        lineRec = readLine(pack, pos);
        line = lineRec.line;
        pos = lineRec.next;
        if (line === '===END===') break;
        bodyLines.push(line);
      }
      if (line !== '===END===') {
        console.error(`unexpected EOF in ${relPath}`);
        process.exit(1);
      }
      buf = Buffer.from(bodyLines.join(''), 'base64');
    } else {
      console.error(`unknown encoding '${enc}' for ${relPath}`);
      process.exit(1);
    }
    if (buf.length !== size) {
      console.error(`size mismatch for ${relPath}: declared ${size}, got ${buf.length}`);
      process.exit(1);
    }

    // Path-traversal guard.
    const target = path.resolve(destRoot, relPath);
    const rel = path.relative(destRoot, target);
    if (rel.startsWith('..') || path.isAbsolute(rel)) {
      console.error(`refusing to write outside dest: ${relPath}`);
      process.exit(1);
    }

    if (fs.existsSync(target) && !force) {
      skipped++;
      continue;
    }
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, buf);
    written++;
  }

  console.log(`unpacked ${written} files -> ${destRoot}`);
  if (skipped) console.log(`  skipped ${skipped} existing files (use --force to overwrite)`);
}

main();
