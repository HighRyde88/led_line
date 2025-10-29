const $ = (id) => document.getElementById(id);
const $all = (sel) => document.querySelectorAll(sel);

const tabs = $all(".tab");
const tabContents = $all(".tab-content");
const loader = document.querySelector(".connection-tab-loader");
const loader_text = document.querySelector(".loader-text");
const ethernet_status = $("ethernet-status");

const MAX_RECONNECT_ATTEMPTS = 3;
const RECONNECT_INTERVAL = 1000;

// Удалена локальная переменная webSocket
let reconnectAttempts = 0;
let notificationTimers = { settingsStatus: null, statusAction: null };

function initWebSocket() {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const host = window.location.hostname || "192.168.4.1";
  const port = window.location.port ? `:${window.location.port}` : ":8810";
  const url = `${protocol}//${host}${port}`;
  try {
    window.webSocket = new WebSocket(url);
    window.webSocket.onopen = () => {
      reconnectAttempts = 0;
      console.log("WebSocket connected");
    };
    window.webSocket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        if (data.type === "event" && data.target === "system", data.status === "ws_ready") {
          window.SettingsCore.callAllModulesAndSendWS("onAppStart");
          return;
        }

        routeMessage(data);
      } catch (e) {
        console.error("Parse error:", e);
      }
    };
    window.webSocket.onclose = handleWebSocketClose;
    window.webSocket.onerror = handleWebSocketError;
  } catch (e) {
    console.error("WebSocket init failed:", e);
    handleWebSocketError();
  }
}

function handleWebSocketClose() {
  reconnectIfNeeded(initWebSocket);
}

function handleWebSocketError() {
  reconnectIfNeeded(initWebSocket);
}

function reconnectIfNeeded(callback) {
  if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    reconnectAttempts++;
    const delay = Math.min(RECONNECT_INTERVAL * reconnectAttempts, 10000);
    setTimeout(callback, delay);
  } else {
    showStatus("connecting", "Соединение потеряно");
  }
}

function sendWS(data) {
  if (!window.webSocket || window.webSocket.readyState !== WebSocket.OPEN) {
    return false;
  }

  try {
    console.log(JSON.stringify(data));
    window.webSocket.send(JSON.stringify(data));
    return true; // Успешно отправлено
  } catch (e) {
    console.error("Send error:", e);
    return false;
  }
}

function routeMessage(data) {
  const handler = handlers[data.type] || handlers.default;
  handler(data);
}

const handlers = {
  response: handleResponse,
  event: handleEvent,
  default: (data) => console.log("Unknown message:", data),
};

function handleResponse(data) {
  if (window.SettingsCore) {
    window.SettingsCore.handleResponse(data);
  }
}

function handleEvent(data) {
  if (window.SettingsCore) {
    window.SettingsCore.handleEvent(data);
  }
}

function showLoader(type, text, bg) {
  if (!loader || !loader_text) return;
  loader.classList.remove(
    "scanning",
    "connecting",
    "processing",
    "uploading",
    "waiting"
  );
  loader.classList.add("show", type);
  loader_text.style.marginTop = type === "scanning" ? "20px" : "0px";
  loader.style.background = bg;
  loader_text.textContent = text;
}

function hideLoader() {
  if (!loader) return;
  loader.classList.remove("show");
  setTimeout(() => {
    if (!loader.classList.contains("show")) {
      loader.classList.remove(
        "scanning",
        "connecting",
        "processing",
        "uploading",
        "waiting"
      );
    }
  }, 400);
}

function showStatus(type, message) {
  showLoader(type, message, "rgba(219, 77, 77, 0.95)");
  setTimeout(hideLoader, 1500);
}

if (tabs.length > 0) {
  tabs.forEach((tab) =>
    tab.addEventListener("click", () => activateTab(tab.dataset.tab))
  );
}

/*let startX = 0;
document.body.addEventListener(
  "touchstart",
  (e) => {
    startX = e.touches[0].clientX;
  },
  { passive: true }
);

document.body.addEventListener(
  "touchend",
  (e) => {
    if (!startX) return;
    const endX = e.changedTouches[0].clientX;
    const diff = startX - endX;
    if (Math.abs(diff) < 40) return;
    const activeIndex = [...tabs].findIndex((t) =>
      t.classList.contains("active")
    );
    const newIndex = diff > 0 ? activeIndex + 1 : activeIndex - 1;
    if (newIndex >= 0 && newIndex < tabs.length) {
      activateTab(tabs[newIndex].dataset.tab);
    }
    startX = 0;
  },
  { passive: true }
);*/

function activateTab(tabId) {
  tabs.forEach((t) => t.classList.remove("active"));
  tabContents.forEach((c) => c.classList.remove("active"));
  const tab = document.querySelector(`[data-tab="${tabId}"]`);
  if (tab) tab.classList.add("active");
  const tabContent = $(`${tabId}-tab`);
  if (tabContent) tabContent.classList.add("active");
}

window.addEventListener("DOMContentLoaded", () => {
  
  window.loader = document.querySelector(
    ".connection-tab-loader .loader-inner"
  );
  window.loader_text = document.querySelector(
    ".connection-tab-loader .loader-text"
  );

  initWebSocket();
  const active = document.querySelector(".tab.active");
  if (active) activateTab(active.dataset.tab);

  if (window.SettingsCore) {
    const modules = window.ModulesRegistry?.getModules() || [];
    modules.forEach((module) => window.SettingsCore.registerModule(module));
    window.SettingsCore.init();
  }
});

window.addEventListener("beforeunload", () => {
  Object.values(notificationTimers).forEach((timer) => clearTimeout(timer));
  if (window.webSocket) {
    window.webSocket.onclose = null;
    window.webSocket.close();
  }
});