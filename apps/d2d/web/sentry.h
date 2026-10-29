// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp — no-op stand-in for Sentry Native in the WebAssembly build.
//
// A browser has no Crashpad handler to run, so the web build links no
// Sentry. sentry_init fails and sentry_is_enabled is false, which keeps
// main.cpp's crash-report paths switched off without #ifdefs there.
#pragma once

#include <cstddef>
#include <cstdint>

struct sentry_options_t {};
struct sentry_value_t { int unused = 0; };
struct sentry_uuid_t { char bytes[16] = {}; };

inline sentry_options_t* sentry_options_new() { static sentry_options_t options; return &options; }
inline void sentry_options_set_dsn(sentry_options_t*, const char*) {}
inline void sentry_options_set_release(sentry_options_t*, const char*) {}
inline void sentry_options_set_auto_session_tracking(sentry_options_t*, int) {}
inline void sentry_options_set_database_path(sentry_options_t*, const char*) {}
inline void sentry_options_set_handler_path(sentry_options_t*, const char*) {}
inline void sentry_options_set_debug(sentry_options_t*, int) {}
inline int sentry_init(sentry_options_t*) { return 1; }
inline int sentry_close() { return 0; }
inline int sentry_is_enabled() { return 0; }
inline void sentry_crash() {}

inline sentry_value_t sentry_value_new_object() { return {}; }
inline sentry_value_t sentry_value_new_string(const char*) { return {}; }
inline sentry_value_t sentry_value_new_string_n(const char*, std::size_t) { return {}; }
inline sentry_value_t sentry_value_new_int32(std::int32_t) { return {}; }
inline sentry_value_t sentry_value_new_breadcrumb(const char*, const char*) { return {}; }
inline int sentry_value_set_by_key(sentry_value_t, const char*, sentry_value_t) { return 0; }
inline int sentry_value_is_null(sentry_value_t) { return 1; }
inline void sentry_add_breadcrumb(sentry_value_t) {}
inline void sentry_set_context(const char*, sentry_value_t) {}
inline void sentry_set_extra(const char*, sentry_value_t) {}
inline void sentry_set_tag(const char*, const char*) {}

inline sentry_value_t sentry_attachment_from_bytes(const char*, std::size_t, const char*) { return {}; }
inline void sentry_attachment_set_content_type(sentry_value_t, const char*) {}
inline sentry_uuid_t sentry_add_attachment(sentry_value_t) { return {}; }
inline void sentry_remove_attachment(sentry_uuid_t) {}
inline int sentry_uuid_is_nil(const sentry_uuid_t*) { return 1; }
