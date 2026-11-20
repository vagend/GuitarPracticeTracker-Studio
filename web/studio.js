// Studio additions to its practice screens (web/index.html).

// Dropdown lists drawn in the colour scheme (web/selects.js).
(function () { var s = document.createElement('script'); s.src = 'selects.js'; document.head.appendChild(s); })();

// Studio's own "What's new" list (newest first). Keep the top entry's version
// the same as the VERSION in CMakeLists.txt.

window.STUDIO_CHANGES = [
  ['0.7.1', 'Nov 19, 2026', [
    'If your audio interface is switched off or unplugged, Studio turns audio off instead of switching to the computer\u2019s own microphone and speakers (which could howl through an amp plugin), says so, and reconnects on its own when the interface is back.',
    'Every dropdown list (metronome, exercises, log filters, dialogs, Fretboard Trainer settings, suggestion boxes) opens in your color scheme instead of the system\u2019s colors.',
    'Fretboard Trainer note scores use your color scheme\u2019s colors, so the text is always readable.',
    'Mac: updating from inside Studio now works and reopens Studio (the update script was being written with Windows line endings). A log is kept in ~/Library/Logs.',
    'Clicking the version number shows What\u2019s new from the top (it was hidden under the header bar).',
    'The selected menu tab no longer has a line across its top.',
    'Fretboard Trainer voice spellings: press Enter in any box to hear it, tick flat or sharp to hear \u201cA flat\u201d and so on, and pick from suggested spellings (also listed under Spelling ideas).',
    'If your audio interface can\u2019t be opened when Studio starts (for example right after an update), Studio keeps trying and never replaces your saved choice; the exercise\u2019s plugin loads as soon as the interface opens.',
    'Switching exercises with time on the clock asks in a dialog: Log it, Discard and switch, or Cancel.',
    'Mute input sits just left of Tuner, and the mute buttons keep their words and size when on.',
    'No scroll bar in the Fretboard Trainer settings.'
  ]],
  ['0.7.0', 'Nov 19, 2026', [
    'Mute input, in the top bar next to Tuner, turns your audio interface\u2019s input off completely: no sound, no level, nothing listening.',
    'Mute guitar is in the Fretboard Trainer (top right): the trainer still hears you, you just don\u2019t hear yourself. It only applies in the trainer.',
    'The Practice page no longer has a Mute button or input meter; the input meter is in Settings \u2192 Audio.',
    'All Fretboard Trainer settings are in Settings. The trainer listens whenever it\u2019s running, so the Start listening button is gone, and Start lights up while running like other buttons.',
    'Mute buttons follow the color scheme.'
  ]],
  ['0.6.0', 'Nov 19, 2026', [
    'A Mute button for your guitar in the top bar, next to Tuner.',
    'Settings has a Privacy section that lists what Studio uses, and a switch for checking for updates at startup.',
    'Tuner and trainer settings (A4 reference, input boost, noise gate) are back in Settings.',
    'Studio\u2019s own windows (Audio settings, Scan for plugins) and their title bars follow the color scheme.',
    'Plugin windows always open with their title bar on screen.',
    'Space always plays and pauses, even after clicking a button; and if the timer is running without the metronome, Play starts the metronome.'
  ]],
  ['0.5.1', 'Nov 19, 2026', [
    'Metronome patterns are Basic, Backbeat, Custom and Clear. Each exercise remembers its choice, and Custom is that exercise\u2019s own grid: tap the squares and it\u2019s kept with the exercise.',
    'The Tuner tab listens through Studio\u2019s audio interface instead of the computer\u2019s microphone.',
    'The tempo graph bars respond to every click.',
    'The metronome always stops when a countdown runs out or a session is discarded; the \u201cStart and stop with the practice timer\u201d setting is gone.',
    'Tempo ladder: click a bar before pressing Play to start the ladder from that step.',
    'Holding on a tempo ladder step just shows \u201cholding\u201d in blue; the bars no longer shift.',
    'Fixes for the Brass scheme: themed effort-level boxes, effort colors from easy to hard, and brass sliders, checkboxes and links. The Fretboard Trainer\u2019s fret numbers are no longer cut off.'
  ]],
  ['0.5.0', 'Nov 19, 2026', [
    'New Studio icon: the practice pick as a tarnished brass pocket watch, with real gears behind the dial.',
    'New Brass color scheme with polished-brass buttons, now the default. Your old scheme is still in Settings \u2192 Color scheme.'
  ]],
  ['0.4.0', 'Nov 18, 2026', [
    'Studio is built for Windows and Mac on GitHub and updates itself: when a newer build is out, Studio offers it, and Settings → Studio has Check for updates.',
    'Windows: ASIO audio interfaces are supported for low latency.',
    'Each exercise has a Metronome setting (the switch on the Exercise card, or Edit exercise). Turn it off for jamming: Play then runs only the practice timer.',
    'The Fretboard Trainer hears your guitar through Studio\u2019s audio interface, and its Mute button silences your guitar in Studio\u2019s output.',
    'The trainer\u2019s third mode is called Play the note, and the voice says E as \u201ce\u201d.'
  ]],
  ['0.3.0', 'Nov 18, 2026', [
    'One Play/Pause and Stop in the Metronome card run the metronome, the tempo ladder and the practice timer. Space plays and pauses.',
    'The Tempo ladder is a switch. The ladder graph is always shown: with the switch off, click a bar to pick a percentage of your goal; with it on, it shows your place in the ladder, click a bar to jump there, and click the current bar to hold that tempo (blue).',
    'Switching Stopwatch/Countdown with time on the clock asks to discard it first. Studio\u2019s own dialogs replace the pop-ups its window couldn\u2019t show.'
  ]],
  ['0.2.0', 'Nov 18, 2026', [
    'Each exercise has its own effect slot at the bottom of the Exercise card. Click the plugin name to open the plugin, or the ▾ to pick another one, with search and your recent plugins.',
    'Each exercise remembers its plugin and that plugin’s settings. Only one plugin runs at a time; switching exercises switches it.',
    'An empty slot plays your raw guitar. Mute silences Studio’s output; it turns red while muted.',
    'Settings → Audio has the audio interface and Scan for plugins.'
  ]],
  ['0.1.0', 'Nov 18, 2026', [
    'First test build of Guitar Practice Studio: the practice app in its own window, plus playing your guitar through an amp-sim or effect plugin (VST3 or AudioUnit).'
  ]]
];

(function () {
  var backend = window.__JUCE__ && window.__JUCE__.backend;
  function send(action, extra) {
    if (!backend) return;
    var msg = { action: action };
    for (var k in (extra || {})) msg[k] = extra[k];
    backend.emitEvent('studio', msg);
  }

  // The page announces the selected exercise; Studio loads that exercise's plugin.
  addEventListener('gp:exercise', function (e) {
    send('exercise', { id: e.detail && e.detail.id != null ? String(e.detail.id) : '' });
  });

  var CSS =
    '.fxrow{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:12px;padding-top:12px;border-top:1px solid var(--line);position:relative}' +
    '.fxsplit{display:inline-flex}.fxsplit button{border-radius:0}' +
    '.fxsplit .fxmain{border-radius:9px 0 0 9px;font-weight:600;min-width:200px;text-align:left}' +
    '.fxsplit .fxmain.empty{font-weight:400;color:var(--mut)}' +
    '.fxsplit .fxcar{border-radius:0 9px 9px 0;border-left:0;padding-left:10px;padding-right:10px}' +
    '.fxmute{font-weight:600}.fxmute.on{background:var(--bad);border-color:var(--bad);color:#fff}' +
    // Same look as the Stopwatch / Countdown buttons, including the brass finish when on.
    'nav#tabs button.navmute{align-self:center;margin:0 6px;padding:7px 14px;background:var(--card);border:1px solid var(--line);border-radius:10px;color:var(--fg);font-weight:400}' +
    'nav#tabs button.navmute.on{background:var(--acc);border-color:var(--acc);color:var(--accfg)}' +
    'html:root[data-skin="brass"] nav#tabs button.navmute.on{background:linear-gradient(180deg,#f3d98e 0%,#d4a64a 45%,#9a6c24 100%);border-color:#7a5418;color:#1a1205;' +
      'text-shadow:0 1px 0 rgba(255,240,200,.5);box-shadow:inset 0 1px 0 rgba(255,250,220,.6),0 1px 2px rgba(0,0,0,.6)}' +
    '.pvrow{display:grid;grid-template-columns:150px 1fr;gap:14px;padding:9px 0;border-top:1px solid var(--line)}' +
    '.pvrow b{font-weight:600}.pvrow .sub{font-size:14px;line-height:1.45}' +
    '@media (max-width:640px){.pvrow{grid-template-columns:1fr;gap:4px}}' +
    '.fxmeter{flex:1;min-width:60px;max-width:150px;height:8px;border-radius:4px;background:var(--soft);overflow:hidden;border:1px solid var(--line)}' +
    '.fxmeter i{display:block;height:100%;width:0;background:var(--ok)}' +
    '.fxmsg{flex-basis:100%;color:var(--mut);font-size:13px}.fxmsg:empty{display:none}' +
    '.fxpop{position:absolute;top:100%;left:0;margin-top:6px;width:min(440px,100%);background:var(--card);border:1px solid var(--line);border-radius:12px;padding:10px;box-shadow:0 18px 50px rgba(0,0,0,.45);z-index:60}' +
    '.fxpop input{width:100%;box-sizing:border-box;font-size:15px;padding:8px 10px}' +
    '.fxlist{max-height:320px;overflow:auto;margin-top:6px}' +
    '.fxhd{font-size:11px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:8px 6px 4px}' +
    '.fxit{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:7px 10px;border-radius:8px;cursor:pointer}' +
    '.fxit:hover{background:var(--soft)}.fxit.sel{background:var(--soft);outline:2px solid var(--acc);outline-offset:-2px}' +
    '.fxit small{color:var(--mut)}.fxbadge{font-size:11px;border:1px solid var(--line);border-radius:6px;padding:1px 6px;color:var(--mut);white-space:nowrap}' +
    '.fxfoot{display:flex;justify-content:space-between;align-items:center;border-top:1px solid var(--line);margin-top:8px;padding-top:8px;color:var(--mut);font-size:13px}' +
    '.fxfoot button{font-size:13px}';

  var st = { plugin: '', muted: true, message: '', device: '', build: 0 };
  var upd = { state: '', build: 0, error: '' };
  // Lets the Fretboard Trainer (an iframe on this page) show and use the same guitar mute.
  var studioMuteListeners = [];
  window.studioGuitarMute = {
    // The guitar mute is for the Fretboard Trainer only (it still hears you, you just don't hear
    // yourself). It's remembered for the trainer and lifted everywhere else.
    get: function () { return !!st.muted; },
    set: function (on) { try { localStorage.setItem('studio.trainerMute', on ? '1' : '0'); } catch (e) {} send('mute', { on: !!on }); },
    onChange: function (f) { studioMuteListeners.push(f); }
  };
  // Lets the Fretboard Trainer hear the guitar through Studio's audio interface (Settings → Audio interface).
  // Several parts of the page (the Tuner tab, the Fretboard Trainer) can listen at once;
  // Studio sends the guitar while at least one of them is listening.
  var inputListeners = {}, inputNext = 1;
  window.studioGuitarInput = {
    start: function (f) {
      var id = inputNext++; inputListeners[id] = f;
      send('listen', { on: true });
      return id;
    },
    stop: function (id) {
      delete inputListeners[id];
      if (!Object.keys(inputListeners).length) send('listen', { on: false });
    }
  };
  var plugins = [], recent = [];

  function el(id) { return document.getElementById(id); }
  function esc(t) { return String(t).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }

  // One entry per plugin: prefer the VST3 when the same plugin is also an AudioUnit.
  function uniquePlugins() {
    var by = {};
    plugins.forEach(function (p) {
      var key = (p.name + '|' + p.maker).toLowerCase();
      if (!by[key] || (p.format === 'VST3' && by[key].format !== 'VST3')) by[key] = p;
    });
    return Object.keys(by).map(function (k) { return by[k]; })
      .sort(function (a, b) { return a.name.localeCompare(b.name); });
  }

  function renderSlot() {
    var main = el('fxMain'); if (!main) return;
    main.textContent = st.plugin || 'Empty';
    main.classList.toggle('empty', !st.plugin);
    main.title = st.plugin ? 'Open ' + st.plugin : 'Pick a VST effect for this exercise';
    var nm = el('navMute');
    if (nm) {
      nm.classList.toggle('on', !!st.inputMuted);
      nm.setAttribute('aria-pressed', st.inputMuted ? 'true' : 'false');
      nm.title = st.inputMuted ? 'Studio is ignoring your audio interface’s input. Click to turn it back on.'
                               : 'Turn off your audio interface’s input (no sound, no listening)';
    }
    var au = el('pvAuto'); if (au) au.checked = st.autoUpdate !== false;
    el('fxMsg').textContent = st.message || '';
    var d = el('stDev'); if (d) d.textContent = st.device || '';
  }

  // ---- updates (Settings → Studio) ----
  function renderUpdate() {
    var t = el('stUpd'), b = el('stUpdBtn'); if (!t || !b) return;
    var cur = st.build ? 'Build ' + st.build : 'Built on this computer';
    b.disabled = upd.state === 'checking' || upd.state === 'downloading' || !st.build;
    b.textContent = upd.state === 'available' ? 'Update to build ' + upd.build : 'Check for updates';
    t.textContent = {
      checking: cur + ' \u00b7 checking\u2026',
      none: cur + ' \u00b7 up to date',
      available: cur + ' \u00b7 build ' + upd.build + ' is available',
      downloading: 'Downloading build ' + upd.build + '\u2026 Studio will restart.',
      failed: cur + ' \u00b7 ' + (upd.error || 'update failed'),
      local: cur + ' \u00b7 rebuild it to update'
    }[upd.state] || cur;
  }

  // ---- picker ----
  var pick = { open: false, items: [], idx: 0 };

  function openPicker() {
    if (pick.open) return closePicker();
    pick.open = true;
    var pop = document.createElement('div');
    pop.className = 'fxpop'; pop.id = 'fxPop';
    pop.innerHTML = '<input type="search" id="fxSearch" placeholder="Search plugins…" autocomplete="off">' +
      '<div class="fxlist" id="fxList"></div>' +
      '<div class="fxfoot"><span>↑↓ to move · Enter to load · Esc to close</span>' +
      '<button id="fxRemove">Remove plugin</button></div>';
    el('fxRow').appendChild(pop);
    el('fxRemove').disabled = !st.plugin;
    el('fxRemove').onclick = function () { send('unload'); closePicker(); };
    var q = el('fxSearch');
    q.oninput = function () { pick.idx = 0; renderList(); };
    q.onkeydown = function (e) {
      if (e.key === 'ArrowDown') { e.preventDefault(); pick.idx = Math.min(pick.items.length - 1, pick.idx + 1); renderList(); }
      else if (e.key === 'ArrowUp') { e.preventDefault(); pick.idx = Math.max(0, pick.idx - 1); renderList(); }
      else if (e.key === 'Enter') { e.preventDefault(); choose(pick.items[pick.idx]); }
      else if (e.key === 'Escape') { e.preventDefault(); closePicker(); }
    };
    send('list');
    renderList();
    q.focus();
    setTimeout(function () { document.addEventListener('mousedown', outside, true); }, 0);
  }

  function outside(e) {
    var pop = el('fxPop');
    if (pop && !pop.contains(e.target) && e.target !== el('fxCar') && e.target !== el('fxMain')) closePicker();
  }

  function closePicker() {
    pick.open = false;
    document.removeEventListener('mousedown', outside, true);
    var pop = el('fxPop'); if (pop) pop.remove();
  }

  function choose(p) {
    if (!p) return;
    closePicker();
    send('load', { id: p.id });
  }

  function renderList() {
    var list = el('fxList'); if (!list) return;
    var all = uniquePlugins();
    var words = (el('fxSearch').value || '').toLowerCase().split(/\s+/).filter(Boolean);
    var html = '', items = [];
    function row(p) {
      var i = items.length; items.push(p);
      var loaded = st.plugin && p.name === st.plugin;
      return '<div class="fxit' + (i === pick.idx ? ' sel' : '') + '" data-i="' + i + '"><span><b>' + esc(p.name) + '</b>' +
        (p.maker ? ' <small>· ' + esc(p.maker) + '</small>' : '') + '</span><span class="fxbadge">' +
        (loaded ? '✓ loaded' : esc(p.format)) + '</span></div>';
    }
    if (!plugins.length) {
      html = '<p class="sub" style="margin:10px 6px">No plugins found yet. Go to Settings → Audio → Scan for plugins.</p>';
    } else if (!words.length) {
      var rec = recent.map(function (id) { return plugins.filter(function (p) { return p.id === id; })[0]; }).filter(Boolean);
      if (rec.length) { html += '<div class="fxhd">Recent</div>' + rec.map(row).join(''); }
      html += '<div class="fxhd">All plugins (' + all.length + ')</div>' + all.map(row).join('');
    } else {
      var hits = all.filter(function (p) {
        var t = (p.name + ' ' + p.maker).toLowerCase();
        return words.every(function (w) { return t.indexOf(w) >= 0; });
      });
      html = hits.length ? '<div class="fxhd">' + hits.length + ' match' + (hits.length === 1 ? '' : 'es') + '</div>' + hits.map(row).join('')
                         : '<p class="sub" style="margin:10px 6px">No plugins match.</p>';
    }
    pick.items = items;
    if (pick.idx >= items.length) pick.idx = Math.max(0, items.length - 1);
    list.innerHTML = html;
    list.querySelectorAll('.fxit').forEach(function (d) {
      d.onclick = function () { choose(pick.items[+d.dataset.i]); };
    });
    var sel = list.querySelector('.fxit.sel'); if (sel) sel.scrollIntoView({ block: 'nearest' });
  }

  document.addEventListener('DOMContentLoaded', function () {
    var style = document.createElement('style');
    style.textContent = CSS;
    document.head.appendChild(style);

    // The web app's "Installing" instructions don't apply to Studio.
    document.querySelectorAll('.card h2').forEach(function (h) {
      if (h.textContent.trim() === 'Installing') h.closest('.card').hidden = true;
    });

    // Input mute in the top bar, just left of Tuner: always in reach, on every page.
    var tunerTab = document.querySelector('#tabs button[data-v="tuner"]');
    if (tunerTab) {
      var nm = document.createElement('button');
      nm.className = 'navmute'; nm.id = 'navMute'; nm.textContent = 'Mute input';
      nm.onclick = function () { send('inputMute', { on: !st.inputMuted }); };
      tunerTab.parentNode.insertBefore(nm, tunerTab);
    }

    // Effect slot, at the bottom of the Exercise card.
    var slot = el('fxSlot');
    if (slot) {
      slot.innerHTML = '<div class="fxrow" id="fxRow">' +
        '<span class="fxsplit"><button class="fxmain empty" id="fxMain">Empty</button>' +
        '<button class="fxcar" id="fxCar" title="Pick a VST effect" aria-label="Pick a VST effect">▾</button></span>' +

        '<div class="fxmsg" id="fxMsg"></div></div>';
      el('fxMain').onclick = function () { if (st.plugin) send('show'); else openPicker(); };
      el('fxCar').onclick = openPicker;
    }

    // Settings: Studio's audio setup replaces the web app's audio input card.
    var audio = el('audioCard');
    if (audio) {
      // The web app's input card becomes "Tuner and trainer": Studio chooses the input, so only
      // the tuning reference, boost and noise gate stay.
      var ah = audio.querySelector('h2'); if (ah) ah.textContent = 'Tuner and trainer';
      // hidden, not removed: the page still keeps them in step with the saved settings
      var hide = function (x) { if (x) x.style.display = 'none'; };
      hide(el('inTest'));
      ['inDev', 'inChan'].forEach(function (id) { var x = el(id); if (x) hide(x.closest('label')); });
      var lv = el('inLvl'); if (lv) hide(lv.closest('.f'));
      var as = audio.querySelector('p.sub'); if (as) as.textContent = 'They hear your guitar through the audio interface above.';
      var card = document.createElement('div');
      card.className = 'card';
      card.style.marginTop = '16px';
      card.innerHTML = '<h2 style="margin:0 0 6px">Audio</h2>' +
        '<div class="row" style="gap:8px;flex-wrap:wrap"><button id="stAudio">Audio interface…</button>' +
        '<button id="stScan">Scan for plugins…</button></div>' +
        '<p class="sub" id="stDev" style="margin:10px 0 0"></p>' +
        '<div class="row" style="gap:10px;align-items:center;margin-top:8px"><span class="sub">Input level</span>' +
        '<span class="fxmeter" style="max-width:260px" title="Input level"><i id="fxLvl"></i></span></div>' +
        '<h2 style="margin:18px 0 6px">Studio</h2>' +
        '<div class="row" style="gap:10px;flex-wrap:wrap;align-items:center"><button id="stUpdBtn">Check for updates</button>' +
        '<span class="sub" id="stUpd"></span></div>';
      audio.parentNode.insertBefore(card, audio);

      // Privacy: what Studio uses, in plain words.
      var row = function (t, d) { return '<div class="pvrow"><b>' + t + '</b><div class="sub">' + d + '</div></div>'; };
      var pv = document.createElement('div');
      pv.className = 'card'; pv.style.marginTop = '16px';
      pv.innerHTML = '<h2 style="margin:0 0 4px">Privacy</h2>' +
        '<p style="margin:0 0 10px">No account, no ads, no tracking. Your practice stays on this computer.</p>' +
        row('Your data', 'Your practice log, exercises and settings are saved only on this computer. Backups and CSV exports go only where you save them.' +
            '<div style="margin-top:8px"><button id="pvData">Show Studio’s folder</button></div>') +
        row('Internet', 'Only to check GitHub for a newer Studio and, if you choose, download it. Nothing about you or your practice is sent.' +
            '<label class="chk" style="margin-top:8px;color:var(--fg)"><input type="checkbox" id="pvAuto" checked> Check for updates when Studio starts</label>') +
        row('Your guitar', 'Studio hears the audio interface you choose in Audio. The sound goes to your effect, the tuner and the trainer, and is never recorded or saved. macOS calls this “Microphone” access.') +
        row('Plugins', 'Effect plugins you load are other companies’ software running inside Studio. They follow their makers’ own privacy rules.') +
        row('Spoken notes', 'The Fretboard Trainer speaks with your computer’s voices. On Windows, voices marked “Online” are spoken by Microsoft’s service.') +
        row('Open source', 'Anyone can read exactly what Studio does: <a href="#" id="pvSrc">github.com/vagend/GuitarPracticeTracker-Studio</a>');
      audio.parentNode.insertBefore(pv, audio.nextSibling);
      el('pvData').onclick = function () { send('showData'); };
      el('pvSrc').onclick = function (e) { e.preventDefault(); send('openSource'); };
      el('pvAuto').onchange = function (e) { st.autoUpdate = e.target.checked; send('autoUpdate', { on: e.target.checked }); };
      el('stAudio').onclick = function () { send('audio'); };
      el('stScan').onclick = function () { send('scan'); };
      el('stUpdBtn').onclick = function () { if (upd.state === 'available') send('update'); else send('checkUpdate'); };
    }

    if (backend) {
      backend.addEventListener('studioStatus', function (s) {
        st = { plugin: s.plugin || '', muted: !!s.muted, inputMuted: !!s.inputMuted, message: s.message || '', device: s.device || '', build: +s.build || 0, autoUpdate: s.autoUpdate !== false };
        renderUpdate();
        renderSlot();
        studioMuteListeners.forEach(function (f) { try { f(st.muted); } catch (e) {} });
        if (pick.open) { el('fxRemove').disabled = !st.plugin; renderList(); }
      });
      backend.addEventListener('studioPlugins', function (p) {
        plugins = p.plugins || []; recent = p.recent || [];
        if (pick.open) renderList();
      });
      backend.addEventListener('studioUpdate', function (u) {
        upd = { state: u.state || '', build: +u.build || upd.build, error: u.error || '' };
        renderUpdate();
        if (upd.state === 'available' && typeof window.toast === 'function')
          window.toast('Studio build ' + upd.build + ' is available', 'Update', function () { send('update'); });
      });
      backend.addEventListener('studioInput', function (m) {
        for (var k in inputListeners) { try { inputListeners[k](m.sr, m.d); } catch (e) {} }
      });
      backend.addEventListener('studioLevel', function (l) {
        var bar = el('fxLvl'); if (!bar) return;
        var v = Math.max(0, Math.min(1, +l.level || 0));
        bar.style.width = (v * 100) + '%';
        bar.style.background = v > 0.9 ? 'var(--bad)' : 'var(--ok)';
      });
    }
    renderSlot();

    var trainerMute = function () { try { return localStorage.getItem('studio.trainerMute') === '1'; } catch (e) { return false; } };
    var applyViewMute = function (v) { send('mute', { on: v === 'trainer' && trainerMute() }); };
    if (typeof window.show === 'function') {
      var origShow = window.show;
      window.show = function (v) { origShow(v); applyViewMute(v); };
    }
    applyViewMute(typeof view !== 'undefined' ? view : 'practice');

    // Fretboard Trainer settings live in Settings, with everything else.
    var tt = el('audioCard');
    if (tt) {
      var fc = document.createElement('div');
      fc.className = 'card'; fc.style.marginTop = '16px';
      fc.innerHTML = '<h2 style="margin:0 0 6px">Fretboard Trainer</h2>' +
        '<iframe id="trSetFrame" scrolling="no" title="Fretboard Trainer settings" style="width:100%;border:0;display:block;height:600px;background:transparent;overflow:hidden"></iframe>';
      tt.parentNode.insertBefore(fc, tt.nextSibling);
      var fr = el('trSetFrame');
      fr.addEventListener('load', function () {
        try {
          var doc = fr.contentDocument, m = doc.querySelector('main');
          var fit = function () { var r = m.getBoundingClientRect(); var h = r.bottom + (doc.defaultView.scrollY || 0); if (h > 0) fr.style.height = Math.ceil(h + 2) + 'px'; };
          new ResizeObserver(fit).observe(m); new ResizeObserver(fit).observe(fr); fit();
        } catch (e) {}
      });
      fr.src = 'trainer.html?embed=1&settings=1';
    }

    // Studio's own windows (Audio settings, Scan for plugins…) follow the colour scheme.
    var probe = document.createElement('i'); probe.style.display = 'none'; document.body.appendChild(probe);
    var hex = function (v) {
      probe.style.color = 'var(' + v + ')';
      var m = getComputedStyle(probe).color.match(/\d+(\.\d+)?/g) || [0, 0, 0];
      return '#' + m.slice(0, 3).map(function (x) { return ('0' + Math.round(+x).toString(16)).slice(-2); }).join('');
    };
    var sendTheme = function () {
      send('theme', { bg: hex('--bg'), card: hex('--card'), line: hex('--line'), fg: hex('--fg'),
                      acc: hex('--acc'), accfg: hex('--accfg'), soft: hex('--soft') });
    };
    sendTheme();
    new MutationObserver(sendTheme).observe(document.documentElement, { attributes: true, attributeFilter: ['data-skin', 'class', 'style'] });
  });
})();
