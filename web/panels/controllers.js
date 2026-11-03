// Controllers: the type the Dreamcast sees on each port, with the button mapping for it, in one click.
import { devCfg } from '../utils/constants.js';
import { saveOutputCfg } from '../utils/saveOutputCfg.js';
import { savePresetInput } from '../utils/savePresetInput.js';
import { el, card, say, page } from './ui.js';

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
      text: "A first step with the sticks: the left stick is the left maraca, the right stick the right one; L1 / R1 shake left / right (B / A), L2 is the left maraca's button C, R2 and Start are Start. Height from the stick for now; tilt and shake from Joy-Cons is planned. Not tried with the game yet." },
    { name: 'Keyboard', mode: 2, map: false, text: 'A Dreamcast keyboard. The mapping of the output is kept.' },
    { name: 'Mouse', mode: 3, map: false, text: 'A Dreamcast mouse. The mapping of the output is kept.' },
];

const $ = (id) => document.getElementById(id);

/* The maracas of Samba de Amigo (docs/samba-de-amigo.md): per maraca two axes (the two sticks), a shake (B left, A
 * right), a button (C left, Start right). The default entries for the sticks and the system combinations are kept. */
function maracasMap(def) {
    const entry = (src, dst) => [src, dst, 0, 100, 50, 135, 0, 0, 0];

    return [
        ...def.slice(0, 8),                       /* the two sticks, as they are */
        entry('PAD_LS', 'PAD_RB_RIGHT'),          /* L1 = shake left (B) */
        entry('PAD_RS', 'PAD_RB_DOWN'),           /* R1 = shake right (A) */
        entry('PAD_LM', 'PAD_RS'),                /* L2 = button C of the left maraca */
        entry('PAD_RM', 'PAD_MM'),                /* R2 = Start of the right maraca */
        entry('PAD_MM', 'PAD_MM'),                /* Start = Start */
        ...def.filter((e) => e[1].startsWith('CB_')),
    ];
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
                dcPresets.map((p, i) => el('div', { class: 'tile' },
                    el('h3', { text: p.name }), el('p', { text: p.text }),
                    el('button', { class: 'btn primary', id: 'dcPreset' + i, text: 'Apply', 'aria-label': 'Apply ' + p.name, onClick: () => apply(p) })))),
            status));
}

async function apply(preset) {
    const cfgId = Number($('dcOutput').value);

    say(status, 'Writing ' + preset.name + ' to output ' + (cfgId + 1) + '...');
    try {
        if (!defaultMap) {
            defaultMap = (await (await fetch('map/default_pad.json')).json()).map;
        }
        await saveOutputCfg(ctx.svc(), Uint8Array.from([preset.mode, ACC_BOTH]), cfgId);
        if (preset.map) {
            await savePresetInput({ map: preset.map === 'maracas' ? maracasMap(defaultMap) : defaultMap }, ctx.svc(), cfgId);
        }
        say(status, preset.name + ' is set on output ' + (cfgId + 1) + ' (' + devCfg[preset.mode] + ').', 'ok');
        ctx.toast(preset.name + ' is set on port ' + 'ABCD'[cfgId] + '.');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

export function connected() { say(status, ''); }
export function disconnected() { say(status, ''); }
