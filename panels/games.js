// Games: the configuration a game gets by itself.
import { getGameId } from '../utils/getGameId.js';
import { setGameIdCfg } from '../utils/setGameIdCfg.js';
import { el, card, say, page } from './ui.js';

let ctx;
let gameText;
let saveBtn;
let status;

export const meta = { id: 'games', title: 'Games', icon: 'disc',
    text: 'A game can have a configuration of its own: its controller type and mapping load by themselves when the game starts, and other games use the normal one.' };

export function init(root, c) {
    ctx = c;
    gameText = el('p', { id: 'dcGameText', text: '' });
    status = el('span', { class: 'status', id: 'dcGameStatus' });
    saveBtn = el('button', { class: 'btn primary', id: 'btnDcGameSave', text: 'Save the current configuration for this game', onClick: save });
    root.append(
        page(meta.title, meta.text),
        card('The game that runs now', 'The game is announced by openMenu when it starts one (the Dreamcast itself tells the adapter nothing). The adapter needs the VMU accessory on for that.',
            gameText,
            el('div', { class: 'actions', id: 'divDcGame' },
                saveBtn,
                el('button', { class: 'btn', id: 'btnGameRefresh', text: 'Check again', onClick: refresh }), status)),
        card('How it works', null,
            el('ol', {},
                el('li', { text: 'Start the game from openMenu. The game id shows here.' }),
                el('li', { text: 'Under Controllers, pick the type the game needs (for example 6 button + dual stick) and, under Advanced, change the mapping if you want.' }),
                el('li', { text: 'Come back here and save: the adapter loads that configuration every time the game starts.' }),
                el('li', { text: 'Game ids can be switched off with the button: give a tap the action "Game id on / off" under Buttons.' }))));
}

async function refresh() {
    try {
        const id = await getGameId(ctx.svc());

        gameText.textContent = id ? 'Game id: ' + id : 'No game announced yet.';
        saveBtn.disabled = !id;
        say(status, '');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

async function save() {
    try {
        await setGameIdCfg(ctx.svc());
        say(status, 'Saved for this game.', 'ok');
        ctx.toast('Saved for this game.');
    }
    catch (error) {
        say(status, 'Argh! ' + error, 'bad');
    }
}

export function connected() { return refresh(); }
export function disconnected() { gameText.textContent = ''; }
