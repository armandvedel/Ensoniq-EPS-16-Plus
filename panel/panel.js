const events = [];
const display = document.querySelector('#display');
const log = document.querySelector('#log');
const expected = [...document.querySelectorAll('#expected li[data-step]')];

const diskCatalog = {
  INSTRUMENT: [
    { number: 1, name: 'ED-001 BANK', type: 'BANK', blocks: 7 },
    { number: 2, name: 'FLUTE 1', type: 'INSTRUMENT', blocks: 308 },
    { number: 3, name: 'PIANO 241', type: 'INSTRUMENT', blocks: 241 },
    { number: 4, name: 'JAZZ BASS', type: 'INSTRUMENT', blocks: 154 },
    { number: 5, name: 'JAZZ DRUMS', type: 'INSTRUMENT', blocks: 851 }
  ],
  'SEQ-SONG': [{ number: 6, name: 'BLUES RONDO', type: 'SONG', blocks: 14 }],
  'SYSTEM-MIDI': [{ name: 'GLOBAL PARAMETERS', type: 'SYSTEM' }],
  EFFECTS: [{ name: 'PARALLEL EFX', type: 'EFFECT' }]
};

const state = {
  mode: null,
  page: 'INSTRUMENT',
  items: [],
  index: 0,
  phase: 'idle',
  selection: null,
  targetTrack: null,
  diskName: 'ED-001.IMG',
  detailView: false
};
let expectedIndex = 0;

function fit(text) {
  return String(text).toUpperCase().slice(0, 22).padEnd(22, ' ');
}

function show(text) {
  display.textContent = fit(text);
}

function updateIndicators() {
  document.querySelector('#mode-indicator').textContent = state.mode || '';
  document.querySelector('#page-indicator').textContent = state.page === 'INSTRUMENT' ? 'INST' : (state.page || '');
  document.querySelector('#type-indicator').textContent = currentItem()?.type === 'BANK' ? 'BANK' : '';
}

function redrawLog() {
  log.textContent = events.length ? events.map((event, index) => {
    const code = event.code == null ? 'code=?' : `code=0x${event.code.toString(16).padStart(2, '0')}`;
    const wire = event.packet ? `  wire=${event.packet}` : '';
    const detail = event.detail ? `  ${event.detail}` : '';
    return `${String(index + 1).padStart(2, '0')}  ${event.action.padEnd(18)} ${code}${wire}${detail}`;
  }).join('\n') : 'Noch keine Eingabe.';
}

function logEvent(action, code, detail = '') {
  const packet = code == null ? null
    : [code | 0x80, 0, code, 0].map(value => value.toString(16).padStart(2, '0')).join('');
  events.push({ action, code, packet, detail, timeMs: Math.round(performance.now()) });
  redrawLog();
}

function updateExpected(action) {
  const current = expected[expectedIndex];
  if (!current) return;
  const step = current.dataset.step;
  const matches = step === action ||
    (step === 'TRACK' && action.startsWith('TRACK-')) ||
    (step === 'BROWSE' && ['UP', 'DOWN', 'LEFT', 'RIGHT'].includes(action));
  if (!matches) return;
  current.classList.remove('current');
  current.classList.add('done');
  expectedIndex += 1;
  expected[expectedIndex]?.classList.add('current');
}

function setPressed(button) {
  button.classList.add('pressed');
  setTimeout(() => button.classList.remove('pressed'), 120);
}

function activateMode(mode) {
  state.mode = mode;
  state.phase = 'page-select';
  state.items = [];
  state.selection = null;
  state.detailView = false;
  document.querySelectorAll('.left-column button').forEach(button => {
    button.classList.toggle('active', button.dataset.action === mode);
  });
  show(`${mode} MODE: SELECT PAGE`);
  updateIndicators();
  if (state.page) openPage(state.page);
}

function menuFor(mode, page) {
  if (mode === 'LOAD') return diskCatalog[page] || [];
  if (mode === 'COMMAND') return (EPS16_MENU_CATALOG.COMMAND[page] || []).map(item => ({ ...item, type: 'COMMAND' }));
  if (mode === 'EDIT') return (EPS16_MENU_CATALOG.EDIT[page] || []).map(item => ({ ...item, type: 'PARAMETER' }));
  return [];
}

function currentItem() {
  return state.items[state.index] || null;
}

function showCurrentItem() {
  const item = currentItem();
  if (!item) {
    show(`${state.mode || ''} ${state.page || ''}: EMPTY`);
    return;
  }
  if (state.mode === 'LOAD') {
    show(state.detailView ? `${item.blocks || 0} BLOCKS` : `FILE ${String(item.number || state.index + 1).padStart(2, ' ')}  ${item.name}`);
  } else {
    show(item.name);
  }
  updateIndicators();
}

function openPage(page) {
  state.page = page;
  document.querySelectorAll('.page-row button').forEach(button => {
    button.classList.toggle('active', button.dataset.action === page);
  });
  if (!state.mode) {
    show(`${page}: SELECT MODE`);
    return;
  }
  state.items = menuFor(state.mode, page);
  state.index = state.mode === 'LOAD' && page === 'INSTRUMENT'
    ? Math.max(0, state.items.findIndex(item => item.type === 'INSTRUMENT'))
    : 0;
  state.detailView = false;
  state.phase = state.items.length ? 'browse' : 'empty';
  showCurrentItem();
}

function browse(delta) {
  if (state.phase !== 'browse' || !state.items.length) {
    show('SELECT MODE AND PAGE');
    return;
  }
  state.index = (state.index + delta + state.items.length) % state.items.length;
  state.detailView = false;
  showCurrentItem();
}

function toggleDetails() {
  if (state.phase === 'browse' && state.mode === 'LOAD' && currentItem()) {
    state.detailView = !state.detailView;
    showCurrentItem();
  } else if (state.phase === 'browse') {
    browse(1);
  }
}

function selectCurrent() {
  const item = currentItem();
  if (!item) {
    show('NOTHING TO SELECT');
    return;
  }
  state.selection = item;
  if (state.mode === 'LOAD' && item.type === 'INSTRUMENT') {
    state.phase = 'track-select';
    show('PICK INSTRUMENT BUTTON');
  } else if (state.mode === 'LOAD') {
    state.phase = 'confirm-load';
    show(`LOAD ${item.name}?`);
  } else {
    state.phase = 'selected';
    show(item.name);
  }
}

function chooseTrack(action) {
  if (state.phase !== 'track-select') {
    show(`${action.replace('-', ' ')} SELECTED`);
    return;
  }
  state.targetTrack = Number(action.split('-')[1]);
  state.phase = 'loading';
  const trackButton = document.querySelector(`[data-action="TRACK-${state.targetTrack}"]`);
  trackButton.classList.add('loading');
  show('LOADING FILE...');
  const loadedName = state.selection.name;
  const loadedTrack = state.targetTrack;
  setTimeout(() => {
    trackButton.classList.remove('loading');
    trackButton.classList.add('loaded');
    show('FILE LOADED');
    logEvent('LOAD-COMPLETE', null, `${loadedName} -> TRACK ${loadedTrack}`);
    state.phase = 'loaded';
  }, 700);
}

function enter() {
  if (state.phase === 'browse') {
    selectCurrent();
    return;
  }
  if (state.phase === 'confirm-load') {
    show(`LOADED ${state.selection.name}`);
    state.phase = 'loaded';
    return;
  }
  show('ENTER');
}

function cancel() {
  if (state.phase === 'track-select' || state.phase === 'confirm-load') {
    state.phase = 'browse';
    state.selection = null;
    state.targetTrack = null;
    showCurrentItem();
  } else {
    state.mode = null;
    state.page = 'INSTRUMENT';
    state.phase = 'idle';
    document.querySelectorAll('button.active').forEach(button => button.classList.remove('active'));
    show('READY');
  }
}

function dispatch(action) {
  if (['LOAD', 'COMMAND', 'EDIT'].includes(action)) {
    if (action === 'LOAD' && state.mode === 'LOAD' && state.phase === 'browse') selectCurrent();
    else activateMode(action);
    return;
  }
  if (state.mode === 'LOAD' && state.phase === 'browse' && /^\d-/.test(action)) {
    const number = Number(action[0]);
    const directIndex = state.items.findIndex(item => item.number === number);
    if (directIndex >= 0) {
      state.index = directIndex;
      state.detailView = false;
      showCurrentItem();
    } else show(`FILE ${number} NOT FOUND`);
    return;
  }
  if (document.querySelector(`.page-row [data-action="${action}"]`)) {
    openPage(action);
    return;
  }
  if (action === 'DOWN') browse(1);
  else if (action === 'UP') browse(-1);
  else if (['LEFT', 'RIGHT'].includes(action)) toggleDetails();
  else if (action.startsWith('TRACK-')) chooseTrack(action);
  else if (action === 'ENTER-YES') enter();
  else if (action === 'CANCEL-NO') cancel();
  else show(action.replaceAll('-', ' '));
}

function record(button) {
  const action = button.dataset.action;
  const code = button.dataset.code ? Number(button.dataset.code) : null;
  setPressed(button);
  logEvent(action, code, currentItem()?.name || '');
  dispatch(action);
  updateExpected(action);
}

document.querySelectorAll('button[data-action]').forEach(button => {
  button.addEventListener('click', () => record(button));
});

function connectFader(id, outputId, action, adcChannel) {
  const fader = document.querySelector(id);
  const output = document.querySelector(outputId);
  fader.addEventListener('input', () => {
    const value = Number(fader.value);
    output.value = value;
    logEvent(action, null, `ADC ${adcChannel}=${value} raw=0x${(value << 6).toString(16).padStart(4, '0')}`);
  });
}

connectFader('#volume-fader', '#volume-value', 'VOLUME', 5);
connectFader('#data-entry-fader', '#data-entry-value', 'DATA-ENTRY', 3);

document.querySelector('#disk').addEventListener('change', event => {
  state.diskName = event.target.files[0]?.name || 'NO DISK';
  show(state.diskName);
  logEvent('INSERT-DISK', null, state.diskName);
});

document.querySelector('#clear').addEventListener('click', () => {
  events.length = 0;
  expectedIndex = 0;
  Object.assign(state, { mode: null, page: 'INSTRUMENT', items: [], index: 0, phase: 'idle', selection: null, targetTrack: null, detailView: false });
  document.querySelectorAll('button.active').forEach(button => button.classList.remove('active'));
  expected.forEach((item, index) => {
    item.classList.toggle('current', index === 0);
    item.classList.remove('done');
  });
  show('READY - INSERT DISK');
  updateIndicators();
  redrawLog();
});

document.querySelector('#copy').addEventListener('click', async () => {
  await navigator.clipboard.writeText(JSON.stringify({ state, events }, null, 2));
  show('EVENT LOG COPIED');
});

expected[0].classList.add('current');
show('READY - ED-001.IMG');
updateIndicators();
