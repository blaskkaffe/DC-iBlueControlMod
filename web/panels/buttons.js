// Buttons: what the tap buttons do, and the profiles.
import { ibluGestureNames, ibluActionNames, ibluProfileResult } from '../utils/constants.js';
import { getGestures, setGestures, getProfile, saveProfile, loadProfile, deleteProfile } from '../utils/iblueCmd.js';
import { el, card, say, page } from './ui.js';

let ctx;
let gestureRows;
let gestureStatus;
let profileRows;
let profileStatus;

export const meta = { id: 'buttons', title: 'Buttons', icon: 'pointer', needsIblue: true,
    text: 'What the button of the adapter does when you tap it, and the profiles you can switch between.' };

const $ = (id) => document.getElementById(id);

export function init(root, c) {
    ctx = c;
    gestureRows = el('div', { id: 'gestureRows' });
    gestureStatus = el('span', { class: 'status', id: 'gestureStatus' });
    profileRows = el('div', { id: 'profileRows' });
    profileStatus = el('span', { class: 'status', id: 'profileStatus' });
    root.append(
        page(meta.title, meta.text),
        card('Tap actions', 'Tap the button once, twice or three times, each tap within about half a second of the last. The single tap waits that half second to see if another follows.',
            gestureRows,
            el('p', { class: 'muted' }, el('small', { text: 'Tap and hold (tap, then press again and keep holding) is fixed: the LED sweeps through all colours, release when it shows the one you want and it is kept as the custom colour. The holds of 3, 6, 10 and 30 seconds are as in BlueRetro.' })),
            el('div', { class: 'actions' },
                el('button', { class: 'btn primary', id: 'btnGestureSave', text: 'Save tap actions', onClick: saveGestureMap }), gestureStatus)),
        card('Profiles', 'A profile is a named copy of the whole configuration: the mapping, output mode and accessories of every output, and the global settings. Save puts what the adapter runs now into a slot; Load replaces the configuration with the profile and keeps it. Profiles survive a factory reset. To switch without a computer, give a tap the action "Load profile" above.',
            profileRows, profileStatus));
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

        say(gestureStatus, result === 0 ? 'Saved.' : (ibluProfileResult[result] || 'Not saved.'), result === 0 ? 'ok' : 'bad');
    }
    catch (error) {
        say(gestureStatus, 'Argh! ' + error, 'bad');
    }
}

async function drawProfiles(info) {
    profileRows.replaceChildren();
    for (let slot = 0; slot < info.profileCount; slot++) {
        const p = await getProfile(ctx.svc(), slot);
        const name = el('input', { type: 'text', id: 'profileName' + slot, maxlength: 15, placeholder: 'Profile ' + (slot + 1), size: 16,
            value: p.used ? p.name : '', 'aria-label': 'Name of profile ' + (slot + 1) });
        const load = el('button', { class: 'btn small', id: 'profileLoad' + slot, text: 'Load', disabled: !p.used, onClick: () => profileAction(slot, 'load') });
        const del = el('button', { class: 'btn small danger', id: 'profileDelete' + slot, text: 'Delete', disabled: !p.used, onClick: () => profileAction(slot, 'delete') });

        profileRows.append(el('div', { class: 'row' },
            el('label', { for: 'profileName' + slot, text: 'Profile ' + (slot + 1) }),
            el('div', { class: 'val' }, name, el('span', { class: 'unit', text: p.used ? 'saved' : 'empty' })),
            el('div', { class: 'val' },
                el('button', { class: 'btn small', id: 'profileSave' + slot, text: 'Save current here', onClick: () => profileAction(slot, 'save') }), load, del)));
    }
}

async function profileAction(slot, what) {
    try {
        let result;

        if (what === 'save') {
            if ($('profileLoad' + slot).disabled === false &&
                !(await ctx.confirm('Replace what is saved in profile ' + (slot + 1) + ' with the current configuration?'))) {
                return;
            }
            result = await saveProfile(ctx.svc(), slot, $('profileName' + slot).value);
        }
        else if (what === 'load') {
            if (!(await ctx.confirm('Replace the current configuration with profile ' + (slot + 1) + '?'))) {
                return;
            }
            result = await loadProfile(ctx.svc(), slot);
        }
        else {
            if (!(await ctx.confirm('Delete profile ' + (slot + 1) + '?'))) {
                return;
            }
            result = await deleteProfile(ctx.svc(), slot);
        }
        if (result === 0) {
            say(profileStatus, (what === 'save' ? 'Saved in' : what === 'load' ? 'Loaded' : 'Deleted') + ' profile ' + (slot + 1) + '.', 'ok');
        }
        else {
            say(profileStatus, ibluProfileResult[result] || 'Failed.', 'bad');
        }
        await drawProfiles(ctx.conn.info);
    }
    catch (error) {
        say(profileStatus, 'Argh! ' + error, 'bad');
    }
}

export async function connected() {
    const info = ctx.conn.info;

    if (!info) { return; }
    if (info.gestures) { await drawGestures(info); }
    if (info.profiles) { await drawProfiles(info); }
}

export function disconnected() {
    gestureRows.replaceChildren();
    profileRows.replaceChildren();
}
