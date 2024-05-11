#include "cxml.h"
#include <locale.h>
#include <stdio.h>
#include <windows.h>

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
    cx_doc d = cx_parse_file(buf, false);
    cx_node* r = d.root;
    print_node(r, 0);
}