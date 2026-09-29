// Qt 5.15 ships Chromium 83. Small standards polyfills for the pinned browser components.
if (!Element.prototype.replaceChildren) {
 Element.prototype.replaceChildren=function(...nodes){while(this.firstChild)this.removeChild(this.firstChild);this.append(...nodes);};
}
