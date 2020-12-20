

#define USE_LAYER_2

#ifdef USE_LAYER_2

#ifndef LAYER2_H_INCLUDED
#define LAYER2_H_INCLUDED


struct al_table2 
{
  short bits;
  short d;
};



void init_layer2(void);
#endif

#endif

