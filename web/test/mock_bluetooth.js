// Injected into the page before it loads: a BlueRetro that answers like the iBlueControlMod firmware
// (main/bluetooth/att_cfg.c, system/iblue_gesture.c, system/iblue_profile.c). window.__br is its state.
(function (uuids, opts) {
    const idx = (u) => uuids.indexOf(u);
    const br = window.__br = {
        iblue: opts.iblue,           // false: any other BlueRetro firmware
        fwName: opts.fwName,
        fwVer: opts.fwVer,
        gestures: [0, 3, 0],         // none, next colour, none
        profiles: [null, null, null, null],
        lastCmd: 0, result: 0, slot: 0,
        ota: { started: false, ended: false, aborted: false, bytes: [], chunks: 0 },
        log: [],
        writes: [],
        anim: opts.animDefaults.slice(), animPart: 0, played: 0, animResult: 0,
    };
    const ACTIONS = 11;
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
        if (c === 0x43) { br.slot = bytes[1]; }
        if (c === 0x47) { br.animPart = bytes[1] ? 1 : 0; }
        if (c === 0x48) {
            br.animResult = 2;
            if (bytes.length >= 2 + 24 && bytes[1] === 1) {
                const v = [];
                for (let i = 0; i < 12; i++) {
                    let x = bytes[2 + 2 * i] | (bytes[3 + 2 * i] << 8);
                    if (x & 0x8000) { x -= 0x10000; }
                    v.push(Math.min(opts.animMax[i], Math.max(opts.animMin[i], x))); /* the firmware clamps */
                }
                br.anim = v;
                br.animResult = 0;
            }
        }
        if (c === 0x49) { br.played++; }
        if (c === 0x4A) { br.anim = opts.animDefaults.slice(); br.animResult = 0; }
        if (c === 0x44) {
            br.result = 2;
            if (bytes[1] < 4) {
                const name = String.fromCharCode(...bytes.slice(2, 17)) || ('Profile ' + (bytes[1] + 1));
                br.profiles[bytes[1]] = name;
                br.result = 0;
            }
        }
        if (c === 0x45) { br.result = (bytes[1] < 4) ? (br.profiles[bytes[1]] ? 0 : 1) : 2; if (br.result === 0) { br.loaded = bytes[1]; } }
        if (c === 0x46) { br.result = bytes[1] < 4 ? 0 : 2; if (bytes[1] < 4) { br.profiles[bytes[1]] = null; } }
    }

    function answer() {
        const c = br.lastCmd;
        if (c === 0x07) { return dv(str(br.fwName)); }
        if (br.iblue) {
            if (c === 0x40) { return dv([0x69, 0x42, 0x4C, 0x55, 1, 15, 3, 4, ACTIONS]); }
            if (c === 0x47) {
                const part = br.animPart ? br.anim.slice(8) : br.anim.slice(0, 8);
                const out = [1, br.animPart];
                part.forEach((x) => { out.push(x & 0xFF, (x >> 8) & 0xFF); });
                return dv(out);
            }
            if (c === 0x41) { return dv(br.gestures); }
            if (c === 0x43) {
                const n = br.profiles[br.slot];
                const name = str(n || '');
                while (name.length < 16) { name.push(0); }
                return dv([n ? 1 : 0, br.slot, ...name]);
            }
            if (c === 0x42 || c === 0x44 || c === 0x45 || c === 0x46) { return dv([br.result]); }
            if (c === 0x48 || c === 0x4A) { return dv([br.animResult]); }
        }
        return dv([2]); // anything else: the ABI version, as the firmware does for a command it does not know
    }

    const chrc = (u) => ({
        async writeValue(v) {
            const bytes = new Uint8Array(v.buffer ? v.buffer.slice(v.byteOffset, v.byteOffset + v.byteLength) : v);
            if (u === idx(uuids[7])) { command(bytes); }
            else if (u === 2 || u === 3 || u === 4 || u === 5) { br.writes.push([u, Array.from(bytes)]); }
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
