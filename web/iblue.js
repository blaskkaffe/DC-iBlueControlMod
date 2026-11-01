// iBlueControlMod page: tap buttons and profiles.

import { brUuid, cfg_cmd_get_fw_name, ibluGestureNames, ibluActionNames, ibluProfileResult } from './utils/constants.js';
import { getStringCmd } from './utils/getStringCmd.js';
import { getAppVersion } from './utils/getAppVersion.js';
import { getBdAddr } from './utils/getBdAddr.js';
import { getIblueInfo, getGestures, setGestures, getProfile, saveProfile, loadProfile, deleteProfile }
    from './utils/iblueCmd.js';

let brService = null;
let info = null;

const $ = (id) => document.getElementById(id);

function show(ids, visible) {
    ids.forEach((id) => { $(id).style.display = visible ? 'block' : 'none'; });
}

function onDisconnected() {
    log('> Bluetooth Device disconnected');
    show(['divBtConn'], true);
    show(['divInfo', 'divGestures', 'divProfiles'], false);
}

function say(id, text) {
    $(id).textContent = text;
}

/* ---- Tap buttons ------------------------------------------------------------------------- */

async function initGestures() {
    const actions = await getGestures(brService, info.gestureCount);
    const rows = $('gestureRows');

    rows.innerHTML = '';
    for (let g = 0; g < info.gestureCount; g++) {
        const div = document.createElement('div');
        const label = document.createElement('label');
        const sel = document.createElement('select');

        label.textContent = (ibluGestureNames[g] || ('Tap x' + (g + 1))) + ': ';
        label.setAttribute('for', 'gesture' + g);
        sel.id = 'gesture' + g;
        for (let a = 0; a < info.actionCount; a++) {
            const option = document.createElement('option');
            option.value = a;
            option.text = ibluActionNames[a] || ('Action ' + a);
            sel.add(option);
        }
        sel.value = actions[g];
        div.appendChild(label);
        div.appendChild(sel);
        rows.appendChild(div);
    }
    say('gestureStatus', '');
}

async function saveGestures() {
    const chosen = [];

    for (let g = 0; g < info.gestureCount; g++) {
        chosen.push(parseInt($('gesture' + g).value, 10));
    }
    try {
        const result = await setGestures(brService, chosen);
        say('gestureStatus', result === 0 ? 'Saved.' : (ibluProfileResult[result] || 'Not saved.'));
    }
    catch (error) {
        say('gestureStatus', 'Argh! ' + error);
    }
}

/* ---- Profiles ---------------------------------------------------------------------------- */

async function refreshProfiles() {
    const rows = $('profileRows');

    rows.innerHTML = '';
    for (let slot = 0; slot < info.profileCount; slot++) {
        const p = await getProfile(brService, slot);
        const div = document.createElement('div');
        div.style.marginBottom = '0.4em';

        const title = document.createElement('b');
        title.textContent = 'Profile ' + (slot + 1) + ': ';
        div.appendChild(title);

        const name = document.createElement('input');
        name.type = 'text';
        name.maxLength = 15;
        name.id = 'profileName' + slot;
        name.placeholder = 'Profile ' + (slot + 1);
        name.value = p.used ? p.name : '';
        name.size = 16;
        div.appendChild(name);

        const save = document.createElement('button');
        save.id = 'profileSave' + slot;
        save.textContent = 'Save current here';
        save.addEventListener('click', () => profileAction(slot, 'save'));
        div.appendChild(save);

        const load = document.createElement('button');
        load.id = 'profileLoad' + slot;
        load.textContent = 'Load';
        load.disabled = !p.used;
        load.addEventListener('click', () => profileAction(slot, 'load'));
        div.appendChild(load);

        const del = document.createElement('button');
        del.id = 'profileDelete' + slot;
        del.textContent = 'Delete';
        del.disabled = !p.used;
        del.addEventListener('click', () => profileAction(slot, 'delete'));
        div.appendChild(del);

        const state = document.createElement('span');
        state.textContent = p.used ? ' saved' : ' empty';
        div.appendChild(state);

        rows.appendChild(div);
    }
}

async function profileAction(slot, what) {
    try {
        let result;

        if (what === 'save') {
            if ($('profileLoad' + slot).disabled === false &&
                !confirm('Replace what is saved in profile ' + (slot + 1) + ' with the current configuration?')) {
                return;
            }
            result = await saveProfile(brService, slot, $('profileName' + slot).value);
        }
        else if (what === 'load') {
            if (!confirm('Replace the current configuration with profile ' + (slot + 1) + '?')) {
                return;
            }
            result = await loadProfile(brService, slot);
        }
        else {
            if (!confirm('Delete profile ' + (slot + 1) + '?')) {
                return;
            }
            result = await deleteProfile(brService, slot);
        }
        say('profileStatus', result === 0 ? 'Done: ' + what + ' profile ' + (slot + 1) + '.' : (ibluProfileResult[result] || 'Failed.'));
        await refreshProfiles();
    }
    catch (error) {
        say('profileStatus', 'Argh! ' + error);
    }
}

/* ---- Connect ----------------------------------------------------------------------------- */

export function btConn() {
    log('Requesting Bluetooth Device...');
    let name = '';
    let bdaddr = '';
    let app_ver = '';

    navigator.bluetooth.requestDevice(
        {filters: [{namePrefix: 'BlueRetro'}],
        optionalServices: [brUuid[0]]})
    .then(device => {
        log('Connecting to GATT Server...');
        name = device.name;
        device.addEventListener('gattserverdisconnected', onDisconnected);
        return device.gatt.connect();
    })
    .then(server => {
        log('Getting BlueRetro Service...');
        return server.getPrimaryService(brUuid[0]);
    })
    .catch(error => {
        log(error.name);
        throw 'Couldn\'t connect to BlueRetro';
    })
    .then(service => {
        brService = service;
        return getBdAddr(brService);
    })
    .then(value => {
        bdaddr = value;
        return getAppVersion(brService);
    })
    .then(value => {
        app_ver = value;
        return getIblueInfo(brService);
    })
    .then(async value => {
        info = value;
        $('divInfo').textContent = 'Connected to: ' + name + ' (' + bdaddr + ') [' + app_ver + ']';
        show(['divBtConn'], false);
        show(['divInfo'], true);
        if (info === null) {
            $('divInfo').textContent += '. This firmware does not have the iBlueControlMod features (tap buttons, profiles). ' +
                'Update it on the OTA page.';
            return;
        }
        $('divInfo').textContent += '. iBlueControlMod features: tap buttons' +
            (info.profiles ? ', profiles' : '') + (info.gameId ? ', game id from openMenu' : '') + '.';
        if (info.gestures) {
            await initGestures();
            show(['divGestures'], true);
        }
        if (info.profiles) {
            await refreshProfiles();
            show(['divProfiles'], true);
        }
    })
    .catch(error => {
        log('Argh! ' + error);
    });
}

$('btnGestureSave').addEventListener('click', saveGestures);
