// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp — makes the page cross-origin isolated on static hosts.
//
// d2d's threads need SharedArrayBuffer, which browsers only give a page
// served with COOP/COEP headers. GitHub Pages can't set headers, so this
// service worker adds them to every response it serves for the page.
// index.html registers it and reloads once it controls the page.
self.addEventListener("install", () => self.skipWaiting());
self.addEventListener("activate", (event) => event.waitUntil(self.clients.claim()));

self.addEventListener("fetch", (event) => {
    const request = event.request;
    if (request.cache === "only-if-cached" && request.mode !== "same-origin") return;
    event.respondWith(fetch(request).then((response) => {
        if (response.status === 0) return response;
        const headers = new Headers(response.headers);
        headers.set("Cross-Origin-Opener-Policy", "same-origin");
        headers.set("Cross-Origin-Embedder-Policy", "require-corp");
        headers.set("Cross-Origin-Resource-Policy", "cross-origin");
        return new Response(response.body, { status: response.status, statusText: response.statusText, headers });
    }));
});
