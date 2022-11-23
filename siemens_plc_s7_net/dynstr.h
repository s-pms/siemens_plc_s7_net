/* SDS (Simple Dynamic Strings), A C dynamic strings library.
 *
 * Copyright (c) 2006-2014, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  * modify:
  *   - rename `sds' to `dynstr'
  *   - add some split function
  **/

#ifndef __DYNSTR_H__
#define __DYNSTR_H__

#include <sys/types.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef char* dynstr;

	struct dynstr_hdr {
		int len;
		int free;
		char buf[];
	};

	dynstr dynstr_newlen(const void* init, size_t initlen);
	dynstr dynstr_new(const char* init);
	dynstr dynstr_empty(void);
	size_t dynstr_len(const dynstr s);
	dynstr dynstr_dup(const dynstr s);
	void dynstr_free(dynstr s);
	size_t dynstr_avail(const dynstr s);
	dynstr dynstr_growzero(dynstr s, size_t len);
	dynstr dynstr_catlen(dynstr s, const void* t, size_t len);
	dynstr dynstr_cat(dynstr s, const char* t);
	dynstr dynstr_cat_dynstr(dynstr s, const dynstr t);
	dynstr dynstr_cpylen(dynstr s, const char* t, size_t len);
	dynstr dynstr_cpy(dynstr s, const char* t);

	dynstr dynstr_catvprintf(dynstr s, const char* fmt, va_list ap);
	dynstr dynstr_vsprintf(const char* fmt, va_list ap);
#ifdef __GNUC__
	dynstr dynstr_catprintf(dynstr s, const char* fmt, ...);
	//	__attribute__((format(printf, 2, 3)));
	dynstr dynstr_sprintf(const char* fmt, ...);
	//	__attribute__((format(printf, 2, 3)));
#else
	dynstr dynstr_catprintf(dynstr s, const char* fmt, ...);
	dynstr dynstr_sprintf(const char* fmt, ...);
#endif

	void dynstr_trim(dynstr s, const char* cset);
	void dynstr_range(dynstr s, int start, int end);
	void dynstr_updatelen(dynstr s);
	void dynstr_clear(dynstr s);
	int dynstr_cmp(const dynstr s1, const dynstr s2);
	int dynstr_find(const char* s, int len, const char* p, int m);
	int dynstr_rfind(const char* s, int len, const char* p, int m);
	dynstr* dynstr_splitwhitespace(const char* s, int len, int* count);
	dynstr* dynstr_rsplitwhitespace(const char* s, int len, int* count);
	dynstr* dynstr_splitchar(const char* s, int len, const char ch, int* count);
	dynstr* dynstr_rsplitchar(const char* s, int len, const char ch, int* count);
	dynstr* dynstr_split(const char* s, int len, const char* sep, int seplen, int* count);
	dynstr* dynstr_rsplit(const char* s, int len, const char* sep, int seplen, int* count);
	dynstr* dynstr_splitlines(const char* s, int len, int* count, int keepends);
	void dynstr_freesplitres(dynstr* tokens, int count);
	void dynstr_tolower(dynstr s);
	void dynstr_toupper(dynstr s);
	dynstr dynstr_fromlonglong(long long value);
	dynstr dynstr_catrepr(dynstr s, const char* p, size_t len);
	dynstr* dynstr_splitargs(const char* line, int* argc);
	dynstr dynstr_mapchars(dynstr s, const char* from, const char* to, size_t setlen);
	dynstr dynstr_join(char** argv, int argc, char* sep, size_t seplen);
	dynstr dynstr_join_dynstr(dynstr* argv, int argc, const char* sep, size_t seplen);

	/* base64 encode and decode */
	dynstr b64encode_standard(const unsigned char* input, size_t len);
	dynstr b64decode_standard(const char* input, size_t len);

	/* Low level functions exposed to the user API */
	dynstr dynstrMakeRoomFor(dynstr s, size_t addlen);
	void dynstrIncrLen(dynstr s, int incr);
	dynstr dynstrRemoveFreeSpace(dynstr s);
	size_t dynstrAllocSize(dynstr s);

#ifdef __cplusplus
}
#endif
#endif  /* !__DYNSTR_H__ */
