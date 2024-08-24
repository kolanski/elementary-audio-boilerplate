import {Renderer, el} from '@elemaudio/core';
console.log("");

// Initialize a volume variable with the default value
let currentVolume = 1.0;

// Create a handler for the Proxy to watch for changes
const volumeHandler = {
    set(target, property, value) {
        target[property] = value;
        return true;
    }
};

// Create a Proxy for currentVolume
let volumeProxy = new Proxy({ value: currentVolume }, volumeHandler);

// Create the Renderer instance
let core = new Renderer(__getSampleRate__(), (batch) => {
    __postNativeMessage__(JSON.stringify(batch));
});

// Variable to store the previous rendering nodes
let previousNodes = null;

// Asynchronous function to render audio with the current volume
async function renderAudioLoop() {
    while (volumeProxy.value > 0) {
        // Stop the previous rendering nodes if they exist
        if (previousNodes) {
            previousNodes.forEach(node => node.stop());
        }

        // Render the new audio
        let nodes = [
            el.mul(volumeProxy.value, el.cycle(440)),
            el.mul(volumeProxy.value, el.cycle(441))
        ];
        let stats = core.render(...nodes);

        // Store the current nodes as previous nodes for the next render
        previousNodes = nodes;

        console.log(stats);

        // Decrease the volume
        volumeProxy.value = Math.max(0, volumeProxy.value - 0.1);

        // Non-blocking delay for 1 second
        await new Promise(resolve => setTimeout(resolve, 1000));
    }
}

// Start the asynchronous rendering loop
renderAudioLoop();