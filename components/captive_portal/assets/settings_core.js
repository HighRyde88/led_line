class SettingsCore {
  constructor() {
    this.modules = [];
    this.isInitialized = false;
    this.moduleRoutes = new Map(); // Карта маршрутов: cmd -> module
  }

  registerModule(moduleInstance) {
    if (!moduleInstance || typeof moduleInstance.init !== "function") {
      console.warn("Попытка зарегистрировать невалидный модуль");
      return;
    }

    // Устанавливаем ядро в модуль
    if (typeof moduleInstance.setCore === "function") {
      moduleInstance.setCore(this);
    }

    const moduleName = moduleInstance.constructor.name
      .replace("Module", "")
      .toLowerCase();
    moduleInstance._moduleName = moduleName;
    this.modules.push(moduleInstance);

    // Автоматически регистрируем маршруты, если у модуля есть метод getRoutes
    if (typeof moduleInstance.getRoutes === "function") {
      const routes = moduleInstance.getRoutes();
      for (const cmd of routes) {
        if (this.moduleRoutes.has(cmd)) {
          console.warn(`Маршрут ${cmd} уже зарегистрирован!`);
        }
        this.moduleRoutes.set(cmd, moduleInstance);
      }
    }

    if (this.isInitialized) {
      moduleInstance.init();
    }
  }

  init() {
    this.modules.forEach((m) => {
      try {
        m.init();
      } catch (e) {
        console.error("Ошибка инициализации модуля:", e);
      }
    });
    this.isInitialized = true;
  }

  loadConfig(config) {
    this.modules.forEach((m) => {
      try {
        m.load(config);
      } catch (e) {
        console.error(
          `Ошибка загрузки модуля ${m.constructor?.name || "unknown"}:`,
          e
        );
      }
    });
  }

  collectConfigForModule(moduleName) {
    const module = this.modules.find((m) => m._moduleName === moduleName);
    if (!module) return null;
    try {
      return module.save();
    } catch (e) {
      console.error(`Ошибка сохранения модуля ${moduleName}:`, e);
      throw e;
    }
  }

  handleResponse(data) {
    // Проверяем по target (если есть)
    if (data.target) {
      const module = this.moduleRoutes.get(data.target);
      if (module && typeof module.handleResponse === "function") {
        try {
          module.handleResponse(data);
        } catch (e) {
          console.error(
            `Ошибка обработки ответа в модуле ${module._moduleName}:`,
            e
          );
        }
        return;
      }
    }

    // Проверяем по action (если есть)
    if (data.action) {
      const module = this.moduleRoutes.get(data.action);
      if (module && typeof module.handleResponse === "function") {
        try {
          module.handleResponse(data);
        } catch (e) {
          console.error(
            `Ошибка обработки ответа в модуле ${module._moduleName}:`,
            e
          );
        }
        return;
      }
    }

    // Если нет маршрута — передаём всем
    this.modules.forEach((m) => {
      if (typeof m.handleResponse === "function") {
        try {
          m.handleResponse(data);
        } catch (e) {
          console.error(
            `Ошибка обработки ответа в модуле ${m._moduleName}:`,
            e
          );
        }
      }
    });
  }

  handleEvent(data) {
    if (data.event) {
      const module = this.moduleRoutes.get(data.event);
      if (module && typeof module.handleEvent === "function") {
        try {
          module.handleEvent(data);
        } catch (e) {
          console.error(
            `Ошибка обработки события в модуле ${module._moduleName}:`,
            e
          );
        }
        return;
      }
    }

    // Если нет маршрута — передаём всем
    this.modules.forEach((m) => {
      if (typeof m.handleEvent === "function") {
        try {
          m.handleEvent(data);
        } catch (e) {
          console.error(
            `Ошибка обработки события в модуле ${m._moduleName}:`,
            e
          );
        }
      }
    });
  }

  // Метод для вызова метода другого модуля
  callModule(moduleName, method, ...args) {
    const module = this.modules.find((m) => m._moduleName === moduleName);
    if (module && typeof module[method] === "function") {
      return module[method](...args);
    } else {
      console.warn(`Модуль ${moduleName} или метод ${method} не найден`);
    }
  }

  callAllModulesAndSendWS(method, ...args) {
    const results = [];

    this.modules.forEach((module) => {
      if (typeof module[method] === "function") {
        try {
          const result = module[method](...args);
          results.push(result);

          // Отправляем результат в sendWS, если он определён
          if (typeof window.sendWS === "function") {
            try {
              window.sendWS(result);
            } catch (e) {
              console.error("Ошибка отправки данных в sendWS:", e);
            }
          } else {
            console.warn("Функция sendWS не найдена в глобальном контексте");
          }
        } catch (e) {
          console.error(
            `Ошибка вызова метода ${method} в модуле ${module._moduleName}:`,
            e
          );
          results.push(undefined);
        }
      } else {
        results.push(undefined);
      }
    });

    return results;
  }
}

// Глобальный доступ (если нужно)
window.SettingsCore = new SettingsCore();
