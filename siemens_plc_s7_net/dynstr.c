/* A C dynamic strings library.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "dynstr.h"

#define DYNSTR_MAX_PREALLOC (1024*1024)

#ifndef container_of
#define container_of(ptr, type, member) \
   ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif

size_t dynstr_len(const dynstr s)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	return sh->len;
}

size_t dynstr_avail(const dynstr s)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	return sh->free;
}

/* Create a new dynstr string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 *
 * The string is always null-termined (all the dynstr strings are, always) so
 * even if you create an dynstr string with:
 *
 * mystring = dynstr_newlen("abc",3");
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the dynstr header.
 **/
dynstr dynstr_newlen(const void* init, size_t initlen)
{
	struct dynstr_hdr* sh;

	if (init) {
		sh = malloc(sizeof(struct dynstr_hdr) + initlen + 1);
	}
	else {
		sh = calloc(sizeof(struct dynstr_hdr) + initlen + 1, 1);
	}
	if (sh == NULL) return NULL;
	sh->len = initlen;
	sh->free = 0;
	if (initlen && init)
		memcpy(sh->buf, init, initlen);
	sh->buf[initlen] = '\0';
	return (char*)sh->buf;
}

/* Create an empty (zero length) dynstr string. Even in this case the string
 * always has an implicit null term.
 **/
dynstr dynstr_empty(void)
{
	return dynstr_newlen("", 0);
}

/* Create a new dynstr string starting from a null termined C string.
 **/
dynstr dynstr_new(const char* init)
{
	size_t initlen = (init == NULL) ? 0 : strlen(init);
	return dynstr_newlen(init, initlen);
}

/* Duplicate an dynstr string. */
dynstr dynstr_dup(const dynstr s)
{
	return dynstr_newlen(s, dynstr_len(s));
}

/* Free an dynstr string. No operation is performed if 's' is NULL.
 **/
void dynstr_free(dynstr s)
{
	if (s == NULL)
		return;
	free(container_of(s, struct dynstr_hdr, buf));
}

/* Set the dynstr string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the dynstr string is hacked manually in some
 * way, like in the following example:
 *
 * s = dynstr_new("foobar");
 * s[2] = '\0';
 * dynstr_updatelen(s);
 * printf("%d\n", dynstr_len(s));
 *
 * The output will be "2", but if we comment out the call to dynstr_updatelen()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes.
 **/
void dynstr_updatelen(dynstr s)
{
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	int reallen = strlen(s);
	sh->free += (sh->len - reallen);
	sh->len = reallen;
}

/* Modify an dynstr string on-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available.
 **/
void dynstr_clear(dynstr s)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	sh->free += sh->len;
	sh->len = 0;
	sh->buf[0] = '\0';
}

/* Enlarge the free space at the end of the dynstr string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the dynstr string as returned
 * by dynstr_len(), but only the free buffer space we have.
 **/
dynstr dynstrMakeRoomFor(dynstr s, size_t addlen)
{
	struct dynstr_hdr* sh, * newsh;
	size_t free = dynstr_avail(s);
	size_t len, newlen;

	if (free >= addlen) return s;
	len = dynstr_len(s);
	sh = container_of(s, struct dynstr_hdr, buf);
	newlen = (len + addlen);
	if (newlen < DYNSTR_MAX_PREALLOC)
		newlen *= 2;
	else
		newlen += DYNSTR_MAX_PREALLOC;
	newsh = realloc(sh, sizeof(struct dynstr_hdr) + newlen + 1);
	if (newsh == NULL) return NULL;

	newsh->free = newlen - len;
	return newsh->buf;
}

/* Reallocate the dynstr string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 **/
dynstr dynstrRemoveFreeSpace(dynstr s)
{
	struct dynstr_hdr* sh;
	assert(s != NULL);
	sh = container_of(s, struct dynstr_hdr, buf);
	sh = realloc(sh, sizeof(struct dynstr_hdr) + sh->len + 1);
	sh->free = 0;
	return sh->buf;
}

/* Return the total size of the allocation of the specifed dynstr string,
 * including:
 * 1) The dynstr header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 **/
size_t dynstrAllocSize(dynstr s)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	return sizeof(struct dynstr_hdr) + sh->len + sh->free + 1;
}

/* Increment the dynstr length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls dynstrMakeRoomFor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using dynstrIncrLen() and dynstrMakeRoomFor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * dynstr string without copying into an intermediate buffer:
 *
 * oldlen = dynstr_len(s);
 * s = dynstrMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * dynstrIncrLen(s, nread);
 **/
void dynstrIncrLen(dynstr s, int incr)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	assert(sh->free >= incr);
	sh->len += incr;
	sh->free -= incr;
	assert(sh->free >= 0);
	s[sh->len] = '\0';
}

/* Grow the dynstr to have the specified length. Bytes that were not part of
 * the original length of the dynstr will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed.
 **/
dynstr dynstr_growzero(dynstr s, size_t len)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	size_t totlen, curlen = sh->len;

	if (len <= curlen) return s;
	s = dynstrMakeRoomFor(s, len - curlen);
	if (s == NULL)
		return NULL;

	/* Make sure added region doesn't contain garbage */
	sh = container_of(s, struct dynstr_hdr, buf);
	memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
	totlen = sh->len + sh->free;
	sh->len = len;
	sh->free = totlen - sh->len;
	return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified dynstr string 's'.
 *
 * After the call, the passed dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 **/
dynstr dynstr_catlen(dynstr s, const void* t, size_t len)
{
	struct dynstr_hdr* sh;
	size_t curlen = dynstr_len(s);

	s = dynstrMakeRoomFor(s, len);
	if (s == NULL) return NULL;
	sh = container_of(s, struct dynstr_hdr, buf);
	memcpy(s + curlen, t, len);
	sh->len = curlen + len;
	sh->free = sh->free - len;
	s[curlen + len] = '\0';
	return s;
}

/* Append the specified null termianted C string to the dynstr string 's'.
 *
 * After the call, the passed dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 **/
dynstr dynstr_cat(dynstr s, const char* t)
{
	return dynstr_catlen(s, t, strlen(t));
}

/* Append the specified dynstr 't' to the existing dynstr 's'.
 *
 * After the call, the modified dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 **/
dynstr dynstr_cat_dynstr(dynstr s, const dynstr t)
{
	return dynstr_catlen(s, t, dynstr_len(t));
}

/* Destructively modify the dynstr string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes.
 **/
dynstr dynstr_cpylen(dynstr s, const char* t, size_t len)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	size_t totlen = sh->free + sh->len;

	if (totlen < len) {
		s = dynstrMakeRoomFor(s, len - sh->len);
		if (s == NULL) return NULL;
		sh = container_of(s, struct dynstr_hdr, buf);
		totlen = sh->free + sh->len;
	}
	memcpy(s, t, len);
	s[len] = '\0';
	sh->len = len;
	sh->free = totlen - len;
	return s;
}

/* Like dynstr_cpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen().
 **/
dynstr dynstr_cpy(dynstr s, const char* t)
{
	return dynstr_cpylen(s, t, strlen(t));
}

dynstr dynstr_vsprintf(const char* fmt, va_list ap)
{
	va_list cpy;
	dynstr s;
	int len;

	va_copy(cpy, ap);
	len = vsnprintf(NULL, 0, fmt, cpy);
	s = dynstr_newlen(0, len);
	if (s == NULL) return NULL;
	dynstr_clear(s);
	va_copy(cpy, ap);
	if (vsnprintf(s, dynstr_avail(s), fmt, cpy) == len) {
		dynstrIncrLen(s, len);
		return s;
	}
	dynstr_free(s);
	return NULL;
}

dynstr dynstr_sprintf(const char* fmt, ...)
{
	dynstr s;
	va_list ap;
	va_start(ap, fmt);
	s = dynstr_vsprintf(fmt, ap);
	va_end(ap);
	return s;
}

/* Like dynstr_catpritf() but gets va_list instead of being variadic.
 **/
dynstr dynstr_catvprintf(dynstr s, const char* fmt, va_list ap)
{
	va_list cpy;
	int len;

	va_copy(cpy, ap);
	len = vsnprintf(NULL, 0, fmt, cpy);
	if (len <= 0) return s;
	s = dynstrMakeRoomFor(s, len);
	va_copy(cpy, ap);
	if (vsnprintf(s + dynstr_len(s), dynstr_avail(s), fmt, cpy) == len) {
		dynstrIncrLen(s, len);
		return s;
	}
	return s;
}

/* Append to the dynstr string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = dynstr_new("Sum is: ");
 * s = dynstr_catprintf(s, "%d+%d = %d", a, b, a + b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use dynstr_empty() as the target string:
 *
 * s = dynstr_catprintf(dynstr_empty(), "... your format ...", args);
 **/
dynstr dynstr_catprintf(dynstr s, const char* fmt, ...)
{
	va_list ap;
	char* t;
	va_start(ap, fmt);
	t = dynstr_catvprintf(s, fmt, ap);
	va_end(ap);
	return t;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = dynstr_new("AA...AA.a.aa.aHelloWorld     :::");
 * s = dynstr_trim(s, "A. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 **/
void dynstr_trim(dynstr s, const char* cset)
{
	assert(s != NULL);
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	char* start, * end, * sp, * ep;
	size_t len;

	sp = start = s;
	ep = end = s + dynstr_len(s) - 1;
	while (sp <= end && strchr(cset, *sp)) sp++;
	while (ep > start && strchr(cset, *ep)) ep--;
	len = (sp > ep) ? 0 : ((ep - sp) + 1);
	if (sh->buf != sp) memmove(sh->buf, sp, len);
	sh->buf[len] = '\0';
	sh->free = sh->free + (sh->len - len);
	sh->len = len;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = dynstr_new("Hello World");
 * dynstr_range(s, 1, -1); => "ello World"
 **/
void dynstr_range(dynstr s, int start, int end)
{
	struct dynstr_hdr* sh = container_of(s, struct dynstr_hdr, buf);
	size_t newlen, len = dynstr_len(s);

	if (len == 0) return;
	if (start < 0) {
		start = len + start;
		if (start < 0) start = 0;
	}
	if (end < 0) {
		end = len + end;
		if (end < 0) end = 0;
	}
	newlen = (start > end) ? 0 : (end - start) + 1;
	if (newlen != 0) {
		if (start >= (signed)len) {
			newlen = 0;
		}
		else if (end >= (signed)len) {
			end = len - 1;
			newlen = (start > end) ? 0 : (end - start) + 1;
		}
	}
	else {
		start = 0;
	}
	if (start && newlen)
		memmove(sh->buf, sh->buf + start, newlen);
	sh->buf[newlen] = 0;
	sh->free = sh->free + (sh->len - newlen);
	sh->len = newlen;
}

/* Apply tolower() to every character of the dynstr string 's'.
 **/
void dynstr_tolower(dynstr s)
{
	int len = dynstr_len(s), j;

	for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

/* Apply toupper() to every character of the dynstr string 's'.
 **/
void dynstr_toupper(dynstr s)
{
	int len = dynstr_len(s), j;

	for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

/* Compare two dynstr strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     1 if s1 > s2.
 *    -1 if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one.
 **/
int dynstr_cmp(const dynstr s1, const dynstr s2)
{
	size_t l1, l2, minlen;
	int cmp;

	l1 = dynstr_len(s1);
	l2 = dynstr_len(s2);
	minlen = (l1 < l2) ? l1 : l2;
	cmp = memcmp(s1, s2, minlen);
	if (cmp == 0) return l1 - l2;
	return cmp;
}

/* Create an dynstr string from a long long value. It is much faster than:
 *
 * dynstr_catprintf(dynstr_empty(), "%lld\n", value);
 **/
dynstr dynstr_fromlonglong(long long value)
{
	char buf[32], * p;
	unsigned long long v;

	v = (value < 0) ? -value : value;
	p = buf + 31; /* point to the last character */
	do {
		*p-- = '0' + (v % 10);
		v /= 10;
	} while (v);
	if (value < 0) *p-- = '-';
	p++;
	return dynstr_newlen(p, 32 - (p - buf));
}

/* Append to the dynstr string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified dynstr string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 **/
dynstr dynstr_catrepr(dynstr s, const char* p, size_t len)
{
	s = dynstr_catlen(s, "\"", 1);
	while (len--) {
		switch (*p) {
		case '\\':
		case '"':
			s = dynstr_catprintf(s, "\\%c", *p);
			break;
		case '\n': s = dynstr_catlen(s, "\\n", 2); break;
		case '\r': s = dynstr_catlen(s, "\\r", 2); break;
		case '\t': s = dynstr_catlen(s, "\\t", 2); break;
		case '\a': s = dynstr_catlen(s, "\\a", 2); break;
		case '\b': s = dynstr_catlen(s, "\\b", 2); break;
		default:
			if (isprint(*p))
				s = dynstr_catprintf(s, "%c", *p);
			else
				s = dynstr_catprintf(s, "\\x%02x", (unsigned char)*p);
			break;
		}
		p++;
	}
	return dynstr_catlen(s, "\"", 1);
}

/* Helper function for dynstr_splitargs() that returns non zero if 'c'
 * is a valid hex digit.
 **/
int is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

/* Helper function for dynstr_splitargs() that converts a hex digit into an
 * integer from 0 to 15
 **/
int hex_digit_to_int(char c)
{
	switch (c) {
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'a': case 'A': return 10;
	case 'b': case 'B': return 11;
	case 'c': case 'C': return 12;
	case 'd': case 'D': return 13;
	case 'e': case 'E': return 14;
	case 'f': case 'F': return 15;
	default: return 0;
	}
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of dynstr is returned.
 *
 * The caller should free the resulting array of dynstr strings with
 * dynstr_freesplitres().
 *
 * Note that dynstr_catrepr() is able to convert back a string into
 * a quoted string in the same format dynstr_splitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 **/
dynstr* dynstr_splitargs(const char* line, int* argc)
{
	const char* p = line;
	char* current = NULL;
	char** vector = NULL;

	*argc = 0;
	while (1) {
		/* skip blanks */
		while (*p && isspace(*p)) p++;
		if (*p) {
			/* get a token */
			int inq = 0;  /* set to 1 if we are in "quotes" */
			int insq = 0; /* set to 1 if we are in 'single quotes' */
			int done = 0;

			if (current == NULL) current = dynstr_empty();
			while (!done) {
				if (inq) {
					if (*p == '\\' && *(p + 1) == 'x' &&
						is_hex_digit(*(p + 2)) &&
						is_hex_digit(*(p + 3))) {
						unsigned char byte;

						byte = (hex_digit_to_int(*(p + 2)) * 16) +
							hex_digit_to_int(*(p + 3));
						current = dynstr_catlen(current, (char*)&byte, 1);
						p += 3;
					}
					else if (*p == '\\' && *(p + 1)) {
						char c;

						p++;
						switch (*p) {
						case 'n': c = '\n'; break;
						case 'r': c = '\r'; break;
						case 't': c = '\t'; break;
						case 'b': c = '\b'; break;
						case 'a': c = '\a'; break;
						default: c = *p; break;
						}
						current = dynstr_catlen(current, &c, 1);
					}
					else if (*p == '"') {
						/* closing quote must be followed by a space or
						 * nothing at all.
						 **/
						if (*(p + 1) && !isspace(*(p + 1))) goto err;
						done = 1;
					}
					else if (!*p) {
						/* unterminated quotes */
						goto err;
					}
					else {
						current = dynstr_catlen(current, p, 1);
					}
				}
				else if (insq) {
					if (*p == '\\' && *(p + 1) == '\'') {
						p++;
						current = dynstr_catlen(current, "'", 1);
					}
					else if (*p == '\'') {
						/* closing quote must be followed by a space or
						 * nothing at all. */
						if (*(p + 1) && !isspace(*(p + 1))) goto err;
						done = 1;
					}
					else if (!*p) {
						/* unterminated quotes */
						goto err;
					}
					else {
						current = dynstr_catlen(current, p, 1);
					}
				}
				else {
					switch (*p) {
					case ' ':
					case '\n':
					case '\r':
					case '\t':
					case '\0':
						done = 1;
						break;
					case '"':
						inq = 1;
						break;
					case '\'':
						insq = 1;
						break;
					default:
						current = dynstr_catlen(current, p, 1);
						break;
					}
				}
				if (*p) p++;
			}
			/* add the token to the vector */
			vector = realloc(vector, ((*argc) + 1) * sizeof(char*));
			vector[*argc] = current;
			(*argc)++;
			current = NULL;
		}
		else {
			/* Even on empty input string return something not NULL. */
			if (vector == NULL) vector = malloc(sizeof(void*));
			return vector;
		}
	}

err:
	while ((*argc)--)
		dynstr_free(vector[*argc]);
	free(vector);
	if (current) dynstr_free(current);
	*argc = 0;
	return NULL;
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: dynstr_mapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the dynstr string pointer, that is always the same
 * as the input pointer since no resize is needed.
 **/
dynstr dynstr_mapchars(dynstr s, const char* from, const char* to, size_t setlen)
{
	size_t j, i, l = dynstr_len(s);

	for (j = 0; j < l; j++) {
		for (i = 0; i < setlen; i++) {
			if (s[j] == from[i]) {
				s[j] = to[i];
				break;
			}
		}
	}
	return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an dynstr string.
 **/
dynstr dynstr_join(char** argv, int argc, char* sep, size_t seplen)
{
	dynstr join = dynstr_empty();
	int j;

	for (j = 0; j < argc; j++) {
		join = dynstr_cat(join, argv[j]);
		if (j != argc - 1) join = dynstr_catlen(join, sep, seplen);
	}
	return join;
}

/* Like dynstr_join, but joins an array of dynstr strings.
 **/
dynstr dynstr_join_dynstr(dynstr* argv, int argc, const char* sep, size_t seplen)
{
	dynstr join = dynstr_empty();
	int j;

	for (j = 0; j < argc; j++) {
		join = dynstr_cat_dynstr(join, argv[j]);
		if (j != argc - 1) join = dynstr_catlen(join, sep, seplen);
	}
	return join;
}

#define STRING_BLOOM_ADD(mask, ch) ((mask |= (1UL << ((ch) & (sizeof(mask) * 8 - 1)))))
#define STRING_BLOOM(mask, ch) ((mask &  (1UL << ((ch) & (sizeof(mask) * 8 -1)))))

enum {
	FAST_COUNT,
	FAST_SEARCH,
	FAST_RSEARCH,
};

static int fastsearch(const char* s, int n,
	const char* p, int m, int maxcount, int mode)
{
	unsigned long mask;
	int skip, count = 0;
	int i, j, mlast, w;

	w = n - m;

	if (w < 0 || (mode == FAST_COUNT && maxcount == 0))
		return -1;

	/* look for special cases */
	if (m <= 1) {
		if (m <= 0)
			return -1;
		/* use special case for 1-character strings */
		if (mode == FAST_COUNT) {
			for (i = 0; i < n; i++)
				if (s[i] == p[0]) {
					count++;
					if (count == maxcount)
						return maxcount;
				}
			return count;
		}
		else if (mode == FAST_SEARCH) {
			for (i = 0; i < n; i++)
				if (s[i] == p[0])
					return i;
		}
		else {    /* FAST_RSEARCH */
			for (i = n - 1; i > -1; i--)
				if (s[i] == p[0])
					return i;
		}
		return -1;
	}

	mlast = m - 1;
	skip = mlast - 1;
	mask = 0;

	if (mode != FAST_RSEARCH) {
		/* create compressed boyer-moore delta 1 table */

		/* process pattern[:-1] */
		for (i = 0; i < mlast; i++) {
			STRING_BLOOM_ADD(mask, p[i]);
			if (p[i] == p[mlast])
				skip = mlast - i - 1;
		}
		/* process pattern[-1] outside the loop */
		STRING_BLOOM_ADD(mask, p[mlast]);

		for (i = 0; i <= w; i++) {
			/* note: using mlast in the skip path slows things down on x86 */
			if (s[i + m - 1] == p[m - 1]) {
				/* candidate match */
				for (j = 0; j < mlast; j++)
					if (s[i + j] != p[j])
						break;
				if (j == mlast) {
					/* got a match! */
					if (mode != FAST_COUNT)
						return i;
					count++;
					if (count == maxcount)
						return maxcount;
					i = i + mlast;
					continue;
				}
				/* miss: check if next character is part of pattern */
				if (!STRING_BLOOM(mask, s[i + m]))
					i = i + m;
				else
					i = i + skip;
			}
			else {
				/* skip: check if next character is part of pattern */
				if (!STRING_BLOOM(mask, s[i + m]))
					i = i + m;
			}
		}
	}
	else {    /* FAST_RSEARCH */

		/* create compressed boyer-moore delta 1 table */

		/* process pattern[0] outside the loop */
		STRING_BLOOM_ADD(mask, p[0]);
		/* process pattern[:0:-1] */
		for (i = mlast; i > 0; i--) {
			STRING_BLOOM_ADD(mask, p[i]);
			if (p[i] == p[0])
				skip = i - 1;
		}

		for (i = w; i >= 0; i--) {
			if (s[i] == p[0]) {
				/* candidate match */
				for (j = mlast; j > 0; j--)
					if (s[i + j] != p[j])
						break;
				if (j == 0)
					/* got a match! */
					return i;
				/* miss: check if previous character is part of pattern */
				if (i > 0 && !STRING_BLOOM(mask, s[i - 1]))
					i = i - m;
				else
					i = i - skip;
			}
			else {
				/* skip: check if previous character is part of pattern */
				if (i > 0 && !STRING_BLOOM(mask, s[i - 1]))
					i = i - m;
			}
		}
	}

	if (mode != FAST_COUNT)
		return -1;
	return count;
}

/* find first substr `p` of `s`.
 **/
int dynstr_find(const char* s, int len, const char* p, int m)
{
	if (len <= 0) return -1;
	return fastsearch(s, len, p, m, -1, FAST_SEARCH);
}

/* find first substr `p` of `s` from right.
 **/
int dynstr_rfind(const char* s, int len, const char* p, int m)
{
	if (len <= 0) return -1;
	return fastsearch(s, len, p, m, -1, FAST_RSEARCH);
}

/* split `s` with separator in whitespace character.
 **/
dynstr* dynstr_splitwhitespace(const char* s, int len, int* count)
{
	int elements = 0, slots = 5, start = 0, i = 0, j = 0;
	dynstr* tokens;
	if (len < 0) return NULL;
	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL)
		return NULL;
	if (len == 0) {
		*count = 0;
		return tokens;
	}
	while (1) {
		while (j < len && isspace((unsigned char)s[j])) j++;
		if (j == len) break;
		start = j++;
		while (j < len && !isspace((unsigned char)s[j])) j++;
		/* make sure there is room for the next element and the final one */
		if (slots < elements + 2) {
			dynstr* newtokens;

			slots *= 2;
			newtokens = realloc(tokens, sizeof(dynstr) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}
		tokens[elements] = dynstr_newlen(s + start, j - start);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
	}
	*count = elements;
	return tokens;

cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* split `s` with separator in whitespace character from right.
 **/
dynstr* dynstr_rsplitwhitespace(const char* s, int len, int* count)
{
	int elements = 0, slots = 5, start, i, j;
	dynstr* tokens;
	if (len < 0) return NULL;
	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL)
		return NULL;
	if (len == 0) {
		*count = 0;
		return tokens;
	}

	j = start = len - 1;
	while (1) {
		while (j >= 0 && isspace((unsigned char)s[j])) j--;
		if (j < 0) break;
		start = j--;
		while (j >= 0 && !isspace((unsigned char)s[j])) j--;
		/* make sure there is room for the next element and the final one */
		if (slots < elements + 2) {
			dynstr* newtokens;

			slots *= 2;
			newtokens = realloc(tokens, sizeof(dynstr) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}
		tokens[elements] = dynstr_newlen(s + j + 1, start - j);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
	}
	*count = elements;
	return tokens;
cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* split `s` with separator in character `ch`.
 **/
dynstr* dynstr_splitchar(const char* s, int len, const char ch, int* count)
{
	int elements = 0, slots = 5, start = 0, i = 0, j = 0;
	dynstr* tokens;
	if (len < 0) return NULL;
	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL)
		return NULL;
	if (len == 0) {
		*count = 0;
		return tokens;
	}

	for (j = 0; j < len; j++) {
		if (s[j] == ch) {
			/* make sure there is room for the next element and the final one */
			if (slots < elements + 2) {
				dynstr* newtokens;

				slots *= 2;
				newtokens = realloc(tokens, sizeof(dynstr) * slots);
				if (newtokens == NULL) goto cleanup;
				tokens = newtokens;
			}
			tokens[elements] = dynstr_newlen(s + start, j - start);
			if (tokens[elements] == NULL) goto cleanup;
			elements++;
			start = j = j + 1;
		}
	}
	if (start <= len) {
		tokens[elements] = dynstr_newlen(s + start, len - start);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
	}
	*count = elements;
	return tokens;

cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* split `s` with separator in character `ch` from right.
 **/
dynstr* dynstr_rsplitchar(const char* s, int len, const char ch, int* count)
{
	int elements = 0, slots = 5, start = 0, i = 0, j = 0;
	dynstr* tokens;
	if (len < 0) return NULL;
	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL)
		return NULL;
	if (len == 0) {
		*count = 0;
		return tokens;
	}

	start = j = len - 1;
	for (; j >= 0; j--) {
		if (s[j] == ch) {
			/* make sure there is room for the next element and the final one */
			if (slots < elements + 2) {
				dynstr* newtokens;

				slots *= 2;
				newtokens = realloc(tokens, sizeof(dynstr) * slots);
				if (newtokens == NULL) goto cleanup;
				tokens = newtokens;
			}
			tokens[elements] = dynstr_newlen(s + j + 1, start - j);
			if (tokens[elements] == NULL) goto cleanup;
			elements++;
			start = j = j - 1;
		}
	}
	if (start >= -1) {
		tokens[elements] = dynstr_newlen(s, start + 1);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
	}
	*count = elements;
	return tokens;
cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* Split 's' with separator in 'sep'. An array
 * of dynstr strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 *
 * dynstr_split("_-_foo_-_bar", "_-_"); will return three
 * elements "", "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. dynstr_split() is just the
 * same function but for zero-terminated strings.
 **/
dynstr* dynstr_split(const char* s, int len, const char* sep, int seplen, int* count)
{
	int elements = 0, slots = 5, start = 0, i, j;
	dynstr* tokens;

	if (seplen < 1 || len < 0) return NULL;

	if (seplen == 1)
		return dynstr_splitchar(s, len, sep[0], count);

	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL) return NULL;

	if (len == 0) {
		*count = 0;
		return tokens;
	}

	while (1) {
		j = fastsearch(s + start, len - start, sep, seplen, -1, FAST_SEARCH);
		if (j < 0) break;
		/* make sure there is room for the next element and the final one */
		if (slots < elements + 2) {
			dynstr* newtokens;

			slots *= 2;
			newtokens = realloc(tokens, sizeof(dynstr) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}
		tokens[elements] = dynstr_newlen(s + start, j);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
		start += j + seplen;
	}

	/* Add the final element. We are sure there is room in the tokens array. */
	tokens[elements] = dynstr_newlen(s + start, len - start);
	if (tokens[elements] == NULL) goto cleanup;
	elements++;
	*count = elements;
	return tokens;

cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

dynstr* dynstr_rsplit(const char* s, int len, const char* sep, int seplen, int* count)
{
	int elements = 0, slots = 5, start = 0, i, j;
	dynstr* tokens;

	if (seplen < 1 || len < 0) return NULL;

	if (seplen == 1)
		return dynstr_rsplitchar(s, len, sep[0], count);

	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL) return NULL;

	if (len == 0) {
		*count = 0;
		return tokens;
	}

	start = len;
	while (1) {
		j = fastsearch(s, start, sep, seplen, -1, FAST_RSEARCH);
		if (j < 0) break;
		/* make sure there is room for the next element and the final one */
		if (slots < elements + 2) {
			dynstr* newtokens;

			slots *= 2;
			newtokens = realloc(tokens, sizeof(dynstr) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}
		tokens[elements] = dynstr_newlen(s + j + seplen, start - j - seplen);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
		start = j;
	}

	/* Add the final element. We are sure there is room in the tokens array. */
	tokens[elements] = dynstr_newlen(s, start);
	if (tokens[elements] == NULL) goto cleanup;
	elements++;
	*count = elements;
	return tokens;

cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* split `s` with separator in `CRLF|CR|LF` line break characters.
 **/
dynstr* dynstr_splitlines(const char* s, int len, int* count, int keepends)
{
	int elements = 0, slots = 5, start = 0, i, j;
	dynstr* tokens;

	if (len < 0) return NULL;

	tokens = malloc(sizeof(dynstr) * slots);
	if (tokens == NULL) return NULL;

	if (len == 0) {
		*count = 0;
		return tokens;
	}

	for (start = j = 0; j < len;) {
		int eol;

		/* Find a line and append it */
		while (j < len && s[j] != '\r' && s[j] != '\n')
			j++;

		/* Skip the line break reading CRLF as one line break */
		eol = j;
		if (j < len) {
			if (s[j] == '\r' && j + 1 < len && s[j + 1] == '\n')
				j += 2;
			else
				j++;
			if (keepends)
				eol = j;
		}
		/* make sure there is room for the next element and the final one */
		if (slots < elements + 2) {
			dynstr* newtokens;

			slots *= 2;
			newtokens = realloc(tokens, sizeof(dynstr) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}
		tokens[elements] = dynstr_newlen(s + start, eol - start);
		if (tokens[elements] == NULL) goto cleanup;
		elements++;
		start = j;
	}
	*count = elements;
	return tokens;

cleanup:
	for (i = 0; i < elements; i++)
		dynstr_free(tokens[i]);
	free(tokens);
	*count = 0;
	return NULL;
}

/* Free the result returned by dynstr_(rsplit|split)*(),
 * or do nothing if 'tokens' is NULL.
 **/
void dynstr_freesplitres(dynstr* tokens, int count)
{
	if (!tokens) return;
	while (count--)
		dynstr_free(tokens[count]);
	free(tokens);
}

static const char base64_standard[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz0123456789+/";

static int decode_one_standard(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (c - 'A');
	if (c >= 'a' && c <= 'z')
		return (c - 'a' + 26);
	if (c >= '0' && c <= '9')
		return (c - '0' + 52);
	if (c == '+')
		return (62);
	if (c == '/')
		return (63);
	if (c == '=')
		return (-1);

	return (-2);
}

dynstr b64encode_standard(const unsigned char* input, size_t len)
{
	dynstr output;
	char tmp[5];
	unsigned int i = 0;

	tmp[4] = '\0';

	output = dynstr_newlen(NULL, (len + 2) / 3 * 4);
	dynstr_clear(output);
	while (i < len) {
		uint32_t c = input[i++] << 16;
		if (i < len)
			c |= input[i] << 8;
		i++;
		if (i < len)
			c |= input[i];
		i++;

		tmp[0] = base64_standard[(c & 0x00fc0000) >> 18];
		tmp[1] = base64_standard[(c & 0x0003f000) >> 12];
		tmp[2] = i > len + 1 ? '=' : base64_standard[(c & 0x00000fc0) >> 6];
		tmp[3] = i > len ? '=' : base64_standard[c & 0x0000003f];

		output = dynstr_catlen(output, tmp, 4);
	}
	return output;
}

dynstr b64decode_standard(const char* input, size_t len)
{
	int typ[4];
	uint8_t buf[4];
	unsigned int i, j;
	dynstr output;

	buf[3] = '\0';
	output = dynstr_newlen(NULL, len / 4 * 3);
	dynstr_clear(output);
	/*
	* Valid encoded strings are a multiple of 4 characters long:
	*/
	if (len % 4 != 0)
		return NULL;

	for (i = 0; i < len; i += 4) {
		for (j = 0; j < 4; j++)
			typ[j] = decode_one_standard(input[i + j]);

		/*
		* Filler must be contiguous on the right of the input
		* string, and at most two bytes:
		*/
		if (typ[0] == -1 || typ[1] == -1)
			goto clean_up;
		if (typ[2] == -1 && typ[3] != -1)
			goto clean_up;

		buf[0] = (typ[0] << 2) | (typ[1] >> 4);
		if (typ[2] != -1)
			buf[1] = ((typ[1] & 0x0f) << 4) | (typ[2] >> 2);
		else
			buf[1] = '\0';
		if (typ[3] != -1)
			buf[2] = ((typ[2] & 0x03) << 6) | typ[3];
		else
			buf[2] = '\0';

		output = dynstr_catlen(output, (const char*)buf, 3);
	}

	return output;
clean_up:
	dynstr_free(output);
	return NULL;
}

#ifdef DYNSTR_TEST_MAIN
#include <stdio.h>
#include "testhelp.h"

int main(void)
{
	{
		struct dynstr_hdr* sh;
		dynstr x = dynstr_new("foo"), y;

		test_cond("Create a string and obtain the length",
			dynstr_len(x) == 3 && memcmp(x, "foo\0", 4) == 0)

			dynstr_free(x);
		x = dynstr_newlen("foo", 2);
		test_cond("Create a string with specified length",
			dynstr_len(x) == 2 && memcmp(x, "fo\0", 3) == 0)

			x = dynstr_cat(x, "bar");
		test_cond("Strings concatenation",
			dynstr_len(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

		x = dynstr_cpy(x, "a");
		test_cond("dynstr_cpy() against an originally longer string",
			dynstr_len(x) == 1 && memcmp(x, "a\0", 2) == 0)

			x = dynstr_cpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
		test_cond("dynstr_cpy() against an originally shorter string",
			dynstr_len(x) == 33 &&
			memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 33) == 0)

			dynstr_free(x);
		x = dynstr_catprintf(dynstr_empty(), "%d", 123);
		test_cond("dynstr_catprintf() seems working in the base case",
			dynstr_len(x) == 3 && memcmp(x, "123\0", 4) == 0)

			dynstr_free(x);
		x = dynstr_new("xxciaoyyy");
		dynstr_trim(x, "xy");
		test_cond("dynstr_trim() correctly trims characters",
			dynstr_len(x) == 4 && memcmp(x, "ciao\0", 5) == 0)

			y = dynstr_dup(x);
		dynstr_range(y, 1, 1);
		test_cond("dynstr_range(...,1,1)",
			dynstr_len(y) == 1 && memcmp(y, "i\0", 2) == 0)

			dynstr_free(y);
		y = dynstr_dup(x);
		dynstr_range(y, 1, -1);
		test_cond("dynstr_range(...,1,-1)",
			dynstr_len(y) == 3 && memcmp(y, "iao\0", 4) == 0)

			dynstr_free(y);
		y = dynstr_dup(x);
		dynstr_range(y, -2, -1);
		test_cond("dynstr_range(...,-2,-1)",
			dynstr_len(y) == 2 && memcmp(y, "ao\0", 3) == 0)

			dynstr_free(y);
		y = dynstr_dup(x);
		dynstr_range(y, 2, 1);
		test_cond("dynstr_range(...,2,1)",
			dynstr_len(y) == 0 && memcmp(y, "\0", 1) == 0)

			dynstr_free(y);
		y = dynstr_dup(x);
		dynstr_range(y, 1, 100);
		test_cond("dynstr_range(...,1,100)",
			dynstr_len(y) == 3 && memcmp(y, "iao\0", 4) == 0)

			dynstr_free(y);
		y = dynstr_dup(x);
		dynstr_range(y, 100, 100);
		test_cond("dynstr_range(...,100,100)",
			dynstr_len(y) == 0 && memcmp(y, "\0", 1) == 0)

			dynstr_free(y);
		dynstr_free(x);
		x = dynstr_new("foo");
		y = dynstr_new("foa");
		test_cond("dynstr_cmp(foo,foa)", dynstr_cmp(x, y) > 0)

			dynstr_free(y);
		dynstr_free(x);
		x = dynstr_new("bar");
		y = dynstr_new("bar");
		test_cond("dynstr_cmp(bar,bar)", dynstr_cmp(x, y) == 0)

			dynstr_free(y);
		dynstr_free(x);
		x = dynstr_new("aar");
		y = dynstr_new("bar");
		test_cond("dynstr_cmp(bar,bar)", dynstr_cmp(x, y) < 0)

			dynstr_free(y);
		dynstr_free(x);
		x = dynstr_newlen("\a\n\0foo\r", 7);
		y = dynstr_catrepr(dynstr_empty(), x, dynstr_len(x));
		test_cond("dynstr_catrepr(...data...)",
			memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0)

		{
			int oldfree;

			dynstr_free(x);
			x = dynstr_new("0");
			sh = (void*)(x - (sizeof(struct dynstr_hdr)));
			test_cond("dynstr_new() free/len buffers", sh->len == 1 && sh->free == 0);
			x = dynstrMakeRoomFor(x, 1);
			sh = (void*)(x - (sizeof(struct dynstr_hdr)));
			test_cond("dynstrMakeRoomFor()", sh->len == 1 && sh->free > 0);
			oldfree = sh->free;
			x[1] = '1';
			dynstrIncrLen(x, 1);
			test_cond("dynstrIncrLen() -- content", x[0] == '0' && x[1] == '1');
			test_cond("dynstrIncrLen() -- len", sh->len == 2);
			test_cond("dynstrIncrLen() -- free", sh->free == oldfree - 1);
		}
	}
	test_report()
		return 0;
}
#endif