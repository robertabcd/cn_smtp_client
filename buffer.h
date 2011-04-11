#ifndef BUFFER_H
#   define BUFFER_H

typedef struct {
    char *buffer;
    int size, curr;
} buffer_ctx;

#define BUFFER_DEFAULT_SIZE (1024)

buffer_ctx *buffer_new(int size);
void	    buffer_free(buffer_ctx *ctx);

// returns current buffer length
int	    buffer_append(buffer_ctx *ctx, const char *data, int size);
int	    buffer_append_string(buffer_ctx *ctx, const char *str);
int	    buffer_shift(buffer_ctx *ctx, int length);
int         buffer_length(buffer_ctx *ctx);

const char *buffer_data(buffer_ctx *ctx);
const char *buffer_cstr(buffer_ctx *ctx);

#endif
// vim: ts=8 sw=4
