(function(root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory;
    return;
  }
  root.createFrozenBubblePersistence = factory;
  Module.persistentStoragePopulateCompleted = false;
  Module.frozenBubbleRuntimeInitialized = false;
  Module.persistentStorageReadyBeforeRuntime = false;

  const previousRuntimeInitialized = Module.onRuntimeInitialized;
  Module.onRuntimeInitialized = function() {
    Module.persistentStorageReadyBeforeRuntime =
        Module.persistentStoragePopulateCompleted;
    Module.frozenBubbleRuntimeInitialized = true;
    if (previousRuntimeInitialized) {
      previousRuntimeInitialized.call(Module);
    }
  };

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function() {
    const controller = factory(Module, FS, IDBFS, {
      addRunDependency: function(name) { addRunDependency(name); },
      removeRunDependency: function(name) { removeRunDependency(name); }
    });
    Module.FS = FS;
    Module.requestPersistentStorageFlush = controller.requestFlush;
    Module.whenPersistentStorageIdle = controller.whenIdle;
    Module.persistentStorageReady = controller.hydrate();
  });
})(typeof globalThis !== 'undefined' ? globalThis : this,
function createFrozenBubblePersistence(Module, FS, IDBFS, runtime) {
  const mountPath = '/libsdl/frozen-bubble';
  const dependency = 'frozen-bubble-idbfs';
  let hydrated = false;
  let inFlight = false;
  let dirty = false;
  let idleWaiters = [];
  let hydratePromise;

  function settleWaiters(error) {
    const waiters = idleWaiters;
    idleWaiters = [];
    for (const waiter of waiters) {
      if (error) waiter.reject(error);
      else waiter.resolve();
    }
  }

  function startFlushIfNeeded() {
    if (!hydrated || inFlight || !dirty) return;
    dirty = false;
    inFlight = true;

    const complete = function(error) {
      inFlight = false;
      if (error) {
        dirty = false;
        if (Module.printErr) {
          Module.printErr('Persistent storage flush failed: ' + error);
        }
        settleWaiters(error);
      } else if (dirty) {
        startFlushIfNeeded();
      } else {
        settleWaiters();
      }
    };

    try {
      FS.syncfs(false, complete);
    } catch (error) {
      complete(error);
    }
  }

  function whenIdle() {
    if (hydrated && !inFlight && !dirty) return Promise.resolve();
    return new Promise(function(resolve, reject) {
      idleWaiters.push({resolve, reject});
    });
  }

  function requestFlush() {
    dirty = true;
    const completion = whenIdle();
    startFlushIfNeeded();
    return completion;
  }

  function hydrate() {
    if (hydratePromise) return hydratePromise;
    runtime.addRunDependency(dependency);
    hydratePromise = new Promise(function(resolve) {
      let completed = false;
      const complete = function(error) {
        if (completed) return;
        completed = true;
        hydrated = true;
        Module.persistentStoragePopulateCompleted = true;
        if (error && Module.printErr) {
          Module.printErr('Persistent storage hydrate failed: ' + error);
        }
        resolve(error ? {ok: false, error} : {ok: true});
        runtime.removeRunDependency(dependency);
        if (error) {
          dirty = false;
          settleWaiters(error);
        } else if (dirty) {
          startFlushIfNeeded();
        } else {
          settleWaiters();
        }
      };

      try {
        FS.mkdirTree(mountPath);
        FS.mount(IDBFS, {}, mountPath);
        FS.syncfs(true, complete);
      } catch (error) {
        complete(error);
      }
    });
    return hydratePromise;
  }

  return {hydrate, requestFlush, whenIdle};
});
