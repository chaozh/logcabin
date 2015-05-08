/* Copyright (c) 2015 Diego Ongaro
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * \file
 * Common utilities and definitions for example client programs.
 */

#include <cinttypes>
#include <string>

#ifndef LOGCABIN_EXAMPLES_UTIL_H
#define LOGCABIN_EXAMPLES_UTIL_H

namespace LogCabin {
namespace Examples {
namespace Util {

/**
 * Convert a human-readable description of a time duration into a number of
 * nanoseconds.
 * \param description
 *      Something like 10, 10s, 200ms, 3us, or 999ns. With no units, defaults
 *      to seconds.
 * \return
 *      Number of nanoseconds.
 * \throw std::runtime_error
 *      If description could not be parsed successfully.
 */
uint64_t parseTime(const std::string& description);

} // namespace LogCabin::Examples::Util
} // namespace LogCabin::Examples
} // namespace LogCabin

#endif /* LOGCABIN_EXAMPLES_UTIL_H */
