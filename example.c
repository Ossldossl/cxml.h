#include "cxml.h"
#include <locale.h>
#include <stdio.h>
#include <windows.h>

static HANDLE get_file_handle(const char* file_name, bool write) 
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
        if (err == 2) {
            printf("Datei %s wurde nicht gefunden", file_name);
            exit(-1);
        } 
        printf("Fehler beim Öffnen der Datei, %lu", err);
        exit(-1);
    }

    return hFile;
}

u64 read_file(const char* file_name, char** file_content)
{
    HANDLE hFile = get_file_handle(file_name, false);
    LARGE_INTEGER file_size_li;
    if (GetFileSizeEx(hFile, &file_size_li) == 0) {
        printf("Fehler beim Abfragen der Größe der Datei, %lu", GetLastError());
        exit(-2);
    }
    size_t file_size = file_size_li.QuadPart;

    char* buf = malloc(file_size+1);
    DWORD read = 0;
    if (ReadFile(hFile, buf, file_size, &read, null) == 0 || read == 0) {
        printf("ERROR: Fehler beim Lesen der Datei, %lu", GetLastError());
        exit(-2);
    }
    CloseHandle(hFile);
    buf[file_size] = '\0';
    *file_content = buf;
    return file_size;
}

void print_indent(u32 indent)
{
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void print_node(cx_node* node, u32 indent)
{
    if (node == null) return;
    print_indent(indent);
    printf("\"%s\" (", cx_to_utf8(node->name));
    cx_attr* attr = node->attrs;
    while (attr) {
        printf("%s=%s", cx_to_utf8(attr->key), cx_to_utf8(attr->value));
        attr = attr->next;
    }
    printf("):\n");
    ++indent;
        if (node->content.len > 0) {
            print_indent(indent);
            printf("content: %s\n", cx_to_utf8(node->content));
        }
        if (node->children) {
            print_indent(indent);
            printf("children: \n");
            indent++;
                cx_node* cur = node->children;
                for (int i = 0; i < node->child_count; i++) {
                    print_node(cur, indent);
                    cur = cur->next;
                }
            --indent;
        }
    --indent;
}

int main(int argc, char** argv)
{   
    // to output utf8 on windows
    SetConsoleOutputCP(CP_UTF8);
    char* buf = null;
    u32 file_size = read_file("test.xml", &buf);
    cx_doc d = cx_parse(buf, file_size, false);
    cx_node* r = d.root;
    print_node(r, 0);
}