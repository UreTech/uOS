#include <u_kernel/util/u_cstr_util.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/memory/u_memory.h>

char ntoc(uint8_t val)
{
	if (val <= 9)
	{
		return '0' + val;
	}
	else
	{
		return 'E';
	}
}

u64 pof(u64 base, u64 exponent)
{
	u64 result = 1;
	while (exponent--)
	{
		result *= base;
	}
	return result;
}

size_t strlen(const char *str, size_t maxSize)
{
    for (size_t i = 0; i < maxSize; i++)
    {
        if (str[i] == '\0')
            return i;
    }

    return 0;
}

size_t wstrlen(const wide_char_t *str, size_t maxSize){
    for (size_t i = 0; i < maxSize; i++)
    {
        if (str[i] == '\0')
            return i;
    }

    return 0;
}

char *ulltoa(uint64_t value, char *buf)
{
	char buffer[22];
	char *ptr = &buffer[21];
	*ptr = '\0';

	if (value == 0)
	{
		*(--ptr) = '0';
	}
	else
	{
		while (value > 0)
		{
			*(--ptr) = '0' + (value % 10);
			value /= 10;
		}
	}

	strcpy(buf, ptr);
	return buf;
}

char *lltoa(int64_t value, char *buf)
{
	char buffer[22];
	char *ptr = &buffer[21];
	*ptr = '\0';

	uint64_t uval;
	int negative = 0;

	if (value < 0)
	{
		negative = 1;

		uval = (uint64_t)(-(value + 1)) + 1;
	}
	else
	{
		uval = (uint64_t)value;
	}

	if (uval == 0)
	{
		*(--ptr) = '0';
	}
	else
	{
		while (uval > 0)
		{
			*(--ptr) = '0' + (uval % 10);
			uval /= 10;
		}
	}

	if (negative)
	{
		*(--ptr) = '-';
	}

	strcpy(buf, ptr);
	return buf;
}

char *ftoa(float val, int precision, char *buf)
{
	char buffer[25] = {0};
	int i = 0;

	if (val < 0)
	{
		buffer[i++] = '-';
		val = -val;
	}

	int whole = (int)val;
	char tmp[32] = {0};
	lltoa(whole, tmp);

	for (int j = 0; tmp[j]; j++)
	{
		buffer[i++] = tmp[j];
	}

	buffer[i++] = '.';

	float frac = val - whole;
	for (int p = 0; p < precision; p++)
	{
		frac *= 10;
		int digit = (int)frac;
		buffer[i++] = digit + '0';
		frac -= digit;
	}

	buffer[i] = '\0';

	strcpy(buf, buffer);
	return buf;
}

const char hexsym[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

char *ulltohexa(uint64_t value, char *buf)
{
	char buffer[21];
	char *ptr = &buffer[20];
	*ptr = '\0';

	if (value == 0)
	{
		*(--ptr) = '0';
		strcpy(buf, ptr);
		return buf;
	}

	while (value > 0)
	{
		*(--ptr) = hexsym[value % 16];
		value /= 16;
	}

	strcpy(buf, ptr);
	return buf;
}

char toupper(char ch)
{
	if (ch >= 'a' && ch <= 'z')
	{
		return ch - 'a' + 'A';
	}
	return ch;
}

char *strcpy(char *dst, const char *src)
{
	memcpy(dst, (void *)src, strlen(src, 10000) + 1);
	return dst;
}

int strcmp(const char *str0, const char *str1)
{
	while (*str0 != '\0' && (*str0 == *str1))
	{
		str0++;
		str1++;
	}
	return (unsigned char)(*str0) - (unsigned char)(*str1);
}

int wstrcmp(const wide_char_t *str0, const wide_char_t *str1){
	while (*str0 != '\0' && (*str0 == *str1))
	{
		str0++;
		str1++;
	}
	return (wide_char_t)(*str0) - (wide_char_t)(*str1);
}

char *append_strs(char **strs, size_t count)
{
	size_t totalSize = 0;
	char *buffer = NULL;
	for (int i = 0; i < count; i++)
	{
		size_t ss = strlen(strs[i], 1000);

		buffer = kreloc(buffer, ss, ss + totalSize);

		memcpy(buffer + totalSize, strs[i], ss);

		totalSize += ss;
	}
	// for last \0
	buffer = kreloc(buffer, totalSize, 1 + totalSize);
	buffer[totalSize] = '\0';

	return buffer;
}

void parse_read(char* output, const char* input, char seperator, size_t index)
{
	size_t len = strlen(input, 4096);
	size_t current = 0;
	size_t begin = 0;
	for (size_t i = 0; i < len; i++)
	{
		if(input[i] == seperator){
			if(current == index){
				memcpy(output, &input[begin], (i - begin));
				output[(i - begin)] = '\0';
				return;
			}
			current++;
			begin = i + 1;
		}
	}
	if(current == index){
		memcpy(output, (input + begin), (len - begin));
		output[(len - begin)] = '\0';
		return;
	}

	// not found
	output[0] = '\0';
	return;
}

void w_parse_read(wide_char_t* output, const wide_char_t* input, wide_char_t seperator, size_t index){
	size_t len = wstrlen(input, 4096);
	size_t current = 0;
	size_t begin = 0;
	for (size_t i = 0; i < len; i++)
	{
		if(input[i] == seperator){
			if(current == index){
				memcpy(output, &input[begin], (i - begin) * sizeof(wide_char_t));
				output[(i - begin)] = '\0';
				return;
			}
			current++;
			begin = i + 1;
		}
	}
	if(current == index){
		memcpy(output, &input[begin], (len - begin) * sizeof(wide_char_t));
		output[(len - begin)] = '\0';
		return;
	}

	// not found
	output[0] = '\0';
	return;
}

void w_parse_read_multiple(wide_char_t* output,  const wide_char_t* input, wide_char_t separator, size_t start_index, size_t count){
    size_t len = wstrlen(input, 4096);

    if (len == 0 || count == 0)
    {
        output[0] = '\0';
        return;
    }

    size_t separator_count = 0;
    size_t begin = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (input[i] == separator)
        {
            separator_count++;

            if (separator_count == start_index)
            {
                begin = i;
                break;
            }
        }
    }

    if (start_index != 0 && separator_count < start_index)
    {
        output[0] = '\0';
        return;
    }

    size_t end = len;

    if (count != UINT64_MAX)
    {
        size_t found = 0;

        for (size_t i = begin; i < len; i++)
        {
            if (input[i] == separator)
            {
                found++;

                if (i == begin)
                    continue;

                if (found == count)
                {
                    end = i;
                    break;
                }
            }
        }
    }

    size_t output_len = end - begin;

    memcpy(
        output,
        input + begin,
        output_len * sizeof(wide_char_t)
    );

    output[output_len] = '\0';
}

size_t parse_read_count(const char* input, char seperator){
	size_t len = strlen(input, 4096);

	if (len == 0) return 0;

	size_t count = 0;
	for (size_t i = 0; i < len; i++)
	{
		if(input[i] == seperator){
			count++;
		}
	}
	if(input[len - 1] != seperator){
			count++;
	}
	return count;
}

size_t w_parse_read_count(const wide_char_t* input, wide_char_t seperator){
	size_t len = wstrlen(input, 4096);

	if (len == 0) return 0;

	size_t count = 0;
	for (size_t i = 0; i < len; i++)
	{
		if(input[i] == seperator){
			count++;
		}
	}
	if(input[len - 1] != seperator){
			count++;
	}
	return count;
}

void char_to_utf16_buf(const char* _char, wide_char_t* _wchar){
    size_t len = strlen(_char, 512) + 1; // include null terminator
    for(size_t i = 0; i < len; i++){
        _wchar[i] = (wide_char_t)(unsigned char)_char[i];
    }
}

void utf16_to_char_buf(const wide_char_t* _wchar, char* _char){
    size_t len = wstrlen(_wchar, 512) + 1; // include null terminator
    for(size_t i = 0; i < len; i++){
        _char[i] = (unsigned char)_wchar[i];
    }
}

_separated_path separate_path(const wide_char_t* path){
	_separated_path result;
	result.file_name = nullptr;
	result.path = nullptr;

	wide_char_t* buf = kmalloc((wstrlen(path, 1024) + 1) * sizeof(wide_char_t));

	size_t entry_count =  w_parse_read_count(path, '/');

	if(entry_count == 0){
		return result;
	}

	w_parse_read_multiple(buf, path, '/', 0, entry_count - 1);
	result.path = kmalloc((wstrlen(buf, 1024) + 1) * sizeof(wide_char_t));
	memcpy(result.path, buf, (wstrlen(buf, 1024) + 1) * sizeof(wide_char_t));

	w_parse_read(buf, path, '/', entry_count - 1);
	result.file_name = kmalloc((wstrlen(buf, 1024) + 1) * sizeof(wide_char_t));
	memcpy(result.file_name, buf, (wstrlen(buf, 1024) + 1) * sizeof(wide_char_t));
	
	kfree(buf);
	return result;
}