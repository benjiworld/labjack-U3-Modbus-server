/* Read-only inspector for map v2. No APPLY/reset or output writes. */
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static int io(int fd,uint8_t *b,size_t n,int sendit) {
 while(n){ssize_t k=sendit?send(fd,b,n,0):recv(fd,b,n,0);
 if(k<0&&errno==EINTR)continue;
 if(k<=0)return -1;
 b+=k;n-=(size_t)k;}return 0;
}
static int rd(int fd,uint16_t a,uint16_t n,uint16_t *out) {
 static uint16_t t;uint8_t q[12]={0},h[7],p[253];size_t z;unsigned i;
 ++t;q[0]=t>>8;q[1]=t;q[5]=6;q[6]=1;q[7]=3;q[8]=a>>8;q[9]=a;q[10]=n>>8;q[11]=n;
 if(io(fd,q,12,1)||io(fd,h,7,0))return -1;
 z=((size_t)h[4]<<8)|h[5];
 if(h[0]!=q[0]||h[1]!=q[1]||h[2]||h[3]||h[6]!=1||z<2||z>254)return -1;
 if(io(fd,p,z-1,0))return -1;
 if(p[0]==0x83){printf("  HR %u: firmware exception %u\n",a,z>=3?p[1]:4);return 1;}
 if(p[0]!=3||z!=3+2U*n||p[1]!=2*n)return -1;
 for(i=0;i<n;i++)out[i]=(uint16_t)(((unsigned)p[2+2*i]<<8)|p[3+2*i]);
 return 0;
}
static int show(int fd,uint16_t a,unsigned n,const char *name,int floating) {
 uint16_t v[2];int e=rd(fd,a,(uint16_t)n,v);if(e)return e;
 printf("%-22s HR %-5u %u bit / %u reg  ",name,a,16*n,n);
 if(n==1)printf("%u (0x%04X)\n",v[0],v[0]);
 else{uint32_t u=((uint32_t)v[0]<<16)|v[1];
 if(floating){float f;memcpy(&f,&u,4);printf("%.7g",(double)f);}
 else printf("%lu",(unsigned long)u);
 printf(" [0x%04X 0x%04X]\n",v[0],v[1]);}return 0;
}
int main(int argc,char **argv) {
 int fd,port=5020,fatal=0,e;unsigned i;char name[32];uint16_t fm,em,nt,cm;
 int masks_ok,nt_ok,cm_ok;struct sockaddr_in a;struct timeval tv={5,0};
 if(argc>3){fprintf(stderr,"Usage: %s [IPv4] [port]\n",argv[0]);return 1;}
 if(argc>2){char *end;long x=strtol(argv[2],&end,10);if(*end||x<1||x>65535)return 1;port=(int)x;}
 signal(SIGPIPE,SIG_IGN);fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 1;
 setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);
 memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);
 if(inet_pton(AF_INET,argc>1?argv[1]:"127.0.0.1",&a.sin_addr)!=1||connect(fd,(struct sockaddr*)&a,sizeof a)){perror("connect/address");close(fd);return 1;}
 puts("DIELTECH inspector | native UD + custom map v2\nNo command register is read or written.");
#define SHOW(a,n,label,f) do{e=show(fd,(a),(n),(label),(f));if(e<0){fatal=1;goto done;}}while(0)
 masks_ok=!rd(fd,50590,1,&fm);masks_ok=(!rd(fd,50591,1,&em))&&masks_ok;
 nt_ok=!rd(fd,50501,1,&nt);cm_ok=!rd(fd,50502,1,&cm);
 for(i=0;i<16;i++){
 if(!masks_ok||!((i<8?fm:em)&(1U<<(i%8)))){printf("AIN%u skipped: digital or mode unknown\n",i);continue;}
 snprintf(name,sizeof name,"AIN%u (V)",i);SHOW((uint16_t)(2*i),2,name,1);}
 SHOW(5000,2,"DAC0 (V)",1);SHOW(5002,2,"DAC1 (V)",1);
 for(i=0;i<20;i++){
 if(i<16&&(!masks_ok||((i<8?fm:em)&(1U<<(i%8)))))continue;
 snprintf(name,sizeof name,"DIO%u state",i);SHOW((uint16_t)(6000+i),1,name,0);
 snprintf(name,sizeof name,"DIO%u direction",i);SHOW((uint16_t)(6100+i),1,name,0);}
 SHOW(7000,2,"Timer clock base",0);SHOW(7002,2,"Timer divisor",0);
 for(i=0;i<2;i++)if(nt_ok&&nt>i){
 snprintf(name,sizeof name,"Timer%u config (raw)",i);SHOW((uint16_t)(7100+2*i),2,name,0);
 snprintf(name,sizeof name,"Timer%u value (raw)",i);SHOW((uint16_t)(7200+2*i),2,name,0);}
 for(i=0;i<2;i++)if(cm_ok&&(cm&(1U<<i))){snprintf(name,sizeof name,"Counter%u",i);SHOW((uint16_t)(7300+2*i),2,name,0);}
 SHOW(50500,1,"Pin offset",0);SHOW(50501,1,"Number timers",0);SHOW(50502,1,"Counter mask",0);
 SHOW(50590,1,"FIO analog mask",0);SHOW(50591,1,"EIO analog mask",0);SHOW(55000,1,"UD detailed error",0);
 for(i=0;i<13;i++){snprintf(name,sizeof name,"Stage word %u",i);SHOW((uint16_t)(45000+i),1,name,0);}
 for(i=45021;i<=45025;i++){snprintf(name,sizeof name,"Custom diagnostic");SHOW((uint16_t)i,1,name,0);}
 puts("Timer values above are raw bits; signedness and units depend on mode.");
 done:close(fd);if(fatal)fputs("Transport failure; inspection aborted.\n",stderr);return fatal?1:0;
}
