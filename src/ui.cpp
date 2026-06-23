// ─────────────────────────────────────────────────────────────────────────────
//  STRUMENTO — La Marzocco companion UI
//
//  Idle   : ivory enamel panel · single chronograph dial · brass hairlines
//  Brewing: warm-black watch face · sweeping LM-red second hand · big numerals
//
//  Design notes:
//    • One accent colour (LM_RED). Everything else is ivory/ink/brass.
//    • Letterspaced small-caps for every label — drawn char-by-char.
//    • No rounded rects on the light theme; only hairlines and circles.
//    • Dial bezel = four concentric rings (ink/brass/brass-hi/paper) for the
//      lathed-metal look of the real brew gauge.
// ─────────────────────────────────────────────────────────────────────────────
#include "ui.h"
#include "config.h"
#include "lm_cloud.h"
#include "storage.h"
#include "keyboard.h"
#include "fonts.h"
#include "version.h"
#include <time.h>
#include <sys/time.h>

using namespace cfg;
namespace ui {

// ── theme ── dark mode reuses the brewing palette everywhere ────────────────
static bool g_dark = false;
static constexpr uint16_t NIGHT_LO = 0x2104;       // hairlines/separators on dark
static inline uint16_t BG()    { return g_dark?NIGHT   :IVORY;    }
static inline uint16_t BG_LO() { return g_dark?NIGHT_LO:IVORY_LO; }
static inline uint16_t FG()    { return g_dark?LUME    :INK;      }
static inline uint16_t FACE()  { return g_dark?NIGHT   :PAPER;    }
void setDark(bool d){ g_dark=d; }

// ── temperature unit ────────────────────────────────────────────────────────
static inline float c2disp(float c){ return settings.fahrenheit?c*1.8f+32.f:c; }
static inline float disp2c(float d){ return settings.fahrenheit?(d-32.f)/1.8f:d; }
static inline float tempStep()     { return settings.fahrenheit?1.f:0.5f; }

// VLW handles parsed once at boot — switching is then a pointer swap.
struct AAFont {
  lgfx::PointerWrapper data;
  lgfx::VLWfont        vlw;
  void load(const uint8_t* p, size_t n){ data.set(p,n); vlw.loadFont(&data); }
};
static AAFont AA_WORDMARK, AA_SERIF_SM, AA_LABEL, AA_LABEL_SM, AA_NUM_LG, AA_NUM_MD;
#define F_WORDMARK  AA_WORDMARK.vlw
#define F_SERIF_SM  AA_SERIF_SM.vlw
#define F_LABEL     AA_LABEL.vlw
#define F_LABEL_SM  AA_LABEL_SM.vlw
#define F_NUM_LG    AA_NUM_LG.vlw
#define F_NUM_MD    AA_NUM_MD.vlw

enum class Screen { Home, Brewing, Controls, Settings, Stats };
static Screen   g_scr = Screen::Home;
static bool     g_dirty = true;
static M5Canvas g_can(&M5.Display);
static uint32_t g_lastFrame = 0;
static float    g_shotHold = 0;       // seconds shown briefly after brew ends
static uint32_t g_shotHoldUntil = 0;  // millis() deadline for the post-shot freeze
static constexpr uint32_t SHOT_HOLD_MS = 4000;
static float    g_dbgBrewSec = -1;    // <0 = live; ≥0 = forced brewing seconds
static bool     g_dbgOn      = false; // force "machine on" view on Home
static int      g_ctrlScroll = 0;     // px offset into Controls content
static int      g_ctrlMax    = 0;
static bool     g_dragging   = false;
static int      g_dragStartY = 0, g_dragStartScroll = 0;
static int      g_dragStartX = 0;
static int      g_homePage   = 0;       // 0=dial 1=tally 2=clock
static constexpr int HOME_PAGES = 3;
static uint32_t g_vibOffAt   = 0;       // non-blocking haptic deadline

struct Btn { int x,y,w,h; std::function<void()> tap; };
static std::vector<Btn> g_btns;

// ── helpers ──────────────────────────────────────────────────────────────────
static int64_t epochMs() {
  struct timeval tv; gettimeofday(&tv,nullptr);
  return (int64_t)tv.tv_sec*1000 + tv.tv_usec/1000;
}
static String fmtClock() {
  time_t now = time(nullptr); struct tm t; localtime_r(&now,&t);
  char b[8]; snprintf(b,sizeof b,"%02d:%02d",t.tm_hour,t.tm_min); return b;
}
static String ago(int64_t atMs) {
  if (!atMs) return "—";
  int64_t s = (epochMs()-atMs)/1000;
  if (s<90)     return String((int)s)+"s";
  if (s<5400)   return String((int)(s/60))+"m";
  if (s<172800) return String((int)(s/3600))+"h";
  return String((int)(s/86400))+"d";
}
static String hhmm(int min){ char b[8]; snprintf(b,sizeof b,"%02d:%02d",min/60,min%60); return b; }

// Letterspaced small-caps. M5GFX has no tracking, so draw glyph-by-glyph.
static int tracked(int x, int y, const char* s, int track,
                   uint16_t col, const lgfx::IFont* f, lgfx::textdatum_t d=top_left) {
  g_can.setFont(f); g_can.setTextColor(col); g_can.setTextDatum(top_left);
  char ch[2]={0,0};
  int total=0; for(const char*p=s;*p;++p){ ch[0]=*p; total += g_can.textWidth(ch)+track; }
  total -= track;
  int cx = x;
  if (d==top_center||d==middle_center||d==bottom_center) cx -= total/2;
  if (d==top_right ||d==middle_right ||d==bottom_right ) cx -= total;
  int cy = y;
  if (d==middle_left||d==middle_center||d==middle_right) cy -= g_can.fontHeight()/2;
  for(const char*p=s;*p;++p){
    ch[0]=*p;
    g_can.drawString(ch,cx,cy);
    cx += g_can.textWidth(ch) + track;
  }
  return total;
}

// Subtle corner vignette.
static void vignette() {
  if (g_dark) return;                              // night needs no vignette
  for (int i=0;i<3;++i){
    int r=28-i*10;
    g_can.fillRect(0,0,r,r,IVORY_LO); g_can.fillRect(W-r,0,r,r,IVORY_LO);
    g_can.fillRect(0,H-r,r,r,IVORY_LO); g_can.fillRect(W-r,H-r,r,r,IVORY_LO);
  }
  g_can.fillCircle(28,28,28,IVORY);   g_can.fillCircle(W-28,28,28,IVORY);
  g_can.fillCircle(28,H-28,28,IVORY); g_can.fillCircle(W-28,H-28,28,IVORY);
}

// AA ring annulus (full circle).
static void ring(int cx,int cy,int rOuter,int rInner,uint16_t col,uint16_t bg){
  g_can.fillSmoothCircle(cx,cy,rOuter,col);
  g_can.fillSmoothCircle(cx,cy,rInner,bg);
}

// AA arc segment via short wide-lines along the curve.
static void smoothArc(int cx,int cy,float rad,float halfW,float a0,float a1,uint16_t col){
  if (a1<a0) std::swap(a0,a1);
  float step = 3.f;                                       // 3° segments
  float px=cx+cosf(a0*DEG_TO_RAD)*rad, py=cy+sinf(a0*DEG_TO_RAD)*rad;
  for(float a=a0+step; a<=a1+0.01f; a+=step){
    float aa=min(a,a1);
    float x=cx+cosf(aa*DEG_TO_RAD)*rad, y=cy+sinf(aa*DEG_TO_RAD)*rad;
    g_can.drawWideLine(px,py,x,y,halfW,col);
    px=x; py=y;
  }
}

// Chronograph bezel: concentric AA rings.
static void bezel(int cx,int cy,int r,uint16_t face) {
  g_can.fillSmoothCircle(cx,cy,r+6,INK);
  g_can.fillSmoothCircle(cx,cy,r+5,BRASS);
  g_can.fillSmoothCircle(cx,cy,r+2,BRASS_HI);
  g_can.fillSmoothCircle(cx,cy,r,  face);
}

// Tick ring: n marks over a sweep, every `major`-th longer.
static void ticks(int cx,int cy,int r,int n,int major,float a0,float sweep,
                  uint16_t col,uint16_t colMajor) {
  for(int i=0;i<=n;++i){
    float a=(a0 + sweep*i/n)*DEG_TO_RAD, c=cosf(a), s=sinf(a);
    bool  m=(i%major==0);
    int   l=m?10:5;
    g_can.drawWideLine(cx+c*(r-2), cy+s*(r-2),
                       cx+c*(r-2-l), cy+s*(r-2-l),
                       m?1.0f:0.5f, m?colMajor:col);
  }
}

// Watch hand: AA tapered wedge + counterweight tail.
static void hand(int cx,int cy,int len,float deg,uint16_t col) {
  float a=deg*DEG_TO_RAD, c=cosf(a), s=sinf(a);
  g_can.drawWedgeLine(cx,cy, cx+c*len,cy+s*len, 2.0f,0.5f, col);   // blade
  g_can.drawWedgeLine(cx,cy, cx-c*14, cy-s*14,  1.0f,2.5f, col);   // tail
  g_can.drawSpot(cx,cy,4,INK);
  g_can.drawSpot(cx,cy,2,BRASS_HI);
}

// Horizontal hairline with optional centred caption.
static void hairline(int y, const char* cap=nullptr) {
  g_can.drawFastHLine(12,y,W-24,BRASS);
  if (cap) {
    int w = tracked(W/2, y-6, cap, 3, INK_40, &F_LABEL, top_center);
    g_can.fillRect(W/2-w/2-8, y-1, w+16, 3, BG());    // erase line under caption
    tracked(W/2, y-6, cap, 3, INK_40, &F_LABEL, top_center);
  }
}

static const char* boilerWord(lmcloud::BoilerStatus b){
  using B=lmcloud::BoilerStatus;
  switch(b){case B::Ready:return "READY";case B::HeatingUp:return "HEATING";
    case B::NoWater:return "NO WATER";case B::Off:return "OFF";
    case B::StandBy:return "STANDBY";default:return "—";}
}

// ── header ───────────────────────────────────────────────────────────────────
static void header(const lmcloud::State& s, bool forceDark=false) {
  bool dark = forceDark || g_dark;
  uint16_t bg = dark?NIGHT:IVORY, fg = dark?LUME:INK_40;
  // hairline + wordmark
  g_can.drawFastHLine(12,20,W-24, dark?INK_40:BRASS);
  // wordmark in serif italic — closest to the etched logo on the grouphead
  g_can.setFont(&F_WORDMARK);
  g_can.setTextColor(dark?LUME:INK); g_can.setTextDatum(middle_center);
  String wm = s.modelName.isEmpty() ? "strumento" : s.modelName;
  wm.toLowerCase();
  int ww = g_can.textWidth(wm);
  g_can.fillRect(W/2-ww/2-8,10,ww+16,20,bg);
  g_can.drawString(wm, W/2, 19);
  // status dot · clock — clear hairline behind both
  uint16_t dot = (s.net==lmcloud::Net::WsLive)?BRASS_HI:
                 (s.net>=lmcloud::Net::AuthOk)?INK_40:LM_RED;
  g_can.fillRect(10,16,16,8,bg);
  g_can.drawSpot(18,19,2,dot);
  g_can.setFont(&F_LABEL_SM); g_can.setTextColor(fg);
  g_can.setTextDatum(middle_right);
  String ck=fmtClock(); int cw=g_can.textWidth(ck);
  g_can.fillRect(W-14-cw-6,12,cw+12,16,bg);
  g_can.drawString(ck, W-14, 19);
}

// ── piano-key bottom bar ─────────────────────────────────────────────────────
static void keys(std::initializer_list<std::pair<const char*,std::function<void()>>> ks) {
  int n=ks.size(), i=0, kw=W/n;
  uint16_t line=g_dark?INK_40:BRASS, fg=FG();
  g_can.drawFastHLine(0,KEY_Y,W,line);
  for (auto& k:ks){
    int x=i*kw;
    if(i) g_can.drawFastVLine(x,KEY_Y+6,KEY_H-12,line);
    tracked(x+kw/2, KEY_Y+KEY_H/2, k.first, 1, fg, &F_LABEL, middle_center);
    g_btns.push_back({x,KEY_Y,kw,KEY_H,k.second});
    ++i;
  }
}

// ── HOME ── swipeable pages ──────────────────────────────────────────────────
static void homeDial(const lmcloud::State& s,bool on,float temp,
                     lmcloud::BoilerStatus cstat){
  const int cx=W/2, cy=106, r=66;
  float range = max(2.f, s.coffeeTempMax - s.coffeeTempMin);
  int   nT = max(4,(int)(range*2)), maj = max(1,(int)(range/2));
  bezel(cx,cy,r,FACE());
  ticks(cx,cy,r, nT,maj, 135,270, INK_40,FG());
  g_can.fillRect(cx-1, cy-r+2, 3, 8, LM_RED);
  if (on && temp>0){
    float frac=constrain((temp-s.coffeeTempMin)/range,0.f,1.f);
    float a=(135+270*frac)*DEG_TO_RAD;
    g_can.drawWedgeLine(cx+cosf(a)*(r-22),cy+sinf(a)*(r-22),
                        cx+cosf(a)*(r-6), cy+sinf(a)*(r-6), 1.5f,0.5f, LM_RED);
    char tbuf[8]; snprintf(tbuf,sizeof tbuf,"%.0f",c2disp(temp));
    g_can.setFont(&F_NUM_LG); g_can.setTextColor(FG());
    g_can.setTextDatum(middle_center);
    int tw=g_can.textWidth(tbuf);
    g_can.drawString(tbuf, cx, cy-2);
    ring(cx+tw/2+6, cy-16, 3,2, INK_40, FACE());
    tracked(cx, cy+30, boilerWord(cstat), 3, FG(), &F_LABEL, middle_center);
  } else {
    tracked(cx,cy-2,
            s.machine==lmcloud::MachineStatus::StandBy?"STANDBY":"OFFLINE",
            4,INK_40,&F_LABEL,middle_center);
    g_can.drawSpot(cx,cy+28,3,INK_40);
  }
}

static void homeTally(const lmcloud::State& s){
  const int cx=W/2, cy=106, r=66;
  bezel(cx,cy,r,FACE());
  char n[10]; snprintf(n,sizeof n,"%d",s.totalCoffee);
  g_can.setFont(&F_NUM_LG); g_can.setTextColor(FG());
  g_can.setTextDatum(middle_center);
  g_can.drawString(n,cx,cy-8);
  tracked(cx,cy+26,"COFFEES",4,INK_40,&F_LABEL,middle_center);
  // last shot mark on the rim — fraction of a 60s sweep
  if (s.lastShotSec>0){
    float a=(-90+constrain(s.lastShotSec,0.f,60.f)*6.f)*DEG_TO_RAD;
    g_can.drawSpot(cx+cosf(a)*(r-6), cy+sinf(a)*(r-6), 3, LM_RED);
  }
}

static void homeClock(const lmcloud::State&){
  const int cx=W/2, cy=106, r=66;
  bezel(cx,cy,r,FACE());
  ticks(cx,cy,r, 60,5, -90,360, INK_40,FG());
  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  float ha=(-90 + (t.tm_hour%12 + t.tm_min/60.f)*30.f)*DEG_TO_RAD;
  float ma=(-90 +  t.tm_min*6.f)*DEG_TO_RAD;
  g_can.drawWedgeLine(cx,cy,cx+cosf(ha)*(r-30),cy+sinf(ha)*(r-30),2.5f,1.0f,FG());
  g_can.drawWedgeLine(cx,cy,cx+cosf(ma)*(r-12),cy+sinf(ma)*(r-12),1.5f,0.5f,LM_RED);
  g_can.drawSpot(cx,cy,4,INK); g_can.drawSpot(cx,cy,2,BRASS_HI);
}

static void renderHome(const lmcloud::State& s){
  g_can.fillScreen(BG()); vignette(); g_btns.clear();
  header(s);

  bool on = g_dbgOn || s.machine==lmcloud::MachineStatus::PoweredOn ||
            s.machine==lmcloud::MachineStatus::Brewing;
  float temp = g_dbgOn ? 93.f : s.coffeeTarget;
  auto cstat = g_dbgOn ? lmcloud::BoilerStatus::Ready : s.coffeeStatus;

  switch(g_homePage){
    case 1:  homeTally(s);            break;
    case 2:  homeClock(s);            break;
    default: homeDial(s,on,temp,cstat);
  }

  // ── infoline + page dots (the dots double as the divider) ──
  const int ly=KEY_Y-12, dotW=(HOME_PAGES-1)*10;
  for(int i=0;i<HOME_PAGES;++i)
    g_can.drawSpot(W/2-dotW/2+i*10, ly, 2, i==g_homePage?BRASS:INK_40);

  int  cleanDays = s.lastCleanMs ? (int)((epochMs()-s.lastCleanMs)/86400000) : -1;
  bool cleanDue  = cleanDays > 7;
  if (s.lastShotSec>0){
    char l[20],rb[16];
    snprintf(l,sizeof l,"LAST  %.1fs",s.lastShotSec);
    tracked(W/2-dotW/2-12,ly,l,1,INK_40,&F_LABEL_SM,middle_right);
    if (cleanDue) snprintf(rb,sizeof rb,"CLEAN  %dd",cleanDays);
    else          snprintf(rb,sizeof rb,"%s AGO",ago(s.lastShotAtMs).c_str());
    tracked(W/2+dotW/2+12,ly,rb,1,cleanDue?LM_RED:INK_40,&F_LABEL_SM,middle_left);
  } else if (cleanDue){
    char rb[16]; snprintf(rb,sizeof rb,"CLEAN  %dd",cleanDays);
    tracked(W/2+dotW/2+12,ly,rb,1,LM_RED,&F_LABEL_SM,middle_left);
  }

  keys({
    { on?"STANDBY":"WAKE", [on]{ lmcloud::setPower(!on); } },
    { "MACHINE", []{ g_scr=Screen::Controls; g_ctrlScroll=0; g_dirty=true; } },
    { "STATS",   []{ g_scr=Screen::Stats;    g_ctrlScroll=0; g_dirty=true; } },
  });
}

// ── BREWING — the chronograph ────────────────────────────────────────────────
static void renderBrewing(const lmcloud::State& s){
  g_can.fillScreen(NIGHT); g_btns.clear();
  header(s,true);

  float sec = (g_dbgBrewSec>=0) ? g_dbgBrewSec
            : s.brewingStartMs ? (epochMs()-s.brewingStartMs)/1000.0f : g_shotHold;
  if (sec<0) sec=0;

  // haptic + tone cue at 25s and 30s — buzz once per threshold per shot
  static int s_buzzedAt = -1;
  int isec = (int)sec;
  if (isec < s_buzzedAt) s_buzzedAt = -1;             // new shot started
  if ((isec==25 || isec==30) && isec!=s_buzzedAt){
    s_buzzedAt = isec;
    M5.Power.setVibration(128); g_vibOffAt = millis()+40;
    M5.Speaker.tone(1800,60);
  }

  const int cx=W/2, cy=132, r=96;
  // bezel — brass ring (3px) on dark
  ring(cx,cy,r+3,r-1,BRASS,NIGHT);
  smoothArc(cx,cy,r+3,0.5f,0,360,BRASS_HI);
  // 60 ticks, majors at 5s
  ticks(cx,cy,r, 60,5, -90,360, 0x39C7,LUME);
  // rim numerals — drawn last so the hand never covers them
  auto rimNums=[&]{
    g_can.setFont(&F_LABEL_SM); g_can.setTextColor(INK_40);
    g_can.setTextDatum(middle_center);
    for(int v=10;v<60;v+=10){
      float a=(-90+v*6)*DEG_TO_RAD;
      g_can.drawNumber(v, cx+(int)(cosf(a)*(r-22)), cy+(int)(sinf(a)*(r-22)));
    }
  };
  // trail arc + hand — kept to the outer ring so the centre stays clear
  float deg=fmodf(sec,60.f)*6.f;
  if (deg>0.5f) smoothArc(cx,cy,r-5,3.0f, -90, -90+deg, LM_RED);
  hand(cx,cy,r-10,-90+deg,LM_RED);
  // clear a disc for the readout so the hand never crosses it
  g_can.fillSmoothCircle(cx,cy,58,NIGHT);
  rimNums();    // numerals on top of arc/hand, outside the cleared disc
  // big numerals
  char tbuf[8]; snprintf(tbuf,sizeof tbuf,"%d",(int)sec);
  g_can.setFont(&F_NUM_LG); g_can.setTextColor(LUME);
  g_can.setTextDatum(middle_center);
  int tw=g_can.textWidth(tbuf);
  g_can.drawString(tbuf,cx,cy-4);
  g_can.setFont(&F_NUM_MD); g_can.setTextColor(BRASS_HI);
  g_can.setTextDatum(bottom_left);
  char frac[4]; snprintf(frac,sizeof frac,".%d",(int)(sec*10)%10);
  g_can.drawString(frac, cx+tw/2+2, cy+16);

  tracked(cx,cy+40,"SECONDS",4,INK_40,&F_LABEL_SM,middle_center);
}

// ── CONTROLS ─────────────────────────────────────────────────────────────────
static void toggleRow(int y,const char* label,bool on,std::function<void()> tap){
  tracked(18,y+18,label,2,FG(),&F_LABEL,middle_left);
  // physical-style toggle — nested smooth round-rects for a clean AA ring
  int tx=W-18-54, ty=y+18;
  g_can.fillSmoothRoundRect(tx-1,ty-11,56,22,11, BRASS);
  g_can.fillSmoothRoundRect(tx+1,ty-9, 52,18, 9, on?LM_RED:BG_LO());
  g_can.fillSmoothCircle   (tx+(on?44:10), ty, 7, g_dark?LUME:PAPER);
  g_can.drawFastHLine(12,y+36,W-24,BG_LO());
  g_btns.push_back({0,y,W,36,std::move(tap)});
}

// stepper row:  label · [-] value [+]
static void stepRow(int y,const char* label,float v,float step,
                    std::function<void(float)> set,const char* fmt="%.1f"){
  tracked(18,y+18,label,2,FG(),&F_LABEL,middle_left);
  char tv[10]; snprintf(tv,sizeof tv,fmt,v);
  int vcx=W-86;
  g_can.setFont(&F_NUM_MD); g_can.setTextColor(FG());
  g_can.setTextDatum(middle_center); g_can.drawString(tv,vcx,y+18);
  int bx0=vcx-50, bx1=vcx+50;
  for(int bx:{bx0,bx1}){
    ring(bx,y+18,14,12,BRASS,BG());
    g_can.drawWideLine(bx-5,y+18,bx+5,y+18,1.0f,FG());
    if(bx==bx1) g_can.drawWideLine(bx,y+13,bx,y+23,1.0f,FG());
  }
  // generous hit-rects: split the right half of the row at the value centre,
  // and let "+" extend to the screen edge.
  g_btns.push_back({bx0-22,y,vcx-(bx0-22),36,[=]{ set(v-step); }});
  g_btns.push_back({vcx,   y,W-vcx,        36,[=]{ set(v+step); }});
  g_can.drawFastHLine(12,y+36,W-24,BG_LO());
}

// cycler row: tap value to advance through options
static void cycleRow(int y,const char* label,const char* val,std::function<void()> tap){
  tracked(18,y+18,label,2,FG(),&F_LABEL,middle_left);
  int vw = tracked(W-18,y+18,val,2,FG(),&F_LABEL,middle_right);
  g_can.drawFastHLine(W-18-vw,y+28,vw,BRASS);          // underline = tappable
  g_can.drawFastHLine(12,y+36,W-24,BG_LO());
  g_btns.push_back({0,y,W,36,std::move(tap)});
}

static void renderControls(const lmcloud::State& s){
  g_can.fillScreen(BG()); vignette(); g_btns.clear();

  bool steamOn=s.steamEnabled;
  bool sbEn=s.sbEnabled, sbAfter=s.sbAfterBrew; int sbMin=s.sbMinutes;
  float t=s.coffeeTarget, pin=s.preBrewIn, pout=s.preBrewOut;
  auto  pm=s.preMode; uint8_t pmAvail=s.preModesAvail;
  const int top=54, viewH=KEY_Y-top;
  int y = top - g_ctrlScroll;

  g_can.setClipRect(0,top,W,viewH);
  toggleRow(y,"STEAM BOILER",steamOn,[steamOn]{ lmcloud::setSteam(!steamOn);}); y+=40;
  if (s.steamLevelSupported){
    stepRow(y,"STEAM LEVEL",(float)s.steamLevel,1,
            [](float v){ lmcloud::setSteamLevel((uint8_t)constrain(v,1.f,3.f)); },"%.0f"); y+=40;
  }
  stepRow  (y,"BREW TEMP", c2disp(t), tempStep(),
            [](float v){lmcloud::setCoffeeTemp(disp2c(v));},
            settings.fahrenheit?"%.0f":"%.1f");                                 y+=40;
  if (pmAvail){
    const char* pmLabel = pm==lmcloud::PreMode::PreBrewing  ? "PREBREW"
                        : pm==lmcloud::PreMode::PreInfusion ? "PREINFUSE" : "DISABLED";
    cycleRow(y,"PRE-EXTRACTION",pmLabel,[pm,pmAvail]{
      lmcloud::PreMode m=pm;
      for(int k=0;k<3;++k){
        m=(lmcloud::PreMode)(((uint8_t)m+1)%3);
        if(m==lmcloud::PreMode::Disabled) break;
        if(m==lmcloud::PreMode::PreBrewing  && (pmAvail&1)) break;
        if(m==lmcloud::PreMode::PreInfusion && (pmAvail&2)) break;
      }
      lmcloud::setPreMode(m);
    }); y+=40;
    if (pm!=lmcloud::PreMode::Disabled){
      stepRow(y,"  PRE  IN  s",pin, 0.5f,[pout](float v){lmcloud::setPreBrewTimes(v,pout);}); y+=40;
      stepRow(y,"  PRE OUT s", pout,0.5f,[pin](float v){lmcloud::setPreBrewTimes(pin,v);});  y+=40;
    }
  }
  // backflush — the cloud only *arms* cleaning; the machine then waits for the
  // user to move the brew paddle. Reflect that state so a tap isn't a silent
  // no-op: START → MOVE PADDLE (armed) → CLEANING.
  tracked(18,y+18,"BACKFLUSH",2,LM_RED,&F_LABEL,middle_left);
  if (s.backflush==lmcloud::BackflushStatus::Requested){
    tracked(W-18,y+18,"MOVE PADDLE",1,LM_RED,&F_LABEL_SM,middle_right);
  } else if (s.backflush==lmcloud::BackflushStatus::Cleaning){
    tracked(W-18,y+18,"CLEANING",2,LM_RED,&F_LABEL,middle_right);
  } else {
    g_can.fillSmoothRoundRect(W-18-90,y+6,90,24,12,LM_RED);
    g_can.fillSmoothRoundRect(W-18-89,y+7,88,22,11,BG());
    tracked(W-18-45,y+18,"START",2,LM_RED,&F_LABEL,middle_center);
  }
  g_btns.push_back({W-18-100,y+2,118,32,[]{ lmcloud::startBackflush(); }});      y+=40;
  // smart standby
  toggleRow(y,"SMART STANDBY",sbEn,[=]{ lmcloud::setSmartStandby(!sbEn,sbMin,sbAfter);}); y+=40;
  stepRow  (y,"  STANDBY MIN",(float)sbMin,5,
            [=](float v){ lmcloud::setSmartStandby(sbEn,(int)v,sbAfter); },"%.0f");       y+=40;
  cycleRow (y,"  STANDBY AFTER", sbAfter?"LAST BREW":"POWER ON",
            [=]{ lmcloud::setSmartStandby(sbEn,sbMin,!sbAfter); });                       y+=40;

  g_can.clearClipRect();
  int contentH = y - (top - g_ctrlScroll);
  g_ctrlMax = max(0, contentH - viewH);
  // clamp content hit-rects to the viewport so nothing is tappable under chrome
  for(auto& b:g_btns){
    int y0=max(b.y,top), y1=min(b.y+b.h,KEY_Y);
    b.y=y0; b.h=y1-y0;
  }
  g_btns.erase(std::remove_if(g_btns.begin(),g_btns.end(),
    [](const Btn&b){ return b.h<=0; }), g_btns.end());
  // chrome on top of scrolled content
  g_can.fillRect(0,0,W,top,BG()); g_can.fillRect(0,KEY_Y,W,KEY_H,BG());
  header(s);
  tracked(W/2,36,"MACHINE",4,FG(),&F_LABEL,top_center);
  if (g_ctrlMax>0){
    int thH = max(12, viewH*viewH/contentH);
    int thY = top + (viewH-thH) * g_ctrlScroll / g_ctrlMax;
    g_can.fillSmoothRoundRect(W-6,thY,3,thH,1,BRASS);
  }
  keys({{ "BACK", []{ g_scr=Screen::Home; g_ctrlScroll=0; g_dirty=true; } }});
}

// ── STATS — read-only scrollable list ────────────────────────────────────────
static void statRow(int y,const char* k,const String& v,uint16_t vc=0){
  if(!vc) vc=FG();
  g_can.setFont(&F_SERIF_SM); g_can.setTextColor(INK_40);
  g_can.setTextDatum(middle_left); g_can.drawString(k,18,y+16);
  tracked(W-18,y+16,v.c_str(),1,vc,&F_LABEL_SM,middle_right);
  g_can.drawFastHLine(12,y+32,W-24,BG_LO());
}
static String dayStr(uint8_t m){
  static const char* D[]={"Mo","Tu","We","Th","Fr","Sa","Su"};
  String s; for(int i=0;i<7;++i) if(m&(1<<i)) s+=D[i];
  return s.isEmpty()?String("—"):s;
}

static void renderStats(const lmcloud::State& s){
  g_can.fillScreen(BG()); vignette(); g_btns.clear();

  const int top=54, viewH=KEY_Y-top;
  int y = top - g_ctrlScroll;
  g_can.setClipRect(0,top,W,viewH);

  statRow(y,"strumento",     STRUMENTO_VERSION, INK_40);                      y+=36;
  statRow(y,"total coffees", String(s.totalCoffee));                          y+=36;
  statRow(y,"total flushes", String(s.totalFlush));                           y+=36;
  statRow(y,"last clean",    s.lastCleanMs?ago(s.lastCleanMs)+" AGO":"—",
          (s.lastCleanMs && (epochMs()-s.lastCleanMs)>7LL*86400000)?LM_RED:0);   y+=36;
  statRow(y,"water",         s.plumbedIn?"PLUMBED":"TANK");                   y+=36;
  statRow(y,"wifi",          String(s.wifiRssi)+" dBm");                      y+=36;
  for (auto& f : s.firmwares){
    String k = f.type; k.toLowerCase(); k += " fw";
    String v = f.version; if(f.status.length()) v += "  "+f.status;
    statRow(y, k.c_str(), v);                                                 y+=36;
  }
  for (auto& w : s.schedules){
    String k = "wake  "+dayStr(w.dayMask);
    String v = hhmm(w.onMin)+"-"+hhmm(w.offMin);
    statRow(y, k.c_str(), v, w.enabled?0:INK_40);                             y+=36;
  }

  g_can.clearClipRect();
  int contentH = y - (top - g_ctrlScroll);
  g_ctrlMax = max(0, contentH - viewH);
  // chrome
  g_can.fillRect(0,0,W,top,BG()); g_can.fillRect(0,KEY_Y,W,KEY_H,BG());
  header(s);
  tracked(W/2,36,"STATS",4,FG(),&F_LABEL,top_center);
  if (g_ctrlMax>0){
    int thH = max(12, viewH*viewH/contentH);
    int thY = top + (viewH-thH) * g_ctrlScroll / g_ctrlMax;
    g_can.fillSmoothRoundRect(W-6,thY,3,thH,1,BRASS);
  }
  keys({
    { "BACK",  []{ g_scr=Screen::Home;     g_ctrlScroll=0; g_dirty=true; } },
    { "SETUP", []{ g_scr=Screen::Settings; g_ctrlScroll=0; g_dirty=true; } },
  });
}

// ── SETTINGS ─────────────────────────────────────────────────────────────────
static void settingRow(int y,const char* k,const String& v,bool mask,std::function<void()> tap){
  g_can.setFont(&F_SERIF_SM); g_can.setTextColor(INK_40);
  g_can.setTextDatum(middle_left); g_can.drawString(k,18,y+16);
  String s; if(mask) for(int i=0;i<8;++i) s+='*'; else s=v;
  if(s.length()>24) s=s.substring(0,23)+"..";
  g_can.setFont(&F_LABEL_SM); g_can.setTextColor(FG());
  g_can.setTextDatum(middle_left); g_can.drawString(s,100,y+16);
  g_can.drawFastHLine(12,y+32,W-24,BG_LO());
  g_btns.push_back({0,y,W,32,std::move(tap)});
}

static void renderSettings(){
  auto& s=lmcloud::state();
  g_can.fillScreen(BG()); vignette(); g_btns.clear();

  const int top=54, viewH=KEY_Y-top;
  int y = top - g_ctrlScroll;
  g_can.setClipRect(0,top,W,viewH);

  settingRow(y,"network", settings.wifiSsid,false,[]{ if(keyboardPrompt(M5.Display,"WiFi SSID",settings.wifiSsid)) settings.save(); g_dirty=true; }); y+=36;
  settingRow(y,"wifi key",settings.wifiPass,true, []{ if(keyboardPrompt(M5.Display,"WiFi password",settings.wifiPass,true)) settings.save(); g_dirty=true; }); y+=36;
  settingRow(y,"account", settings.lmUser,  false,[]{ if(keyboardPrompt(M5.Display,"La Marzocco e-mail",settings.lmUser)) settings.save(); g_dirty=true; }); y+=36;
  settingRow(y,"password",settings.lmPass,  true, []{ if(keyboardPrompt(M5.Display,"La Marzocco password",settings.lmPass,true)) settings.save(); g_dirty=true; }); y+=36;
  toggleRow(y,"DARK MODE",settings.darkMode,[]{
    settings.darkMode=!settings.darkMode; settings.save();
    g_dark=settings.darkMode; g_dirty=true;
  }); y+=40;
  toggleRow(y,"FAHRENHEIT",settings.fahrenheit,[]{
    settings.fahrenheit=!settings.fahrenheit; settings.save(); g_dirty=true;
  }); y+=40;

  g_can.clearClipRect();
  int contentH = y - (top - g_ctrlScroll);
  g_ctrlMax = max(0, contentH - viewH);
  for(auto& b:g_btns){ int y0=max(b.y,top),y1=min(b.y+b.h,KEY_Y); b.y=y0; b.h=y1-y0; }
  g_btns.erase(std::remove_if(g_btns.begin(),g_btns.end(),
    [](const Btn&b){return b.h<=0;}),g_btns.end());
  g_can.fillRect(0,0,W,top,BG()); g_can.fillRect(0,KEY_Y,W,KEY_H,BG());
  header(s);
  tracked(W/2,36,"SETUP",4,FG(),&F_LABEL,top_center);
  if (g_ctrlMax>0){
    int thH=max(12,viewH*viewH/contentH);
    int thY=top+(viewH-thH)*g_ctrlScroll/g_ctrlMax;
    g_can.fillSmoothRoundRect(W-6,thY,3,thH,1,BRASS);
  }
  keys({
    { "BACK",    []{ g_scr=Screen::Home; g_ctrlScroll=0; g_dirty=true; } },
    { "RECONNECT",[]{ lmcloud::reconnect(); g_scr=Screen::Home; g_dirty=true; } },
  });
}

// ── frame ────────────────────────────────────────────────────────────────────
static void render(){
  lmcloud::lockState();
  auto& s=lmcloud::state();
  if (g_dbgBrewSec>=0){ /* debug: honour g_scr as set */ }
  else if (s.machine==lmcloud::MachineStatus::Brewing && g_scr!=Screen::Settings){
    g_scr=Screen::Brewing; g_shotHoldUntil=0;        // live brew — cancel any freeze
  }
  else if (g_scr==Screen::Brewing){
    // Brew ended. The cloud reports the stop a beat after the paddle drops, so
    // the live count (device clock − server brew-start) has already overshot the
    // real pour. Snap the dial to LM's authoritative extractionSeconds and hold
    // it on screen briefly, so the last number shown is the machine's own
    // measurement — not the latency-inflated live value.
    if (g_shotHoldUntil==0 && s.lastShotSec>0){
      g_shotHold=s.lastShotSec; g_shotHoldUntil=millis()+SHOT_HOLD_MS;
    }
    if (g_shotHoldUntil==0 || millis()>=g_shotHoldUntil){ g_shotHoldUntil=0; g_scr=Screen::Home; }
  }
  switch(g_scr){
    case Screen::Home:     renderHome(s);     break;
    case Screen::Brewing:  renderBrewing(s);  break;
    case Screen::Controls: renderControls(s); break;
    case Screen::Settings: renderSettings();  break;
    case Screen::Stats:    renderStats(s);    break;
  }
  lmcloud::unlockState();
  g_can.pushSprite(0,0);
}

// ── public ───────────────────────────────────────────────────────────────────
void begin(){
  g_can.setPsram(true);
  g_can.setColorDepth(16);
  g_can.createSprite(W,H);
  M5.Display.setBrightness(200);
  // Parse VLW headers once; setFont(&F_*) is then a pointer swap.
  AA_WORDMARK.load(vlw_wordmark, sizeof vlw_wordmark);
  AA_SERIF_SM.load(vlw_serif_sm, sizeof vlw_serif_sm);
  AA_LABEL   .load(vlw_label,    sizeof vlw_label);
  AA_LABEL_SM.load(vlw_label_sm, sizeof vlw_label_sm);
  AA_NUM_LG  .load(vlw_num_lg,   sizeof vlw_num_lg);
  AA_NUM_MD  .load(vlw_num_md,   sizeof vlw_num_md);
  g_dark = settings.darkMode;
  lmcloud::onChange([]{ g_dirty=true; });
}

void forceSetup(){ g_scr=Screen::Settings; g_dirty=true; }

void splash(const char* line){
  g_can.fillScreen(BG());
  g_can.setTextDatum(middle_center);
  g_can.setFont(&F_WORDMARK); g_can.setTextColor(FG());
  g_can.drawString("strumento", W/2, H/2-18);
  g_can.drawFastHLine(W/2-56,H/2+6,112,BRASS);
  tracked(W/2,H/2+22,STRUMENTO_VERSION,3,INK_40,&F_LABEL_SM,middle_center);
  g_can.setFont(&F_LABEL_SM); g_can.setTextColor(INK_40);
  g_can.setTextDatum(middle_center);
  g_can.drawString(line, W/2, H-22);
  g_can.pushSprite(0,0);
}

void screenshot(){
  const uint8_t* buf=(const uint8_t*)g_can.getBuffer();
  static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  Serial.flush(); delay(10);
  Serial.println("###SHOT_BEGIN###");
  char line[16+ (W*2+2)/3*4 +2];
  for(int y=0;y<H;++y){
    const uint8_t* row=buf+(size_t)y*W*2;
    int li=snprintf(line,sizeof line,"#R%03d:",y);
    for(int i=0;i<W*2;i+=3){
      uint32_t v=row[i]<<16 | (i+1<W*2?row[i+1]:0)<<8 | (i+2<W*2?row[i+2]:0);
      line[li++]=T[(v>>18)&63]; line[li++]=T[(v>>12)&63];
      line[li++]=(i+1<W*2)?T[(v>>6)&63]:'='; line[li++]=(i+2<W*2)?T[v&63]:'=';
    }
    line[li]=0; Serial.println(line);
  }
  Serial.println("###SHOT_END###");
}

void debugScreen(int n,float arg){
  g_dbgBrewSec = (n==1)?arg:-1;
  g_dbgOn      = (n==4||n==6||n==7);
  g_homePage   = (n==6)?1:(n==7)?2:0;
  g_scr = (n==4||n==6||n==7)?Screen::Home : (n==5)?Screen::Stats : (Screen)n;
  render();
}

void tick(){
  M5.update();
  if (g_vibOffAt && millis()>=g_vibOffAt){ M5.Power.setVibration(0); g_vibOffAt=0; }
  auto t=M5.Touch.getDetail();
  bool scrollable = (g_scr==Screen::Controls || g_scr==Screen::Stats ||
                     g_scr==Screen::Settings);
  bool swipeable  = (g_scr==Screen::Home);

  if (t.wasPressed()){
    g_dragging=false; g_dragStartX=t.x; g_dragStartY=t.y;
    g_dragStartScroll=g_ctrlScroll;
  }
  if (t.isPressed() && scrollable){
    int dy=t.y-g_dragStartY;
    if (g_dragging || abs(dy)>8){
      g_dragging=true;
      g_ctrlScroll=constrain(g_dragStartScroll-dy,0,g_ctrlMax);
      g_dirty=true;
    }
  }
  if (t.wasReleased() && swipeable){
    int dx=t.x-g_dragStartX, dy=t.y-g_dragStartY;
    if (abs(dx)>50 && abs(dx)>2*abs(dy)){
      g_homePage = (g_homePage + (dx<0?1:HOME_PAGES-1)) % HOME_PAGES;
      M5.Speaker.tone(2000,14);
      g_dragging=true; g_dirty=true;      // suppress tap
    }
  }
  if (t.wasReleased() && !g_dragging){
    for(auto& b:g_btns) if(t.x>=b.x&&t.x<b.x+b.w&&t.y>=b.y&&t.y<b.y+b.h){
      M5.Speaker.tone(2200,18);
      if(b.tap) b.tap();
      g_dirty=true; break;
    }
  }
  uint32_t now=millis();
  bool fast = (g_scr==Screen::Brewing) || (scrollable && t.isPressed());
  bool due  = fast ? (now-g_lastFrame>60) : (now-g_lastFrame>1000);
  if (g_dirty||due){ g_lastFrame=now; g_dirty=false; render(); }
}

}  // namespace ui
