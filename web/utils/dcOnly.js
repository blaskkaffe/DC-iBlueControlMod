// "Dreamcast only" setting of the iBlueControlMod web config: hides what only other consoles use.
// Kept in this browser (localStorage); on unless the user switched it off.
const KEY = 'iblueDcOnly';
export const dcLabelIdx = 38;     // labelName: "Dreamcast"
export const dcSystemName = 'DC'; // systemCfg

export function isDcOnly() {
    try { return localStorage.getItem(KEY) !== '0'; }
    catch (e) { return true; }
}

export function setDcOnly(on) {
    try { localStorage.setItem(KEY, on ? '1' : '0'); }
    catch (e) { /* the setting just does not stick */ }
}
