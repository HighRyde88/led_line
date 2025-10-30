class LedStripModule extends BaseSettingsModule {
  constructor() {
    super();
    this.lednum = document.getElementById("led-count");
    this.hostname = document.getElementById("next-device-hostname");
    this.ledpin = document.getElementById("led-pin");
  }

  getRoutes() {
    return ["ledstrip"];
  }

  init() {}

  save() {
    const lednumValue = this.lednum?.value?.trim();
    const ledpinValue = this.ledpin?.value?.trim();

    if (!lednumValue) {
      alert("Введите количество светодиодов!");
      return null;
    }

    if (!ledpinValue) {
      alert("Введите порт подключения!");
      return null;
    }

    const hostnameValue = this.hostname?.value?.trim();
    const result = {
      lednum: lednumValue,
      ledpin: ledpinValue,
    };

    if (hostnameValue) {
      result.hostname = hostnameValue;
    }

    return result;
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "ledstrip",
      action: "load_partial",
    };
  }

  handleResponse(data) {
    const handler = this.responseHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
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
      let error_message = "❌ Ошибка сохранения";

      if (error_message === "invalid ledpin provided") {
        error_message = "❌ Неверный порт подключения";
      } else if(error_message === "invalid lednum provided (must be > 0)"){
        error_message = "❌ Неверное количество светодиодов";
      }
        
      this.callModule("control", "setSaveButtonState", false);
      this.callModule("control", "showSettingsStatus", "error", error_message);
    },
    load_partial: (data) => {
      const load_data = data?.data || {};

      if (this.lednum) this.lednum.value = load_data.lednum || "";
      if (this.ledpin) this.ledpin.value = load_data.ledpin || "";
      if (this.hostname) this.hostname.value = load_data.hostname || "";
    },
  };

  handleEvent(data) {
    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  eventHandlers = {};
}
