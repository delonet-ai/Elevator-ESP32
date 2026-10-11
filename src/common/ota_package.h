#pragma once
#include <stdint.h>
#include <string.h>
struct OtaHeader {uint8_t magic[8],role,reserved[3];uint32_t size;uint8_t sha256[32];};
static_assert(sizeof(OtaHeader)==48,"OTA package layout");
inline bool otaHeaderValid(const OtaHeader &h,uint8_t role,uint32_t capacity){
  return !memcmp(h.magic,"LIFTOTA1",8)&&h.role==role&&!h.reserved[0]&&!h.reserved[1]&&!h.reserved[2]&&h.size>=24&&h.size<=capacity;
}
