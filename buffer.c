#include <stdlib.h>
#include <string.h>
#include "buffer.h"

buffer_ctx *buffer_new(int size) {
    buffer_ctx *ctx = (buffer_ctx *)malloc(sizeof(buffer_ctx));
    ctx->size = size > 0 ? size : BUFFER_DEFAULT_SIZE;
    ctx->curr = 0;
    ctx->buffer = (char *)malloc(ctx->size);
    return ctx;
}

void buffer_free(buffer_ctx *ctx) {
    if(ctx->buffer) free(ctx->buffer);
    free(ctx);
}

static void buffer__grow(buffer_ctx *ctx, int preferred) {
    if(ctx->size >= preferred)
	return;
    while(ctx->size < preferred)
	ctx->size <<= 1;
    ctx->buffer = (char *)realloc(ctx->buffer, ctx->size);
}

int buffer_append(buffer_ctx *ctx, const char *data, int size) {
    if(ctx->curr + size >= ctx->size)
	buffer__grow(ctx, ctx->curr + size);
    memcpy(&ctx->buffer[ctx->curr], data, size);
    return ctx->curr += size;
}

int buffer_append_string(buffer_ctx *ctx, const char *str) {
    return buffer_append(ctx, str, strlen(str));
}

int buffer_shift(buffer_ctx *ctx, int length) {
    int newlen = ctx->curr - length;
    if(newlen > 0) {
	memmove(ctx->buffer, &ctx->buffer[ctx->curr + length], newlen);
	ctx->curr = newlen;
    } else {
	ctx->curr = 0;
    }
    return ctx->curr;
}

int buffer_length(buffer_ctx *ctx) {
    return ctx->curr;
}

const char *buffer_data(buffer_ctx *ctx) {
    return ctx->buffer;
}

const char *buffer_cstr(buffer_ctx *ctx) {
    if(ctx->curr + 1 >= ctx->size)
        buffer__grow(ctx, ctx->curr + 1);

    ctx->buffer[ctx->curr] = '\0';

    return ctx->buffer;
}

// vim: ts=8 sw=4
