#include "cxml.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    cx_doc* cur_doc;
    cx_str content;
    u8chr* cur;
    arena arena;
} cx_parser;

cx_parser parser;

// ==== CXSTR ====
static u8 const u8_lengths[] = {
//  0 1 2 3 4 5 6 7 8 9 A B C D E F
    1,1,1,1,1,1,1,1,0,0,0,0,2,2,3,4
};
#define u8_length(s) u8_lengths[(((u8*)(s))[0] & 0xFF) >> 4]

bool u8_validate(u8chr c)
{
    if (c <= 0x7F) return true;
    if (0xC280 <= c && c <= 0xDFBF) {
        return ((c & 0xE0C0) == 0xC080);
    }
    if (0xEDA080 <= c && c <= 0xEDBFBF) {
        return false; // no utf16 surrogates
    }
    if (0xE0A080 <= c && c <= 0xEFBFBF) {
        return ((c & 0xF0C0C0) == 0xE08080);
    }
    if (0xF0908080 <= c && c <= 0xF48FBFBF) {
        return ((c & 0xF8C0C0C0) == 0xF0808080);
    }
    return false;
}

cx_str cxstr_from_char_malloc(char* data, u32 len)
{
    if (len == 0) {
        len = strlen(data);
    }
    cx_str result;
    result.data = malloc(len * sizeof(u32));

    char* cur_src = data;
    u8chr* cur_dest = result.data;
    while (*cur_src != 0) {
        if ((unsigned char)*cur_src <= 0x7F) {
            // normal ascii character
            *cur_dest = *cur_src;
            cur_dest++; cur_src++;
            continue;
        }
        
        u32 encoded = 0;
        u8 expected_byte_length = 0;
        if ((*cur_src & 0b11100000) == 0b11000000) {
            // two byte 
            encoded |= *cur_src & 0b11111;
            expected_byte_length = 1;
        } else if ((*cur_src & 0b11110000) == 0b11100000) {
            encoded |= *cur_src & 0b1111;
            expected_byte_length = 2;
        } else if ((*cur_src & 0b11111000) == 0b11110000) {
            encoded |= *cur_src & 0b111;
            expected_byte_length = 3;
        } else {
            // just skip this character and insert the replacement character
            *cur_dest = 0xFFFD; cur_dest++;
            cur_src++;
            continue;
        }
        cur_src++;
        for (u8 i = 0; i < expected_byte_length; i++) {
            if ((*cur_src & 0b11000000) != 0b10000000) {
                // unexpected end of utf8 byte sequence, place replacement character
                *cur_dest = 0xFFFD; cur_dest++;
                continue;
            }
            encoded <<= 6;
            encoded |= *cur_src & 0b00111111;
            cur_src++;
        }
        *cur_dest = encoded;
        cur_dest++;
    }
    result.len = ((u64)cur_dest - (u64)result.data) / sizeof(u32);
    result.data = realloc(result.data, (result.len+1) * sizeof(u32));
    result.data[result.len] =  0;
    return result;
}

cx_str cxstr_from_char(char* data, u32 len)
{
    if (len == 0) {
        len = strlen(data);
    }
    cx_str result;
    result.data = arena_alloc(&parser.arena, len * sizeof(u32));

    char* cur_src = data;
    u8chr* cur_dest = result.data;
    while (*cur_src != 0) {
        if ((unsigned char)*cur_src <= 0x7F) {
            // normal ascii character
            *cur_dest = *cur_src;
            cur_dest++; cur_src++;
            continue;
        }
        
        u32 encoded = 0;
        u8 expected_byte_length = 0;
        if ((*cur_src & 0b11100000) == 0b11000000) {
            // two byte 
            encoded |= *cur_src & 0b11111;
            expected_byte_length = 1;
        } else if ((*cur_src & 0b11110000) == 0b11100000) {
            encoded |= *cur_src & 0b1111;
            expected_byte_length = 2;
        } else if ((*cur_src & 0b11111000) == 0b11110000) {
            encoded |= *cur_src & 0b111;
            expected_byte_length = 3;
        } else {
            // just skip this character and insert the replacement character
            *cur_dest = 0xFFFD; cur_dest++;
            cur_src++;
            continue;
        }
        cur_src++;
        for (u8 i = 0; i < expected_byte_length; i++) {
            if ((*cur_src & 0b11000000) != 0b10000000) {
                // unexpected end of utf8 byte sequence, place replacement character
                *cur_dest = 0xFFFD; cur_dest++;
                continue;
            }
            encoded <<= 6;
            encoded |= *cur_src & 0b00111111;
            cur_src++;
        }
        *cur_dest = encoded;
        cur_dest++;
    }
    result.len = ((u64)cur_dest - (u64)result.data) / sizeof(u32);
    arena_free_last(&parser.arena);
    result.data = arena_alloc(&parser.arena, (result.len+1) * sizeof(u32));
    result.data[result.len] =  0;
    return result;
}

char* cx_to_utf8(cx_str str)
{
    const char first_byte_mark_table[] = { 0, 0, 0b11000000, 0b11100000, 0b11110000 };
    char* result = arena_alloc(&parser.arena, str.len * sizeof(u8chr)+1);
    char* cur = result;
    u32 bytelen = 0;
    for(int i = 0; i < str.len; i++) 
    {
        u8chr c = str.data[i];
        if (c < 0x80) {
            *cur = c;
            cur++;
            continue;
        }
        else if ( c < 0x800 ) {
            bytelen = 2;
        }
        else if ( c < 0x10000 ) {
            bytelen = 3;
        }
        else if ( c < 0x200000 ) {
            bytelen = 4;
        }
        else {
            *cur = '?'; cur++;
            continue;
        }
        cur += bytelen;
        switch (bytelen) {
            case 4: {
                --cur;
                *cur = (c & 0b111111) | 0b10000000;
                c >>= 6;
            }// fallthrough
            case 3: {
                --cur;
                *cur = (c & 0b111111) | 0b10000000;
                c >>= 6;
            }
            case 2: {
                --cur;
                *cur = (c & 0b111111) | 0b10000000;
                c >>= 6;
            }
            case 1: {
                --cur;
                *cur = (c & 0b111111) | first_byte_mark_table[bytelen];
                c >>= 6;
            }
        }
        cur += bytelen;
    }
    u32 actual_length = (u64)cur - (u64)result;
    arena_free_last(&parser.arena);
    result = arena_alloc(&parser.arena, actual_length+1);
    result[actual_length] = 0;
    return result;
}

u32 cx_strlen(u8chr* data)
{
    u8chr* cur = data;
    while(*cur != 0) {
        cur++;
    }
    return (u64)cur - (u64)data;
}

cx_str new_cxstr(u8chr *data, u32 len)  
{
    if (len == 0) {
        len = cx_strlen(data);
    }
    cx_str result = {
        .data = data, .len = len
    };
    return result;
}

bool cx_strcmp(cx_str a, cx_str b)
{
    if (a.len != b.len) return false;
    for (int i = 0; i < a.len; i++) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

//==== ARENA ====
arena arena_init(u32 bucket_size)
{
	arena result;
	result.bucket_count = 1;
	result.bucket_size = bucket_size;
	result.buckets = malloc(1 * sizeof(arena_bucket));
	arena_bucket* bucket = &result.buckets[0];
	bucket->data = malloc(bucket_size);
	bucket->cur = bucket->data;
	bucket->last_alloc_size = 0;
	return result;
}

void* arena_alloc(arena* arena, u32 size)
{
	arena_bucket* bucket = &arena->buckets[arena->bucket_count-1];
	u64 diff = (u64)((u64)bucket->cur + size) - (u64)bucket->data;
	if (diff > arena->bucket_size) {
		// alloc new bucket
		arena->bucket_count++;
		arena->buckets = realloc(arena->buckets, arena->bucket_count * sizeof(arena_bucket));
		bucket = &arena->buckets[arena->bucket_count-1];
		bucket->data = malloc(arena->bucket_size);
		bucket->cur = bucket->data;
		bucket->last_alloc_size = 0;
	}
	void* result = bucket->cur;
	bucket->cur = ((char*)bucket->cur) + size;
    bucket->last_alloc_size = size;
	return result;
}

void arena_free_last(arena* arena) 
{
	arena_bucket* bucket = &arena->buckets[arena->bucket_count-1];
	bucket->cur = ((char*)bucket->cur) - bucket->last_alloc_size;
}

void arena_reset(arena* arena)
{
	// free all other buckets, except the first one
	arena->buckets = realloc(arena->buckets, sizeof(arena_bucket));
	arena_bucket* bucket = &arena->buckets[0];
	bucket->cur = bucket->data;
	arena->bucket_count = 1;
}

void arena_destroy(arena* arena)
{
	for (int i = 0; i < arena->bucket_count; i++) {
		arena_bucket* bucket = &arena->buckets[i];
		free(bucket->data);
	}
	free(arena->buckets);
}

// ==== MAIN ====
static u8chr get_cur(void)
{
    return *parser.cur;
}

static u8chr peek(void)
{
    return *parser.cur+1;
}

static u8chr get_next(void)
{
    parser.cur++;
    return *parser.cur;
}

static bool match_stringc(char* expected)
{
    char* cur = expected;
    u8chr* start = parser.cur;
    while (*cur != 0) {
        if (*cur != *parser.cur) { 
            parser.cur = start;
            return false;
        }
        cur++;
        parser.cur++;
    }
    return true;
}

static bool match_string(cx_str expected)
{
    for (int i = 0; i < expected.len; i++) {
        if (expected.data[i] != parser.cur[i]) return false;
    }   
    parser.cur += expected.len;
    return true;
}

static bool match_c(char expected)
{
    if (*parser.cur == expected) { get_next(); return true; }
    return false;
}

static bool is_whitespace(char c)
{
    return c == '\x20' || c == '\x9' || c == '\x0D' || c == '\x0A';
}

u8chr skip_whitespace(void)
{
    u8chr cur = *parser.cur;
    while (is_whitespace(cur)) {
        cur = get_next();
    }
    return cur;
}

static bool cx_parse_prolog(void)
{
    u8chr c = get_cur();
    if (!match_stringc("<?xml ")) return false;
// TODO
    while (true) {
        if (c == 0) return false;
        if (c == '>') { get_next(); return true; }
        c = get_next();
    }

    return true;
}

void skip_comment()
{
    u8chr c = get_cur();
    while (true) {
        if (match_c('-')
         && match_c('-')
         && match_c('>')) { return; }
        c = get_next();
    }
}

u8chr skip_until(u8chr until)
{
    u8chr c = get_cur();
    while (c != 0 && c != until) {
        c = get_next();
    }
    return get_next();
}

cx_str parse_name(void)
{
    u8chr* start = parser.cur;
    u8chr c = *parser.cur;
    while (c != 0 && !is_whitespace(c) && c != '=' && c != '>') {
        c = get_next();
    }
    u16 len = ((u64)parser.cur - (u64)start) / sizeof(u32);

    cx_str result;
    result.data = arena_alloc(&parser.arena, (len+1) * sizeof(u32));
    memcpy(result.data, start, (len+1)*sizeof(u32));
    result.data[len] = 0;
    result.len = len;
    return result;
}

cx_str parse_str(u8chr expected_end)
{
    u8chr* start = parser.cur;
    u8chr c = *parser.cur;
    while (c != 0 && c != expected_end) {
        c = get_next();
    }
    u32 len = ((u64)parser.cur - (u64)start) / sizeof(u32);
    get_next();

    cx_str result;
    result.data = arena_alloc(&parser.arena, (len+1) * sizeof(u32));
    memcpy(result.data, start, (len) * sizeof(u32));
    result.data[len] = 0;
    result.len = len;
    return result;
}

cx_node* make_node(cx_str name, cx_node* parent)
{
    cx_node* result = arena_alloc(&parser.arena, sizeof(cx_node));
    memset(result, 0, sizeof(cx_node));
    result->name = name; result->parent = parent;
    return result;
}

cx_attr* make_attr(void)
{
    cx_attr* result = arena_alloc(&parser.arena, sizeof(cx_attr));
    memset(result, 0, sizeof(cx_attr));
    return result;
}

cx_node* cx_parse_node(cx_node* parent, bool allow_content) 
{
    u8chr c = skip_whitespace();
    if (c == 0) return null;
    if (c == '<') {
        c = get_next();
        if (match_c('!')) {
            parser.cur += 3;
            skip_comment();
            return cx_parse_node(parent, allow_content);
        } 
        if (match_c('/')) return null;

        cx_node* result = make_node(parse_name(), parent);
        c = get_cur();
        cx_attr* last_attr = null;
        while (c != '>') {
            c = skip_whitespace();
            if (c == '>') break;
            result->attr_count++;
            cx_attr* attr = make_attr();
            if (last_attr) {
                last_attr->next = attr;
            } else {
                result->attrs = attr;
            }
            last_attr = attr;
            attr->key = parse_name();
            //c = skip_whitespace();
            if (match_c('=')) {
                c = skip_whitespace();
                if (match_c('"')) {
                    attr->value = parse_str('"');
                } else if (match_c('\'')) {
                    attr->value = parse_str('\'');
                } else {
                    parser.cur_doc->err = CX_INVALID_ATTR_VALUE;
                    parser.cur_doc->err_loc = (u64)parser.cur - (u64)parser.content.data;
                    c = skip_until('>'); break;
                }
            }
            c = get_cur();
        }
        c = get_next();
        cx_node* last_node = null;
        while (true) {
            cx_node* child = cx_parse_node(result, true);
            if (child == null) break;

            if (last_node) {
                last_node->next = child;
                child->prev = last_node;
            } else {
                result->children = child;
            }
            last_node = child;
            result->child_count++;
        }
        if (match_string(result->name)) {
            skip_until('>');
        } else {
            parser.cur_doc->err = CX_INVALID_CLOSING_TAG;
            parser.cur_doc->err_loc = (u64)parser.cur - (u64)parser.content.data;
            skip_until('<');
        }
        return result;
    }
    // else add content and then look for new nodes
    if (!allow_content || parent == null) return null;
    u8chr* start = parser.cur;
    while (c != '<' && c != 0) {
        c = get_next();
    }
    u32 len = ((u64)parser.cur - (u64)start) / sizeof(u32);
    cx_str content; content.len = len;
    content.data = arena_alloc(&parser.arena, (len+1)*sizeof(u32));
    memcpy(content.data, start, len*sizeof(u32));
    content.data[len] = 0;

    parent->content = content;
    return cx_parse_node(parent, true);
}

cx_doc cx_parse(char* data, u32 len, bool strict)
{
    parser.arena = arena_init(524288);
    cx_str content = cxstr_from_char_malloc(data, len);
    
    cx_doc result = {0};
    parser.cur_doc = &result;
    parser.content = content;
    parser.cur = parser.content.data;;


    // check for utf8 bom
    if (*(u32*)parser.cur == 0xEFBBBF) {
        parser.cur += 3;
    }
    
    bool has_prolog = cx_parse_prolog();
    if (strict && !has_prolog) {
        result.err = CX_NO_PROLOG; result.err_loc = 0;
        return result;
    }
    result.root = cx_parse_node(null, false);

    free(content.data);
    return result;
}

#ifdef _WIN32
#include <windows.h>

static HANDLE cx_get_file_handle(const char* file_name, bool write) 
{
	HANDLE hFile;
	hFile = CreateFile(file_name,
		write ? GENERIC_WRITE : GENERIC_READ,
		write ? 0 : FILE_SHARE_READ,
		null,
		write ? CREATE_ALWAYS : OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL ,
		null);

    if (hFile == INVALID_HANDLE_VALUE) 
    {
        DWORD err = GetLastError();
        return null;
    }

    return hFile;
}

u64 cx_get_file_size(const char* file_name)
{
    HANDLE hfile = cx_get_file_handle(file_name, false);
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(hfile, &file_size) == 0) return 0;

    return file_size.QuadPart;
}

u64 cx_read_file(const char* file_name, char** file_content)
{
    HANDLE hFile = cx_get_file_handle(file_name, false);
    LARGE_INTEGER file_size_li;
    if (GetFileSizeEx(hFile, &file_size_li) == 0) {
        return null;
    }
    size_t file_size = file_size_li.QuadPart;

    char* buf = malloc(file_size+1);
    DWORD read = 0;
    if (ReadFile(hFile, buf, file_size, &read, null) == 0 || read == 0) {
        return null;
    }
    CloseHandle(hFile);
    buf[file_size] = '\0';
    *file_content = buf;
    return file_size;
}

cx_doc cx_parse_file(char* filename, bool strict)
{
    char* buf;
    u64 file_size = read_file(filename, &buf);
    cx_doc result;
    if (file_size == 0) {
        result.err = CX_FILE_NOT_FOUND;
        return result;
    }
    return cx_parse(buf, file_size, strict);
}
#endif