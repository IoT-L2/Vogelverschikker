#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#pragma once
#include <stddef.h>

    class SimpleConfig {
    public:
        SimpleConfig(const char *namespace_name);

        bool setLine(int line, const char *text);
        bool getLine(int line, char *out_buf, size_t buf_len);
        bool clearLine(int line);
        bool clearAll();

    private:
        const char *_namespace;
    };
#ifdef __cplusplus
}
#endif

