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
