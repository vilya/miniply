// Copyright 2019 Vilya Harvey
#include "miniply.h"

#include <cstdio>
#include <cstring>
#include <string>


static const char* kFileTypes[] = {
  "ascii",
  "binary_little_endian",
  "binary_big_endian",
};
static const char* kPropertyTypes[] = {
  "char",
  "uchar",
  "short",
  "ushort",
  "int",
  "uint",
  "float",
  "double",
};

int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <ply-file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  miniply::PLYReader reader(argv[1]);
  if (!reader.valid()) {
    fprintf(stderr, "Failed to open %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  printf("ply\n");
  printf("format %s %d.%d\n", kFileTypes[int(reader.file_type())], reader.version_major(), reader.version_minor());
  for (uint32_t i = 0, endI = reader.num_elements(); i < endI; i++) {
    const miniply::PLYElement* elem = reader.get_element(i);
    printf("element %s %u\n", elem->name.c_str(), elem->count);
    for (const miniply::PLYProperty& prop : elem->properties) {
      if (prop.countType != miniply::PLYPropertyType::None) {
        printf("property list %s %s %s\n", kPropertyTypes[uint32_t(prop.countType)], kPropertyTypes[uint32_t(prop.type)], prop.name.c_str());
      }
      else {
        printf("property %s %s\n", kPropertyTypes[uint32_t(prop.type)], prop.name.c_str());
      }
    }
  }
  printf("end_header\n");

  return EXIT_SUCCESS;
}
