/* Native UD Modbus gateway. No custom reinterpretation of official registers. */
#include "modbus_u3.h"
#include <stdio.h>
#include <string.h>
static uint16_t word(const uint8_t *p) { return (uint16_t)(((unsigned)p[0]<<8)|p[1]); }
static void put(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static int native(modbus_ctx_t *c, uint8_t uid, const uint8_t *p, size_t n,
                  uint8_t *out, size_t *on) {
    uint8_t tx[64], rx[64]; unsigned long got; uint16_t t=++c->transaction, len;
    if(n+9>sizeof tx) return 3;
    memset(tx,0,9); put(tx+2,t); put(tx+6,(uint16_t)(n+1)); tx[8]=uid;
    memcpy(tx+9,p,n); /* two USB prefix zeroes, followed by TCP MBAP and PDU */
    if(LJUSB_Write(c->handle,tx,(unsigned long)n+9)!=n+9) return 4;
    got=LJUSB_Read(c->handle,rx,sizeof rx);
    if(got<8 || word(rx)!=t || word(rx+2)!=0 || rx[6]!=uid) return 4;
    len=word(rx+4);
    if(len<2 || (size_t)len+6!=got) return 4;
    *on=(size_t)len-1;
    if(rx[7]==(uint8_t)(p[0]|0x80)) return *on==2?rx[8]:4;
    if(rx[7]!=p[0]) return 4;
    if((p[0]==3 || p[0]==4) && (*on<2 || rx[8]!=2U*word(p+3) || *on!=(size_t)rx[8]+2)) return 4;
    if((p[0]==6 || p[0]==16) && (*on!=5 || memcmp(rx+8,p+1,4))) return 4;
    memcpy(out,rx+7,*on); return 0;
}
static int rd(modbus_ctx_t *c,uint8_t uid,uint16_t a,uint16_t n,uint16_t *r) {
    uint8_t p[5],o[253];size_t on;unsigned i;int e;
    p[0]=3;put(p+1,a);put(p+3,n);e=native(c,uid,p,5,o,&on);
    if(!e) for(i=0;i<n;i++) r[i]=word(o+2+2*i);
    return e;
}
static int wr(modbus_ctx_t *c,uint8_t uid,uint16_t a,uint16_t n,const uint16_t *r) {
    uint8_t p[55],o[253];size_t on;unsigned i;
    if(n>24) return 3;
    p[0]=16;put(p+1,a);put(p+3,n);p[5]=(uint8_t)(2*n);
    for(i=0;i<n;i++) put(p+6+2*i,r[i]);
    return native(c,uid,p,6+2*n,o,&on);
}
/* Stage is a raw image: packed timer-config UInt32 values remain firmware-defined. */
static const uint16_t addresses[9]={50590,50591,50500,50501,50502,7000,7002,7100,7102};
static const unsigned offsets[9]={0,1,2,3,4,5,7,9,11};
static const uint16_t counts[9]={1,1,1,1,1,2,2,2,2};
static int load(modbus_ctx_t *c,uint8_t uid) {
    uint16_t temp[13];unsigned i;int e;
    for(i=0;i<9;i++) { e=rd(c,uid,addresses[i],counts[i],temp+offsets[i]);if(e) return e; }
    memcpy(c->stage,temp,sizeof temp); c->loaded=1;return 0;
}
static int apply(modbus_ctx_t *c,uint8_t uid) {
    /* Sequential USB writes: NOT atomic, no rollback. Disable before allocation. */
    static const unsigned order[9]={0,1,2,5,6,7,8,3,4};
    uint16_t zero=0;unsigned j;int e;
    c->writes_done=0;
    if(!c->loaded) return 3;
    if(c->stage[0]>255 || c->stage[1]>255 || c->stage[2]>8 || c->stage[3]>2 || c->stage[4]>3) return 3;
    { unsigned n=c->stage[3]+(c->stage[4]&1U)+((c->stage[4]>>1)&1U),k;
      /* This extension conservatively supports timer/counter allocation in FIO only. */
      if(c->stage[2]+n>8) return 3;
      for(k=0;k<n;k++) if(c->stage[0]&(1U<<(c->stage[2]+k))) return 3;
    }
    e=wr(c,uid,50501,1,&zero);if(e) return e;c->writes_done++;
    e=wr(c,uid,50502,1,&zero);if(e) return e;c->writes_done++;
    for(j=0;j<9;j++) { unsigned i=order[j];
      /* Disabled timer configurations need not be programmed. */
      if((i==7 && c->stage[3]<1) || (i==8 && c->stage[3]<2)) continue;
      e=wr(c,uid,addresses[i],counts[i],c->stage+offsets[i]);if(e) return e;c->writes_done++;
    }
    return 0;
}
int modbus_init(modbus_ctx_t *c) {
    memset(c,0,sizeof *c);c->handle=openUSBConnection(-1);
    return c->handle?0:-1; /* No automatic I/O changes at startup. */
}
void modbus_close(modbus_ctx_t *c) { if(c->handle) closeUSBConnection(c->handle);c->handle=NULL; }
int mb_request(modbus_ctx_t *c,uint8_t uid,const uint8_t *p,size_t n,uint8_t *out,size_t *on) {
    uint16_t a,count;unsigned i;int e;
    if(n<5) return 3;
    a=word(p+1);count=word(p+3);
    if(p[0]!=3 && p[0]!=4 && p[0]!=6 && p[0]!=16) return 1;
    if((p[0]==3 || p[0]==4) && (n!=5 || !count || count>125)) return 3;
    if(p[0]==6 && n!=5) return 3;
    if(p[0]==16 && (n<6 || !count || count>123 || p[5]!=count*2U || n!=6U+count*2U)) return 3;
    if(p[0]==6) count=1;
    if((unsigned)a+count>65536U) return 2;
    if(a>=CUSTOM_BASE && a<=45031) {
      if((unsigned)a+count>45032) return 2;
      if(p[0]==3 || p[0]==4) {
        for(i=0;i<count;i++) { uint16_t x=(uint16_t)(a+i),v;
          if(x<=45012) v=c->stage[x-CUSTOM_BASE];
          else if(x==45021) v=c->apply_status;
          else if(x==45022) v=c->last_error;
          else if(x==45023) v=c->loaded;
          else if(x==45024) v=c->writes_done;
          else if(x==45025) v=2;
          else return 2;
          put(out+2+2*i,v);
        } out[0]=p[0];out[1]=(uint8_t)(count*2);*on=2+count*2;return 0;
      }
      if(a==CUSTOM_COMMAND && count==1) {
        uint16_t cmd=word(p+(p[0]==6?3:6));
        if(cmd==2) e=load(c,uid);
        else if(cmd==APPLY_MAGIC) e=apply(c,uid);
        else return 3;
        c->last_error=(uint16_t)e;
        if(cmd==APPLY_MAGIC) c->apply_status=(uint16_t)e;
        fprintf(stderr,"DIELTECH command 0x%04X: exception=%d completed_writes=%u\n",cmd,e,c->writes_done);
        if(e) return e;
      } else {
        if(a>45012 || (unsigned)a+count>45013) return 2;
        for(i=0;i<count;i++) c->stage[a-CUSTOM_BASE+i]=word(p+(p[0]==6?3:6)+2*i);
      }
      memcpy(out,p,5);*on=5;return 0;
    }
    /* Do not forward requests which cross the custom block. */
    if(a<CUSTOM_BASE && (unsigned)a+count>CUSTOM_BASE) return 2;
    if(p[0]==3 || p[0]==4) {
      uint16_t r[125];unsigned done=0;
      while(done<count) { uint16_t k=(uint16_t)(count-done);if(k>26) k=26;
        uint8_t q[5],o[253];size_t z;
        q[0]=p[0];put(q+1,(uint16_t)(a+done));put(q+3,k);
        e=native(c,uid,q,5,o,&z);if(e) {c->last_error=(uint16_t)e;return e;}
        for(i=0;i<k;i++) r[done+i]=word(o+2+2*i);
        done+=k;
      }
      out[0]=p[0];out[1]=(uint8_t)(2*count);
      for(i=0;i<count;i++) put(out+2+2*i,r[i]);
      *on=2+2*count;return 0;
    }
    /* Do not split writes: preserve native multi-register transaction semantics. */
    if(p[0]==16 && count>24) return 3;
    e=native(c,uid,p,n,out,on);if(e) c->last_error=(uint16_t)e;return e;
}
