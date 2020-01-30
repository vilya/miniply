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


static bool has_extension(const char* filename, const char* ext)
{
  int j = int(strlen(ext));
  int i = int(strlen(filename)) - j;
  if (i <= 0 || filename[i - 1] != '.') {
    return false;
  }
  return strcmp(filename + i, ext) == 0;
}


bool print_ply_header(const char* filename)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    fprintf(stderr, "Failed to open %s\n", filename);
    return false;
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

  while (reader.has_element()) {
    const miniply::PLYElement* elem = reader.element();
    if (elem->fixedSize || elem->count == 0) {
      reader.next_element();
      continue;
    }

    if (!reader.load_element()) {
      fprintf(stderr, "Element %s failed to load\n", elem->name.c_str());
    }
    for (const miniply::PLYProperty& prop : elem->properties) {
      if (prop.countType == miniply::PLYPropertyType::None) {
        continue;
      }
      bool mixedSize = false;
      const uint32_t firstRowCount = prop.rowCount.front();
      for (const uint32_t rowCount : prop.rowCount) {
        if (rowCount != firstRowCount) {
          mixedSize = true;
          break;
        }
      }
      if (mixedSize) {
        printf("Element '%s', property '%s': not all lists have the same size\n",
               elem->name.c_str(), prop.name.c_str());
      }
    }
    reader.next_element();
  }

  return true;
}


int main(int argc, char** argv)
{
  const int kFilenameBufferLen = 16 * 1024 - 1;
  char* filenameBuffer = new char[kFilenameBufferLen + 1];
  filenameBuffer[kFilenameBufferLen] = '\0';

  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++) {
    if (has_extension(argv[i], "txt")) {
      FILE* f = fopen(argv[i], "r");
      if (f != nullptr) {
        while (fgets(filenameBuffer, kFilenameBufferLen, f)) {
          filenames.push_back(filenameBuffer);
          while (filenames.back().back() == '\n') {
            filenames.back().pop_back();
          }
        }
        fclose(f);
      }
      else {
        fprintf(stderr, "Failed to open %s\n", argv[i]);
      }
    }
    else {
      filenames.push_back(argv[i]);
    }
  }

  if (filenames.empty()) {
    fprintf(stderr, "No input files provided.\n");
    return EXIT_SUCCESS;
  }
  else if (filenames.size() == 1) {
    return print_ply_header(filenames[0].c_str()) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  bool anyFailed = false;
  for (const std::string& filename : filenames) {
    printf("---- %s ----\n", filename.c_str());
    if (!print_ply_header(filename.c_str())) {
      anyFailed = true;
    }
    printf("\n");
  }
  return anyFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
