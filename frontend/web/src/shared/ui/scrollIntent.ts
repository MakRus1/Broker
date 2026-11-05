/** Explicit navigation takes priority over automatic workspace scroll restoration. */
export function scrollIntent() {
  window.dispatchEvent(new Event('workspace-scroll-intent'));
}
