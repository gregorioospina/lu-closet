// The frames and the blurred placeholders are generated in WebAssembly
// (src/frame.c). This file loads the module and builds the page.
const PHONE = '573183309699'; // WhatsApp number, digits only
const COLORS = ['#f2731a', '#1a59d9', '#8fb0de', '#d2604f', '#2f2f2a', '#e3a72f', '#4f9a78', '#e58fb1'];

const [{ instance }, items] = await Promise.all([
  WebAssembly.instantiateStreaming(fetch('frame.wasm')),
  fetch('items.json').then((r) => r.json()),
]);
const { frame, len, styles, pick, memory, hash_buf, thumb, thumb_w, thumb_h } = instance.exports;
const text = new TextDecoder();
const framePath = (seed, style) => {
  const ptr = frame(seed, style);
  return text.decode(new Uint8Array(memory.buffer, ptr, len()));
};

// Blurred placeholder: frame.wasm turns the item's ThumbHash into a few
// pixels, drawn on a small canvas that sits behind the image until it loads.
const placeholder = (b64) => {
  const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
  new Uint8Array(memory.buffer, hash_buf(), bytes.length).set(bytes);
  const ptr = thumb(bytes.length);
  const w = thumb_w(), h = thumb_h();
  if (!w || !h) return null;
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const pixels = new Uint8ClampedArray(memory.buffer.slice(ptr, ptr + w * h * 4));
  canvas.getContext('2d').putImageData(new ImageData(pixels, w, h), 0, 0);
  canvas.setAttribute('aria-hidden', 'true');
  return canvas;
};

const el = (tag, className, content) => {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (content) node.textContent = content;
  return node;
};

const list = document.getElementById('list');
let lastStyle = -1;
const recentColors = [];

items.forEach((item, i) => {
  // Pseudo-random frame style and colour: fixed for each position, and the style
  // never repeats the item above and the colour never repeats the last three.
  const h = pick(i + 1);
  let style = h % styles();
  let color = (h >>> 8) % COLORS.length;
  if (style === lastStyle) style = (style + 1) % styles();
  while (recentColors.includes(color)) color = (color + 1) % COLORS.length;
  lastStyle = style;
  recentColors.push(color);
  if (recentColors.length > 3) recentColors.shift();

  const row = el('article', 'item');
  row.style.setProperty('--c', COLORS[color]);

  const box = el('div', 'frame');
  box.innerHTML =
    `<svg viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">` +
    `<path d="${framePath(i + 1, style)}"/></svg>`;
  const img = new Image();
  img.alt = item.description || '';
  img.decoding = 'async';
  // The first rows load right away; the rest wait until they are near the screen.
  if (i < 4) img.fetchPriority = 'high';
  else img.loading = 'lazy';
  const blur = item.thumb ? placeholder(item.thumb) : null;
  const show = () => {
    img.classList.add('ready');
    if (blur) blur.classList.add('gone');
  };
  img.addEventListener('load', show);
  img.addEventListener('error', show);
  img.src = item.image;
  if (img.complete) show();
  box.prepend(img);
  if (blur) box.prepend(blur);

  const info = el('div', 'info');
  info.append(el('p', '', item.description));
  const dl = el('dl');
  if (item.size) dl.append(el('dt', '', 'Talla'), el('dd', '', item.size));
  if (item.price) dl.append(el('dt', '', 'Precio'), el('dd', '', item.price));
  if (dl.children.length) info.append(dl);
  const msg = `Hola Lu, me interesa esta prenda: ${item.description}` +
    (item.size ? ` (talla ${item.size})` : '') + (item.price ? ` - ${item.price}` : '');
  const chat = el('a', '', 'Habla conmigo');
  chat.href = `https://wa.me/${PHONE}?text=${encodeURIComponent(msg)}`;
  chat.target = '_blank';
  chat.rel = 'noopener';
  info.append(chat);

  row.append(box, info);
  list.append(row);
});
