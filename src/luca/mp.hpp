#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
#define LUCA_CPP_DECL_EMPTY_BASES __declspec(empty_bases)
#else
#define LUCA_CPP_DECL_EMPTY_BASES
#endif  // _MSC_VER && !__clang__

template <class... Fs>
struct LUCA_CPP_DECL_EMPTY_BASES overloaded : Fs... {
  using Fs::operator()...;
};

#undef LUCA_CPP_DECL_EMPTY_BASES
