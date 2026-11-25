// Device: what is connected, and the things that act on the whole adapter.
import { setDeepSleep } from '../utils/setDeepSleep.js';
import { setReset } from '../utils/setReset.js';
import { setFactoryReset } from '../utils/setFactoryReset.js';
import { isDcOnly, setDcOnly } from '../utils/dcOnly.js';
import { runPortCheck } from '../utils/iblueCmd.js';
import { el, card, say, page } from './ui.js';

let ctx;
let kv;
let status;
let pcStatus;
let pcOut;

export const meta = { id: 'device', title: 'Device', icon: 'chip',
    text: 'The connected adapter and the actions that affect all of it.' };

export function init(root, c) {
    ctx = c;
    kv = el('dl', { class: 'kv', id: 'deviceInfo' });
    status = el('div', { class: 'status', id: 'deviceStatus' });
    pcStatus = el('div', { class: 'status', id: 'portCheckStatus' });
    pcOut = el('pre', { id: 'portCheckOut', hidden: true });
    root.append(
        page(meta.title, meta.text),
        card('Adapter', null, kv),
        card('Maintenance', 'Each of these asks first. The adapter disconnects when it resets or sleeps.',
            el('div', { class: 'actions' },
                el('button', { class: 'btn', id: 'btnSleep', text: 'Deep sleep', onClick: () => run('Put the adapter in deep sleep?', setDeepSleep, 'Going to sleep.') }),
                el('button', { class: 'btn', id: 'btnReset', text: 'Reset', onClick: () => run('Reset the adapter?', setReset, 'Resetting.') }),
                el('button', { class: 'btn danger', id: 'btnFactory', text: 'Factory reset', onClick: () => run('Factory reset: erase the settings of the adapter?', setFactoryReset, 'Reset to factory settings.') })),
            status),
        card('Port check', 'For a port that a physical controller does not work on: with the console on and the controller plugged in, run it and send the report. It only reads the pins (about 2 s).',
            el('div', { class: 'actions' },
                el('button', { class: 'btn', id: 'btnPortCheck', text: 'Run port check', onClick: portCheck }),
                el('button', { class: 'btn', id: 'btnPortCheckCopy', text: 'Copy report', hidden: true, onClick: copyReport })),
            pcStatus, pcOut),
        card('This page', null,
            el('label', { class: 'row', style: 'grid-template-columns:auto 1fr;cursor:pointer' },
                el('input', { type: 'checkbox', id: 'dcOnly', checked: isDcOnly(), onChange: (e) => { setDcOnly(e.target.checked); window.dispatchEvent(new Event('dconlychange')); } }),
                el('span', {}, el('b', { text: 'Dreamcast only' }), el('br'),
                    el('small', { text: 'Hides the settings, presets and tools of other consoles (the N64 Controller Pak, other systems in the mapping lists). Kept in this browser.' })))),
        card('Connection log', 'What the page and the adapter said to each other; useful if something does not work.',
            el('details', {}, el('summary', { text: 'Show the log' }),
                el('div', { id: 'status' }), el('div', { id: 'content' }), el('pre', { id: 'log' }))));
}

async function portCheck() {
    if (!ctx.conn.info || !ctx.conn.info.portCheck) {
        say(pcStatus, 'This firmware has no port check (it came with test build step 10).', 'bad');
        return;
    }
    say(pcStatus, 'Checking the ports...');
    try {
        pcOut.textContent = await runPortCheck(ctx.svc());
        pcOut.hidden = false;
        document.getElementById('btnPortCheckCopy').hidden = false;
        say(pcStatus, 'Done. Copy the report and send it.', 'ok');
    }
    catch (error) {
        say(pcStatus, 'Argh! ' + error, 'bad');
    }
}

async function copyReport() {
    try {
        await navigator.clipboard.writeText(pcOut.textContent);
        ctx.toast('Report copied.');
    }
    catch (e) {
        const r = document.createRange();
        r.selectNodeContents(pcOut);
        window.getSelection().removeAllRanges();
        window.getSelection().addRange(r);
        ctx.toast('Selected: copy it with Ctrl+C.');
    }
}

async function run(question, fn, done) {
    if (!(await ctx.confirm(question))) { return; }
    try {
        await fn(ctx.svc());
        say(status, done, 'ok');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

export function connected() {
    const c = ctx.conn;
    const features = c.info ? ['tap buttons', c.info.gameId ? 'game id' : null, c.info.motion ? 'Joy-Con motion' : null].filter(Boolean).join(', ') : 'none (another BlueRetro firmware)';

    kv.replaceChildren(
        el('dt', { text: 'Name' }), el('dd', { text: c.name }),
        el('dt', { text: 'Address' }), el('dd', { text: c.bdaddr }),
        el('dt', { text: 'Firmware' }), el('dd', { text: c.appVer || 'unknown' }),
        el('dt', { text: 'iBlueControlMod features' }), el('dd', { text: features }));
    say(status, '');
}

export function disconnected() {
    kv.replaceChildren();
}
