#include "keyboard.h"
#include "config.h"

using namespace cfg;
namespace {
const char* L[4][3] = {
  {"qwertyuiop","asdfghjkl@","zxcvbnm,._"},
  {"QWERTYUIOP","ASDFGHJKL@","ZXCVBNM,._"},
  {"1234567890","-=+[]{}/\\|","!#$%^&*()~"},
  {":;'\"<>?`~ ","@#$%^&*_+=","!?.,|{}/\\ "},
};
constexpr int KW=32, KH=36, KY0=70;

struct Key { int x,y,w,h; char ch; const char* lab; };

void drawKey(M5GFX& d,const Key& k,bool hi=false){
  d.fillRect(k.x,k.y,k.w,k.h, hi?LM_RED:PAPER);
  d.drawRect(k.x,k.y,k.w,k.h, IVORY_LO);
  d.setTextDatum(middle_center);
  d.setTextColor(hi?PAPER:INK);
  if(k.lab){ d.setFont(&fonts::FreeSans9pt7b); d.drawString(k.lab,k.x+k.w/2,k.y+k.h/2); }
  else { d.setFont(&fonts::FreeMonoBold12pt7b); char s[2]={k.ch,0}; d.drawString(s,k.x+k.w/2,k.y+k.h/2); }
}
}  // namespace

bool keyboardPrompt(M5GFX& d,const char* title,String& value,bool mask){
  int layer=0; String buf=value;
  std::vector<Key> keys;
  auto layout=[&]{
    keys.clear();
    for(int r=0;r<3;r++) for(int c=0;c<10;c++)
      keys.push_back({c*KW,KY0+r*KH,KW,KH,L[layer][r][c],nullptr});
    int y=KY0+3*KH;
    keys.push_back({  0,y,48,KH,0,"abc"});
    keys.push_back({ 48,y,48,KH,0,"?12"});
    keys.push_back({ 96,y,128,KH,' ',nullptr});
    keys.push_back({224,y,48,KH,0,"del"});
    keys.push_back({272,y,48,KH,0,"OK"});
    keys.push_back({0,KY0+4*KH,320,26,0,"cancel"});
  };
  auto draw=[&]{
    d.fillScreen(IVORY);
    d.drawFastHLine(12,24,W-24,BRASS);
    d.setFont(&fonts::FreeSerifItalic9pt7b); d.setTextColor(INK_40);
    d.setTextDatum(top_left); d.drawString(title,14,6);
    d.fillRect(12,32,W-24,28,PAPER); d.drawRect(12,32,W-24,28,BRASS);
    d.setFont(&fonts::FreeMono9pt7b); d.setTextColor(INK);
    String s; if(mask) for(size_t i=0;i<buf.length();++i) s+='*'; else s=buf;
    if(s.length()>26) s="…"+s.substring(s.length()-25);
    d.setTextDatum(middle_left); d.drawString(s,18,46);
    for(auto&k:keys) drawKey(d,k);
  };
  layout(); draw();
  for(;;){
    M5.update(); auto t=M5.Touch.getDetail();
    if(t.wasPressed()) for(auto&k:keys)
      if(t.x>=k.x&&t.x<k.x+k.w&&t.y>=k.y&&t.y<k.y+k.h){
        drawKey(d,k,true); M5.Speaker.tone(2200,12); delay(60); drawKey(d,k,false);
        if(k.lab){
          if(!strcmp(k.lab,"abc")){ layer=(layer==0)?1:0; layout(); draw(); }
          else if(!strcmp(k.lab,"?12")){ layer=(layer<2)?2:(layer==2)?3:0; layout(); draw(); }
          else if(!strcmp(k.lab,"del")){ if(buf.length()) buf.remove(buf.length()-1); draw(); }
          else if(!strcmp(k.lab,"OK")){ value=buf; return true; }
          else if(!strcmp(k.lab,"cancel")) return false;
        } else { buf+=k.ch; draw(); }
        break;
      }
    delay(8);
  }
}
