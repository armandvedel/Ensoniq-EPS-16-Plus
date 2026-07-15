(() => {
  if (location.protocol === 'file:') return;
  document.documentElement.classList.add('live-mode');

  const realDisplay = document.querySelector('#display');
  const status = document.querySelector('#live-status');
  const liveLog = document.querySelector('#log');
  const verified = new Set([
    'LOAD', 'COMMAND', 'EDIT', 'INSTRUMENT', 'SEQ-SONG', 'SYSTEM-MIDI', 'EFFECTS', 'SAMPLE',
    '1-ENV1', '2-ENV2', '3-ENV3', '4-PITCH', '5-FILTER', '6-AMP',
    '7-LFO', '8-WAVE', '9-LAYER', '0-TRACK', 'UP', 'DOWN', 'LEFT', 'RIGHT',
    'ENTER-YES', 'TRACK-1', 'EFFECT-SELECT'
  ]);
  const mapped = new Set([
    ...verified,
    'CANCEL-NO',
    'TRACK-2', 'TRACK-3', 'TRACK-4', 'TRACK-5', 'TRACK-6', 'TRACK-7', 'TRACK-8'
  ]);
  const rawLabels = new Map([
    [0x02, 'TR1'], [0x04, 'TR5'], [0x05, 'EDIT'], [0x06, 'CMD'], [0x07, 'FX SEL'],
    [0x08, 'TR2'], [0x09, 'FX'],
    [0x0a, 'UP'], [0x0b, 'DOWN'], [0x0c, '0'], [0x0d, '1'], [0x0f, 'LOAD'],
    [0x10, 'LEFT'], [0x11, 'RIGHT'], [0x12, '2'], [0x13, '3'],
    [0x0e, 'TR3'], [0x14, 'TR4'], [0x15, 'SEQ'], [0x16, 'TR8'],
    [0x18, '4'], [0x19, '5'], [0x1a, 'INST'], [0x1b, 'SYS'],
    [0x1c, 'TR7'],
    [0x1e, '6'], [0x1f, '7'], [0x20, 'SAMPLE'], [0x23, 'ENTER'],
    [0x21, 'CANCEL'], [0x22, 'TR6'], [0x24, '8'], [0x25, '9']
  ]);
  let logLines = [];

  function renderDisplay(state) {
    const text = String(state.display || '').padEnd(22, ' ').slice(0, 22);
    const decimalMask = Number(state.decimalMask || 0);
    const start = Number(state.cursorStart);
    const end = Number(state.cursorEnd);
    realDisplay.replaceChildren(...Array.from(text, (character, index) => {
      const span = document.createElement('span');
      span.textContent = character;
      if (index >= start && index < end) span.classList.add('display-cursor');
      if ((decimalMask & (1 << index)) !== 0) span.classList.add('display-decimal');
      return span;
    }));
  }

  function log(text) {
    logLines.push(text);
    logLines = logLines.slice(-12);
    liveLog.textContent = logLines.join('\n');
  }

  async function send(message) {
    try {
      await fetch('/api/event', { method: 'POST', body: message });
    } catch (error) {
      status.textContent = 'BACKEND GETRENNT';
    }
  }

  document.querySelector('#mount-os')?.addEventListener('click', event => {
    event.preventDefault();
    event.stopImmediatePropagation();
    send('disk:os');
    log('OS-DISK eingelegt');
  }, true);

  document.querySelector('#mount-instrument')?.addEventListener('click', event => {
    event.preventDefault();
    event.stopImmediatePropagation();
    send('disk:instrument');
    log('ED-001 eingelegt');
  }, true);

  const micButton = document.querySelector('#enable-mic');
  const micMeter = document.querySelector('#mic-meter');
  const micGain = document.querySelector('#mic-gain');
  const micGainValue = document.querySelector('#mic-gain-value');
  let micStream = null;
  let micContext = null;
  let micSource = null;
  let micProcessor = null;
  let micSending = false;
  let micDrainPromise = Promise.resolve();
  let micQueue = [];
  let micBrowserPeak = 0;
  let micSession = null;

  function sendMicQueue() {
    if (micSending) return micDrainPromise;
    if (!micQueue.length) return Promise.resolve();
    micSending = true;
    micDrainPromise = (async () => {
      try {
        while (micQueue.length) {
          const session = micQueue[0].session;
          const blocks = [];
          while (micQueue.length && micQueue[0].session === session)
            blocks.push(micQueue.shift().buffer);
          const byteLength = blocks.reduce((total, block) => total + block.byteLength - 4, 4);
          const payload = new Uint8Array(byteLength);
          payload.set(new Uint8Array(blocks[0], 0, 4), 0);
          let offset = 4;
          for (const block of blocks) {
            const pcm = new Uint8Array(block, 4);
            payload.set(pcm, offset);
            offset += pcm.byteLength;
          }
          await fetch(`/api/audio-input/data/${encodeURIComponent(session)}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/octet-stream' },
            body: payload
          });
        }
      } catch (error) {
        status.textContent = 'MIC INPUT GETRENNT';
      } finally {
        micSending = false;
        if (micQueue.length) await sendMicQueue();
      }
    })();
    return micDrainPromise;
  }

  async function sendPanelClick(code) {
    await send(`click:${code}`);
  }

  micGain?.addEventListener('input', () => {
    micGainValue.value = `${micGain.value}×`;
  });

  function stopMicInput(notifyHost = true) {
    const session = micSession;
    micSession = null;
    micProcessor && (micProcessor.onaudioprocess = null);
    micStream?.getTracks().forEach(track => track.stop());
    micSource?.disconnect();
    micProcessor?.disconnect();
    micContext?.close();
    micStream = micContext = micSource = micProcessor = null;
    micQueue = [];
    micBrowserPeak = 0;
    micButton?.classList.remove('active');
    if (micButton) micButton.textContent = 'MIC INPUT';
    if (notifyHost && session) {
      fetch(`/api/audio-input/deactivate/${encodeURIComponent(session)}`, {
        method: 'POST',
        keepalive: true
      }).catch(() => {});
    }
    return session;
  }

  addEventListener('pagehide', () => {
    const session = stopMicInput(false);
    if (session)
      navigator.sendBeacon(`/api/audio-input/deactivate/${encodeURIComponent(session)}`);
  });

  micButton?.addEventListener('click', async event => {
    event.preventDefault();
    event.stopImmediatePropagation();
    if (micStream) {
      stopMicInput();
      log('MIC INPUT aus');
      return;
    }
    try {
      micStream = await navigator.mediaDevices.getUserMedia({ audio: {
        echoCancellation: false,
        noiseSuppression: false,
        autoGainControl: false
      }});
      /* Keep the microphone at its native host rate.  The ES5510/device layer
         owns conversion to whichever sampling rate the original OS selects. */
      micContext = new AudioContext();
      await micContext.resume();
      micSession = crypto.randomUUID();
      const activation = await fetch(`/api/audio-input/activate/${encodeURIComponent(micSession)}`, {
        method: 'POST'
      });
      if (!activation.ok) throw new Error(`Host-Aktivierung ${activation.status}`);
      micSource = micContext.createMediaStreamSource(micStream);
      micProcessor = micContext.createScriptProcessor(1024, 1, 1);
      const session = micSession;
      const sampleRate = Math.round(micContext.sampleRate);
      const silent = micContext.createGain();
      silent.gain.value = 0;
      micProcessor.onaudioprocess = audioEvent => {
        const input = audioEvent.inputBuffer.getChannelData(0);
        const pcm = new Int16Array(input.length);
        let peak = 0;
        const gain = Number(micGain?.value || 1);
        for (let index = 0; index < input.length; index += 1) {
          const value = Math.max(-1, Math.min(1, input[index]));
          peak = Math.max(peak, Math.abs(value));
          const amplified = Math.max(-1, Math.min(1, value * gain));
          pcm[index] = Math.round(amplified * (amplified < 0 ? 32768 : 32767));
        }
        micBrowserPeak = peak;
        const payload = new Uint8Array(4 + pcm.byteLength);
        new DataView(payload.buffer).setUint32(0, sampleRate, true);
        payload.set(new Uint8Array(pcm.buffer), 4);
        /* Never discard a complete Web Audio block merely because the prior
           localhost request is still in flight.  Keep a bounded FIFO; if the
           UI is stalled for over a second, discard oldest latency rather than
           punching fresh 1024-sample holes into the recording. */
        if (micQueue.length >= 48) micQueue.shift();
        micQueue.push({ session, buffer: payload.buffer });
        sendMicQueue();
      };
      micSource.connect(micProcessor);
      micProcessor.connect(silent);
      silent.connect(micContext.destination);
      micButton.classList.add('active');
      micButton.textContent = 'MIC INPUT · ON';
      log(`MIC INPUT aktiv · ${micContext.sampleRate} Hz`);
    } catch (error) {
      stopMicInput();
      log(`MIC INPUT nicht verfügbar: ${error.message}`);
    }
  }, true);

  document.addEventListener('pointerdown', event => {
    const button = event.target.closest('button[data-action]');
    if (!button) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    const action = button.dataset.action;
    if (!mapped.has(action) || !button.dataset.code) return;
    const code = Number(button.dataset.code);
    button.classList.add('pressed');
    setTimeout(() => button.classList.remove('pressed'), 80);
    sendPanelClick(code);
    log(`${action} click   0x${code.toString(16).padStart(2, '0')}`);
  }, true);

  document.querySelectorAll('button[data-action]').forEach(button => {
    const action = button.dataset.action;
    if (!mapped.has(action) || !button.dataset.code) {
      button.classList.add('live-unverified');
      button.disabled = true;
      button.title = 'Noch nicht gegen das Original-OS verifiziert';
      return;
    }
    if (!verified.has(action)) {
      button.classList.add('live-provisional');
      button.title = 'Aus der Original-OS-Matrixtabelle abgeleitet; Live-Bestätigung offen';
    }
  });

  const rawProbe = document.querySelector('#raw-probe');
  if (rawProbe) {
    for (let code = 0; code <= 0x25; code += 1) {
      const button = document.createElement('button');
      const label = rawLabels.get(code);
      button.type = 'button';
      button.className = label ? 'raw-known' : 'raw-unknown';
      button.textContent = `${code.toString(16).padStart(2, '0').toUpperCase()}${label ? ` ${label}` : ''}`;
      button.title = label ? `Verifiziert: ${label}` : 'Unverifizierter Matrixcode';
      button.addEventListener('pointerdown', event => {
        event.preventDefault();
        event.stopImmediatePropagation();
        button.classList.add('pressed');
        setTimeout(() => button.classList.remove('pressed'), 80);
        sendPanelClick(code);
        log(`RAW 0x${code.toString(16).padStart(2, '0')} click`);
      }, true);
      rawProbe.appendChild(button);
    }
  }

  function connectFader(selector, channel) {
    const fader = document.querySelector(selector);
    const label = channel === 3 ? 'DATA ENTRY' : 'VOLUME';
    const toAdc = value => channel === 3
      ? Math.round((Number(value) * 715) / 1023)
      : Number(value);
    let pendingValue = null;
    let sending = false;

    async function sendLatestPosition() {
      if (sending) return;
      sending = true;
      while (pendingValue !== null) {
        const value = pendingValue;
        pendingValue = null;
        await send(`adc:${channel}:${value}`);
      }
      sending = false;
    }

    fader.addEventListener('input', event => {
      event.stopImmediatePropagation();
      const panelValue = Number(event.target.value);
      pendingValue = toAdc(panelValue);
      document.querySelector(`${selector.replace('fader', 'value')}`).value = panelValue;
      log(`${label} ${panelValue}`);
      sendLatestPosition();
    }, true);
  }
  connectFader('#volume-fader', 5);
  connectFader('#data-entry-fader', 3);

  const piano = document.querySelector('#piano');
  const blackNotes = new Set([1, 3, 6, 8, 10]);
  for (let note = 48; note <= 72; note += 1) {
    const key = document.createElement('button');
    key.type = 'button';
    key.dataset.midi = note;
    key.className = blackNotes.has(note % 12) ? 'black' : 'white';
    key.title = `MIDI ${note}`;
    if (note % 12 === 0) key.textContent = `C${Math.floor(note / 12) - 1}`;
    let held = false;
    const off = () => {
      if (!held) return;
      held = false;
      key.classList.remove('playing');
      send(`off:${note}`);
    };
    key.addEventListener('pointerdown', event => {
      event.preventDefault();
      held = true;
      key.setPointerCapture(event.pointerId);
      key.classList.add('playing');
      send(`note:${note}:100`);
    });
    key.addEventListener('pointerup', off);
    key.addEventListener('pointercancel', off);
    piano.appendChild(key);
  }

  const typing = { a:60, w:61, s:62, e:63, d:64, f:65, t:66, g:67, z:68, h:69, u:70, j:71, k:72 };
  const typingHeld = new Set();
  addEventListener('keydown', event => {
    const note = typing[event.key.toLowerCase()];
    if (note == null || typingHeld.has(note)) return;
    event.preventDefault();
    typingHeld.add(note);
    document.querySelector(`[data-midi="${note}"]`)?.classList.add('playing');
    send(`note:${note}:100`);
  });
  addEventListener('keyup', event => {
    const note = typing[event.key.toLowerCase()];
    if (note == null) return;
    typingHeld.delete(note);
    document.querySelector(`[data-midi="${note}"]`)?.classList.remove('playing');
    send(`off:${note}`);
  });

  async function refresh() {
    /* Metering at 20 Hz is sufficient while streaming and leaves the
       single localhost HTTP listener more capacity for ordered PCM bodies. */
    let nextRefresh = micStream ? 50 : 16;
    try {
      const response = await fetch('/api/state', { cache: 'no-store' });
      const state = await response.json();
      renderDisplay(state);
      if (micMeter) {
        const browserPercent = Math.round(micBrowserPeak * 100);
        const epsPercent = Math.round((Number(state.audioInputPeak || 0) * 100) / 32768);
        const fresh = Number(state.audioInputAgeMs) < 500;
        micMeter.textContent = micStream
          ? `MIC B:${browserPercent}% · EPS:${fresh ? epsPercent : '—'}%`
          : 'MIC —';
        micMeter.classList.toggle('active', Boolean(micStream && fresh));
      }
      status.textContent = 'LIVE · ORIGINAL OS';
    } catch (error) {
      status.textContent = 'BACKEND GETRENNT';
      nextRefresh = 250;
    } finally {
      setTimeout(refresh, nextRefresh);
    }
  }
  refresh();
  log('Live-Verbindung aktiv. Warte auf das Original-OS …');
})();
