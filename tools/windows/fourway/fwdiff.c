/* [TEMP] [CHOSEN, not recovered] Test tool for compare4.py (fourway comparison): alignment search and
 * mean |diff| of two raw 8-bit RGB frames of the same size.
 *   fwdiff REF.rgb CAND.rgb W H MAXSHIFT MASK HEAT.rgb
 * MASK 1 leaves out the first-person weapon corner (x >= 0.65 W and y >= 0.567 H), 0 compares all.
 * The search finds the shift (dx, dy), |dx|,|dy| <= MAXSHIFT, for which CAND(x+dx, y+dy) best matches
 * REF(x, y) over the inner frame (MAXSHIFT px from every edge, so every shift compares the same
 * pixels): first on every second pixel, then exactly around the best. Prints
 *   dx dy mean0 mean pct
 * (mean0 = mean |diff| at shift 0, mean = at the found shift, both 0-255 over RGB; pct = share of
 * pixels whose largest channel difference exceeds 24) and writes the aligned |diff| x4 as RGB. */
#include <stdio.h>
#include <stdlib.h>

static unsigned char *load(const char *p, long n)
{
    FILE *f = fopen(p, "rb");
    unsigned char *b = malloc(n);
    if (!f || fread(b, 1, n, f) != (size_t)n) { fprintf(stderr, "cannot read %s\n", p); exit(2); }
    fclose(f);
    return b;
}

static int W, H, S, M;
static unsigned char *A, *B;

static int masked(int x, int y) { return M && x * 100 >= 65 * W && y * 1000 >= 567 * H; }

static double score(int dx, int dy, int step, double *pct)
{
    double sum = 0; long n = 0, over = 0;
    for (int y = S; y < H - S; y += step)
        for (int x = S; x < W - S; x += step) {
            if (masked(x, y)) continue;
            const unsigned char *a = A + 3L * (y * W + x), *b = B + 3L * ((y + dy) * W + x + dx);
            int d0 = abs(a[0] - b[0]), d1 = abs(a[1] - b[1]), d2 = abs(a[2] - b[2]);
            sum += d0 + d1 + d2; n++;
            int m = d0 > d1 ? d0 : d1; if (d2 > m) m = d2;
            if (m > 24) over++;
        }
    if (pct) *pct = n ? 100.0 * over / n : 0;
    return n ? sum / (3.0 * n) : 0;
}

int main(int argc, char **argv)
{
    if (argc < 8) { fprintf(stderr, "usage: fwdiff REF CAND W H MAXSHIFT MASK HEAT\n"); return 2; }
    W = atoi(argv[3]); H = atoi(argv[4]); S = atoi(argv[5]); M = atoi(argv[6]);
    long n = 3L * W * H;
    A = load(argv[1], n); B = load(argv[2], n);
    int bx = 0, by = 0; double best = 1e9;
    for (int dy = -S; dy <= S; dy += 1)
        for (int dx = -S; dx <= S; dx += 1) {
            double s = score(dx, dy, 3, 0);
            if (s < best) { best = s; bx = dx; by = dy; }
        }
    int cx = bx, cy = by; best = 1e9;
    for (int dy = cy - 2; dy <= cy + 2; dy++)
        for (int dx = cx - 2; dx <= cx + 2; dx++) {
            if (dx < -S || dx > S || dy < -S || dy > S) continue;
            double s = score(dx, dy, 1, 0);
            if (s < best) { best = s; bx = dx; by = dy; }
        }
    double pct0, pct;
    double m0 = score(0, 0, 1, &pct0), m = score(bx, by, 1, &pct);
    unsigned char *h = calloc(n, 1);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int xs = x + bx, ys = y + by;
            unsigned char *o = h + 3L * (y * W + x);
            if (xs < 0 || ys < 0 || xs >= W || ys >= H) { o[0] = 40; o[1] = 0; o[2] = 40; continue; }
            if (masked(x, y)) { o[0] = o[1] = o[2] = 30; continue; }
            const unsigned char *a = A + 3L * (y * W + x), *b = B + 3L * (ys * W + xs);
            for (int c = 0; c < 3; c++) { int d = abs(a[c] - b[c]) * 4; o[c] = d > 255 ? 255 : d; }
        }
    FILE *f = fopen(argv[7], "wb"); fwrite(h, 1, n, f); fclose(f);
    printf("%d %d %.2f %.2f %.2f\n", bx, by, m0, m, pct);
    return 0;
}
