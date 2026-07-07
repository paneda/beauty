// ASIO SSL separate compilation unit.
// When ASIO is built with ASIO_SEPARATE_COMPILATION (as the asio.cmake
// wrapper does), the SSL implementation must be compiled in exactly one
// translation unit.  This file is only compiled when BEAUTY_ENABLE_SSL
// is defined.
#ifdef BEAUTY_ENABLE_SSL
#include <asio/ssl/impl/src.hpp>
#endif
