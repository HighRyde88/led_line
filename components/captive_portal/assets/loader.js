class UILoader {
  constructor() {
    this.loader = document.querySelector(".connection-tab-loader");
    this.loaderInner = document.querySelector(".connection-tab-loader .loader-inner");
    this.loaderText = document.querySelector(".connection-tab-loader .loader-text");
  }

  show(type, text, bg) {
    if (!this.loader || !this.loaderText) return;

    this.loader.classList.remove("scanning", "connecting", "processing", "uploading", "waiting");
    this.loader.classList.add("show", type);

    this.loaderText.style.marginTop = type === "scanning" ? "20px" : "0px";
    this.loaderText.style.marginLeft = type === "scanning" ? "20px" : "0px";

    this.loaderText.textContent = text;
    this.loader.style.backgroundColor = bg;
  }

  hide() {
    if (this.loader) {
      this.loader.classList.remove("show");
    }
  }
}

// Использование:
const uiLoader = new UILoader();