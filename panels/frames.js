// A category made of the BlueRetro pages (update, mapping, files ...): each is shown inside the app, on the
// connection of the app (utils/connection.js), when its tab is opened.
import { isDcOnly } from '../utils/dcOnly.js';
import { el, page } from './ui.js';

export function makeFrames(meta, tabs) {
    let root;
    let ctx;
    let wrap;
    let tabBar;
    let current = null;
    const frames = new Map();

    function dcOnlyChanged() {
        for (const b of tabBar.children) {
            b.style.display = (tabs[Number(b.dataset.i)].notDc && isDcOnly()) ? 'none' : '';
        }
        /* the pages inside read the setting when they open: open them again */
        for (const f of frames.values()) { f.remove(); }
        frames.clear();
        if (current !== null && tabs[current].notDc && isDcOnly()) { current = 0; }
        if (current !== null && ctx.conn.service) { show(current); }
    }

    function show(i) {
        current = i;
        for (const b of tabBar.children) { b.setAttribute('aria-selected', String(Number(b.dataset.i) === i)); }
        for (const [k, f] of frames) { f.style.display = k === i ? 'block' : 'none'; }
        if (!frames.has(i) && ctx.conn.service) {
            const f = el('iframe', { src: tabs[i].src + '?embed=1', title: tabs[i].label, id: 'frame-' + meta.id + '-' + i });
            frames.set(i, f);
            wrap.append(f);
        }
        if (frames.has(i)) { frames.get(i).style.display = 'block'; }
    }

    return {
        meta: meta,
        init(r, c) {
            root = r;
            ctx = c;
            tabBar = el('div', { class: 'tabs', role: 'tablist' },
                tabs.map((t, i) => el('button', { role: 'tab', 'data-i': i, id: 'tab-' + meta.id + '-' + i, text: t.label, 'aria-selected': 'false', onClick: () => show(i) })));
            wrap = el('div', { class: 'frame-wrap' });
            root.append(page(meta.title, meta.text), tabs.length > 1 ? tabBar : null, wrap);
            if (tabs.length === 1) { tabBar.style.display = 'none'; root.append(tabBar); }
            window.addEventListener('dconlychange', dcOnlyChanged);
            dcOnlyChanged();
        },
        /* the category was opened */
        opened() { if (current === null) { show(0); } else { show(current); } },
        connected() { if (current !== null) { show(current); } },
        disconnected() { for (const f of frames.values()) { f.remove(); } frames.clear(); },
    };
}
