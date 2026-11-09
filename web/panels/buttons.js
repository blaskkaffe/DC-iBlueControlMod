// Buttons: what the tap buttons do.
import { ibluGestureNames, ibluActionNames, ibluResult } from '../utils/constants.js';
import { getGestures, setGestures } from '../utils/iblueCmd.js';
import { el, card, say, page } from './ui.js';

let ctx;
let gestureRows;
let gestureStatus;

export const meta = { id: 'buttons', title: 'Buttons', icon: 'pointer', needsIblue: true,
    text: 'What the button of the adapter does when you tap it,.' };

const $ = (id) => document.getElementById(id);

export function init(root, c) {
    ctx = c;
    gestureRows = el('div', { id: 'gestureRows' });
    gestureStatus = el('span', { class: 'status', id: 'gestureStatus' });
    root.append(
        page(meta.title, meta.text),
        card('Tap actions', 'Tap the button once, twice or three times, each tap within about half a second of the last. The single tap waits that half second to see if another follows.',
            gestureRows,
            el('p', { class: 'muted' }, el('small', { text: 'Tap and hold (tap, then press again and keep holding) is fixed: the LED sweeps through all colours, release when it shows the one you want and it is kept as the custom colour. The holds of 3, 6, 10 and 30 seconds are as in BlueRetro.' })),
            el('div', { class: 'actions' },
                el('button', { class: 'btn primary', id: 'btnGestureSave', text: 'Save tap actions', onClick: saveGestureMap }), gestureStatus)));
}

async function drawGestures(info) {
    const actions = await getGestures(ctx.svc(), info.gestureCount);

    gestureRows.replaceChildren();
    for (let g = 0; g < info.gestureCount; g++) {
        const sel = el('select', { id: 'gesture' + g, 'aria-label': ibluGestureNames[g] || ('Tap x' + (g + 1)) });
        for (let a = 0; a < info.actionCount; a++) {
            sel.append(el('option', { value: a, text: ibluActionNames[a] || ('Action ' + a) }));
        }
        sel.value = actions[g];
        gestureRows.append(el('div', { class: 'row' },
            el('label', { for: 'gesture' + g, text: ibluGestureNames[g] || ('Tap x' + (g + 1)) }), sel, el('span')));
    }
    say(gestureStatus, '');
}

async function saveGestureMap() {
    const info = ctx.conn.info;
    const chosen = [];

    for (let g = 0; g < info.gestureCount; g++) {
        chosen.push(parseInt($('gesture' + g).value, 10));
    }
    try {
        const result = await setGestures(ctx.svc(), chosen);

        say(gestureStatus, result === 0 ? 'Saved.' : (ibluResult[result] || 'Not saved.'), result === 0 ? 'ok' : 'bad');
    }
    catch (error) {
        say(gestureStatus, 'Argh! ' + error, 'bad');
    }
}

export async function connected() {
    const info = ctx.conn.info;

    if (!info) { return; }
    if (info.gestures) { await drawGestures(info); }
}

export function disconnected() {
    gestureRows.replaceChildren();
}
