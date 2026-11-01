// Dreamcast controller presets: a controller type (output mode) plus the default button mapping, one click.

import { brUuid, devCfg } from './utils/constants.js';
import { getAppVersion } from './utils/getAppVersion.js';
import { getBdAddr } from './utils/getBdAddr.js';
import { getGameId } from './utils/getGameId.js';
import { setGameIdCfg } from './utils/setGameIdCfg.js';
import { saveOutputCfg } from './utils/saveOutputCfg.js';
import { savePresetInput } from './utils/savePresetInput.js';

let brService = null;
let defaultMap = null;
let gameid = '';

const $ = (id) => document.getElementById(id);
const ACC_BOTH = 3;

/* mode: index into devCfg; map: write the default mapping too (false: keep the mapping the output has) */
export const dcPresets = [
    { name: 'Standard controller', mode: 0, map: true,
      text: 'The normal Dreamcast pad: A, B, X, Y, Start, d-pad, one stick and the two analog triggers. Rumble and memory card on.' },
    { name: '6 button + dual stick', mode: 1, map: true,
      text: 'Adds C, Z, D, a second d-pad and a right stick: L1 = Z, R1 = C, L2 / R2 = triggers, right stick = right stick. Some games read what the pad has and misbehave with this type: use it for the games that need it (see "Only for this game").' },
    { name: 'Racing controller', mode: 4, map: true,
      text: 'The Dreamcast racing controller: A, B, Start, d-pad, steering (left stick), accelerator (R2) and brake (L2). It has no X and Y. Not tried on a console yet.' },
    { name: 'Keyboard', mode: 2, map: false, text: 'A Dreamcast keyboard. The mapping of the output is kept.' },
    { name: 'Mouse', mode: 3, map: false, text: 'A Dreamcast mouse. The mapping of the output is kept.' },
];

function say(id, text) {
    $(id).textContent = text;
}

async function apply(preset) {
    const cfgId = Number($('dcOutput').value);

    say('dcPresetStatus', 'Writing ' + preset.name + ' to output ' + (cfgId + 1) + '...');
    try {
        await saveOutputCfg(brService, Uint8Array.from([preset.mode, ACC_BOTH]), cfgId);
        if (preset.map) {
            await savePresetInput({ map: defaultMap }, brService, cfgId);
        }
        say('dcPresetStatus', preset.name + ' is set on output ' + (cfgId + 1) + ' (' + devCfg[preset.mode] + ').');
    }
    catch (error) {
        say('dcPresetStatus', 'Argh! ' + error);
    }
}

function initPresets() {
    const out = $('dcOutput');
    const rows = $('dcPresetRows');

    out.innerHTML = '';
    for (let i = 0; i < 4; i++) {
        const option = document.createElement('option');
        option.value = i;
        option.text = 'Output ' + (i + 1);
        out.add(option);
    }
    rows.innerHTML = '';
    dcPresets.forEach((p, i) => {
        const div = document.createElement('div');
        div.style.marginBottom = '0.8em';
        const button = document.createElement('button');
        button.id = 'dcPreset' + i;
        button.textContent = p.name;
        button.addEventListener('click', () => apply(p));
        div.appendChild(button);
        const text = document.createElement('div');
        text.innerHTML = '<small></small>';
        text.firstChild.textContent = p.text;
        div.appendChild(text);
        rows.appendChild(div);
    });
}

function initGame() {
    const box = $('divDcGame');

    if (!gameid) {
        box.style.display = 'none';
        return;
    }
    box.style.display = 'block';
    say('dcGameText', 'Game: ' + gameid + '. Pick a preset above, then save: the adapter loads it whenever this game starts, and games without one use the normal configuration. Game ids can be switched off with the button (tap action "Game id on / off").');
    $('btnDcGameSave').onclick = async () => {
        try {
            await setGameIdCfg(brService);
            say('dcGameStatus', 'Saved for ' + gameid + '.');
        }
        catch (error) {
            say('dcGameStatus', 'Argh! ' + error);
        }
    };
}

export function btConn() {
    log('Requesting Bluetooth Device...');
    let name = '';
    let bdaddr = '';
    let app_ver = '';

    navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'BlueRetro' }], optionalServices: [brUuid[0]] })
    .then(device => {
        log('Connecting to GATT Server...');
        name = device.name;
        device.addEventListener('gattserverdisconnected', () => {
            log('> Bluetooth Device disconnected');
            $('divBtConn').style.display = 'block';
            $('divInfo').style.display = 'none';
            $('divDcPresets').style.display = 'none';
        });
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
        return getGameId(brService);
    })
    .then(value => {
        gameid = value;
        return fetch('map/default_pad.json');
    })
    .then(rsp => rsp.json())
    .then(data => {
        defaultMap = data.map;
        $('divInfo').textContent = 'Connected to: ' + name + ' (' + bdaddr + ') [' + app_ver + ']';
        $('divBtConn').style.display = 'none';
        $('divInfo').style.display = 'block';
        $('divDcPresets').style.display = 'block';
        initPresets();
        initGame();
    })
    .catch(error => {
        log('Argh! ' + error);
    });
}
