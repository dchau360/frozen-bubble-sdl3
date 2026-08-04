'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const createFrozenBubblePersistence = require('../web/persistence.js');

function createHarness() {
  const calls = [];
  const pendingSyncs = [];
  const dependencyEvents = [];
  const errors = [];

  const FS = {
    mkdirTree(path) {
      calls.push({operation: 'mkdirTree', path});
    },
    mount(type, options, path) {
      calls.push({operation: 'mount', type, options, path});
    },
    syncfs(populate, callback) {
      calls.push({operation: 'syncfs', populate});
      pendingSyncs.push(callback);
    },
  };
  const IDBFS = {name: 'fake-idbfs'};
  const Module = {
    printErr(message) {
      errors.push(message);
    },
  };
  const runtime = {
    addRunDependency(name) {
      dependencyEvents.push({operation: 'add', name});
    },
    removeRunDependency(name) {
      dependencyEvents.push({operation: 'remove', name});
    },
  };

  return {
    calls,
    pendingSyncs,
    dependencyEvents,
    errors,
    Module,
    IDBFS,
    controller: createFrozenBubblePersistence(Module, FS, IDBFS, runtime),
    completeNextSync(error) {
      const callback = pendingSyncs.shift();
      assert.ok(callback, 'expected one queued syncfs callback');
      callback(error);
    },
    syncModes() {
      return calls
        .filter((call) => call.operation === 'syncfs')
        .map((call) => call.populate);
    },
  };
}

async function hydrateSuccessfully(harness) {
  const hydration = harness.controller.hydrate();
  harness.completeNextSync();
  assert.deepEqual(await hydration, {ok: true});
}

test('hydrate holds the run dependency until populate succeeds', async () => {
  const harness = createHarness();

  const hydration = harness.controller.hydrate();

  assert.deepEqual(harness.calls.slice(0, 3), [
    {operation: 'mkdirTree', path: '/libsdl/frozen-bubble'},
    {
      operation: 'mount',
      type: harness.IDBFS,
      options: {},
      path: '/libsdl/frozen-bubble',
    },
    {operation: 'syncfs', populate: true},
  ]);
  assert.deepEqual(harness.dependencyEvents, [
    {operation: 'add', name: 'frozen-bubble-idbfs'},
  ]);

  harness.completeNextSync();

  assert.deepEqual(await hydration, {ok: true});
  assert.equal(harness.Module.persistentStoragePopulateCompleted, true);
  assert.deepEqual(harness.dependencyEvents, [
    {operation: 'add', name: 'frozen-bubble-idbfs'},
    {operation: 'remove', name: 'frozen-bubble-idbfs'},
  ]);
  assert.deepEqual(harness.syncModes(), [true]);
});

test('hydrate error releases startup and reports failure', async () => {
  const harness = createHarness();
  const failure = new Error('populate failed');

  const hydration = harness.controller.hydrate();
  harness.completeNextSync(failure);

  assert.deepEqual(await hydration, {ok: false, error: failure});
  assert.deepEqual(harness.dependencyEvents, [
    {operation: 'add', name: 'frozen-bubble-idbfs'},
    {operation: 'remove', name: 'frozen-bubble-idbfs'},
  ]);
  assert.deepEqual(harness.syncModes(), [true]);
  assert.deepEqual(harness.errors, [
    'Persistent storage hydrate failed: Error: populate failed',
  ]);
});

test('flush requested before hydrate waits for populate', async () => {
  const harness = createHarness();

  const flush = harness.controller.requestFlush();
  assert.deepEqual(harness.syncModes(), []);

  const hydration = harness.controller.hydrate();
  assert.deepEqual(harness.syncModes(), [true]);

  harness.completeNextSync();
  assert.deepEqual(await hydration, {ok: true});
  assert.deepEqual(harness.syncModes(), [true, false]);

  harness.completeNextSync();
  await flush;
  assert.deepEqual(harness.syncModes(), [true, false]);
});

test('rapid flush requests never overlap syncfs', async () => {
  const harness = createHarness();
  await hydrateSuccessfully(harness);

  const first = harness.controller.requestFlush();
  const second = harness.controller.requestFlush();
  const third = harness.controller.requestFlush();

  assert.equal(harness.pendingSyncs.length, 1);
  assert.deepEqual(harness.syncModes(), [true, false]);

  harness.completeNextSync();
  assert.equal(harness.pendingSyncs.length, 1);
  assert.deepEqual(harness.syncModes(), [true, false, false]);

  harness.completeNextSync();
  await Promise.all([first, second, third]);
  assert.equal(harness.pendingSyncs.length, 0);
  assert.deepEqual(harness.syncModes(), [true, false, false]);
});

test('dirty write during flush schedules exactly one follow-up', async () => {
  const harness = createHarness();
  await hydrateSuccessfully(harness);

  const initialFlush = harness.controller.requestFlush();
  const dirtyFlush = harness.controller.requestFlush();
  const anotherDirtyFlush = harness.controller.requestFlush();

  harness.completeNextSync();
  assert.deepEqual(harness.syncModes(), [true, false, false]);

  harness.completeNextSync();
  await Promise.all([initialFlush, dirtyFlush, anotherDirtyFlush]);
  assert.deepEqual(harness.syncModes(), [true, false, false]);
});

test('flush error settles waiters and later requests can retry', async () => {
  const harness = createHarness();
  await hydrateSuccessfully(harness);
  const failure = new Error('flush failed');

  const failedFlush = harness.controller.requestFlush();
  const failedIdle = harness.controller.whenIdle();
  harness.completeNextSync(failure);

  await assert.rejects(failedFlush, failure);
  await assert.rejects(failedIdle, failure);
  assert.deepEqual(harness.errors, [
    'Persistent storage flush failed: Error: flush failed',
  ]);

  const retry = harness.controller.requestFlush();
  assert.deepEqual(harness.syncModes(), [true, false, false]);
  harness.completeNextSync();
  await retry;

  assert.deepEqual(harness.syncModes(), [true, false, false]);
});
