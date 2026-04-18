(function() {
    'use strict';

    // Hide patched functions from being detected via toString()
    const rawToString = Function.prototype.toString;
    const patchedSet = new WeakSet();
    Function.prototype.toString = function() {
        if (patchedSet.has(this)) return `function ${this.name}() { [native code] }`;
        return rawToString.apply(this, arguments);
    };
    const seal = (fn) => { patchedSet.add(fn); return fn; };

    const mask = (obj, prop, val) => {
        try {
            Object.defineProperty(obj, prop, {
                get: seal(function() { return val; }),
                                  configurable: true
            });
        } catch (e) {}
    };

    // Standardize hardware specs to reduce entropy
    mask(navigator, 'hardwareConcurrency', 2); // CPU_CORE2
    mask(navigator, 'deviceMemory', 4); // RAM4

    const fakeArray = Object.freeze([]);

    // Disable plugin enumeration
    Object.defineProperty(Navigator.prototype, "plugins", {
        get: seal(function() { return fakeArray; }),
                          configurable: true
    });

    // Remove modern APIs that leak device and network state
    if (navigator.userAgentData) {
        delete Navigator.prototype.userAgentData;
    }
    if (navigator.connection) {
        delete Navigator.prototype.connection;
    }
    if (navigator.getBattery) {
        delete Navigator.prototype.getBattery;
    }

    // Block font-based tracking
    if (document.fonts) {
        Object.defineProperty(document, 'fonts', {
            get: seal(function() { return undefined; })
        });
    }

    // Spoof text metrics to prevent font fingerprinting
    CanvasRenderingContext2D.prototype.measureText = seal(function(txt) {
        return { width: 0, actualBoundingBoxLeft: 0, actualBoundingBoxRight: 0 };
    });

    // Block WebGL debug extensions that reveal GPU model
    const patchGL = (proto) => {
        if (!proto) return;
        const oldGetExt = proto.getExtension;
        proto.getExtension = seal(function(name) {
            const sensitive = ['WEBGL_debug_renderer_info', 'WEBGL_debug_shaders'];
            if (sensitive.includes(name)) return null;
            return oldGetExt.apply(this, arguments);
        });
    };
    patchGL(WebGLRenderingContext.prototype);
    patchGL(WebGL2RenderingContext.prototype);

    const origGetImageData = CanvasRenderingContext2D.prototype.getImageData;

    // Inject noise into canvas image data to disrupt hash consistency
    CanvasRenderingContext2D.prototype.getImageData = seal(function(x, y, w, h) {
        const img = origGetImageData.apply(this, arguments);
        const data = img.data;

        for (let i = 0; i < data.length; i += 4) {
            data[i] ^= 1;       // R
            data[i + 1] ^= 1;   // G
            data[i + 2] ^= 1;   // B

            if (data[i + 3] === 255) {
                data[i + 3] = 254;
            } else {
                data[i + 3] ^= 1;
            }
        }
        return img;
    });

    // Add microscopic noise to audio buffers
    if (window.AudioBuffer) {
        const origGetChannel = AudioBuffer.prototype.getChannelData;
        AudioBuffer.prototype.getChannelData = seal(function() {
            const data = origGetChannel.apply(this, arguments);
            if (data.length > 0) data[0] += 0.0000001;
            return data;
        });
    }
})();
