//Copyright (C) 2011  Carl Rogers
//Released under MIT License
//license available in LICENSE file, or at http://www.opensource.org/licenses/mit-license.php

#include"cnpy.h"
#include<complex>
#include<cstdlib>
#include<algorithm>
#include<cstring>
#include<iomanip>
#include<stdint.h>
#include<stdexcept>
#include <regex>

char cnpy::BigEndianTest() {
    int x = 1;
    return (((char *)&x)[0]) ? '<' : '>';
}

char cnpy::map_type(const std::type_info& t)
{
    if(t == typeid(float) ) return 'f';
    if(t == typeid(double) ) return 'f';
    if(t == typeid(long double) ) return 'f';

    if(t == typeid(int) ) return 'i';
    if(t == typeid(char) ) return 'i';
    if(t == typeid(short) ) return 'i';
    if(t == typeid(long) ) return 'i';
    if(t == typeid(long long) ) return 'i';

    if(t == typeid(unsigned char) ) return 'u';
    if(t == typeid(unsigned short) ) return 'u';
    if(t == typeid(unsigned long) ) return 'u';
    if(t == typeid(unsigned long long) ) return 'u';
    if(t == typeid(unsigned int) ) return 'u';

    if(t == typeid(bool) ) return 'b';

    if(t == typeid(std::complex<float>) ) return 'c';
    if(t == typeid(std::complex<double>) ) return 'c';
    if(t == typeid(std::complex<long double>) ) return 'c';

    else return '?';
}

cnpy::NPY_TYPE cnpy::map_type_to_npy_types(const std::type_info& t) {
    if(t == typeid(float) ) return NPY_FLOAT;
    if(t == typeid(double) ) return NPY_DOUBLE;
    if(t == typeid(long double) ) return NPY_LONGDOUBLE;

    if(t == typeid(int) || t == typeid(int32_t) ) return NPY_INT;
    if(t == typeid(char) ) return NPY_INT;
    if(t == typeid(short) ) return NPY_INT;
    if(t == typeid(long) ) {
#if defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64)
        return NPY_INT;
#else
        return NPY_LONG;
#endif
    }

    if(t == typeid(long long) || t == typeid(int64_t) ) return NPY_LONGLONG;

    if(t == typeid(unsigned char) ) return NPY_UINT;
    if(t == typeid(unsigned short) ) return NPY_UINT;
    if(t == typeid(unsigned long) ){
#if defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64)
        return NPY_UINT;
#else
        return NPY_ULONG;
#endif
    }
    if(t == typeid(unsigned long long) || t == typeid(uint64_t) ) return NPY_UINT;
    if(t == typeid(unsigned int) || t == typeid(uint32_t) ||  t == typeid(unsigned long)) return NPY_UINT;

    if(t == typeid(bool) ) return NPY_BOOL;

    if(t == typeid(std::complex<float>) ) return NPY_CFLOAT;
    if(t == typeid(std::complex<double>) ) return NPY_CDOUBLE;
    if(t == typeid(std::complex<long double>) ) return NPY_CLONGDOUBLE;

    else return NPY_NOTYPE;
}

template<> std::vector<char>& cnpy::operator+=(std::vector<char>& lhs, const std::string rhs) {
    lhs.insert(lhs.end(),rhs.begin(),rhs.end());
    return lhs;
}

template<> std::vector<char>& cnpy::operator+=(std::vector<char>& lhs, const char* rhs) {
    //write in little endian
    size_t len = strlen(rhs);
    lhs.reserve(len);
    for(size_t byte = 0; byte < len; byte++) {
        lhs.push_back(rhs[byte]);
    }
    return lhs;
}


namespace cnpy{
static NPY_TYPE get_type_from_type_char_and_word_size(char type_char, size_t word_size) {
    cnpy::NPY_TYPE type;
    using cnpy::NPY_TYPE;
    if (type_char == 'f' && word_size == 4)
        type = NPY_FLOAT;
    else if (type_char == 'f' && word_size == 8)
        type = NPY_DOUBLE;
    else if (type_char == 'f' && word_size == 16)
        type = NPY_LONGDOUBLE;
    else if (type_char == 'i' && word_size == 1)
        type = NPY_BYTE;
    else if (type_char == 'i' && word_size == 2)
        type = NPY_SHORT;
    else if (type_char == 'i' && word_size == 4)
        type = NPY_INT;
    else if (type_char == 'i' && word_size == 8)
        type = NPY_LONGLONG;
    else if (type_char == 'u' && word_size == 1)
        type = NPY_UBYTE;
    else if (type_char == 'u' && word_size == 2)
        type = NPY_USHORT;
    else if (type_char == 'u' && word_size == 4)
        type = NPY_UINT;
    else if (type_char == 'u' && word_size == 8)
        type = NPY_ULONGLONG;
    else if (type_char == 'b' && word_size == 1)
        type = NPY_BOOL;
    else if (type_char == 'c' && word_size == 8)
        type = NPY_CFLOAT;
    else if (type_char == 'c' && word_size == 16)
        type = NPY_CDOUBLE;
    else if (type_char == 'c' && word_size == 32)
        type = NPY_CLONGDOUBLE;
    else if (type_char == 'S')
        type = NPY_STRING;
    else if (type_char == 'U')
        type = NPY_UNICODE;
    else if (type_char == 'V')
        type = NPY_VOID;
    else
        throw std::runtime_error("parse_npy_header: unsupported dtype: " + std::string(1, type_char) + std::to_string(word_size));
    return type;
}
} // namespace cnpy

void cnpy::parse_npy_header(unsigned char* buffer, size_t& word_size, std::vector<size_t>& shape, bool& fortran_order, NPY_TYPE& type) {
    //std::string magic_string(buffer,6);
    uint8_t major_version = *reinterpret_cast<uint8_t*>(buffer+6);
    uint8_t minor_version = *reinterpret_cast<uint8_t*>(buffer+7);
    uint16_t header_len = *reinterpret_cast<uint16_t*>(buffer+8);
    std::string header(reinterpret_cast<char*>(buffer+9),header_len);

    size_t loc1, loc2;

    //fortran order
    loc1 = header.find("fortran_order")+16;
    fortran_order = (header.substr(loc1,4) == "True" ? true : false);

    //shape
    loc1 = header.find("(");
    loc2 = header.find(")");

    std::regex num_regex("[0-9][0-9]*");
    std::smatch sm;
    shape.clear();

    std::string str_shape = header.substr(loc1+1,loc2-loc1-1);
    while(std::regex_search(str_shape, sm, num_regex)) {
        shape.push_back(std::stoi(sm[0].str()));
        str_shape = sm.suffix().str();
    }

    //endian, word size, data type
    //byte order code | stands for not applicable.
    //not sure when this applies except for byte array
    loc1 = header.find("descr");
    if (loc1 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: 'descr'");

    loc1 += 9;
    char byteorder = header[loc1];
    char typechar = header[loc1+1];

    std::string str_ws = header.substr(loc1+2);
    loc2 = str_ws.find("'");
    word_size = atoi(str_ws.substr(0, loc2).c_str());

    // Optional: check for littleEndian
    bool littleEndian = (byteorder == '<' || byteorder == '|');
    assert(littleEndian);  // Keep or remove depending on your target

    type = cnpy::get_type_from_type_char_and_word_size(typechar, word_size);
}

void cnpy::parse_npy_header(FILE* fp, size_t& word_size, std::vector<size_t>& shape, bool& fortran_order, NPY_TYPE& type) {
    char buffer[256];
    size_t res = fread(buffer,sizeof(char),11,fp);
    if(res != 11)
        throw std::runtime_error("parse_npy_header: failed fread");
    std::string header = fgets(buffer,256,fp);
    assert(header[header.size()-1] == '\n');

    size_t loc1, loc2;

    //fortran order
    loc1 = header.find("fortran_order");
    if (loc1 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: 'fortran_order'");
    loc1 += 16;
    fortran_order = (header.substr(loc1,4) == "True" ? true : false);

    //shape
    loc1 = header.find("(");
    loc2 = header.find(")");
    if (loc1 == std::string::npos || loc2 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: '(' or ')'");

    std::regex num_regex("[0-9][0-9]*");
    std::smatch sm;
    shape.clear();

    std::string str_shape = header.substr(loc1+1,loc2-loc1-1);
    while(std::regex_search(str_shape, sm, num_regex)) {
        shape.push_back(std::stoi(sm[0].str()));
        str_shape = sm.suffix().str();
    }

    //endian, word size, data type
    //byte order code | stands for not applicable. 
    //not sure when this applies except for byte array
    loc1 = header.find("descr");
    if (loc1 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: 'descr'");

    loc1 += 9;
    char byteorder = header[loc1];
    char typechar = header[loc1+1];

    std::string str_ws = header.substr(loc1+2);
    loc2 = str_ws.find("'");
    word_size = atoi(str_ws.substr(0, loc2).c_str());

    // Optional: check for littleEndian
    bool littleEndian = (byteorder == '<' || byteorder == '|');
    assert(littleEndian);  // Keep or remove depending on your target

    type = cnpy::get_type_from_type_char_and_word_size(typechar, word_size);
}

void cnpy::parse_zip_footer(FILE* fp, uint16_t& nrecs, size_t& global_header_size, size_t& global_header_offset)
{
    std::vector<char> footer(22);
    fseek(fp,-22,SEEK_END);
    size_t res = fread(&footer[0],sizeof(char),22,fp);
    if(res != 22)
        throw std::runtime_error("parse_zip_footer: failed fread");

    uint16_t disk_no, disk_start, nrecs_on_disk, comment_len;
    disk_no = *(uint16_t*) &footer[4];
    disk_start = *(uint16_t*) &footer[6];
    nrecs_on_disk = *(uint16_t*) &footer[8];
    nrecs = *(uint16_t*) &footer[10];
    global_header_size = *(uint32_t*) &footer[12];
    global_header_offset = *(uint32_t*) &footer[16];
    comment_len = *(uint16_t*) &footer[20];

    assert(disk_no == 0);
    assert(disk_start == 0);
    assert(nrecs_on_disk == nrecs);
    assert(comment_len == 0);
}

cnpy::NpyArray load_the_npy_file(FILE* fp) {
    std::vector<size_t> shape;
    size_t word_size;
    bool fortran_order;
    cnpy::NPY_TYPE type;
    cnpy::parse_npy_header(fp,word_size,shape,fortran_order, type);

    cnpy::NpyArray arr(shape, word_size, fortran_order, type);
    size_t nread = fread(arr.data<char>(),1,arr.num_bytes(),fp);
    if(nread != arr.num_bytes())
        throw std::runtime_error("load_the_npy_file: failed fread");
    return arr;
}

cnpy::NpyArray load_the_npz_array(FILE* fp, uint32_t compr_bytes, uint32_t uncompr_bytes) {

    std::vector<unsigned char> buffer_compr(compr_bytes);
    std::vector<unsigned char> buffer_uncompr(uncompr_bytes);
    size_t nread = fread(&buffer_compr[0],1,compr_bytes,fp);
    if(nread != compr_bytes)
        throw std::runtime_error("load_the_npy_file: failed fread");

    int err;
    z_stream d_stream;

    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    d_stream.avail_in = 0;
    d_stream.next_in = Z_NULL;
    err = inflateInit2(&d_stream, -MAX_WBITS);

    d_stream.avail_in = compr_bytes;
    d_stream.next_in = &buffer_compr[0];
    d_stream.avail_out = uncompr_bytes;
    d_stream.next_out = &buffer_uncompr[0];

    err = inflate(&d_stream, Z_FINISH);
    err = inflateEnd(&d_stream);

    std::vector<size_t> shape;
    size_t word_size;
    bool fortran_order;
    cnpy::NPY_TYPE type;
    cnpy::parse_npy_header(&buffer_uncompr[0],word_size,shape,fortran_order, type);

    cnpy::NpyArray array(shape, word_size, fortran_order, type);

    size_t offset = uncompr_bytes - array.num_bytes();
    memcpy(array.data<unsigned char>(),&buffer_uncompr[0]+offset,array.num_bytes());

    return array;
}

// --- Helper struct for entry info ---
struct NpzEntryInfo {
    std::string array_name;
    uint16_t compression_method;
    uint32_t compressed_byte_count;
    uint32_t uncompressed_byte_count;
    long data_offset; // position in file where data begins
};

// --- Helper: parse next entry header and fill info ---
static bool parse_npz_entry(FILE* fp, NpzEntryInfo& info) {
    std::vector<char> local_header(30);
    size_t header_read_byte_count = fread(&local_header[0], sizeof(char), 30, fp);
    if(header_read_byte_count != 30) return false;
    if(local_header[2] != 0x03 || local_header[3] != 0x04) return false;

    uint16_t name_byte_count;
    memcpy(&name_byte_count, &local_header[26], sizeof(uint16_t));
    uint16_t extra_field_byte_count;
    memcpy(&extra_field_byte_count, &local_header[28], sizeof(uint16_t));
    uint16_t compression_method;
    memcpy(&compression_method, &local_header[8], sizeof(uint16_t));
    uint32_t compressed_byte_count;
    memcpy(&compressed_byte_count, &local_header[18], sizeof(uint32_t));
    uint32_t uncompressed_byte_count;
    memcpy(&uncompressed_byte_count, &local_header[22], sizeof(uint32_t));
    // Read file name
    std::string array_name(name_byte_count, ' ');
    size_t array_name_read_byte_count = fread(&array_name[0], sizeof(char), name_byte_count, fp);
    if(array_name_read_byte_count != name_byte_count) return false;
    array_name.erase(array_name.end() - 4, array_name.end());
    // Read extra field
    std::vector<char> extra_field(extra_field_byte_count);
    if(extra_field_byte_count > 0) {
        size_t read = fread(&extra_field[0], sizeof(char), extra_field_byte_count, fp);
        if(read != extra_field_byte_count) return false;
    }
    // ZIP64 fix
    if(compressed_byte_count == 0xFFFFFFFF || uncompressed_byte_count == 0xFFFFFFFF) {
        size_t idx = 0;
        while(idx + 4 <= extra_field.size()) {
            uint16_t header_id, data_size;
            memcpy(&header_id, &extra_field[idx], 2);
            memcpy(&data_size, &extra_field[idx+2], 2);
            if(header_id == 0x0001) {
                size_t zip64_idx = idx + 4;
                uint64_t zip64_size = 0;
                if(uncompressed_byte_count == 0xFFFFFFFF && zip64_idx + 8 <= extra_field.size()) {
                    memcpy(&zip64_size, &extra_field[zip64_idx], 8);
                    uncompressed_byte_count = static_cast<uint32_t>(zip64_size);
                    zip64_idx += 8;
                }
                if(compressed_byte_count == 0xFFFFFFFF && zip64_idx + 8 <= extra_field.size()) {
                    memcpy(&zip64_size, &extra_field[zip64_idx], 8);
                    compressed_byte_count = static_cast<uint32_t>(zip64_size);
                    zip64_idx += 8;
                }
                break;
            }
            idx += 4 + data_size;
        }
    }
    info.array_name = array_name;
    info.compression_method = compression_method;
    info.compressed_byte_count = compressed_byte_count;
    info.uncompressed_byte_count = uncompressed_byte_count;
    info.data_offset = ftell(fp);
    return true;
}

// --- Refactored npz_load (single file, all arrays) ---
cnpy::npz_t cnpy::npz_load(std::string fname) {
    FILE* fp = fopen(fname.c_str(),"rb");
    if(!fp) throw std::runtime_error("npz_load: Error! Unable to open file "+fname+"!");
    cnpy::npz_t arrays;
    while(1) {
        NpzEntryInfo info;
        long entry_start = ftell(fp);
        if(!parse_npz_entry(fp, info)) break;
        fseek(fp, info.data_offset, SEEK_SET);
        if(info.compression_method == 0) {
            arrays[info.array_name] = load_the_npy_file(fp);
        } else {
            arrays[info.array_name] = load_the_npz_array(fp, info.compressed_byte_count, info.uncompressed_byte_count);
        }
        fseek(fp, entry_start + 30 + info.array_name.size() + 4 + info.compressed_byte_count + info.uncompressed_byte_count, SEEK_SET); // skip to next entry
        // Actually, after reading, fp is at end of data, so just continue
    }
    fclose(fp);
    return arrays;
}

// --- Refactored npz_load (single variable) ---
cnpy::NpyArray cnpy::npz_load(std::string fname, std::string varname) {
    struct AutoCloser {
        FILE * fp;
        ~AutoCloser() { fclose(fp); }
    } closer{};
    closer.fp = fopen(fname.c_str(), "rb");
    if(!closer.fp) throw std::runtime_error("npz_load: Unable to open file "+fname);
    while(1) {
        NpzEntryInfo info;
        if(!parse_npz_entry(closer.fp, info)) break;
        if(info.array_name == varname) {
            fseek(closer.fp, info.data_offset, SEEK_SET);
            if(info.compression_method == 0) {
                return load_the_npy_file(closer.fp);
            } else {
                return load_the_npz_array(closer.fp, info.compressed_byte_count, info.uncompressed_byte_count);
            }
        } else {
            fseek(closer.fp, info.data_offset + info.compressed_byte_count, SEEK_SET);
        }
    }
    throw std::runtime_error("npz_load: variable not found: " + varname);
}

cnpy::NpyArray cnpy::npy_load(std::string fname) {

    struct AutoCloser
    {
        FILE * fp;
        ~AutoCloser (void)
        {
            fclose(fp);
        }
    } closer;
    closer.fp = fopen(fname.c_str(), "rb");

    if(!closer.fp) throw std::runtime_error("npy_load: Unable to open file "+fname);

    NpyArray arr = load_the_npy_file(closer.fp);

    return arr;
}
