(function () {
    "use strict";

    const AD_FIELDS = ["adPlacements", "adSlots", "playerAds"];

    const stripAdData = (obj) => {
        if (!obj || typeof obj !== "object") return obj;
        for (const field of AD_FIELDS) {
            if (field in obj) delete obj[field];
        }
        if (obj.playerConfig && obj.playerConfig.ssapConfig) {
            delete obj.playerConfig.ssapConfig; // server-side ad stitching config
        }
        return obj;
    };

    let _ytInitialPlayerResponse;
    Object.defineProperty(window, "ytInitialPlayerResponse", {
        configurable: true,
        get() { return _ytInitialPlayerResponse; },
                          set(value) { _ytInitialPlayerResponse = stripAdData(value); },
    });

    const origFetch = window.fetch;
    window.fetch = function (...args) {
        const urlArg = args[0];
        const url = typeof urlArg === "string" ? urlArg : (urlArg && urlArg.url) || "";
        const isPlayerEndpoint = url.includes("/youtubei/v1/player") || url.includes("/youtubei/v1/next");

        return origFetch.apply(this, args).then((response) => {
            if (!isPlayerEndpoint) return response;
            return response.clone().json().then((data) => {
                stripAdData(data);
                return new Response(JSON.stringify(data), {
                    status: response.status,
                    statusText: response.statusText,
                    headers: response.headers,
                });
            }).catch(() => response);
        });
    };

    const origOpen = XMLHttpRequest.prototype.open;
    const origSend = XMLHttpRequest.prototype.send;
    XMLHttpRequest.prototype.open = function (method, url, ...rest) {
        this._isPlayerEndpoint = typeof url === "string" &&
        (url.includes("/youtubei/v1/player") || url.includes("/youtubei/v1/next"));
        return origOpen.call(this, method, url, ...rest);
    };
    XMLHttpRequest.prototype.send = function (...args) {
        if (this._isPlayerEndpoint) {
            this.addEventListener("readystatechange", function () {
                if (this.readyState === 4 && this.responseText) {
                    try {
                        const data = JSON.parse(this.responseText);
                        stripAdData(data);
                        Object.defineProperty(this, "responseText", { value: JSON.stringify(data) });
                        Object.defineProperty(this, "response", { value: JSON.stringify(data) });
                    } catch (e) {}
                }
            });
        }
        return origSend.apply(this, args);
    };

    const state = {
        initialized: false,
        observers: [],
        intervals: [],
    };

    const selectors = {
        ads: [
            "ytd-player-legacy-desktop-watch-ads-renderer",
            "ytd-display-ad-renderer",
            "ytd-in-feed-ad-layout-renderer",
            "ytd-promoted-sparkles-web-renderer",
            "ytd-promoted-video-renderer",
            "ytd-banner-promo-renderer",
            "#masthead-ad",
            "ytd-ad-slot-renderer",
            "ytd-mealbar-promo-renderer",
            "div#player-ads.style-scope.ytd-watch-flexy",
        ],
        sponsored: [
            "ytd-rich-item-renderer[is-sponsored]",
            "ytd-video-renderer[is-sponsored]",
            "ytd-compact-promoted-video-renderer",
            "ytd-in-feed-ad-layout-renderer",
            "ytd-display-ad-renderer",
        ],
        linkedAds: [
            "ytd-companion-slot-renderer",
            "ytd-watch-next-secondary-results-renderer ytd-display-ad-renderer",
            "ytd-watch-next-secondary-results-renderer ytd-ad-slot-renderer",
            'ytd-engagement-panel-section-list-renderer[visibility="ENGAGEMENT_PANEL_VISIBILITY_EXPANDED"]',
        ],
    };

    window.nullAdBlocking = function () {
        if (state.adBlockInitialized) return;
        state.adBlockInitialized = true;

        const blockAds = () => {
            // Remove ad elements
            selectors.ads.forEach((selector) => {
                document.querySelectorAll(selector).forEach((el) => {
                    try {
                        el.remove();
                    } catch {}
                });
            });

            // Remove sponsored content
            selectors.sponsored.forEach((selector) => {
                document.querySelectorAll(selector).forEach((el) => {
                    const parent = el.closest(
                        "ytd-rich-item-renderer, ytd-video-renderer",
                    );
                    parent?.isConnected && parent.remove();
                });
            });

            // Remove linked ads (These are related to the video.)
            selectors.linkedAds.forEach((selector) => {
                document.querySelectorAll(selector).forEach((el) => {
                    try {
                        el.remove();
                    } catch {}
                });
            });

            document
            .querySelectorAll("#contents > ytd-rich-section-renderer")
            .forEach((section) => {
                if (!section.isConnected) return;

                if (
                    section.querySelector(
                        "ytd-background-promo-renderer, ytd-background-promo-card-renderer, ytd-message-renderer",
                    )
                ) {
                    return;
                }

                if (
                    section.querySelector("ytd-rich-shelf-renderer, [is-sponsored]")
                ) {
                    section.remove();
                }
            });

            document.querySelectorAll("ytd-rich-item-renderer").forEach((item) => {
                if (!item.isConnected) return;

                if (
                    !item.innerText.trim() ||
                    item.querySelector("[is-sponsored]") ||
                    item.offsetWidth === 0 ||
                    item.offsetHeight === 0
                ) {
                    item.remove();
                }
            });

            document.querySelectorAll("ytd-rich-grid-row").forEach((row) => {
                if (
                    row.isConnected &&
                    row.querySelectorAll("ytd-rich-item-renderer").length === 0
                ) {
                    row.remove();
                }
            });
        };

        // Initial blocking
        blockAds();

        // Setup interval for continuous blocking
        if (!state.adBlockInterval) {
            state.adBlockInterval = setInterval(blockAds, 500);
            state.intervals.push(state.adBlockInterval);
        }

        // Mutation observer for dynamic content
        if (!state.adObserver) {
            let scheduled = false;
            state.adObserver = new MutationObserver(() => {
                if (scheduled) return;
                scheduled = true;
                requestAnimationFrame(() => {
                    blockAds();
                    scheduled = false;
                });
            });
            state.adObserver.observe(document.body, {
                childList: true,
                subtree: true,
            });
            state.observers.push(state.adObserver);
        }
    };

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", window.nullAdBlocking);
    } else {
        setTimeout(window.nullAdBlocking, 500);
    }
})();
