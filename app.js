// The iBlueControlMod web config: one page, one Bluetooth connection, the settings in categories.
import { brUuid } from './utils/constants.js';
import { getBdAddr } from './utils/getBdAddr.js';
import { getAppVersion } from './utils/getAppVersion.js';
import { getIblueInfo } from './utils/iblueCmd.js';
import { el } from './panels/ui.js';
import * as device from './panels/device.js';
import * as leds from './panels/leds.js';
import * as buttons from './panels/buttons.js';
import * as controllers from './panels/controllers.js';
import * as games from './panels/games.js';
import { makeFrames } from './panels/frames.js';

const $ = (id) => document.getElementById(id);

/* the connection, shared with the BlueRetro pages shown inside this one (utils/connection.js) */
const conn = window.iblueConn = {
    device: null, service: null, name: '', bdaddr: '', appVer: '', info: null,
    handlers: [],
    onDisconnect(fn) { if (!this.handlers.includes(fn)) { this.handlers.push(fn); } },
};

/* ChromeSamples and log() are what the BlueRetro modules write their progress to */
window.log = function () {
    const line = Array.prototype.slice.call(arguments).map((a) => (typeof a === 'string' ? a : JSON.stringify(a))).join(' ');
    const l = $('log');
    if (l) { l.textContent = line + '\n' + l.textContent; }
};

const icons = {
    chip: '<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    pointer: '<path d="M9 11V5a2 2 0 0 1 4 0v6M13 10V8a2 2 0 0 1 4 0v5a7 7 0 0 1-7 7h-1a6 6 0 0 1-5-3l-2-3a2 2 0 0 1 3-2l2 2"/>',
    gamepad: '<rect x="2" y="7" width="20" height="11" rx="4"/><path d="M7 10v5M4.5 12.5h5M15 11h.01M18 14h.01"/>',
    disc: '<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="2.5"/>',
    update: '<path d="M12 16V4M7 9l5-5 5 5M4 20h16"/>',
    sliders: '<path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0"/><circle cx="16" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="18" r="2"/>',
};

const update = makeFrames({ id: 'update', title: 'Update', icon: 'update', text: 'Install a new firmware: from a file, or one of the firmwares published on GitHub (the tested baseline is listed first).' },
    [{ label: 'Firmware', src: 'ota.html' }]);
const advanced = makeFrames({ id: 'advanced', title: 'Advanced', icon: 'sliders', text: 'The full BlueRetro configuration and tools. These are the standard BlueRetro pages, shown here so that one connection is enough.' },
    [{ label: 'Mapping', src: 'advance.html' }, { label: 'Input presets', src: 'presets.html' }, { label: 'Files', src: 'files.html' },
     { label: 'Dreamcast VMU', src: 'dc_vmu.html' }, { label: 'N64 Controller Pak', src: 'n64_ctrlpak.html', notDc: true }, { label: 'Debug', src: 'debug.html' }]);

const panels = [device, leds, buttons, controllers, games, update, advanced];

/* ---- messages ---- */
function toast(text, kind) {
    const t = el('div', { class: 'toast' + (kind ? ' ' + kind : ''), role: 'status', text: text });

    $('toasts').append(t);
    setTimeout(() => t.remove(), 3600);
}

function confirmAsk(text) {
    return Promise.resolve(window.confirm(text));
}

const ctx = { conn: conn, svc: () => conn.service, toast: toast, confirm: confirmAsk };

/* ---- the page ---- */
function build() {
    const nav = $('nav');
    const main = $('main');

    for (const p of panels) {
        const m = p.meta;
        const section = el('section', { id: 'p-' + m.id, 'aria-label': m.title });
        const gate = el('div', { class: 'gate' },
            el('div', { class: 'card empty' },
                el('h2', { text: 'Connect to the adapter' }),
                el('p', { text: 'Press Connect at the top and pick your BlueRetro. Everything here then works on that one connection; nothing else needs connecting.' }),
                el('button', { class: 'btn primary', text: 'Connect', onClick: connect })));
        const nope = el('div', { class: 'nope' },
            el('div', { class: 'banner warn' }, 'This firmware does not have the iBlueControlMod features. Install one that has under Update.'));
        const body = el('div', { class: 'body' });

        section.append(gate, nope, body);
        main.append(section);
        p.init(body, ctx);
        const a = el('a', { href: '#' + m.id, id: 'nav-' + m.id });
        a.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true">' + icons[m.icon] + '</svg>';
        a.append(m.title);
        nav.insertBefore(a, $('navTools'));
        if (m.id === 'games') { nav.insertBefore(el('div', { class: 'sep' }), $('navTools')); }
    }
}

function visibility() {
    for (const p of panels) {
        const s = $('p-' + p.meta.id);
        const needs = p.meta.needsIblue && conn.service && !conn.info;

        s.querySelector('.gate').style.display = conn.service ? 'none' : 'block';
        s.querySelector('.nope').style.display = needs ? 'block' : 'none';
        s.querySelector('.body').style.display = conn.service && !needs ? 'block' : 'none';
    }
}

let currentId = null;

function route() {
    const id = (location.hash || '#device').slice(1);
    const known = panels.find((p) => p.meta.id === id) || panels[0];

    currentId = known.meta.id;
    for (const p of panels) {
        const on = p.meta.id === currentId;

        $('p-' + p.meta.id).classList.toggle('on', on);
        const a = $('nav-' + p.meta.id);
        if (on) { a.setAttribute('aria-current', 'page'); } else { a.removeAttribute('aria-current'); }
    }
    document.title = known.meta.title + ' · iBlueControlMod';
    if (conn.service && known.opened) { known.opened(); }
    window.scrollTo(0, 0);
}

function chip() {
    const c = $('connChip');

    c.className = 'chip' + (conn.service ? ' ok' : '');
    c.textContent = conn.service ? conn.name : 'Not connected';
    $('btConn').textContent = conn.service ? 'Disconnect' : 'Connect';
    $('btConn').classList.toggle('primary', !conn.service);
}

/* ---- connecting ---- */
function lost() {
    conn.service = null;
    conn.info = null;
    chip();
    visibility();
    for (const p of panels) { p.disconnected(); }
    for (const fn of conn.handlers.splice(0)) {
        try { fn(); } catch (e) { /* a page that is gone */ }
    }
    toast('The adapter disconnected.', 'bad');
}

async function connect() {
    if (conn.service) {
        if (conn.device && conn.device.gatt && conn.device.gatt.connected) { conn.device.gatt.disconnect(); }
        return;
    }
    if (!navigator.bluetooth) {
        toast('This browser has no Web Bluetooth. Use Chrome or Edge, over https.', 'bad');
        return;
    }
    try {
        log('Requesting Bluetooth Device...');
        const dev = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'BlueRetro' }], optionalServices: [brUuid[0]] });

        dev.addEventListener('gattserverdisconnected', lost);
        log('Connecting to GATT Server...');
        const server = await dev.gatt.connect();
        log('Getting BlueRetro Service...');
        const service = await server.getPrimaryService(brUuid[0]);
        const bdaddr = await getBdAddr(service);
        let appVer = '';

        try { appVer = await getAppVersion(service); }
        catch (e) { appVer = ''; }
        const info = await getIblueInfo(service);

        conn.device = dev;
        conn.name = dev.name;
        conn.bdaddr = bdaddr;
        conn.appVer = appVer;
        conn.info = info;
        conn.service = service;
        chip();
        visibility();
        for (const p of panels) {
            try { await p.connected(); }
            catch (e) { log('Argh! ' + p.meta.id + ': ' + e); }
        }
        const here = panels.find((p) => p.meta.id === currentId);
        if (here && here.opened) { here.opened(); }
        toast('Connected to ' + conn.name + '.');
    }
    catch (error) {
        log('Argh! ' + error);
        if (!(error && error.name === 'NotFoundError')) { toast('Could not connect: ' + (error && error.message || error), 'bad'); }
    }
}

/* ---- theme ---- */
function theme(next) {
    const root = document.documentElement;

    if (next) {
        root.setAttribute('data-theme', next);
        try { localStorage.setItem('iblueTheme', next); } catch (e) { /* not kept */ }
    }
    window.dispatchEvent(new Event('themechange'));
}

try {
    const saved = localStorage.getItem('iblueTheme');
    if (saved) { document.documentElement.setAttribute('data-theme', saved); }
}
catch (e) { /* no storage */ }

build();
chip();
visibility();
$('btConn').addEventListener('click', connect);
$('themeBtn').addEventListener('click', () => {
    const dark = getComputedStyle(document.documentElement).colorScheme.indexOf('dark') !== -1;
    theme(dark ? 'light' : 'dark');
});
window.addEventListener('themechange', () => { if (leds.redraw) { leds.redraw(); } });
window.addEventListener('hashchange', route);
route();
