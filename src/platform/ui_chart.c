#include "../core/engine.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_core.h"
#include "ui_host.h"
#include "ui_shared.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ─── Color palette ──────────────────────────────────────────────────────────
static const uint32_t PALETTE[] = {
  0x4C78A8FF, 0xF58518FF, 0x54A24BFF, 0xEECA3BFF,
  0xE45756FF, 0x72B7B2FF, 0x9D755DFF, 0xB279A2FF,
};
#define PALETTE_SIZE 8

static NVGcolor pal(int i, float a) {
  uint32_t c = PALETTE[i % PALETTE_SIZE];
  return nvgRGBAf(((c>>24)&0xFF)/255.f,((c>>16)&0xFF)/255.f,
                  ((c>>8)&0xFF)/255.f, ((c&0xFF)/255.f)*a);
}
static NVGcolor u32c(uint32_t c, float a) {
  return nvgRGBAf(((c>>24)&0xFF)/255.f,((c>>16)&0xFF)/255.f,
                  ((c>>8)&0xFF)/255.f, ((c&0xFF)/255.f)*a);
}

// ─── Table lookup ────────────────────────────────────────────────────────────
static AslanTable *find_table(Page *page, uint32_t tid) {
  if (!page || !tid) return NULL;
  for (Block *b = page->first_block; b; b = b->next)
    if (b->type == BLOCK_TABLE && b->id == tid && b->data.table.columnar)
      return b->data.table.columnar;
  return NULL;
}

// ─── Series ──────────────────────────────────────────────────────────────────
#define MAX_PTS 64
typedef struct {
  double  v[MAX_PTS];
  char    lbl[MAX_PTS][32];
  int     n;
  double  vmin, vmax, vtotal;
} Series;

static void series_fill(Series *s, AslanTable *tbl, int lc, int vc) {
  memset(s, 0, sizeof *s);
  if (tbl && (int)aslan_table_view_count(tbl) > 0) {
    int n = (int)aslan_table_view_count(tbl);
    if (n > MAX_PTS) n = MAX_PTS;
    s->n = n;
    for (int i = 0; i < n; i++) {
      uint64_t id = aslan_table_view_row_id(tbl, i);
      s->v[i] = (vc >= 0 && (size_t)vc < tbl->column_count)
                ? aslan_table_get_number(tbl, vc, id) : (double)(i+1);
      const char *lbl = (lc >= 0 && (size_t)lc < tbl->column_count)
                        ? aslan_table_get_text(tbl, lc, id) : NULL;
      if (lbl && lbl[0]) strncpy(s->lbl[i], lbl, 31);
      else                snprintf(s->lbl[i], 32, "%d", i+1);
      s->vtotal += s->v[i];
    }
  } else {
    static const double dv[] = {4.2,7.1,3.5,8.9,5.3,6.7};
    static const char *dl[] = {"Jan","Feb","Mar","Apr","May","Jun"};
    s->n = 6;
    for (int i=0;i<6;i++){s->v[i]=dv[i];strncpy(s->lbl[i],dl[i],31);s->vtotal+=dv[i];}
  }
  s->vmin = s->vmax = s->v[0];
  for (int i=1;i<s->n;i++){
    if(s->v[i]<s->vmin) s->vmin=s->v[i];
    if(s->v[i]>s->vmax) s->vmax=s->v[i];
  }
}

static double nice_ceil(double v) {
  if (v <= 0) return 10;
  double e = pow(10.0, floor(log10(v)));
  if (v <= e)   return e;
  if (v <= 2*e) return 2*e;
  if (v <= 5*e) return 5*e;
  return 10*e;
}

// ─── Text helpers ─────────────────────────────────────────────────────────────
static void chart_label(NVGcontext *vg, float x, float y, float sz,
                        int align, NVGcolor col, const char *txt) {
  nvgFontSize(vg, sz); nvgFontFace(vg, "Inter");
  nvgTextAlign(vg, align); nvgFillColor(vg, col);
  nvgText(vg, x, y, txt, NULL);
}

// Draw a centered placeholder when required columns are missing
static void chart_placeholder(NVGcontext *vg, AslanRect r, bool dark,
                               const char *line1, const char *line2) {
  float cx = r.x + r.w * 0.5f;
  float cy = r.y + r.h * 0.5f;
  NVGcolor icon_col = dark ? nvgRGBA(80,82,100,255)  : nvgRGBA(190,192,208,255);
  NVGcolor txt_col  = dark ? nvgRGBA(110,112,130,255): nvgRGBA(155,157,175,255);
  NVGcolor sub_col  = dark ? nvgRGBA(75,77,95,255)   : nvgRGBA(175,177,195,255);

  // Dashed border hint
  nvgBeginPath(vg); nvgRoundedRect(vg, r.x+8, r.y+8, r.w-16, r.h-16, 8);
  nvgStrokeColor(vg, icon_col); nvgStrokeWidth(vg, 1.5f); nvgStroke(vg);

  // Icon: simple table grid symbol
  float gx = cx - 14, gy = cy - 28, gw = 28, gh = 20, gcol = 7, grow = 5;
  for (int row = 0; row <= 2; row++) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, gx, gy + row * grow);
    nvgLineTo(vg, gx + gw, gy + row * grow);
    nvgStrokeColor(vg, icon_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
  }
  for (int col = 0; col <= 4; col++) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, gx + col * gcol, gy);
    nvgLineTo(vg, gx + col * gcol, gy + gh);
    nvgStrokeColor(vg, icon_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
  }

  chart_label(vg, cx, cy - 2, 11, NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM, txt_col, line1);
  chart_label(vg, cx, cy + 4, 10, NVG_ALIGN_CENTER|NVG_ALIGN_TOP,    sub_col, line2);
}

// ─── BAR CHART ───────────────────────────────────────────────────────────────
static void render_bar(NVGcontext *vg, ChartData *cd, AslanRect r,
                       AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)   : nvgRGBA(225,225,230,255);

  float pl=44,pr=8,pt=12,pb=36;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0) return;

  double ymax = nice_ceil(s.vmax);
  int nticks = 5;

  // Grid + Y labels
  for (int i=0;i<=nticks;i++) {
    float ty = oy + ph - (float)i/nticks*ph;
    nvgBeginPath(vg); nvgMoveTo(vg,ox,ty); nvgLineTo(vg,ox+pw,ty);
    nvgStrokeColor(vg, grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[16]; snprintf(buf,16,"%.4g",ymax*i/nticks);
    chart_label(vg, ox-4, ty, 9, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE, text_col, buf);
  }

  // Bars
  if (s.n == 0) return;
  float bw = pw / (float)s.n * 0.7f;
  float gap= pw / (float)s.n;
  NVGcolor bar_col = u32c(cd->color, 1.0f);
  for (int i=0;i<s.n;i++) {
    float bx = ox + i*gap + gap*0.15f;
    float bh = (float)(s.v[i]/ymax)*ph;
    float by = oy + ph - bh;
    if(bh<1) bh=1;
    // shadow
    nvgBeginPath(vg); nvgRoundedRect(vg,bx+1,by+2,bw,bh,3);
    nvgFillColor(vg,nvgRGBA(0,0,0,20)); nvgFill(vg);
    // bar
    nvgBeginPath(vg); nvgRoundedRect(vg,bx,by,bw,bh,3);
    nvgFillColor(vg, pal(i,1.0f)); nvgFill(vg);
    // value
    if (cd->show_values) {
      char buf[16]; snprintf(buf,16,"%.4g",s.v[i]);
      chart_label(vg,bx+bw*0.5f,by-3,8,NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM,text_col,buf);
    }
    // X label (rotate via scissor trick — just clip and draw)
    chart_label(vg,bx+bw*0.5f,oy+ph+14,9,NVG_ALIGN_CENTER|NVG_ALIGN_TOP,text_col,s.lbl[i]);
  }
  (void)bar_col;
}

// ─── LINE / AREA CHART ───────────────────────────────────────────────────────
static void render_line(NVGcontext *vg, ChartData *cd, AslanRect r,
                        AslanTable *tbl, bool dark, bool filled) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)   : nvgRGBA(225,225,230,255);

  float pl=44,pr=8,pt=12,pb=36;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0||s.n<1) return;

  double ymax = nice_ceil(s.vmax);
  int nticks = 5;

  for (int i=0;i<=nticks;i++) {
    float ty = oy + ph - (float)i/nticks*ph;
    nvgBeginPath(vg); nvgMoveTo(vg,ox,ty); nvgLineTo(vg,ox+pw,ty);
    nvgStrokeColor(vg, grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[16]; snprintf(buf,16,"%.4g",ymax*i/nticks);
    chart_label(vg, ox-4, ty, 9, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE, text_col, buf);
  }

  float step = s.n>1 ? pw/(float)(s.n-1) : pw;
  NVGcolor lc = pal(0, 1.0f);

  // Filled area
  if (filled) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, oy+ph);
    for (int i=0;i<s.n;i++) {
      float px = ox + i*step;
      float py = oy + ph - (float)(s.v[i]/ymax)*ph;
      nvgLineTo(vg, px, py);
    }
    nvgLineTo(vg, ox+(s.n-1)*step, oy+ph);
    nvgClosePath(vg);
    NVGpaint grad = nvgLinearGradient(vg, ox, oy, ox, oy+ph,
                                      pal(0,0.35f), pal(0,0.04f));
    nvgFillPaint(vg, grad); nvgFill(vg);
  }

  // Line
  nvgBeginPath(vg);
  for (int i=0;i<s.n;i++) {
    float px = ox + i*step;
    float py = oy + ph - (float)(s.v[i]/ymax)*ph;
    if(i==0) nvgMoveTo(vg,px,py); else nvgLineTo(vg,px,py);
  }
  nvgStrokeColor(vg,lc); nvgStrokeWidth(vg,2.0f); nvgStroke(vg);

  // Dots + labels
  for (int i=0;i<s.n;i++) {
    float px = ox + i*step;
    float py = oy + ph - (float)(s.v[i]/ymax)*ph;
    nvgBeginPath(vg); nvgCircle(vg,px,py,4);
    nvgFillColor(vg, lc); nvgFill(vg);
    nvgBeginPath(vg); nvgCircle(vg,px,py,3);
    nvgFillColor(vg, nvgRGBA(255,255,255,220)); nvgFill(vg);
    if (cd->show_values) {
      char buf[16]; snprintf(buf,16,"%.4g",s.v[i]);
      chart_label(vg,px,py-7,8,NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM,text_col,buf);
    }
    chart_label(vg,px,oy+ph+14,9,NVG_ALIGN_CENTER|NVG_ALIGN_TOP,text_col,s.lbl[i]);
  }
}

// ─── PIE / DONUT CHART ───────────────────────────────────────────────────────
static void render_pie(NVGcontext *vg, ChartData *cd, AslanRect r,
                       AslanTable *tbl, bool dark, bool donut) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  if (s.vtotal <= 0 || s.n == 0) return;

  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  float leg_h = 18.0f * (float)((s.n+1)/2);
  float avail_h = r.h - leg_h - 8;
  float rad = fminf(r.w, avail_h) * 0.42f;
  float cx = r.x + r.w * 0.5f;
  float cy = r.y + avail_h * 0.5f;
  float inner = donut ? rad * 0.52f : 0.0f;

  float angle = -NVG_PI * 0.5f; // start from top
  for (int i=0;i<s.n;i++) {
    float sweep = (float)(s.v[i]/s.vtotal) * NVG_PI * 2.0f;
    NVGcolor col = pal(i, 1.0f);

    nvgBeginPath(vg);
    if (donut) {
      nvgArc(vg, cx, cy, rad,   angle, angle+sweep, NVG_CW);
      nvgArc(vg, cx, cy, inner, angle+sweep, angle, NVG_CCW);
    } else {
      nvgMoveTo(vg, cx, cy);
      nvgArc(vg, cx, cy, rad, angle, angle+sweep, NVG_CW);
    }
    nvgClosePath(vg);
    nvgFillColor(vg, col); nvgFill(vg);

    // Thin separator
    nvgBeginPath(vg);
    if (donut) {
      nvgArc(vg, cx, cy, rad,   angle, angle+sweep, NVG_CW);
      nvgArc(vg, cx, cy, inner, angle+sweep, angle, NVG_CCW);
    } else {
      nvgMoveTo(vg, cx, cy);
      nvgArc(vg, cx, cy, rad, angle, angle+sweep, NVG_CW);
    }
    nvgClosePath(vg);
    nvgStrokeColor(vg, dark ? nvgRGBA(30,32,40,200) : nvgRGBA(255,255,255,200));
    nvgStrokeWidth(vg, 1.5f); nvgStroke(vg);

    // Percentage label inside slice
    if (cd->show_values) {
      float mid = angle + sweep * 0.5f;
      float lr  = donut ? (rad+inner)*0.5f : rad*0.6f;
      float lx  = cx + cosf(mid)*lr;
      float ly  = cy + sinf(mid)*lr;
      float pct = (float)(s.v[i]/s.vtotal)*100.0f;
      if (pct >= 5.0f) {
        char buf[12]; snprintf(buf,12,"%.0f%%", pct);
        chart_label(vg, lx, ly, 9, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,
                    nvgRGBA(255,255,255,230), buf);
      }
    }

    angle += sweep;
  }

  // Legend
  float lx = r.x + 8, ly = r.y + avail_h + 6;
  int col_count = 2;
  float col_w = r.w / col_count;
  for (int i=0;i<s.n;i++) {
    float lbx = lx + (i % col_count) * col_w;
    float lby = ly + (i / col_count) * 18.0f;
    nvgBeginPath(vg); nvgRect(vg, lbx, lby+3, 10, 10);
    nvgFillColor(vg, pal(i,1.0f)); nvgFill(vg);
    char buf[40];
    snprintf(buf, 40, "%s", s.lbl[i]);
    chart_label(vg, lbx+14, lby+8, 10, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, text_col, buf);
  }
}

// ─── HORIZONTAL BAR ──────────────────────────────────────────────────────────
static void render_hbar(NVGcontext *vg, ChartData *cd, AslanRect r,
                        AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)    : nvgRGBA(225,225,230,255);

  float pl=80,pr=12,pt=12,pb=24;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0||s.n==0) return;

  double xmax = nice_ceil(s.vmax);
  int nticks = 4;
  for (int i=0;i<=nticks;i++) {
    float tx = ox + (float)i/nticks*pw;
    nvgBeginPath(vg); nvgMoveTo(vg,tx,oy); nvgLineTo(vg,tx,oy+ph);
    nvgStrokeColor(vg, grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[16]; snprintf(buf,16,"%.4g",xmax*i/nticks);
    chart_label(vg, tx, oy+ph+12, 9, NVG_ALIGN_CENTER|NVG_ALIGN_TOP, text_col, buf);
  }

  float row_h = ph / (float)s.n;
  float bar_h = row_h * 0.65f;
  for (int i=0;i<s.n;i++) {
    float by = oy + i*row_h + (row_h-bar_h)*0.5f;
    float bw = (float)(s.v[i]/xmax)*pw;
    if(bw<1) bw=1;
    nvgBeginPath(vg); nvgRoundedRect(vg,ox,by,bw,bar_h,3);
    nvgFillColor(vg, pal(i,1.0f)); nvgFill(vg);
    chart_label(vg, ox-4, by+bar_h*0.5f, 10, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE, text_col, s.lbl[i]);
    if (cd->show_values) {
      char buf[16]; snprintf(buf,16,"%.4g",s.v[i]);
      chart_label(vg, ox+bw+4, by+bar_h*0.5f, 9, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, text_col, buf);
    }
  }
}

// ─── SCATTER CHART ───────────────────────────────────────────────────────────
static void render_scatter(NVGcontext *vg, ChartData *cd, AslanRect r,
                           AslanTable *tbl, bool dark) {
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)    : nvgRGBA(225,225,230,255);

  float pl=40,pr=8,pt=12,pb=28;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0) return;

  // Build XY pairs
  double xs[MAX_PTS], ys[MAX_PTS]; int n=0;
  if (tbl && (int)aslan_table_view_count(tbl)>0) {
    int cnt = (int)aslan_table_view_count(tbl);
    if(cnt>MAX_PTS) cnt=MAX_PTS; n=cnt;
    for(int i=0;i<cnt;i++){
      uint64_t id = aslan_table_view_row_id(tbl,i);
      int lc = cd->label_col_idx, vc = cd->value_col_idx;
      xs[i] = (lc>=0&&(size_t)lc<tbl->column_count) ? aslan_table_get_number(tbl,lc,id):(double)i;
      ys[i] = (vc>=0&&(size_t)vc<tbl->column_count) ? aslan_table_get_number(tbl,vc,id):(double)i*1.3;
    }
  } else {
    n=8;
    double dx[]={1,2,3,4,5,6,7,8}; double dy[]={2.1,3.5,2.8,5.2,4.1,6.3,5.8,7.2};
    for(int i=0;i<8;i++){xs[i]=dx[i];ys[i]=dy[i];}
  }

  double xmax=xs[0],xmin=xs[0],ymax=ys[0],ymin=ys[0];
  for(int i=1;i<n;i++){
    if(xs[i]>xmax)xmax=xs[i]; if(xs[i]<xmin)xmin=xs[i];
    if(ys[i]>ymax)ymax=ys[i]; if(ys[i]<ymin)ymin=ys[i];
  }
  double xr=xmax-xmin; if(xr<=0)xr=1;
  double yr=ymax-ymin; if(yr<=0)yr=1;

  int nticks=4;
  for(int i=0;i<=nticks;i++){
    float ty=oy+ph-(float)i/nticks*ph;
    float tx=ox+(float)i/nticks*pw;
    nvgBeginPath(vg); nvgMoveTo(vg,ox,ty); nvgLineTo(vg,ox+pw,ty);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[12]; snprintf(buf,12,"%.3g",ymin+yr*i/nticks);
    chart_label(vg,ox-3,ty,9,NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE,text_col,buf);
    snprintf(buf,12,"%.3g",xmin+xr*i/nticks);
    chart_label(vg,tx,oy+ph+12,9,NVG_ALIGN_CENTER|NVG_ALIGN_TOP,text_col,buf);
  }

  NVGcolor dot_col = u32c(cd->color, 0.85f);
  for(int i=0;i<n;i++){
    float px = ox + (float)((xs[i]-xmin)/xr)*pw;
    float py = oy + ph - (float)((ys[i]-ymin)/yr)*ph;
    nvgBeginPath(vg); nvgCircle(vg,px,py,5);
    nvgFillColor(vg,pal(0,0.8f)); nvgFill(vg);
    nvgBeginPath(vg); nvgCircle(vg,px,py,4);
    nvgFillColor(vg,dot_col); nvgFill(vg);
  }
  (void)dot_col;
}

// ─── GANTT CHART ─────────────────────────────────────────────────────────────
static void render_gantt(NVGcontext *vg, ChartData *cd, AslanRect r,
                         AslanTable *tbl, bool dark) {
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(55,57,70,255)    : nvgRGBA(225,225,232,255);

  // Find relevant columns automatically
  int name_col=-1, date_col=-1, dur_col=-1;
  if (tbl) {
    for(int c=0;c<(int)tbl->column_count;c++){
      AslanColumnType ct = aslan_table_column_type(tbl,c);
      if(name_col<0 && ct==ASLAN_COL_TEXT)     name_col=c;
      if(date_col<0 && ct==ASLAN_COL_DATE)     date_col=c;
      if(dur_col <0 && ct==ASLAN_COL_DURATION) dur_col =c;
      if(dur_col <0 && ct==ASLAN_COL_NUMBER)   dur_col =c;
    }
  }
  // Override with user selection
  if(cd->label_col_idx>=0) name_col=cd->label_col_idx;
  if(cd->value_col_idx>=0) date_col=cd->value_col_idx;
  if(cd->col_idx_2>=0)     dur_col =cd->col_idx_2;

  float lbl_w = 100.0f;
  float pl=lbl_w,pr=8,pt=28,pb=8;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0) return;

  int n=0;
  int64_t ts_ms[MAX_PTS]; int64_t dur_min[MAX_PTS]; char names[MAX_PTS][32];
  int64_t t_min=0, t_max=0;

  if (tbl && (int)aslan_table_view_count(tbl)>0) {
    int cnt=(int)aslan_table_view_count(tbl); if(cnt>MAX_PTS)cnt=MAX_PTS; n=cnt;
    for(int i=0;i<n;i++){
      uint64_t rid = aslan_table_view_row_id(tbl,i);
      ts_ms[i]  = (date_col>=0) ? aslan_table_get_date(tbl,date_col,rid)   : 0;
      dur_min[i]= (dur_col >=0) ? aslan_table_get_duration(tbl,dur_col,rid): 0;
      // Fallback: if dur is NUMBER column, treat as days
      if(dur_col>=0 && aslan_table_column_type(tbl,dur_col)==ASLAN_COL_NUMBER)
        dur_min[i] = (int64_t)(aslan_table_get_number(tbl,dur_col,rid)*1440.0+0.5);
      const char *nm = (name_col>=0) ? aslan_table_get_text(tbl,name_col,rid) : NULL;
      if(nm && nm[0]) strncpy(names[i],nm,31); else snprintf(names[i],32,"Task %d",i+1);
    }
    // Compute time range
    for(int i=0;i<n;i++){
      if(!ts_ms[i]) continue;
      int64_t end = ts_ms[i] + dur_min[i]*60000LL;
      if(!t_min || ts_ms[i]<t_min) t_min=ts_ms[i];
      if(!t_max || end>t_max)       t_max=end;
    }
  }

  // Sample data fallback
  if (n==0 || !t_min) {
    n=4;
    int64_t now = 1710000000000LL; // approx 2024-03-10
    ts_ms[0]=now;               dur_min[0]=2*1440;
    ts_ms[1]=now+2*86400000LL;  dur_min[1]=3*1440;
    ts_ms[2]=now+4*86400000LL;  dur_min[2]=2*1440;
    ts_ms[3]=now+3*86400000LL;  dur_min[3]=4*1440;
    strncpy(names[0],"Planning",31); strncpy(names[1],"Design",31);
    strncpy(names[2],"Build",31);    strncpy(names[3],"Review",31);
    t_min=now; t_max=now+7*86400000LL;
  }

  double t_range = (double)(t_max-t_min); if(t_range<=0) t_range=86400000;

  // Day headers
  int64_t start_day = t_min/86400000LL;
  int64_t end_day   = t_max/86400000LL+1;
  int ndays = (int)(end_day-start_day); if(ndays<1)ndays=1;
  float day_w = pw/(float)ndays;

  for(int d=0;d<ndays;d++){
    float dx = ox+d*day_w;
    // Alternating tint for weekends (simplified: every 5th-6th day)
    nvgBeginPath(vg); nvgRect(vg,dx,r.y,day_w,pt-2);
    nvgFillColor(vg, dark?nvgRGBA(42,44,52,255):nvgRGBA(240,241,245,255)); nvgFill(vg);
    // Day number label
    char dnum[4]; snprintf(dnum,4,"%d",(int)((start_day+d)%28+1)); // simplified day
    chart_label(vg,dx+day_w*0.5f,r.y+pt*0.5f,8,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,text_col,dnum);
    // Vertical grid line
    nvgBeginPath(vg); nvgMoveTo(vg,dx,r.y); nvgLineTo(vg,dx,r.y+r.h);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.5f); nvgStroke(vg);
  }

  // Today marker
  int64_t today_ms = 1710000000000LL; // approx current; ideally pass time
  if(today_ms>=t_min && today_ms<=t_max){
    float tx = ox+(float)((today_ms-t_min)/(double)t_range)*pw;
    nvgBeginPath(vg); nvgMoveTo(vg,tx,oy); nvgLineTo(vg,tx,oy+ph);
    nvgStrokeColor(vg,nvgRGBA(0,180,200,180)); nvgStrokeWidth(vg,1.5f); nvgStroke(vg);
  }

  float row_h = ph/(float)n;
  float bar_h = row_h*0.5f;
  for(int i=0;i<n;i++){
    float by = oy + i*row_h + (row_h-bar_h)*0.5f;
    // Row background
    if(i%2==0){
      nvgBeginPath(vg); nvgRect(vg,r.x,oy+i*row_h,r.w,row_h);
      nvgFillColor(vg,dark?nvgRGBA(38,40,50,120):nvgRGBA(248,249,252,120)); nvgFill(vg);
    }
    // Task label
    chart_label(vg,r.x+lbl_w-4,by+bar_h*0.5f,9,NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE,text_col,names[i]);
    // Bar
    if(ts_ms[i]>0 && dur_min[i]>0){
      float bx = ox+(float)((ts_ms[i]-t_min)/(double)t_range)*pw;
      float bw = (float)(dur_min[i]*60000LL/(double)t_range)*pw;
      if(bw<3)bw=3;
      nvgBeginPath(vg); nvgRoundedRect(vg,bx,by,bw,bar_h,3);
      nvgFillColor(vg,pal(i,0.9f)); nvgFill(vg);
      if(bw>30){
        char buf[32]; snprintf(buf,32,"%s",names[i]);
        nvgSave(vg);
        nvgScissor(vg,bx,by,bw,bar_h);
        chart_label(vg,bx+bw*0.5f,by+bar_h*0.5f,8,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,
                    nvgRGBA(255,255,255,230),buf);
        nvgRestore(vg);
      }
    }
  }
}

// ─── STACKED BAR CHART ───────────────────────────────────────────────────────
static void render_stacked_bar(NVGcontext *vg, ChartData *cd, AslanRect r,
                                AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)    : nvgRGBA(225,225,230,255);
  (void)cd;

  float pl=44,pr=8,pt=12,pb=36;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0||s.n==0) return;

  // 3 sub-series: A = main, B and C = sample complements (proportional for demo)
  static const double fb[] = {0.40,0.55,0.30,0.45,0.60,0.35};
  static const double fc[] = {0.30,0.25,0.50,0.20,0.35,0.45};
  double sa[MAX_PTS], sb[MAX_PTS], sc[MAX_PTS];
  double ymax = 0;
  for (int i=0;i<s.n;i++) {
    sa[i] = s.v[i];
    sb[i] = s.v[i] * fb[i%6];
    sc[i] = s.v[i] * fc[i%6];
    double sum = sa[i]+sb[i]+sc[i];
    if (sum > ymax) ymax = sum;
  }
  ymax = nice_ceil(ymax);

  int nticks = 5;
  for (int i=0;i<=nticks;i++) {
    float ty = oy + ph - (float)i/nticks*ph;
    nvgBeginPath(vg); nvgMoveTo(vg,ox,ty); nvgLineTo(vg,ox+pw,ty);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[16]; snprintf(buf,16,"%.4g",ymax*i/nticks);
    chart_label(vg,ox-4,ty,9,NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE,text_col,buf);
  }

  float slot = pw/(float)s.n;
  float bw   = slot * 0.65f;
  float boff = slot * 0.175f;
  const double *segs[] = {sa,sb,sc};
  for (int i=0;i<s.n;i++) {
    float bx = ox + i*slot + boff;
    float cur_y = oy + ph;
    for (int si=0;si<3;si++) {
      float bh = (float)(segs[si][i]/ymax)*ph;
      if(bh<1) bh=1;
      cur_y -= bh;
      nvgBeginPath(vg);
      nvgRect(vg,bx,cur_y,bw,bh);
      nvgFillColor(vg,pal(si,1.0f)); nvgFill(vg);
    }
    chart_label(vg,bx+bw*0.5f,oy+ph+14,9,NVG_ALIGN_CENTER|NVG_ALIGN_TOP,text_col,s.lbl[i]);
  }
}

// ─── WATERFALL CHART ─────────────────────────────────────────────────────────
static void render_waterfall(NVGcontext *vg, ChartData *cd, AslanRect r,
                              AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  NVGcolor text_col  = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col  = dark ? nvgRGBA(60,62,75,255)    : nvgRGBA(225,225,230,255);
  NVGcolor pos_col   = nvgRGBA(76,175,80,255);
  NVGcolor neg_col   = nvgRGBA(229,57,53,255);
  NVGcolor total_col = dark ? nvgRGBA(100,130,220,255) : nvgRGBA(60,100,200,255);
  (void)cd;

  float pl=44,pr=8,pt=12,pb=36;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0||s.n==0) return;

  double starts[MAX_PTS], running = 0;
  double top_v = 0, bot_v = 0;
  for (int i=0;i<s.n;i++) {
    starts[i] = running;
    running += s.v[i];
    double hi = starts[i] + (s.v[i]>0 ? s.v[i] : 0);
    double lo = starts[i] + (s.v[i]<0 ? s.v[i] : 0);
    if (hi > top_v) top_v = hi;
    if (lo < bot_v) bot_v = lo;
  }
  if (running > top_v) top_v = running;
  double ymax   = nice_ceil(top_v > 0 ? top_v : 10);
  double ymin   = bot_v < 0 ? -nice_ceil(-bot_v) : 0;
  double yrange = ymax - ymin; if(yrange<=0) yrange=1;

  int nticks = 5;
  for (int i=0;i<=nticks;i++) {
    float ty = oy + ph - (float)i/nticks*ph;
    nvgBeginPath(vg); nvgMoveTo(vg,ox,ty); nvgLineTo(vg,ox+pw,ty);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    char buf[16]; snprintf(buf,16,"%.4g",ymin+yrange*i/nticks);
    chart_label(vg,ox-4,ty,9,NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE,text_col,buf);
  }

  int n_bars = s.n + 1;
  float slot = pw/(float)n_bars;
  float bw   = slot * 0.65f;
  float boff = slot * 0.175f;
  float zero_y = oy + ph - (float)((-ymin)/yrange)*ph;

  for (int i=0;i<n_bars;i++) {
    bool is_total = (i == s.n);
    double sv   = is_total ? 0.0     : starts[i];
    double val  = is_total ? running : s.v[i];
    float bx    = ox + i*slot + boff;
    float top_y = oy + ph - (float)((sv + val - ymin)/yrange)*ph;
    float bot_y = is_total ? zero_y : oy + ph - (float)((sv - ymin)/yrange)*ph;
    if (is_total && running < 0) { top_y = zero_y; bot_y = oy + ph - (float)((running-ymin)/yrange)*ph; }
    float bh = fabsf(bot_y - top_y); if(bh<1) bh=1;
    float by = fminf(top_y, bot_y);

    NVGcolor col = is_total ? total_col : (val >= 0 ? pos_col : neg_col);
    nvgBeginPath(vg); nvgRoundedRect(vg,bx,by,bw,bh,3);
    nvgFillColor(vg,col); nvgFill(vg);

    // Connector line to next bar
    if (!is_total && i < s.n-1) {
      float conn_y = (val >= 0) ? top_y : bot_y;
      float nx = bx + slot;
      nvgBeginPath(vg); nvgMoveTo(vg,bx+bw,conn_y); nvgLineTo(vg,nx,conn_y);
      nvgStrokeColor(vg,dark?nvgRGBA(130,132,150,180):nvgRGBA(160,162,180,180));
      nvgStrokeWidth(vg,1.0f); nvgStroke(vg);
    }

    const char *lbl = is_total ? "Total" : s.lbl[i];
    chart_label(vg,bx+bw*0.5f,oy+ph+14,9,NVG_ALIGN_CENTER|NVG_ALIGN_TOP,text_col,lbl);
    if (is_total) {
      char buf[16]; snprintf(buf,16,"%.4g",running);
      chart_label(vg,bx+bw*0.5f,by-3,8,NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM,text_col,buf);
    }
  }
}

// ─── RADAR / SPIDER CHART ────────────────────────────────────────────────────
static void render_radar(NVGcontext *vg, ChartData *cd, AslanRect r,
                          AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  if (s.n < 3) return;
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  NVGcolor grid_col = dark ? nvgRGBA(60,62,75,255)    : nvgRGBA(225,225,230,255);
  (void)cd;

  float cx  = r.x + r.w * 0.5f;
  float cy  = r.y + r.h * 0.52f;
  float rad = fminf(r.w * 0.38f, r.h * 0.38f);
  if (rad < 12) return;

  int n = s.n;
  double vmax = nice_ceil(s.vmax > 0 ? s.vmax : 1);
  float astep  = NVG_PI * 2.0f / (float)n;
  float astart = -NVG_PI * 0.5f;

  // Grid rings
  int nrings = 4;
  for (int ri=1;ri<=nrings;ri++) {
    float rr = rad * (float)ri / nrings;
    nvgBeginPath(vg);
    for (int i=0;i<n;i++) {
      float a  = astart + i*astep;
      float px = cx + cosf(a)*rr, py = cy + sinf(a)*rr;
      if(i==0) nvgMoveTo(vg,px,py); else nvgLineTo(vg,px,py);
    }
    nvgClosePath(vg);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    if (ri < nrings) {
      char buf[12]; snprintf(buf,12,"%.4g",vmax*ri/nrings);
      float lx = cx + cosf(astart) * rr + 4;
      float ly = cy + sinf(astart) * rr;
      chart_label(vg,lx,ly,7,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE,
                  dark?nvgRGBA(120,122,138,200):nvgRGBA(160,162,178,200),buf);
    }
  }

  // Spoke axes
  for (int i=0;i<n;i++) {
    float a  = astart + i*astep;
    float px = cx + cosf(a)*rad, py = cy + sinf(a)*rad;
    nvgBeginPath(vg); nvgMoveTo(vg,cx,cy); nvgLineTo(vg,px,py);
    nvgStrokeColor(vg,grid_col); nvgStrokeWidth(vg,0.8f); nvgStroke(vg);
    float lx = cx + cosf(a)*(rad+16), ly = cy + sinf(a)*(rad+16);
    chart_label(vg,lx,ly,9,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,text_col,s.lbl[i]);
  }

  // Filled data polygon
  nvgBeginPath(vg);
  for (int i=0;i<n;i++) {
    float a  = astart + i*astep;
    float rr = rad * (float)(s.v[i]/vmax);
    float px = cx + cosf(a)*rr, py = cy + sinf(a)*rr;
    if(i==0) nvgMoveTo(vg,px,py); else nvgLineTo(vg,px,py);
  }
  nvgClosePath(vg);
  nvgFillColor(vg,pal(0,0.25f)); nvgFill(vg);
  nvgStrokeColor(vg,pal(0,1.0f)); nvgStrokeWidth(vg,2.0f); nvgStroke(vg);

  // Data point dots
  for (int i=0;i<n;i++) {
    float a  = astart + i*astep;
    float rr = rad * (float)(s.v[i]/vmax);
    float px = cx + cosf(a)*rr, py = cy + sinf(a)*rr;
    nvgBeginPath(vg); nvgCircle(vg,px,py,3.5f);
    nvgFillColor(vg,pal(0,1.0f)); nvgFill(vg);
    nvgBeginPath(vg); nvgCircle(vg,px,py,2.0f);
    nvgFillColor(vg,nvgRGBA(255,255,255,220)); nvgFill(vg);
  }
}

// ─── FUNNEL CHART ────────────────────────────────────────────────────────────
static void render_funnel(NVGcontext *vg, ChartData *cd, AslanRect r,
                           AslanTable *tbl, bool dark) {
  Series s; series_fill(&s, tbl, cd->label_col_idx, cd->value_col_idx);
  if (s.n == 0) return;
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(80,82,95,255);
  (void)cd;

  float pt=8,pb=8,pl=8,pr=8;
  float pw=r.w-pl-pr, ph=r.h-pt-pb;
  float ox=r.x+pl, oy=r.y+pt;
  if(pw<=0||ph<=0) return;

  int n = s.n;
  float row_h = ph / (float)n;
  float gap   = 2.5f;
  float cx    = ox + pw * 0.5f;
  double vmax = s.vmax > 0 ? s.vmax : 1;

  for (int i=0;i<n;i++) {
    float hw_top = pw * 0.5f * (float)(s.v[i] / vmax);
    float hw_bot = (i < n-1)
      ? pw * 0.5f * (float)(s.v[i+1] / vmax)
      : hw_top * 0.65f;

    float ty = oy + i * row_h;
    float by = ty + row_h - gap;

    // Trapezoid fill
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - hw_top, ty);
    nvgLineTo(vg, cx + hw_top, ty);
    nvgLineTo(vg, cx + hw_bot, by);
    nvgLineTo(vg, cx - hw_bot, by);
    nvgClosePath(vg);
    nvgFillColor(vg, pal(i, 1.0f)); nvgFill(vg);

    // Label (inside if wide enough, outside otherwise)
    float mid_y = (ty + by) * 0.5f;
    float seg_w = hw_top * 2.0f;
    if (seg_w > 50) {
      chart_label(vg,cx,mid_y,10,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,
                  nvgRGBA(255,255,255,230),s.lbl[i]);
      if (cd->show_values) {
        char buf[16]; snprintf(buf,16,"%.4g",s.v[i]);
        chart_label(vg,cx,mid_y+13,9,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,
                    nvgRGBA(255,255,255,180),buf);
      }
    } else {
      chart_label(vg,cx+hw_top+6,mid_y,10,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE,text_col,s.lbl[i]);
    }
  }
}

// ─── KANBAN BOARD ────────────────────────────────────────────────────────────
// Linked to: label_col = card title (TEXT), value_col = status (SELECT)
// Columns = SELECT options; cards grouped by option value
static void render_kanban(NVGcontext *vg, ChartData *cd, AslanRect r,
                           AslanTable *tbl, bool dark) {
  NVGcolor text_col = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(55,57,72,255);
  NVGcolor col_bg   = dark ? nvgRGBA(38,40,50,255)   : nvgRGBA(242,243,247,255);
  NVGcolor card_bg  = dark ? nvgRGBA(50,52,64,255)   : nvgRGBA(255,255,255,255);
  NVGcolor card_brd = dark ? nvgRGBA(65,67,82,255)   : nvgRGBA(218,220,230,255);

  // ── Detect columns ──────────────────────────────────────────────────────────
  int title_col = cd->label_col_idx;
  int sel_col   = cd->value_col_idx;
  if (tbl) {
    for (int c = 0; c < (int)tbl->column_count; c++) {
      if (title_col < 0 && tbl->columns[c].type == ASLAN_COL_TEXT)   title_col = c;
      if (sel_col   < 0 && tbl->columns[c].type == ASLAN_COL_SELECT) sel_col   = c;
    }
  }

  // ── Column definitions from SELECT options ──────────────────────────────────
  int     n_cols = 3;
  char    col_names[8][64];
  NVGcolor col_accents[8];

  bool has_real_data = tbl && sel_col >= 0
                       && tbl->columns[sel_col].option_count > 0;

  // ── Missing columns → placeholder ───────────────────────────────────────────
  if (!has_real_data) {
    chart_placeholder(vg, r, dark,
                      "Link a table with:",
                      "TEXT column (title)  +  SELECT column (status)");
    return;
  }

  n_cols = tbl->columns[sel_col].option_count;
  if (n_cols > 6) n_cols = 6;
  for (int c = 0; c < n_cols; c++) {
    const char *lbl = aslan_table_get_option_label(tbl, sel_col, c);
    strncpy(col_names[c], lbl && lbl[0] ? lbl : "—", 63);
    uint32_t oc = aslan_table_get_option_color(tbl, sel_col, c);
    col_accents[c] = oc
      ? nvgRGBA((oc>>24)&0xFF,(oc>>16)&0xFF,(oc>>8)&0xFF,255)
      : pal(c, 1.0f);
  }

  // ── Layout ──────────────────────────────────────────────────────────────────
  float pt=8,pb=8,pl=8,pr=8,gap=6;
  float pw = r.w-pl-pr, ph = r.h-pt-pb;
  float cw = (pw - gap*(n_cols-1)) / (float)n_cols;
  float ox = r.x+pl, oy = r.y+pt;
  float hdr_h=30.f, card_h=28.f, card_gap=5.f;
  (void)pr;

  for (int c = 0; c < n_cols; c++) {
    float cx = ox + c*(cw+gap);

    nvgBeginPath(vg); nvgRoundedRect(vg,cx,oy,cw,ph,6);
    nvgFillColor(vg,col_bg); nvgFill(vg);
    nvgBeginPath(vg); nvgRoundedRect(vg,cx,oy,cw,3,2);
    nvgFillColor(vg,col_accents[c]); nvgFill(vg);

    // ── Gather cards for this column ─────────────────────────────────────────
    int  card_count = 0;
    char card_texts[8][64];
    if (has_real_data && title_col >= 0) {
      int nrows = (int)aslan_table_view_count(tbl);
      for (int ri = 0; ri < nrows && card_count < 8; ri++) {
        uint64_t rid = aslan_table_view_row_id(tbl, ri);
        if (aslan_table_get_select(tbl, sel_col, rid) == c) {
          const char *t = aslan_table_get_text(tbl, title_col, rid);
          strncpy(card_texts[card_count++], t && t[0] ? t : "—", 63);
        }
      }
    } else {
      static const char *dc[3][4] = {
        {"Design mockup","Write tests","Update docs",NULL},
        {"Build API","Review PR",NULL,NULL},
        {"Setup CI","DB schema","Auth",NULL},
      };
      static const int dcnt[] = {3,2,3};
      if (c < 3) {
        card_count = dcnt[c];
        for (int i=0;i<card_count;i++) strncpy(card_texts[i],dc[c][i],63);
      }
    }

    // Column title + badge
    chart_label(vg,cx+10,oy+hdr_h*0.5f+2,11,
                NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE,text_col,col_names[c]);
    char cnt[4]; snprintf(cnt,4,"%d",card_count);
    float bx2=cx+cw-24, by2=oy+hdr_h*0.5f+2;
    nvgBeginPath(vg); nvgRoundedRect(vg,bx2-2,by2-9,18,17,8);
    nvgFillColor(vg,dark?nvgRGBA(60,62,76,255):nvgRGBA(225,227,235,255)); nvgFill(vg);
    chart_label(vg,bx2+7,by2,9,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE,text_col,cnt);

    float cy2 = oy+hdr_h+4;
    for (int i=0;i<card_count;i++) {
      if (cy2+card_h > oy+ph-4) break;
      nvgBeginPath(vg); nvgRoundedRect(vg,cx+4,cy2,cw-8,card_h,4);
      nvgFillColor(vg,card_bg); nvgFill(vg);
      nvgBeginPath(vg); nvgRoundedRect(vg,cx+4,cy2,cw-8,card_h,4);
      nvgStrokeColor(vg,card_brd); nvgStrokeWidth(vg,1.f); nvgStroke(vg);
      nvgBeginPath(vg); nvgRoundedRect(vg,cx+4,cy2+3,3,card_h-6,2);
      nvgFillColor(vg,col_accents[c]); nvgFill(vg);
      chart_label(vg,cx+13,cy2+card_h*0.5f,10,
                  NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE,text_col,card_texts[i]);
      cy2 += card_h+card_gap;
    }
  }
}

// ─── TO-DO LIST ───────────────────────────────────────────────────────────────
// Linked to: label_col = task name (TEXT), value_col = done state (CHECKBOX)
static void render_todo(NVGcontext *vg, ChartData *cd, AslanRect r,
                         AslanTable *tbl, bool dark) {
  NVGcolor text_col  = dark ? nvgRGBA(190,192,205,255) : nvgRGBA(55,57,72,255);
  NVGcolor done_col  = dark ? nvgRGBA(110,112,128,255) : nvgRGBA(170,172,190,255);
  NVGcolor check_col = nvgRGBA(76,175,80,255);
  NVGcolor box_col   = dark ? nvgRGBA(75,77,92,255)   : nvgRGBA(198,200,214,255);

  // ── Detect columns ──────────────────────────────────────────────────────────
  int name_col  = cd->label_col_idx;
  int check_idx = cd->value_col_idx;
  if (tbl) {
    for (int c = 0; c < (int)tbl->column_count; c++) {
      if (name_col  < 0 && tbl->columns[c].type == ASLAN_COL_TEXT)     name_col  = c;
      if (check_idx < 0 && tbl->columns[c].type == ASLAN_COL_CHECKBOX) check_idx = c;
    }
  }

  // ── Load tasks ──────────────────────────────────────────────────────────────
  int  n = 0;
  char task_names[MAX_PTS][32];
  bool task_done_arr[MAX_PTS];

  // ── Missing columns → placeholder ───────────────────────────────────────────
  if (!tbl || name_col < 0) {
    chart_placeholder(vg, r, dark,
                      "Link a table with:",
                      "TEXT column (task name)  +  CHECKBOX column (done)");
    return;
  }

  int cnt = (int)aslan_table_view_count(tbl);
  if (cnt > 16) cnt = 16;
  n = 0;
  for (int i = 0; i < cnt; i++) {
    uint64_t rid = aslan_table_view_row_id(tbl, i);
    const char *nm = aslan_table_get_text(tbl, name_col, rid);
    if (!nm || !nm[0]) continue;
    strncpy(task_names[n], nm, 31);
    task_done_arr[n] = check_idx >= 0
                       ? aslan_table_get_checkbox(tbl, check_idx, rid)
                       : false;
    n++;
  }
  if (n == 0) {
    chart_placeholder(vg, r, dark,
                      "No tasks found.",
                      "Add rows to your table to see them here.");
    return;
  }

  float pt=8, pb=22, pl=12, pr=12;
  float ox = r.x + pl, oy = r.y + pt;
  float avail_h = r.h - pt - pb;
  float row_h   = avail_h / (float)(n > 0 ? n : 1);
  if (row_h > 34) row_h = 34;
  if (row_h < 18) row_h = 18;
  int max_n = (int)(avail_h / row_h);
  if (n > max_n) n = max_n;

  for (int i = 0; i < n; i++) {
    bool  done   = task_done_arr[i];
    float ty     = oy + i * row_h;
    float row_cy = ty + row_h * 0.5f;

    if (i > 0) {
      nvgBeginPath(vg);
      nvgMoveTo(vg, ox+22, ty); nvgLineTo(vg, r.x+r.w-pr, ty);
      nvgStrokeColor(vg, dark ? nvgRGBA(48,50,62,200) : nvgRGBA(232,234,244,200));
      nvgStrokeWidth(vg, 0.8f); nvgStroke(vg);
    }

    float cbx = ox+9, cby = row_cy, cbr = 8.5f;
    nvgBeginPath(vg); nvgCircle(vg, cbx, cby, cbr);
    if (done) {
      nvgFillColor(vg, check_col); nvgFill(vg);
      nvgBeginPath(vg);
      nvgMoveTo(vg, cbx-4.0f, cby+0.5f);
      nvgLineTo(vg, cbx-1.0f, cby+3.5f);
      nvgLineTo(vg, cbx+4.5f, cby-3.5f);
      nvgStrokeColor(vg, nvgRGBA(255,255,255,240));
      nvgStrokeWidth(vg, 1.8f);
      nvgLineCap(vg, NVG_ROUND); nvgLineJoin(vg, NVG_ROUND);
      nvgStroke(vg);
    } else {
      nvgFillColor(vg, dark ? nvgRGBA(38,40,50,255) : nvgRGBA(255,255,255,255));
      nvgFill(vg);
      nvgStrokeColor(vg, box_col); nvgStrokeWidth(vg, 1.5f); nvgStroke(vg);
    }

    NVGcolor lbl_col = done ? done_col : text_col;
    chart_label(vg, ox+24, row_cy, 12, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE,
                lbl_col, task_names[i]);

    if (done) {
      float bounds[4];
      nvgFontSize(vg, 12.0f); nvgFontFace(vg, "Inter");
      nvgTextBounds(vg, ox+24, row_cy, task_names[i], NULL, bounds);
      nvgBeginPath(vg);
      nvgMoveTo(vg, bounds[0], row_cy); nvgLineTo(vg, bounds[2], row_cy);
      nvgStrokeColor(vg, done_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
    }
  }

  if (n > 0) {
    int dc = 0;
    for (int i = 0; i < n; i++) if (task_done_arr[i]) dc++;
    float py  = r.y + r.h - 12;
    float pxb = r.x + pl;
    float pw2 = r.w - pl - pr;
    nvgBeginPath(vg); nvgRoundedRect(vg, pxb, py, pw2, 4, 2);
    nvgFillColor(vg, dark ? nvgRGBA(50,52,64,255) : nvgRGBA(218,220,230,255));
    nvgFill(vg);
    if (dc > 0) {
      nvgBeginPath(vg); nvgRoundedRect(vg, pxb, py, pw2*(float)dc/(float)n, 4, 2);
      nvgFillColor(vg, check_col); nvgFill(vg);
    }
    char buf[24]; snprintf(buf, 24, "%d / %d completed", dc, n);
    chart_label(vg, pxb, py-5, 9, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM,
                dark ? nvgRGBA(110,112,128,255) : nvgRGBA(150,152,170,255), buf);
  }
}

// ─── Main dispatch ────────────────────────────────────────────────────────────
void ui_render_chart_impl(UIHost *ui, ChartData *cd, AslanRect bounds, Page *page) {
  if (!ui || !cd) return;
  NVGcontext *vg = ui->vg;
  if (!vg) return;
  bool dark = ui->dark_mode != 0;

  AslanTable *tbl = find_table(page, cd->source_table_id);

  // Card background
  nvgBeginPath(vg); nvgRoundedRect(vg, bounds.x, bounds.y, bounds.w, bounds.h, 6);
  nvgFillColor(vg, dark ? nvgRGBA(32,34,42,255) : nvgRGBA(255,255,255,255));
  nvgFill(vg);
  nvgBeginPath(vg); nvgRoundedRect(vg, bounds.x, bounds.y, bounds.w, bounds.h, 6);
  nvgStrokeColor(vg, dark ? nvgRGBA(55,57,68,255) : nvgRGBA(220,220,228,255));
  nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);

  // Inner clip
  nvgSave(vg);
  nvgScissor(vg, bounds.x+1, bounds.y+1, bounds.w-2, bounds.h-2);

  switch (cd->type) {
    case CHART_BAR:          render_bar        (vg, cd, bounds, tbl, dark); break;
    case CHART_LINE:         render_line       (vg, cd, bounds, tbl, dark, false); break;
    case CHART_AREA:         render_line       (vg, cd, bounds, tbl, dark, true);  break;
    case CHART_PIE:          render_pie        (vg, cd, bounds, tbl, dark, false); break;
    case CHART_DONUT:        render_pie        (vg, cd, bounds, tbl, dark, true);  break;
    case CHART_HBAR:         render_hbar       (vg, cd, bounds, tbl, dark); break;
    case CHART_SCATTER:      render_scatter    (vg, cd, bounds, tbl, dark); break;
    case CHART_GANTT:        render_gantt      (vg, cd, bounds, tbl, dark); break;
    case CHART_STACKED_BAR:  render_stacked_bar(vg, cd, bounds, tbl, dark); break;
    case CHART_WATERFALL:    render_waterfall  (vg, cd, bounds, tbl, dark); break;
    case CHART_RADAR:        render_radar      (vg, cd, bounds, tbl, dark); break;
    case CHART_FUNNEL:       render_funnel     (vg, cd, bounds, tbl, dark); break;
    case CHART_KANBAN:       render_kanban     (vg, cd, bounds, tbl, dark); break;
    case CHART_TODO:         render_todo       (vg, cd, bounds, tbl, dark); break;
    default:                 render_bar        (vg, cd, bounds, tbl, dark); break;
  }

  nvgRestore(vg);
}

void ui_render_chart_node(UIHost *ui, Node *node, AslanRect bounds, Page *page) {
  if (!node || !node->chart_data) return;
  ui_render_chart_impl(ui, node->chart_data, bounds, page);
}

void ui_render_chart_node_config_bar(UIHost *ui, Engine *engine, Node *node,
                                     AslanRect bounds, Page *page) {
  (void)ui; (void)engine; (void)node; (void)bounds; (void)page;
}
