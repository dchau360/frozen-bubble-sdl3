'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const {spawn} = require('node:child_process');
const {mkdtemp, readdir, rm} = require('node:fs/promises');
const {tmpdir} = require('node:os');
const {join, resolve} = require('node:path');

function run(command, args, options) {
  return new Promise((resolveRun, rejectRun) => {
    const child = spawn(command, args, options);
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => { stdout += chunk; });
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('error', rejectRun);
    child.on('close', (code, signal) => {
      resolveRun({code, signal, stdout, stderr});
    });
  });
}

test('launch failure is handled and removes the temporary browser profile', async () => {
  const isolatedTemp = await mkdtemp(join(tmpdir(), 'fb-wasm-driver-test-'));
  const driver = resolve(__dirname, '../tools/test-wasm-persistence.mjs');

  try {
    const result = await run(process.execPath, [driver, 'unused-build'], {
      env: {
        ...process.env,
        CHROME_BIN: process.execPath,
        PATH: isolatedTemp,
        TMPDIR: isolatedTemp,
      },
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    const leftovers = await readdir(isolatedTemp);

    assert.equal(result.code, 1);
    assert.deepEqual({
      emittedUnhandledError: /Unhandled 'error' event/.test(result.stderr),
      leftovers,
    }, {
      emittedUnhandledError: false,
      leftovers: [],
    });
    assert.match(result.stderr, /spawn python3 ENOENT/);
  } finally {
    await rm(isolatedTemp, {recursive: true, force: true});
  }
});
