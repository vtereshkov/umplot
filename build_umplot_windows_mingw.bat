gcc -O3 -DUMKA_STATIC umplot.c -o umplot_windows.umi -shared -Wl,--dll -static-libgcc -static -lumka_static -lraylib -L. -lkernel32 -luser32 -lgdi32 -lwinmm 
