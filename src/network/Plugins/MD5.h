#ifndef PLUGINS_MD5_H
#define PLUGINS_MD5_H

namespace Quazal {

    class MD5 {
    public:
        MD5();
        void init();
        void update(const unsigned char *, unsigned int);
        void finalize();
        void transform(const unsigned char *);

    private:
        static void encode(unsigned char *, const unsigned int *, unsigned int);
        static void decode(unsigned int *, const unsigned char *, unsigned int);

        static void
        FF(unsigned int &,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int);
        static void
        GG(unsigned int &,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int);
        static void
        HH(unsigned int &,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int);
        static void
        II(unsigned int &,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int,
           unsigned int);

        unsigned int state[4];
        unsigned int count[2];
        unsigned char buffer[64];
        unsigned char digest[16];
        bool finalized;
    };

}

#endif
