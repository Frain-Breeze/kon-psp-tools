#include "cpk.hpp"

#include <string.h>
#include <string>

#include <logging.hpp>

// https://github.com/esperknight/CriPakTools/blob/0dbd2229f7c3b519c67d65e89055fd4e610d6dca/CriPakTools/CPK.cs#L577



class UTF_Table {
public:
    struct Flags {
        enum class Storage {
            none = 0,
            zero = 1,
            constant = 3,
            per_row = 5,
        };

        enum class Type {
            i8 = 0,
            i8t = 1,
            i16 = 2,
            i16t = 3,
            i32 = 4,
            i32t = 5,
            i64 = 6,
            i64t = 7,
            f32 = 8,
            str = 0xA,
            data = 0xB,
        };
        
        Type type : 4;
        Storage storage : 4;
    } __attribute__((packed));
    static_assert(sizeof(Flags) == 1);

    uint32_t _table_size;
    uint32_t _rows_offset;
    uint32_t _strings_offset;
    uint32_t _data_offset;

    uint32_t _table_name;
    uint16_t _num_columns;
    uint16_t _row_length;
    uint32_t _num_rows;

    struct Column {
        Flags flags;
        std::string name;
    };

    struct Unit {
        union {
            uint8_t i8;
            uint16_t i16;
            uint32_t i32;
            uint64_t i64;
            float flt;
            char* string;
            //data type 0xB?
        } data;
        
        Flags::Type type;
    };

    std::vector<Column> _columns; //only contains info on types and names
    std::vector<std::vector<Unit>> _rows; //contains actual data

    //...

    void open(Tea::File& file) {
        size_t offset = file.tell(); //HACK: we skipped the @UTF, and we probably shouldn't do that (it's what the -4 is for)
        uint32_t magic = file.read<uint32_t>();

        file.endian(Tea::Endian_big);
        file.read(_table_size);
        _rows_offset = file.read<uint32_t>() + offset + 8;
        _strings_offset = file.read<uint32_t>() + offset + 8;
        _data_offset = file.read<uint32_t>() + offset + 8;

        file.read(_table_name);
        file.read(_num_columns);
        file.read(_row_length);
        file.read(_num_rows);

        _columns.resize(_num_columns);
        for(int i = 0; i < _columns.size(); i++) {
            file.read(_columns[i].flags);
            uint32_t name_offset = file.read<uint32_t>();
            size_t offs = file.tell();
            file.seek(name_offset + _strings_offset);
            while(char curr = file.read<char>()) {
                _columns[i].name += curr;
            }
            file.seek(offs);
        }

        _rows.resize(_num_rows);
        for(int i = 0; i < _rows.size(); i++) {
            _rows[i].resize(_num_columns);
            
            file.seek(_rows_offset + (i * _row_length));
            
            for(int a = 0; a < _num_columns; a++) {
                _rows[i][a].type = _columns[a].flags.type;

                Flags::Storage storage_flag = _columns[a].flags.storage;
                if(storage_flag == Flags::Storage::none
                    || storage_flag == Flags::Storage::zero
                    || storage_flag == Flags::Storage::constant) {
                    
                    continue;
                }
                
                
                switch(_rows[i][a].type) {
                    default:
                        LOGERR("invalid type value (or 0xB maybe)");
                        break;
                    case Flags::Type::i8:
                    case Flags::Type::i8t:
                        file.read(_rows[i][a].data.i8);
                        break;
                    case Flags::Type::i16:
                    case Flags::Type::i16t:
                        file.read(_rows[i][a].data.i16);
                        break;
                    case Flags::Type::i32:
                    case Flags::Type::i32t:
                        file.read(_rows[i][a].data.i32);
                        break;
                    case Flags::Type::i64:
                    case Flags::Type::i64t:
                        file.read(_rows[i][a].data.i64);
                        break;
                    case Flags::Type::f32:
                        file.read(_rows[i][a].data.flt);
                        break;
                    case Flags::Type::str:
                        uint32_t tmp_offset = file.tell();
                        file.seek(file.read<uint32_t>() + _strings_offset);
                        std::string str;
                        //ugly
                        while(char curr = file.read<char>()) {
                            str += curr;
                        }
                        _rows[i][a].data.string = (char*)malloc(strlen(str.c_str())+1);
                        file.seek(tmp_offset);
                        break;
                }
            }
        }
    }
};

void decryptUTF(std::vector<uint8_t>& data) {
    uint32_t m = 0x0000655f;
    uint32_t t = 0x00004115;

    uint8_t d;

    for(size_t i = 0; i < data.size(); i++) {
        d = data[i];
        d = d ^ (m & 0xFF);
        data[i] = d;
        m *= t;
    }
}

void readUTF(Tea::File& file, std::vector<uint8_t>& out_data) {
    uint32_t unk1; file.read(unk1);
    uint64_t utf_size; file.read(utf_size);
    out_data.resize(utf_size);
    file.read(out_data.data(), utf_size);
    if(out_data[0] != 0x40 || out_data[1] != 0x55 || out_data[2] != 0x54 || out_data[3] != 0x46) { //@UTF
        decryptUTF(out_data);
    }
}

bool CPK::close() {
    //TODO: finish
    return false;
}

bool CPK::open(Tea::File& file) {
    this->close();

    _file = &file;

    char magic[5] = "\0\0\0\0";
    if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading magic"); return false; }
    if(strncmp(magic, "CPK ", 4)) { return false; }

    std::vector<uint8_t> CPK_packet;
    readUTF(*_file, CPK_packet);

    UTF_Table table;
    Tea::FileMemory cpk_file_packet;
    cpk_file_packet.open(CPK_packet.data(), CPK_packet.size());
    table.open(cpk_file_packet);

    //TODO: continue
}
