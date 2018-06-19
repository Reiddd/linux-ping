#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ICMP_SIZE      (sizeof(struct icmp))
#define ICMP_ECHO      0
#define ICMP_ECHOREPLY 0
#define BUF_SIZE       1024
#define SEND_TIMES     5

/*
    some structs used here:

        struct sockaddr_in {                // can be cast to sockaddr
            sa_family_t     sin_family;     // address family
            uint16_t        sin_port;
            struct in_addr  sin_addr;
            char            sin_zero[8];    // currently no use
        };
    
    --------------

        struct in_addr {
            in_addr_t       s_addr;         // 32 bits address
        };

    --------------

        struct sockaddr {
            sa_family_t     in_family;
            char            sa_data[14];    // IP + port
        };
    
    --------------

        struct timeval {
            long int        tv_sec;         // second
            long int        tv_usec;        // micro second
        };

    --------------

        struct timezone {
            int             tz_minuteswest;
            int             tz_dsttime;
        };

    --------------

        struct hostent {
            char*           h_name;
            char**          h_aliases;
            short           h_addrtype;
            short           h_length;
            char**          h_addr_list;    // elems in the list could be cast to (struct in_addr *)
        };
*/

struct icmp {                   // could be cast to (unsigned short *)
    unsigned char   type;       // request: 0, response: 8
    unsigned char   code;       // request: 0, response: 0
    unsigned short  checksum;
    unsigned short  id;         // use pid here to identify each package
    unsigned short  sequence;
    struct timeval  timestamp;
};

/*
    __BYTE_ORDER: 
        __LITTLE_ENDIAN 
        __BIG_ENDIAN
    different CPU handle data bytes in different orders. some handle low bytes first, some handle high bytes first.
    the __BYTE_ORDER specified by the Internet is __BIG_ENDIAN.

    to transform between host byte order and network byte order, C offers these APIs:
        unsigned short htons(unsigned short);
        unsigned long  htonl(unsigned long);
        unsigned short ntohs(unsigned short);
        unsigned long  ntohl(unsigned long);
*/
struct ip {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char   hlen:4;             // head length, no need of a whole byte, 4 bits is enough
    unsigned char   version:4;
#else
    unsigned char   version:4;
    unsigned char   hlen:4;
#endif
    unsigned char   tos;
    unsigned short  len;
    unsigned short  id;
    unsigned short  offset;
    unsigned char   ttl;
    unsigned char   protocol;
    unsigned short  checksum;
    struct in_addr  ipsrc;
    struct in_addr  ipdst;
};

char buf[BUF_SIZE] = { 0 };


/* S -------------- function declaration -------------- */

unsigned short check_sum(unsigned short*, int);                     // calculate the checksum
float          timediff (struct timeval*, struct timeval*);         // calculate the time duration
void           pack     (struct icmp*   , int);                     // pack an ICMP message
int            unpack   (char*          , int             , char*); // unpack an ICMP message

/* E -------------- function declaration -------------- */
/* S -------------- function implements -------------- */

unsigned short check_sum(unsigned short* addr, int len)
{
/*
    1. 把校验和字段置为0
    2. 对需要校验的数据看成以16bit为单位的数字组成，依次进行二进制求和
    3. 将上一步的求和结果取反，存入校验和字段
*/
    unsigned int sum = 0;

    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(unsigned char *)addr;
    }

    sum  = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    return (unsigned short)~sum;
}


float timediff(struct timeval* begin, struct timeval* end)
{
    int n = (end->tv_sec - begin->tv_sec) * 1000000   +   (end->tv_usec - begin->tv_usec);

    return (float)(n / 1000);
}


void pack(struct icmp* icmp, int sequence)
{
/*
    gettimeofday(struct timeval* tv, struct timezone* tz);
        put current time and time zone into tv, tz
*/
    icmp->type     = ICMP_ECHO;
    icmp->code     = 0;
    icmp->checksum = 0;
    icmp->id       = getpid();
    icmp->sequence = sequence; // nsent
    icmp->checksum = check_sum((unsigned short *)icmp, ICMP_SIZE);
    gettimeofday(&icmp->timestamp, 0);
}


int unpack(char* buf, int len, char* addr)
{
    int             i, ipheadlen;
    struct ip*      ip;
    struct icmp*    icmp;
    float           rtt;            // round-trip time
    struct timeval  end;            // the timestamp receiving the message

    ip         = (struct ip *)buf;
    ipheadlen  = ip->hlen << 2;
    icmp       = (struct icmp *)(buf + ipheadlen);
    len       -= ipheadlen;

    if (len < 8) {
        printf("ICMP packages\' length is less than 8 \n");
        return -1;
    }

    if (icmp->type != ICMP_ECHOREPLY || icmp->id != getpid()) {
        printf("received ICMP packages are sent by others \n");
        return -1;
    }

    gettimeofday(&end, 0);
    rtt = timediff(&icmp->timestamp, &end);

    printf("%d bytes from %s : icmp_seq = %u  ttl = %d  rtt = %fms \n",
           len,
           addr,
           icmp->sequence,
           ip->ttl,
           rtt);
    
    return 0;
}

/* E -------------- function implements -------------- */


int main(int argc, char** argv)
{
    struct sockaddr_in  from;
    struct sockaddr_in  to;

    memset(&from, 0, sizeof(struct sockaddr_in));
    memset(&to  , 0, sizeof(struct sockaddr_in));


    // check the number of arguments
    if (argc < 2) {
        printf("use: %s hostname/IP address \n", argv[0]);
        exit(1);
    }


    // generate a raw socket
    /*
        int socket(int domain, int type, int protocol);
            type:
                SOCK_STREAM
                SOCK_DGRAM
                SOCK_SEQPACKET
                SOCK_RAW
                SOCK_RDM
                SOCK_PACKET

            return fd on success, otherwise return -1
    */
   int sockfd;
    if ( (sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
        printf("socket() error \n");
        exit(1);
    }


    // set dest-addr's address family
    to.sin_family = AF_INET;


    // check the argument is a host or an IP
    /*
        in_addr_t inet_addr(const char* );
            transfer IP "xxx.xxx.xxx.xxx" into network byte order. 
            return INADDR_NONE on failure.
        
        --------------

        struct hostent* gethostbyname(const char*);
            resolve the host and return a (struct hostent *) to describe it.
            return NULL on failure.
    */
   struct hostent* host;
   in_addr_t       inaddr;
    if ( (inaddr = inet_addr(argv[1])) == INADDR_NONE ) {        // host
        if ( (host = gethostbyname(argv[1])) == NULL) {
            printf("gethostbyname() error \n");
            exit(1);
        }
        to.sin_addr = *(struct in_addr*)host->h_addr_list[0];
    }
    else {                                                       // IP
        to.sin_addr.s_addr = inaddr;
    }


    // output
    /*
        char* inet_ntoa(struct in_addr);
            transfer the struct to a string in the form of "xxx.xxx.xxx.xxx"
    */
    printf("ping %s (%s) : %d bytes of data.\n", argv[1], inet_ntoa(to.sin_addr), (int)ICMP_SIZE);

    // loop to send and receive the ICMP message
    int nsent     = 0;
    int nreceived = 0;
    for (int i=0; i < SEND_TIMES; i++) {
        nsent++;
        struct icmp sendicmp;
        memset(&sendicmp, 0, ICMP_SIZE);
        pack(&sendicmp, nsent);


        // send a message
        /*
            int snedto(int sockfd  ,  const void* message  ,  int msg_len  ,  unsigned int flags  ,  const struct sockaddr* to  ,  int to_len);
                return -1 on failure.
                normally flags is set to 0.
        */
        if ( sendto(sockfd, &sendicmp, ICMP_SIZE, 0, (struct sockaddr *)&to, sizeof(to)) == -1) {
            printf("sendto() error \n");
            continue;
        }


        // receive a message
        /*
            int recvfrom(int sockfd  ,  void* buf  ,  int buf_size  ,  unsigned int flags  ,  (struct sockaddr *) from  ,  int* from_len);
                return a num less than 0 on failure.
                normally flags is set to 0.
        */
        int n;
        int fromlen = 0;
        if ( (n = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&from, &fromlen)) < 0) {
            printf("recvfrom() error \n");
        }
        nreceived++;
        if (unpack(buf, n, inet_ntoa(from.sin_addr)) == -1) {
            printf("unpack() error \n");
        }


        sleep(1);
    }


    printf("--- %s ping statistics ---\n", argv[1]);
    printf("%d packets transmitted, %d received, %d%% packets lost\n", 
           nsent, 
           nreceived, 
           (nsent - nreceived) * 100 / nsent );
    return 0;
}