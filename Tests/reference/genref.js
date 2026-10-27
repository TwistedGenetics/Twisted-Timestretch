// Reference vectors from the independent browser port (Akaizer.html core).
// Usage: node Tests/reference/genref.js Tests/reference
const fs = require('fs'), path = process.argv[2];
const html = fs.readFileSync(require('path').join(__dirname, '../../web/Akaizer.html'), 'utf8');
const AK = new Function('module', html.match(/<script>([\s\S]*?)<\/script>/)[1] + '; return AK;')({});
function input(words, seed) {               // integer-only, identical in C++
  const x = new Int16Array(words); let s = seed >>> 0;
  for (let i = 0; i < words; i++) {
    s = (Math.imul(s, 1103515245) + 12345) >>> 0;
    const noise = ((s >>> 16) & 0x3fff) - 0x2000;           // -8192..8191
    const saw = (((i * 37) % 2000) - 1000) * 12;              // -12000..11988
    let v = saw + noise; if (v > 32767) v = 32767; if (v < -32768) v = -32768;
    x[i] = v;
  }
  return x;
}
const cases = [
  ['a_mono_130_800_51',     1, 44100, 22050, 1, 130,  800,  51],
  ['b_stereo_75_300_137',   2, 44100, 22050, 2,  75,  300, 137],
  ['c_stereo48k_250_1500_27', 2, 48000, 24000, 3, 250, 1500,  27],
  ['d_mono22k_2000_20_2050', 1, 22050,  1000, 4, 2000,  20, 2050],
  ['e_mono_25_2000_20',     1, 44100, 30000, 5,  25, 2000,  20],
  ['f_stereo_101_33_41_odd', 2, 44100, 9000, 6, 101.37, 33, 41],
];
const lines = [];
for (const [name, ch, sr, words, seed, pct, len, freq] of cases) {
  const r = AK.stretch(input(words, seed), ch, sr, pct, len, freq);
  fs.writeFileSync(`${path}/${name}.raw`, Buffer.from(r.out.buffer));
  lines.push(`${name} ${ch} ${sr} ${words} ${seed} ${pct} ${len} ${freq} ${r.out.length}`);
}
fs.writeFileSync(`${path}/cases.txt`, lines.join('\n') + '\n');
console.log(lines.join('\n'));
