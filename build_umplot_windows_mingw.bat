gcc -O3 umplot.c -o umplot.umi -shared -Wl,--dll -static-libgcc -static -lraylib -lumka -L%cd% -lkernel32 -luser32 -lgdi32 -lwinmm 
