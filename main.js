// The hand-drawn frames are generated in WebAssembly (src/frame.c).
// This file only loads the module and wires the result to the page.
const [{ instance }, items] = await Promise.all([
  WebAssembly.instantiateStreaming(fetch('frame.wasm')),
  fetch('items.json').then((r) => r.json()),
]);
const { frame, len, memory } = instance.exports;
const text = new TextDecoder();
const framePath = (seed) => {
  const ptr = frame(seed);
  return text.decode(new Uint8Array(memory.buffer, ptr, len()));
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
  img.src = item.image;
  img.alt = item.description || '';
  img.loading = 'lazy';
  card.prepend(img);
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
