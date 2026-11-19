// Themed dropdown lists for Guitar Practice Studio.
// The closed boxes are ordinary <select> elements (already styled by the page). Windows and
// macOS draw the list that opens in their own colours, so this opens a list drawn by the page
// instead, in the chosen colour scheme. Text boxes with suggestions (<input list>) get the same
// list. The real <select> keeps its value and fires its usual change event, so nothing else
// in the app needs to know about this.
(function () {
  'use strict';
  if (window.__themedSelects) return;
  window.__themedSelects = true;

  var css =
    '.tsel-pop{position:fixed;z-index:2147483000;box-sizing:border-box;overflow-y:auto;overscroll-behavior:contain;' +
    'background:var(--card,#222);color:var(--fg,#eee);border:1px solid var(--line,#444);border-radius:8px;' +
    'box-shadow:0 8px 24px rgba(0,0,0,.45);padding:4px;font:inherit;font-size:14px;outline:none}' +
    '.tsel-pop .o{padding:6px 10px 6px 26px;border-radius:6px;cursor:pointer;white-space:nowrap;position:relative;line-height:1.3}' +
    '.tsel-pop .o.sel::before{content:"\\2713";position:absolute;left:8px;color:var(--acc,#fb923c)}' +
    '.tsel-pop .o.act{background:var(--acc,#fb923c);color:var(--accfg,#111)}' +
    '.tsel-pop .o.act.sel::before{color:inherit}' +
    '.tsel-pop .o.dis{opacity:.45;cursor:default}' +
    '.tsel-pop .g{padding:6px 10px 2px;font-size:12px;font-weight:600;color:var(--mut,#999);text-transform:none}';
  function addStyle() {
    var st = document.createElement('style');
    st.textContent = css;
    (document.head || document.documentElement).appendChild(st);
  }
  if (document.head) addStyle(); else document.addEventListener('DOMContentLoaded', addStyle);

  var pop = null, src = null, items = [], act = -1, typed = '', typedAt = 0;

  function usable(sel) {
    return sel && !sel.disabled && !sel.multiple && !(sel.size > 1);
  }

  // The choices for a <select> or for an <input list="...">.
  function readItems(el) {
    var out = [];
    if (el.tagName === 'SELECT') {
      Array.prototype.forEach.call(el.children, function (c) {
        if (c.tagName === 'OPTGROUP') {
          out.push({ group: c.label });
          Array.prototype.forEach.call(c.children, function (o) {
            out.push({ opt: o, text: o.textContent, value: o.value, dis: o.disabled || c.disabled, sel: o.selected });
          });
        } else if (c.tagName === 'OPTION' && !c.hidden) {
          out.push({ opt: c, text: c.textContent, value: c.value, dis: c.disabled, sel: c.selected });
        }
      });
    } else {
      var dl = document.getElementById(el.dataset.tlist), cur = el.value;
      if (dl) Array.prototype.forEach.call(dl.querySelectorAll('option'), function (o) {
        out.push({ text: o.label || o.value, value: o.value, sel: o.value === cur });
      });
    }
    return out;
  }

  function close(refocus) {
    if (!pop) return;
    pop.remove();
    pop = null;
    var s = src;
    src = null;
    if (refocus && s) try { s.focus({ preventScroll: true }); } catch (e) {}
  }

  function setActive(i, scroll) {
    if (!pop) return;
    var nodes = pop.querySelectorAll('.o');
    if (act >= 0 && nodes[act]) nodes[act].classList.remove('act');
    act = i;
    if (act >= 0 && nodes[act]) {
      nodes[act].classList.add('act');
      if (scroll) nodes[act].scrollIntoView({ block: 'nearest' });
    }
  }

  function choose(i) {
    var choices = items.filter(function (x) { return !x.group; });
    var it = choices[i];
    if (!it || it.dis || !src) return;
    var el = src;
    close(true);
    if (el.tagName === 'SELECT') {
      if (el.value === it.value && it.opt.selected) return;
      it.opt.selected = true;
    } else {
      el.value = it.value;
    }
    el.dispatchEvent(new Event('input', { bubbles: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function open(el) {
    close(false);
    items = readItems(el);
    if (!items.some(function (x) { return !x.group; })) return;
    src = el;
    pop = document.createElement('div');
    pop.className = 'tsel-pop';
    pop.setAttribute('role', 'listbox');
    pop.tabIndex = -1;
    var n = 0, start = -1;
    items.forEach(function (it) {
      var d = document.createElement('div');
      if (it.group) {
        d.className = 'g';
        d.textContent = it.group;
      } else {
        var idx = n++;
        d.className = 'o' + (it.sel ? ' sel' : '') + (it.dis ? ' dis' : '');
        d.setAttribute('role', 'option');
        d.setAttribute('aria-selected', it.sel ? 'true' : 'false');
        d.textContent = it.text;
        d.addEventListener('mousemove', function () { if (act !== idx && !it.dis) setActive(idx, false); });
        d.addEventListener('mousedown', function (e) { e.preventDefault(); });
        d.addEventListener('click', function () { choose(idx); });
        if (it.sel && start < 0) start = idx;
      }
      pop.appendChild(d);
    });
    // Inside an open dialog the list must sit in the dialog, or the dialog would cover it.
    var host = el.closest('dialog[open]') || document.body;
    host.appendChild(pop);

    var r = el.getBoundingClientRect(), vh = window.innerHeight, vw = window.innerWidth;
    var below = vh - r.bottom - 6, above = r.top - 6;
    var natural = pop.scrollHeight;
    var down = below >= Math.min(natural, 240) || below >= above;
    var room = Math.max(120, (down ? below : above) - 4);
    pop.style.maxHeight = Math.min(natural, room, 420) + 'px';
    pop.style.minWidth = r.width + 'px';
    pop.style.maxWidth = Math.max(r.width, vw - 16) + 'px';
    var w = pop.offsetWidth, h = pop.offsetHeight;
    var left = Math.min(Math.max(8, r.left), vw - w - 8);
    var top = down ? r.bottom + 2 : r.top - h - 2;
    pop.style.left = Math.max(0, left) + 'px';
    pop.style.top = Math.max(0, top) + 'px';
    setActive(start >= 0 ? start : 0, true);
  }

  function choiceCount() { return items.filter(function (x) { return !x.group; }).length; }
  function step(from, dir) {
    var choices = items.filter(function (x) { return !x.group; }), i = from;
    for (var k = 0; k < choices.length; k++) {
      i += dir;
      if (i < 0 || i >= choices.length) return from;
      if (!choices[i].dis) return i;
    }
    return from;
  }

  // Open on click instead of the system list.
  document.addEventListener('mousedown', function (e) {
    if (e.button !== 0) return;
    var t = e.target;
    if (pop && pop.contains(t)) return;
    var sel = t.closest && t.closest('select');
    if (sel && usable(sel)) {
      e.preventDefault();
      try { sel.focus({ preventScroll: true }); } catch (x) {}
      if (pop && src === sel) close(false); else open(sel);
      return;
    }
    // A suggestions box opens its list from the arrow at its right edge.
    var inp = t.closest && t.closest('input[data-tlist]');
    if (inp) {
      var r = inp.getBoundingClientRect();
      if (e.clientX > r.right - 28) {
        e.preventDefault();
        try { inp.focus({ preventScroll: true }); } catch (x) {}
        if (pop && src === inp) close(false); else open(inp);
        return;
      }
    }
    close(false);
  }, true);

  document.addEventListener('keydown', function (e) {
    if (pop) {
      var choices = choiceCount();
      if (e.key === 'ArrowDown') { setActive(step(act, 1), true); }
      else if (e.key === 'ArrowUp') { setActive(step(act, -1), true); }
      else if (e.key === 'Home') { setActive(step(-1, 1), true); }
      else if (e.key === 'End') { setActive(step(choices, -1), true); }
      else if (e.key === 'PageDown') { for (var i = 0; i < 8; i++) act = step(act, 1); setActive(act, true); }
      else if (e.key === 'PageUp') { for (var j = 0; j < 8; j++) act = step(act, -1); setActive(act, true); }
      else if (e.key === 'Enter' || e.key === ' ') { if (e.key === ' ' && src && src.tagName === 'INPUT') { close(false); return; } choose(act); }
      else if (e.key === 'Escape') { close(true); }
      else if (e.key === 'Tab') { close(false); return; }
      else if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey) {
        if (src && src.tagName === 'INPUT') { close(false); return; }
        var now = Date.now();
        typed = (now - typedAt < 700 ? typed : '') + e.key.toLowerCase();
        typedAt = now;
        var list = items.filter(function (x) { return !x.group; });
        for (var k = 0; k < list.length; k++)
          if (!list[k].dis && list[k].text.trim().toLowerCase().indexOf(typed) === 0) { setActive(k, true); break; }
      }
      else return;
      e.preventDefault();
      e.stopPropagation();
      return;
    }
    var t = e.target;
    if (t && t.tagName === 'SELECT' && usable(t)) {
      // The keys that would open the system list open this one instead.
      if (e.key === ' ' || e.key === 'Enter' || (e.altKey && (e.key === 'ArrowDown' || e.key === 'ArrowUp')) ||
          (/Mac/.test(navigator.platform) && (e.key === 'ArrowDown' || e.key === 'ArrowUp'))) {
        e.preventDefault();
        e.stopPropagation();
        open(t);
      }
    } else if (t && t.tagName === 'INPUT' && t.dataset.tlist && (e.key === 'ArrowDown' || (e.altKey && e.key === 'ArrowDown'))) {
      e.preventDefault();
      open(t);
    }
  }, true);

  // Close when the page moves under the list or the window loses focus.
  window.addEventListener('scroll', function (e) { if (pop && !(e.target && pop.contains && e.target.nodeType === 1 && pop.contains(e.target))) close(false); }, true);
  window.addEventListener('resize', function () { close(false); });
  window.addEventListener('blur', function () { close(false); });

  // Text boxes with suggestions: take the list off the box so the system never shows its own,
  // draw an arrow at the right edge, and open the themed list from there (or with the down arrow key).
  function adoptLists() {
    Array.prototype.forEach.call(document.querySelectorAll('input[list]'), function (inp) {
      inp.dataset.tlist = inp.getAttribute('list');
      inp.removeAttribute('list');
      inp.setAttribute('autocomplete', 'off');
    });
  }
  var arrowCss = 'input[data-tlist]{background-image:linear-gradient(45deg,transparent 50%,var(--mut,#999) 50%),linear-gradient(135deg,var(--mut,#999) 50%,transparent 50%)!important;' +
    'background-position:calc(100% - 15px) 55%,calc(100% - 10px) 55%!important;background-size:5px 5px,5px 5px!important;background-repeat:no-repeat!important;padding-right:28px!important}';
  function startLists() {
    var st = document.createElement('style');
    st.textContent = arrowCss;
    document.head.appendChild(st);
    adoptLists();
    new MutationObserver(adoptLists).observe(document.body, { childList: true, subtree: true, attributes: true, attributeFilter: ['list'] });
  }
  if (document.body) startLists(); else document.addEventListener('DOMContentLoaded', startLists);
})();
