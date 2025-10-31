class FSM {
  constructor(initialState, transitions, callbacks = {}) {
    this.state = initialState;
    this.transitions = transitions || {};
    this.callbacks = callbacks || {};
  }

  canTransition(to) {
    const allowed = this.transitions[this.state];
    return Array.isArray(allowed) && allowed.includes(to);
  }

  transition(to, data) {
    if (this.canTransition(to)) {
      const from = this.state;
      this.state = to;

      const callback = this.callbacks[to];
      if (typeof callback === "function") {
        callback(data, from, to);
      }

      this.onStateChange && this.onStateChange(from, to, data);

      return true;
    }
    console.warn(`Переход из "${this.state}" в "${to}" недопустим.`);
    return false;
  }

  onChange(callback) {
    this.onStateChange = callback;
  }

  onState(state, callback) {
    this.callbacks[state] = callback;
  }

  reset(initialState) {
    this.state = initialState || Object.keys(this.transitions)[0];
  }
}

class WifiModule extends BaseSettingsModule {
  static COLORS = {
    SUCCESS: "rgba(80, 216, 98, 0.95)",
    ERROR: "rgba(216, 100, 80, 0.96)",
    SCANNING: "rgba(77, 175, 231, 0.9)",
    TIMEOUT: "rgba(240, 53, 53, 0.9)",
  };

  static TIMEOUTS = {
    SCAN: 10000,
    CONNECT: 15000,
  };

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

    this.lastScanResults = [];
    this.lastManualSsid = "";

    this.showTimeoutTimer = null;
    this.webSocket = window.webSocket;
    this.wifiFSM = new FSM("idle", this.wifiTransitions, this.wifiCallbacks);
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
      action: "ap_config",
    };
  }

  bindEvents() {
    this.scanBtn?.addEventListener("click", () => {
      this.wifiFSM.transition("ap_scan");
    });

    this.connectBtn?.addEventListener("click", (e) => {
      e.preventDefault();
      this.hideConnectionInfo();
      if (this.connectBtn.classList.contains("disconnect")) {
        if (confirm("Отключиться от сети?")) {
          this.wifiFSM.transition("ap_disconnect");
        }
      } else {
        const ssidEl = document.getElementById("ssid-select");
        const ssid = ssidEl?.value || "";
        const password = this.password?.value || "";
        if (!ssid) {
          alert("Выберите или введите SSID");
          return;
        }
        this.wifiFSM.transition("ap_connect", {
          ssid: ssid,
          password: password,
        });
      }
    });

    this.toggle?.addEventListener("click", () =>
      this.toggleInputVisibility(this.password, this.toggle)
    );
    this.manualSsidBtn?.addEventListener("click", () => this.toggleSsidInput());

    this.standaloneToggle?.addEventListener("change", (e) => {
      this.blockUi(e.target.checked);
      this.connectBtn.disabled = e.target.checked;

      this.webSocketSend({
        type: "request",
        target: "wifi",
        action: "save_partial",
        data: {
          standalone: e.target.checked.toString(),
        },
      });
    });
  }

  webSocketSend(data) {
    try {
      this.webSocket.send(JSON.stringify(data));
      return true;
    } catch (error) {
      console.error("WebSocket send error:", error);
      return false;
    }
  }

  wifiTransitions = {
    idle: ["ap_scan", "ap_connect", "ap_disconnect", "ap_disconnected"],
    ap_scan: ["ap_scan_error", "idle"],
    ap_scan_error: ["idle"],
    ap_connect: ["ap_wait_ip", "ap_disconnected", "ap_connect_error", "idle"],
    ap_disconnect: ["ap_disconnect_error", "idle"],
    ap_wait_ip: ["ap_check_eth", "ap_connect_error", "idle"],
    ap_check_eth: ["ap_connected", "ap_connect_error", "idle"],
    ap_connected: ["idle"],
    ap_disconnected: ["idle"],
    ap_connect_error: ["idle"],
  };

  wifiCallbacks = {
    idle: () => {
      uiLoader.hide();
      clearTimeout(this.showTimeoutTimer);
    },
    ap_scan: () => {
      if (this.webSocketSend({
        type: "request",
        target: "wifi",
        action: "ap_scan_start",
      })) {
        this.password.value = "";
        uiLoader.show("scanning", "Поиск сетей...", WifiModule.COLORS.SCANNING);
        this.showTimeoutTimer = setTimeout(() => {
          this.wifiFSM.transition("ap_scan_error");
        }, WifiModule.TIMEOUTS.SCAN);
      } else {
        this.wifiFSM.transition("ap_scan_error");
      }
    },
    ap_scan_error: () => {
      this.showError("Ошибка поиска...", () => this.wifiFSM.transition("idle"));
    },
    ap_connect: (data) => {
      const credentials = data || null;
      if (credentials && this.webSocketSend({
        type: "request",
        target: "wifi",
        action: "ap_connect",
        data: credentials,
      })) {
        uiLoader.show("connecting", "Подключение...", WifiModule.COLORS.SUCCESS);
        this.showTimeoutTimer = setTimeout(() => {
          this.showError("Неизвестная ошибка.", () => this.wifiFSM.transition("idle"));
          console.log("Таймаут подключения.");
        }, WifiModule.TIMEOUTS.CONNECT);
      } else {
        this.wifiFSM.transition("idle");
      }
    },
    ap_disconnect: () => {
      this.webSocketSend({
        type: "request",
        target: "wifi",
        action: "ap_disconnect",
      });
      this.showTimeoutTimer = setTimeout(() => {
        this.wifiFSM.transition("idle");
      });
    },
    ap_wait_ip: () => {
      clearTimeout(this.showTimeoutTimer);
      uiLoader.show("connecting", "Ожидание IP адреса...", WifiModule.COLORS.SUCCESS);
      this.showTimeoutTimer = setTimeout(() => {
        this.showError("Неизвестная ошибка.", () => this.wifiFSM.transition("idle"));
        console.log("Таймаут ожидания IP.");
      }, WifiModule.TIMEOUTS.CONNECT);
    },
    ap_check_eth: () => {
      clearTimeout(this.showTimeoutTimer);
      uiLoader.show("connecting", "Проверка соединения...", WifiModule.COLORS.SUCCESS);
      this.showTimeoutTimer = setTimeout(() => {
        this.showError("Неизвестная ошибка.", () => this.wifiFSM.transition("idle"));
        console.log("Таймаут проверки соединения.");
      }, WifiModule.TIMEOUTS.CONNECT);
    },
    ap_connect_error: () => {
      this.showError("Ошибка сервера.", () => this.wifiFSM.transition("idle"));
    },
    ap_disconnected: (data) => {
      clearTimeout(this.showTimeoutTimer);
      this.blockUi(false);
      this.blockSlandstone(false);
      this.buttonConnectionRole("connected");

      const reasonCode = data.data?.reason;
      if (reasonCode != 1) {
        const reason = this.getWiFiErrorMessage(reasonCode);
        this.showError(reason, () => this.wifiFSM.transition("idle"));
        return;
      }

      this.wifiFSM.transition("idle");
    },
    ap_connected: (data) => {
      clearTimeout(this.showTimeoutTimer);
      this.blockUi(true);
      this.blockSlandstone(true);
      this.buttonConnectionRole("disconnect");

      this.onConnected(data);
    },
  };

  showError(message, nextStateCallback) {
    uiLoader.show("connecting", message, WifiModule.COLORS.ERROR);
    setTimeout(() => {
      uiLoader.hide();
      nextStateCallback();
    }, 2000);
  }

  onConnected(data) {
    this.callModule(
      "network",
      "setNetworkValue",
      data.ip,
      data.netmask,
      data.gateway
    );

    const text_status = data.ethernet
      ? "Доступ в интернет есть..."
      : "Доступ в интернет отсутствует...";

    uiLoader.show("connecting", text_status, WifiModule.COLORS.SUCCESS);
    setTimeout(() => {
      uiLoader.hide();
      this.wifiFSM.transition("idle");
    }, 2000);
  }

  handleResponse(data) {
    if (!this.isValidStatus(data.status)) return;

    const handler = this.responseHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  handleEvent(data) {
    if (!this.isValidStatus(data.status)) return;

    const handler = this.eventHandlers[data.status];
    if (handler) {
      handler.call(this, data);
    }
  }

  isValidStatus(status) {
    const valid = [
      ...Object.keys(this.responseHandlers),
      ...Object.keys(this.eventHandlers)
    ];
    return valid.includes(status);
  }

  responseHandlers = {
    ap_scan_result: (data) => {
      clearTimeout(this.showTimeoutTimer);
      const networks = data.data?.networks || null;
      if (networks) {
        this.displayScanResults(networks);
      }
      this.wifiFSM.transition("idle");
    },
    ap_config: (data) => {
      const current = document.getElementById("ssid-select");
      if (current && data.data.ssid && data.data.password && data.data.standalone) {
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
    ap_connect_error: (data) => {
      this.wifiFSM.transition("ap_connect_error", data);
    },
    common_error: (data) => {
      this.wifiFSM.transition("ap_connect_error", data);
    },
  };

  eventHandlers = {
    ap_scan_success: (data) => {
      const ap_count = data.data?.count || 0;
      if (ap_count > 0) {
        if (!this.webSocketSend({
          type: "request",
          target: "wifi",
          action: "ap_scan_result",
        })) {
          this.wifiFSM.transition("ap_scan_error");
        }
      } else {
        this.wifiFSM.transition("idle");
      }
    },
    ap_disconnected_from_reason: (data) => {
      this.updateStatus(null);
      this.wifiFSM.transition("ap_disconnected", data);
    },
    ap_wait_ip: () => {
      this.wifiFSM.transition("ap_wait_ip");
    },
    ap_got_ip: () => {
      this.wifiFSM.transition("ap_check_eth");
    },
    ap_status: (data) => {
      const status = data.data;

      if (this.standaloneToggle) this.standaloneToggle.checked = false;

      this.updateStatus(status);

      this.wifiFSM.transition("ap_connected", status.connect);
    },
  };

  hideConnectionInfo() {
    const infoText = this.connectionInfoText;
    const info = document.querySelector(".connection-info");
    if (infoText) infoText.value = "";
    if (info) info.style.display = "none";
  }

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
          SECURITY: "Неверный пароль.",
          NOT_FOUND: "Не удалось подключиться.",
        }[group];
    }
    return "Не удалось подключиться. Повторите попытку.";
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

  destroy() {
    if (this.showTimeoutTimer) {
      clearTimeout(this.showTimeoutTimer);
      this.showTimeoutTimer = null;
    }
  }
}