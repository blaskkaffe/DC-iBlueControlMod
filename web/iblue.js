// iBlueControlMod page: tap buttons and profiles.

import { brUuid, cfg_cmd_get_fw_name, ibluGestureNames, ibluActionNames, ibluProfileResult } from './utils/constants.js';
import { getStringCmd } from './utils/getStringCmd.js';
import { getAppVersion } from './utils/getAppVersion.js';
import { getBdAddr } from './utils/getBdAddr.js';
import { getIblueInfo, getGestures, setGestures, getProfile, saveProfile, loadProfile, deleteProfile,
    getAnimCfg, setAnimCfg, resetAnimCfg, playAnim } from './utils/iblueCmd.js';
import { ibluAnimFields, ibluAnimDefaults, ibluAnimClamp, ibluAnimLevel, ibluAnimTank } from './utils/ibluAnim.js';

let brService = null;
let info = null;

const $ = (id) => document.getElementById(id);

function show(ids, visible) {
    ids.forEach((id) => { $(id).style.display = visible ? 'block' : 'none'; });
}

function onDisconnected() {
    log('> Bluetooth Device disconnected');
    show(['divBtConn'], true);
    show(['divInfo', 'divGestures', 'divLeds', 'divProfiles'], false);
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

/* ---- Port LEDs and the boot animation ------------------------------------------------------- */

function ledValues() {
    return ibluAnimFields.map((f) => Number($('led_' + f.key).value));
}

function drawLeds() {
    const values = ibluAnimClamp(ledValues());
    const tank = $('ledTank').getContext('2d');
    const w = $('ledTank').width;
    const h = $('ledTank').height;
    const geo = ibluAnimTank(values);
    const pad = 30;
    const scale = (w - 2 * pad) / geo.width;

    tank.clearRect(0, 0, w, h);
    tank.strokeStyle = '#7ab';
    tank.lineWidth = 3;
    tank.beginPath();
    tank.moveTo(pad, 10); tank.lineTo(pad, h - 10);
    tank.moveTo(w - pad, 10); tank.lineTo(w - pad, h - 10);
    tank.stroke();
    tank.fillStyle = 'rgba(60,110,200,0.35)';
    tank.fillRect(pad, 18, w - 2 * pad, h - 36);
    geo.leds.forEach((x, i) => {
        tank.fillStyle = '#6af';
        tank.beginPath();
        tank.arc(pad + x * scale, h / 2, 9, 0, 2 * Math.PI);
        tank.fill();
        tank.fillStyle = '#fff';
        tank.font = '12px sans-serif';
        tank.textAlign = 'center';
        tank.fillText('ABCD'[i], pad + x * scale, h / 2 + 4);
    });

    const curve = $('ledCurve').getContext('2d');
    const cw = $('ledCurve').width;
    const ch = $('ledCurve').height;

    curve.clearRect(0, 0, cw, ch);
    curve.strokeStyle = '#444';
    curve.lineWidth = 1;
    for (let g = 0; g <= 4; g++) {
        curve.beginPath();
        curve.moveTo(30, 10 + (ch - 30) * g / 4); curve.lineTo(cw - 10, 10 + (ch - 30) * g / 4);
        curve.stroke();
    }
    curve.strokeStyle = '#6af';
    curve.lineWidth = 2;
    curve.beginPath();
    for (let i = 0; i <= 200; i++) {
        const u = i / 200;
        const x = 30 + u * (cw - 40);
        const y = 10 + (ch - 30) * (1 - ibluAnimLevel(values, u));
        if (i === 0) { curve.moveTo(x, y); } else { curve.lineTo(x, y); }
    }
    curve.stroke();
    curve.fillStyle = '#999';
    curve.font = '11px sans-serif';
    curve.textAlign = 'left';
    curve.fillText('0 %', 2, ch - 18);
    curve.fillText('100 %', 0, 20);
    curve.textAlign = 'center';
    curve.fillText('start of the drops', 90, ch - 4);
    curve.fillText('end', cw - 20, ch - 4);
}

function setLedValues(values) {
    ibluAnimFields.forEach((f, i) => {
        $('led_' + f.key).value = values[i];
        $('led_' + f.key + '_n').value = values[i];
    });
    drawLeds();
}

function initLeds(values) {
    const rows = $('ledRows');

    rows.innerHTML = '';
    ibluAnimFields.forEach((f, i) => {
        const div = document.createElement('div');
        div.style.margin = '0.4em 0';
        const label = document.createElement('label');
        label.textContent = f.label + ': ';
        label.setAttribute('for', 'led_' + f.key + '_n');
        label.style.display = 'inline-block';
        label.style.minWidth = '17em';
        const range = document.createElement('input');
        range.type = 'range';
        range.id = 'led_' + f.key;
        range.min = f.min; range.max = f.max; range.step = f.step;
        range.value = values[i];
        const num = document.createElement('input');
        num.type = 'number';
        num.id = 'led_' + f.key + '_n';
        num.min = f.min; num.max = f.max; num.step = f.step;
        num.value = values[i];
        num.style.width = '5.5em';
        range.addEventListener('input', () => { num.value = range.value; drawLeds(); });
        num.addEventListener('input', () => { range.value = num.value; drawLeds(); });
        const unit = document.createElement('span');
        unit.textContent = ' ' + f.unit;
        div.appendChild(label); div.appendChild(range); div.appendChild(num); div.appendChild(unit);
        if (f.hint) {
            const hint = document.createElement('div');
            hint.innerHTML = '<small></small>';
            hint.firstChild.textContent = f.hint;
            div.appendChild(hint);
        }
        rows.appendChild(div);
    });
    drawLeds();
}

async function saveLeds() {
    try {
        const values = ibluAnimClamp(ledValues());
        const result = await setAnimCfg(brService, values);

        if (result !== 0) {
            say('ledStatus', ibluProfileResult[result] || 'Not saved.');
            return;
        }
        setLedValues(await getAnimCfg(brService)); /* what the adapter kept */
        await playAnim(brService);
        say('ledStatus', 'Saved. Playing the animation.');
    }
    catch (error) {
        say('ledStatus', 'Argh! ' + error);
    }
}

async function defaultsLeds() {
    try {
        const result = await resetAnimCfg(brService);

        setLedValues(result === 0 ? await getAnimCfg(brService) : ibluAnimDefaults());
        say('ledStatus', result === 0 ? 'Back to the defaults (saved).' : 'Not saved.');
    }
    catch (error) {
        say('ledStatus', 'Argh! ' + error);
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
        if (info.animation) {
            initLeds(await getAnimCfg(brService));
            show(['divLeds'], true);
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
$('btnLedSave').addEventListener('click', saveLeds);
$('btnLedDefaults').addEventListener('click', defaultsLeds);
