/*
MIT License

Copyright (c) 2019 Vilya Harvey

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "miniply.h"

#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef _WIN32
#include <errno.h>
#endif


namespace miniply {

  //
  // PLY constants
  //

  static constexpr uint32_t kPLYReadBufferSize = 128 * 1024;

  static const char* kPLYFileTypes[] = { "ascii", "binary_little_endian", "binary_big_endian", nullptr };
  static const uint32_t kPLYPropertySize[]= { 1, 1, 2, 2, 4, 4, 4, 8 };

  struct PLYTypeAlias {
    const char* name;
    PLYPropertyType type;
  };

  static const PLYTypeAlias kTypeAliases[] = {
    { "char",   PLYPropertyType::Char   },
    { "uchar",  PLYPropertyType::UChar  },
    { "short",  PLYPropertyType::Short  },
    { "ushort", PLYPropertyType::UShort },
    { "int",    PLYPropertyType::Int    },
    { "uint",   PLYPropertyType::UInt   },
    { "float",  PLYPropertyType::Float  },
    { "double", PLYPropertyType::Double },

    { "uint8",  PLYPropertyType::UChar  },
    { "uint16", PLYPropertyType::UShort },
    { "uint32", PLYPropertyType::UInt   },

    { "int8",   PLYPropertyType::Char   },
    { "int16",  PLYPropertyType::Short  },
    { "int32",  PLYPropertyType::Int    },

    { nullptr,  PLYPropertyType::None   }
  };


  //
  // Constants
  //

  static constexpr double kDoubleDigits[10] = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };

  static constexpr float kPi = 3.14159265358979323846f;



  //
  // Vec2 type
  //

  struct Vec2 {
    float x, y;
  };

  static inline Vec2 operator - (Vec2 lhs, Vec2 rhs) { return Vec2{ lhs.x - rhs.x, lhs.y - rhs.y }; }

  static inline float dot(Vec2 lhs, Vec2 rhs) { return lhs.x * rhs.x + lhs.y * rhs.y; }
  static inline float length(Vec2 v) { return std::sqrt(dot(v, v)); }
  static inline Vec2 normalize(Vec2 v) { float len = length(v); return Vec2{ v.x / len, v.y / len }; }


  //
  // Vec3 type
  //

  struct Vec3 {
    float x, y, z;
  };

  static inline Vec3 operator - (Vec3 lhs, Vec3 rhs) { return Vec3{ lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }

  static inline float dot(Vec3 lhs, Vec3 rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z; }
  static inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
  static inline Vec3 normalize(Vec3 v) { float len = length(v); return Vec3{ v.x / len, v.y / len, v.z / len }; }
  static inline Vec3 cross(Vec3 lhs, Vec3 rhs) { return Vec3{ lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x }; }


  //
  // Internal-only functions
  //

  static inline bool is_whitespace(char ch)
  {
    return ch == ' ' || ch == '\t' || ch == '\r';
  }


  static inline bool is_digit(char ch)
  {
    return ch >= '0' && ch <= '9';
  }


  static inline bool is_letter(char ch)
  {
    ch |= 32; // upper and lower case letters differ only at this bit.
    return ch >= 'a' && ch <= 'z';
  }


  static inline bool is_alnum(char ch)
  {
    return is_digit(ch) || is_letter(ch);
  }


  static inline bool is_keyword_start(char ch)
  {
    return is_letter(ch) || ch == '_';
  }


  static inline bool is_keyword_part(char ch)
  {
    return is_alnum(ch) || ch == '_';
  }


  static inline bool is_safe_buffer_end(char ch)
  {
    return (ch > 0 && ch <= 32) || (ch >= 127);
  }


  static int file_open(FILE** f, const char* filename, const char* mode)
  {
  #ifdef _WIN32
    return fopen_s(f, filename, mode);
  #else
    *f = fopen(filename, mode);
    return (*f != nullptr) ? 0 : errno;
  #endif
  }


  static inline int64_t file_pos(FILE* file)
  {
  #ifdef _WIN32
    return _ftelli64(file);
  #else
    static_assert(sizeof(off_t) == sizeof(int64_t), "off_t is not 64 bits.");
    return ftello(file);
  #endif
  }


  static inline int file_seek(FILE* file, int64_t offset, int origin)
  {
  #ifdef _WIN32
    return _fseeki64(file, offset, origin);
  #else
    static_assert(sizeof(off_t) == sizeof(int64_t), "off_t is not 64 bits.");
    return fseeko(file, offset, origin);
  #endif
  }


  static bool int_literal(const char* start, char const** end, int* val)
  {
    const char* pos = start;

    bool negative = false;
    if (*pos == '-') {
      negative = true;
      ++pos;
    }
    else if (*pos == '+') {
      ++pos;
    }

    bool hasLeadingZeroes = *pos == '0';
    if (hasLeadingZeroes) {
      do {
        ++pos;
      } while (*pos == '0');
    }

    int numDigits = 0;
    int localVal = 0;
    while (is_digit(*pos)) {
      // FIXME: this will overflow if we get too many digits.
      localVal = localVal * 10 + static_cast<int>(*pos - '0');
      ++numDigits;
      ++pos;
    }

    if (numDigits == 0 && hasLeadingZeroes) {
      numDigits = 1;
    }

    if (numDigits == 0 || is_letter(*pos) || *pos == '_') {
      return false;
    }
    else if (numDigits > 10) {
      // Overflow, literal value is larger than an int can hold.
      // FIXME: this won't catch *all* cases of overflow, make it exact.
      return false;
    }

    if (val != nullptr) {
      *val = negative ? -localVal : localVal;
    }
    if (end != nullptr) {
      *end = pos;
    }
    return true;
  }


  static bool double_literal(const char* start, char const** end, double* val)
  {
    const char* pos = start;

    bool negative = false;
    if (*pos == '-') {
      negative = true;
      ++pos;
    }
    else if (*pos == '+') {
      ++pos;
    }

    double localVal = 0.0;

    bool hasIntDigits = is_digit(*pos);
    if (hasIntDigits) {
      do {
        localVal = localVal * 10.0 + kDoubleDigits[*pos - '0'];
        ++pos;
      } while (is_digit(*pos));
    }
    else if (*pos != '.') {
//      set_error("Not a floating point number");
      return false;
    }

    bool hasFracDigits = false;
    if (*pos == '.') {
      ++pos;
      hasFracDigits = is_digit(*pos);
      if (hasFracDigits) {
        double scale = 0.1;
        do {
          localVal += scale * kDoubleDigits[*pos - '0'];
          scale *= 0.1;
          ++pos;
        } while (is_digit(*pos));
      }
      else if (!hasIntDigits) {
//        set_error("Floating point number has no digits before or after the decimal point");
        return false;
      }
    }

    bool hasExponent = *pos == 'e' || *pos == 'E';
    if (hasExponent) {
      ++pos;
      bool negativeExponent = false;
      if (*pos == '-') {
        negativeExponent = true;
        ++pos;
      }
      else if (*pos == '+') {
        ++pos;
      }

      if (!is_digit(*pos)) {
//        set_error("Floating point exponent has no digits");
        return false; // error: exponent part has no digits.
      }

      double exponent = 0.0;
      do {
        exponent = exponent * 10.0 + kDoubleDigits[*pos - '0'];
        ++pos;
      } while (is_digit(*pos));

      if (val != nullptr) {
        if (negativeExponent) {
          exponent = -exponent;
        }
        localVal *= std::pow(10.0, exponent);
      }
    }

    if (*pos == '.' || *pos == '_' || is_alnum(*pos)) {
//      set_error("Floating point number has trailing chars");
      return false;
    }

    if (negative) {
      localVal = -localVal;
    }

    if (val != nullptr) {
      *val = localVal;
    }
    if (end != nullptr) {
      *end = pos;
    }
    return true;
  }


  static bool float_literal(const char* start, char const** end, float* val)
  {
    double tmp = 0.0;
    bool ok = double_literal(start, end, &tmp);
    if (ok && val != nullptr) {
      *val = static_cast<float>(tmp);
    }
    return ok;
  }


  static void endian_swap_2(uint8_t* data)
  {
    uint8_t tmp = data[0];
    data[0] = data[1];
    data[1] = tmp;
  }


  static void endian_swap_4(uint8_t* data)
  {
    uint8_t tmp = data[0];
    data[0] = data[3];
    data[3] = tmp;
    tmp = data[1];
    data[1] = data[2];
    data[2] = tmp;
  }


  static void endian_swap_8(uint8_t* data)
  {
    uint8_t tmp[8];
    data[0] = tmp[7];
    data[1] = tmp[6];
    data[2] = tmp[5];
    data[3] = tmp[4];
    data[4] = tmp[3];
    data[5] = tmp[2];
    data[6] = tmp[1];
    data[7] = tmp[0];
    std::memcpy(data, tmp, 8);
  }


  //
  // PLYElement methods
  //

  uint32_t PLYElement::find_property(const char *propName) const
  {
    for (uint32_t i = 0, endI = uint32_t(properties.size()); i < endI; i++) {
      if (strcmp(propName, properties.at(i).name.c_str()) == 0) {
        return i;
      }
    }
    return kInvalidIndex;
  }


  //
  // PLYReader methods
  //

  PLYReader::PLYReader(const char* filename)
  {
    m_buf = new char[kPLYReadBufferSize + 1];
    m_buf[kPLYReadBufferSize] = '\0';

    m_bufEnd = m_buf + kPLYReadBufferSize;
    m_pos = m_bufEnd;
    m_end = m_bufEnd;

    if (file_open(&m_f, filename, "rb") != 0) {
      m_f = nullptr;
      m_valid = false;
      return;
    }
    m_valid = true;

    refill_buffer();

    m_valid = keyword("ply") && next_line() &&
              keyword("format") && advance() &&
              typed_which(kPLYFileTypes, &m_fileType) && advance() &&
              int_literal(&m_majorVersion) && advance() &&
              match(".") && advance() &&
              int_literal(&m_minorVersion) && next_line() &&
              parse_elements() &&
              keyword("end_header") && advance() && match("\n") && accept();
    if (!m_valid) {
      return;
    }
    m_inDataSection = true;
    if (m_fileType == PLYFileType::ASCII) {
      advance();
    }

    for (PLYElement& elem : m_elements) {
      setup_element(elem);
    }
  }


  PLYReader::~PLYReader()
  {
    if (m_f != nullptr) {
      fclose(m_f);
    }
    delete[] m_buf;
  }


  bool PLYReader::valid() const
  {
    return m_valid;
  }


  bool PLYReader::has_element() const
  {
    return m_valid && m_currentElement < m_elements.size();
  }


  const PLYElement* PLYReader::element() const
  {
    assert(has_element());
    return &m_elements[m_currentElement];
  }


  bool PLYReader::load_element()
  {
    assert(has_element());
    if (m_elementLoaded) {
      return true;
    }

    PLYElement& elem = m_elements[m_currentElement];
    return elem.fixedSize ? load_fixed_size_element(elem) : load_variable_size_element(elem);
  }


  void PLYReader::next_element()
  {
    if (!has_element()) {
      return;
    }

    // If the element was loaded, the read buffer should already be positioned at
    // the start of the next element.
    PLYElement& elem = m_elements[m_currentElement];
    m_currentElement++;

    if (m_elementLoaded) {
      // Clear any temporary storage used for list properties in the current element.
      for (PLYProperty& prop : elem.properties) {
        if (prop.countType == PLYPropertyType::None) {
          continue;
        }
        prop.listData.clear();
        prop.listData.shrink_to_fit();
        prop.rowCount.clear();
        prop.rowCount.shrink_to_fit();
        prop.rowStart.clear();
        prop.rowStart.shrink_to_fit();
      }

      // Clear temporary storage for the non-list properties in the current element.
      m_elementData.clear();
      m_elementLoaded = false;
      return;
    }

    // If the element wasn't loaded, we have to move the file pointer past its
    // contents. How we do that depends on whether this is an ASCII or binary
    // file and, if it's a binary, whether the element is fixed or variable
    // size.
    if (m_fileType == PLYFileType::ASCII) {
      for (uint32_t row = 0; row < elem.count; row++) {
        next_line();
      }
    }
    else if (elem.fixedSize) {
      int64_t elementStart = static_cast<int64_t>(m_pos - m_buf);
      int64_t elementSize = elem.rowStride * elem.count;
      int64_t elementEnd = elementStart + elementSize;
      if (elementEnd >= kPLYReadBufferSize) {
        m_bufOffset += elementEnd;
        file_seek(m_f, m_bufOffset, SEEK_SET);
        m_bufEnd = m_buf + kPLYReadBufferSize;
        m_pos = m_bufEnd;
        m_end = m_bufEnd;
        refill_buffer();
      }
      else {
        m_pos = m_buf + elementEnd;
        m_end = m_pos;
      }
    }
    else if (m_fileType == PLYFileType::Binary) {
      for (uint32_t row = 0; row < elem.count; row++) {
        for (const PLYProperty& prop : elem.properties) {
          if (prop.countType == PLYPropertyType::None) {
            uint32_t numBytes = kPLYPropertySize[uint32_t(prop.type)];
            if (m_pos + numBytes > m_bufEnd) {
              if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
                m_valid = false;
                return;
              }
            }
            m_pos += numBytes;
            m_end = m_pos;
            continue;
          }

          uint32_t numBytes = kPLYPropertySize[uint32_t(prop.countType)];
          if (m_pos + numBytes > m_bufEnd) {
            if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
              m_valid = false;
              return;
            }
          }

          int count = 0;
          switch (prop.countType) {
          case PLYPropertyType::Char:
            count = static_cast<int>(*reinterpret_cast<const int8_t*>(m_pos));
            break;
          case PLYPropertyType::UChar:
            count = static_cast<int>(*reinterpret_cast<const uint8_t*>(m_pos));
            break;
          case PLYPropertyType::Short:
            count = static_cast<int>(*reinterpret_cast<const int16_t*>(m_pos));
            break;
          case PLYPropertyType::UShort:
            count = static_cast<int>(*reinterpret_cast<const uint16_t*>(m_pos));
            break;
          case PLYPropertyType::Int:
            count = *reinterpret_cast<const int*>(m_pos);
            break;
          case PLYPropertyType::UInt:
            count = static_cast<int>(*reinterpret_cast<const uint32_t*>(m_pos));
            break;
          default:
            m_valid = false;
            return;
          }

          if (count < 0) {
            m_valid = false;
            return;
          }

          numBytes += uint32_t(count) * kPLYPropertySize[uint32_t(prop.type)];
          if (m_pos + numBytes > m_bufEnd) {
            if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
              m_valid = false;
              return;
            }
          }
          m_pos += numBytes;
          m_end = m_pos;
        }
      }
    }
    else { // PLYFileType::BinaryBigEndian
      for (uint32_t row = 0; row < elem.count; row++) {
        for (const PLYProperty& prop : elem.properties) {
          if (prop.countType == PLYPropertyType::None) {
            uint32_t numBytes = kPLYPropertySize[uint32_t(prop.type)];
            if (m_pos + numBytes > m_bufEnd) {
              if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
                m_valid = false;
                return;
              }
            }
            m_pos += numBytes;
            m_end = m_pos;
            continue;
          }

          uint32_t numBytes = kPLYPropertySize[uint32_t(prop.countType)];
          if (m_pos + numBytes > m_bufEnd) {
            if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
              m_valid = false;
              return;
            }
          }

          uint8_t tmp[8];
          memcpy(tmp, m_pos, numBytes);
          switch (numBytes) {
          case 2:
            endian_swap_2(tmp);
            break;
          case 4:
            endian_swap_4(tmp);
            break;
          case 8:
            endian_swap_8(tmp);
            break;
          default:
            break;
          }

          int count = 0;
          switch (prop.countType) {
          case PLYPropertyType::Char:
            count = static_cast<int>(*reinterpret_cast<const int8_t*>(tmp));
            break;
          case PLYPropertyType::UChar:
            count = static_cast<int>(*reinterpret_cast<const uint8_t*>(tmp));
            break;
          case PLYPropertyType::Short:
            count = static_cast<int>(*reinterpret_cast<const int16_t*>(tmp));
            break;
          case PLYPropertyType::UShort:
            count = static_cast<int>(*reinterpret_cast<const uint16_t*>(tmp));
            break;
          case PLYPropertyType::Int:
            count = *reinterpret_cast<const int*>(tmp);
            break;
          case PLYPropertyType::UInt:
            count = static_cast<int>(*reinterpret_cast<const uint32_t*>(tmp));
            break;
          default:
            m_valid = false;
            return;
          }

          if (count < 0) {
            m_valid = false;
            return;
          }

          numBytes += uint32_t(count) * kPLYPropertySize[uint32_t(prop.type)];
          if (m_pos + numBytes > m_bufEnd) {
            if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
              m_valid = false;
              return;
            }
          }

          m_pos += numBytes;
          m_end = m_pos;
        }
      }
    }
  }


  bool PLYReader::has_vec2(const char* xname, const char* yname) const
  {
    assert(has_element());
    const PLYElement* elem = element();
    return (elem->find_property(xname) != kInvalidIndex) &&
           (elem->find_property(yname) != kInvalidIndex);
  }


  bool PLYReader::has_vec3(const char* xname, const char* yname, const char* zname) const
  {
    assert(has_element());
    const PLYElement* elem = element();
    return (elem->find_property(xname) != kInvalidIndex) &&
           (elem->find_property(yname) != kInvalidIndex) &&
           (elem->find_property(zname) != kInvalidIndex);
  }


  static float to_float(PLYPropertyType type, const uint8_t* tmp)
  {
    switch (type) {
    case PLYPropertyType::Char:
      return static_cast<float>(*reinterpret_cast<const int8_t*>(tmp));
    case PLYPropertyType::UChar:
      return static_cast<float>(*reinterpret_cast<const uint8_t*>(tmp));
    case PLYPropertyType::Short:
      return static_cast<float>(*reinterpret_cast<const int16_t*>(tmp));
    case PLYPropertyType::UShort:
      return static_cast<float>(*reinterpret_cast<const uint16_t*>(tmp));
    case PLYPropertyType::Int:
      return static_cast<float>(*reinterpret_cast<const int*>(tmp));
    case PLYPropertyType::UInt:
      return static_cast<float>(*reinterpret_cast<const uint32_t*>(tmp));
    case PLYPropertyType::Float:
      return *reinterpret_cast<const float*>(tmp);
    case PLYPropertyType::Double:
      return static_cast<float>(*reinterpret_cast<const double*>(tmp));
    default:
      return 0.0f;
    }
  }


  static int to_int(PLYPropertyType type, const uint8_t* tmp)
  {
    switch (type) {
    case PLYPropertyType::Char:
      return static_cast<int>(*reinterpret_cast<const int8_t*>(tmp));
    case PLYPropertyType::UChar:
      return static_cast<int>(*reinterpret_cast<const uint8_t*>(tmp));
    case PLYPropertyType::Short:
      return static_cast<int>(*reinterpret_cast<const int16_t*>(tmp));
    case PLYPropertyType::UShort:
      return static_cast<int>(*reinterpret_cast<const uint16_t*>(tmp));
    case PLYPropertyType::Int:
      return *reinterpret_cast<const int*>(tmp);
    case PLYPropertyType::UInt:
      return static_cast<int>(*reinterpret_cast<const uint32_t*>(tmp));
    case PLYPropertyType::Float:
      return static_cast<int>(*reinterpret_cast<const float*>(tmp));
    case PLYPropertyType::Double:
      return static_cast<int>(*reinterpret_cast<const double*>(tmp));
    default:
      return 0;
    }
  }


  bool PLYReader::extract_vec2(const char* xname, const char* yname, float* dest) const
  {
    assert(has_element());

    const PLYElement* elem = element();
    uint32_t xidx = elem->find_property(xname);
    uint32_t yidx = elem->find_property(yname);
    if (xidx == kInvalidIndex || yidx == kInvalidIndex) {
      return false;
    }

    const PLYProperty& x = elem->properties[xidx];
    const PLYProperty& y = elem->properties[yidx];
    if (x.countType != PLYPropertyType::None || y.countType != PLYPropertyType::None) {
      return false;
    }

    // In order from fastest to slowest:
    // 1. if x and y are contiguous floats and are the only properties in this element, use a single memcpy.
    // 2. if x and y are contiguous floats, do 1 memcpy per row.
    // 3. if x and y are both floats, do 2 memcpys per row.
    // 4. if x and y are not both floats, then do 2 type conversions and assignments per row
    if (x.type == PLYPropertyType::Float &&
        y.type == PLYPropertyType::Float) {
      // x and y are both floats, could be any of cases 1, 2 or 3.
      if (y.offset == (x.offset + sizeof(float))) {
        // x and y are contiguous floats, could be either case 1 or case 2
        if (elem->properties.size() == 2) {
          // case 1
          std::memcpy(dest, m_elementData.data(), sizeof(float) * elem->count * 2);
        }
        else {
          // case 2
          const uint8_t* src = m_elementData.data() + x.offset;
          const uint8_t* srcEnd = m_elementData.data() + m_elementData.size();
          for (; src < srcEnd; src += elem->rowStride) {
            std::memcpy(dest, src, sizeof(float) * 2);
            dest += 2;
          }
        }
      }
      else {
        // x and y are not contiguous --> case 3
        const uint8_t* row = m_elementData.data();
        const uint8_t* rowEnd = m_elementData.data() + m_elementData.size();
        for (; row < rowEnd; row += elem->rowStride) {
          dest[0] = *reinterpret_cast<const float*>(row + x.offset);
          dest[1] = *reinterpret_cast<const float*>(row + y.offset);
          dest += 2;
        }
      }
    }
    else {
      // either x, y or both are not floats --> case 4
      const uint8_t* row = m_elementData.data();
      const uint8_t* rowEnd = m_elementData.data() + m_elementData.size();
      for (; row < rowEnd; row += elem->rowStride) {
        dest[0] = to_float(x.type, row + x.offset);
        dest[1] = to_float(y.type, row + y.offset);
        dest += 2;
      }
    }
    return true;
  }


  bool PLYReader::extract_vec3(const char* xname, const char* yname, const char* zname, float* dest) const
  {
    assert(has_element());

    const PLYElement* elem = element();
    uint32_t xidx = elem->find_property(xname);
    uint32_t yidx = elem->find_property(yname);
    uint32_t zidx = elem->find_property(zname);
    if (xidx == kInvalidIndex || yidx == kInvalidIndex || zidx == kInvalidIndex) {
      return false;
    }

    const PLYProperty& x = elem->properties[xidx];
    const PLYProperty& y = elem->properties[yidx];
    const PLYProperty& z = elem->properties[zidx];
    if (x.countType != PLYPropertyType::None || y.countType != PLYPropertyType::None || z.countType != PLYPropertyType::None) {
      return false;
    }

    // In order from fastest to slowest:
    // 1. if xyz are contiguous floats and are the only properties in this element, use a single memcpy.
    // 2. if xyz are contiguous floats, do 1 memcpy per row.
    // 3. if xyz are all floats, do 3 memcpys per row.
    // 4. if xyz are not all floats, then do 3 type conversions and assignments per row
    if (x.type == PLYPropertyType::Float &&
        y.type == PLYPropertyType::Float &&
        z.type == PLYPropertyType::Float) {
      // xyz are all floats, could be any of cases 1, 2 or 3.
      if (y.offset == (x.offset + sizeof(float)) && z.offset == (y.offset + sizeof(float))) {
        // xyz are contiguous floats, could be either case 1 or case 2
        if (elem->properties.size() == 3) {
          // case 1
          std::memcpy(dest, m_elementData.data(), sizeof(float) * elem->count * 3);
        }
        else {
          // case 2
          const uint8_t* src = m_elementData.data() + x.offset;
          const uint8_t* srcEnd = m_elementData.data() + m_elementData.size();
          for (; src < srcEnd; src += elem->rowStride) {
            std::memcpy(dest, src, sizeof(float) * 3);
            dest += 3;
          }
        }
      }
      else {
        // x and y are not contiguous --> case 3
        const uint8_t* row = m_elementData.data();
        const uint8_t* rowEnd = m_elementData.data() + m_elementData.size();
        for (; row < rowEnd; row += elem->rowStride) {
          dest[0] = *reinterpret_cast<const float*>(row + x.offset);
          dest[1] = *reinterpret_cast<const float*>(row + y.offset);
          dest[2] = *reinterpret_cast<const float*>(row + z.offset);
          dest += 3;
        }
      }
    }
    else {
      // either x, y or both are not floats --> case 4
      const uint8_t* row = m_elementData.data();
      const uint8_t* rowEnd = m_elementData.data() + m_elementData.size();
      for (; row < rowEnd; row += elem->rowStride) {
        dest[0] = to_float(x.type, row + x.offset);
        dest[1] = to_float(y.type, row + y.offset);
        dest[1] = to_float(z.type, row + z.offset);
        dest += 3;
      }
    }
    return true;
  }


  uint32_t PLYReader::count_triangles(const char* propName) const
  {
    // Find the indices property.
    const PLYElement* elem = element();
    uint32_t indicesIdx = elem->find_property(propName);
    if (indicesIdx == kInvalidIndex) {
      return 0; // missing indices property
    }

    const PLYProperty& faces = elem->properties[indicesIdx];
    if (faces.countType == PLYPropertyType::None) {
      return 0; // invalid indices property, should be a list
    }

    // Count the number of triangles in the mesh.
    uint32_t numTriangles = 0;
    for (uint32_t i = 0; i < elem->count; i++) {
      if (faces.rowCount[i] >= 3) {
        numTriangles += faces.rowCount[i] - 2;
      }
    }

    return numTriangles;
  }


  bool PLYReader::all_faces_are_triangles(const char *propName) const
  {
    // Find the indices property.
    const PLYElement* elem = element();
    uint32_t indicesIdx = elem->find_property(propName);
    if (indicesIdx == kInvalidIndex) {
      return 0; // missing indices property
    }

    const PLYProperty& faces = elem->properties[indicesIdx];
    if (faces.countType == PLYPropertyType::None) {
      return 0; // invalid indices property, should be a list
    }

    // Count the number of triangles in the mesh.
    for (uint32_t i = 0; i < elem->count; i++) {
      if (faces.rowCount[i] != 3) {
        return false;
      }
    }

    return true;
  }


  // We assume that `indices` has already been allocated, with enough space to
  // hold all triangles.
  //
  // If there are any invalid indices on a polygon (i.e. where idx < 0 or
  // idx >= numVerts) then that face will be skipped.
  bool PLYReader::extract_triangles(const char* propname, const float pos[], uint32_t numVerts, int indices[]) const
  {
    // Find the indices property.
    const PLYElement* elem = element();
    uint32_t indicesIdx = elem->find_property(propname);
    if (indicesIdx == kInvalidIndex) {
      return false; // missing indices property
    }

    const PLYProperty& faces = elem->properties[indicesIdx];
    if (faces.countType == PLYPropertyType::None) {
      return false; // invalid indices property, should be a list
    }

    // Count the number of triangles in the mesh.
    uint32_t numTriangles = count_triangles(propname);
    if (numTriangles == 0) {
      return false; // can't have a mesh with 0 triangles!
    }

    // Allocate storage for the indices.
    uint32_t numIndices = numTriangles * 3;

    // Fill in the indices. From fastest to slowest:
    // 1. If all faces are triangles and the indices have type Int or UInt, memcpy them all in one go.
    // 2. If all faces are triangles and the indices are some other type, do 3 type conversions and assignments per face.
    // 3. If there are some non-triangle faces and the indices are Int or UInt, memcpy contiguous runs of triangles & triang
    if (all_faces_are_triangles(propname)) {
      if (faces.type == PLYPropertyType::Int || faces.type == PLYPropertyType::UInt) {
        // All faces are triangles and have a type compatible with trimesh indices.
        std::memcpy(indices, faces.listData.data(), sizeof(uint32_t) * numIndices);
      }
      else {
        // All faces are triangles but the indices require type conversion.
        const uint8_t* src = faces.listData.data();
        const size_t srcIndexBytes = kPLYPropertySize[uint32_t(faces.type)];
        for (uint32_t i = 0; i < numIndices; i++) {
          indices[i] = to_int(faces.type, src);
          src += srcIndexBytes;
        }
      }
    }
    else {
      if (faces.type == PLYPropertyType::Int || faces.type == PLYPropertyType::UInt) {
        // Some faces are not triangles but the indices do not require type conversion.
        // If we find a contiguous run of triangles, we can memcpy them all in one go.
        // Faces with a different vertex count have to be handled as they occur.
        int* dst = indices;
        uint32_t triStart = 0;
        bool wasTri = false;
        for (uint32_t i = 0; i < elem->count; i++) {
          uint32_t faceVerts = faces.rowCount[i];
          if (faceVerts == 3) {
            if (!wasTri) {
              triStart = i;
            }
            wasTri = true;
          }
          else {
            // If we've come to the end of a run of triangles, copy them all over.
            if (wasTri) {
              uint32_t numInts = (i - triStart) * 3;
              std::memcpy(dst, faces.listData.data() + faces.rowStart[triStart], numInts * sizeof(int));
              dst += numInts;
            }
            wasTri = false;

            if (faceVerts >= 4) {
              const int* src = reinterpret_cast<const int*>(faces.listData.data() + faces.rowStart[i]);
              uint32_t numTrisAdded = triangulate_polygon(faceVerts, pos, numVerts, src, dst);
              dst += numTrisAdded * 3;
            }
            else {
              // Face is degenerate (less than 3 verts) so we ignore it.
              continue;
            }
          }
        }
        // If there is a run of triangles at the end of the faces list,
        // make sure they're copied too.
        if (wasTri) {
          uint32_t numInts = (elem->count - triStart) * 3;
          std::memcpy(dst, faces.listData.data() + faces.rowStart[triStart], numInts * sizeof(int));
        }
      }
      else {
        // Some faces are not triangles and the indices require type conversion.
        const size_t srcIndexBytes = kPLYPropertySize[uint32_t(faces.type)];
        int* dst = indices;
        std::vector<int> tmp;
        tmp.reserve(32);
        for (uint32_t i = 0; i < elem->count; i++) {
          uint32_t faceVerts = faces.rowCount[i];
          if (faceVerts < 3) {
            continue;
          }
          const uint8_t* src = faces.listData.data() + faces.rowStart[i];
          tmp.clear();
          for (uint32_t v = 0; v < faceVerts; v++) {
            tmp.push_back(to_int(faces.type, src));
            src += srcIndexBytes;
          }

          if (faceVerts == 3) {
            std::memcpy(dst, tmp.data(), sizeof(int) * 3);
            dst += 3;
          }
          else {
            uint32_t numTrisAdded = triangulate_polygon(faceVerts, pos, numVerts, tmp.data(), dst);
            dst += numTrisAdded * 3;
          }
        }
      }
    }

    return true;
  }


  //
  // PLYReader private methods
  //

  bool PLYReader::refill_buffer()
  {
    if (m_f == nullptr || m_atEOF) {
      // Nothing left to read.
      return false;
    }

    if (m_pos == m_buf && m_end == m_bufEnd) {
      // Can't make any more room in the buffer!
      return false;
    }

    // Move everything from the start of the current token onwards, to the
    // start of the read buffer.
    int64_t bufSize = static_cast<int64_t>(m_bufEnd - m_buf);
    if (bufSize < kPLYReadBufferSize) {
      m_buf[bufSize] = m_buf[kPLYReadBufferSize];
      m_buf[kPLYReadBufferSize] = '\0';
      m_bufEnd = m_buf + kPLYReadBufferSize;
    }
    size_t keep = static_cast<size_t>(m_bufEnd - m_pos);
    if (keep > 0 && m_pos > m_buf) {
      std::memmove(m_buf, m_pos, sizeof(char) * keep);
      m_bufOffset += static_cast<int64_t>(m_pos - m_buf);
    }
    m_end = m_buf + (m_end - m_pos);
    m_pos = m_buf;

    // Fill the remaining space in the buffer with data from the file.
    size_t fetched = fread(m_buf + keep, sizeof(char), kPLYReadBufferSize - keep, m_f) + keep;
    m_atEOF = fetched < kPLYReadBufferSize;
    m_bufEnd = m_buf + fetched;

    if (!m_inDataSection || m_fileType == PLYFileType::ASCII) {
      return rewind_to_safe_char();
    }
    return true;
  }


  bool PLYReader::rewind_to_safe_char()
  {
    // If it looks like a token might run past the end of this buffer, move
    // the buffer end pointer back before it & rewind the file. This way the
    // next refill will pick up the whole of the token.
    if (!m_atEOF && (m_bufEnd[-1] == '\n' || !is_safe_buffer_end(m_bufEnd[-1]))) {
      const char* safe = m_bufEnd - 2;
      // If '\n' is the last char in the buffer, then a call to `next_line()`
      // will move `m_pos` to point at the null terminator but won't refresh
      // the buffer. It would be clearer to fix this in `next_line()` but I
      // believe it'll be more performant to simply treat `\n` as an unsafe
      // character here.
      while (safe >= m_end && (*safe == '\n' || !is_safe_buffer_end(*safe))) {
        --safe;
      }
      if (safe < m_end) {
        // No safe places to rewind to in the whole buffer!
        return false;
      }
      ++safe;
      m_buf[kPLYReadBufferSize] = *safe;
      m_bufEnd = safe;
    }
    m_buf[m_bufEnd - m_buf] = '\0';

    return true;
  }


  bool PLYReader::accept()
  {
    m_pos = m_end;
    return true;
  }


  // Advances to end of line or to next non-whitespace char.
  bool PLYReader::advance()
  {
    m_pos = m_end;
    while (true) {
      while (is_whitespace(*m_pos)) {
        ++m_pos;
      }
      if (m_pos == m_bufEnd) {
        m_end = m_pos;
        if (refill_buffer()) {
          continue;
        }
        return false;
      }
      break;
    }
    m_end = m_pos;
    return true;
  }


  bool PLYReader::next_line()
  {
    m_pos = m_end;
    do {
      while (*m_pos != '\n') {
        if (m_pos == m_bufEnd) {
          m_end = m_pos;
          if (refill_buffer()) {
            continue;
          }
          return false;
        }
        ++m_pos;
      }
      ++m_pos; // move past the newline char
      m_end = m_pos;
    } while (match("comment"));

    return true;
  }


  bool PLYReader::match(const char* str)
  {
    m_end = m_pos;
    while (m_end < m_bufEnd && *str != '\0' && *m_end == *str) {
      ++m_end;
      ++str;
    }
    if (*str != '\0') {
      return false;
    }
    return true;
  }


  bool PLYReader::which(const char* values[], uint32_t* index)
  {
    for (uint32_t i = 0; values[i] != nullptr; i++) {
      if (keyword(values[i])) {
        *index = i;
        return true;
      }
    }
    return false;
  }


  bool PLYReader::which_property_type(PLYPropertyType* type)
  {
    for (uint32_t i = 0; kTypeAliases[i].name != nullptr; i++) {
      if (keyword(kTypeAliases[i].name)) {
        *type = kTypeAliases[i].type;
        return true;
      }
    }
    return false;
  }


  bool PLYReader::keyword(const char* kw)
  {
    return match(kw) && !is_keyword_part(*m_end);
  }


  bool PLYReader::identifier(char* dest, size_t destLen)
  {
    m_end = m_pos;
    if (!is_keyword_start(*m_end) || destLen == 0) {
      return false;
    }
    do {
      ++m_end;
    } while (is_keyword_part(*m_end));

    size_t len = static_cast<size_t>(m_end - m_pos);
    if (len >= destLen) {
      return false; // identifier too large for dest!
    }
    std::memcpy(dest, m_pos, sizeof(char) * len);
    dest[len] = '\0';
    return true;
  }


  bool PLYReader::int_literal(int* value)
  {
    return miniply::int_literal(m_pos, &m_end, value);
  }


  bool PLYReader::float_literal(float* value)
  {
    return miniply::float_literal(m_pos, &m_end, value);
  }


  bool PLYReader::double_literal(double* value)
  {
    return miniply::double_literal(m_pos, &m_end, value);
  }


  bool PLYReader::parse_elements()
  {
    m_elements.reserve(4);
    while (m_valid && keyword("element")) {
      parse_element();
    }
    return true;
  }


  bool PLYReader::parse_element()
  {
    char name[256];
    int count = 0;

    m_valid = keyword("element") && advance() &&
              identifier(name, sizeof(name)) && advance() &&
              int_literal(&count) && next_line();
    if (!m_valid || count < 0) {
      return false;
    }

    m_elements.push_back(PLYElement());
    PLYElement& elem = m_elements.back();
    elem.name = name;
    elem.count = static_cast<uint32_t>(count);
    elem.properties.reserve(10);

    while (m_valid && keyword("property")) {
      parse_property(elem.properties);
    }

    return true;
  }


  bool PLYReader::parse_property(std::vector<PLYProperty>& properties)
  {
    char name[256];
    PLYPropertyType type      = PLYPropertyType::None;
    PLYPropertyType countType = PLYPropertyType::None;

    m_valid = keyword("property") && advance();
    if (!m_valid) {
      return false;
    }

    if (keyword("list")) {
      // This is a list property
      m_valid = advance() && which_property_type(&countType) && advance();
      if (!m_valid) {
        return false;
      }
    }

    m_valid = which_property_type(&type) && advance() &&
              identifier(name, sizeof(name)) && next_line();
    if (!m_valid) {
      return false;
    }

    properties.push_back(PLYProperty());
    PLYProperty& prop = properties.back();
    prop.name = name;
    prop.type = type;
    prop.countType = countType;

    return true;
  }


  void PLYReader::setup_element(PLYElement& elem)
  {
    for (PLYProperty& prop : elem.properties) {
      if (prop.countType != PLYPropertyType::None) {
        elem.fixedSize = false;
      }
    }

    // Note that each list property gets its own separate storage. Only fixed
    // size properties go into the common data block. The `rowStride` is the
    // size of a row in the common data block.

    for (PLYProperty& prop : elem.properties) {
      if (prop.countType != PLYPropertyType::None) {
        continue;
      }
      prop.offset = elem.rowStride;
      elem.rowStride += kPLYPropertySize[uint32_t(prop.type)];
    }
  }


  bool PLYReader::load_fixed_size_element(PLYElement& elem)
  {
    size_t numBytes = elem.count * elem.rowStride;

    m_elementData.resize(numBytes);

    if (m_fileType == PLYFileType::ASCII) {
      size_t back = 0;

      for (uint32_t row = 0; row < elem.count; row++) {
        for (PLYProperty& prop : elem.properties) {
          if (!load_ascii_scalar_property(prop, back)) {
            m_valid = false;
            return false;
          }
        }
        next_line();
      }
    }
    else {
      uint8_t* dst = m_elementData.data();
      uint8_t* dstEnd = dst + numBytes;
      while (dst < dstEnd) {
        size_t bytesAvailable = static_cast<size_t>(m_bufEnd - m_pos);
        if (dst + bytesAvailable > dstEnd) {
          bytesAvailable = static_cast<size_t>(dstEnd - dst);
        }
        std::memcpy(dst, m_pos, bytesAvailable);
        m_pos += bytesAvailable;
        m_end = m_pos;
        dst += bytesAvailable;
        if (!refill_buffer()) {
          break;
        }
      }
      if (dst < dstEnd) {
        m_valid = false;
        return false;
      }

      // We assume the CPU is little endian, so if the file is big-endian we
      // need to do an endianness swap on every data item in the block.
      if (m_fileType == PLYFileType::BinaryBigEndian) {
        uint8_t* data = m_elementData.data();
        for (uint32_t row = 0; row < elem.count; row++) {
          for (PLYProperty& prop : elem.properties) {
            size_t numBytes = kPLYPropertySize[uint32_t(prop.type)];
            switch (numBytes) {
            case 2:
              endian_swap_2(data);
              break;
            case 4:
              endian_swap_4(data);
              break;
            case 8:
              endian_swap_8(data);
              break;
            default:
              break;
            }
            data += numBytes;
          }
        }
      }
    }

    m_elementLoaded = true;
    return true;
  }


  bool PLYReader::load_variable_size_element(PLYElement& elem)
  {
    m_elementData.resize(elem.count * elem.rowStride);

    if (m_fileType == PLYFileType::Binary) {
      size_t back = 0;
      for (uint32_t row = 0; row < elem.count; row++) {
        for (PLYProperty& prop : elem.properties) {
          if (prop.countType == PLYPropertyType::None) {
            m_valid = load_binary_scalar_property(prop, back);
          }
          else {
            load_binary_list_property(prop);
          }
        }
      }
    }
    else if (m_fileType == PLYFileType::ASCII) {
      size_t back = 0;
      for (uint32_t row = 0; row < elem.count; row++) {
        for (PLYProperty& prop : elem.properties) {
          if (prop.countType == PLYPropertyType::None) {
            m_valid = load_ascii_scalar_property(prop, back);
          }
          else {
            load_ascii_list_property(prop);
          }
        }
        next_line();
      }
    }
    else { // m_fileType == PLYFileType::BinaryBigEndian
      size_t back = 0;
      for (uint32_t row = 0; row < elem.count; row++) {
        for (PLYProperty& prop : elem.properties) {
          if (prop.countType == PLYPropertyType::None) {
            m_valid = load_binary_scalar_property_big_endian(prop, back);
          }
          else {
            load_binary_list_property_big_endian(prop);
          }
        }
      }
    }

    m_elementLoaded = true;
    return true;
  }


  bool PLYReader::load_ascii_scalar_property(PLYProperty& prop, size_t& destIndex)
  {
    uint8_t value[8];
    if (!ascii_value(prop.type, value)) {
      return false;
    }

    size_t numBytes = kPLYPropertySize[uint32_t(prop.type)];
    std::memcpy(m_elementData.data() + destIndex, value, numBytes);
    destIndex += numBytes;
    return true;
  }


  bool PLYReader::load_ascii_list_property(PLYProperty& prop)
  {
    int count = 0;
    m_valid = (prop.countType < PLYPropertyType::Float) && int_literal(&count) && advance() && (count >= 0);
    if (!m_valid) {
      return false;
    }

    const size_t numBytes = kPLYPropertySize[uint32_t(prop.type)];

    size_t back = prop.listData.size();
    prop.rowStart.push_back(static_cast<uint32_t>(back));
    prop.rowCount.push_back(static_cast<uint32_t>(count));
    prop.listData.resize(back + numBytes * size_t(count));

    for (uint32_t i = 0; i < uint32_t(count); i++) {
      if (!ascii_value(prop.type, prop.listData.data() + back)) {
        m_valid = false;
        return false;
      }
      back += numBytes;
    }

    return true;
  }


  bool PLYReader::load_binary_scalar_property(PLYProperty& prop, size_t& destIndex)
  {
    size_t numBytes = kPLYPropertySize[uint32_t(prop.type)];
    if (m_pos + numBytes > m_bufEnd) {
      if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
        m_valid = false;
        return false;
      }
    }
    std::memcpy(m_elementData.data() + destIndex, m_pos, numBytes);
    m_pos += numBytes;
    m_end = m_pos;
    destIndex += numBytes;
    return true;
  }


  bool PLYReader::load_binary_list_property(PLYProperty& prop)
  {
    size_t countBytes = kPLYPropertySize[uint32_t(prop.countType)];
    if (m_pos + countBytes > m_bufEnd) {
      if (!refill_buffer() || m_pos + countBytes > m_bufEnd) {
        m_valid = false;
        return false;
      }
    }

    uint8_t tmp[8];
    std::memcpy(tmp, m_pos, countBytes);

    int count = 0;
    switch (prop.countType) {
    case PLYPropertyType::Char:
      count = static_cast<int>(*reinterpret_cast<int8_t*>(tmp));
      break;
    case PLYPropertyType::UChar:
      count = static_cast<int>(*reinterpret_cast<uint8_t*>(tmp));
      break;
    case PLYPropertyType::Short:
      count = static_cast<int>(*reinterpret_cast<int16_t*>(tmp));
      break;
    case PLYPropertyType::UShort:
      count = static_cast<int>(*reinterpret_cast<uint16_t*>(tmp));
      break;
    case PLYPropertyType::Int:
      count = *reinterpret_cast<int*>(tmp);
      break;
    case PLYPropertyType::UInt:
      count = static_cast<int>(*reinterpret_cast<uint32_t*>(tmp));
      break;
    default:
      m_valid = false;
      return false;
    }

    if (count < 0) {
      m_valid = false;
      return false;
    }

    m_pos += countBytes;
    m_end = m_pos;

    const size_t numBytes = kPLYPropertySize[uint32_t(prop.type)] * uint32_t(count);
    if (m_pos + numBytes > m_bufEnd) {
      if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
        m_valid = false;
        return false;
      }
    }
    size_t back = prop.listData.size();
    prop.rowStart.push_back(static_cast<uint32_t>(back));
    prop.rowCount.push_back(static_cast<uint32_t>(count));
    prop.listData.resize(back + numBytes);
    std::memcpy(prop.listData.data() + back, m_pos, numBytes);

    m_pos += numBytes;
    m_end = m_pos;
    return true;
  }


  bool PLYReader::load_binary_scalar_property_big_endian(PLYProperty &prop, size_t &destIndex)
  {
    size_t startIndex = destIndex;
    if (load_binary_scalar_property(prop, destIndex)) {
      switch (kPLYPropertySize[uint32_t(prop.type)]) {
      case 2:
        endian_swap_2(m_elementData.data() + startIndex);
        break;
      case 4:
        endian_swap_4(m_elementData.data() + startIndex);
        break;
      case 8:
        endian_swap_8(m_elementData.data() + startIndex);
        break;
      default:
        break;
      }
      return true;
    }
    else {
      return false;
    }
  }


  bool PLYReader::load_binary_list_property_big_endian(PLYProperty &prop)
  {
    size_t countBytes = kPLYPropertySize[uint32_t(prop.countType)];
    if (m_pos + countBytes > m_bufEnd) {
      if (!refill_buffer() || m_pos + countBytes > m_bufEnd) {
        m_valid = false;
        return false;
      }
    }

    uint8_t tmp[8];
    std::memcpy(tmp, m_pos, countBytes);

    int count = 0;
    switch (prop.countType) {
    case PLYPropertyType::Char:
      count = static_cast<int>(*reinterpret_cast<int8_t*>(tmp));
      break;
    case PLYPropertyType::UChar:
      count = static_cast<int>(*reinterpret_cast<uint8_t*>(tmp));
      break;
    case PLYPropertyType::Short:
      endian_swap_2(tmp);
      count = static_cast<int>(*reinterpret_cast<int16_t*>(tmp));
      break;
    case PLYPropertyType::UShort:
      endian_swap_2(tmp);
      count = static_cast<int>(*reinterpret_cast<uint16_t*>(tmp));
      break;
    case PLYPropertyType::Int:
      endian_swap_4(tmp);
      count = *reinterpret_cast<int*>(tmp);
      break;
    case PLYPropertyType::UInt:
      endian_swap_4(tmp);
      count = static_cast<int>(*reinterpret_cast<uint32_t*>(tmp));
      break;
    default:
      m_valid = false;
      return false;
    }

    if (count < 0) {
      m_valid = false;
      return false;
    }

    m_pos += countBytes;
    m_end = m_pos;

    const size_t numBytes = kPLYPropertySize[uint32_t(prop.type)] * uint32_t(count);
    if (m_pos + numBytes > m_bufEnd) {
      if (!refill_buffer() || m_pos + numBytes > m_bufEnd) {
        m_valid = false;
        return false;
      }
    }
    size_t back = prop.listData.size();
    prop.rowStart.push_back(static_cast<uint32_t>(back));
    prop.rowCount.push_back(static_cast<uint32_t>(count));
    prop.listData.resize(back + numBytes * size_t(count));
    std::memcpy(prop.listData.data() + back, m_pos, numBytes);

    const uint8_t* listEnd = prop.listData.data() + prop.listData.size();
    uint8_t* listPos = prop.listData.data() + back;
    switch (numBytes) {
    case 2:
      for (; listPos < listEnd; listPos += numBytes) {
        endian_swap_2(listPos);
      }
      break;
    case 4:
      for (; listPos < listEnd; listPos += numBytes) {
        endian_swap_4(listPos);
      }
      break;
    case 8:
      for (; listPos < listEnd; listPos += numBytes) {
        endian_swap_8(listPos);
      }
      break;
    default:
      break;
    }

    m_pos += numBytes;
    m_end = m_pos;
    return true;
  }


  bool PLYReader::ascii_value(PLYPropertyType propType, uint8_t value[8])
  {
    int tmpInt = 0;

    switch (propType) {
    case PLYPropertyType::Char:
    case PLYPropertyType::UChar:
    case PLYPropertyType::Short:
    case PLYPropertyType::UShort:
      m_valid = int_literal(&tmpInt);
      break;
    case PLYPropertyType::Int:
    case PLYPropertyType::UInt:
      m_valid = int_literal(reinterpret_cast<int*>(value));
      break;
    case PLYPropertyType::Float:
      m_valid = float_literal(reinterpret_cast<float*>(value));
      break;
    case PLYPropertyType::Double:
    default:
      m_valid = double_literal(reinterpret_cast<double*>(value));
      break;
    }

    if (!m_valid) {
      return false;
    }
    advance();

    switch (propType) {
    case PLYPropertyType::Char:
      reinterpret_cast<int8_t*>(value)[0] = static_cast<int8_t>(tmpInt);
      break;
    case PLYPropertyType::UChar:
      value[0] = static_cast<uint8_t>(tmpInt);
      break;
    case PLYPropertyType::Short:
      reinterpret_cast<int16_t*>(value)[0] = static_cast<int16_t>(tmpInt);
      break;
    case PLYPropertyType::UShort:
      reinterpret_cast<uint16_t*>(value)[0] = static_cast<uint16_t>(tmpInt);
      break;
    default:
      break;
    }
    return true;
  }


  //
  // Polygon triangulation
  //

  static float angle_at_vert(uint32_t idx,
                             const std::vector<Vec2>& points2D,
                             const std::vector<uint32_t>& prev,
                             const std::vector<uint32_t>& next)
  {
    Vec2 xaxis = normalize(points2D[next[idx]] - points2D[idx]);
    Vec2 yaxis = Vec2{-xaxis.y, xaxis.x};
    Vec2 p2p0 = points2D[prev[idx]] - points2D[idx];
    float angle = std::atan2(dot(p2p0, yaxis), dot(p2p0, xaxis));
    if (angle <= 0.0f || angle >= kPi) {
      angle = 10000.0f;
    }
    return angle;
  }


  uint32_t triangulate_polygon(uint32_t n, const float pos[], uint32_t numVerts, const int indices[], int dst[])
  {
    if (n < 3) {
      return 0;
    }
    else if (n == 3) {
      dst[0] = indices[0];
      dst[1] = indices[1];
      dst[2] = indices[2];
      return 1;
    }
    else if (n == 4) {
      dst[0] = indices[0];
      dst[1] = indices[1];
      dst[2] = indices[3];

      dst[3] = indices[2];
      dst[4] = indices[3];
      dst[5] = indices[1];
      return 2;
    }

    // Check that all indices for this face are in the valid range before we
    // try to dereference them.
    for (uint32_t i = 0; i < n; i++) {
      if (indices[i] < 0 || uint32_t(indices[i]) >= numVerts) {
        return 0;
      }
    }

    const Vec3* vpos = reinterpret_cast<const Vec3*>(pos);

    // Calculate the geometric normal of the face
    Vec3 origin = vpos[indices[0]];
    Vec3 faceU = normalize(vpos[indices[1]] - origin);
    Vec3 faceNormal = normalize(cross(faceU, normalize(vpos[indices[n - 1]] - origin)));
    Vec3 faceV = normalize(cross(faceNormal, faceU));

    // Project the faces points onto the plane perpendicular to the normal.
    std::vector<Vec2> points2D(n, Vec2{0.0f, 0.0f});
    for (uint32_t i = 1; i < n; i++) {
      Vec3 p = vpos[indices[i]] - origin;
      points2D[i] = Vec2{dot(p, faceU), dot(p, faceV)};
    }

    std::vector<uint32_t> next(n, 0u);
    std::vector<uint32_t> prev(n, 0u);
    uint32_t first = 0;
    for (uint32_t i = 0, j = n - 1; i < n; i++) {
      next[j] = i;
      prev[i] = j;
      j = i;
    }

    // Do ear clipping.
    while (n > 3) {
      // Find the (remaining) vertex with the sharpest angle.
      uint32_t bestI = first;
      float bestAngle = angle_at_vert(first, points2D, prev, next);
      for (uint32_t i = next[first]; i != first; i = next[i]) {
        float angle = angle_at_vert(i, points2D, prev, next);
        if (angle < bestAngle) {
          bestI = i;
          bestAngle = angle;
        }
      }

      // Clip the triangle at bestI.
      uint32_t nextI = next[bestI];
      uint32_t prevI = prev[bestI];

      dst[0] = indices[bestI];
      dst[1] = indices[nextI];
      dst[2] = indices[prevI];
      dst += 3;

      if (bestI == first) {
        first = nextI;
      }
      next[prevI] = nextI;
      prev[nextI] = prevI;
      --n;
    }

    // Add the final triangle.
    dst[0] = indices[first];
    dst[1] = indices[next[first]];
    dst[2] = indices[prev[first]];

    return n - 2;
  }

} // namespace minipbrt
