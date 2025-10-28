// wifi_module.js
class WifiModule extends BaseSettingsModule {
  constructor() {
    super();
    this.standaloneToggle = document.getElementById("standalone-mode-toggle");
    this.scanBtn = document.getElementById("scan-btn");
    this.connectBtn = document.getElementById("connect-btn");
    this.connectBtnText = document.getElementById("connect-btn-text");
    this.password = document.getElementById("wifi-password");
    this.toggle = document.getElementById("toggle-wifi-password");
    this.manualSsidBtn = document.getElementById("manual-ssid-btn");
    this.connectionInfoText = document.getElementById("connection-info-text");
    this.ethernet_status = document.getElementById("ethernet-status");

    this.isScanning = false;
    this.isConnecting = false;
    this.scanTimeout = null;
    this.connectTimeout = null;

    this.lastScanResults = [];
    this.isUserDisconnect = false;
    this.lastManualSsid = "";

    // FSM для подключения
    this.connectionState = "idle"; // idle, connecting, getting_ip, connected, failed
  }

  getRoutes() {
    return ["wifi"];
  }

  init() {
    this.bindEvents();
  }

  onAppStart(config) {
    return {
      type: "request",
      target: "wifi",
      action: "load_ap_config",
    };
  }

  bindEvents() {
    this.scanBtn?.addEventListener("click", () => this.startScan());
    this.toggle?.addEventListener("click", () =>
      this.toggleInputVisibility(this.password, this.toggle)
    );
    this.manualSsidBtn?.addEventListener("click", () => this.toggleSsidInput());
    this.connectBtn?.addEventListener("click", (e) => {
      e.preventDefault();
      this.handleConnectClick();
    });
    this.standaloneToggle?.addEventListener("change", (e) => {
      this.blockUi(e.target.checked);
      this.connectBtn.disabled = e.target.checked;

      this.sendWS({
        type: "request",
        target: "wifi",
        action: "save_partial",
        data: {
          standalone: e.target.checked.toString(),
        },
      });
    });
  }

  startScan() {
    if (!this.isScanning) {
      this.sendWS({
        type: "request",
        target: "wifi",
        action: "scan_ap_start",
      });
    }
  }

  handleConnectClick() {
    if (this.connectBtn.classList.contains("disconnect")) {
      if (confirm("Отключиться от сети?")) {
        this.isUserDisconnect = true;
        this.sendWS({
          type: "request",
          target: "wifi",
          action: "disconnect_ap_attempt",
        });
      }
    } else {
      const ssidEl = document.getElementById("ssid-select");
      const ssid = ssidEl?.value || "";
      const password = this.password?.value || "";
      if (!ssid) {
        alert("Выберите или введите SSID");
        return;
      }

      this.startConnection(ssid, password);
    }
  }

  startConnection(ssid, password) {
    this.isUserDisconnect = false;
    this.connectionState = "connecting";

    this.sendWS({
      type: "request",
      target: "wifi",
      action: "connect_ap_attempt",
      data: { ssid, password },
    });

    this.hideConnectionInfo();
    this.showConnectingLoader("Попытка подключения...");
    this.startConnectTimeout();
  }

  hideConnectionInfo() {
    const infoText = this.connectionInfoText;
    const info = document.querySelector(".connection-info");
    if (infoText) infoText.value = "";
    if (info) info.style.display = "none";
  }

  showConnectingLoader(message) {
    uiLoader.show("connecting", message, "rgba(80, 216, 98, 0.95)");
  }

  startConnectTimeout() {
    this.clearConnectTimeout();
    this.connectTimeout = setTimeout(() => {
      if (this.isConnecting) {
        this.connectionState = "failed";
        this.isConnecting = false;
        this.showConnectingLoader("Таймаут подключения.");
        setTimeout(() => uiLoader.hide(), 2000);
      }
    }, 25000);
  }

  clearConnectTimeout() {
    if (this.connectTimeout) {
      clearTimeout(this.connectTimeout);
      this.connectTimeout = null;
    }
  }

  // Обработка WebSocket-ответов
  handleResponse(data) {
    const handler = this.responseHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  responseHandlers = {
    scan_ap_started: (data) => {
      this.isScanning = true;
      uiLoader.hide();
      uiLoader.show("scanning", "Поиск сетей...", "rgba(77, 175, 231, 0.9)");
      clearTimeout(this.scanTimeout);
      this.scanTimeout = setTimeout(() => {
        this.isScanning = false;
        uiLoader.show("scanning", "Таймаут поиска", "rgba(226, 45, 45, 0.95)");
        setTimeout(() => uiLoader.hide(), 1500);
      }, 15000);
    },

    scan_ap_failed: (data) => {
      this.isScanning = false;
      clearTimeout(this.scanTimeout);
      uiLoader.show(
        "scanning",
        "Ошибка сканирования..." + (data.data?.count || ""),
        "rgba(226, 45, 45, 0.95)"
      );
      setTimeout(() => uiLoader.hide(), 1500);
    },

    scan_ap_results: (data) => {
      if (data.data) this.displayScanResults(data.data.networks);
    },

    connect_ap_trying: () => {
      // Таймаут уже установлен в startConnection
    },

    connect_ap_failed: () => {
      this.clearConnectTimeout();
      uiLoader.show(
        "connecting",
        "Ошибка подключения",
        "rgba(219, 77, 77, 0.95)"
      );
      setTimeout(() => uiLoader.hide(), 2000);
    },

    disconnect_ap_success: () => {},

    load_ap_config: (data) => {
      const current = document.getElementById("ssid-select");
      if (
        current &&
        data.data.ssid &&
        data.data.password &&
        data.data.standalone
      ) {
        const input = this.createSsidInput();
        input.value = data.data.ssid || "";
        this.password.value = data.data.password || "";

        if (current.parentNode.classList.contains("select-wrapper")) {
          current.parentNode.replaceWith(input);
        } else {
          current.replaceWith(input);
        }

        this.manualSsidBtn.innerHTML = "✅";

        const standalone = this.parseBoolean(data.data?.standalone);
        if (this.standaloneToggle) this.standaloneToggle.checked = standalone;
        if (this.connectBtn) this.connectBtn.disabled = standalone;
        this.blockUi(standalone);
      }
    },

    load_connection_status: (data) => {
      const connection_status = data.data;
      this.blockUi(true);

      this.blockSlandstone(true);
      if (this.standaloneToggle) this.standaloneToggle.checked = false;

      if (connection_status) {
        this.updateStatus(connection_status);

        const { connect } = connection_status;

        this.callModule(
          "network",
          "setNetworkValue",
          connect.ip,
          connect.netmask,
          connect.gateway
        );

        const text_status = connect.ethernet
          ? "Доступ в интернет есть..."
          : "Доступ в интернет отсутствует...";

        uiLoader.show("connecting", text_status, "rgba(80, 216, 98, 0.95)");
        setTimeout(() => {
          uiLoader.hide();
        }, 1000);
      }
    },
  };

  // Обработка WebSocket-событий
  handleEvent(data) {
    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  eventHandlers = {
    scan_ap_completed: (data) => {
      this.isScanning = false;
      clearTimeout(this.scanTimeout);
      const scan_result = data.data;
      if (scan_result.count > 0) {
        setTimeout(() => {
          this.sendWS({
            type: "request",
            target: "wifi",
            action: "scan_ap_results",
          });
        }, 100);
      } else {
        uiLoader.show(
          "scanning",
          "Сети не найдены!" + scan_result.count,
          "rgba(252, 143, 18, 0.9)"
        );
      }
      setTimeout(() => uiLoader.hide(), 500);
    },

    connection_ap_success: (data) => {
      uiLoader.show(
        "connecting",
        "Получение IP адреса...",
        "rgba(80, 216, 98, 0.95)"
      );
    },

    connection_ap_got_ip: (data) => {
      this.connectionState = "connected";
      setTimeout(() => {
        this.isConnecting = false;
        this.clearConnectTimeout();
        this.sendWS({
          type: "request",
          target: "wifi",
          action: "load_connection_status",
        });
        this.buttonConnectionRole("disconnect");
        if (this.password) this.password.value = "";

        uiLoader.show(
          "connecting",
          "Проверка сети...",
          "rgba(80, 216, 98, 0.95)"
        );
      }, 500);
    },

    disconnected_ap_from_reason: (data) => {
      this.isConnecting = false;
      this.clearConnectTimeout();
      this.connectionState = "failed";

      const reasonCode = data.data?.reason;
      if (reasonCode != 1) {
        const reason = this.getWiFiErrorMessage(reasonCode);
        uiLoader.show("connecting", reason, "rgba(219, 77, 77, 0.95)");
        setTimeout(() => uiLoader.hide(), 2000);
      }

      this.buttonConnectionRole("connect");
      this.blockUi(false);
      this.blockSlandstone(false);
      this.updateStatus(null);
    },
  };

  buttonConnectionRole(role) {
    if (role === "connect") {
      this.connectBtn.type = "button";
      this.connectBtn.classList.remove("disconnect");
      this.connectBtn.classList.add("connect");
      this.connectBtnText.innerHTML = "🔌 Подключиться";
    } else if (role === "disconnect") {
      this.connectBtn.type = "button";
      this.connectBtn.classList.remove("connect");
      this.connectBtn.classList.add("disconnect");
      this.connectBtnText.innerHTML = "Отключиться";
    }
  }

  toggleInputVisibility(input, button) {
    if (!input || !button) return;
    const type = input.type === "password" ? "text" : "password";
    input.type = type;
    button.innerHTML = type === "password" ? "👁️" : "🙈";
  }

  toggleSsidInput() {
    const current = document.getElementById("ssid-select");
    if (!current) return;
    const value = current.value;

    if (this.manualSsidBtn.innerHTML.includes("✏️")) {
      const input = this.createSsidInput();
      input.value = value;
      this.lastManualSsid = value;

      if (current.parentNode.classList.contains("select-wrapper")) {
        current.parentNode.replaceWith(input);
      } else {
        current.replaceWith(input);
      }

      this.manualSsidBtn.innerHTML = "✅";
      this.password.disabled = false;
    } else {
      let selectedValue = value;
      if (current.tagName === "INPUT") this.lastManualSsid = value;

      const select = this.createSsidSelect(this.lastScanResults);
      select.value = selectedValue;

      const wrapper = document.createElement("div");
      wrapper.className = "select-wrapper";
      wrapper.appendChild(select);

      current.replaceWith(wrapper);
      this.manualSsidBtn.innerHTML = "✏️";
    }
  }

  parseBoolean(str) {
    if (typeof str === "string") {
      return str.toLowerCase() === "true";
    }
    return Boolean(str);
  }

  createSsidSelect(networks) {
    const select = document.createElement("select");
    select.id = "ssid-select";
    select.name = "ssid";
    select.required = true;
    select.className = "settings-input";
    const placeholder = new Option("Выберите сеть...", "");
    select.add(placeholder);

    networks
      .sort((a, b) => b.rssi - a.rssi)
      .forEach((ap) => {
        const opt = new Option(`${ap.ssid}`, ap.ssid);
        opt.dataset.authmode = ap.authmode;
        select.add(opt);
      });

    select.addEventListener("change", () => {
      const selectedOption = select.selectedOptions[0];
      const authmode = selectedOption?.dataset.authmode;
      if (this.password) {
        this.password.disabled = authmode === "OPEN";
        if (authmode === "OPEN") this.password.value = "";
      }
    });

    return select;
  }

  createSsidInput() {
    const input = document.createElement("input");
    input.type = "text";
    input.id = "ssid-select";
    input.name = "ssid";
    input.placeholder = "Введите SSID";
    input.required = true;
    input.className = "settings-input";
    return input;
  }

  displayScanResults(networks) {
    const uniqueNetworks = [
      ...new Map(networks.map((net) => [net.ssid, net])).values(),
    ];
    this.lastScanResults = uniqueNetworks.slice(0, 20);

    const oldSelect = document.getElementById("ssid-select");
    if (!oldSelect) return;

    if (oldSelect.tagName === "INPUT") {
      const inputVal = oldSelect.value;
      const newSelect = this.createSsidSelect(this.lastScanResults);
      newSelect.value = inputVal;

      const wrapper = document.createElement("div");
      wrapper.className = "select-wrapper";
      wrapper.appendChild(newSelect);

      oldSelect.parentNode.replaceWith(wrapper);
      this.manualSsidBtn.innerHTML = "✏️";
    } else {
      const newSelect = this.createSsidSelect(this.lastScanResults);
      const wrapper = document.createElement("div");
      wrapper.className = "select-wrapper";
      wrapper.appendChild(newSelect);
      oldSelect.parentNode.replaceWith(wrapper);
    }

    if (this.lastScanResults.length > 0 && this.password) {
      const firstNetwork = this.lastScanResults[0];
      const newSelect = document.getElementById("ssid-select");
      newSelect.value = firstNetwork.ssid;
      this.password.disabled = firstNetwork.authmode === "OPEN";
    }
  }

  updateStatus(data) {
    const info = document.querySelector(".connection-info");
    if (data == null) {
      if (info) info.style.display = "none";
      this.buttonConnectionRole("connect");
      this.ethernet_status.innerHTML = "💻 - 🌐 - ❌";
      return;
    }
    const { connect } = data;
    const infoText = this.connectionInfoText;
    if (infoText && connect.ssid && connect.ip !== "0.0.0.0") {
      //infoText.value = `${connect.ssid} (IP: ${connect.ip})`;
      infoText.value = `${connect.ip}`;
      if (info) info.style.display = "flex";
      this.buttonConnectionRole("disconnect");

      if (connect.ethernet) {
        this.ethernet_status.innerHTML = "💻 - 🌐 - ⚡";
        this.callModule("mqtt", "setMqttTestButtonEnabled", "true");
        this.callModule("update", "setUpdateEnabled", "true");
      } else {
        this.ethernet_status.innerHTML = "💻 - 🌐 - ❌";
        this.callModule("mqtt", "setMqttTestButtonEnabled", "false");
        this.callModule("update", "setUpdateEnabled", "false");
      }
    } else {
      if (info) info.style.display = "none";
      this.buttonConnectionRole("connect");
      this.ethernet_status.innerHTML = "💻-🌐-❌";
    }
  }

  getWiFiErrorMessage(code) {
    const map = {
      SECURITY: [202, 15, 17, 18, 19, 20, 24, 3, 210, 2],
      NOT_FOUND: [201, 211, 212],
    };
    for (const [group, codes] of Object.entries(map)) {
      if (codes.includes(code))
        return {
          SECURITY: "Неверный пароль",
          NOT_FOUND: "Не удалось подключиться",
        }[group];
    }
    return "Не удалось подключиться";
  }

  blockUi(state) {
    const ssidElement = document.getElementById("ssid-select");
    this.scanBtn.disabled = state;
    this.manualSsidBtn.disabled = state;
    this.password.disabled = state;
    this.toggle.disabled = state;
    if (ssidElement) ssidElement.disabled = state;
  }

  blockSlandstone(state) {
    const standaloneElement = document.querySelector(".toggle-group");

    this.standaloneToggle.disabled = state;

    if (state) standaloneElement.classList.add("disabled");
    else standaloneElement.classList.remove("disabled");
  }

  sendWS(data) {
    if (window.sendWS) window.sendWS(data);
  }

  destroy() {
    if (this.scanTimeout) {
      clearTimeout(this.scanTimeout);
      this.scanTimeout = null;
    }
    this.clearConnectTimeout();

    this.isScanning = false;
    this.isConnecting = false;
  }
}
