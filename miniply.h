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

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>


/// miniply - A simple and fast parser for PLY files
/// ================================================

namespace miniply {

  //
  // Constants
  //

  static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;


  //
  // PLY Parsing types
  //

  enum class PLYFileType {
    ASCII,
    Binary,
    BinaryBigEndian,
  };

  enum class PLYPropertyType {
    Char,
    UChar,
    Short,
    UShort,
    Int,
    UInt,
    Float,
    Double,

    None, //!< Special value used in Element::listCountType to indicate a non-list property.
  };

  struct PLYProperty {
    std::string name;
    PLYPropertyType type      = PLYPropertyType::None; //!< Type of the data. Must be set to a value other than None.
    PLYPropertyType countType = PLYPropertyType::None; //!< None indicates this is not a list type, otherwise it's the type for the list count.
    uint32_t offset           = 0;                  //!< Byte offset from the start of the row.
    uint32_t stride           = 0;

    std::vector<uint8_t> listData;
    std::vector<uint32_t> rowStart; // Entry `i` is the index in listData at which the data for row i starts.
    std::vector<uint32_t> rowCount; // Entry `i` is the number of items (*not* the number of bytes) in row `i`.
  };


  struct PLYElement {
    std::string              name;                 //!< Name of this element.
    std::vector<PLYProperty> properties;
    uint32_t                 count      = 0;       //!< The number of items in this element (e.g. the number of vertices if this is the vertex element).
    bool                     fixedSize  = true;    //!< `true` if there are only fixed-size properties in this element, i.e. no list properties.
    uint32_t                 rowStride  = 0;

    uint32_t find_property(const char* propName) const;
  };


  class PLYReader {
  public:
    PLYReader(const char* filename);
    ~PLYReader();

    bool valid() const;

    bool has_element() const;
    const PLYElement* element() const;
    bool load_element();
    void next_element();

    bool has_vec2(const char* xname, const char* yname) const;
    bool has_vec3(const char* xname, const char* yname, const char* zname) const;
    bool extract_vec2(const char* xname, const char* yname, float* dest) const;
    bool extract_vec3(const char* xname, const char* yname, const char* zname, float* dest) const;

    uint32_t count_triangles(const char* propName) const;
    bool all_faces_are_triangles(const char* propName) const;
    bool extract_triangles(const char* propname, const float pos[], uint32_t numVerts, int indices[]) const;

  private:
    bool refill_buffer();
    bool rewind_to_safe_char();
    bool accept();
    bool advance();
    bool next_line();
    bool match(const char* str);
    bool which(const char* values[], uint32_t* index);
    bool which_property_type(PLYPropertyType* type);
    bool keyword(const char* kw);
    bool identifier(char* dest, size_t destLen);

    template <class T> // T must be a type compatible with uint32_t.
    bool typed_which(const char* values[], T* index) {
      return which(values, reinterpret_cast<uint32_t*>(index));
    }

    bool int_literal(int* value);
    bool float_literal(float* value);
    bool double_literal(double* value);

    bool parse_elements();
    bool parse_element();
    bool parse_property(std::vector<PLYProperty>& properties);

    void setup_element(PLYElement& elem);

    bool load_fixed_size_element(PLYElement& elem);
    bool load_variable_size_element(PLYElement& elem);

    bool load_ascii_scalar_property(PLYProperty& prop, size_t& destIndex);
    bool load_ascii_list_property(PLYProperty& prop);
    bool load_binary_scalar_property(PLYProperty& prop, size_t& destIndex);
    bool load_binary_list_property(PLYProperty& prop);
    bool load_binary_scalar_property_big_endian(PLYProperty& prop, size_t& destIndex);
    bool load_binary_list_property_big_endian(PLYProperty& prop);

    bool ascii_value(PLYPropertyType propType, uint8_t value[8]);

  private:
    FILE* m_f             = nullptr;
    char* m_buf           = nullptr;
    const char* m_bufEnd  = nullptr;
    const char* m_pos     = nullptr;
    const char* m_end     = nullptr;
    bool m_inDataSection  = false;
    bool m_atEOF          = false;
    int64_t m_bufOffset   = 0;

    bool m_valid          = false;

    PLYFileType m_fileType = PLYFileType::ASCII; //!< Whether the file was ascii, binary little-endian, or binary big-endian.
    int m_majorVersion     = 0;
    int m_minorVersion     = 0;
    std::vector<PLYElement> m_elements;         //!< Element descriptors for this file.

    size_t m_currentElement = 0;
    bool m_elementLoaded    = false;
    std::vector<uint8_t> m_elementData;
  };


  /// Given a polygon with `n` vertices, where `n` > 3, triangulate it and
  /// store the indices for the resulting triangles in `dst`. The `pos`
  /// parameter is the array of all vertex positions for the mesh; `indices` is
  /// the list of `n` indices for the polygon we're triangulating; and `dst` is
  /// where we write the new indices to.
  ///
  /// The triangulation will always produce `n - 2` triangles, so `dst` must
  /// have enough space for `3 * (n - 2)` indices.
  ///
  /// If `n == 3`, we simply copy the input indices to `dst`. If `n < 3`,
  /// nothing gets written to dst.
  ///
  /// The return value is the number of triangles.
  uint32_t triangulate_polygon(uint32_t n, const float pos[], uint32_t numVerts, const int indices[], int dst[]);

} // namespace minipbrt
