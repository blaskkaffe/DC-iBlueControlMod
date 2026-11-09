// Joy-Con motion for Samba de Amigo: settings, calibration and a live view. Shown in the Controllers category.
// The firmware reads the accelerometer and the gyro of the Joy-Cons and makes a position and a shake of it
// (docs/samba-de-amigo.md); the "Samba de Amigo (Joy-Con motion)" preset maps those to the maracas.
import { getMotionCfg, setMotionCfg, motionPose, resetMotionCal, getMotionStatus } from '../utils/iblueCmd.js';
import { ibluMotionFields, ibluMotionClamp, ibluMotionDefaults, ibluMotionPoses } from '../utils/ibluMotion.js';
import { el, card, say } from './ui.js';

const $ = (id) => document.getElementById(id);
const shown = ibluMotionFields.filter((f) => f.label);

let ctx;
let root;
let status;
let live;
let poseStatus;
let timer = null;
let busy = false;

function values() {
    return ibluMotionFields.map((f) => {
        const n = $('mo_' + f.key);

        if (f.key === 'mode') { return n.checked ? 1 : 0; }
        return f.kind === 'check' ? (n.checked ? 1 : 0) : Number(n.value);
    });
}

function setValues(v) {
    ibluMotionFields.forEach((f, i) => {
        const n = $('mo_' + f.key);

        if (f.kind === 'check' || f.key === 'mode') { n.checked = v[i] === 1; return; }
        n.value = v[i];
        $('mo_' + f.key + '_n').value = v[i];
    });
}

function row(f) {
    if (f.kind === 'check') {
        return el('div', { class: 'row' },
            el('label', { for: 'mo_' + f.key, text: f.label }),
            el('input', { type: 'checkbox', id: 'mo_' + f.key }),
            el('span'));
    }
    const range = el('input', { type: 'range', id: 'mo_' + f.key, min: f.min, max: f.max, step: f.step, value: f.def, 'aria-label': f.label });
    const num = el('input', { type: 'number', id: 'mo_' + f.key + '_n', min: f.min, max: f.max, step: f.step, value: f.def, 'aria-label': f.label + ' (number)' });

    range.addEventListener('input', () => { num.value = range.value; });
    num.addEventListener('input', () => { range.value = num.value; });
    return el('div', { class: 'row' },
        el('label', { for: 'mo_' + f.key + '_n', text: f.label }),
        range,
        el('div', { class: 'val' }, num, el('span', { class: 'unit', text: f.unit })),
        f.hint ? el('p', { class: 'hint', text: f.hint }) : null);
}

/* The cards; they go into the Controllers category */
export function build(c) {
    ctx = c;
    status = el('span', { class: 'status', id: 'moStatus' });
    poseStatus = el('p', { class: 'status', id: 'moPoseStatus' });
    live = el('div', { id: 'moLive', class: 'live', 'aria-live': 'polite' });
    root = el('div', { id: 'motionCards' },
        card('Joy-Con motion', 'For Samba de Amigo: the Joy-Cons are the maracas. Tilt gives the height, turning gives the sideways position, a shake gives the shake. ' +
            'Joy-Cons only; switch it on, then connect the Joy-Cons again. Apply the "Samba de Amigo (Joy-Con motion)" type above to the port, with the left Joy-Con on the first controller and the right one on the second.',
            el('div', { class: 'row' },
                el('label', { for: 'mo_mode', text: 'Motion mode' }),
                el('input', { type: 'checkbox', id: 'mo_mode' }), el('span')),
            shown.map(row),
            el('div', { class: 'actions' },
                el('button', { class: 'btn primary', id: 'btnMoSave', text: 'Save', onClick: save }),
                el('button', { class: 'btn', id: 'btnMoDefaults', text: 'Defaults', onClick: () => { setValues(ibluMotionDefaults()); } }),
                status)),
        card('Calibrate', 'Hold the Joy-Cons still when the time is up. A short blink of the adapter\'s LED means the pose was taken, three blinks that it was not still enough.',
            el('div', { class: 'grid', id: 'moPoses' },
                ibluMotionPoses.map((p) => el('div', { class: 'tile' },
                    el('h3', { text: p.name }), el('p', { text: p.text }),
                    el('button', { class: 'btn primary', id: 'moPose' + p.id, text: 'In ' + p.delay + ' seconds', 'aria-label': p.name + ' pose in ' + p.delay + ' seconds', onClick: () => pose(p) })))),
            poseStatus,
            el('div', { class: 'actions' },
                el('button', { class: 'btn', id: 'btnMoForget', text: 'Forget the saved pointing directions', onClick: forget }))),
        card('Live', 'What the adapter sees from each Joy-Con while this page is open.', live));
    root.style.display = 'none';
    return root;
}

function liveText(s) {
    const side = s.side < 0 ? 'Left' : 'Right';
    const bits = [s.forward ? 'forward set' : 'forward not set', s.axis ? 'pointing direction learned' : 'pointing direction guessed'];

    if (s.range) { bits.push('sideways range learned'); }
    if (s.failed) { bits.push('last pose failed'); }
    return side + ' Joy-Con: height ' + s.height + '°, heading ' + s.heading + '°, position X ' + (128 + s.x) + ' Y ' + (128 + s.y) +
        (s.shaking ? ', SHAKE' : '') + ' (' + bits.join(', ') + ')';
}

async function refresh() {
    if (busy || !ctx.svc() || !$('moLive')) { return; }
    busy = true;
    try {
        const st = await getMotionStatus(ctx.svc());

        live.replaceChildren(...(st.length ? st.map((s) => el('p', { class: 'live-row', text: liveText(s) }))
            : [el('p', { class: 'hint', text: 'No Joy-Con is sending motion. Switch motion mode on and connect the Joy-Cons again.' })]));
    }
    catch (e) { /* the next round tries again */ }
    busy = false;
}

async function save() {
    say(status, 'Saving...');
    try {
        const r = await setMotionCfg(ctx.svc(), ibluMotionClamp(values()));

        say(status, r === 0 ? 'Saved. Connect the Joy-Cons again if you changed the motion mode.' : 'The adapter did not accept it (' + r + ').', r === 0 ? 'ok' : 'bad');
        if (r === 0) { setValues(await getMotionCfg(ctx.svc())); }
    }
    catch (error) { say(status, 'Argh! ' + error, 'bad'); }
}

async function pose(p) {
    say(poseStatus, p.name + ' in ' + p.delay + ' seconds: hold still...');
    try {
        const r = await motionPose(ctx.svc(), p.id, p.delay);

        if (r !== 0) { say(poseStatus, 'The adapter did not accept it (' + r + ').', 'bad'); return; }
        setTimeout(async () => {
            await refresh();
            say(poseStatus, p.name + ' was asked for. If the LED blinked three times it was not still enough: try again.', 'ok');
        }, (p.delay + 2) * 1000);
    }
    catch (error) { say(poseStatus, 'Argh! ' + error, 'bad'); }
}

async function forget() {
    if (!(await ctx.confirm('Forget what the adapter learned about how the Joy-Cons point and how far they turn?'))) { return; }
    try {
        await resetMotionCal(ctx.svc());
        say(poseStatus, 'Forgotten. Take the Up (and Out) pose again.', 'ok');
    }
    catch (error) { say(poseStatus, 'Argh! ' + error, 'bad'); }
}

/* Called when the connection is made (loads the settings), and when it is lost */
export async function connected() {
    const has = !!(ctx.conn.info && ctx.conn.info.motion);

    root.style.display = has ? '' : 'none';
    if (!has) { return; }
    try { setValues(await getMotionCfg(ctx.svc())); }
    catch (e) { say(status, 'Argh! ' + e, 'bad'); }
    start();
}

export function disconnected() {
    if (root) { root.style.display = 'none'; }
    stop();
}

/* The live view is read while the Controllers category is open */
function start() {
    stop();
    timer = setInterval(() => {
        const p = $('p-controllers');

        if (p && p.classList.contains('on') && !document.hidden) { refresh(); }
    }, 1000);
}

function stop() {
    if (timer) { clearInterval(timer); timer = null; }
}
