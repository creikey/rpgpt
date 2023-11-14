// this is json interop in C with metadesk
#pragma once

#include "md.h"

typedef struct ServerResponse {
    bool rate_limited;
    String8 error;
    String8 ai_error;
    Node* ai_response;
} ServerResponse;

bool get_bool(Node *root, String8 field) {
    Node *maybe_child = MD_ChildFromString(root, field, 0);
    if(!NodeIsNil(maybe_child->first_child)) {
        return S8Match(maybe_child->first_child->string, S8Lit("true"), 0);
    }
    return false;
}

String8 get_string(Arena *arena, Node *root, String8 field) {
    Node *maybe_child = MD_ChildFromString(root, field, 0);
    if(!NodeIsNil(maybe_child->first_child)) {
        return S8Copy(arena, maybe_child->first_child->string);
    }
    return S8Lit("");
}

String8List get_string_array(Arena *arena, Node *root, String8 field) {
    String8List ret = {0};
    Node *maybe_child = MD_ChildFromString(root, field, 0);
    if(!NodeIsNil(maybe_child->first_child)) {
        for(Node *cur = maybe_child->first_child; !NodeIsNil(cur); cur = cur->next) {
            S8ListPush(arena, &ret, S8Copy(arena, cur->string));
        }
    }
    return ret;
}

void parse_response(Arena *arena, ServerResponse *response, Node *root) {
    response->rate_limited = get_bool(root, S8Lit("rate_limited"));
    response->error = get_string(arena, root, S8Lit("error"));
    response->ai_error = get_string(arena, root, S8Lit("ai_error"));
    response->ai_response = MD_ChildFromString(root, S8Lit("ai_response"), 0)->first_child;
}