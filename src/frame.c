// Hand-drawn frame generator. Compiled to WebAssembly (no libc, no runtime).
// frame(seed) writes an SVG path string for a wobbly rectangle in a
// 100x100 box into memory and returns its address; len() returns its length.
static char out[8192];
static int n;
static unsigned s;

static float rnd(void) {            // xorshift32 -> [-1, 1]
  s ^= s << 13; s ^= s >> 17; s ^= s << 5;
  return (float)(s & 0xffff) / 32767.5f - 1.0f;
}
static void ch(char c) { if (n < (int)sizeof(out) - 1) out[n++] = c; }
static void num(float v) {          // one decimal place
  if (v < 0) { ch('-'); v = -v; }
  int i = (int)(v * 10.0f + 0.5f), d = i / 10, k = 1;
  while (d / k >= 10) k *= 10;
  for (; k; k /= 10) ch('0' + d / k % 10);
  ch('.'); ch('0' + i % 10); ch(' ');
}
static void line(float x0, float y0, float x1, float y1) {
  enum { N = 5 };
  float px[N + 1], py[N + 1];
  float nx = -(y1 - y0) / 100.0f, ny = (x1 - x0) / 100.0f;
  float drift = rnd() * 1.1f;
  for (int i = 0; i <= N; i++) {
    float t = (float)i / N;
    float o = rnd() * 0.55f + drift * 4.0f * t * (1.0f - t);
    px[i] = x0 + (x1 - x0) * t + nx * o;
    py[i] = y0 + (y1 - y0) * t + ny * o;
  }
  ch('M'); num(px[0]); num(py[0]);
  for (int i = 0; i < N; i++) {     // Catmull-Rom -> cubic Bezier
    int a = i ? i - 1 : 0, c = i + 1, d = i + 2 > N ? N : i + 2;
    ch('C');
    num(px[i] + (px[c] - px[a]) / 6); num(py[i] + (py[c] - py[a]) / 6);
    num(px[c] - (px[d] - px[i]) / 6); num(py[c] - (py[d] - py[i]) / 6);
    num(px[c]); num(py[c]);
  }
}
__attribute__((export_name("frame")))
char *frame(unsigned seed) {
  n = 0; s = seed * 2654435761u + 0x9e3779b9u; if (!s) s = 1;
  const float a = 4.0f, b = 96.0f;  // inset of the box
  for (int pass = 0; pass < 2; pass++) {
    float o;                         // overshoot past the corners
    o = 1.0f + (rnd() + 1.0f) * 1.5f; line(a - o, a + rnd(), b + o, a + rnd());
    o = 1.0f + (rnd() + 1.0f) * 1.5f; line(b + rnd(), a - o, b + rnd(), b + o);
    o = 1.0f + (rnd() + 1.0f) * 1.5f; line(b + o, b + rnd(), a - o, b + rnd());
    o = 1.0f + (rnd() + 1.0f) * 1.5f; line(a + rnd(), b + o, a + rnd(), a - o);
  }
  out[n] = 0;
  return out;
}
__attribute__((export_name("len")))
int len(void) { return n; }

// ---------------------------------------------------------------------------
// Blurred placeholders: decode a ThumbHash (about 25 bytes) into RGBA pixels.
// JS writes the hash bytes at hash_buf(), calls thumb(length), then reads
// thumb_w() x thumb_h() RGBA pixels from the returned address.
static unsigned char hash[64];
static unsigned char rgba[32 * 32 * 4];
static int tw, th;

static float cosine(float x) {       // no libc: reduce to [-pi, pi], then polynomial
  const float PI = 3.14159265f;
  if (x < 0) x = -x;
  x -= (float)(int)(x / (2 * PI)) * 2 * PI;
  if (x > PI) x = 2 * PI - x;
  float sign = 1;
  if (x > PI / 2) { x = PI - x; sign = -1; }
  float x2 = x * x;
  return sign * (1 - x2 / 2 * (1 - x2 / 12 * (1 - x2 / 30 * (1 - x2 / 56))));
}
static int nibble_at;
static float next_ac(float scale) {
  int b = hash[nibble_at >> 1], v = (nibble_at & 1) ? b >> 4 : b & 15;
  nibble_at++;
  return ((float)v / 7.5f - 1.0f) * scale;
}
static int read_channel(float *ac, int nx, int ny, float scale) {
  int k = 0;
  for (int cy = 0; cy < ny; cy++)
    for (int cx = cy ? 0 : 1; cx * ny < nx * (ny - cy); cx++) ac[k++] = next_ac(scale);
  return k;
}
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

__attribute__((export_name("hash_buf")))
unsigned char *hash_buf(void) { return hash; }
__attribute__((export_name("thumb_w"))) int thumb_w(void) { return tw; }
__attribute__((export_name("thumb_h"))) int thumb_h(void) { return th; }

__attribute__((export_name("thumb")))
unsigned char *thumb(int length) {
  const float PI = 3.14159265f;
  tw = th = 0;
  if (length < 5 || length > (int)sizeof(hash)) return rgba;
  unsigned h24 = hash[0] | hash[1] << 8 | hash[2] << 16, h16 = hash[3] | hash[4] << 8;
  float l_dc = (float)(h24 & 63) / 63, p_dc = (float)(h24 >> 6 & 63) / 31.5f - 1,
        q_dc = (float)(h24 >> 12 & 63) / 31.5f - 1, l_s = (float)(h24 >> 18 & 31) / 31,
        p_s = (float)(h16 >> 3 & 63) / 63, q_s = (float)(h16 >> 9 & 63) / 63;
  int alpha = h24 >> 23, land = h16 >> 15, lim = alpha ? 5 : 7, n = h16 & 7;
  int lx = land ? lim : n, ly = land ? n : lim;
  if (lx < 3) lx = 3;
  if (ly < 3) ly = 3;
  float a_dc = alpha ? (float)(hash[5] & 15) / 15 : 1, a_s = alpha ? (float)(hash[5] >> 4) / 15 : 0;
  float l_ac[64], p_ac[8], q_ac[8], a_ac[16];
  nibble_at = (alpha ? 6 : 5) * 2;
  read_channel(l_ac, lx, ly, l_s);
  read_channel(p_ac, 3, 3, p_s * 1.25f);
  read_channel(q_ac, 3, 3, q_s * 1.25f);
  if (alpha) read_channel(a_ac, 5, 5, a_s);
  float ratio = (float)lx / (float)ly;
  tw = ratio > 1 ? 32 : (int)(32 * ratio + 0.5f);
  th = ratio > 1 ? (int)(32 / ratio + 0.5f) : 32;
  float fx[8], fy[8];
  unsigned char *o = rgba;
  for (int y = 0; y < th; y++) {
    for (int c = 0; c < 8; c++) fy[c] = cosine(PI / th * (y + 0.5f) * c);
    for (int x = 0; x < tw; x++) {
      for (int c = 0; c < 8; c++) fx[c] = cosine(PI / tw * (x + 0.5f) * c);
      float l = l_dc, p = p_dc, q = q_dc, a = a_dc;
      int k = 0;
      for (int cy = 0; cy < ly; cy++)
        for (int cx = cy ? 0 : 1; cx * ly < lx * (ly - cy); cx++) l += l_ac[k++] * fx[cx] * fy[cy] * 2;
      k = 0;
      for (int cy = 0; cy < 3; cy++)
        for (int cx = cy ? 0 : 1; cx < 3 - cy; cx++, k++) {
          float f = fx[cx] * fy[cy] * 2;
          p += p_ac[k] * f; q += q_ac[k] * f;
        }
      if (alpha) {
        k = 0;
        for (int cy = 0; cy < 5; cy++)
          for (int cx = cy ? 0 : 1; cx < 5 - cy; cx++) a += a_ac[k++] * fx[cx] * fy[cy] * 2;
      }
      float b = l - 2.0f / 3.0f * p, r = (3 * l - b + q) / 2, g = r - q;
      *o++ = (unsigned char)(255 * clamp01(r)); *o++ = (unsigned char)(255 * clamp01(g));
      *o++ = (unsigned char)(255 * clamp01(b)); *o++ = (unsigned char)(255 * clamp01(a));
    }
  }
  return rgba;
}
