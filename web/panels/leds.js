// LEDs: the controller LEDs and the boot animation (the water).
import { getAnimCfg, setAnimCfg, resetAnimCfg, playAnim } from '../utils/iblueCmd.js';
import { ibluAnimFields, ibluAnimDefaults, ibluAnimClamp, ibluAnimLevel, ibluAnimTank } from '../utils/ibluAnim.js';
import { ibluProfileResult } from '../utils/constants.js';
import { el, card, say, page } from './ui.js';

let ctx;
let status;
let tankCanvas;
let curveCanvas;

export const meta = { id: 'leds', title: 'LEDs', icon: 'light', needsIblue: true,
    text: 'The four port LEDs: how bright a connected controller is, and the boot animation they play (fireworks, then drops on water).' };

const groups = [
    { title: 'Port LEDs', sub: null, keys: ['ctrl_pct'] },
    { title: 'Boot animation', sub: 'The fireworks take 3 s at the start; the drops fill the rest.', keys: ['total_s'] },
    { title: 'The water', sub: 'The four LEDs sit in a tank of water; a drop makes ripples that bounce off the walls.',
      keys: ['led_dist', 'tank_len', 'group_pos', 'density', 'depth'], plot: 'tank' },
    { title: 'Water level curve', sub: 'The general brightness of the water over the drops: it starts low, rises, and goes down to nothing.',
      keys: ['level_low', 'level_high', 'rise_at', 'rise_len', 'fade_at'], plot: 'curve' },
];

const $ = (id) => document.getElementById(id);

function values() {
    return ibluAnimFields.map((f) => Number($('led_' + f.key + '_n').value));
}

function row(f) {
    const range = el('input', { type: 'range', id: 'led_' + f.key, min: f.min, max: f.max, step: f.step, value: f.def, 'aria-label': f.label });
    const num = el('input', { type: 'number', id: 'led_' + f.key + '_n', min: f.min, max: f.max, step: f.step, value: f.def, 'aria-label': f.label + ' (number)' });

    range.addEventListener('input', () => { num.value = range.value; draw(); });
    num.addEventListener('input', () => { range.value = num.value; draw(); });
    return el('div', { class: 'row' },
        el('label', { for: 'led_' + f.key + '_n', text: f.label }),
        range,
        el('div', { class: 'val' }, num, el('span', { class: 'unit', text: f.unit })),
        f.hint ? el('p', { class: 'hint', text: f.hint }) : null);
}

export function init(root, c) {
    ctx = c;
    status = el('span', { class: 'status', id: 'ledStatus' });
    tankCanvas = el('canvas', { class: 'plot', id: 'ledTank', width: 640, height: 90, 'aria-label': 'The tank from above with the four LEDs' });
    curveCanvas = el('canvas', { class: 'plot', id: 'ledCurve', width: 640, height: 160, 'aria-label': 'Water level curve' });
    root.append(page(meta.title, meta.text));
    for (const g of groups) {
        const k = g.keys.map((key) => row(ibluAnimFields.find((f) => f.key === key)));
        root.append(card(g.title, g.sub, k,
            g.plot === 'tank' ? [tankCanvas, el('small', { text: 'The tank from above: the walls, and where the four LEDs (A to D) sit in the water.' })] : null,
            g.plot === 'curve' ? [curveCanvas, el('small', { text: 'Brightness of the water over the drops (the curve above, times the depth).' })] : null));
    }
    root.append(card('Save', 'Save and play runs the whole animation on the adapter right away, so you can see the change.',
        el('div', { class: 'actions' },
            el('button', { class: 'btn primary', id: 'btnLedSave', text: 'Save and play', onClick: save }),
            el('button', { class: 'btn', id: 'btnLedDefaults', text: 'Defaults', onClick: defaults }),
            status)));
    draw();
}

function setValues(v) {
    ibluAnimFields.forEach((f, i) => {
        $('led_' + f.key).value = v[i];
        $('led_' + f.key + '_n').value = v[i];
    });
    draw();
}

function css(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim() || '#888';
}

function draw() {
    const v = ibluAnimClamp(values());
    const geo = ibluAnimTank(v);
    const t = tankCanvas.getContext('2d');
    const w = tankCanvas.width;
    const h = tankCanvas.height;
    const pad = 30;
    const scale = (w - 2 * pad) / geo.width;
    const accent = css('--accent');
    const text = css('--text');
    const muted = css('--muted');

    t.clearRect(0, 0, w, h);
    t.fillStyle = css('--accent-soft');
    t.fillRect(pad, 16, w - 2 * pad, h - 32);
    t.strokeStyle = muted;
    t.lineWidth = 3;
    t.beginPath();
    t.moveTo(pad, 8); t.lineTo(pad, h - 8);
    t.moveTo(w - pad, 8); t.lineTo(w - pad, h - 8);
    t.stroke();
    geo.leds.forEach((x, i) => {
        t.fillStyle = accent;
        t.beginPath();
        t.arc(pad + x * scale, h / 2, 10, 0, 2 * Math.PI);
        t.fill();
        t.fillStyle = css('--accent-text');
        t.font = '600 12px system-ui, sans-serif';
        t.textAlign = 'center';
        t.fillText('ABCD'[i], pad + x * scale, h / 2 + 4);
    });

    const g = curveCanvas.getContext('2d');
    const cw = curveCanvas.width;
    const ch = curveCanvas.height;

    g.clearRect(0, 0, cw, ch);
    g.strokeStyle = css('--border');
    g.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
        g.beginPath();
        g.moveTo(34, 10 + (ch - 30) * i / 4); g.lineTo(cw - 10, 10 + (ch - 30) * i / 4);
        g.stroke();
    }
    g.strokeStyle = accent;
    g.lineWidth = 2.5;
    g.beginPath();
    for (let i = 0; i <= 200; i++) {
        const u = i / 200;
        const x = 34 + u * (cw - 44);
        const y = 10 + (ch - 30) * (1 - ibluAnimLevel(v, u));
        if (i === 0) { g.moveTo(x, y); } else { g.lineTo(x, y); }
    }
    g.stroke();
    g.fillStyle = muted;
    g.font = '11px system-ui, sans-serif';
    g.textAlign = 'left';
    g.fillText('100 %', 0, 18);
    g.fillText('0 %', 8, ch - 18);
    g.textAlign = 'center';
    g.fillText('start of the drops', 90, ch - 4);
    g.fillText('end', cw - 22, ch - 4);
    void text;
}

async function save() {
    try {
        const result = await setAnimCfg(ctx.svc(), ibluAnimClamp(values()));

        if (result !== 0) {
            say(status, ibluProfileResult[result] || 'Not saved.', 'bad');
            return;
        }
        setValues(await getAnimCfg(ctx.svc())); /* what the adapter kept */
        await playAnim(ctx.svc());
        say(status, 'Saved. Playing the animation.', 'ok');
        ctx.toast('Saved. Playing the animation.');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

async function defaults() {
    try {
        const result = await resetAnimCfg(ctx.svc());

        setValues(result === 0 ? await getAnimCfg(ctx.svc()) : ibluAnimDefaults());
        say(status, result === 0 ? 'Back to the defaults (saved).' : 'Not saved.', result === 0 ? 'ok' : 'bad');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

export async function connected() {
    if (!ctx.conn.info || !ctx.conn.info.animation) { return; }
    setValues(await getAnimCfg(ctx.svc()));
    say(status, '');
}

export function disconnected() {
    say(status, '');
}

/* the colours of the page changed (light / dark) */
export function redraw() {
    if (tankCanvas) { draw(); }
}
