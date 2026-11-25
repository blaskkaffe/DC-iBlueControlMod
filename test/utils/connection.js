// One Bluetooth connection for the whole web config.
//
// The app page (index.html) connects once and keeps the connection in window.iblueConn. The BlueRetro pages it
// shows inside itself (mapping, files, update ...) are separate documents; they call connectBlueRetro() instead of
// asking the browser for a device again, and get the connection of the app page. A page opened on its own
// (without the app around it) still connects the way it always did.
import { brUuid } from './constants.js';

function sharedConnection() {
    try {
        if (window.parent !== window && window.parent.iblueConn && window.parent.iblueConn.service) {
            return window.parent.iblueConn;
        }
    }
    catch (e) {
        // a parent of another origin: not ours
    }
    return null;
}

/* True when the page runs inside the app page and that is connected. */
export function hasSharedConnection() {
    return sharedConnection() !== null;
}

/* Resolves to the BlueRetro GATT service. onDisconnected runs when the connection is lost; onDevice gets the
 * device (its name). */
export function connectBlueRetro(onDisconnected, onDevice) {
    const shared = sharedConnection();

    if (shared) {
        onDevice(shared.device);
        shared.onDisconnect(onDisconnected);
        return Promise.resolve(shared.service);
    }
    return navigator.bluetooth.requestDevice(
        {filters: [{namePrefix: 'BlueRetro'}],
        optionalServices: [brUuid[0]]})
    .then(device => {
        log('Connecting to GATT Server...');
        onDevice(device);
        device.addEventListener('gattserverdisconnected', onDisconnected);
        return device.gatt.connect();
    })
    .then(server => {
        log('Getting BlueRetro Service...');
        return server.getPrimaryService(brUuid[0]);
    });
}
