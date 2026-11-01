// Commands of the iBlueControlMod firmware additions (src: main/bluetooth/att_cfg.c in iBlueControlMod).
// A command is written to the command characteristic, the answer is read from it right after.
import { brUuid, cfg_cmd_get_iblue_info, cfg_cmd_get_gestures, cfg_cmd_set_gestures,
    cfg_cmd_get_profile, cfg_cmd_profile_save, cfg_cmd_profile_load, cfg_cmd_profile_delete }
    from './constants.js';

async function send(service, bytes, readAnswer) {
    const chrc = await service.getCharacteristic(brUuid[7]);
    await chrc.writeValue(Uint8Array.from(bytes));
    return readAnswer ? chrc.readValue() : null;
}

/* Features of the firmware, or null when it is not an iBlueControlMod build (it then answers a read with the
 * one byte ABI version, never the "iBLU" marker). */
export async function getIblueInfo(service) {
    const v = await send(service, [cfg_cmd_get_iblue_info], true);
    if (v.byteLength < 9 || String.fromCharCode(v.getUint8(0), v.getUint8(1), v.getUint8(2), v.getUint8(3)) !== 'iBLU') {
        return null;
    }
    const f = v.getUint8(5);
    return {
        version: v.getUint8(4),
        gestures: (f & 1) !== 0,
        profiles: (f & 2) !== 0,
        gameId: (f & 4) !== 0,
        gestureCount: v.getUint8(6),
        profileCount: v.getUint8(7),
        actionCount: v.getUint8(8),
    };
}

export async function getGestures(service, count) {
    const v = await send(service, [cfg_cmd_get_gestures], true);
    const out = [];
    for (let i = 0; i < count && i < v.byteLength; i++) {
        out.push(v.getUint8(i));
    }
    return out;
}

/* 0 = saved */
export async function setGestures(service, actions) {
    const v = await send(service, [cfg_cmd_set_gestures, ...actions], true);
    return v.getUint8(0);
}

export async function getProfile(service, slot) {
    const v = await send(service, [cfg_cmd_get_profile, slot], true);
    let name = '';
    for (let i = 2; i < v.byteLength && v.getUint8(i) !== 0; i++) {
        name += String.fromCharCode(v.getUint8(i));
    }
    return { slot: v.getUint8(1), used: v.getUint8(0) === 1, name: name };
}

/* The name is at most 15 characters, ASCII */
export async function saveProfile(service, slot, name) {
    const bytes = [];
    for (const ch of name.replace(/[^\x20-\x7e]/g, '?').slice(0, 15)) {
        bytes.push(ch.charCodeAt(0));
    }
    const v = await send(service, [cfg_cmd_profile_save, slot, ...bytes], true);
    return v.getUint8(0);
}

export async function loadProfile(service, slot) {
    const v = await send(service, [cfg_cmd_profile_load, slot], true);
    return v.getUint8(0);
}

export async function deleteProfile(service, slot) {
    const v = await send(service, [cfg_cmd_profile_delete, slot], true);
    return v.getUint8(0);
}
