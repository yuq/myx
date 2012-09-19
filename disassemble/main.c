#include <evergreen-asm.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void usage(char *name)
{
	printf("Usage: %s code.bin [ALU|TEX|CF addr cnt]\n", name);
}

int main(int argc, char **argv)
{
	FILE *f;
	int size;
	char *buff;

	if (argc != 2 && argc != 5) {
		usage(argv[0]);
		return 0;
	}

	assert((f = fopen(argv[1], "r")) != NULL);
	// get file size
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	assert((buff = malloc(size)) != NULL);
	assert(fread(buff, 1, size, f) == size);
	fclose(f);

	evg_disasm_init();

	if (argc == 2) {
		struct elf_buffer_t elf_buffer;
		elf_buffer.ptr = buff;
		elf_buffer.size = size;
		elf_buffer.pos = 0;
		evg_disasm_buffer(&elf_buffer, stdout);
	}
	else {
		int addr = atoi(argv[3]);
		int cnt = atoi(argv[4]);
		int i;
		buff += addr << 3;
		if (strncmp(argv[2], "ALU", 3) == 0) {
			struct evg_alu_group_t alu_group;
			for (i = 0; i < cnt; i++) {
				buff = evg_inst_decode_alu_group(buff, i, &alu_group);
				evg_alu_group_dump(&alu_group, 0, stdout);
			}
		}
		else if (strncmp(argv[2], "TEX", 3) == 0) {
			struct evg_inst_t inst;
			for (i = 0; i < cnt; i++) {
				buff = evg_inst_decode_tc(buff, &inst);
				evg_inst_dump(&inst, i, 0, stdout);
			}
		}
		else if (strncmp(argv[2], "CF", 2) == 0) {
			struct evg_inst_t cf_inst;
			for (i = 0; i < cnt; i++) {
				buff = evg_inst_decode_cf(buff, &cf_inst);
				evg_inst_dump(&cf_inst, i, 0, stdout);
			}
		}
		else {
			usage(argv[0]);
		}
	}

	evg_disasm_done();

	return 0;
}

