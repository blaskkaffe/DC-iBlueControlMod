// Commands of the iBlueControlMod firmware additions (src: main/bluetooth/att_cfg.c in iBlueControlMod).
// A command is written to the command characteristic, the answer is read from it right after.
import { brUuid, cfg_cmd_get_iblue_info, cfg_cmd_get_gestures, cfg_cmd_set_gestures }
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
        gameId: (f & 4) !== 0,
        gestureCount: v.getUint8(6),
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
