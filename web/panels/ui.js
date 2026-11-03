// Small helpers to build the panels without a framework.

/* el('div', { class: 'card', onClick: fn }, child, 'text', ...) */
export function el(tag, props, ...kids) {
    const n = document.createElement(tag);

    for (const [k, v] of Object.entries(props || {})) {
        if (v === false || v === null || v === undefined) { continue; }
        if (k === 'class') { n.className = v; }
        else if (k === 'text') { n.textContent = v; }
        else if (k.startsWith('on')) { n.addEventListener(k.slice(2).toLowerCase(), v); }
        else { n.setAttribute(k, v === true ? '' : v); }
    }
    for (const kid of kids.flat()) {
        if (kid !== null && kid !== undefined && kid !== false) { n.append(kid); }
    }
    return n;
}

/* A card with a heading and an optional line under it. Returns the card; put the content in it. */
export function card(title, sub, ...kids) {
    return el('div', { class: 'card' },
        el('h2', { text: title }),
        sub ? el('p', { class: 'sub', text: sub }) : null,
        ...kids);
}

/* The line a button's result goes to: kind 'ok', 'bad' or '' */
export function say(node, text, kind) {
    node.textContent = text;
    node.className = 'status' + (kind ? ' ' + kind : '');
}

export function page(title, text) {
    return el('div', { class: 'page-head' }, el('h1', { text: title }), text ? el('p', { text: text }) : null);
}
