module;

#include <utf8cpp/utf8.h>

export module utf8cpp;

export namespace utf8
{
    using ::utf8::advance;
    using ::utf8::append;
    using ::utf8::append16;
    using ::utf8::distance;
    using ::utf8::next;
    using ::utf8::next16;
    using ::utf8::peek_next;

    namespace unchecked
    {
        using ::utf8::unchecked::advance;
        using ::utf8::unchecked::append;
        using ::utf8::unchecked::append16;
        using ::utf8::unchecked::distance;
        using ::utf8::unchecked::next;
        using ::utf8::unchecked::next16;
        using ::utf8::unchecked::peek_next;
    }
}
