// iBlueControlMod page: tap buttons.

import { brUuid, cfg_cmd_get_fw_name, ibluGestureNames, ibluActionNames, ibluResult } from './utils/constants.js';
import { getStringCmd } from './utils/getStringCmd.js';
import { getAppVersion } from './utils/getAppVersion.js';
import { getBdAddr } from './utils/getBdAddr.js';
import { getIblueInfo, getGestures, setGestures }
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
    show(['divInfo', 'divGestures'], false);
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
        say('gestureStatus', result === 0 ? 'Saved.' : (ibluResult[result] || 'Not saved.'));
    }
    catch (error) {
        say('gestureStatus', 'Argh! ' + error);
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
            $('divInfo').textContent += '. This firmware does not have the iBlueControlMod features (tap buttons). ' +
                'Update it on the OTA page.';
            return;
        }
        $('divInfo').textContent += '. iBlueControlMod features: tap buttons' +
            (info.gameId ? ', game id from openMenu' : '') + '.';
        if (info.gestures) {
            await initGestures();
            show(['divGestures'], true);
        }
    })
    .catch(error => {
        log('Argh! ' + error);
    });
}

$('btnGestureSave').addEventListener('click', saveGestures);
