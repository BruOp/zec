#pragma once

// Forward declared string
namespace std
{
    template <class _Elem>
    class allocator;

    template <class _Elem>
    struct char_traits;

    template <class _Elem, class _Traits, class _Alloc>
    class basic_string;

    typedef basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t>> wstring;
    typedef basic_string<char, char_traits<char>, allocator<char>> string;

    template <class _Elem, class _Traits>
    class basic_string_view;

    typedef basic_string_view<char, char_traits<char>> string_view;
    typedef basic_string_view<wchar_t, char_traits<wchar_t>> wstring_view;
}