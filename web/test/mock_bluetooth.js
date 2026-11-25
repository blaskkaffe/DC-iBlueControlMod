// Injected into the page before it loads: a BlueRetro that answers like the iBlueControlMod firmware
// (main/bluetooth/att_cfg.c, iblue/iblue_gesture.c). window.__br is its state.
(function (uuids, opts) {
    const idx = (u) => uuids.indexOf(u);
    const br = window.__br = {
        iblue: opts.iblue,           // false: any other BlueRetro firmware
        fwName: opts.fwName,
        fwVer: opts.fwVer,
        gestures: [1, 3, 0],         // pair, next colour, none
        profiles: [null, null, null, null],
        lastCmd: 0, result: 0, slot: 0,
        ota: { started: false, ended: false, aborted: false, bytes: [], chunks: 0 },
        log: [],
        writes: [],
        modes: [0, 0, 0, 0], outSel: 0,
        motion: opts.motionDefaults.slice(), motionPart: 0, motionResult: 0, poses: [], motionReset: 0,
    };
    const ACTIONS = 6;
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
        if (c === 0x4B) { br.motionPart = bytes[1] ? 1 : 0; }
        if (c === 0x4C) {
            br.motionResult = 2;
            if (bytes.length >= 2 + 24 && bytes[1] === 1) {
                const v = [];
                for (let i = 0; i < 12; i++) {
                    let x = bytes[2 + 2 * i] | (bytes[3 + 2 * i] << 8);
                    if (x & 0x8000) { x -= 0x10000; }
                    v.push(Math.min(opts.motionMax[i], Math.max(opts.motionMin[i], x))); /* the firmware clamps */
                }
                br.motion = v;
                br.motionResult = 0;
            }
        }
        if (c === 0x4D) { br.motionResult = (bytes.length > 2 && bytes[1] < 4) ? 0 : 2; if (br.motionResult === 0) { br.poses.push([bytes[1], bytes[2]]); } }
        if (c === 0x4F) { br.motionReset++; br.motionResult = 0; }
        if (c === 0x52) { if (bytes.length >= 3) { br.pcOff = bytes[1] | (bytes[2] << 8); } else { br.pcRuns = (br.pcRuns || 0) + 1; br.pcOff = 0; } }
    }

    function answer() {
        const c = br.lastCmd;
        if (c === 0x07) { return dv(str(br.fwName)); }
        if (br.iblue) {
            if (c === 0x40) { return dv([0x69, 0x42, 0x4C, 0x55, 1, 149, 3, 0, ACTIONS]); }
            if (c === 0x4B) {
                const part = br.motionPart ? br.motion.slice(8) : br.motion.slice(0, 8);
                const out = [1, br.motionPart];
                part.forEach((x) => { out.push(x & 0xFF, (x >> 8) & 0xFF); });
                return dv(out);
            }
            if (c === 0x50) { const o = [4]; br.modes.forEach((m) => o.push(1, m, 3, 0, 0)); return dv(o); }
            if (c === 0x4C || c === 0x4D || c === 0x4F) { return dv([br.motionResult]); }
            if (c === 0x4E) { return dv([2, 0xFF, 3, 10, 20, 17, 8, 1, 7, 246, 0, 255, 6]); } // left: forward + axis, right: shaking
            if (c === 0x52) {
                const text = 'iBlue port check 1\nport D: adapter quiet, sense GPIO33=0 (plugged)\n'.repeat(3);
                const t = Array.from(text).map((ch) => ch.charCodeAt(0));
                return dv([br.pcRuns ? 2 : 0, t.length & 0xFF, t.length >> 8, ...t.slice(br.pcOff || 0, (br.pcOff || 0) + 17)]);
            }
            if (c === 0x41) { return dv(br.gestures); }
            if (c === 0x42) { return dv([br.result]); }
        }
        return dv([2]); // anything else: the ABI version, as the firmware does for a command it does not know
    }

    const chrc = (u) => ({
        async writeValue(v) {
            const bytes = new Uint8Array(v.buffer ? v.buffer.slice(v.byteOffset, v.byteOffset + v.byteLength) : v);
            if (u === idx(uuids[7])) { command(bytes); }
            else if (u === 2 || u === 3 || u === 4 || u === 5) {
                br.writes.push([u, Array.from(bytes)]);
                if (u === 2) { br.outSel = bytes[0]; }
                if (u === 3) { br.modes[br.outSel] = bytes[0]; }
            }
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
    Object.defineProperty(navigator, 'bluetooth', { value: { requestDevice: async () => { window.__requests = (window.__requests || 0) + 1; return device; } }, configurable: true });
})(window.__uuids, window.__opts);
