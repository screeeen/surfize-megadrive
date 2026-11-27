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
# Al cambiar de opción recompila solo (ver FLAGS_STAMP al final).
ifeq ($(GFXDEBUG),1)
EXTRA_FLAGS += -DPORT_DEBUG_GFX=1
endif

include $(GDK)/makefile.gen

# make no sabe que hay que recompilar cuando cambian las opciones (-D en
# EXTRA_FLAGS, GFXDEBUG=1...; la versión en C va con
# EXTRA_FLAGS="-DPORT_NO_ASM=1", no con make PORT_NO_ASM=1). Aquí se guarda la línea de
# opciones en out/flags.stamp, que sólo se reescribe si cambia, y todos los
# objetos dependen de ella: cambiar de variante recompila lo que haga falta
# sin el touch de antes.
FLAGS_STAMP := out/flags.stamp
$(shell mkdir -p out; echo '$(EXTRA_FLAGS)' | cmp -s - $(FLAGS_STAMP) || echo '$(EXTRA_FLAGS)' > $(FLAGS_STAMP))
$(OBJS): $(FLAGS_STAMP)
