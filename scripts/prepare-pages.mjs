import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const repository = process.env.GITHUB_REPOSITORY || process.argv[2];
if (!/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(repository || '')) {
  throw new Error('Supply OWNER/REPO as an argument, or set GITHUB_REPOSITORY.');
}
const destination = resolve('_site');
await mkdir(destination, { recursive: true });
await cp('dist', destination, { recursive: true });
const html = await readFile('dist/index.html', 'utf8');
const releaseUrl = `https://github.com/${repository}/releases/latest/download/keychron_q8_iso_encoder_kafka.bin`;
const marker = 'id="firmware-download" href="releases.html"';
if (!html.includes(marker)) throw new Error('Firmware download link is missing.');
await writeFile(`${destination}/index.html`, html.replace(marker, `id="firmware-download" href="${releaseUrl}"`));
console.log(`Prepared GitHub Pages for ${repository}. Firmware downloads use GitHub Releases.`);
