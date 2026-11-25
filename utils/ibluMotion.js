// Settings of the Joy-Con motion of the iBlueControlMod firmware (docs/samba-de-amigo.md).
// The table is the same as `struct iblue_motion_cfg` in main/iblue/iblue_motion.h (order, defaults, ranges);
// test/web_test.js compares the two when the firmware source is next to this folder.

export const ibluMotionFields = [
    { key: 'mode', min: 0, max: 1, def: 0 },
    { key: 'el_range', label: 'Height range', unit: '° from level', min: 10, max: 90, def: 45, step: 1,
      hint: 'How far a Joy-Con is tilted up or down from level to give the highest and the lowest position.' },
    { key: 'yaw_range', label: 'Sideways range', unit: '° from forward', min: 10, max: 120, def: 45, step: 1,
      hint: 'How far it is turned left or right from forward for the full sideways position. The out pose of the calibration sets this for you.' },
    { key: 'use_x', label: 'Report the sideways position', min: 0, max: 1, def: 1, kind: 'check',
      hint: 'Off: the sideways position stays in the middle and only the height counts.' },
    { key: 'shake_mg', label: 'Shake strength', unit: 'mg', min: 300, max: 4000, def: 1200, step: 50,
      hint: 'How hard a sudden movement has to be to count as a shake. Lower is more sensitive.' },
    { key: 'shake_ms', label: 'Shake lasts', unit: 'ms', min: 20, max: 300, def: 80, step: 5,
      hint: 'How long the shake button stays pressed.' },
    { key: 'lockout_ms', label: 'Pause after a shake', unit: 'ms', min: 0, max: 500, def: 120, step: 10,
      hint: 'No new shake for this long, so one swing does not count twice.' },
    { key: 'y_invert', label: 'Turn the height around', min: 0, max: 1, def: 0, kind: 'check', hint: '' },
    { key: 'x_invert', label: 'Turn the sideways position around', min: 0, max: 1, def: 0, kind: 'check', hint: '' },
    { key: 'yaw_leak', label: 'Drift back to forward', unit: '%', min: 0, max: 100, def: 0, step: 1,
      hint: 'The gyro slowly drifts. Above 0 the sideways position creeps back to forward; 0 keeps it until you calibrate again.' },
    { key: 'filter_ms', label: 'Smoothing', unit: 'ms', min: 0, max: 300, def: 40, step: 5,
      hint: 'Smooths the position. Higher is steadier but slower.' },
    { key: 'cal_button', label: 'Hold Minus (left) or Home (right) for a second to set forward', min: 0, max: 1, def: 1, kind: 'check', hint: '' },
];

export function ibluMotionDefaults() {
    return ibluMotionFields.map((f) => f.def);
}

export function ibluMotionClamp(values) {
    return values.map((v, i) => Math.min(ibluMotionFields[i].max, Math.max(ibluMotionFields[i].min, Math.round(v))));
}

// The calibration poses, in the order of the firmware command (enum in iblue_motion.h)
export const ibluMotionPoses = [
    { id: 0, name: 'Forward', delay: 5,
      text: 'Point both Joy-Cons at the TV, level, and hold them still. This is the middle position. Do it each time you start playing (or hold Minus / Home for a second on a Joy-Con).' },
    { id: 1, name: 'Up', delay: 5,
      text: 'Point the Joy-Cons straight up and hold still. Once is enough: the adapter remembers which way each Joy-Con points. Take Forward first.' },
    { id: 2, name: 'Down', delay: 5,
      text: 'Point them straight down and hold still. Makes the pointing direction more exact (optional if you took Up).' },
    { id: 3, name: 'Out', delay: 5,
      text: 'From the forward position, turn the left Joy-Con out to the left and the right one out to the right, as far as you want to reach, level, and hold still. Sets the sideways range and which way is left.' },
];
