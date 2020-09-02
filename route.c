#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>

// #define SZ 16384
#define SZ 8192
#define eprintf(...) fprintf(stderr,__VA_ARGS__)

int fd=-1;

char recvbuf[SZ]={};
int len=0;

struct in_addr gw={};

void init(){
  fd=socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);
  assert(fd==3);
  assert(0==bind(fd,(struct sockaddr*)(&(struct sockaddr_nl){
    .nl_family=AF_NETLINK,
    .nl_pad=0,
    .nl_pid=getpid(),
    .nl_groups=0
  }),sizeof(struct sockaddr_nl)));
}

void end(){
  assert(0==close(fd));
  fd=-1;
}

void clearbuf(){
  bzero(recvbuf,SZ);
  len=0;
}

void receive(){
  for(char *p=recvbuf;;){
    const int seglen=recv(fd,p,sizeof(recvbuf)-len,0);
    assert(seglen>=1);
    len += seglen;
    // printf("0x%X\n",((struct nlmsghdr*)p)->nlmsg_type);
    if(((struct nlmsghdr*)p)->nlmsg_type==NLMSG_DONE||((struct nlmsghdr*)p)->nlmsg_type==NLMSG_ERROR)
      break;
    p += seglen;
  }
}

/*void poll(){
  struct nlmsghdr *nh=(struct nlmsghdr*)recvbuf;
  for(;NLMSG_OK(nh, len);nh=NLMSG_NEXT(nh, len)){
    switch(nh->nlmsg_type){
      case NLMSG_NOOP:printf("NLMSG_NOOP ");break;
      case NLMSG_ERROR:printf("NLMSG_ERROR ");break;
      case NLMSG_DONE:printf("NLMSG_DONE ");break;
      case RTM_NEWROUTE:
        printf("RTM_NEWROUTE ");
        // struct rtmsg *rtm=NLMSG_DATA(nh);
        break;
      default:assert(false);
    }
    printf("\n");
  }
}*/

void ack(){
  assert(((struct nlmsghdr*)recvbuf)->nlmsg_type==NLMSG_ERROR);
  assert(((struct nlmsgerr*)NLMSG_DATA((struct nlmsghdr*)recvbuf))->error==0);
}

void attr(struct nlmsghdr *np,struct rtattr *rp,int type,const void *data){
  rp->rta_type=type;
  int l=0;
  if(type==RTA_DST||type==RTA_GATEWAY){
    l=sizeof(struct in_addr);
    rp->rta_len=RTA_LENGTH(l);
    bzero(RTA_DATA(rp),l);
    assert(1==inet_pton(AF_INET,data,RTA_DATA(rp)));
  }
  else if(type==RTA_OIF){
    l=sizeof(int);
    rp->rta_len=RTA_LENGTH(l);
    *((int*)RTA_DATA(rp))=*((int*)data);
  }else{
    assert(false);
  }
  np->nlmsg_len=NLMSG_ALIGN(np->nlmsg_len)+RTA_LENGTH(l);
}

void add(){

  printf("+ dst 7.7.7.7 gw 192.168.1.1 dev 3\n");

  typedef struct {
    struct nlmsghdr nh;
    struct rtmsg rt;
    char attrbuf[SZ]; // rtnetlink(3)
  } Req;

  Req req={
    .nh={
      .nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)),
      .nlmsg_type = RTM_NEWROUTE,
      // .nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE,
      .nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE | NLM_F_ACK ,
      .nlmsg_seq=0,
      .nlmsg_pid=0
    },
    .rt={
      .rtm_family=AF_INET,
      .rtm_dst_len=32,
      .rtm_src_len=0,
      .rtm_tos=0,
      .rtm_table=RT_TABLE_MAIN,
      .rtm_protocol=RTPROT_BOOT,
      .rtm_scope=RT_SCOPE_UNIVERSE,
      .rtm_type=RTN_UNICAST,
      .rtm_flags=0
    },
    .attrbuf={}
  };
  assert(NLMSG_DATA(&req.nh)==&req.rt);

  struct rtattr *rta=(struct rtattr *)((char*)&req.nh+NLMSG_LENGTH(sizeof(struct rtmsg)));
  assert(RTM_RTA(NLMSG_DATA(&req.nh))==rta);
  int len=sizeof(Req)-NLMSG_LENGTH(sizeof(struct rtmsg));

  attr(&req.nh,rta,RTA_DST,"7.7.7.7");
  rta=RTA_NEXT(rta,len);
  attr(&req.nh,rta,RTA_GATEWAY,"192.168.1.1");
  rta=RTA_NEXT(rta,len);
  attr(&req.nh,rta,RTA_OIF,&((int){3}));

  assert(sizeof(Req)==send(fd,&req,sizeof(Req),0));

  receive();
  ack();
  clearbuf();

}

void ask(){
  // rtnetlink(3)
  typedef struct {
    struct nlmsghdr nh;
    struct rtmsg    rt;
  } Req;
  assert(NLMSG_LENGTH(sizeof(struct rtmsg))==sizeof(Req));
  assert(sizeof(Req)==send(fd,&(Req){
    .nh={
      .nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)), // netlink(3)
      .nlmsg_type = RTM_GETROUTE, // rtnetlink(7)
      .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP, /*NLM_F_ACK*/ /*NLM_F_ECHO*/
      .nlmsg_seq=0,
      .nlmsg_pid=0
    },
    .rt={
      .rtm_family=AF_INET,
      .rtm_dst_len=0,
      .rtm_src_len=0,
      .rtm_tos=0,
      .rtm_table=RT_TABLE_MAIN,
      .rtm_protocol=RTPROT_UNSPEC,
      .rtm_scope=RT_SCOPE_UNIVERSE,
      .rtm_type=RTN_UNSPEC,
      .rtm_flags=0
    }
  },sizeof(Req),0));
}

void show(){

  ask();
  receive();

  // netlink(7)
  struct nlmsghdr *nh=(struct nlmsghdr*)recvbuf;
  for(;NLMSG_OK(nh, len);nh=NLMSG_NEXT(nh, len)){

    if(nh->nlmsg_type==NLMSG_DONE)
      break;

    assert(nh->nlmsg_type==RTM_NEWROUTE);
    assert(nh->nlmsg_pid==(unsigned)getpid());
    assert(nh->nlmsg_flags==NLM_F_MULTI);

    const struct rtmsg *rtm=(struct rtmsg*)NLMSG_DATA(nh);
    if(rtm->rtm_table!=RT_TABLE_MAIN){
      assert(rtm->rtm_table==RT_TABLE_LOCAL);
      printf("[local] (skipped)\n");
      continue;
    }
    printf("[main] ");
    assert(rtm->rtm_family==AF_INET);
    printf("/%u ",rtm->rtm_dst_len);
    assert(rtm->rtm_src_len==0);
    assert(rtm->rtm_tos==0);
    // printf("%u ",rtm->rtm_protocol);
    if(rtm->rtm_protocol==RTPROT_DHCP)
      printf("dhcp ");
    else if(rtm->rtm_protocol==RTPROT_BOOT)
      printf("boot ");
    if(rtm->rtm_scope==RT_SCOPE_LINK)
      printf("link ");
    else if(rtm->rtm_scope==RT_SCOPE_UNIVERSE)
      printf("universe ");
    else
      assert(false);
    assert(rtm->rtm_type==RTN_UNICAST);
    assert(rtm->rtm_flags==0);

    printf("||| ");

    struct rtattr *p=(struct rtattr*)RTM_RTA(rtm);
    assert(p==(struct rtattr*)((char*)nh+NLMSG_LENGTH(sizeof(struct rtmsg))));
    // assert(RTM_RTA(rtm)==p);

    int rtl = RTM_PAYLOAD(nh);

    for(;RTA_OK(p, rtl);p=RTA_NEXT(p,rtl)){
      char s[INET_ADDRSTRLEN]={};
      switch(p->rta_type){
        case RTA_DST:
          assert(((struct sockaddr_in*)RTA_DATA(p))->sin_addr.s_addr!=INADDR_ANY);
          assert(s==inet_ntop(AF_INET,RTA_DATA(p),s,INET_ADDRSTRLEN));
          printf("dst %s ",s);
          break;
          // if(((struct sockaddr_in*)RTA_DATA(p))->sin_addr.s_addr==INADDR_ANY){
          //   printf("default ");
          // }else{
          //   assert(s==inet_ntop(AF_INET,RTA_DATA(p),s,INET_ADDRSTRLEN));
          //   printf("dst %s ",s);
          // }
          // break;
        case RTA_OIF:
          printf("dev %d ",*((int*)RTA_DATA(p)));
          break;
        case RTA_GATEWAY:
          assert(s==inet_ntop(AF_INET,RTA_DATA(p),s,INET_ADDRSTRLEN));
          printf("gw %s ",s);
          break;
        case RTA_PRIORITY:
          printf("pri %d ",*((int*)RTA_DATA(p)));
          break;
        case RTA_PREFSRC:
          assert(s==inet_ntop(AF_INET,RTA_DATA(p),s,INET_ADDRSTRLEN));
          printf("prefsrc %s ",s);
          break;
        case RTA_TABLE:
          assert(RT_TABLE_MAIN==*((int*)RTA_DATA(p)));
          break;
        default:
          assert(false);
          // printf("[type %u] ",p->rta_type);
          break;
      }
    }

    printf("\n");

  }

  clearbuf();

}

void getgw(){

  ask();
  receive();

  struct nlmsghdr *nh=(struct nlmsghdr*)recvbuf;
  for(;NLMSG_OK(nh, len);nh=NLMSG_NEXT(nh, len)){

    assert(
      // nh->nlmsg_len
      nh->nlmsg_type==RTM_NEWROUTE &&
      nh->nlmsg_flags==NLM_F_MULTI &&
      nh->nlmsg_seq==0 &&
      nh->nlmsg_pid==(unsigned)getpid()
    );

    const struct rtmsg *rtm=(struct rtmsg*)NLMSG_DATA(nh);
    if(
      rtm->rtm_dst_len!=0 ||
      rtm->rtm_table!=RT_TABLE_MAIN ||
      rtm->rtm_scope!=RT_SCOPE_UNIVERSE
    ){
      assert(rtm->rtm_dst_len==24);
      assert(rtm->rtm_table==RT_TABLE_LOCAL);
      assert(rtm->rtm_scope==RT_SCOPE_LINK);
      continue;
    }
    assert(
      rtm->rtm_family==AF_INET &&
      // rtm_dst_len
      rtm->rtm_src_len==0 &&
      rtm->rtm_tos==0 &&
      // rtm_table
      (rtm->rtm_protocol==RTPROT_DHCP || rtm->rtm_protocol==RTPROT_BOOT) &&
      // rtm_scope
      rtm->rtm_type==RTN_UNICAST &&
      rtm->rtm_flags==0
    );

    struct rtattr *rta=(struct rtattr*)RTM_RTA(rtm);
    assert(rta==(struct rtattr*)((char*)nh+NLMSG_LENGTH(sizeof(struct rtmsg))));
    int rtl=RTM_PAYLOAD(nh);
    for(;RTA_OK(rta, rtl);rta=RTA_NEXT(rta,rtl)){
      switch(rta->rta_type){
        case RTA_GATEWAY:
          gw=*(struct in_addr*)RTA_DATA(rta);
          break;
        case RTA_OIF:
          assert(3==*((int*)RTA_DATA(rta)));
          break;
        case RTA_PREFSRC:
          break;
        case RTA_PRIORITY:
          assert(303==*((int*)RTA_DATA(rta)));
          break;
        case RTA_TABLE:
          assert(RT_TABLE_MAIN==*((int*)RTA_DATA(rta)));
          break;
        default:
          assert(false);
          break;
      }

    }

    clearbuf();
    return;

  }

}

int main(){

  init();

  show();

  getgw();
  char s[INET_ADDRSTRLEN]={};
  inet_ntop(AF_INET,&gw,s,INET_ADDRSTRLEN);
  printf("%s\n",s);

  show();

  end();

}

