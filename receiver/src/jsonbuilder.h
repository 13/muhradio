#pragma once
#include <stddef.h>
#include <stdio.h>

// Append src to dst (bounded by dstSize), producing valid JSON-string content.
// Escapes control chars as \u00XX and replaces invalid UTF-8 bytes with '?', so a
// corrupted value can never yield unparseable JSON. Returns new length; sets
// *truncated (if given) when src did not fit completely.
// The single escaper for web.cpp, conf.cpp, status.h and JsonBuilder::kvs.
static size_t jsonAppendEscaped(char* dst, size_t len, size_t dstSize,
                                const char* src, bool* truncated = nullptr) {
  static const char hex[] = "0123456789abcdef";
  const unsigned char* p = (const unsigned char*)src;
  if (len >= dstSize) { if (truncated) *truncated = (*p != '\0'); return len; }
  while (*p && len + 6 < dstSize) {
    unsigned char c = *p;
    if (c == '"' || c == '\\') { dst[len++] = '\\'; dst[len++] = c; p++; }
    else if (c == '\n')        { dst[len++] = '\\'; dst[len++] = 'n'; p++; }
    else if (c == '\r')        { dst[len++] = '\\'; dst[len++] = 'r'; p++; }
    else if (c == '\t')        { dst[len++] = '\\'; dst[len++] = 't'; p++; }
    else if (c < 0x20) {
      dst[len++] = '\\'; dst[len++] = 'u'; dst[len++] = '0'; dst[len++] = '0';
      dst[len++] = hex[c >> 4]; dst[len++] = hex[c & 0xF]; p++;
    }
    else if (c < 0x80) { dst[len++] = (char)c; p++; }
    else {
      // Multi-byte UTF-8: pass through only if the continuation bytes are valid.
      int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : -1;
      bool ok = n > 0;
      for (int i = 1; ok && i <= n; i++) if ((p[i] & 0xC0) != 0x80) ok = false;
      if (ok) { for (int i = 0; i <= n; i++) dst[len++] = (char)p[i]; p += n + 1; }
      else    { dst[len++] = '?'; p++; }
    }
  }
  dst[len] = '\0';
  if (truncated) *truncated = (*p != '\0');
  return len;
}

// Minimal JSON object builder (no heap, no ArduinoJson). Writes into a
// caller-provided buffer; a key/value pair that doesn't fit is dropped whole,
// so the output is always valid, null-terminated JSON.
struct JsonBuilder {
  char*  buf;
  size_t cap;
  size_t n;
  bool   sep;

  JsonBuilder(char* b, size_t c) : buf(b), cap(c), n(0), sep(false) {
    buf[n++] = '{';
  }
  void finish() {
    buf[n++] = '}'; buf[n] = '\0'; // room reserved by _commit()
  }
  bool ok() const { return n < cap - 16; }

  void kv(const char* k, long v) {
    size_t save = n; bool ps = sep;
    _key(k); _adv(snprintf(buf+n, cap-n, "%ld", v));
    _commit(save, ps);
  }
  void kv(const char* k, float v) {
    size_t save = n; bool ps = sep;
    _key(k); _adv(snprintf(buf+n, cap-n, "%.1f", (double)v));
    _commit(save, ps);
  }
  void kvs(const char* k, const char* v) {
    size_t save = n; bool ps = sep;
    _key(k);
    bool trunc = false;
    if (n < cap) buf[n++] = '"';
    n = jsonAppendEscaped(buf, n, cap, v, &trunc);
    if (n < cap) buf[n++] = '"';
    if (trunc) { n = save; sep = ps; buf[n] = '\0'; return; }
    _commit(save, ps);
  }

private:
  void _key(const char* k) {
    if (sep && n < cap) buf[n++] = ',';
    _adv(snprintf(buf+n, cap-n, "\"%s\":", k));
    sep = true;
  }
  // snprintf returns the would-be length on truncation — never let n pass cap
  void _adv(int w) {
    n += w;
    if (n > cap - 1) n = cap - 1;
  }
  // Drop the whole pair if it (plus the closing "}\0") didn't fit — a
  // truncated key/value would make the published JSON invalid.
  void _commit(size_t save, bool prevSep) {
    if (n + 2 > cap) { n = save; sep = prevSep; buf[n] = '\0'; }
  }
};
