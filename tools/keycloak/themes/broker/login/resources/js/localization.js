// Публичные тексты. Запрос не содержит данные формы и учётные данные.
(() => {
  const endpoint = document.querySelector(
    'meta[name="broker-localization-url"]',
  )?.content;
  if (!endpoint) return;
  const locale = document.documentElement.lang.toLowerCase().split(/[-_]/)[0];
  if (locale !== "ru" && locale !== "en") return;
  const targets = {
    password: 'label[for="password"]',
    doLogIn: "#kc-login",
    brokerLoginHint: '[data-broker-message="brokerLoginHint"]',
  };
  // Заголовки остальных действий (сброс пароля, OTP и т. п.) принадлежат Keycloak.
  if (document.body.dataset.pageId === "login-login")
    targets.loginAccountTitle = "#kc-page-title";
  const usernameKey = document.querySelector(
    'meta[name="broker-username-key"]',
  )?.content;
  if (["username", "usernameOrEmail", "email"].includes(usernameKey))
    targets[usernameKey] = 'label[for="username"]';
  const initial = new Map();
  for (const [key, selector] of Object.entries(targets)) {
    const element = document.querySelector(selector);
    if (!element) continue;
    initial.set(key, {
      element,
      text:
        element instanceof HTMLInputElement
          ? element.value
          : element.textContent,
    });
  }
  let pending = false;
  async function refresh() {
    if (pending || document.visibilityState === "hidden") return;
    pending = true;
    try {
      const url = new URL(endpoint);
      if (url.protocol !== "https:" && url.protocol !== "http:") return;
      url.searchParams.set("locale", locale);
      const response = await fetch(url, {
        credentials: "omit",
        referrerPolicy: "no-referrer",
        signal: AbortSignal.timeout(5000),
      });
      if (!response.ok) return;
      const catalog = await response.json();
      if (!Array.isArray(catalog.messages) || catalog.messages.length > 10000)
        return;
      const messages = new Map();
      for (const item of catalog.messages) {
        if (
          item &&
          typeof item.key === "string" &&
          typeof item.value === "string" &&
          item.value.trim() &&
          item.value.length <= 2048 &&
          !/[{}]/.test(item.value)
        )
          messages.set(item.key, item.value);
      }
      for (const [key, { element, text }] of initial) {
        const value = messages.get("keycloak." + key) ?? text;
        if (element instanceof HTMLInputElement) element.value = value;
        else element.textContent = value;
      }
    } catch {
      // При сбое остаются встроенные или последние опубликованные тексты.
    } finally {
      pending = false;
    }
  }
  void refresh();
  window.setInterval(() => void refresh(), 60000);
  document.addEventListener("visibilitychange", () => void refresh());
})();
