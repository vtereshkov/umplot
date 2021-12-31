gcc -O3 -fPIC umplot.c -o umplot.umi -shared -static-libgcc -L$PWD -lm -lraylib -lpthread

