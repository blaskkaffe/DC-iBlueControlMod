// Drives the built site (default /tmp/site) in Chromium with a mocked BlueRetro and mocked GitHub.
// NODE_PATH=/opt/node22/lib/node_modules node web/test/web_test.js [site dir]
const path = require('path');
const fs = require('fs');
const http = require('http');
const crypto = require('crypto');
const { chromium } = require('playwright');

const site = path.resolve(process.argv[2] || '/tmp/site');
let fails = 0, checks = 0;
async function go(page, id) {
    await page.click('#nav-' + id);
    await page.waitForSelector('#p-' + id, { state: 'visible' });
}

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
    const mo = await import(path.join(site, 'utils/ibluMotion.js'));
    opts = Object.assign({ motionDefaults: mo.ibluMotionDefaults(), motionMin: mo.ibluMotionFields.map((f) => f.min), motionMax: mo.ibluMotionFields.map((f) => f.max) }, opts);
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

    // ---- the app with an iBlueControlMod firmware: one connection, all categories
    {
        const { page, errors } = await open(browser, server, 'index.html', iblue);
        check((await page.$$('#nav > a')).length === 6, 'six categories in the menu');
        check(await page.isVisible('#p-device .gate'), 'before connecting, a prompt to connect');
        await page.click('#btConn');
        await page.waitForFunction(() => document.getElementById('connChip').classList.contains('ok'));
        const info = await page.textContent('#deviceInfo');
        check(/tap buttons, game id, Joy-Con motion/.test(info), 'the device card shows the features: ' + info);
        check(await page.evaluate(() => window.__requests) === 1, 'one request for a device');
        await page.click('#btnPortCheck');
        await page.waitForFunction(() => !document.getElementById('portCheckOut').hidden, null, { timeout: 10000 });
        const pc = await page.textContent('#portCheckOut');
        check(pc === 'iBlue port check 1\nport D: adapter quiet, sense GPIO33=0 (plugged)\n'.repeat(3), 'the port check report is read whole: ' + JSON.stringify(pc.slice(0, 60)));
        await go(page, 'buttons');
        await page.waitForSelector('#gesture0', { state: 'visible' });
        check(await page.inputValue('#gesture0') === '1' && await page.inputValue('#gesture1') === '3' && await page.inputValue('#gesture2') === '0',
            'defaults: single tap opens pairing, double tap next colour, triple tap nothing');
        check((await page.$$('#gesture0 option')).length === 6, 'six actions to choose from');
        await page.selectOption('#gesture2', '5');
        await page.selectOption('#gesture0', '2');
        await page.click('#btnGestureSave');
        await page.waitForFunction(() => document.getElementById('gestureStatus').textContent === 'Saved.');
        check(JSON.stringify(await page.evaluate(() => window.__br.gestures)) === '[2,3,5]', 'the firmware got the new tap actions');

        check(errors.length === 0, 'no script errors on the iBlue page: ' + errors.join('; '));
        await page.context().close();
    }

    {   // the motion table of the page is the one of the firmware
        const mo = await import(path.join(site, 'utils/ibluMotion.js'));
        const cands = [path.join(__dirname, '../../main/iblue/iblue_motion.c'), '/home/user/iBlueControlMod/main/iblue/iblue_motion.c'];
        const src = cands.find((f) => fs.existsSync(f));
        if (src) {
            const text = fs.readFileSync(src, 'utf8');
            const grab = (name) => text.match(new RegExp(name + '\\s*=\\s*\\{([^}]*)\\}'))[1].split(',').map((x) => parseInt(x.trim(), 10));
            check(grab('iblue_motion_cfg_default').join() === mo.ibluMotionDefaults().join(), 'motion defaults equal the firmware');
            check(grab('iblue_motion_cfg_min').join() === mo.ibluMotionFields.map((f) => f.min).join(), 'motion minimums equal the firmware');
            check(grab('iblue_motion_cfg_max').join() === mo.ibluMotionFields.map((f) => f.max).join(), 'motion maximums equal the firmware');
        }
    }
    // ---- the same page with some other BlueRetro firmware
    {
        const { page, errors } = await open(browser, server, 'index.html', { iblue: false, fwName: 'BlueRetro_hw1_dreamcast', fwVer: 'v25.10' });
        await page.click('#btConn');
        await page.waitForFunction(() => document.getElementById('connChip').classList.contains('ok'));
        await go(page, 'buttons');
        check(await page.isVisible('#p-buttons .nope') && await page.isHidden('#gesture0'), 'another firmware: a note instead of the iBlue settings');
        await go(page, 'controllers');
        check(await page.isVisible('#dcPreset0'), 'the controller presets still work on another firmware');
        check(/none/.test(await page.textContent('#deviceInfo')), 'the device card says it has no iBlue features');
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
        await page.click('#btConn');
        await page.waitForFunction(() => document.getElementById('connChip').classList.contains('ok'));
        for (const id of ['device', 'buttons', 'controllers', 'games', 'update', 'advanced']) {
            await go(page, id);
            check(await page.isVisible('#p-' + id), 'category ' + id + ' opens');
        }
        await page.context().close();
    }

    // ---- Dreamcast only setting (on by default), in the device category
    for (const off of [false, true]) {
        const { page } = await open(browser, server, 'index.html', iblue);
        if (off) { await page.evaluate(() => localStorage.setItem('iblueDcOnly', '0')); await page.reload(); }
        await page.click('#btConn');
        await page.waitForFunction(() => document.getElementById('connChip').classList.contains('ok'));
        await go(page, 'advanced');
        const hidden = await page.isHidden('#tab-advanced-4');
        check(hidden === !off, 'N64 tab ' + (off ? 'shown' : 'hidden') + ' when Dreamcast only is ' + (off ? 'off' : 'on'));
        await go(page, 'device');
        check(await page.isChecked('#dcOnly') === !off, 'the checkbox shows the setting');
        if (!off) {
            await page.uncheck('#dcOnly');
            check(await page.evaluate(() => localStorage.getItem('iblueDcOnly')) === '0', 'unchecking keeps the setting');
            await go(page, 'advanced');
            check(await page.isVisible('#tab-advanced-4'), 'and shows the other consoles again');
        }
        await page.context().close();
    }
    for (const off of [false, true]) {
        const { page, errors } = await open(browser, server, 'advance.html', iblue);
        if (off) { await page.evaluate(() => localStorage.setItem('iblueDcOnly', '0')); await page.reload(); }
        await page.click('#btConn');
        await page.waitForSelector('#systemCfg', { state: 'attached' });
        const sys = await page.$$eval('#systemCfg option', (o) => o.map((x) => x.text));
        const dst = await page.$$eval('#dstLabel option', (o) => o.map((x) => x.text));
        check(off ? sys.length > 10 && dst.length > 10 : (sys.join() === 'Auto,DC' && dst.join() === 'Default,Dreamcast'),
            'advance page system / destination lists with Dreamcast only ' + (off ? 'off' : 'on') + ': ' + sys.length + ' / ' + dst.length);
        await page.context().close();
    }

    // ---- Dreamcast controller presets
    {
        const { page, errors } = await open(browser, server, 'index.html', iblue);
        await page.click('#btConn');
        await go(page, 'controllers');
        await page.waitForSelector('#dcPreset0', { state: 'visible' });
        check((await page.$$('#dcPresetRows button')).length === 7, 'seven presets');
        check(await page.evaluate(() => document.getElementById('dcTile0').classList.contains('on') && document.getElementById('dcPreset0').textContent === 'Selected'), 'the standard controller is shown as selected at first');
        await page.selectOption('#dcOutput', '1');
        await page.click('#dcPreset2'); // racing
        await page.waitForFunction(() => /is set on output 2/.test(document.getElementById('dcPresetStatus').textContent));
        check(await page.evaluate(() => document.getElementById('dcTile2').classList.contains('on') && !document.getElementById('dcTile0').classList.contains('on')), 'racing is shown as selected after Apply');
        await page.selectOption('#dcOutput', '0');
        check(await page.evaluate(() => document.getElementById('dcTile0').classList.contains('on')), 'port A still shows the standard controller');
        await page.selectOption('#dcOutput', '1');
        const w = await page.evaluate(() => window.__br.writes);
        check(w.some((x) => x[0] === 3 && x[1][0] === 4 && x[1][1] === 3), 'racing mode (4) and rumble + memory written as the output data');
        check(w.some((x) => x[0] === 2 && x[1][0] === 1), 'output 2 selected first');
        check(w.some((x) => x[0] === 4 || x[0] === 5), 'the default mapping was written');
        await page.evaluate(() => { window.__br.writes.length = 0; });
        await page.click('#dcPreset5'); // keyboard: mode only
        await page.waitForFunction(() => /Keyboard/.test(document.getElementById('dcPresetStatus').textContent));
        const w2 = await page.evaluate(() => window.__br.writes);
        check(w2.every((x) => x[0] === 2 || x[0] === 3) && w2.some((x) => x[0] === 3 && x[1][0] === 2), 'keyboard: only the output mode is written');
        await page.evaluate(() => { window.__br.writes.length = 0; });
        await page.click('#dcPreset3'); // maracas
        await page.waitForFunction(() => /Samba de Amigo maracas is set/.test(document.getElementById('dcPresetStatus').textContent));
        const w3 = await page.evaluate(() => window.__br.writes);
        check(w3.some((x) => x[0] === 3 && x[1][0] === 5 && x[1][1] === 3), 'maracas mode (5) written with rumble and memory');
        check(w3.some((x) => x[0] === 4 || x[0] === 5), 'the maracas mapping was written');
        // the Joy-Con motion preset: two inputs, the right Joy-Con into the first output
        await page.evaluate(() => { window.__br.writes.length = 0; });
        await page.selectOption('#dcOutput', '0');
        await page.click('#dcPreset4');
        await page.waitForFunction(() => /Joy-Con motion\) is set/.test(document.getElementById('dcPresetStatus').textContent));
        const w4 = await page.evaluate(() => window.__br.writes);
        check(w4.some((x) => x[0] === 3 && x[1][0] === 5), 'motion preset: maracas mode written');
        check(await page.evaluate(() => document.getElementById('dcTile4').classList.contains('on') && !document.getElementById('dcTile3').classList.contains('on')), 'the Joy-Con preset (not the stick one) is shown as selected on port A');
        check(await page.evaluate(() => document.querySelectorAll('#dcTile4 ol.steps li').length === 3), 'the Joy-Con preset has three numbered steps');
        const inputs = w4.filter((x) => x[0] === 4 || x[0] === 5);
        check(inputs.length >= 2, 'motion preset: two inputs were written');
        // motion settings and the calibration poses
        check(await page.isVisible('#motionCards'), 'the Joy-Con motion cards are shown');
        check(await page.isChecked('#mo_mode') === false, 'motion mode is off by default');
        await page.check('#mo_mode');
        await page.fill('#mo_shake_mg_n', '9999'); // beyond the range: the firmware clamps
        await page.click('#btnMoSave');
        await page.waitForFunction(() => /Saved/.test(document.getElementById('moStatus').textContent));
        const mcfg = await page.evaluate(() => window.__br.motion);
        check(mcfg[0] === 1 && mcfg[4] === 4000, 'motion settings saved and clamped: ' + mcfg);
        await page.click('#btnMoDefaults');
        check(await page.inputValue('#mo_shake_mg_n') === '1200' && !(await page.isChecked('#mo_mode')), 'defaults put the form back');
        await page.click('#moPose1');
        await page.waitForFunction(() => window.__br.poses.length === 1);
        check(JSON.stringify(await page.evaluate(() => window.__br.poses[0])) === '[1,5]', 'the up pose was asked for with 5 s delay');
        await page.waitForFunction(() => /Left Joy-Con: height 17/.test(document.getElementById('moLive').textContent), null, { timeout: 8000 });
        const liveText = await page.textContent('#moLive');
        check(/Right Joy-Con.*SHAKE/.test(liveText) && /X 138 Y 148/.test(liveText), 'live view shows both Joy-Cons: ' + liveText);
        await page.click('#btnMoForget');
        await page.waitForFunction(() => window.__br.motionReset === 1);
        await go(page, 'games');
        await page.waitForFunction(() => /No game announced/.test(document.getElementById('dcGameText').textContent));
        check(await page.isDisabled('#btnDcGameSave'), 'no game id, nothing to save for a game');
        check(errors.length === 0, 'no script errors on the Dreamcast presets page: ' + errors.join('; '));
        await page.context().close();
    }

    // ---- one connection for the BlueRetro pages shown inside the app
    {
        const { page, errors } = await open(browser, server, 'index.html', iblue);
        await page.click('#btConn');
        await page.waitForFunction(() => document.getElementById('connChip').classList.contains('ok'));
        await go(page, 'advanced');
        await page.waitForSelector('#frame-advanced-0');
        const frame = page.frames().find((f) => f.url().includes('advance.html'));
        check(!!frame, 'the mapping page opened inside the app');
        await frame.waitForSelector('#systemCfg', { state: 'attached' });
        check(await frame.evaluate(() => window.__requests || 0) === 0, 'the page inside did not ask for a device of its own');
        check(await page.evaluate(() => window.__requests) === 1, 'still the one request of the app');
        await page.click('#tab-advanced-2'); // files
        await page.waitForSelector('#frame-advanced-2');
        check(page.frames().some((f) => f.url().includes('files.html')), 'a second tool opens on the same connection');
        await go(page, 'update');
        await page.waitForSelector('#frame-update-0');
        const ota = page.frames().find((f) => f.url().includes('ota.html'));
        await ota.waitForFunction(() => document.querySelectorAll('#fwGithub option').length > 1 || /No firmware|Could not/.test(document.getElementById('fwGithub').textContent), null, { timeout: 15000 });
        check(await page.evaluate(() => window.__requests) === 1, 'the update page used the same connection');
        check(errors.length === 0, 'no script errors in the app: ' + errors.join('; '));
        await page.context().close();
    }

    await browser.close();
    server.close();
    console.log(checks + ' checks, ' + fails + ' failed');
    process.exit(fails ? 1 : 0);
})();
