#include "pegasus_protocol.h"
#include <string.h>
uint8_t protocol_crc(const char *text, uint8_t length){
    uint8_t crc=0;
    while(length--){
        crc ^= (uint8_t)*text++;
        for(uint8_t i=0;i<8;i++)crc = (crc & 128) ? (uint8_t)((crc<<1)^7) : (uint8_t)(crc<<1);
    }
    return crc;
}
static int8_t hex(char c){
    if(c>='0' && c<='9')return c-'0';
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
uint8_t protocol_decode(char *f, uint8_t *seq, char **payload){
    size_t n=strlen(f);
    if(n<9 || n>=FRAME_MAX || f[0]!='@' || f[n-1]!='\n' || f[n-4]!='*')return 0;
    for(size_t i=1;i<n-4;i++)if(f[i]<33 || f[i]>126 || f[i]=='@' || f[i]=='*')return 0;
    int8_t a=hex(f[n-3]), b=hex(f[n-2]);
    if(a<0 || b<0 || protocol_crc(f+1,n-5)!=(uint8_t)((a<<4)|b))return 0;
    uint16_t number=0;
    char *p=f+1;
    if(*p<'0' || *p>'9')return 0;
    uint8_t digits=0;
    while(*p>='0' && *p<='9'){
        if(++digits>3)return 0;
        number=number*10+(*p++-'0');
    }
    if(number>255 || *p++!=',' || p>=f+n-4)return 0;
    f[n-4]=0; *seq=number; *payload=p;
    return 1;
}
