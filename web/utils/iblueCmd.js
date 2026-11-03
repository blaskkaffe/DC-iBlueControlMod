// Commands of the iBlueControlMod firmware additions (src: main/bluetooth/att_cfg.c in iBlueControlMod).
// A command is written to the command characteristic, the answer is read from it right after.
import { brUuid, cfg_cmd_get_iblue_info, cfg_cmd_get_gestures, cfg_cmd_set_gestures,
    cfg_cmd_get_profile, cfg_cmd_profile_save, cfg_cmd_profile_load, cfg_cmd_profile_delete,
    cfg_cmd_get_anim_cfg, cfg_cmd_set_anim_cfg, cfg_cmd_play_anim, cfg_cmd_reset_anim_cfg,
    cfg_cmd_get_motion_cfg, cfg_cmd_set_motion_cfg, cfg_cmd_motion_pose, cfg_cmd_get_motion_status, cfg_cmd_reset_motion_cal }
    from './constants.js';
import { ibluAnimFields } from './ibluAnim.js';
import { ibluMotionFields } from './ibluMotion.js';

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
        animation: (f & 8) !== 0,
        motion: (f & 16) !== 0,
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

/* The settings of the boot animation and the port LEDs, in the order of ibluAnimFields. A read answer is short
 * (the shortest ATT payload), so they come in two parts: fields 0 to 7, then 8 to 11. */
export async function getAnimCfg(service) {
    const values = [];

    for (const part of [0, 1]) {
        const v = await send(service, [cfg_cmd_get_anim_cfg, part], true);
        for (let i = 2; i + 1 < v.byteLength; i += 2) {
            values.push(v.getInt16(i, true));
        }
    }
    return values.slice(0, ibluAnimFields.length);
}

/* 0 = saved. The firmware clamps the numbers to its ranges. */
export async function setAnimCfg(service, values) {
    const bytes = [cfg_cmd_set_anim_cfg, 1];

    for (const x of values) {
        const u = x & 0xFFFF;
        bytes.push(u & 0xFF, u >> 8);
    }
    const v = await send(service, bytes, true);
    return v.getUint8(0);
}

export async function resetAnimCfg(service) {
    const v = await send(service, [cfg_cmd_reset_anim_cfg], true);
    return v.getUint8(0);
}

/* Play the whole boot animation now, from its start */
export async function playAnim(service) {
    await send(service, [cfg_cmd_play_anim], false);
}

/* The Joy-Con motion settings, in the order of ibluMotionFields (two parts, like the animation settings) */
export async function getMotionCfg(service) {
    const values = [];

    for (const part of [0, 1]) {
        const v = await send(service, [cfg_cmd_get_motion_cfg, part], true);
        for (let i = 2; i + 1 < v.byteLength; i += 2) {
            values.push(v.getInt16(i, true));
        }
    }
    return values.slice(0, ibluMotionFields.length);
}

/* 0 = saved */
export async function setMotionCfg(service, values) {
    const bytes = [cfg_cmd_set_motion_cfg, 1];

    for (const x of values) {
        const u = x & 0xFFFF;
        bytes.push(u & 0xFF, u >> 8);
    }
    const v = await send(service, bytes, true);
    return v.getUint8(0);
}

/* Take a calibration pose after `delay` seconds (hold the Joy-Cons still then). 0 = asked */
export async function motionPose(service, pose, delay) {
    const v = await send(service, [cfg_cmd_motion_pose, pose, delay], true);
    return v.getUint8(0);
}

export async function resetMotionCal(service) {
    const v = await send(service, [cfg_cmd_reset_motion_cal], true);
    return v.getUint8(0);
}

/* The Joy-Cons that are followed: side (-1 left, 1 right), flags, x and y (-127..127), height and heading in degrees */
export async function getMotionStatus(service) {
    const v = await send(service, [cfg_cmd_get_motion_status], true);
    const out = [];

    for (let i = 0; i < v.getUint8(0) && 1 + 6 * i + 5 < v.byteLength; i++) {
        const o = 1 + 6 * i;
        const f = v.getUint8(o + 1);
        out.push({ side: v.getInt8(o), forward: !!(f & 1), axis: !!(f & 2), shaking: !!(f & 4), range: !!(f & 8), failed: !!(f & 16),
            x: v.getInt8(o + 2), y: v.getInt8(o + 3), height: v.getInt8(o + 4), heading: v.getInt8(o + 5) * 2 });
    }
    return out;
}
