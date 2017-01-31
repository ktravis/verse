#include <stdio.h>
#include <stdlib.h>

void create_prelude_h(int length) {
    FILE *f = fopen("src/prelude.h", "w");
    if (f == NULL) {
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }
    fprintf(f, "const char prelude[] = {\n");
    fprintf(f, "#include \"prelude.bin\"\n");
    fprintf(f, "};\n");
    fprintf(f, "const int prelude_length = %d;\n", length);
    fclose(f);
}

void run(const char *filepath) {
    int c;
    int n = 0;
    FILE *fin = fopen(filepath, "r");
    FILE *fout = fopen("src/prelude.bin", "w");
    if (fin == NULL) {
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }
    while((c = fgetc(fin)) != EOF) {
        fprintf(fout, "'\\x%X',", (unsigned)c);
        n++;
    }
    /*fprintf(fout, "'\\0'");*/
    fclose(fin);
    fclose(fout);
    create_prelude_h(n);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        run(argv[1]);
    } else {
        fprintf(stderr, "Not enough arguments\n");
    }
}
