#!/usr/bin/env node

import assert from 'node:assert/strict';
import {constants as fsConstants} from 'node:fs';
import {access, mkdtemp, rm} from 'node:fs/promises';
import {get as httpGet} from 'node:http';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {spawn} from 'node:child_process';

const SETTINGS_FIXTURE =
    '[GFX]\nQuality = 2\n[Keys]\nSpeedMultiplier = 4.25\n';
const HIGHSCORES_FIXTURE = '17,Browser Test,12.5,3\n';
const HISTORY_FIXTURE =
`0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7
0   1   2   3   4   5   6   7

`;
const FIXTURES = {
  'settings.ini': SETTINGS_FIXTURE,
  'highscores': HIGHSCORES_FIXTURE,
  'highlevelshistory': HISTORY_FIXTURE,
};
const TIMEOUT_MS = 120_000;

function delay(milliseconds) {
  return new Promise((resolveDelay) => setTimeout(resolveDelay, milliseconds));
}

async function isExecutable(candidate) {
  try {
    await access(candidate, fsConstants.X_OK);
    return true;
  } catch {
    return false;
  }
}

async function findChrome() {
  const candidates = [
    process.env.CHROME_BIN,
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Chromium.app/Contents/MacOS/Chromium',
    '/usr/bin/google-chrome',
    '/usr/bin/google-chrome-stable',
    '/usr/bin/chromium',
    '/usr/bin/chromium-browser',
  ].filter(Boolean);

  for (const candidate of candidates) {
    if (await isExecutable(candidate)) return candidate;
  }
  throw new Error(
      'Chrome not found. Set CHROME_BIN or install Chrome/Chromium in a known path.');
}

function waitForMatch(stream, pattern, description, timeoutMs = TIMEOUT_MS) {
  return new Promise((resolveMatch, rejectMatch) => {
    let output = '';
    const timeout = setTimeout(() => {
      cleanup();
      rejectMatch(new Error(`Timed out waiting for ${description}. Output:\n${output}`));
    }, timeoutMs);

    const onData = (chunk) => {
      output += chunk.toString();
      const match = output.match(pattern);
      if (match) {
        cleanup();
        resolveMatch(match[1]);
      }
    };
    const onEnd = () => {
      cleanup();
      rejectMatch(new Error(`Process output ended before ${description}. Output:\n${output}`));
    };
    const cleanup = () => {
      clearTimeout(timeout);
      stream.off('data', onData);
      stream.off('end', onEnd);
      stream.off('close', onEnd);
    };

    stream.on('data', onData);
    stream.on('end', onEnd);
    stream.on('close', onEnd);
  });
}

function waitForSpawn(child) {
  return new Promise((resolveSpawn, rejectSpawn) => {
    child.once('spawn', resolveSpawn);
    child.once('error', rejectSpawn);
  });
}

function getJson(url) {
  return new Promise((resolveJson, rejectJson) => {
    const request = httpGet(url, (response) => {
      let body = '';
      response.setEncoding('utf8');
      response.on('data', (chunk) => { body += chunk; });
      response.on('end', () => {
        if (response.statusCode !== 200) {
          rejectJson(new Error(`GET ${url} returned ${response.statusCode}: ${body}`));
          return;
        }
        try {
          resolveJson(JSON.parse(body));
        } catch (error) {
          rejectJson(new Error(`Invalid JSON from ${url}: ${error.message}`));
        }
      });
    });
    request.on('error', rejectJson);
  });
}

async function findPageWebSocket(browserWebSocketUrl) {
  const endpoint = new URL(browserWebSocketUrl);
  const listUrl = `http://${endpoint.host}/json/list`;
  const deadline = Date.now() + TIMEOUT_MS;
  let lastError;

  while (Date.now() < deadline) {
    try {
      const targets = await getJson(listUrl);
      const page = targets.find((target) =>
        target.type === 'page' && target.webSocketDebuggerUrl);
      if (page) return page.webSocketDebuggerUrl;
    } catch (error) {
      lastError = error;
    }
    await delay(100);
  }
  throw new Error(`Timed out finding Chrome page target${
    lastError ? `: ${lastError.message}` : ''}`);
}

class DevToolsPage {
  constructor(socket) {
    this.socket = socket;
    this.nextId = 1;
    this.pending = new Map();
    this.eventWaiters = new Map();
    socket.addEventListener('message', (event) => {
      const message = JSON.parse(String(event.data));
      if (!message.id) {
        const waiters = this.eventWaiters.get(message.method);
        if (waiters?.length) waiters.shift()(message.params);
        return;
      }
      const waiter = this.pending.get(message.id);
      if (!waiter) return;
      this.pending.delete(message.id);
      if (message.error) {
        waiter.reject(new Error(`${message.error.message} (${message.error.code})`));
      } else {
        waiter.resolve(message.result);
      }
    });
    socket.addEventListener('close', () => {
      for (const waiter of this.pending.values()) {
        waiter.reject(new Error('DevTools page WebSocket closed'));
      }
      this.pending.clear();
    });
  }

  static async connect(url) {
    if (typeof WebSocket === 'undefined') {
      throw new Error('Node 22 or newer is required for the global WebSocket API');
    }
    const socket = new WebSocket(url);
    await new Promise((resolveOpen, rejectOpen) => {
      socket.addEventListener('open', resolveOpen, {once: true});
      socket.addEventListener('error', () => {
        rejectOpen(new Error(`Unable to connect to DevTools WebSocket ${url}`));
      }, {once: true});
    });
    return new DevToolsPage(socket);
  }

  send(method, params = {}) {
    const id = this.nextId++;
    return new Promise((resolveCommand, rejectCommand) => {
      this.pending.set(id, {resolve: resolveCommand, reject: rejectCommand});
      this.socket.send(JSON.stringify({id, method, params}));
    });
  }

  waitForEvent(method, timeoutMs = TIMEOUT_MS) {
    return new Promise((resolveEvent, rejectEvent) => {
      const waiters = this.eventWaiters.get(method) || [];
      const timeout = setTimeout(() => {
        const currentWaiters = this.eventWaiters.get(method) || [];
        const index = currentWaiters.indexOf(complete);
        if (index !== -1) currentWaiters.splice(index, 1);
        rejectEvent(new Error(`Timed out waiting for DevTools event ${method}`));
      }, timeoutMs);
      const complete = (params) => {
        clearTimeout(timeout);
        resolveEvent(params);
      };
      waiters.push(complete);
      this.eventWaiters.set(method, waiters);
    });
  }

  async evaluate(expression) {
    const response = await this.send('Runtime.evaluate', {
      expression,
      awaitPromise: true,
      returnByValue: true,
    });
    if (response.exceptionDetails) {
      const details = response.exceptionDetails.exception?.description ||
          response.exceptionDetails.text;
      throw new Error(`Browser evaluation failed: ${details}`);
    }
    return response.result.value;
  }

  close() {
    this.socket.close();
  }
}

async function waitForPersistenceReady(page) {
  const deadline = Date.now() + TIMEOUT_MS;
  let lastError;
  while (Date.now() < deadline) {
    try {
      const state = await page.evaluate(`(async function() {
        if (typeof Module === 'undefined' ||
            !Module.persistentStorageReady ||
            !Module.frozenBubbleRuntimeInitialized) {
          return {available: false};
        }
        const hydration = await Module.persistentStorageReady;
        return {
          available: true,
          hydrationOk: hydration.ok,
          hydrationError: hydration.error ? String(hydration.error) : '',
          readyBeforeRuntime: Module.persistentStorageReadyBeforeRuntime
        };
      })()`);
      if (state?.available) {
        assert.equal(state.hydrationOk, true,
            `IDBFS hydration failed: ${state.hydrationError}`);
        return state;
      }
    } catch (error) {
      lastError = error;
    }
    await delay(100);
  }
  throw new Error(`Timed out waiting for WASM persistence readiness${
    lastError ? `: ${lastError.message}` : ''}`);
}

async function stopChild(child) {
  if (!child || child.pid === undefined ||
      child.exitCode !== null || child.signalCode !== null) return;
  await new Promise((resolveExit) => {
    const settle = () => {
      clearTimeout(forceTimer);
      child.off('error', settle);
      child.off('close', settle);
      resolveExit();
    };
    const forceTimer = setTimeout(() => {
      if (child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
    }, 5_000);
    child.once('error', settle);
    child.once('close', settle);
    child.kill('SIGTERM');
  });
}

async function main() {
  const buildDirectory = resolve(process.argv[2] || 'build-wasm');
  const chromeBinary = await findChrome();
  const browserProfile = await mkdtemp(join(tmpdir(), 'frozen-bubble-wasm-'));
  let server;
  let chrome;
  let page;

  try {
    server = spawn('python3', [
      'tools/serve-wasm.py', buildDirectory, '--port', '0',
    ], {stdio: ['ignore', 'pipe', 'pipe']});
    await waitForSpawn(server);
    server.stderr.resume();
    const serverUrl = await waitForMatch(
        server.stdout, /\s(http:\/\/localhost:\d+\/\S*)/, 'WASM server URL');
    server.stdout.resume();

    chrome = spawn(chromeBinary, [
      '--headless=new',
      '--no-sandbox',
      '--remote-debugging-port=0',
      `--user-data-dir=${browserProfile}`,
      '--no-first-run',
      '--no-default-browser-check',
      'about:blank',
    ], {stdio: ['ignore', 'ignore', 'pipe']});
    await waitForSpawn(chrome);
    const browserWebSocketUrl = await waitForMatch(
        chrome.stderr, /(ws:\/\/[^\s]+\/devtools\/browser\/[^\s]+)/,
        'Chrome DevTools endpoint');
    chrome.stderr.resume();
    const pageWebSocketUrl = await findPageWebSocket(browserWebSocketUrl);
    page = await DevToolsPage.connect(pageWebSocketUrl);
    await page.send('Page.enable');
    await page.send('Runtime.enable');
    await page.send('Page.navigate', {url: serverUrl});

    const initialState = await waitForPersistenceReady(page);
    assert.equal(initialState.readyBeforeRuntime, true,
        'IDBFS populate did not complete before runtime initialization');

    await page.evaluate(`(async function() {
      const fixtures = ${JSON.stringify(FIXTURES)};
      for (const [name, contents] of Object.entries(fixtures)) {
        Module.FS.writeFile('/libsdl/frozen-bubble/' + name,
            new TextEncoder().encode(contents));
      }
      await Module.requestPersistentStorageFlush();
      await Module.whenPersistentStorageIdle();
      return true;
    })()`);

    const reloadComplete = page.waitForEvent('Page.loadEventFired');
    await page.send('Page.reload', {ignoreCache: true});
    await reloadComplete;
    const reloadedState = await waitForPersistenceReady(page);
    assert.equal(reloadedState.readyBeforeRuntime, true,
        'reload initialized runtime before IDBFS populate completed');

    const persistedBytes = await page.evaluate(`(function() {
      const names = ['settings.ini', 'highscores', 'highlevelshistory'];
      return Object.fromEntries(names.map(function(name) {
        return [name, Array.from(Module.FS.readFile(
            '/libsdl/frozen-bubble/' + name))];
      }));
    })()`);
    for (const [name, contents] of Object.entries(FIXTURES)) {
      assert.deepEqual(persistedBytes[name], Array.from(Buffer.from(contents, 'utf8')),
          `${name} bytes changed across reload`);
    }

    console.log(
        'PASS: IDBFS hydrated before runtime and persisted all three fixtures across reload');
  } finally {
    page?.close();
    await Promise.all([stopChild(chrome), stopChild(server)]);
    await rm(browserProfile, {recursive: true, force: true});
  }
}

main().catch((error) => {
  console.error(error.stack || error);
  process.exitCode = 1;
});
