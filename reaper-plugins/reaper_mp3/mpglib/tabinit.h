#ifndef MPGLIB_TABINIT_H_INCLUDED
#define MPGLIB_TABINIT_H_INCLUDED

extern real decwin[512+32];
extern "C" {
extern real *pnts[5];
}

void make_decode_tables(int scale);

#endif

