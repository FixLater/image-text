// utility.cpp
#include <boost/locale.hpp>

class UnicodeUtil {
public:
    static std::string gbk_to_utf8(const std::string &gbk) {
        return boost::locale::conv::to_utf<char>(gbk, "GBK");
        return "";
    }

    static int64_t to_big_endian_long(int64_t value) {
        return ((value & 0xFF00000000000000) >> 56) |
               ((value & 0x00FF000000000000) >> 40) |
               ((value & 0x0000FF0000000000) >> 24) |
               ((value & 0x000000FF00000000) >> 8) |
               ((value & 0x00000000FF000000) << 8) |
               ((value & 0x0000000000FF0000) << 24) |
               ((value & 0x000000000000FF00) << 40) |
               ((value & 0x00000000000000FF) << 56);
    }


    static int to_big_endian_int(int value) {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    }
};