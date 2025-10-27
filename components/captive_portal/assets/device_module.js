class DeviceModule extends BaseSettingsModule {
  constructor() {
    super();
    this.hostname = document.getElementById("hostname");
    //this.connCode = document.getElementById('connection-code');
    //this.toggleBtn = document.getElementById('toggle-connection-code');
  }

  getRoutes() {
    return ["device"];
  }

  init() {
    //if (this.toggleBtn) {
    //  this.toggleBtn.addEventListener('click', () => {
    //    this.togglePasswordVisibility(this.connCode, this.toggleBtn);
    //  });
    //}
  }

  save() {
    const hostnameValue = this.hostname?.value?.trim() || "";

    if (hostnameValue) {
      return {
        hostname: hostnameValue,
      };
    } else {
      alert("Имя хоста по умолчанию - esp32device");
      return {
        hostname: "esp32device",
      };
    }
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "device",
      action: "load_partial",
    };
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
      this.hostname.value = this.hostname?.value.trim() || "esp32device";
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

      if (this.hostname) this.hostname.value = load_data.hostname || "";
    },
  };
}
