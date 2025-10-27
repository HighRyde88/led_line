class ControlModule extends BaseSettingsModule {
  constructor() {
    super();
    // Ссылки на DOM-элементы
    this.saveGeneralBtn = document.getElementById("save-settings-btn");
    this.generalSaveLoader = document.getElementById("general-save-loader");
    this.generalSettingsStatus = document.getElementById(
      "general-settings-status"
    );
    this.resetBtn = document.getElementById("reset-btn");
    this.rebootBtn = document.getElementById("reboot-btn");
    this.logoutBtn = document.getElementById("logout-btn");

    // Состояния
    this.notificationTimers = { settingsStatus: null };
  }

  getRoutes() {
    return ["control"];
  }

  init() {
    this.bindEvents();
    this.initAccordion();
    this.updateSaveButtonState();
  }

  bindEvents() {
    // Кнопка сохранить
    if (this.saveGeneralBtn) {
      this.saveGeneralBtn.addEventListener("click", () =>
        this.handleSaveClick()
      );
    }

    // Кнопка сброса
    if (this.resetBtn) {
      this.resetBtn.addEventListener("click", () => this.handleResetClick());
    }

    // Кнопка перезагрузки
    if (this.rebootBtn) {
      this.rebootBtn.addEventListener("click", () => this.handleRebootClick());
    }

    // Кнопка выхода
    if (this.logoutBtn) {
      this.logoutBtn.addEventListener("click", () => this.handleLogoutClick());
    }
  }

  // === Управление аккордеоном ===
  initAccordion() {
    const accordionHeaders = document.querySelectorAll(".accordion-header");
    accordionHeaders.forEach((header) => {
      header.addEventListener("click", () => {
        const body = header.nextElementSibling;
        const icon = header.querySelector(".accordion-icon");

        // Закрываем все остальные
        document.querySelectorAll(".accordion-body").forEach((b) => {
          if (b !== body) b.classList.remove("open");
        });
        document.querySelectorAll(".accordion-icon").forEach((i) => {
          if (i !== icon) i.textContent = "►";
        });

        // Переключаем текущий
        if (body.classList.contains("open")) {
          body.classList.remove("open");
          if (icon) icon.textContent = "►";
        } else {
          body.classList.add("open");
          if (icon) icon.textContent = "▼";
        }

        // Обновляем состояние кнопки через ControlModule
        this.updateSaveButtonState();
      });
    });
  }

  // === Управление кнопкой "Сохранить" ===
  updateSaveButtonState() {
    const hasOpenAccordion =
      document.querySelector(".accordion-body.open") !== null;
    if (this.saveGeneralBtn) {
      this.saveGeneralBtn.disabled = !hasOpenAccordion;
    }
  }

  setSaveButtonState(loading, text = "💾 Сохранить настройки") {
    if (!this.saveGeneralBtn || !this.generalSaveLoader) return;
    const btnText = this.saveGeneralBtn.querySelector(".general-btn-text");
    this.saveGeneralBtn.disabled = loading;
    if (btnText) btnText.style.display = loading ? "none" : "inline";
    this.generalSaveLoader.style.display = loading ? "flex" : "none";
    if (!loading && btnText) btnText.textContent = text;
  }

  // === Обработка клика "Сохранить" ===
  handleSaveClick() {
    const openBody = document.querySelector(".accordion-body.open");
    if (!openBody) {
      alert("Откройте раздел, который хотите сохранить");
      return;
    }

    const header = openBody.previousElementSibling;
    const headerText = header
      ?.querySelector("span:first-child")
      ?.textContent?.trim();

    const moduleMap = {
      "📱 Устройство": "device",
      "💡 Настройка ленты": "ledstrip",
      "🌎 Адрес устройства": "network",
      "🛜 Точка доступа": "apoint",
      "🔗 MQTT": "mqtt",
    };

    const moduleName = moduleMap[headerText];
    if (!moduleName) {
      console.warn("Неизвестный модуль:", headerText);
      alert("Не удалось определить раздел для сохранения");
      return;
    }

    try {
      const data = this.core?.collectConfigForModule(moduleName);
      if (!data || Object.keys(data).length === 0) {
        return;
      }

      if (
        this.sendWS({
          type: "request",
          target: moduleName,
          action: "save_partial",
          data,
        })
      ) {
        this.setSaveButtonState(true);
      }
    } catch (err) {
      alert("Ошибка в настройках: " + (err.message || err));
      this.setSaveButtonState(false);
    }
  }

  // === Управление системными действиями ===
  handleResetClick() {
    if (confirm("Сбросить настройки? Устройство перезагрузится.")) {
      uiLoader.show("processing", "Сброс системы...", "#6d5179");
      this.sendWS({
        type: "request",
        target: "control",
        action: "reset",
      });
    }
  }

  handleRebootClick() {
    if (confirm("Перезагрузить устройство?")) {
      uiLoader.show("waiting", "Перезагрузка...", "#aac549b9");
      this.sendWS({
        type: "request",
        target: "control",
        action: "reboot",
      });
    }
  }

  handleLogoutClick() {
    if (confirm("Выйти из панели управления?")) {
      uiLoader.show("waiting", "Выход...", "#aac549b9");
      this.sendWS({
        type: "request",
        target: "control",
        action: "logout",
      });
    }
  }

  // === Управление уведомлениями и лоадерами ===
  showSettingsStatus(type, message) {
    if (!this.generalSettingsStatus) return;
    clearTimeout(this.notificationTimers.settingsStatus);
    this.generalSettingsStatus.className = `settings-status ${type} show`;
    this.generalSettingsStatus.textContent = message;
    this.notificationTimers.settingsStatus = setTimeout(() => {
      this.generalSettingsStatus.classList.remove("show");
    }, 4000);
  }

  handleResponse(data) {
    const handler = this.responseHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  responseHandlers = {};

  handleEvent(data) {
    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  eventHandlers = {};

  callModule(moduleName, method, ...args) {
    return this.core.callModule(moduleName, method, ...args);
  }

  sendWS(data) {
    if (window.sendWS) return window.sendWS(data);
    else return false;
  }
}
