gcc -O3 -fPIC umplot.c -o umplot_linux.umi -shared -static-libgcc -L$PWD -lm -lraylib -lumka_static -lpthread

