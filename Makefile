ifeq ($(GDK),)
    $(error GDK environment variable is not set. Install SGDK and export GDK=/path/to/sgdk)
endif

# src/rom_header.c es la cabecera del cartucho y va en el repo. SGDK sólo
# copia su plantilla ($(GDK)/src/boot/rom_header.c) si el archivo no existe;
# si existe, no lo toca. Sus campos son arrays de ancho fijo sin NUL (así es
# el formato de la cabecera), cosa que este GCC avisa diez veces, así que se
# silencia ese aviso concreto para que la lista de problemas del editor sólo
# muestre lo nuestro.
EXTRA_FLAGS := -Wno-unterminated-string-initialization

# Depurador de gráficos en el juego (ver inc/gfxdebug.h): make GFXDEBUG=1.
# make no recompila al cambiar de opción: antes, touch src/*.c inc/*.h src/*.s
ifeq ($(GFXDEBUG),1)
EXTRA_FLAGS += -DPORT_DEBUG_GFX=1
endif

include $(GDK)/makefile.gen
