// Settings of the boot animation and of the port LEDs of the iBlueControlMod firmware.
// The table is the same as `struct iblue_anim_cfg` in main/system/iblue_anim_curve.h (order, defaults, ranges);
// test/web_test.js compares the two when the firmware source is next to this folder.

export const ibluAnimFields = [
    { key: 'total_s', label: 'Length of the boot animation', unit: 's', min: 5, max: 60, def: 15, step: 1,
      hint: 'Counted from the console\'s power-on: the fireworks (3 s) and then the drops.' },
    { key: 'ctrl_pct', label: 'Brightness of a connected controller', unit: '%', min: 0, max: 100, def: 20, step: 1,
      hint: 'The port LED of a controller that is connected.' },
    { key: 'led_dist', label: 'LED distance', unit: '%', min: 30, max: 300, def: 100, step: 5,
      hint: 'How far apart the four LEDs sit in the water. 100 % is one LED spacing.' },
    { key: 'tank_len', label: 'Length between the outer walls', unit: '% of a spacing', min: 100, max: 1500, def: 400, step: 10,
      hint: 'The tank the ripples run in: they bounce back from the walls. 400 % is four LED spacings.' },
    { key: 'group_pos', label: 'Position of the LED group', unit: '% of a spacing', min: -500, max: 500, def: 0, step: 10,
      hint: 'Shifts the four LEDs left (negative) or right (positive) in the tank.' },
    { key: 'density', label: 'Water density', unit: '%', min: 30, max: 300, def: 100, step: 5,
      hint: 'Heavier water: the ripples travel slower and die out sooner.' },
    { key: 'depth', label: 'Water depth (brightness)', unit: '%', min: 20, max: 200, def: 100, step: 5,
      hint: 'Brightness of the water and its ripples. The LEDs never get brighter than the firmware\'s maximum.' },
    { key: 'level_low', label: 'Water level at the start', unit: '%', min: 0, max: 100, def: 20, step: 1,
      hint: 'General brightness of calm water when the drops begin.' },
    { key: 'level_high', label: 'Water level when risen', unit: '%', min: 0, max: 100, def: 42, step: 1,
      hint: 'General brightness after the water has risen.' },
    { key: 'rise_at', label: 'The water starts to rise at', unit: '% of the drops', min: 0, max: 90, def: 30, step: 1, hint: '' },
    { key: 'rise_len', label: 'The rise takes', unit: '% of the drops', min: 1, max: 60, def: 20, step: 1, hint: '' },
    { key: 'fade_at', label: 'The water goes down from', unit: '% of the drops', min: 10, max: 95, def: 45, step: 1,
      hint: 'From here the level and the light go down to nothing at the end.' },
];

export function ibluAnimDefaults() {
    return ibluAnimFields.map((f) => f.def);
}

export function ibluAnimClamp(values) {
    return values.map((v, i) => Math.min(ibluAnimFields[i].max, Math.max(ibluAnimFields[i].min, Math.round(v))));
}

const clamp01 = (x) => Math.min(1, Math.max(0, x));
const smoother = (x) => { x = clamp01(x); return x * x * x * (x * (x * 6 - 15) + 10); };
const val = (values, key) => values[ibluAnimFields.findIndex((f) => f.key === key)];

/* General brightness of the water at u (0 to 1 over the drops): the curve of the firmware
 * (iblue_anim_curve.c: rest_level and master), times the depth. */
export function ibluAnimLevel(values, u) {
    const low = val(values, 'level_low') / 100;
    const high = val(values, 'level_high') / 100;
    const riseAt = val(values, 'rise_at') / 100;
    const riseLen = val(values, 'rise_len') / 100;
    const fadeAt = val(values, 'fade_at') / 100;
    const rest = low + (high - low) * smoother((u - riseAt) / riseLen);
    const master = clamp01(smoother(u / 0.16) * (1 - smoother((u - fadeAt) / (1 - fadeAt))));

    return clamp01(rest * master * val(values, 'depth') / 100);
}

/* Where the four LEDs sit in the tank, in LED spacings from the left wall, and the tank length. */
export function ibluAnimTank(values) {
    const width = val(values, 'tank_len') / 100;
    const dist = val(values, 'led_dist') / 100;
    const x0 = (width - 3 * dist) / 2 + val(values, 'group_pos') / 100;
    const leds = [0, 1, 2, 3].map((i) => Math.min(width - 0.02, Math.max(0.02, x0 + i * dist)));

    return { width: width, leds: leds };
}
