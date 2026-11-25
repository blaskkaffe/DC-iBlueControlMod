// The firmware files of the iBlueControlMod GitHub repository, for the OTA page.
// The list comes from the GitHub contents API (no login: 60 requests an hour per address, one page load
// uses about 3 to 9), the files from raw.githubusercontent.com, which allows reading from a web page.
// When the folder has a SHA256SUMS.txt the download is checked against it before it is flashed.

async function getJson(url) {
    const r = await fetch(url, { headers: { 'Accept': 'application/vnd.github+json' } });

    if (r.status === 404) {
        return null; // no such branch or folder
    }
    if (!r.ok) {
        throw new Error('GitHub answered ' + r.status + ' for ' + url);
    }
    return r.json();
}

async function sumsOf(items) {
    const sums = {};
    const file = items.find((i) => i.type === 'file' && i.name === 'SHA256SUMS.txt');

    if (file) {
        const text = await (await fetch(file.download_url)).text();
        text.split('\n').forEach((line) => {
            const m = line.trim().match(/^([0-9a-fA-F]{64})\s+\*?(.+)$/);
            if (m) {
                sums[m[2]] = m[1].toLowerCase();
            }
        });
    }
    return sums;
}

async function listFolder(repo, source, path, out) {
    const items = await getJson('https://api.github.com/repos/' + repo + '/contents/' + path + '?ref=' + encodeURIComponent(source.ref));

    if (!Array.isArray(items)) {
        return;
    }
    const sums = await sumsOf(items);

    items.filter((i) => i.type === 'file' && i.name.toLowerCase().endsWith('.bin')).forEach((i) => {
        out.push({
            source: source,
            name: i.name,
            path: i.path,
            size: i.size,
            url: i.download_url,
            sha256: sums[i.name] || null,
        });
    });
    return items.filter((i) => i.type === 'dir');
}

/* All firmware files of all sources, the baseline first. Never throws: a source that fails is skipped
 * and named in `problems`. */
export async function listFirmware(sourcesUrl) {
    const cfg = await (await fetch(sourcesUrl)).json();
    const files = [];
    const problems = [];

    for (const source of cfg.sources) {
        try {
            const dirs = (await listFolder(cfg.repo, source, 'firmware', files)) || [];
            for (const d of dirs) {
                await listFolder(cfg.repo, source, d.path, files);
            }
        }
        catch (e) {
            problems.push(source.label + ': ' + e.message);
        }
    }
    files.sort((a, b) => (b.source.baseline ? 1 : 0) - (a.source.baseline ? 1 : 0));
    return { repo: cfg.repo, files: files, problems: problems };
}

function hex(buffer) {
    return Array.from(new Uint8Array(buffer)).map((b) => b.toString(16).padStart(2, '0')).join('');
}

/* Download a listed file. Returns { data: ArrayBuffer, verified: true|false } and throws when the
 * checksum is known and does not match. */
export async function fetchFirmware(entry, progress) {
    const r = await fetch(entry.url);

    if (!r.ok) {
        throw new Error('Download failed: ' + r.status);
    }
    progress && progress('Downloading ' + entry.name + '...');
    const data = await r.arrayBuffer();

    if (!entry.sha256) {
        return { data: data, verified: false };
    }
    const digest = hex(await crypto.subtle.digest('SHA-256', data));

    if (digest !== entry.sha256) {
        throw new Error('Checksum mismatch for ' + entry.name + ' (' + digest.slice(0, 12) + '... is not ' + entry.sha256.slice(0, 12) + '...)');
    }
    return { data: data, verified: true };
}
