#ifndef REGISTRY_H
#define REGISTRY_H

// Used as reference:
// https://github.com/pspdev/pspsdk/blob/master/src/registry/pspreg.h

enum RegKeyTypes 
{
  REG_TYPE_DIR = 1,
  REG_TYPE_INT,
  REG_TYPE_STR,
  REG_TYPE_BIN
};

struct RegParam
{
  unsigned int regtype;  
  char name[256];
  unsigned int namelen;  
  unsigned int unknown2;     
  unsigned int unknown3;     
};

#endif
