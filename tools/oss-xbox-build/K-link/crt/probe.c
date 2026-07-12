/* Lane-C link-probe: references a spread of crt symbols + provides DllMain,
 * so the linker must resolve everything from crt.obj alone. */
typedef unsigned int   u32;
typedef unsigned short u16;

extern int   sprintf(char*, const char*, ...);
extern int   sscanf(const char*, const char*, ...);
extern void* memset(void*, int, u32);
extern void* memcpy(void*, const void*, u32);
extern char* strncpy(char*, const char*, u32);
extern char* strchr(const char*, int);
extern char* strrchr(const char*, int);
extern char* strstr(const char*, const char*);
extern int   atoi(const char*);
extern double atof(const char*);
extern int   isspace(int);
extern int   isxdigit(int);
extern int   tolower(int);
extern u16*  wcscat(u16*, const u16*);
extern u32   wcstombs(char*, const u16*, u32);
extern void* malloc(u32);

/* satisfy crt's _DllMainCRTStartup forward */
int DllMain(void* h, unsigned long r, void* v){ (void)h;(void)r;(void)v; return 1; }

/* many locals -> forces the compiler to emit __savegprlr_/__restgprlr_ calls */
int probe(const char* in)
{
    char buf[128];
    int a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p;
    u16 w1[8]={0}, w2[4]={0};
    void* mem = malloc(64);
    a=atoi(in); b=isspace((int)in[0]); c=isxdigit((int)in[1]); d=tolower((int)in[2]);
    e=(int)atof(in); f=(int)(strchr(in,'/')!=0); g=(int)(strrchr(in,'.')!=0);
    h=(int)(strstr(in,"song")!=0);
    memset(buf,0,sizeof(buf)); memcpy(buf,in,4); strncpy(buf,in,8);
    wcscat(w1,w2); i=(int)wcstombs(buf,w1,4);
    sprintf(buf,"%i %08x %s %p %.2f %lld %c",a,(u32)b,in,mem,3.5,(long long)e,'Z');
    j=sscanf(in,"%hu.%hu.%hu.%hu",&w1[0],&w1[1],&w1[2],&w1[3]);
    k=a+b+c+d+e+f+g+h+i+j; l=k*2; m=l+1; n=m-1; o=n^k; p=o+(int)(long)mem;
    return p;
}
