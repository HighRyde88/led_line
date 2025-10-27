class NetworkModule extends BaseSettingsModule {
  constructor() {
    super();
    this.dhcpBtn = document.getElementById("dhcp-btn");
    this.staticBtn = document.getElementById("static-btn");
    this.staticFields = document.getElementById("static-ip-fields");
    this.ipInput = document.getElementById("static-ip");
    this.nmInput = document.getElementById("subnet-mask");
    this.gwInput = document.getElementById("gateway");
  }

  isValidIPv4(str) {
    if (!str) return false;
    const parts = str.split(".");
    if (parts.length !== 4) return false;
    return parts.every((part) => {
      const num = Number(part);
      return !isNaN(num) && num >= 0 && num <= 255 && String(num) === part;
    });
  }

  getRoutes() {
    return ["network", "device_network"]; // Команды, которые обрабатывает модуль
  }

  init() {
    if (this.dhcpBtn)
      this.dhcpBtn.addEventListener("click", () => this.setMode("dhcp"));
    if (this.staticBtn)
      this.staticBtn.addEventListener("click", () => this.setMode("static"));
    this.setMode("dhcp");
  }

  setMode(mode) {
    const isStatic = mode === "static";
    if (this.dhcpBtn) this.dhcpBtn.classList.toggle("active", !isStatic);
    if (this.staticBtn) this.staticBtn.classList.toggle("active", isStatic);
    if (this.staticFields)
      this.staticFields.style.display = isStatic ? "block" : "none";
  }

  save() {
    const isStatic = this.staticBtn?.classList.contains("active") || false;
    if (isStatic) {
      const ip = this.ipInput?.value.trim() || "";
      const nm = this.nmInput?.value.trim() || "";
      const gw = this.gwInput?.value.trim() || "";
      if (!this.isValidIPv4(ip)) throw new Error("Некорректный IP-адрес");
      if (!this.isValidIPv4(nm)) throw new Error("Некорректная маска подсети");
      if (!this.isValidIPv4(gw)) throw new Error("Некорректный шлюз");
    }
    return {
      mode: isStatic ? "static" : "dhcp",
      ip: this.ipInput?.value.trim() || "192.168.0.1",
      netmask: this.nmInput?.value.trim() || "255.255.255.0",
      gateway: this.gwInput?.value.trim() || "192.168.1.1",
    };
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "network",
      action: "load_partial",
    };
  }

  setNetworkValue(ip, netmask, gateway) {
    if (this.ipInput) this.ipInput.value = ip || "";
    if (this.nmInput) this.nmInput.value = netmask || "";
    if (this.gwInput) this.gwInput.value = gateway || "";
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
      const mode = load_data?.mode || "dhcp";
      this.setMode(mode);
      if (mode === "static") {
        if (this.ipInput) this.ipInput.value = load_data.ip || "";
        if (this.nmInput) this.nmInput.value = load_data.netmask || "";
        if (this.gwInput) this.gwInput.value = load_data.gateway || "";
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
}
