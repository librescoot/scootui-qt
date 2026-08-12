#!/usr/bin/env node
// Regenerate the SDF glyph ranges the map styles reference as
// file:///usr/share/scootui/glyphs/{fontstack}/{range}.pbf
//
//   npm install fontnik
//   node tools/make-glyphs.js assets/fonts/Roboto-Regular.ttf assets/glyphs/roboto_regular
//
// Every range is written, including the ones the font has no glyphs for. A
// range that resolves to nothing is a valid empty pbf, whereas a missing file
// is an error, and MapLibre never marks a failed glyph request as parsed: the
// tile keeps waiting on the dependency and every feature on it disappears,
// not just the label. Empty ranges are around 40 bytes each, so covering the
// whole BMP costs far less than that failure mode.

const fontnik = require('fontnik');
const fs = require('fs');
const path = require('path');

const src = process.argv[2];
const outdir = process.argv[3];
if (!src || !outdir) {
    console.error('usage: make-glyphs.js <font.ttf> <outdir>');
    process.exit(1);
}

fs.mkdirSync(outdir, { recursive: true });
const font = fs.readFileSync(src);

let done = 0, bytes = 0, withGlyphs = 0;
const total = 256;
for (let i = 0; i < total; i++) {
    const start = i * 256;
    const end = start + 255;
    fontnik.range({ font, start, end }, (err, res) => {
        if (err) {
            console.error('range', start, err.message);
            process.exit(1);
        }
        fs.writeFileSync(path.join(outdir, `${start}-${end}.pbf`), res);
        bytes += res.length;
        if (res.length > 100) withGlyphs++;
        if (++done === total) {
            console.log(`wrote ${total} ranges (${withGlyphs} carrying glyphs), ` +
                        `${(bytes / 1024).toFixed(1)} KB into ${outdir}`);
        }
    });
}
