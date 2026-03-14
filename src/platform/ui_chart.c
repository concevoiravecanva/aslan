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
    case CHART_BAR:     render_bar    (vg, cd, bounds, tbl, dark); break;
    case CHART_LINE:    render_line   (vg, cd, bounds, tbl, dark, false); break;
    case CHART_AREA:    render_line   (vg, cd, bounds, tbl, dark, true);  break;
    case CHART_PIE:     render_pie    (vg, cd, bounds, tbl, dark, false); break;
    case CHART_DONUT:   render_pie    (vg, cd, bounds, tbl, dark, true);  break;
    case CHART_HBAR:    render_hbar   (vg, cd, bounds, tbl, dark); break;
    case CHART_SCATTER: render_scatter(vg, cd, bounds, tbl, dark); break;
    case CHART_GANTT:   render_gantt  (vg, cd, bounds, tbl, dark); break;
    default:            render_bar    (vg, cd, bounds, tbl, dark); break;
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
