// modules_registry.js
window.ModulesRegistry = (function() {
  const modules = new Map();

  function register(name, moduleClass, condition = () => true) {
    modules.set(name, { class: moduleClass, condition });
  }

  function getModules() {
    const activeModules = [];
    for (const [name, { class: ModuleClass, condition }] of modules) {
      if (condition()) {
        activeModules.push(new ModuleClass());
      }
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