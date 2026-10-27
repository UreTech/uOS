#include <u_kernel/util/u_ctypes.h>

char ntoc(uint8_t val);

u64 pof(u64 base, u64 exponent);

char *ulltoa(uint64_t val, char *buf);

char *lltoa(int64_t val, char *buf);

char *ftoa(float val, int precision, char *buf);

char *ulltohexa(uint64_t val, char *buf);

// !CAUTION! free the output buffer when its job is done
char *append_strs(char **strs, size_t count);

void parse_read(char* output, const char* input, char seperator, size_t index);
void w_parse_read(wide_char_t* output, const wide_char_t* input, wide_char_t seperator, size_t index);
void w_parse_read_multiple(wide_char_t* output, const wide_char_t* input, wide_char_t seperator, size_t start_index, size_t count);
size_t parse_read_count(const char* input, char seperator);
size_t w_parse_read_count(const wide_char_t* input, wide_char_t seperator);

size_t strlen(const char *str, size_t maxSize);
size_t wstrlen(const wide_char_t *str, size_t maxSize);

char toupper(char ch);

// be careful! can lock down the kernel!
char *strcpy(char *dst, const char *src);

// be careful! can lock down the kernel!
int strcmp(const char *str0, const char *str1);
int wstrcmp(const wide_char_t *str0, const wide_char_t *str1);

// wide string convertion
void char_to_utf16_buf(const char* _char, wide_char_t* _wchar);
void utf16_to_char_buf(const wide_char_t* _wchar, char* _char);

// separates last entry from rest
typedef struct
{
    const wide_char_t* path;
    const wide_char_t* file_name;
}_separated_path;

_separated_path separate_path(const wide_char_t* path); // NOTE: DO NOT FORGET TO FREE OUTPUT
