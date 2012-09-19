#include <stdio.h>
#include <assert.h>
#include "evergreen_shader.h"


#define shader_gen(name)										\
	size = evergreen_##name##_vs(buff);							\
	assert((f = fopen(#name ".vs.bin", "w")) != NULL);			\
	assert(fwrite(buff, sizeof(uint32_t), size, f) == size);	\
	fclose(f);													\
	size = evergreen_##name##_ps(buff);							\
	assert((f = fopen(#name ".ps.bin", "w")) != NULL);			\
	assert(fwrite(buff, sizeof(uint32_t), size, f) == size);	\
	fclose(f);

int main(void)
{
	int size;
	FILE *f;
	uint32_t buff[1024];

	shader_gen(solid);
	shader_gen(copy);
	shader_gen(xv);
	shader_gen(comp);
	return 0;
}

