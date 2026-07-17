(function () {
    "use strict";

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

    window.voxTubeAdBlocking = function () {
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
            state.adObserver = new MutationObserver(blockAds);
            state.adObserver.observe(document.body, {
                childList: true,
                subtree: true,
            });
            state.observers.push(state.adObserver);
        }
    };

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", window.voxTubeAdBlocking);
    } else {
        setTimeout(window.voxTubeAdBlocking, 500);
    }
})();
