// mqtt_module.js
class MqttModule extends BaseSettingsModule {
  constructor() {
    super();
    this.offBtn = document.getElementById("mqtt-off-btn");
    this.onBtn = document.getElementById("mqtt-on-btn");
    this.fields = document.getElementById("mqtt-fields");
    this.server = document.getElementById("mqtt-server");
    this.port = document.getElementById("mqtt-port");
    this.user = document.getElementById("mqtt-user");
    this.password = document.getElementById("mqtt-password");
    this.toggleBtn = document.getElementById("toggle-mqtt-password");

    this.mqttTestBtn = document.getElementById("test-mqtt-btn");
    this.mqttTestLoader = document.getElementById("mqtt-test-loader");
  }

  getRoutes() {
    return ["mqtt"];
  }

  init() {
    if (this.toggleBtn) {
      this.toggleBtn.addEventListener("click", () => {
        this.togglePasswordVisibility(this.password, this.toggleBtn);
      });
    }
    if (this.offBtn)
      this.offBtn.addEventListener("click", () => this.setMode(false));
    if (this.onBtn)
      this.onBtn.addEventListener("click", () => this.setMode(true));

    if (this.mqttTestBtn) {
      this.mqttTestBtn.addEventListener("click", () => {
        this.setMqttTestButtonState(true, "Идёт подключение...", true);
        const mqtt_partial = this.getMqttPartial();
        if (mqtt_partial) {
          this.sendWS({
            type: "request",
            target: "mqtt",
            action: "test_connection",
            data: mqtt_partial,
          });
        }
      });
    }

    this.setMode(false);
  }

  setMode(enabled) {
    if (this.offBtn) this.offBtn.classList.toggle("active", !enabled);
    if (this.onBtn) this.onBtn.classList.toggle("active", enabled);
    if (this.fields) this.fields.style.display = enabled ? "block" : "none";
  }

  getMqttPartial() {
    const enable = this.onBtn?.classList.contains("active") || false;
    const server = this.server?.value?.trim();
    const port = this.port?.value?.trim();
    const user = this.user?.value?.trim();
    const password = this.password?.value?.trim();

    const missingFields = [];

    if (!server) missingFields.push("Сервер");
    if (!port) missingFields.push("Порт");
    if (!user) missingFields.push("Пользователь");
    if (!password) missingFields.push("Пароль");

    if (missingFields.length > 0) {
      alert(`Необходимо ввести: ${missingFields.join(", ")}`);
      return null;
    }

    const portNum = parseInt(port, 10);
    if (isNaN(portNum) || portNum < 0 || portNum > 0xffff) {
      alert("Порт должен быть числом от 0 до 65535");
      return null;
    }

    return {
      enable: enable.toString(),
      server,
      port: portNum.toString(),
      user,
      password,
    };
  }

  save() {
    return this.getMqttPartial();
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "mqtt",
      action: "load_partial",
    };
  }

  setMqttTestButtonState(
    loading,
    text = "🛠️ Проверить соединение",
    showLoader = true
  ) {
    const mqttBtn = document.getElementById("test-mqtt-btn");
    const mqttLoader = document.getElementById("mqtt-test-loader");
    const mainBtnText = mqttBtn.querySelector(".general-btn-text");
    const loaderText = document.getElementById("mqtt-test-loader-text");

    if (!mqttBtn || !mqttLoader || !mainBtnText || !loaderText) return;

    mqttBtn.disabled = loading;

    if (loading && showLoader) {
      mainBtnText.style.display = "none";
      loaderText.textContent = text; // Обновляем текст лоадера
      mqttLoader.style.display = "flex";
    } else {
      mainBtnText.style.display = "inline";
      mqttLoader.style.display = "none";
      mainBtnText.textContent = text; // Обновляем основной текст кнопки
    }
  }

  setMqttTestButtonEnabled(enabled) {
    const mqttBtn = document.getElementById("test-mqtt-btn");

    if (!mqttBtn) return;

    mqttBtn.disabled = !enabled;
  }

  parseBoolean(str) {
    if (typeof str === "string") {
      return str.toLowerCase() === "true";
    }
    return Boolean(str);
  }

  handleResponse(data) {
    const handler = this.responseHandlers[data.status];
    if (handler) handler.call(this, data);
  }

  responseHandlers = {
    saved_partial: (data) => {
      this.callModule("control", "setSaveButtonState", false);
      this.callModule(
        "control",
        "showSettingsStatus",
        "success",
        "✅ Настройки сохранены"
      );
    },
    error_partial: (data) => {
      this.callModule("control", "setSaveButtonState", false);
      this.callModule(
        "control",
        "showSettingsStatus",
        "error",
        "❌ Ошибка сохранения"
      );
    },
    load_partial: (data) => {
      const load_data = data?.data || {};
      const enable = this.parseBoolean(load_data?.enable);
      this.setMode(enable);
      if (enable) {
        if (this.server) this.server.value = load_data.server || "";
        if (this.port) this.port.value = parseInt(load_data.port) || 0;
        if (this.user) this.user.value = load_data.user || "";
        if (this.password) this.password.value = load_data.password || "";
      }
    },
    error_mqtt: (data) => {
      this.setMqttTestButtonState(false);
    },
    mqtt_test_ok: (data) => {
      this.setMqttTestButtonState(true, "Идёт подключение...", true);
    },
    mqtt_test_error: (data) => {
      this.setMqttTestButtonState(true, "Ошибка соединения ⚠️", false);
      setTimeout(() => {
        this.setMqttTestButtonState(false);
      }, 2500);
    },
  };

  handleEvent(data) {
    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  eventHandlers = {
    mqtt_connection_success: (data) => {
      this.setMqttTestButtonState(true, "Соединение установлено ✅", false);
      setTimeout(() => {
        this.setMqttTestButtonState(false);
      }, 2500);
    },
    mqtt_connection_failed: (data) => {
      this.setMqttTestButtonState(true, "Ошибка соединения ⚠️", false);
      setTimeout(() => {
        this.setMqttTestButtonState(false);
      }, 2500);
    },
  };

  sendWS(data) {
    if (window.sendWS) return window.sendWS(data);
    else return false;
  }
}
