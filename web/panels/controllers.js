// Controllers: the type the Dreamcast sees on each port, with the button mapping for it, in one click.
import { devCfg } from '../utils/constants.js';
import { saveOutputCfg } from '../utils/saveOutputCfg.js';
import { savePresetInput } from '../utils/savePresetInput.js';
import { el, card, say, page } from './ui.js';
import * as motion from './motion.js';
import { getPortStatus } from '../utils/iblueCmd.js';

let ctx;
let status;
let defaultMap = null;

export const meta = { id: 'controllers', title: 'Controllers', icon: 'gamepad',
    text: 'Pick what the Dreamcast sees on a port. The mapping is BlueRetro\'s default one: L1 = Z, R1 = C, L2 and R2 = the triggers, the left stick is the stick and the right stick is the right stick (on types that have one). L3 and R3 do nothing and can be mapped under Advanced.' };

const ACC_BOTH = 3; /* rumble and memory card */

/* mode: index into devCfg; map: write the default mapping too (false: keep the mapping the output has) */
export const dcPresets = [
    { name: 'Standard controller', mode: 0, map: true,
      text: 'The normal Dreamcast pad: A, B, X, Y, Start, d-pad, one stick and the two analog triggers. Rumble and memory card on.' },
    { name: '6 button + dual stick', mode: 1, map: true,
      text: 'Adds C, Z, D, a second d-pad and a right stick. Some games read what the pad has and misbehave with this type: use it for the games that need it, saved for that game (Games).' },
    { name: 'Racing controller', mode: 4, map: true,
      text: 'The Dreamcast racing controller: A, B, Start, d-pad, steering (left stick), accelerator (R2) and brake (L2). No X and Y. Not tried on a console yet.' },
    { name: 'Samba de Amigo maracas', mode: 5, map: 'maracas',
      text: "For a pad with two sticks: each stick is one maraca (stick up is high). L1 / R1 shake left / right, R2 and Start are Start. Use port A or B. For Joy-Cons use the next one instead. Not tried with the game yet." },
    { name: 'Samba de Amigo (Joy-Con motion)', mode: 5, map: 'motion',
      text: 'Two Joy-Cons are the two maracas on one port. Tilt is the height, a shake is the shake.',
      steps: ['Switch on "Joy-Con motion" below, then connect both Joy-Cons (the left one first).',
          'Choose Port A above and press Apply here. Port B is then used by the right Joy-Con: leave it alone.',
          'Calibrate below: hold each Joy-Con still in the pose the page asks for.'] },
    { name: 'Keyboard', mode: 2, map: false, text: 'A Dreamcast keyboard. The mapping of the output is kept.' },
    { name: 'Mouse', mode: 3, map: false, text: 'A Dreamcast mouse. The mapping of the output is kept.' },
];

const $ = (id) => document.getElementById(id);

/* The maracas of Samba de Amigo (docs/samba-de-amigo.md): per maraca two axes (the two sticks), a shake (B left, A
 * right), a button (C left, Start right). The default entries for the sticks and the system combinations are kept. */
function maracasMap(def) {
    const entry = (src, dst) => [src, dst, 0, 100, 50, 135, 0, 0, 0];

    /* def[0..7]: LX left, LX right, LY down, LY up, RX left, RX right, RY down, RY up. X is as on a pad (left 0, right 255).
     * Y is the height: the maracas report 255 at the top, a pad reports 0 for up, so up and down swap places. */
    const turn = (e, to) => [e[0], to, ...e.slice(2)];

    return [
        def[0], def[1], def[4], def[5],            /* the X axes as they are */
        turn(def[2], 'PAD_LY_UP'), turn(def[3], 'PAD_LY_DOWN'),
        turn(def[6], 'PAD_RY_UP'), turn(def[7], 'PAD_RY_DOWN'),
        entry('PAD_LS', 'PAD_RB_RIGHT'),          /* L1 = shake left (B) */
        entry('PAD_RS', 'PAD_RB_DOWN'),           /* R1 = shake right (A) */
        entry('PAD_LM', 'PAD_RS'),                /* L2 = button C of the left maraca */
        entry('PAD_RM', 'PAD_MM'),                /* R2 = Start of the right maraca */
        entry('PAD_MM', 'PAD_MM'),                /* Start = Start */
        ...def.filter((e) => e[1].startsWith('CB_')),
    ];
}

/* The Joy-Cons as maracas (docs/samba-de-amigo.md): in motion mode a Joy-Con's right stick is its motion (X, Y) and its
 * left trigger button is its shake. The left Joy-Con (first input) fills the left maraca, the right one (second input,
 * destination -1) the right maraca. Y is turned around as in maracasMap: up is 255. */
function motionMaps(def) {
    const e = (src, dst, id) => [src, dst, id, 100, 50, 135, 0, 0, 0];
    const left = [
        e('PAD_RX_LEFT', 'PAD_LX_LEFT', 0), e('PAD_RX_RIGHT', 'PAD_LX_RIGHT', 0),
        e('PAD_RY_UP', 'PAD_LY_DOWN', 0), e('PAD_RY_DOWN', 'PAD_LY_UP', 0),
        e('PAD_LT', 'PAD_RB_RIGHT', 0),            /* shake = B */
        e('PAD_LM', 'PAD_RS', 0),                  /* ZL = C */
        ...def.filter((x) => x[1].startsWith('CB_')),
    ];
    const right = [
        e('PAD_RX_LEFT', 'PAD_RX_LEFT', -1), e('PAD_RX_RIGHT', 'PAD_RX_RIGHT', -1),
        e('PAD_RY_UP', 'PAD_RY_DOWN', -1), e('PAD_RY_DOWN', 'PAD_RY_UP', -1),
        e('PAD_LT', 'PAD_RB_DOWN', -1),            /* shake = A */
        e('PAD_MM', 'PAD_MM', -1),                 /* Plus = Start */
        e('PAD_RM', 'PAD_MM', -1),                 /* ZR = Start */
    ];

    return [left, right];
}

export function init(root, c) {
    ctx = c;
    status = el('p', { class: 'status', id: 'dcPresetStatus' });
    const out = el('select', { id: 'dcOutput', 'aria-label': 'Output' });

    for (let i = 0; i < 4; i++) {
        out.append(el('option', { value: i, text: 'Port ' + 'ABCD'[i] + ' (output ' + (i + 1) + ')' }));
    }
    root.append(
        page(meta.title, meta.text),
        card('Controller type', null,
            el('div', { class: 'row' }, el('label', { for: 'dcOutput', text: 'Apply to' }), out, el('span')),
            el('div', { class: 'grid', id: 'dcPresetRows', style: 'margin-top:14px' },
                dcPresets.map((p, i) => el('div', { class: 'tile', id: 'dcTile' + i },
                    el('h3', { text: p.name }), el('p', { text: p.text }),
                    p.steps ? el('ol', { class: 'steps' }, p.steps.map((t) => el('li', { text: t }))) : null,
                    el('button', { class: 'btn primary', id: 'dcPreset' + i, text: 'Apply', 'aria-label': 'Apply ' + p.name, onClick: () => apply(p) })))),
            status),
        motion.build(c));
    out.addEventListener('change', () => { refresh(); });
}

/* The type each port has now, as the adapter reports it. Two tiles can share a type (the two maracas): the one that was
 * applied last on this port wins, remembered in the browser. */
let ports = null;

function remembered(port) {
    try { return Number(localStorage.getItem('iblue.preset.' + port)); } catch (e) { return NaN; }
}

function remember(port, i) {
    try { localStorage.setItem('iblue.preset.' + port, String(i)); } catch (e) { /* storage is only a convenience */ }
}

function mark() {
    const port = Number($('dcOutput').value);
    const now = ports && ports[port] ? ports[port].mode : null;
    let on = -1;

    if (now !== null) {
        const r = remembered(port);

        on = dcPresets[r] && dcPresets[r].mode === now ? r : dcPresets.findIndex((p) => p.mode === now);
    }
    dcPresets.forEach((p, i) => {
        const tile = $('dcTile' + i);
        const btn = $('dcPreset' + i);

        tile.classList.toggle('on', i === on);
        btn.textContent = i === on ? 'Selected' : 'Apply';
        btn.setAttribute('aria-pressed', i === on ? 'true' : 'false');
    });
}

async function refresh() {
    try { ports = ctx.svc() ? await getPortStatus(ctx.svc()) : null; } catch (e) { ports = null; }
    mark();
}

async function apply(preset) {
    const cfgId = Number($('dcOutput').value);

    say(status, 'Writing ' + preset.name + ' to output ' + (cfgId + 1) + '...');
    try {
        if (!defaultMap) {
            defaultMap = (await (await fetch('map/default_pad.json')).json()).map;
        }
        await saveOutputCfg(ctx.svc(), Uint8Array.from([preset.mode, ACC_BOTH]), cfgId);
        if (preset.map === 'motion') {
            if (cfgId > 2) { throw new Error('this one needs two inputs: use port A, B or C'); }
            const [l, r] = motionMaps(defaultMap);

            await savePresetInput({ map: l }, ctx.svc(), cfgId);
            await savePresetInput({ map: r }, ctx.svc(), cfgId + 1);
        }
        else if (preset.map) {
            await savePresetInput({ map: preset.map === 'maracas' ? maracasMap(defaultMap) : defaultMap }, ctx.svc(), cfgId);
        }
        remember(cfgId, dcPresets.indexOf(preset));
        say(status, preset.name + ' is set on output ' + (cfgId + 1) + ' (' + devCfg[preset.mode] + ').', 'ok');
        await refresh();
        ctx.toast(preset.name + ' is set on port ' + 'ABCD'[cfgId] + '.');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

export async function connected() { say(status, ''); await refresh(); await motion.connected(); }
export function disconnected() { say(status, ''); ports = null; mark(); motion.disconnected(); }
