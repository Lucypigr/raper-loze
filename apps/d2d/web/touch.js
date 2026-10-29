// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp — touch controls for phones and tablets, in landscape.
//
// The game reads a mouse and a keyboard. A tap is already the left button
// (SDL turns a canvas touch into one); this adds what a touch screen lacks,
// as the same DOM events SDL's Emscripten backend listens for, so d2d itself
// doesn't change: buttons in the bars beside the 4:3 picture press keys
// (belt 1-4, inventory, map...), the Skill button makes taps right-clicks,
// and the keyboard button opens the on-screen keyboard for names and chat.
// index.html attaches it only on a device whose main pointer is a finger.
"use strict";

const D2dTouch = (() => {
  // [label, KeyboardEvent.code, KeyboardEvent.key, keyCode] — d2d's default
  // keys (apps/d2d/keyconfig.hpp).
  const kLeft = [
    ["選單<br>Menu", "Escape", "Escape", 27],
    ["地圖<br>Map", "Tab", "Tab", 9],
    ["藥水 1", "Digit1", "1", 49],
    ["藥水 2", "Digit2", "2", 50],
    ["藥水 3", "Digit3", "3", 51],
    ["藥水 4", "Digit4", "4", 52],
  ];
  const kRight = [
    ["背包<br>Inv", "KeyI", "i", 73],
    ["角色<br>Char", "KeyC", "c", 67],
    ["技能樹<br>Skills", "KeyT", "t", 84],
    ["任務<br>Quests", "KeyQ", "q", 81],
    ["物品<br>Items", "AltLeft", "Alt", 18],
    ["跑/走<br>Run", "KeyR", "r", 82],
  ];
  const kMore = [
    ["F1", "F1", "F1", 112], ["F2", "F2", "F2", 113], ["F3", "F3", "F3", 114], ["F4", "F4", "F4", 115],
    ["F5", "F5", "F5", 116], ["F6", "F6", "F6", 117], ["F7", "F7", "F7", 118], ["F8", "F8", "F8", 119],
    ["快捷技能<br>Skill bar", "KeyS", "s", 83],
    ["換武器<br>Swap", "KeyW", "w", 87],
    ["傭兵<br>Merc", "KeyO", "o", 79],
    ["聊天<br>Chat", "Enter", "Enter", 13],
    ["關閉<br>Close", "Space", " ", 32],
  ];

  const key = (type, code, keyName, keyCode, charCode = 0) =>
    window.dispatchEvent(new KeyboardEvent(type, { code, key: keyName, keyCode, which: keyCode, charCode, bubbles: true, cancelable: true }));

  // One character typed: keydown, keypress (SDL's text comes from its
  // charCode), keyup.
  function typeChar(char) {
    const upper = char.toUpperCase();
    const code = /[a-z]/i.test(char) ? "Key" + upper : /[0-9]/.test(char) ? "Digit" + char : char === " " ? "Space" : "";
    const keyCode = /[a-z0-9]/i.test(char) ? upper.charCodeAt(0) : char === " " ? 32 : 0;
    key("keydown", code, char, keyCode);
    key("keypress", code, char, char.charCodeAt(0), char.charCodeAt(0));
    key("keyup", code, char, keyCode);
  }

  function button(label, onDown, onUp, extraClass = "") {
    const element = document.createElement("button");
    element.className = "d2t-button " + extraClass;
    element.innerHTML = label;
    element.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      element.setPointerCapture(event.pointerId);
      element.classList.add("d2t-down");
      onDown();
    });
    const up = (event) => {
      event.preventDefault();
      if (!element.classList.contains("d2t-down")) return;
      element.classList.remove("d2t-down");
      if (onUp) onUp();
    };
    element.addEventListener("pointerup", up);
    element.addEventListener("pointercancel", up);
    element.addEventListener("contextmenu", (event) => event.preventDefault());
    return element;
  }
  // A key held as long as the button is.
  const keyButton = ([label, code, keyName, keyCode]) =>
    button(label, () => key("keydown", code, keyName, keyCode), () => key("keyup", code, keyName, keyCode));

  function attach(canvas) {
    const style = document.createElement("style");
    style.textContent = `
      body.d2t { -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; overscroll-behavior: none; }
      .d2t-bar { position: fixed; top: 0; bottom: 0; width: var(--d2t-bar); display: flex; flex-direction: column;
                 justify-content: space-between; gap: 4px; padding: max(6px, env(safe-area-inset-top)) 4px max(6px, env(safe-area-inset-bottom));
                 box-sizing: border-box; z-index: 10; pointer-events: none; }
      .d2t-left { left: env(safe-area-inset-left); }
      .d2t-right { right: env(safe-area-inset-right); }
      .d2t-group { display: flex; flex-direction: column; gap: 4px; }
      .d2t-button { pointer-events: auto; touch-action: none; flex: 0 0 auto; min-height: 34px; padding: 2px; border-radius: 8px;
                    border: 1px solid rgba(199,179,119,.55); background: rgba(20,16,8,.55); color: #e6d6a0;
                    font: 600 11px/1.15 system-ui, sans-serif; text-align: center; }
      .d2t-button.d2t-down, .d2t-button.d2t-on { background: rgba(160,120,40,.75); color: #fff; }
      .d2t-skill { min-height: 64px; font-size: 13px; border-color: rgba(220,120,90,.8); }
      .d2t-more { position: fixed; left: 50%; bottom: 8px; transform: translateX(-50%); display: none; flex-wrap: wrap; gap: 4px;
                  justify-content: center; width: min(70vw, 520px); z-index: 11; padding: 6px; border-radius: 10px; background: rgba(0,0,0,.75); }
      .d2t-more.d2t-open { display: flex; }
      .d2t-more .d2t-button { width: 60px; }
      .d2t-typing { position: fixed; left: -1000px; top: 0; opacity: 0; width: 1px; height: 1px; }
      .d2t-rotate { position: fixed; inset: 0; z-index: 20; display: none; align-items: center; justify-content: center; text-align: center;
                    background: #000; color: #c7b377; font: 18px/1.6 system-ui, sans-serif; padding: 24px; }
      @media (orientation: portrait) { .d2t-rotate { display: flex; } }
    `;
    document.head.append(style);
    document.body.classList.add("d2t");

    // The bars fill the black beside the 4:3 picture, at least 64 px.
    const sizeBars = () => {
      const side = (innerWidth - innerHeight * 4 / 3) / 2;
      document.documentElement.style.setProperty("--d2t-bar", Math.max(64, Math.min(side, 110)) + "px");
    };
    sizeBars();
    addEventListener("resize", sizeBars);

    const rotate = document.createElement("div");
    rotate.className = "d2t-rotate";
    rotate.innerHTML = "請把手機轉成橫向<br>Turn your device sideways";
    document.body.append(rotate);

    // Skill mode: taps on the picture are the right button (cast the
    // right-hand skill), until it's switched off again.
    let skillMode = false;
    let skillTouch = null;
    const mouse = (type, target, touch, buttons) =>
      target.dispatchEvent(new MouseEvent(type, { clientX: touch.clientX, clientY: touch.clientY, screenX: touch.screenX, screenY: touch.screenY,
                                                  button: 2, buttons, bubbles: true, cancelable: true, view: window }));
    // Capture on window runs before SDL's own touch handlers on the canvas.
    addEventListener("touchstart", (event) => {
      if (!skillMode || event.target !== canvas || skillTouch !== null) return;
      event.stopImmediatePropagation();
      event.preventDefault();
      const touch = event.changedTouches[0];
      skillTouch = touch.identifier;
      mouse("mousemove", canvas, touch, 0);
      mouse("mousedown", canvas, touch, 2);
    }, { capture: true, passive: false });
    const follow = (type) => (event) => {
      if (skillTouch === null) return;
      const touch = [...event.changedTouches].find((each) => each.identifier === skillTouch);
      if (!touch) return;
      event.stopImmediatePropagation();
      event.preventDefault();
      if (type === "move") { mouse("mousemove", canvas, touch, 2); return; }
      mouse("mouseup", document, touch, 0);
      skillTouch = null;
    };
    addEventListener("touchmove", follow("move"), { capture: true, passive: false });
    addEventListener("touchend", follow("end"), { capture: true, passive: false });
    addEventListener("touchcancel", follow("end"), { capture: true, passive: false });

    // The on-screen keyboard: a hidden field whose typing goes on as keys.
    const typing = document.createElement("input");
    typing.className = "d2t-typing";
    typing.autocapitalize = "off";
    typing.autocomplete = "off";
    typing.spellcheck = false;
    const kSentinel = " ";   // something for the keyboard's backspace to delete
    typing.value = kSentinel;
    for (const type of ["keydown", "keypress", "keyup"])
      typing.addEventListener(type, (event) => {
        if (!event.isTrusted) return;
        event.stopPropagation();   // the field's own keys: sent below, once
        if (type === "keydown" && event.key === "Enter") {
          event.preventDefault();
          key("keydown", "Enter", "Enter", 13);
          key("keyup", "Enter", "Enter", 13);
        }
      });
    typing.addEventListener("input", (event) => {
      if (event.inputType === "deleteContentBackward" || typing.value.length < kSentinel.length) {
        key("keydown", "Backspace", "Backspace", 8);
        key("keyup", "Backspace", "Backspace", 8);
      } else {
        for (const char of typing.value.slice(kSentinel.length)) typeChar(char);
      }
      typing.value = kSentinel;
    });
    document.body.append(typing);

    const more = document.createElement("div");
    more.className = "d2t-more";
    for (const entry of kMore) more.append(keyButton(entry));
    document.body.append(more);

    const left = document.createElement("div");
    left.className = "d2t-bar d2t-left";
    const leftTop = document.createElement("div");
    leftTop.className = "d2t-group";
    for (const entry of kLeft.slice(0, 2)) leftTop.append(keyButton(entry));
    // Focused on click, not pointerdown: iOS opens its keyboard only then.
    const keyboard = button("鍵盤<br>Keys", () => {});
    keyboard.addEventListener("click", () => {
      if (document.activeElement === typing) { typing.blur(); canvas.focus(); }
      else typing.focus();
    });
    leftTop.append(keyboard);
    const fullscreen = button("全螢幕<br>Full", async () => {
      try {
        if (document.fullscreenElement) await document.exitFullscreen();
        else {
          await document.documentElement.requestFullscreen({ navigationUI: "hide" });
          await screen.orientation?.lock?.("landscape").catch(() => {});
        }
      } catch { /* iPhone Safari: no fullscreen for pages; Add to Home Screen instead */ }
    });
    if (document.documentElement.requestFullscreen) leftTop.append(fullscreen);
    const leftBottom = document.createElement("div");
    leftBottom.className = "d2t-group";
    for (const entry of kLeft.slice(2)) leftBottom.append(keyButton(entry));
    left.append(leftTop, leftBottom);

    const right = document.createElement("div");
    right.className = "d2t-bar d2t-right";
    const rightTop = document.createElement("div");
    rightTop.className = "d2t-group";
    for (const entry of kRight) rightTop.append(keyButton(entry));
    const rightBottom = document.createElement("div");
    rightBottom.className = "d2t-group";
    const moreButton = button("更多<br>More", () => {
      more.classList.toggle("d2t-open");
      moreButton.classList.toggle("d2t-on", more.classList.contains("d2t-open"));
    });
    const skill = button("技能<br>Skill", () => {
      skillMode = !skillMode;
      skill.classList.toggle("d2t-on", skillMode);
    }, null, "d2t-skill");
    rightBottom.append(moreButton, skill);
    right.append(rightTop, rightBottom);

    document.body.append(left, right);
  }

  // A finger is the main pointer (phones, tablets), not a mouse.
  const isTouchDevice = () => matchMedia("(pointer: coarse)").matches && navigator.maxTouchPoints > 0;

  return { attach, isTouchDevice };
})();
