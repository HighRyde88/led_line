class UpdateModule extends BaseSettingsModule {
  constructor() {
    super();
    this.codeInput = document.getElementById("firmware-code");
    this.updateBtn = document.getElementById("update-firmware-btn");
    this.appVersionText = document.getElementById("application-version");
    this.updateLoader = document.getElementById("update-loader");
    this.updateStatus = document.getElementById("update-status");
  }

  getRoutes() {
    return ["update"]; // Команды, которые обрабатывает модуль
  }

  init() {
    this.bindEvents();
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "update",
      action: "load_partial",
    };
  }

  bindEvents() {
    if (this.updateBtn) {
      this.updateBtn.addEventListener("click", () => {
        if (!this.codeInput) return;
        const code = this.codeInput.value.trim().toUpperCase();
        if (!code) {
          alert("Введите код обновления");
          this.codeInput.focus();
          return;
        }
        this.sendWS({
          type: "request",
          cmd: "system",
          action: "update_firmware",
          data: { code: code },
        });
      });
    }

    if (this.codeInput) {
      this.codeInput.addEventListener("input", function () {
        const start = this.selectionStart;
        const end = this.selectionEnd;
        const upperValue = this.value.toUpperCase();
        if (this.value !== upperValue) {
          this.value = upperValue;
          this.setSelectionRange(start, end);
        }
      });
    }
  }

  setUpdateEnabled(enabled) {
    const updateBtn = document.getElementById("update-firmware-btn");
    const codeInput = document.getElementById("firmware-code");

    if (!updateBtn || !codeInput) return;

    codeInput.disabled = !enabled;
    updateBtn.disabled = !enabled;
  }

  handleResponse(data) {
    const handler = this.responseHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  responseHandlers = {
    load_partial: (data) => {
      const versions = data.data;
      if (this.appVersionText && versions?.application) {
        this.appVersionText.textContent = `Текущая версия: ${versions.application}`;
      }
    },
  };

  handleEvent(data) {
    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  eventHandlers = {};

  sendWS(data) {
    if (window.sendWS) window.sendWS(data);
  }
}
