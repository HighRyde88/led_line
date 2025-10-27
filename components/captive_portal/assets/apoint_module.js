class ApointModule extends BaseSettingsModule {
  constructor() {
    super();
    this.ssid = document.getElementById("captive-ssid");
    this.password = document.getElementById("captive-password");
    this.toggle = document.getElementById("toggle-captive-password");
  }

  getRoutes() {
    return ["apoint"];
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "apoint",
      action: "load_partial",
    };
  }

  init() {
    if (this.toggle) {
      this.toggle.addEventListener("click", () => {
        this.togglePasswordVisibility(this.password, this.toggle);
      });
    }
  }

  save() {
    const ssidValue = this.ssid?.value.trim();

    if (!ssidValue) {
      alert("SSID не может быть пустым.");
      return null;
    }

    const passwordValue = this.password?.value.trim();
    const result = { ssid: ssidValue };

    if (passwordValue) {
      result.password = passwordValue;
    }

    return result;
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

      if (this.ssid) this.ssid.value = load_data.ssid || "";
      if (this.password) this.password.value = load_data.password || "";
    },
  };
}
