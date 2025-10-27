class BaseSettingsModule {
  constructor() {
    this.core = null; // Изначально ядро не установлено
  }

  // Метод для установки ядра (для гибкости и тестирования)
  setCore(core) {
    this.core = core;
  }

  init() {
    throw new Error("Метод init() должен быть реализован в подклассе");
  }

  save() {
    throw new Error("Метод save() должен быть реализован в подклассе");
  }

  /**
   * Обработка ответов от сервера
   * @param {Object} data - данные ответа
   */
  handleResponse(data) {
    // Метод можно переопределить в подклассе
  }

  /**
   * Обработка событий от сервера
   * @param {Object} data - данные события
   */
  handleEvent(data) {
    // Метод можно переопределить в подклассе
  }

  // Универсальный вызов метода другого модуля
  callModule(moduleName, method, ...args) {
    if (this.core) {
      return this.core.callModule(moduleName, method, ...args);
    } else {
      console.warn("Core не установлен для модуля", this.constructor.name);
      return null;
    }
  }

  togglePasswordVisibility(input, button) {
    if (!input || !button) return;
    const isPassword = input.type === "password";
    input.type = isPassword ? "text" : "password";
    button.textContent = isPassword ? "👁️" : "🙈";
  }
}
