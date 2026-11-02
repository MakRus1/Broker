// Значение влияет только на оформление. Протокол входа обслуживает Keycloak.
(() => {
  const valid = (value) =>
    value === "light" || value === "dark" || value === "system";
  const requested = new URLSearchParams(window.location.search).get(
    "broker_theme",
  );
  let theme = "system";
  try {
    if (valid(requested))
      sessionStorage.setItem("broker:login-theme", requested);
    const saved = sessionStorage.getItem("broker:login-theme");
    if (valid(saved)) theme = saved;
  } catch {
    if (valid(requested)) theme = requested;
  }
  document.documentElement.dataset.theme = theme;
})();
