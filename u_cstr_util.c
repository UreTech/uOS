#include "u_cstr_util.h"
#include "u_ctypes.h"

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
	size_t res = 0;
	while (*(str + res) != '\0')
	{
		if (res > maxSize)
			return 0;
		res++;
	}
	return res;
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