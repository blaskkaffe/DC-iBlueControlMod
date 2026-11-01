// Drives the built site (default /tmp/site) in Chromium with a mocked BlueRetro and mocked GitHub.
// NODE_PATH=/opt/node22/lib/node_modules node web/test/web_test.js [site dir]
const path = require('path');
const fs = require('fs');
const http = require('http');
const crypto = require('crypto');
const { chromium } = require('playwright');

const site = path.resolve(process.argv[2] || '/tmp/site');
let fails = 0, checks = 0;
function check(c, what) { checks++; if (!c) { fails++; console.log('FAIL: ' + what); } }

function serve() {
    const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.css': 'text/css', '.png': 'image/png' };
    return new Promise((resolve) => {
        const s = http.createServer((req, res) => {
            const p = path.join(site, decodeURIComponent(req.url.split('?')[0]));
            fs.readFile(p, (e, d) => {
                if (e) { res.writeHead(404); res.end(); return; }
                res.writeHead(200, { 'Content-Type': types[path.extname(p)] || 'application/octet-stream' });
                res.end(d);
            });
        }).listen(0, () => resolve(s));
    });
}

// the GitHub of the test: a baseline and a test firmware, one with a good checksum, one with a bad one
const fw = {
    baseline: Buffer.concat([Buffer.from('BlueRetro_dreamcast_iblue v25 '), crypto.randomBytes(3000)]),
    test: Buffer.concat([Buffer.from('BlueRetro_dreamcast_iblue test '), crypto.randomBytes(5000)]),
    hw2: Buffer.concat([Buffer.from('BlueRetro_hw2_dreamcast '), crypto.randomBytes(2000)]),
};
const sha = (b) => crypto.createHash('sha256').update(b).digest('hex');
const API = 'https://api.github.com/repos/blaskkaffe/iBlueControlMod/contents/';
const RAW = 'https://raw.githubusercontent.com/blaskkaffe/iBlueControlMod/';

async function setupGithub(page, opts) {
    const dir = (ref, files) => files.map((f) => ({ type: 'file', name: f.name, path: 'firmware/' + f.name, size: f.data.length, download_url: RAW + ref + '/firmware/' + f.name }));
    const lists = {
        'main': [{ name: 'BlueRetro_iBlueControlMod_RGB_v1.bin', data: fw.baseline }, { name: 'SHA256SUMS.txt', data: Buffer.from(sha(fw.baseline) + '  BlueRetro_iBlueControlMod_RGB_v1.bin\n') }],
        'claude/test-fw': [{ name: 'test1.bin', data: fw.test }, { name: 'hw2.bin', data: fw.hw2 },
            { name: 'SHA256SUMS.txt', data: Buffer.from(sha(opts.badSum ? fw.baseline : fw.test) + '  test1.bin\n') }],
    };
    await page.route('https://api.github.com/**', async (route) => {
        const u = new URL(route.request().url());
        if (u.pathname.includes('/releases')) { return route.fulfill({ json: [{ tag_name: 'v25.10' }] }); }
        const ref = u.searchParams.get('ref');
        if (u.pathname.endsWith('/contents/firmware') && lists[ref]) { return route.fulfill({ json: dir(ref, lists[ref]) }); }
        return route.fulfill({ status: 404, json: { message: 'Not Found' } });
    });
    await page.route(RAW + '**', async (route) => {
        const m = route.request().url().slice(RAW.length).match(/^(.+)\/firmware\/(.+)$/);
        const f = m && lists[m[1]] && lists[m[1]].find((x) => x.name === m[2]);
        if (!f) { return route.fulfill({ status: 404 }); }
        return route.fulfill({ body: f.data, headers: { 'access-control-allow-origin': '*' } });
    });
}

async function open(browser, server, url, opts) {
    const consts = await import(path.join(site, 'utils/constants.js'));
    const context = await browser.newContext();
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (e) => { errors.push(String(e)); console.log('PAGEERROR', e); });
    page.on('console', (m) => console.log('CONSOLE', m.text()));
    page.on('dialog', (d) => d.accept());
    await page.addInitScript('window.__uuids = ' + JSON.stringify(consts.brUuid) + '; window.__opts = ' + JSON.stringify(opts) + ';\n' +
        fs.readFileSync(path.join(__dirname, 'mock_bluetooth.js'), 'utf8'));
    await setupGithub(page, opts);
    await page.goto('http://localhost:' + server.address().port + '/' + url);
    return { page, errors };
}

(async () => {
    const server = await serve();
    const browser = await chromium.launch();
    const iblue = { iblue: true, fwName: 'BlueRetro_dreamcast_iblue', fwVer: 'v25.10-beta-test' };

    // ---- the iBlueControlMod page with an iBlueControlMod firmware
    {
        const { page, errors } = await open(browser, server, 'iblue.html', iblue);
        await page.click('#btConn');
        await page.waitForSelector('#divGestures', { state: 'visible' });
        const info = await page.textContent('#divInfo');
        check(/iBlueControlMod features: tap buttons, profiles, game id/.test(info), 'info line shows the features: ' + info);
        check(await page.inputValue('#gesture0') === '0' && await page.inputValue('#gesture1') === '3' && await page.inputValue('#gesture2') === '0',
            'defaults: single tap nothing, double tap next colour, triple tap nothing');
        check((await page.$$('#gesture0 option')).length === 10, 'ten actions to choose from');
        await page.selectOption('#gesture2', '6');
        await page.selectOption('#gesture0', '1');
        await page.click('#btnGestureSave');
        await page.waitForFunction(() => document.getElementById('gestureStatus').textContent === 'Saved.');
        check(JSON.stringify(await page.evaluate(() => window.__br.gestures)) === '[1,3,6]', 'the firmware got the new tap actions');

        await page.waitForSelector('#divProfiles', { state: 'visible' });
        check((await page.$$('#profileRows > div')).length === 4, 'four profile slots');
        check(await page.isDisabled('#profileLoad0') && await page.isDisabled('#profileDelete0'), 'an empty slot cannot be loaded or deleted');
        await page.fill('#profileName1', 'Fighting');
        await page.click('#profileSave1');
        await page.waitForFunction(() => !document.getElementById('profileLoad1').disabled);
        check(await page.evaluate(() => window.__br.profiles[1]) === 'Fighting', 'profile saved with its name');
        check(await page.inputValue('#profileName1') === 'Fighting', 'the name is shown again after a refresh');
        await page.click('#profileSave2'); // empty name: the firmware names it
        await page.waitForFunction(() => !document.getElementById('profileLoad2').disabled);
        check(await page.inputValue('#profileName2') === 'Profile 3', 'an empty name becomes Profile 3');
        await page.click('#profileLoad1');
        await page.waitForFunction(() => document.getElementById('profileStatus').textContent.startsWith('Done: load'));
        check(await page.evaluate(() => window.__br.loaded) === 1, 'profile 2 was loaded');
        await page.click('#profileDelete1');
        await page.waitForFunction(() => document.getElementById('profileLoad1').disabled);
        check(await page.evaluate(() => window.__br.profiles[1]) === null, 'profile deleted');
        check(errors.length === 0, 'no script errors on the iBlue page: ' + errors.join('; '));
        await page.context().close();
    }

    // ---- the same page with some other BlueRetro firmware
    {
        const { page, errors } = await open(browser, server, 'iblue.html', { iblue: false, fwName: 'BlueRetro_hw1_dreamcast', fwVer: 'v25.10' });
        await page.click('#btConn');
        await page.waitForSelector('#divInfo', { state: 'visible' });
        await page.waitForFunction(() => /does not have/.test(document.getElementById('divInfo').textContent));
        check(await page.isHidden('#divGestures') && await page.isHidden('#divProfiles'), 'no iBlue sections for another firmware');
        check(errors.length === 0, 'no script errors with another firmware: ' + errors.join('; '));
        await page.context().close();
    }

    // ---- OTA: the firmware list from GitHub
    async function ota(optsServer, pick) {
        const { page, errors } = await open(browser, server, 'ota.html', Object.assign({}, iblue, optsServer));
        await page.click('#btConn');
        await page.waitForSelector('#divFwSelect', { state: 'visible' });
        await page.waitForFunction(() => document.querySelectorAll('#fwGithub option').length > 1 || /No firmware|Could not/.test(document.getElementById('fwGithub').textContent));
        return { page, errors };
    }
    {
        const { page, errors } = await ota({}, 0);
        const options = await page.$$eval('#fwGithub option', (o) => o.map((x) => x.text));
        check(options.length === 3, 'three firmware files listed, got ' + JSON.stringify(options));
        check(/^Baseline \(main, tested on hardware\): BlueRetro_iBlueControlMod_RGB_v1\.bin/.test(options[0]) && !/no checksum/.test(options[0]),
            'the baseline is first and has a checksum: ' + options[0]);
        check(/Test firmware: test1\.bin/.test(options[1]) && /Test firmware: hw2\.bin .*no checksum/.test(options[2]), 'the test firmware follows: ' + JSON.stringify(options));
        check(await page.inputValue('#fwGithub') === '0', 'the baseline is selected');

        // flash the baseline
        await page.click('#btnFwUpdateGithub');
        await page.waitForFunction(() => window.__br.ota.ended);
        const got = await page.evaluate(() => window.__br.ota);
        check(got.started && got.ended && !got.aborted, 'OTA started and ended');
        check(Buffer.from(got.bytes).equals(fw.baseline), 'the bytes the adapter got are the downloaded file (' + got.bytes.length + ' of ' + fw.baseline.length + ')');
        check(got.chunks === Math.ceil(fw.baseline.length / 244), 'written in chunks of 244 bytes: ' + got.chunks);
        check(errors.length === 0, 'no script errors on the OTA page: ' + errors.join('; '));
        await page.context().close();
    }
    {   // a download that does not match its checksum is not flashed
        const { page } = await ota({ badSum: true }, 1);
        await page.selectOption('#fwGithub', '1');
        await page.click('#btnFwUpdateGithub');
        await page.waitForFunction(() => /Checksum mismatch/.test(document.getElementById('log').textContent));
        check(!(await page.evaluate(() => window.__br.ota.started)), 'a checksum mismatch is refused before anything is sent');
        await page.context().close();
    }
    {   // no checksum: asks first; an image tagged hw2 does not go to a firmware that is not
        const { page } = await ota({}, 2);
        await page.selectOption('#fwGithub', '2');
        await page.click('#btnFwUpdateGithub');
        await page.waitForFunction(() => /Hardware and firmware mismatch/.test(document.getElementById('log').textContent));
        check(!(await page.evaluate(() => window.__br.ota.started)), 'hw2 image refused for a firmware that is not hw2');
        await page.context().close();
    }
    {   // the list still shows when one source is missing: only main has files here (the others answer 404)
        const { page } = await open(browser, server, 'index.html', iblue);
        const text = await page.textContent('body');
        check(/iblue\.html/.test(await page.content()) && /tap buttons and profiles/.test(text), 'the index links to the iBlueControlMod page');
        await page.context().close();
    }

    await browser.close();
    server.close();
    console.log(checks + ' checks, ' + fails + ' failed');
    process.exit(fails ? 1 : 0);
})();
