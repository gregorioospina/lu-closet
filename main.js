// The hand-drawn frames are generated in WebAssembly (src/frame.c).
// This file only loads the module and wires the result to the page.
const [{ instance }, items] = await Promise.all([
  WebAssembly.instantiateStreaming(fetch('frame.wasm')),
  fetch('items.json').then((r) => r.json()),
]);
const { frame, len, memory, hash_buf, thumb, thumb_w, thumb_h } = instance.exports;
const text = new TextDecoder();
const framePath = (seed) => {
  const ptr = frame(seed);
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

const grid = document.getElementById('grid');
const dialog = document.getElementById('info');
const PHONE = '573183309699'; // WhatsApp number, digits only
const chat = document.getElementById('d-chat');
const field = (id, value) => {
  document.getElementById(id).textContent = value || '—';
};
const columns = () => getComputedStyle(grid).gridTemplateColumns.split(' ').length;

// Alternate orange and blue like a checkerboard, whatever the column count.
const paint = () => {
  const n = columns();
  [...grid.children].forEach((card, i) => {
    const odd = (i % n + Math.floor(i / n)) % 2;
    card.style.setProperty('--c', odd ? 'var(--blue)' : 'var(--orange)');
  });
};

items.forEach((item, i) => {
  const card = document.createElement('button');
  card.className = 'card';
  card.innerHTML =
    `<svg viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">` +
    `<path d="${framePath(i + 1)}"/></svg>`;
  const img = new Image();
  img.alt = item.description || '';
  img.decoding = 'async';
  // The first rows load right away; the rest wait until they are near the screen.
  if (i < 6) img.fetchPriority = 'high';
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
  card.prepend(img);
  if (blur) card.prepend(blur);
  card.addEventListener('click', () => {
    field('d-desc', item.description);
    field('d-size', item.size);
    field('d-price', item.price);
    const msg = `Hola Lu, me interesa esta prenda: ${item.description}` +
      (item.size ? ` (talla ${item.size})` : '') + (item.price ? ` - ${item.price}` : '');
    chat.href = `https://wa.me/${PHONE}?text=${encodeURIComponent(msg)}`;
    dialog.showModal();
  });
  grid.append(card);
});
paint();
addEventListener('resize', paint);

// Tap anywhere outside the dialog to close it.
dialog.addEventListener('click', (e) => {
  if (e.target === dialog) dialog.close();
});
