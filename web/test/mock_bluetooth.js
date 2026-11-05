// Injected into the page before it loads: a BlueRetro that answers like the iBlueControlMod firmware
// (main/bluetooth/att_cfg.c, system/iblue_gesture.c). window.__br is its state.
(function (uuids, opts) {
    const idx = (u) => uuids.indexOf(u);
    const br = window.__br = {
        iblue: opts.iblue,           // false: any other BlueRetro firmware
        fwName: opts.fwName,
        fwVer: opts.fwVer,
        gestures: [0, 3, 0],         // none, next colour, none
        lastCmd: 0, result: 0,
        ota: { started: false, ended: false, aborted: false, bytes: [], chunks: 0 },
        log: [],
    };
    const ACTIONS = 5;
    const dv = (arr) => new DataView(Uint8Array.from(arr).buffer);
    const str = (s) => Array.from(s).map((c) => c.charCodeAt(0));

    function command(bytes) {
        const c = bytes[0];
        br.lastCmd = c;
        br.log.push(Array.from(bytes));
        if (c === 0xA5) { br.ota.started = true; br.ota.bytes = []; br.ota.chunks = 0; }
        if (c === 0x5A) { br.ota.ended = true; }
        if (c === 0xDE) { br.ota.aborted = true; }
        if (!br.iblue) { return; }
        if (c === 0x42) {
            const ok = bytes.length > 3 && bytes.slice(1, 4).every((a) => a < ACTIONS);
            if (ok) { br.gestures = Array.from(bytes.slice(1, 4)); }
            br.result = ok ? 0 : 2;
        }
    }

    function answer() {
        const c = br.lastCmd;
        if (c === 0x07) { return dv(str(br.fwName)); }
        if (br.iblue) {
            if (c === 0x40) { return dv([0x69, 0x42, 0x4C, 0x55, 1, 5, 3, 0, ACTIONS]); }
            if (c === 0x41) { return dv(br.gestures); }
            if (c === 0x42) { return dv([br.result]); }
        }
        return dv([2]); // anything else: the ABI version, as the firmware does for a command it does not know
    }

    const chrc = (u) => ({
        async writeValue(v) {
            const bytes = new Uint8Array(v.buffer ? v.buffer.slice(v.byteOffset, v.byteOffset + v.byteLength) : v);
            if (u === idx(uuids[7])) { command(bytes); }
            else if (u === 8) { br.ota.chunks++; br.ota.bytes.push(...bytes); }
        },
        async readValue() {
            if (u === 7) { return answer(); }
            if (u === 9) { return dv(str(br.fwVer)); }
            if (u === 12) { return dv([1, 2, 3, 4, 5, 6]); }
            if (u === 6) { return dv([2]); }
            return dv([0]);
        },
    });
    const service = { async getCharacteristic(uuid) { return chrc(idx(uuid)); } };
    const server = { async getPrimaryService() { return service; } };
    const device = { name: 'BlueRetro', addEventListener() {}, gatt: { connect: async () => server } };
    Object.defineProperty(navigator, 'bluetooth', { value: { requestDevice: async () => device }, configurable: true });
})(window.__uuids, window.__opts);
