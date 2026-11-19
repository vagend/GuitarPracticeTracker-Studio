// Guitar Practice — offline support. Bump VERSION when files change.
const VERSION='gp-2.1.0';
const FILES=['./','index.html','trainer.html','manifest.webmanifest','icons/icon.svg','icons/icon-32.png','icons/icon-192.png','icons/icon-512.png','icons/maskable-192.png','icons/maskable-512.png','icons/apple-touch-icon.png','icons/icon-orange.svg','icons/icon-green.svg','icons/icon-greenlight.svg','icons/icon-blue.svg','icons/icon-red.svg','icons/icon-purple.svg'];
self.addEventListener('install',e=>{e.waitUntil(caches.open(VERSION).then(c=>c.addAll(FILES)).then(()=>self.skipWaiting()))});
self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==VERSION).map(k=>caches.delete(k)))).then(()=>self.clients.claim()))});
// Network first (so updates show up while the launcher is running), cache when the launcher isn't running.
self.addEventListener('fetch',e=>{
  const r=e.request;if(r.method!=='GET'||new URL(r.url).origin!==location.origin)return;
  e.respondWith(fetch(r).then(res=>{if(res.ok){const cp=res.clone();caches.open(VERSION).then(c=>c.put(r,cp))}return res})
    .catch(()=>caches.match(r,{ignoreSearch:true}).then(m=>m||(r.mode==='navigate'?caches.match('index.html'):Response.error()))));
});
