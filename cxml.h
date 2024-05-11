#pragma once

#include <stdbool.h>
#include <stdlib.h>

typedef signed char        i8;
typedef short              i16;
typedef int                i32;
typedef long long          i64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

#define null NULL
#define max(a, b) (a>b?a:b)
#define min(a, b) (a<b?a:b)

//==== ARENA ====
typedef struct {
	void* data;
	void* cur;
	u32 last_alloc_size;
} arena_bucket;

typedef struct{
	u32 bucket_size; // up to ~4mb
	arena_bucket* buckets; 
	u8 bucket_count;
} arena;

arena arena_init(u32 bucket_size);
void* arena_alloc(arena* arena, u32 size);
void arena_free_last(arena* arena);
void arena_reset(arena* arena);
void arena_destroy(arena* arena);

// ==== STRINGS ====
typedef u32 u8chr;
// wide utf8 null terminated string
typedef struct cx_str {
    u8chr* data;
    u32 len;
} cx_str;
cx_str new_cxstr(u8chr* data, u32 len);
// converts multi-byte unicode to wide 
cx_str cxstr_from_char(char* data, u32 len);
char* cx_to_utf8(cx_str str);
#define snew_cxstr(_data) cxstr_from_char(_data, sizeof(_data))
cx_str cxstr_concat(cx_str a, cx_str b);

// ==== MAIN ====
typedef enum {
    CX_OK,
    CX_NO_PROLOG, // only an error in strict mode
    CX_INVALID_EOF_OR_CHAR,
    CX_UNEXPECTED_TEXT,
    CX_INVALID_ATTR_VALUE,
    CX_INVALID_CLOSING_TAG,
    CX_FILE_NOT_FOUND,
    //....
    CX_ERROR_COUNT,
} cx_error;

typedef struct cx_attr {
    struct cx_attr* next;
    cx_str key;
    cx_str value;
} cx_attr;

typedef struct cx_node {
    struct cx_node* parent;
    struct cx_node* children;
    u32 child_count;
    struct cx_node* next;
    struct cx_node* prev;

    cx_str name;
    cx_str content;
    u16 attr_count;
    cx_attr* attrs;
} cx_node;

typedef enum cx_encoding {
    CX_UTF8,
    CX_ISO_8859_1,
} cx_encoding;

typedef struct cx_doc {
    cx_encoding encoding;
    cx_error err;
    u32 err_loc;
    cx_node* root;
} cx_doc;

cx_doc cx_parse(char* data, u32 len, bool strict);
#ifdef _WIN32
#include <windows.h>
static HANDLE get_file_handle(const char* file_name, bool write);
u64 read_file(const char* file_name, char** file_content);
cx_doc cx_parse_file(char* filename, bool strict);
#endif