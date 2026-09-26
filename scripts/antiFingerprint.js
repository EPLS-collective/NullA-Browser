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
    if (navigator.connection) {
        delete Navigator.prototype.connection;
    }
    if (navigator.getBattery) {
        delete Navigator.prototype.getBattery;
    }
    if (navigator.userAgentData) {
        const CHROME_MAJOR = "__NULLA_CHROME_MAJOR__";
        const fakeBrands = [
            { brand: "Not)A;Brand", version: "99" },
            { brand: "Google Chrome", version: CHROME_MAJOR },
            { brand: "Chromium", version: CHROME_MAJOR }
        ];
        const fakeFullVersionList = [
            { brand: "Not)A;Brand", version: "99.0.0.0" },
            { brand: "Google Chrome", version: CHROME_MAJOR + ".0.0.0" },
            { brand: "Chromium", version: CHROME_MAJOR + ".0.0.0" }
        ];
        const fakeUAData = {
            brands: fakeBrands,
            mobile: false,
            platform: "Windows",
            toJSON: seal(function() {
                return { brands: fakeBrands, mobile: false, platform: "Windows" };
            }),
            getHighEntropyValues: seal(function() {
                return Promise.resolve({
                    brands: fakeBrands,
                    mobile: false,
                    platform: "Windows",
                    platformVersion: "10.0.0",
                    architecture: "x86",
                    bitness: "64",
                    fullVersionList: fakeFullVersionList,
                    uaFullVersion: CHROME_MAJOR + ".0.0.0"
                });
            })
        };
        mask(navigator, 'userAgentData', fakeUAData);
    }

    // Per-page-session seed: the canvas noise stays identical for every call
    // on this page, but differs between sites and between sessions.
    const seedHash = (s) => {
        let h = 2166136261;
        for (let i = 0; i < s.length; i++) {
            h ^= s.charCodeAt(i);
            h = Math.imul(h, 16777619);
        }
        return h >>> 0;
    };
    const seed = (seedHash(location.origin) ^ (Math.random() * 0xFFFFFFFF >>> 0)) >>> 0;

    const noiseTable = (() => {
        const table = new Array(256);
        let s = seed >>> 0;
        const rng = () => {
            s = (s + 0x6D2B79F5) >>> 0;
            let z = s;
            z = Math.imul(z ^ (z >>> 15), z | 1);
            z ^= z + Math.imul(z ^ (z >>> 7), z | 61);
            return ((z ^ (z >>> 14)) >>> 0) / 4294967296;
        };
        for (let i = 0; i < 256; i++) table[i] = [rng(), rng(), rng(), rng()];
        return table;
    })();

    // Deterministic per-pixel noise, so every read path agrees.
    const needle = (x, y) => noiseTable[((x * 131) + (y * 73)) & 255];

    const farble = (img) => {
        const d = img.data;
        for (let i = 0; i < d.length; i += 4) {
            const p = i / 4;
            const n = needle(p % img.width, (p / img.width) | 0);
            d[i] ^= n[0] < 0.5 ? 1 : 0;
            d[i + 1] ^= n[1] < 0.5 ? 1 : 0;
            d[i + 2] ^= n[2] < 0.5 ? 1 : 0;
            if (d[i + 3] === 255 && n[3] < 0.05) {
                d[i + 3] = 254;
            }
        }
        return img;
    };

    // Poison every canvas read path (getImageData, toDataURL, toBlob), not
    // just getImageData. Large canvases are left untouched to avoid cost.
    const farbleAllPaths = (ctxProto, canvasProto) => {
        const origGetImageData = ctxProto.getImageData;
        ctxProto.getImageData = seal(function(x, y, w, h) {
            return farble(origGetImageData.apply(this, arguments));
        });

        const origToDataURL = canvasProto.toDataURL;
        canvasProto.toDataURL = seal(function(type, quality) {
            const w = this.width, h = this.height;
            if (!w || !h || w * h > 262144) {
                return origToDataURL.apply(this, arguments);
            }
            try {
                const tmp = document.createElement('canvas');
                tmp.width = w;
                tmp.height = h;
                const c = tmp.getContext('2d');
                c.drawImage(this, 0, 0);
                c.putImageData(farble(c.getImageData(0, 0, w, h)), 0, 0);
                return origToDataURL.call(tmp, type, quality);
            } catch (e) {
                return origToDataURL.apply(this, arguments);
            }
        });

        const origToBlob = canvasProto.toBlob;
        canvasProto.toBlob = seal(function(callback, type, quality) {
            const w = this.width, h = this.height;
            if (!w || !h || w * h > 262144) {
                return origToBlob.apply(this, arguments);
            }
            try {
                const tmp = document.createElement('canvas');
                tmp.width = w;
                tmp.height = h;
                const c = tmp.getContext('2d');
                c.drawImage(this, 0, 0);
                c.putImageData(farble(c.getImageData(0, 0, w, h)), 0, 0);
                return origToBlob.call(tmp, callback, type, quality);
            } catch (e) {
                return origToBlob.apply(this, arguments);
            }
        });
    };
    farbleAllPaths(CanvasRenderingContext2D.prototype, HTMLCanvasElement.prototype);

    // Mask GPU identifiers; extensions lists are filtered too.
    const patchGL = (proto) => {
        if (!proto) return;
        const oldGetExt = proto.getExtension;
        proto.getExtension = seal(function(name) {
            const sensitive = ['WEBGL_debug_renderer_info', 'WEBGL_debug_shaders'];
            if (sensitive.includes(name)) return null;
            return oldGetExt.apply(this, arguments);
        });
        const oldGetParam = proto.getParameter;
        proto.getParameter = seal(function(pname) {
            if (pname === 0x1F00 || pname === 0x9245) return 'WebKit';
            if (pname === 0x1F01 || pname === 0x9246) return 'WebKit WebGL';
            return oldGetParam.apply(this, arguments);
        });
        const oldGetExtensions = proto.getSupportedExtensions;
        proto.getSupportedExtensions = seal(function() {
            const list = oldGetExtensions.apply(this, arguments);
            if (!list) return list;
            return list.filter((e) => e !== 'WEBGL_debug_renderer_info' && e !== 'WEBGL_debug_shaders');
        });
    };
    patchGL(WebGLRenderingContext.prototype);
    patchGL(WebGL2RenderingContext.prototype);

    // Fix the reported locale to en-US across all Intl constructors
    const forceEnUS = (proto) => {
        const orig = proto.resolvedOptions;
        proto.resolvedOptions = seal(function() {
            let o;
            try {
                o = orig.apply(this, arguments);
            } catch (e) {
                return orig.apply(this, arguments);
            }
            o.locale = 'en-US';
            return o;
        });
    };
    forceEnUS(Intl.DateTimeFormat.prototype);
    forceEnUS(Intl.NumberFormat.prototype);
    forceEnUS(Intl.Collator.prototype);
    forceEnUS(Intl.RelativeTimeFormat.prototype);
    forceEnUS(Intl.PluralRules.prototype);
    forceEnUS(Intl.ListFormat.prototype);

    mask(navigator, 'language', 'en-US');
    mask(navigator, 'languages', Object.freeze(['en-US', 'en']));

    // Report UTC everywhere; formatting also runs in UTC so the generated
    // text and resolvedOptions().timeZone always agree.
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
        o.locale = 'en-US';
        o.timeZone = 'UTC';
        return o;
    });

    const utcFormatter = (self) => {
        const base = safeResolve(self);
        return base === undefined
            ? null
            : new Intl.DateTimeFormat('en-US', Object.assign(base, { timeZone: 'UTC' }));
    };
    const oFormatDTF = DTF.format;
    const oFormatToPartsDTF = DTF.formatToParts;
    const oFormatRangeDTF = DTF.formatRange;
    const oFormatRangeToPartsDTF = DTF.formatRangeToParts;
    const asDate = (d) => (d === undefined || d === null)
        ? new Date() : (typeof d === 'number' || typeof d === 'string' ? new Date(d) : d);

    // Detached calls (this == undefined) lose the instance; route them through
    // a bare UTC formatter instead of native, which would throw on the same
    // receiver just like the patched wrapper did.
    const utcDefault = new Intl.DateTimeFormat('en-US', { timeZone: 'UTC' });
    DTF.format = seal(function(date) {
        const u = utcFormatter(this);
        return oFormatDTF.call(u !== null ? u : utcDefault, asDate(date));
    });
    DTF.formatToParts = seal(function(date) {
        const u = utcFormatter(this);
        return oFormatToPartsDTF.call(u !== null ? u : utcDefault, asDate(date));
    });
    if (DTF.formatRange) {
        DTF.formatRange = seal(function(a, b) {
            const u = utcFormatter(this);
            return oFormatRangeDTF.call(u !== null ? u : utcDefault, asDate(a), asDate(b));
        });
        DTF.formatRangeToParts = seal(function(a, b) {
            const u = utcFormatter(this);
            return oFormatRangeToPartsDTF.call(u !== null ? u : utcDefault, asDate(a), asDate(b));
        });
    }

    Date.prototype.getTimezoneOffset = seal(function() { return 0; });

    const sanitizeTz = (s) =>
        s.replace(/GMT[+-]\d{2}:?\d{2}\s*(\([^)]*\))?/g, 'GMT+0000 (Coordinated Universal Time)');
    const oDateToString = Date.prototype.toString;
    const oDateToTimeString = Date.prototype.toTimeString;
    const oDateToLocaleString = Date.prototype.toLocaleString;
    const oDateToLocaleTimeString = Date.prototype.toLocaleTimeString;
    Date.prototype.toString = seal(function() { return sanitizeTz(oDateToString.call(this)); });
    Date.prototype.toTimeString = seal(function() { return sanitizeTz(oDateToTimeString.call(this)); });
    Date.prototype.toLocaleString = seal(function() { return sanitizeTz(oDateToLocaleString.apply(this, arguments)); });
    Date.prototype.toLocaleTimeString = seal(function() { return sanitizeTz(oDateToLocaleTimeString.apply(this, arguments)); });

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
