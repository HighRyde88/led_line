// modules_registry.js
window.ModulesRegistry = (function() {
  const modules = new Map();

  function register(name, moduleClass, constructorArgs = []) {
    modules.set(name, { 
      class: moduleClass, 
      args: constructorArgs 
    });
  }

  function getModules() {
    const activeModules = [];
    for (const [name, { class: ModuleClass, args }] of modules) {
      activeModules.push(new ModuleClass(...args));
    }
    return activeModules;
  }

  // Регистрация модулей
  register("control", ControlModule);
  register("device", DeviceModule);
  register("ledstrip", LedStripModule);
  register("network", NetworkModule);
  register("apoint", ApointModule);
  register("wifi", WifiModule);
  register("mqtt", MqttModule);
  register("update", UpdateModule);

  return { register, getModules };
})();