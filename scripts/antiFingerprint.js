(function() {
    'use strict';

    // Make patched functions survive toString(). The label is per function, so
    // accessors still read "function get hardwareConcurrency() { [native code] }".
    const rawToString = Function.prototype.toString;
    const patchedSet = new WeakSet();
    const patchedLabels = new WeakMap();
    Function.prototype.toString = function() {
        if (patchedSet.has(this)) {
            return 'function ' + (patchedLabels.get(this) || '') + '() { [native code] }';
        }
        return rawToString.apply(this, arguments);
    };
    // label is '' for plain functions, 'get x' for accessors
    const seal = (fn, label) => {
        patchedSet.add(fn);
        patchedLabels.set(fn, label || '');
        return fn;
    };
    // The override itself has to look native too.
    seal(Function.prototype.toString, 'toString');

    // WebIDL attributes are enumerable accessors on the prototype. Putting one
    // on the navigator instance shows up in hasOwnProperty and in
    // getOwnPropertyNames, and drops it out of for..in.
    const defineGetter = (target, prop, val) => {
        defineAccessor(target, prop, function() { return val; });
    };
    // Same, but installs the caller's getter. defineGetter returns a value, so
    // passing a getter to it hands the function itself to the page.
    const defineAccessor = (target, prop, getter) => {
        try {
            Object.defineProperty(target, prop, {
                get: seal(getter, 'get ' + prop),
                set: undefined,
                enumerable: true,
                configurable: true
            });
        } catch (e) {}
    };
    // WebIDL operations are writable/enumerable/configurable data properties.
    const defineMethod = (target, prop, fn) => {
        try {
            Object.defineProperty(target, prop, {
                value: seal(fn, prop),
                writable: true,
                enumerable: true,
                configurable: true
            });
        } catch (e) {}
    };

    // Blink splits window attributes between the instance and Window.prototype.
    // Patch wherever the real one lives, inventing an own property is the same
    // leak as putting navigator properties on the navigator.
    const patchWindowProp = (prop, getter) => {
        const targets = [];
        if (Object.getOwnPropertyDescriptor(window, prop)) targets.push(window);
        if (Object.getOwnPropertyDescriptor(Window.prototype, prop)) targets.push(Window.prototype);
        if (!targets.length) targets.push(Window.prototype);
        for (const t of targets) defineAccessor(t, prop, getter);
    };

    // Values sit on an intermediate prototype, so instanceof still works and the
    // own-property list stays empty like a real platform object.
    const protoShaped = (nativeProto, props) => {
        try {
            const fakeProto = Object.create(nativeProto);
            for (const key of Object.keys(props)) {
                Object.defineProperty(fakeProto, key, {
                    get: seal(function() { return props[key]; }, 'get ' + key),
                    set: undefined,
                    enumerable: true,
                    configurable: true
                });
            }
            return Object.create(fakeProto);
        } catch (e) {
            return null;
        }
    };

    const PERSONA = {
        locale: 'en-US',
        languages: ['en-US', 'en'],
        platform: 'Linux',
        hardwareConcurrency: 2,
        deviceMemory: 4, // 4 GB RAM
        colorDepth: 24,
        storageQuota: 274877906944, // 256 GiB, a plausible desktop SSD figure
        gpuVendor: 'Google Inc. (Intel)',
        gpuRenderer: 'ANGLE (Intel, Intel(R) UHD Graphics 620 (0x00005917) OpenGL 4.5)'
    };

    // Noise seed: same value for every call on a page, different per profile.
    // Browser.cpp swaps the bare identifier for a number, so it never reaches
    // the page's global scope.
    const seedSource = (typeof __NULLA_FP_SEED__ !== 'undefined') ? __NULLA_FP_SEED__ : 0x4E756C6C;
    const strHash = (s) => {
        let h = 2166136261;
        for (let i = 0; i < s.length; i++) {
            h ^= s.charCodeAt(i);
            h = Math.imul(h, 16777619);
        }
        return h >>> 0;
    };
    const seed = (typeof seedSource === 'number' ? seedSource : strHash('NullA')) >>> 0;

    // Index-seeded noise, for signals that are not 2D pixel grids.
    const idxNoise = (i, salt) => {
        let h = (Math.imul(i ^ seed, 2654435761) ^ (salt || 0)) >>> 0;
        h = Math.imul(h ^ (h >>> 15), h | 1);
        h ^= h + Math.imul(h ^ (h >>> 7), h | 61);
        return ((h ^ (h >>> 14)) >>> 0) / 4294967296;
    };

    const noiseTable = (() => {
        const table = new Array(256);
        for (let i = 0; i < 256; i++) {
            table[i] = [idxNoise(i, 1), idxNoise(i, 2), idxNoise(i, 3), idxNoise(i, 4)];
        }
        return table;
    })();

    // Deterministic per-pixel noise, so every read path agrees.
    const needle = (x, y) => noiseTable[((x * 131) + (y * 73)) & 255];

    // Real renderers differ by a few levels, not by a one-bit flip on half the
    // pixels, XOR on the low bit is the classic injected-noise signature.
    // Transparent pixels stay untouched, a real rasteriser leaves them at 0.
    const clamp255 = (v) => (v < 0 ? 0 : (v > 255 ? 255 : v));
    const farble = (data, width, height) => {
        if (!data || !width || !height) return data;
        for (let i = 0, p = 0; i + 3 < data.length; i += 4, p++) {
            if (data[i + 3] === 0) continue;
            const n = needle(p % width, (p / width) | 0);
            data[i] = clamp255(data[i] + Math.round((n[0] - 0.5) * 6));
            data[i + 1] = clamp255(data[i + 1] + Math.round((n[1] - 0.5) * 6));
            data[i + 2] = clamp255(data[i + 2] + Math.round((n[2] - 0.5) * 6));
        }
        return data;
    };

    const origToDataURL = HTMLCanvasElement.prototype.toDataURL;
    const origToBlob = HTMLCanvasElement.prototype.toBlob;

    // Round-trip through a scratch canvas. getImageData is patched, so what
    // putImageData gets is already noised farble() on top would double it.
    const scratchNoised = (source, w, h) => {
        const tmp = document.createElement('canvas');
        tmp.width = w;
        tmp.height = h;
        const c = tmp.getContext('2d');
        c.drawImage(source, 0, 0);
        c.putImageData(c.getImageData(0, 0, w, h), 0, 0);
        return tmp;
    };

    // Navigator bros
    defineGetter(Navigator.prototype, 'hardwareConcurrency', PERSONA.hardwareConcurrency);
    defineGetter(Navigator.prototype, 'deviceMemory', PERSONA.deviceMemory);
    defineGetter(Navigator.prototype, 'plugins', Object.freeze([]));
    defineGetter(Navigator.prototype, 'language', PERSONA.locale);
    defineGetter(Navigator.prototype, 'languages', Object.freeze(PERSONA.languages.slice()));

    // connection and getBattery are in every Chromium, so deleting them is a
    // tell on its own, and delete on a non-configurable attribute throws under
    // strict mode, which would kill the rest of this script.
    if (window.NetworkInformation) {
        const conn = protoShaped(NetworkInformation.prototype, {
            effectiveType: '4g',
            downlink: 10,
            rtt: 50,
            saveData: false,
            type: 'ethernet',
            onchange: null
        });
        if (conn) defineGetter(Navigator.prototype, 'connection', conn);
    }

    if (window.BatteryManager && typeof navigator.getBattery === 'function') {
        const battery = protoShaped(BatteryManager.prototype, {
            charging: true,
            chargingTime: 0,
            dischargingTime: Infinity,
            level: 1,
            onchargingchange: null,
            onchargingtimechange: null,
            ondischargingtimechange: null,
            onlevelchange: null
        });
        if (battery) {
            defineMethod(Navigator.prototype, 'getBattery', function() {
                return Promise.resolve(battery);
            });
        }
    }

    // Keyboard layout is close to a country id. Plain US layout, same as the
    // en-US locale above.
    if (window.KeyboardLayoutMap && navigator.keyboard) {
        const KEYS = [
            'a', 'A', '1', '!', 'b', 'B', '2', '@', 'c', 'C', '3', '#',
            'd', 'D', '4', '$', 'e', 'E', '5', '%', 'f', 'F', '6', '^',
            'g', 'G', '7', '&', 'h', 'H', '8', '*', 'i', 'I', '9', '(',
            'j', 'J', '0', ')', 'k', 'K', '-', '_', 'l', 'L', '=', '+',
            'm', 'M', 'Backspace', 'n', 'N', 'o', 'O', 'p', 'P', 'q', 'Q',
            'r', 'R', 't', 'T', 'u', 'U', 'v', 'V', 'w', 'W', 'x', 'X',
            'y', 'Y', 'z', 'Z', 'Tab', 'CapsLock', 'Escape', 'ArrowUp',
            'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'ControlLeft',
            'ShiftLeft', 'AltLeft', 'MetaLeft', 'ControlRight', 'ShiftRight',
            'AltRight', 'MetaRight', 'Space', ';', ':', '?', '/', '\'',
            '"', '[', '{', ']', '}', '\\', '|', '`', '~'
        ];
        try {
            defineMethod(KeyboardLayoutMap.prototype, 'has', function(code) {
                return KEYS.indexOf(code) !== -1;
            });
            defineGetter(KeyboardLayoutMap.prototype, 'size', KEYS.length);
            defineMethod(KeyboardLayoutMap.prototype, 'keys', function() {
                return KEYS.slice();
            });
        } catch (e) {}
    }

    // Quota tracks free disk space, which is a decent proxy for disk size. Pin it.
    if (window.StorageManager) {
        const usage = Math.floor(idxNoise(seed, 11) * 4194304) + 131072;
        defineMethod(StorageManager.prototype, 'estimate', function() {
            // StorageEstimate is a dictionary, not an interface: no global, and
            // native returns a plain object carrying its own properties.
            return Promise.resolve({
                quota: PERSONA.storageQuota,
                usage: usage,
                usageDetails: {}
            });
        });
    }

    // Canvas
    const ctxProto = CanvasRenderingContext2D.prototype;
    const canvasProto = HTMLCanvasElement.prototype;

    const origGetImageData = ctxProto.getImageData;
    ctxProto.getImageData = seal(function(x, y, w, h) {
        const img = origGetImageData.apply(this, arguments);
        farble(img.data, img.width, img.height);
        return img;
    }, 'getImageData');

    canvasProto.toDataURL = seal(function(type, quality) {
        const w = this.width, h = this.height;
        if (!w || !h) return origToDataURL.apply(this, arguments);
        try {
            return origToDataURL.call(scratchNoised(this, w, h), type, quality);
        } catch (e) {
            return origToDataURL.apply(this, arguments);
        }
    }, 'toDataURL');

    canvasProto.toBlob = seal(function(callback, type, quality) {
        const w = this.width, h = this.height;
        if (!w || !h) return origToBlob.apply(this, arguments);
        try {
            return origToBlob.call(scratchNoised(this, w, h), callback, type, quality);
        } catch (e) {
            return origToBlob.apply(this, arguments);
        }
    }, 'toBlob');

    // OffscreenCanvas shares no prototype with HTMLCanvasElement, and
    // convertToBlob still has to hand back a promise.
    if (window.OffscreenCanvas && typeof OffscreenCanvas.prototype.convertToBlob === 'function') {
        const offProto = OffscreenCanvas.prototype;
        const origConvertToBlob = offProto.convertToBlob;
        offProto.convertToBlob = seal(function(options) {
            const w = this.width, h = this.height;
            if (!w || !h) return origConvertToBlob.apply(this, arguments);
            try {
                const opts = options || {};
                const tmp = scratchNoised(this, w, h);
                return new Promise((resolve) => {
                    origToBlob.call(tmp, resolve, opts.type, opts.quality);
                });
            } catch (e) {
                return origConvertToBlob.apply(this, arguments);
            }
        }, 'convertToBlob');
    }

    // Glyph advances measure the font directly, so a spoofed font list is worth
    // little while these stay readable. Nudge each (font, text) pair by a stable
    // sub-pixel amount, well under a tenth of a pixel.
    const TEXT_SHIFT_MAX = 0.08;
    const textShiftCache = new Map();
    const textShift = (font, text) => {
        const key = font + '\u0000' + text;
        let shift = textShiftCache.get(key);
        if (shift === undefined) {
            shift = idxNoise(strHash(key), 13) * TEXT_SHIFT_MAX;
            if (textShiftCache.size > 4096) textShiftCache.clear();
            textShiftCache.set(key, shift);
        }
        return shift;
    };

    const origMeasureText = ctxProto.measureText;
    ctxProto.measureText = seal(function(text) {
        const metrics = origMeasureText.apply(this, arguments);
        try {
            const font = (typeof this.font === 'string') ? this.font : '';
            const shift = textShift(font, String(text));
            if (!shift) return metrics;
            // Reflect with the real receiver - a Proxy forwards no internal
            // slots, so the native getters would fail their brand check.
            return new Proxy(metrics, {
                get(target, prop) {
                    if (prop === 'width') return target.width + shift;
                    if (prop === 'actualBoundingBoxLeft') return target.actualBoundingBoxLeft - shift / 2;
                    if (prop === 'actualBoundingBoxRight') return target.actualBoundingBoxRight + shift / 2;
                    return Reflect.get(target, prop, target);
                }
            });
        } catch (e) {
            return metrics;
        }
    }, 'measureText');

    // getExtension() is left alone. Returning null for WEBGL_debug_renderer_info
    // or filtering it out of getSupportedExtensions() is a well known tell, and
    // the extension object itself carries nothing but two enum constants. So
    // hand back the real object and intercept at getParameter() - that covers
    // both the numeric and the ext.UNMASKED_RENDERER_WEBGL spelling.
    const GL_VENDOR = 0x1F00;
    const GL_RENDERER = 0x1F01;
    const UNMASKED_VENDOR_WEBGL = 0x9245;
    const UNMASKED_RENDERER_WEBGL = 0x9246;
    const UNSIGNED_BYTE = 0x1401;

    // The scalar limits give the driver away just as much as the renderer
    // string. These are what Chrome on Mesa / Intel UHD reports.
    const GL_INTS = new Map([
        [0x0D33, 16384],    // MAX_TEXTURE_SIZE
        [0x851C, 16384],    // MAX_CUBE_MAP_TEXTURE_SIZE
        [0x84E8, 16384],    // MAX_RENDERBUFFER_SIZE
        [0x0D52, 8],        // RED_BITS
        [0x0D53, 8],        // GREEN_BITS
        [0x0D54, 8],        // BLUE_BITS
        [0x0D55, 8],        // ALPHA_BITS
        [0x0D56, 24],       // DEPTH_BITS
        [0x0D57, 8],        // STENCIL_BITS
        [0x80A8, 1],        // SAMPLE_BUFFERS
        [0x80A9, 4],        // SAMPLES
        [0x8D57, 4],        // MAX_SAMPLES
        [0x8D09, 1048575],  // MAX_ELEMENT_INDEX
        [0x8869, 16],       // MAX_VERTEX_ATTRIBS
        [0x8872, 32],       // MAX_TEXTURE_IMAGE_UNITS
        [0x8B4C, 32],       // MAX_VERTEX_TEXTURE_IMAGE_UNITS
        [0x8B4D, 96],       // MAX_COMBINED_TEXTURE_IMAGE_UNITS
        [0x8DFA, true],     // SHADER_COMPILER
        [0x8DFB, 4096],     // MAX_VERTEX_UNIFORM_VECTORS
        [0x8DFC, 30],       // MAX_VARYING_VECTORS
        [0x8DFD, 1024],     // MAX_FRAGMENT_UNIFORM_VECTORS
        [0x8824, 8],        // MAX_DRAW_BUFFERS
        [0x8CDF, 8]         // MAX_COLOR_ATTACHMENTS
    ]);

    const GL_VIEWPORT_DIMS = new Int32Array([16384, 16384]);
    const GL_LINE_WIDTH_RANGE = new Float32Array([1, 1]);
    const GL_POINT_SIZE_RANGE = new Float32Array([1, 1024]);

    // Strong vendor signal, and it varies a lot between drivers.
    const PRECISION = {
        'highp float': { rangeMin: 127, rangeMax: 127, precision: 23 },
        'mediump float': { rangeMin: 127, rangeMax: 127, precision: 23 },
        'lowp float': { rangeMin: 127, rangeMax: 127, precision: 23 },
        'highp int': { rangeMin: 31, rangeMax: 31, precision: 23 },
        'mediump int': { rangeMin: 31, rangeMax: 31, precision: 23 },
        'lowp int': { rangeMin: 31, rangeMax: 31, precision: 23 }
    };
    const PRECISION_BY_ENUM = {
        0x8DF0: 'lowp float',
        0x8DF1: 'mediump float',
        0x8DF2: 'highp float',
        0x8DF4: 'lowp int',
        0x8DF5: 'mediump int',
        0x8DF6: 'highp int'
    };

    // readPixels hands back a raw byte buffer, so it walks straight past every
    // HTMLCanvasElement patch. It must still return undefined - passing the
    // buffer on would be a new tell.
    const patchReadPixels = (proto) => {
        if (typeof proto.readPixels !== 'function') return;
        const orig = proto.readPixels;
        proto.readPixels = seal(function(x, y, w, h, format, type, pixels) {
            orig.apply(this, arguments);
            try {
                if (type === UNSIGNED_BYTE && pixels && pixels.length) {
                    farble(pixels, w, h);
                }
            } catch (e) {}
        }, 'readPixels');
    };

    const patchGL = (proto) => {
        if (!proto) return;

        const oldGetParam = proto.getParameter;
        proto.getParameter = seal(function(pname) {
            if (pname === GL_VENDOR) return 'WebKit';
            if (pname === GL_RENDERER) return 'WebKit WebGL';
            if (pname === UNMASKED_VENDOR_WEBGL) return PERSONA.gpuVendor;
            if (pname === UNMASKED_RENDERER_WEBGL) return PERSONA.gpuRenderer;
            if (GL_INTS.has(pname)) return GL_INTS.get(pname);
            if (pname === 0x0D3A) return Int32Array.from(GL_VIEWPORT_DIMS);       // MAX_VIEWPORT_DIMS
            if (pname === 0x846E) return Float32Array.from(GL_LINE_WIDTH_RANGE);  // ALIASED_LINE_WIDTH_RANGE
            if (pname === 0x846D) return Float32Array.from(GL_POINT_SIZE_RANGE);  // ALIASED_POINT_SIZE_RANGE
            return oldGetParam.apply(this, arguments);
        }, 'getParameter');

        if (typeof proto.getShaderPrecisionFormat === 'function'
            && window.WebGLShaderPrecisionFormat) {
            const oldPrecision = proto.getShaderPrecisionFormat;
            proto.getShaderPrecisionFormat = seal(function(shaderType, precisionType, precision) {
                const key = PRECISION_BY_ENUM[precision];
                if (!key) return oldPrecision.apply(this, arguments);
                return protoShaped(WebGLShaderPrecisionFormat.prototype, PRECISION[key]);
            }, 'getShaderPrecisionFormat');
        }

        // Only the request hint is pinned. The rest describe the context the
        // page asked for, and disagreeing with the real framebuffer is worse.
        if (typeof proto.getContextAttributes === 'function') {
            const oldAttrs = proto.getContextAttributes;
            proto.getContextAttributes = seal(function() {
                const attrs = oldAttrs.apply(this, arguments);
                try {
                    if (attrs) attrs.powerPreference = 'default';
                } catch (e) {}
                return attrs;
            }, 'getContextAttributes');
        }

        // getSupportedExtensions() passes through on purpose. A hand-written
        // list would have to agree with the masked limits, the real driver and
        // the renderer string at once, and a wrong guess is more identifying.

        patchReadPixels(proto);
    };
    patchGL(WebGLRenderingContext.prototype);
    patchGL(WebGL2RenderingContext.prototype);

    // Screen and window geometry
    //
    // Sizes are snapped to the nearest common desktop resolution, not replaced.
    // innerWidth has to stay consistent with documentElement.clientWidth and the
    // canvas backing store, all of which report the real layout - a fixed
    // persona screen shows up as a viewport disagreeing with the page.
    const RESOLUTIONS = [
        [1280, 720], [1366, 768], [1440, 900], [1536, 864], [1600, 900],
        [1680, 1050], [1920, 1080], [2560, 1440], [3440, 1440], [3840, 2160]
    ];
    const snapScreen = (w, h) => {
        let width = 1920;
        let height = 1080;
        let bestCost = Infinity;
        for (const r of RESOLUTIONS) {
            const cost = Math.abs(r[0] - w) / r[0] + Math.abs(r[1] - h) / r[1];
            if (cost < bestCost) { bestCost = cost; width = r[0]; height = r[1]; }
        }
        return {
            width: width,
            height: height,
            availWidth: width,
            availHeight: height - 40, // room for a panel
            availLeft: 0,
            availTop: 0,
            colorDepth: PERSONA.colorDepth,
            pixelDepth: PERSONA.colorDepth
        };
    };

    const reportedScreen = (window.Screen && window.screen)
        ? snapScreen(screen.width, screen.height)
        : { width: 1920, height: 1080, availWidth: 1920, availHeight: 1040,
            availLeft: 0, availTop: 0, colorDepth: 24, pixelDepth: 24 };

    if (window.Screen) {
        for (const key of Object.keys(reportedScreen)) {
            defineGetter(Screen.prototype, key, reportedScreen[key]);
        }
        if (window.ScreenOrientation && screen.orientation) {
            const orientation = protoShaped(ScreenOrientation.prototype, {
                type: reportedScreen.width >= reportedScreen.height
                    ? 'landscape-primary'
                    : 'portrait-primary',
                angle: 0
            });
            if (orientation) defineGetter(Screen.prototype, 'orientation', orientation);
        }
    }

    // devicePixelRatio is left alone. X11 scaling gives values like 1.3333, but
    // the canvas backing store and the layout viewport are both built from the
    // real ratio, and canvas.width == rect.width * devicePixelRatio gets checked.

    const FRAME_W_MAX = 16;
    const FRAME_H_MAX = 90;
    const origWindowGetter = (prop) => {
        let d = Object.getOwnPropertyDescriptor(window, prop);
        if (!d && Window.prototype) d = Object.getOwnPropertyDescriptor(Window.prototype, prop);
        return d && d.get ? d.get : null;
    };
    const gOuterW = origWindowGetter('outerWidth');
    const gOuterH = origWindowGetter('outerHeight');
    const gInnerW = origWindowGetter('innerWidth');
    const gInnerH = origWindowGetter('innerHeight');
    const gScreenX = origWindowGetter('screenX');
    const gScreenY = origWindowGetter('screenY');

    const windowGeometry = () => {
        const rawOuterW = gOuterW ? gOuterW.call(window) : 0;
        const rawOuterH = gOuterH ? gOuterH.call(window) : 0;
        const rawInnerW = gInnerW ? gInnerW.call(window) : 0;
        const rawInnerH = gInnerH ? gInnerH.call(window) : 0;
        const outerW = Math.min(rawOuterW, reportedScreen.availWidth);
        const outerH = Math.min(rawOuterH, reportedScreen.availHeight);
        const frameW = Math.min(Math.max(0, rawOuterW - rawInnerW), FRAME_W_MAX);
        const frameH = Math.min(Math.max(0, rawOuterH - rawInnerH), FRAME_H_MAX);
        return {
            outerWidth: outerW,
            outerHeight: outerH,
            innerWidth: Math.max(0, Math.min(rawInnerW, outerW - frameW)),
            innerHeight: Math.max(0, Math.min(rawInnerH, outerH - frameH))
        };
    };

    const windowOrigin = () => {
        const geo = windowGeometry();
        const maxX = Math.max(0, reportedScreen.availWidth - geo.outerWidth);
        const maxY = Math.max(0, reportedScreen.availHeight - geo.outerHeight);
        const rawX = gScreenX ? gScreenX.call(window) : 0;
        const rawY = gScreenY ? gScreenY.call(window) : 0;
        return {
            x: Math.min(Math.max(0, rawX), maxX),
            y: Math.min(Math.max(0, rawY), maxY)
        };
    };

    if (gOuterW) patchWindowProp('outerWidth', function() { return windowGeometry().outerWidth; });
    if (gOuterH) patchWindowProp('outerHeight', function() { return windowGeometry().outerHeight; });
    if (gInnerW) patchWindowProp('innerWidth', function() { return windowGeometry().innerWidth; });
    if (gInnerH) patchWindowProp('innerHeight', function() { return windowGeometry().innerHeight; });
    // Fresh function per property. seal() labels by identity, so sharing one
    // getter would render both screenX and screenLeft as "get screenX".
if (gScreenX) {
        patchWindowProp('screenX', function() { return windowOrigin().x; });
        patchWindowProp('screenLeft', function() { return windowOrigin().x; });
    }
    if (gScreenY) {
        patchWindowProp('screenY', function() { return windowOrigin().y; });
        patchWindowProp('screenTop', function() { return windowOrigin().y; });
    }

    // userAgentData and the Sec-CH-UA-* headers are two views of the same values.
    // Only the two that leak are pinned, on both sides; the rest is left to
    // Chromium so it keeps agreeing with the User-Agent.
    if (navigator.userAgentData) {
        try {
            const UAD = Object.getPrototypeOf(navigator.userAgentData);
            defineGetter(UAD, 'platform', PERSONA.platform);
            defineGetter(UAD, 'mobile', false);
            const origHigh = navigator.userAgentData.getHighEntropyValues;
            if (typeof origHigh === 'function') {
                defineMethod(UAD, 'getHighEntropyValues', function() {
                    return origHigh.apply(this, arguments).then((values) => {
                        return Object.assign({}, values, {
                            platform: PERSONA.platform,
                            platformVersion: '',
                            model: '',
                            wow64: false
                        });
                    });
                });
            }
        } catch (e) {}
    }

    // Speech voices are an OS package fingerprint. Pin the list instead of
    // enumerating whatever speech backend is installed.
    if (window.SpeechSynthesis && window.SpeechSynthesisVoice
        && window.speechSynthesis) {
        const voices = [
            { name: 'Speech Dispatcher', lang: 'en-US', voiceURI: 'Speech Dispatcher',
              localService: true, default: true },
            { name: 'Google US English', lang: 'en-US', voiceURI: 'Google US English',
              localService: false, default: false }
        ].map((v) => protoShaped(SpeechSynthesisVoice.prototype, v)).filter(Boolean);
        if (voices.length) {
            defineMethod(SpeechSynthesis.prototype, 'getVoices', function() {
                return voices.slice();
            });
        }
    }

    // Locale and time zone
    const forceEnUS = (proto) => {
        if (!proto) return;
        const orig = proto.resolvedOptions;
        proto.resolvedOptions = seal(function() {
            let o;
            try {
                o = orig.apply(this, arguments);
            } catch (e) {
                return orig.apply(this, arguments);
            }
            o.locale = PERSONA.locale;
            return o;
        }, 'resolvedOptions');
    };
    forceEnUS(Intl.DateTimeFormat.prototype);
    forceEnUS(Intl.NumberFormat.prototype);
    forceEnUS(Intl.Collator.prototype);
    forceEnUS(Intl.RelativeTimeFormat.prototype);
    forceEnUS(Intl.PluralRules.prototype);
    forceEnUS(Intl.ListFormat.prototype);

    // Report UTC everywhere, and format in UTC too so the text and
    // resolvedOptions().timeZone agree.
    const DTF = Intl.DateTimeFormat.prototype;
    const oResolvedDTF = DTF.resolvedOptions;
    const safeResolve = (self) => {
        try {
            return oResolvedDTF.apply(self);
        } catch (e) {
            return undefined;
        }
    };
    DTF.resolvedOptions = seal(function() {
        const o = safeResolve(this);
        if (o === undefined) return oResolvedDTF.apply(this);
        o.locale = PERSONA.locale;
        o.timeZone = 'UTC';
        return o;
    }, 'resolvedOptions');

    const utcFormatter = (self) => {
        const base = safeResolve(self);
        return base === undefined
            ? null
            : new Intl.DateTimeFormat(PERSONA.locale, Object.assign(base, { timeZone: 'UTC' }));
    };
    const asDate = (d) => (d === undefined || d === null)
        ? new Date() : (typeof d === 'number' || typeof d === 'string' ? new Date(d) : d);
    const TZ_LONG = 'Coordinated Universal Time';
    const sanitizeTz = (s) =>
        s.replace(/GMT[+-]\d{2}:?\d{2}\s*(\([^)]*\))?/g, 'GMT+0000 (' + TZ_LONG + ')');
    // formatToParts and formatRangeToParts return part lists, not strings.
    const sanitizeParts = (parts) => {
        try {
            if (Array.isArray(parts)) {
                for (const p of parts) {
                    if (p && p.type === 'timeZoneName') p.value = TZ_LONG;
                }
            }
        } catch (e) {}
        return parts;
    };
    // Detached calls lose the instance, so they go through a bare UTC formatter -
    // native would throw on the same receiver.
    const utcDefault = new Intl.DateTimeFormat(PERSONA.locale, { timeZone: 'UTC' });
    // Reading .format invokes the native getter, whose brand check rejects the
    // prototype and throws. Assigning to it is a silent no-op too - accessor, no
    // setter. So read the descriptor, never the value.
    const descOrNull = (obj, prop) => {
        try { return Object.getOwnPropertyDescriptor(obj, prop) || null; } catch (e) { return null; }
    };
    // A native bound method reports name "format" and length 1. A bare wrapper
    // reports "", which is a one-line detector.
    const mimicBound = (fn, name, arity) => {
        try {
            Object.defineProperty(fn, 'name', { value: name, configurable: true });
            Object.defineProperty(fn, 'length', { value: arity, configurable: true });
        } catch (e) {}
        return fn;
    };
    const patchFormatMethod = (prop, arity, transform) => {
        const d = descOrNull(DTF, prop);
        if (!d) return;
        const argsOf = function() {
            const a = [];
            for (let i = 0; i < arity; i++) a.push(asDate(arguments[i]));
            return a;
        };
        if (typeof d.get === 'function') {
            // Accessor form. Resolve the bound method on a UTC clone of this
            // instance so the page's own format options survive.
            const nativeGet = d.get;
            defineAccessor(DTF, prop, function() {
                nativeGet.call(this);
                const u = utcFormatter(this);
                if (u === null) return seal(nativeGet.call(this), prop);
                const boundUtc = nativeGet.call(u);
                return seal(mimicBound(function() {
                    try { return transform(boundUtc.apply(u, argsOf.apply(null, arguments))); }
                    catch (e) { return transform(nativeGet.call(this).apply(this, arguments)); }
                }, prop, arity), prop);
            });
            return;
        }
        if (typeof d.value !== 'function') return;
        const oMethod = d.value;
        defineMethod(DTF, prop, function() {
            const u = utcFormatter(this);
            const target = u !== null ? u : utcDefault;
            try { return transform(oMethod.apply(target, argsOf.apply(null, arguments))); }
            catch (e) { return transform(oMethod.apply(this, arguments)); }
        });
    };

    patchFormatMethod('format', 1, sanitizeTz);
    patchFormatMethod('formatToParts', 1, sanitizeParts);
    patchFormatMethod('formatRange', 2, sanitizeTz);
    patchFormatMethod('formatRangeToParts', 2, sanitizeParts);

    Date.prototype.getTimezoneOffset = seal(function() { return 0; }, 'getTimezoneOffset');

    const oDateToString = Date.prototype.toString;
    const oDateToTimeString = Date.prototype.toTimeString;
    const oDateToLocaleString = Date.prototype.toLocaleString;
    const oDateToLocaleTimeString = Date.prototype.toLocaleTimeString;
    Date.prototype.toString = seal(function() { return sanitizeTz(oDateToString.call(this)); }, 'toString');
    Date.prototype.toTimeString = seal(function() { return sanitizeTz(oDateToTimeString.call(this)); }, 'toTimeString');
    Date.prototype.toLocaleString = seal(function() { return sanitizeTz(oDateToLocaleString.apply(this, arguments)); }, 'toLocaleString');
    Date.prototype.toLocaleTimeString = seal(function() { return sanitizeTz(oDateToLocaleTimeString.apply(this, arguments)); }, 'toLocaleTimeString');

    // Temporal reads the real IANA zone through its own path, untouched by the
    // Intl and Date patches. Unflagged in Firefox 139 / Chrome 144, so
    // Temporal.Now.timeZoneId() alone would bypass the UTC spoof above.
    if (window.Temporal && window.Temporal.Now) {
        try {
            const TNow = window.Temporal.Now;
            const utcNow = () => TNow.instant().toZonedDateTimeISO('UTC');
            if (typeof TNow.timeZoneId === 'function') {
                defineMethod(TNow, 'timeZoneId', function() { return 'UTC'; });
            }
            // timeZoneLike is ignored on purpose; honouring it would only show
            // that the page asked for another zone.
            if (typeof TNow.zonedDateTimeISO === 'function') {
                defineMethod(TNow, 'zonedDateTimeISO', function() { return utcNow(); });
            }
            if (typeof TNow.plainDateTimeISO === 'function') {
                defineMethod(TNow, 'plainDateTimeISO', function() { return utcNow().toPlainDateTime(); });
            }
            if (typeof TNow.plainDateISO === 'function') {
                defineMethod(TNow, 'plainDateISO', function() { return utcNow().toPlainDate(); });
            }
            if (typeof TNow.plainTimeISO === 'function') {
                defineMethod(TNow, 'plainTimeISO', function() { return utcNow().toPlainTime(); });
            }
        } catch (e) {}
    }

    // OfflineAudioContext output varies with CPU and audio hardware, and
    // fingerprinters read the rendered buffer. Nudging one sample by a constant
    // leaves the rest intact and the constant is the giveaway, so perturb the
    // whole buffer relative to each sample's own magnitude.
    const AUDIO_EPS = 1e-7;
    if (window.AudioBuffer) {
        const origGetChannel = AudioBuffer.prototype.getChannelData;
        AudioBuffer.prototype.getChannelData = seal(function() {
            const data = origGetChannel.apply(this, arguments);
            try {
                const salt = 17 + ((typeof this.numberOfChannels === 'number')
                    ? this.numberOfChannels : 1);
                for (let i = 0; i < data.length; i++) {
                    data[i] *= 1 + ((idxNoise(i, salt) - 0.5) * 2 * AUDIO_EPS);
                }
            } catch (e) {}
            return data;
        }, 'getChannelData');
    }
})();
