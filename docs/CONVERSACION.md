# Surfize para Mega Drive: la conversación entera

Transcripción de lo hablado durante el port, sin las llamadas a
herramientas ni sus resultados: sólo lo que se pidió y lo que se
contestó. El detalle técnico de cada decisión está en
`PORT_AUDIT_GML.md`, que es el informe vivo del port.

Generado desde el transcript de la sesión.

---


## 1. Tú · 2026-11-06

> corre el proyecto

I'll check the project structure first.

A Mega Drive (SGDK) project. Let me look at how it builds and runs.

Toolchain is in place. Building first:

El proyecto está corriendo. 🎮

- **Build**: `make` compiló sin errores → `out/rom.bin` (128 KB, recién generado).
- **Emulador**: BlastEm está abierto con el ROM (PID 48338) — deberías ver la ventana del juego en pantalla.

Controles: D-Pad para mover la nave (el disparo es automático al enemigo más cercano). El contador de FPS está arriba a la derecha.

Un detalle del build, nada que te bloquee: el link descarta `-flto` a propósito porque la `libmd.a` precompilada de SGDK trae bytecode LTO de un GCC más viejo que el del toolchain; el comentario en el Makefile de SGDK ya lo explica.

Para cerrarlo: `kill 48338`, o simplemente cierra la ventana.


## 2. Tú · 2026-11-06

> <pasted_content id="45e2">
> # ROL: INGENIERO ESPECIALISTA EN PORTING A SEGA MEGA DRIVE
> 
> Actúa como un ingeniero senior especializado en desarrollo de videojuegos retro, arquitectura de hardware de 16 bits y programación de Sega Mega Drive / Genesis.
> 
> Tu misión es portar un videojuego existente, desarrollado originalmente en GameMaker Studio utilizando GML (GameMaker Language), a Sega Mega Drive.
> 
> El port debe conservar la esencia, mecánicas, jugabilidad, comportamiento y diseño del juego original, adaptándolo técnicamente a las capacidades del hardware original de Mega Drive.
> 
> ## 1. HARDWARE OBJETIVO
> 
> El juego debe ejecutarse en hardware real de Sega Mega Drive / Genesis.
> 
> Ten en cuenta sus características:
> 
> - CPU principal Motorola 68000 a 7,67 MHz.
> - Procesador secundario Zilog Z80.
> - VDP (Video Display Processor).
> - Resolución habitual de 320x224 píxeles.
> - Paleta de colores de 9 bits, con 512 colores posibles.
> - Hasta 64 colores simultáneos en pantalla, organizados en paletas de 16 entradas.
> - Sprites por hardware, con sus limitaciones de tamaño, cantidad y visualización por línea.
> - Scroll por planos de fondo.
> - VRAM limitada a 64 KB.
> - RAM principal de 64 KB.
> - Audio mediante YM2612 y SN76489.
> 
> No presupongas que las características de GameMaker existen en Mega Drive.
> 
> ## 2. ENTORNO DE DESARROLLO
> 
> Utiliza preferentemente:
> 
> - SGDK (Sega Genesis Development Kit).
> - Lenguaje C compatible con SGDK.
> - Ensamblador Motorola 68000 cuando sea necesario para optimizar rutinas críticas.
> - Herramientas de conversión de gráficos y audio compatibles con Mega Drive.
> 
> Organiza el proyecto para que pueda compilarse y generar una ROM ejecutable.
> 
> ## 3. ANÁLISIS DEL JUEGO ORIGINAL
> 
> Antes de comenzar a programar:
> 
> 1. Analiza todos los archivos GML proporcionados.
> 2. Identifica objetos, scripts, eventos, variables globales y locales.
> 3. Documenta las mecánicas principales.
> 4. Identifica dependencias entre objetos.
> 5. Analiza el sistema de colisiones.
> 6. Identifica cámaras, scroll, animaciones y efectos visuales.
> 7. Analiza el sistema de sonido y música.
> 8. Detecta características imposibles de reproducir directamente en Mega Drive.
> 
> Crea una tabla de equivalencias entre las funcionalidades de GameMaker y sus implementaciones en SGDK.
> 
> No elimines funcionalidades sin justificarlo.
> 
> ## 4. ADAPTACIÓN TÉCNICA
> 
> Adapta cada sistema al hardware de Mega Drive:
> 
> - Sprites y animaciones.
> - Fondos y escenarios.
> - Colisiones.
> - Física y movimiento.
> - Inteligencia artificial.
> - Gestión de enemigos.
> - Cámara y scroll.
> - Interfaz gráfica.
> - Menús.
> - Sistema de puntuación.
> - Sonido y música.
> - Gestión de memoria.
> 
> Implementa alternativas eficientes cuando una característica de GML no tenga equivalente directo en el hardware.
> 
> Prioriza el uso de enteros y aritmética de punto fijo cuando sea apropiado. Evita cálculos innecesarios de coma flotante.
> 
> Respeta las limitaciones del VDP, DMA, VRAM, sprites y ciclos de CPU.
> 
> ## 5. CONSERVACIÓN DE LA JUGABILIDAD
> 
> El objetivo es que el port se sienta como el juego original.
> 
> Conserva:
> 
> - Velocidad de movimiento.
> - Aceleraciones y deceleraciones.
> - Física de saltos.
> - Distancias de colisión.
> - Comportamiento de enemigos.
> - Temporización de animaciones.
> - Dificultad.
> - Progresión.
> - Diseño de niveles.
> 
> Cuando sea necesario modificar algún elemento por limitaciones técnicas, documenta el cambio y explica sus consecuencias.
> 
> ## 6. GESTIÓN DE RECURSOS
> 
> Diseña un pipeline para convertir los recursos originales:
> 
> - PNG a gráficos compatibles con Mega Drive.
> - Spritesheets a tiles y sprites.
> - Fondos a planos de tiles.
> - Fuentes a gráficos compatibles.
> - Música y efectos de sonido a formatos compatibles con los chips de audio.
> 
> Adapta los colores a la paleta de Mega Drive.
> 
> No reduzcas automáticamente la calidad gráfica sin analizar primero las posibilidades de optimización.
> 
> ## 7. ARQUITECTURA DEL PORT
> 
> Organiza el proyecto de forma modular.
> 
> Ejemplo:
> 
> src/
>   main.c
>   player.c
>   player.h
>   enemies.c
>   enemies.h
>   collision.c
>   collision.h
>   physics.c
>   physics.h
>   level.c
>   level.h
>   camera.c
>   camera.h
>   audio.c
>   audio.h
>   input.c
>   input.h
> 
> res/
>   sprites/
>   backgrounds/
>   fonts/
>   audio/
> 
> Incluye los archivos necesarios para compilar mediante SGDK.
> 
> ## 8. METODOLOGÍA DE TRABAJO
> 
> Trabaja por fases:
> 
> FASE 1: Auditoría del código GML.
> FASE 2: Análisis de recursos gráficos y sonoros.
> FASE 3: Diseño de arquitectura para Mega Drive.
> FASE 4: Port de las mecánicas principales.
> FASE 5: Port de niveles y enemigos.
> FASE 6: Port de gráficos y animaciones.
> FASE 7: Port de sonido y música.
> FASE 8: Optimización de rendimiento.
> FASE 9: Pruebas en emulador.
> FASE 10: Validación en hardware real.
> 
> No intentes desarrollar todo el juego de golpe.
> 
> Completa y valida cada fase antes de avanzar.
> 
> ## 9. REGLAS IMPORTANTES
> 
> - No inventes código GML que no haya sido proporcionado.
> - No supongas cómo funcionan las mecánicas: analiza el código original.
> - No elimines características sin autorización.
> - No utilices funciones inexistentes en SGDK.
> - Comprueba la compatibilidad del código con la versión de SGDK utilizada.
> - Evita asignaciones dinámicas innecesarias.
> - Optimiza el uso de RAM y VRAM.
> - Documenta las decisiones técnicas.
> - Proporciona código completo y compilable para cada módulo.
> - No utilices pseudocódigo cuando se solicite una implementación real.
> - Si falta información, pregunta antes de tomar decisiones importantes.
> 
> ## 10. RESULTADO FINAL
> 
> El resultado debe ser un proyecto completo de SGDK que permita generar una ROM de Sega Mega Drive.
> 
> Debe incluir:
> 
> - Código fuente.
> - Recursos convertidos.
> - Sistema de compilación.
> - Documentación técnica.
> - Instrucciones para ejecutar el juego en emuladores.
> - Informe de limitaciones y optimizaciones.
> - Lista de pruebas realizadas.
> 
> El objetivo final es obtener un port funcional, estable y fiel al videojuego original, ejecutable en una Sega Mega Drive real.
> 
> ## PRIMER PASO
> 
> No empieces a programar todavía.
> 
> Solicita primero el proyecto original de GameMaker, incluyendo archivos GML, sprites, fondos, sonidos, música y configuración del proyecto.
> 
> Después realiza una auditoría técnica completa y presenta un plan de porting antes de escribir código.
> </pasted_content id="45e2">
> 
> quiero que adaptes este prompt para correrlo, quiero la versión más fidedigna al original

El prompt pide "solicita primero el proyecto original de GameMaker". Antes de adaptarlo, veo si ya está en disco — eso cambia el primer paso.

Encontrado. `surfize_changes` parece ser la versión GameMaker de arena (oCu2, oGusano, o3way...). Audito su estructura:

Es el proyecto real: `oSpider`, `oCabeza1/2`, `oWBlock`, `oLaser`, `o3way`, `oCircular`, `oBomba`, `oRayosPowUp`, `oOca`, `oInGameHUD`, `startGame`, `generaMalos`... Mido el volumen antes de leer:

5155 líneas de GML en total — auditable por completo. Leo el núcleo de jugabilidad:

Datos reales ya: `acc=6`, spider `velo=3`, spawn `alarm[0]=90`, cadencias 6/12. Ahora la máquina de estados, spawns y snake:

Hallazgo importante: los enemigos **sí** tienen estado WANDER (`stWalkingA1W`, 600 pasos) y ATTACK (`stAppearA1W`, 300 pasos) con giro gradual, y spawnean desde `oAgujerSuper` (los agujeros). Sigo con constantes, room y snake:

60 fps y room de **1600×1200** — eso implica que todas las velocidades hay que escalarlas a 320×224 (factor 0.2), dato clave para la fidelidad. Leo snake, agujeros, observer y balas:

Audit casi completa. Ahora comparo con lo que ya hay en el port para medir divergencias:

Verifico tamaños de sprites del original para calcular bien el factor de escala (clave para colisiones fieles):

Cinco agujeros con posiciones fijas — extraigo sus coordenadas exactas:

Auditoría completa: 572 PNG y 22 OGG disponibles, 5155 líneas de GML leídas. Antes de escribir el prompt adaptado hay tres decisiones que cambian materialmente qué significa "fiel", y el propio prompt exige preguntarlas:

Dos detalles que cambian la jugabilidad y aún no he verificado: si el cuerpo del gusano es disparable (`oMalo` es el padre de todo) y el segundo tipo de gusano.

Auditoría cerrada y verificada. Escribo el prompt adaptado (como slash command ejecutable) y el informe de FASE 1–2:

Prompt adaptado y ejecutado su primer paso (el propio prompt prohíbe programar antes de la auditoría y el plan).

## Lo que hay ahora

**`/port-arena`** (`.claude/commands/port-arena.md`) — el prompt, convertido en slash command ejecutable por fases. Cambios frente al original que pasaste: rol y hardware se mantienen, pero el "PRIMER PASO: solicita el proyecto" desaparece (el proyecto está en disco, ya localizado), las 10 fases quedan atadas al estado real de este repo, y las decisiones que el prompt mandaba preguntar quedan fijadas: PAL 320×240 @50 Hz, posiciones ×0.2, velocidades ×0.24, giro ×1.2, frames ÷1.2, arte desde los PNG de GameMaker, longitud de gusano fiel. Añade la lista de funciones de GameMaker que **no** existen en MD y la prohibición de inventar constantes de balance.

**`docs/PORT_AUDIT_GML.md`** — FASE 1 y 2 completas: 5155 líneas de GML, 60 objetos, 42 sprites (572 PNG), 22 OGG.

## Hallazgos que cambian el port

El port actual venía de Unity y varias cosas documentadas como "no portadas" o "inventadas" **sí están** en el GML, con valores exactos:

- **No hay spawn por intervalo.** Las oleadas salen al morir una cabeza de gusano (`checkMorte` → `generaMalos`), con `malosCounter` subiendo 0.5 por oleada hasta 4, y 3 arañas de golpe. El port usa `SPAWN_INTERVAL 60`/`180`, que no existe en el original.
- **Los agujeros existen**: 5 `oAgujerSuper` en posiciones fijas del room. Los enemigos nacen ahí, invisibles e inmóviles 90 frames (→75 en PAL).
- **IA de 3 estados con giro gradual**, no persecución recta: espera 6 f → persigue 300 f girando `velo` grados/frame → vaga 600 f hacia un agujero girando 2 grados/frame.
- **Gusanos: 13 y 11 partes** (no 3), `hp` 4 y 2 → 5 y 3 impactos, `stepdelay` 7 y 6.
- **7 armas** con cadencias propias, munición por paquete (+10/+4/+10/+5/+8/+1/+1) y desbloqueo por kills (láser 1, 3way 2, circular 3, tri 4), más 4 power-ups.
- Constantes reales: jugador `acc 6`→**1.44**, araña `velo 3`→**0.72**, bala `32`→**7.68**, cadencia por defecto `12`→**10**. El port tiene 3, 2, 6 y 14.

Coincidencias ya correctas: araña muere de un impacto, segmentos del gusano no disparables pero letales al tocarlos, wrap de pantalla, disparo automático al más cercano, bala por defecto infinita.

**Riesgo que anoté para FASE 10:** con los topes del GML (4+2 gusanos, 8 arañas) salen ~91 sprites por frame, por encima del límite de 80 del VDP. Según tu decisión, se recortarán las cantidades simultáneas —no la longitud— y sólo con la medición delante.

El plan está en el informe; la ROM sigue compilando y corriendo en BlastEm con el estado previo. Para seguir: `/port-arena fase 3` (arquitectura y arranque PAL) o directamente `fase 4` si prefieres ver ya el núcleo con los valores reales.


## 3. Tú · 2026-11-06

> port-arena fase 3


## 4. Tú · 2026-11-06

> # ROL
> 
> Eres ingeniero senior de videojuegos retro: arquitectura de hardware de 16 bits, Motorola 68000 y Sega Mega Drive.
> 
> Portas **arena** de GameMaker Studio 2 (GML) a Mega Drive conservando mecánicas, jugabilidad, comportamiento y diseño del original, adaptados al hardware real.
> 
> Fase a ejecutar en esta corrida: **fase 3** (si está vacío: continúa por la primera fase no completada según `docs/PORT_AUDIT_GML.md`).
> 
> # FUENTE DE VERDAD
> 
> El original es la versión **GameMaker**, no la de Unity:
> 
> - Proyecto: `/Users/miguelangelalvarez/Desktop/___pendireve/game maker move/surfize_changes/`
>   - `surfize_changes.yyp`, `objects/` (60 objetos), `scripts/` (~180, la mayoría librerías GM8 heredadas), `rooms/_1`, `sprites/` (42 sprites, 572 PNG), `sounds/` (22 OGG), `options/main/options_main.yy` (60 fps).
> - `../arena` es el port **Unity** del mismo juego: úsalo sólo como referencia secundaria. Donde Unity y GML discrepen, **manda el GML**.
> - Destino: este repo (`/Users/miguelangelalvarez/arena-megadrive`), SGDK en `$GDK`, build `make` → `out/rom.bin`, prueba `blastem out/rom.bin`.
> 
> Lee el GML antes de afirmar cómo funciona algo. No inventes GML que no exista. Si un valor no está en el GML, dilo explícitamente en lugar de inventarlo.
> 
> # DECISIONES YA TOMADAS (no volver a abrir)
> 
> 1. **Vídeo: PAL 320×240 @50 Hz.** El room original es 1600×1200 a 60 fps, de una sola pantalla (la cámara abarca el room entero, sin scroll) con wrap toroidal en los bordes. PAL mantiene el 4:3 exacto sin recortar campo.
> 2. **Conversión de unidades** (derivada de 1):
>    - Posiciones y tamaños: `× 0.2`
>    - Velocidades en px/frame: `× 0.24`  (0.2 de escala × 60/50 de refresco)
>    - Velocidades de giro en grados/frame: `× 1.2` (los grados no escalan con la resolución, sólo con el refresco)
>    - Contadores de frames y alarmas: `÷ 1.2`
>    - Aritmética en `fix16` de SGDK (10.6, precisión 1/64). Documenta el error de redondeo cuando pase del 1%.
> 3. **Arte: reconvertido desde los PNG de GameMaker**, no desde el arte de Unity. Los sprites actuales del repo (siluetas trazadas de `nave.png`/`culobicho.png`) se sustituyen.
> 4. **Gusano con longitud fiel** (12 y 10 partes según tipo, `stepdelay` del GML). Si al medir en emulador se pierde framerate o sprites por línea, se recortan `MAX_CABEZA_1`/`MAX_CABEZA_2` —nunca la longitud— y se documenta la medición.
> 
> # HARDWARE OBJETIVO
> 
> 68000 a 7,67 MHz; Z80; VDP; 320×240 PAL; paleta de 9 bits (512 colores), 64 simultáneos en 4 líneas de 16; sprites por hardware (máx. 80 por frame, 20 por línea, 16×16 por celda hasta 4×4 celdas); 2 planos de scroll + window; 64 KB de VRAM; 64 KB de RAM; YM2612 + SN76489.
> 
> No presupongas que existe nada de GameMaker. En particular, no existen: `instance_create` dinámico sin coste, listas/colas (`ds_list`, `ds_queue`), `point_direction`/`lengthdir_*` en coma flotante, rotación de sprites en tiempo real (`image_angle`), `draw_line`, surfaces, ni `audio_play_sound` polifónico sin presupuesto.
> 
> # ENTORNO
> 
> SGDK en C (comprueba la API contra `$GDK/inc` antes de usar una función; no inventes funciones de SGDK). Ensamblador 68000 sólo en rutinas críticas y sólo con una medición previa que lo justifique. `rescomp` para recursos vía `res/resources.res`.
> 
> # FIDELIDAD: QUÉ SE CONSERVA
> 
> Velocidades, cadencias de disparo, radios de colisión, HP, temporizaciones de IA y de animación, umbrales de desbloqueo de armas, progresión de oleadas, posiciones de los 5 agujeros de spawn y el wrap de pantalla — todos con los valores medidos del GML, convertidos según la regla de arriba.
> 
> Cuando una limitación del hardware obligue a cambiar algo: impleméntalo, **documéntalo en `docs/PORT_AUDIT_GML.md`** (qué, por qué, consecuencia en juego) y no lo entierres en un comentario del código.
> 
> Prohibido inventar constantes de balance. Si no sale del GML, va marcada como invención en la tabla de divergencias.
> 
> # METODOLOGÍA
> 
> Fases, cada una validada antes de pasar a la siguiente. El estado vive en `docs/PORT_AUDIT_GML.md`:
> 
> 1. Auditoría del GML — objetos, scripts, eventos, globales, mecánicas, colisiones, dependencias, tabla de equivalencias GML→SGDK.
> 2. Auditoría de recursos — sprites, tamaños, frames, paletas, sonidos.
> 3. Arquitectura Mega Drive — módulos, presupuesto de VRAM/sprites/RAM, pipeline de recursos.
> 4. Mecánicas núcleo — jugador, movimiento, wrap, disparo automático, bala por defecto, colisiones.
> 5. Enemigos y oleadas — máquina de estados (steady/attack/wander), agujeros, arañas, los dos tipos de gusano con sus segmentos, progresión `malosCounter`.
> 6. Gráficos y animaciones — conversión de los PNG de GameMaker, paletas, rotación por frames pre-rotados.
> 7. Armas y power-ups — WBLOCK, LASER, 3WAY, PIVOT/TRI, PARA, BOMBA, RAY; paquetes de munición, desbloqueo por kills.
> 8. HUD, menú y estados de partida — kills, LAST, TOP, NEXT; menú y muerte.
> 9. Sonido y música — 22 OGG a YM2612/PSG (XGM), presupuesto de canales.
> 10. Optimización y pruebas — medición de framerate y sprites por línea en emulador, y validación en hardware real.
> 
> En cada fase: implementa código completo y compilable (sin pseudocódigo), compila con `make`, arranca `blastem out/rom.bin`, y anota en el informe qué se probó y qué valor se verificó.
> 
> # REGLAS
> 
> - Evita asignación dinámica: pools estáticos dimensionados por los máximos del GML (`MAX_SPIDERS 8`, `MAX_CABEZA_1 4`, `MAX_CABEZA_2 2`).
> - Enteros y punto fijo; nada de coma flotante en runtime. Tablas precalculadas para seno/coseno y ángulos (`F16_sin`/`F16_cos`/`F16_getAngle` de SGDK, tablas de 1°).
> - Respeta VDP, DMA, VRAM, límites de sprites y ciclos de CPU; mide antes de optimizar.
> - No elimines funcionalidad sin autorización: propónla y espera respuesta.
> - Si falta información para una decisión importante, pregunta antes de implementarla.
> - Documenta cada decisión técnica en el informe.

FASE 3: arquitectura. Primero leo el código actual y verifico la API de SGDK que necesito (PAL 240, sprites, fix16).

Nota crítica: `VDP_setScreenHeight240()` sólo funciona en consola PAL. Verifico presupuesto real de VRAM y sprites antes de diseñar:

SGDK 2.12, `TILE_MAX_NUM`=1536 tiles, pool de sprites 420 tiles por defecto. Necesito dos cosas más: la convención de ángulos de `F16_getAngle` (para que el giro sea fiel al de GML) y el resto del código actual.

Verifico `F16_normalizeAngle` con entradas negativas antes de basar el giro en él:

Diseño verificado contra la API real. Escribo la capa de conversión y el modelo de entidades:

Corrijo un detalle del modelo: `velo` del GML debe ser un campo propio, no colarse en `turnRate`.

Ahora colisiones, agujeros de spawn, estado de partida y HUD:

Aplico los cambios de arquitectura a los módulos existentes (pools a los máximos del GML, campo de 240, sin tocar su lógica previa):

Ahora el arranque: detección PAL, 240 líneas y bucle delegado a `game.c`.

Compila limpio. Mido el presupuesto real de RAM/ROM y arranco en modo PAL:

Verificado el build y el arranque en PAL. Documento FASE 3 con los presupuestos medidos:

FASE 3 cerrada. Compila limpio, arranca en PAL y la ROM sigue jugable.

## Qué se ha montado

**Capa de conversión** (`inc/arena.h`, única fuente de constantes). Cada valor del GML queda visible y la conversión la hace el compilador:

```c
#define SPIDER_SPEED  GML_SPEED(3)    // velo = 3   -> 0.72 px/f
#define SPIDER_TURN   GML_TURN(3)     // 3 grados/f -> 3.6
#define ST_ATTACK_TIME GML_FRAMES(300) // -> 250 f
```

Añadí `FIX16R`: el `FIX16()` de SGDK trunca, y truncar velocidades lentas desviaba de más (1.872 → 1.859 en vez de 1.875). En ejecución no queda ni un flotante.

**Módulos nuevos**: `entity` (modelo de instancia: `speed`/`direction`, `alarm[0]`, `state_machine`, `turn_towards_direction`, `point_direction`, wrap de `stPlayerMovement`), `collision` (AABB en lugar de los eventos `Collision_oX`), `hole` (los 5 `oAgujerSuper` del room escalados), `game` (globales de `scrInit`, `startGame`, `mataPlayer`, bucle) y `hud` (BULLETS / KILLS / LAST con la disposición de `oInGameHUD`). `main.c` queda en arranque: PAL → 240 líneas, NTSC → 224.

Lo heredado del port de Unity está marcado `[PREVIO AL PORT]` y sigue vivo sólo para que la ROM arranque: `SPEED 3` del jugador, los dos temporizadores de spawn inventados y los 3 segmentos / 3 HP del gusano. Los pools ya están dimensionados a los topes del GML (8 arañas, 4+2 gusanos).

## Presupuesto medido

| | Uso | Límite |
|---|---|---|
| RAM | 3168 B | 65536 B |
| ROM | 58,6 KB | 128 KB |
| Tiles de sprite | 272 | 420 del pool |
| Sprites de hardware | 49 | 80 por frame |

El historial del gusano fiel (1,9 KB los seis) entra sin apretar. Donde no hay margen es en los sprites: el peor caso fiel de FASE 5 son ~99 sprites por frame, sobre el límite de 80 — y el de 20 por línea se rompe antes. Queda como medición de FASE 10 con la decisión ya tomada (recortar cantidades, no longitud).

## Desviaciones que anoté en el informe

- **NTSC**: la lógica mantiene siempre el campo de 320×240 (distancias y agujeros fieles), pero `VDP_setScreenHeight240` sólo funciona en PAL, así que en consola NTSC no se ve la franja inferior de 16 px y el juego corre un 20% más rápido. Consecuencia asumida de la decisión PAL.
- **Ángulos**: uso la convención de SGDK (horaria, Y hacia abajo) en vez de la de GameMaker (antihoraria, Y hacia arriba). Son espejo (`a_SGDK = 360 − dir_GML`), así que las trayectorias son idénticas; sólo cambia el número, que nunca se muestra.
- **`F16_sin`/`F16_cos` resuelven con tabla de 1 grado**, así que el rumbo se cuantiza a grados enteros (medio grado de desviación máxima). El giro gradual sí conserva su ritmo fraccionario.
- **`MAX_BULLETS 16`** donde el GML no tiene tope: cota derivada de cadencia × vida de la bala, no inventada.

## Pruebas

`make` limpio sin errores ni avisos propios (los 10 que salen son del `rom_header.c` de plantilla de SGDK, campos de ancho fijo sin NUL: preexistentes e inocuos). `blastem -r E out/rom.bin` arranca y sigue jugable — la ventana debería estar ahora a 320×240; esa comprobación es visual, en tu pantalla, no la puedo ver desde aquí.

Un detalle del enlazado: `entity`, `collision` y `hole` compilan pero el linker se los come con `--gc-sections` porque todavía nadie los llama; entran en la ROM en las fases 4 y 5.

Siguiente: `/port-arena fase 4` — jugador a 1.44 px/f con wrap de 3 px, apuntado al más cercano y bala por defecto a 7.69 con cadencia 10, todo sobre `entity`.


## 5. Tú · 2026-11-06

> sigueinte

Sigo con FASE 4 (mecánicas núcleo). Necesito datos que aún no he leído: orígenes y máscaras de colisión de los sprites, y qué hace la bala al salir de pantalla.

FASE 4. Datos nuevos del GML que cambian cosas: los sprites tienen **origen propio** (la nave en (19,19), no el centro) y máscaras de colisión (`bbox`), y el jugador dispara **siempre**, con o sin objetivo. Implemento sobre `entity`:

Ahora el bucle de partida con la estructura de disparo real del GML (dispara siempre, con o sin objetivo):

Compila y las constantes salen exactas. Arranco para comprobar que no revienta:

Actualizo el informe con FASE 4, incluidos dos hallazgos nuevos del GML:

FASE 4 lista. Jugador y bala ya corren con los números del GML.

## Portado

| | Compilado | Del GML |
|---|---|---|
| Velocidad del jugador | 1.4375 px/f | `acc = 6` |
| Bala por defecto | 7.6875 px/f | `vspeed = -32` |
| Cadencia | 10 f (primera a los 25) | `controlTime` 12 / 30 |
| Margen de wrap | 3 px | `offSet = 15` |
| Caja del jugador | origen +(−4,−4), 7×11 | `bbox` de `sprite173` |
| Caja de la bala | origen +(−2,−3), 4×5 | `bbox` de `sBulletThin` |

## Tres hallazgos que cambiaron el diseño

**La posición de una instancia es el origen de su sprite, y no está centrado.** La nave lo tiene en (19,19) de 38×58; la araña en (30,40) de 64×64. El wrap, el apuntado, el punto de disparo y las colisiones se miden desde ahí, así que el port guarda el origen y lo resta sólo al dibujar. Las cajas salen de las `bbox` de las máscaras, no del tamaño del sprite.

**El jugador dispara siempre, haya enemigos o no.** En `oPlayer/Step_0` el apuntado está dentro de `if instance_exists(oMalo)` pero el bloque de disparo va aparte, incondicional: sin enemigos sigue disparando al último rumbo, y al empezar la partida al rumbo 0 de GameMaker (a la derecha). El port anterior sólo disparaba con objetivo.

**El daño por bala era ambiguo en el original y lo he resuelto por orden de instancia.** Hay dos manejadores para el mismo impacto: el del enemigo (`hp -= 1` + destruye la bala) y el de la bala (`with other hp -= 1` + se destruye). Si corrieran los dos, cada bala restaría 2 y la cabeza moriría en 3 impactos, no 5. GameMaker procesa por orden de instancia y los enemigos existen antes que la bala, así que el del enemigo corre primero y la bala ya no llega a ejecutar el suyo: 1 de daño. Queda anotado como asunción razonada, no verificada ejecutando el original — si quieres certeza absoluta habría que abrir el proyecto en GameMaker y medirlo.

## Lo que vas a ver raro ahora

La caja de colisión del jugador ya es la fiel (7×11) pero el sprite dibujado sigue siendo el de 32×32 trazado del arte de Unity: **la nave se ve mucho mayor que su área de contacto**, y los enemigos la solaparán sin matarla. Desajuste de arte, no de lógica, y se cierra en FASE 6. Las cajas de araña y gusano siguen siendo las antiguas hasta FASE 5, donde ya están calculadas.

También notarás la nave más lenta: 1.44 px/f es lo que corresponde al original en esta resolución, frente al 3 inventado que tenía antes.

`make` limpio y la ROM corriendo en PAL (`blastem -r E`). Las constantes las verifiqué una a una contra la tabla de la auditoría.

Siguiente: FASE 5, la gorda — máquina de 3 estados con giro gradual, los 5 agujeros, arañas de 3 en 3, los dos tipos de gusano con 13 y 11 partes, y las oleadas por `malosCounter`. Ahí entra también el detalle de que `instance_nearest` cuenta los segmentos del gusano, que son invulnerables.


## 6. Tú · 2026-11-06

> sigueinte

FASE 5. Antes de escribir: con 13 partes por gusano salen ~78 sprites de gusano, y cada sprite de SGDK reserva su propia VRAM (488 tiles > los 420 del pool). Verifico la API para compartir tiles:

Un límite duro que cambia la decisión: el pool de objetos Sprite de SGDK es **80** (`MAX_SPRITE` en `src/sprite_eng.c`), así que con 4+2 gusanos de 13 partes (103 objetos) la asignación fallaría. Falta un dato del GML:

Con esos datos el presupuesto cuadra: tope de 3+1 gusanos (50 partes) → 67 objetos Sprite de 80 y 344 tiles de 420. Compruebo los nombres de recursos y escribo:

Actualizo constantes (topes forzados por el límite de 80 objetos) y añado la IA compartida de los tres estados:

Ahora araña y los dos tipos de gusano:

Dos correcciones: el `velo` del gusano es **un solo sorteo** que sirve de velocidad y de giro, y `Enemy_hit` cambió de firma.

Y las oleadas (`generaMalos` / `checkMorte`), que sustituyen a los temporizadores inventados:

El tamaño del historial necesita ser constante entera: `GML_FRAMES` usaba flotante. Lo paso a aritmética entera exacta:

Un detalle de fidelidad que me había colado: en el original la máquina de estados corre **también durante los 90 frames invisibles** (sólo `speed` está a 0), así que el enemigo aparece ya apuntado y con su reloj de ataque avanzado. Lo corrijo:

No queda nada del port anterior y los valores cuadran con la auditoría. Documento FASE 5:

FASE 5 cerrada: la lógica de enemigos ya es la del GML, no la inventada.

## Portado

| | Compilado |
|---|---|
| Araña | 0.7188 px/f, giro 3.59 °/f, hp 0 → muere de 1 impacto |
| Gusano tipo 1 | 1.203–1.875 px/f, giro 6.0–9.36 °/f, hp 4 → 5 impactos, **13 partes**, `stepdelay` 6 |
| Gusano tipo 2 | 1.484 px/f, giro 7.44 °/f, hp 2 → 3 impactos, **11 partes**, `stepdelay` 5 |
| Estados | quieto 5 f → ataque 250 f → vagar 500 f (giro 2.4 °/f) |
| Aparición | 75 f invisible e inmóvil en el agujero |
| Oleadas | `malosCounter` +0.5 con techo 4, al morir cada cabeza, + 3 arañas |

## Hallazgos que cambian cómo se juega

**Los bucles de elección de agujero del original están sesgados, y portarlos fiel cambia el juego.** Los tres calculan el índice de forma descuidada y lo que vale es la última iteración:

- `oSpider`: `instance_find(oAgujerSuper, i)` con `i` acabando en 4 → **todas las arañas nacen siempre en el mismo agujero**, el (219,147).
- `oCabeza1`: `trunc(4 - random(4))` → gusanos en los agujeros 0–3, nunca el 4.
- `stWalkingA1W`: `trunc(4 - random(3))` **re-sorteado cada frame** → el que vaga tiembla de rumbo entre tres agujeros.

Lo he portado tal cual. Si lo ves como bug del original y prefieres arreglarlo, es una línea en `hole.c`, pero sería apartarse de la fuente — dímelo y lo cambio.

**La IA corre durante los 90 frames invisibles.** `Step_1` se ejecuta aunque la instancia no se vea; sólo `speed = 0` la frena. El enemigo emerge ya apuntado al jugador y con 75 de sus 250 frames de ataque gastados.

**`velo` es un solo sorteo que hace de velocidad y de giro**, así que un gusano rápido gira rápido. Sorteo un índice y lo interpolo en las dos escalas; con dos sorteos independientes los gusanos girarían a un ritmo ajeno a su velocidad.

**Al tocar cualquier agujero mientras vaga, el enemigo salta al agujero 0** — `oAgujerSuper` sin cualificar es la primera instancia, no la tocada. Y la máscara del agujero es de 3×2 px (el `bbox` de `sprAgujero` son 15×12 del room, no el disco de 197), así que casi nunca ocurre.

**El apuntado cuenta los eslabones del gusano**, que son invulnerables: la nave puede quedarse disparando a un segmento mientras la cabeza campa libre. Está en el original (`oCorpo` desciende de `oMalo`) y queda portado.

**No hay arañas hasta que muere el primer gusano**: sólo `generaMalos` las crea. La partida arranca con un único gusano de tipo 1.

## Recorte forzado, no opcional

El pool de objetos Sprite de SGDK es de **80** (`MAX_SPRITE` en su fuente), no sólo el límite del VDP. Con los topes del original (4+2 gusanos de 13 y 11 partes) harían falta 103 objetos y `SPR_addSprite` devolvería NULL. Aplicando tu decisión —longitud intacta, cantidad recortada— he puesto **3 gusanos de tipo 1 y 1 de tipo 2**: 69 objetos asignados, 67 visibles como máximo, 368 tiles de VRAM (subí el pool a 600 con `SPR_initEx` porque 420 quedaba al borde). También bajé el pool de balas a 8, que es lo que caben en vuelo con la bala por defecto; FASE 7 obligará a recortar en otro sitio.

Lo que sigue sin medir es el límite de **20 sprites por línea**: si un gusano de 13 partes cruza una franja a lo ancho, puede perder eslabones. Eso se ve en pantalla — si notas parpadeos o partes que desaparecen, dímelo y ajusto.

`make` limpio, RAM 4216 B de 64 KB, ROM corriendo en PAL. No queda nada del port anterior en el código.

Siguiente: FASE 6, el arte — reconvertir los PNG de GameMaker (nave de 4 frames, araña de 32, las cuatro siluetas del gusano, los agujeros al plano de fondo) y con ello desaparece el desajuste entre sprites gigantes y cajas de colisión fieles.


## 7. Tú · 2026-11-06

> siguiente

FASE 6: el arte. Primero compruebo herramientas y miro el material real (orden de frames y colores):

PIL no está. Veo qué herramientas de imagen hay y cómo se generaron los sprites actuales:

ImageMagick disponible. Analizo el arte real: orden de frames y paleta de cada sprite.

Corrección sobre mi propio inventario: la araña tiene **8 frames**, no 32, y la nave **1**, no 4 (mi primer conteo fue por regex, mal). Ahora los colores:

El arte vuelve a ser máscaras con la forma en el canal alfa. Veo los colores reales donde el alfa es opaco:

Dato clave: **todo el arte de enemigos es negro puro con la forma en el alfa** (siluetas sin tintar en el GML), y la nave es blanco y negro. Mirar el fondo es ahora imprescindible:

El arte rota en ejecución (`image_angle`), así que necesito frames pre-rotados. Compruebo qué partes rotan en el original antes de decidir el presupuesto:

Reparto de rotación en el original: rotan nave, araña, las dos cabezas y los dos eslabones de cola; los `oCorpo` del cuerpo **no** (su línea está comentada). Eso abarata mucho el presupuesto. Miro el fondo:

Tengo todo lo que necesita el pipeline. Verifico primero el sentido de giro de ImageMagick (si lo invierto, todos los sprites apuntarían mal):

ImageMagick gira en sentido horario con `-rotate` (verificado: la marca arriba-centro acaba a la derecha). Escribo el generador de recursos:

Compruebo que los frames tienen contenido real y que la rotación los diferencia:

Contenido correcto en todos los frames. Ahora el lado del código: paletas, fondo real y los agujeros en el plano A.

Ahora el código: paletas del arte real, fondo del room, agujeros en el plano A y los nuevos sprites por parte.

Añado las piezas que faltan (índice de frame por rumbo, velocidad de animación) y reescribo el dibujado del gusano por partes:

El tamaño no ha cambiado: sospechoso. Build limpio con la salida a la vista:

Error real que mi filtro de salida me había escondido: borré `spider.png` recién generado (estaba en la lista de limpieza). Lo regenero y construyo comprobando el código de salida de verdad:

Todo coherente (fondo teal 0x0886, paleta de sprites con negro/blanco/amarillo). Documento FASE 6, incluida la corrección de mi propio inventario:

FASE 6 lista. El juego ya se ve con el arte del original, no con sustitutos.

## Pipeline

**`tools/gm2md.py`** es ahora el pipeline de recursos: lee el origen y los frames de cada `.yy`, compone sobre un lienzo centrado en el origen del sprite, rota, escala ×0.2 exacto y escribe PNG indexados de 16 colores con paleta fija + `res/gm_palettes.h`. Sólo necesita ImageMagick (no hay PIL en el sistema).

| Recurso | Celda | Frames |
|---|---|---|
| nave (`sprite173`) | 24×24 | 8 rotaciones |
| araña (`sprite185`) | 24×24 | 8 animaciones × 8 rotaciones |
| cabeza tipo 1 y 2, cola | 24×24 | 8 rotaciones cada una |
| cuerpo (`oCorpo`) | 16×16 | 1 (en el original no rota) |
| bala | 8×8 | 8 rotaciones |
| agujero | bloque 8×8 tiles | plano A |

## Hallazgos

**El juego original es de siluetas negras.** El arte lleva la forma en el alfa y el RGB en negro; el GML no lo tinta. Así que los bichos son negros y la nave negra con detalles blancos, sobre el fondo. Una sola paleta de sprites (transparente, negro, blanco, amarillo) en PAL1.

**El fondo es un color plano y su textura no existe en este hardware.** La capa de color del room es un teal grisáceo (117,154,154) y `background38` encima son dos grises **al 6% de opacidad**: compuestos dan (118,152,152) y (112,147,147), que en la rejilla de 3 bits del VDP caen en el mismo color. El fondo fiel es ese teal plano. Con esto se va el campo de estrellas que el port anterior había inventado.

**Cada objeto rota con una fórmula distinta** (`direction - 90` en nave y araña, `direction` en cabezas, `direction + 90` en la bala), y eso determina hacia dónde mira el arte sin rotar. El generador corrige ese desfase por sprite.

**Las celdas son grandes a propósito**: el original rota alrededor del origen, no del centro. La nave tiene el origen en (19,19) de 38×58, así que la cola barre 8.4 px de radio y hace falta una celda de 24×24 para un dibujo de 8×12.

**Corrección de mi propio inventario de FASE 2**: la araña tiene **8** frames, no 32, y la nave **1**, no 4 — mi primer conteo salió de una regex mal hecha; ahora está leído parseando el `.yy` como JSON. Queda anotado en el informe.

## Un fallo que casi se me cuela

Al limpiar los assets del port anterior borré `spider.png` recién generado, y el build fallaba en rescomp. Mi filtro de salida del `make` me lo escondió y estuve a punto de dar por bueno un ROM viejo de hace 15 minutos; lo cacé porque el tamaño no había cambiado. Ahora compruebo el código de salida y el tamaño del binario explícitamente: **exit 0, 70,5 KB** de código y datos (+8,3 KB de arte).

## Lo que toca mirar en pantalla

Esta fase es la primera que no puedo verificar yo: lo que compruebo es que los frames tienen contenido (36–37 px opacos por rotación de la nave) y que arranca. Mira si:

- La nave y los bichos se ven del tamaño correcto respecto a lo que les mata (ya no hay desajuste entre sprite y caja).
- Las rotaciones apuntan bien (si el arte mira 90° girado, el desfase natural de ese sprite está mal).
- Los cinco agujeros aparecen donde deben.
- Si notas parpadeo o eslabones que desaparecen cuando un gusano cruza la pantalla a lo ancho, es el límite de 20 sprites por línea del VDP, pendiente de medir.

Siguiente: FASE 7, armas y power-ups — las 7 armas de `WeaponsController` con sus cadencias, la munición por paquete y el desbloqueo por kills. Ahí hay que recortar en el presupuesto de sprites, que ya está a 69 de 80.


## 8. Tú · 2026-11-06

> esta rompiendo la build

Build limpio desde cero: exit 0 y ROM regenerada. Así que el fallo está en cómo se construye en tu lado, no en el código. Reviso lo que puede diferir: qué queda fuera de git y la tarea de VS Code.

Reproduzco el build como lo lanza VS Code (shell no interactivo, que no lee `~/.zshrc`):

Encontré el gancho `EXTRA_FLAGS` en SGDK. Silencio los avisos de su propia plantilla y arreglo la tarea de ejecución, que seguía lanzando en NTSC:

Build limpio desde cero: **exit 0, cero avisos, cero errores**, ROM regenerada. No consigo reproducir la rotura.

Lo que sí hubo fue una ventana real de build roto, de unos 10 minutos: al limpiar los assets del port anterior borré `spider.png` recién generado y rescomp fallaba con `Can't open image .../spider.png`. Si construiste en ese rato, eso es lo que viste. Ya está regenerado y verificado.

De paso he arreglado dos cosas que pueden estar detrás de lo que ves:

- **Los 10 avisos de `src/rom_header.c`** ya no salen. Son de la plantilla de SGDK (campos de ancho fijo sin NUL), no de código nuestro, y con el `problemMatcher: $gcc` de tu `tasks.json` aparecían en rojo en el panel de Problemas de VS Code, que es fácil de leer como "build roto". Los silencio con `EXTRA_FLAGS := -Wno-unterminated-string-initialization` en el Makefile, que es el gancho que SGDK deja para eso.
- **La tarea "SGDK: run in BlastEm" lanzaba en NTSC** (`blastem out/rom.bin`). Desde FASE 3 el port está calculado para PAL 320×240 a 50 Hz: en NTSC se recorta la franja inferior y el juego corre un 20% rápido. Ahora la tarea usa `blastem -r E out/rom.bin`.

Para acabar de localizarlo, ¿me dices qué ves exactamente? Me sirve saber:

- El texto del error, o si es el panel de Problemas en vez de un fallo de `make`.
- Si lo lanzas con la tarea de VS Code o con `make` en terminal.

Y una posibilidad práctica: si construyes mientras yo estoy editando, cazas estados intermedios a medias. Si estás trabajando en paralelo, dímelo y te aviso cuando cada fase quede estable en vez de ir tocando ficheros mientras pruebas.


## 9. Tú · 2026-11-06

> se freezea al empezar

Eso es un bug real mío. Voy a localizarlo con el depurador de BlastEm en vez de adivinar. Primero veo si acepta comandos por stdin:

El depurador por stdin no suelta nada útil. Mejor: capturo la pantalla y lo veo yo mismo.

La captura funciona. Veo qué está pasando en pantalla:

La ventana no está en ese display. Capturo todas las pantallas:

Permiso de grabación de pantalla denegado (la captura sólo trae el escritorio, sin ventanas). Lo ataco por cálculo: verifico si la asignación de VRAM de sprites se pasa, que haría que `SPR_addSprite` devuelva NULL y corrompa la lista.

Encontrado. Veo la estructura real para confirmarlo:

**Encontrado el bug.** rescomp interpreta cada **fila** de la hoja como una *animación* distinta, no como frames: mis sprites tienen 8 animaciones de 1 frame (la araña, 8 de 8), y yo llamaba `SPR_setFrame()` con índices 0–7 fuera de rango → puntero basura → cuelgue. Verifico la API correcta:

Confirmado: `SPR_setFrame` fuera de rango sigue adelante y dereferencia basura. Antes de arreglarlo, establezco una medida objetiva del cuelgue con el log de eventos del emulador:

Ahora el arreglo: `SPR_setAnim` para la rotación y `SPR_setFrame` sólo para la animación de la araña.

Arreglado y verificado de forma objetiva: la actividad de hardware del emulador pasa de **1691 B/4s** (CPU colgada) a **15554 B/4s** (juego escribiendo sprites cada frame).

Arreglado. Era mío, de FASE 6.

## Qué pasaba

rescomp interpreta **cada fila de la hoja de sprites como una animación distinta**, no como frames de una sola. Mis hojas llevan las rotaciones en filas, así que la nave quedó con 8 animaciones de 1 frame y la araña con 8 animaciones de 8 frames.

Yo llamaba `SPR_setFrame(sprite, 0..7)` para elegir la rotación. Pero `SPR_setFrame` indexa **dentro de la animación en curso**: pedirle el frame 3 de una animación de un solo frame se sale del array. Y SGDK, aunque lo registra como error, **sigue adelante** y dereferencia el puntero basura. De ahí el cuelgue en el primer frame, en cuanto la nave intentaba apuntar.

La forma correcta con este reparto: `SPR_setAnim()` para la rotación, `SPR_setFrame()` sólo para el frame de animación de la araña, y `SPR_setAnimAndFrame()` para poner los dos a la vez sin que girar reinicie la animación. Corregido en los cuatro sitios: jugador, araña, cabezas y colas del gusano, y bala.

## Cómo lo verifiqué sin poder ver la pantalla

Intenté capturar la ventana pero macOS no le da permiso de grabación de pantalla a la terminal: la captura sólo trae el escritorio, sin ventanas. Y el depurador de BlastEm por stdin no suelta nada aprovechable.

Lo que sí sirvió: el log de eventos de hardware del emulador (`blastem -e`). Con el cuelgue crecía **1691 B en 4 segundos**; con el arreglo, **15554 B** — nueve veces más, que es el juego escribiendo sprites cada frame en vez de una CPU parada. Lo dejo corriendo en PAL para que lo mires.

## Lo que esto me deja claro

Mi comprobación de "sigue vivo tras N segundos" no servía para nada: un juego colgado también mantiene vivo el proceso. Ese cuelgue pudo estar ahí desde FASE 6 y yo lo di por bueno. A partir de ahora uso el crecimiento del log de eventos como señal de que el juego realmente corre, y te pregunto lo visual en vez de darlo por hecho.

Dime qué ves ahora: si arranca y se mueve, sigo con FASE 7 (armas y power-ups). Si sigue parado o ves basura en pantalla, dímelo y sigo cavando — con el log de eventos ya tengo con qué medirlo.


## 10. Tú · 2026-11-06

> si, se mueve, habrá que optimizar en el futuro.
> siguiente

Anoto lo de optimizar: la medición de rendimiento y el límite de 20 sprites por línea son FASE 10, y ahí entra.

FASE 7: armas y power-ups. Primero leo el GML de los 7 disparos y los power-ups, que aún no había mirado en detalle.

Dato importante: **todas** las balas descienden de `oBulletS`, así que el manejador del enemigo las cubre a todas. Leo el resto de armas y los power-ups:

Antes de implementar hay un reparto de presupuesto que cambia lo que se ve en pantalla, y el prompt manda preguntarlo. Los datos:

- Ahora: **69 de 80** objetos Sprite (jugador 1, balas 8, arañas 8, gusanos 52). Libres: 11.
- Los power-ups son **estáticos** en el original, así que los puedo pintar como tiles del plano A: **cuestan 0 sprites**. Eso lo hago en cualquier caso.
- Lo que sí cuesta: el LÁSER crea 10 segmentos a la vez, 3WAY mete 3 por disparo (~13 en vuelo), PIVOT tiene 3 satélites y la nave réplica hasta 2. Y el original pinta explosiones en cada impacto.

Claro, dime qué quieres aclarar.

Por si ayuda a enfocarlo, el contexto en corto:

**De dónde sale el límite.** La Mega Drive muestra como mucho 80 sprites por frame, y SGDK además tiene un pool de 80 objetos. Ahora mismo gasto 69: el jugador, 8 balas, 8 arañas y 52 partes de gusano (los gusanos son largos, 13 y 11 partes, que es lo fiel al original). Quedan 11 libres.

**Por qué las armas piden tanto.** El LÁSER del original crea 10 segmentos encadenados a la vez, el 3WAY dispara de tres en tres y llega a tener unos 13 en vuelo, el PIVOT tiene 3 satélites orbitando y la nave réplica hasta 2. Además, en cada impacto el original pinta una explosión.

**Lo que no es problema.** Los paquetes de munición y los power-ups son estáticos en el original (no se mueven, sólo laten), así que los pinto como tiles del fondo y no gastan ni un sprite, aunque sean 16 o 24 en pantalla. Eso lo haré de todas formas.

Así que la pregunta es de dónde saco los sprites que faltan: apurar hasta 80 clavados sin margen, recortar un gusano más (quedarían 2+1 simultáneos en vez de los 4+2 del original), o dejar los impactos sin explosión.

Y un apunte que quizá cambie tu criterio: en el original **el arma no se cambia durante la partida**, sólo en el menú entre partidas (`oWeaponBox` sólo lee izquierda/derecha `if global.isMenu`). Y sin munición siempre dispara la bala básica. Así que hasta que haga el menú (FASE 8) vas a ver casi siempre la bala básica, y las armas sólo aparecen al coger paquetes.

¿Qué te gustaría aclarar?


## 11. Tú · 2026-11-06

> puedes recortar armas, explosiones las quiero intactas


## 12. Tú · 2026-11-06

> continua


## 13. Tú · 2026-11-06

> continua, vuelve a preguntar. Recorta el laser

Entendido: recorto el láser, que era el que más gastaba con sus 10 segmentos simultáneos. Con eso el presupuesto deja de apretar y no hace falta tocar los enemigos. Quedan dos cosas por decidir:

Decidido: láser a 3 segmentos y explosiones (4 simultáneas). Presupuesto: pool compartido de 14 proyectiles + 4 explosiones = 79 de 80, sin tocar enemigos. Leo lo que me falta de las armas:

Todo el conjunto es portable. Añado el arte que falta al generador; primero sus orígenes y tamaños:

Sólo el proyectil WBLOCK da problema (96×116 con el origen en la punta: pediría una celda de 56). Lo resuelvo centrando el arte y emitiendo una tabla de desplazamiento por rotación, que reproduce el giro alrededor del origen con celdas pequeñas:

Arte listo y verificado (41 animaciones, ninguna vacía). Un último dato antes de codificar: qué hace la explosión de la bomba.

Escribo el módulo de proyectiles. Primero las máscaras exactas de cada uno:

Ahora el módulo de proyectiles con las 7 armas, y los efectos:

Ahora los paquetes de munición (como tiles, coste cero en sprites) y el cableado del bucle:

Error localizado (una llamada huérfana a la API vieja). Lo arreglo:

Se ha vuelto a colgar (1595 B/4s). Compruebo primero el presupuesto real de VRAM que pide cada definición:

Perfecto, eso es justo lo que necesitaba: un **ADDRESS ERROR** del manejador de excepciones de SGDK, con el PC en 0x97DE y `a1 = 00000002` (puntero basura). Lo mapeo a símbolo:

El error es un **ADDRESS ERROR** leyendo la dirección 3 con `a1 = 2`: un puntero basura. El PC (0x97DE) cae dentro de `updateFrame` de SGDK. Miro la pila para ver quién lo llamó:

`SPR_update` encuentra un sprite con puntero de animación corrupto, así que hay un índice fuera de rango. El sospechoso: la explosión, cuyos 2 frames pueden haberse quedado en 1 al reducir a 8×8.

Confirmado: rescomp dedujo que los 2 frames de la explosión son idénticos al reducir a 8×8 y dejó **uno solo** (`dc.w 256` = numFrame 1), así que mi `SPR_setFrame(…, 1)` se salía. Compruebo si a la araña le pasó lo mismo:

Sólo la explosión estaba mal (la araña sí tiene sus 8 frames). Lo arreglo, y de paso un fallo de fidelidad que vi al cablear: los paquetes nacían en la nave en vez de donde muere el gusano.

Corriendo (24320 B/5s). Antes de cerrar la fase, quito un apaño que había dejado: los paquetes usaban tiles del agujero como arte provisional.

FASE 7, parcial pero con lo gordo dentro. Y gracias por la captura: con el volcado de registros lo localicé en dos minutos.

## El ADDRESS ERROR

Mapeando el PC del volcado (0x97DE) contra `symbol.txt`: caía en `updateFrame`, llamado desde `SPR_update`, con `a1 = 2` (puntero de animación corrupto).

Causa: **rescomp deduplica frames idénticos**. Los 2 frames de `sprExplosion` a escala ×0.2 miden 6×6 px y salen iguales, así que rescomp dejó uno solo — y yo le pedía el frame 1. Mismo tipo de fallo que el cuelgue anterior, pero por el otro lado (ahí animación, aquí frame). Ahora compruebo `numFrame` en `resources.s` antes de indexar.

## Armas portadas

| Arma | Cadencia | Proyectiles |
|---|---|---|
| WBLOCK | 5 f | 1 a 7.69 px/f |
| LÁSER | 5 f | **3** segmentos (original 10), 9.59 px/f, vida 7 f |
| 3WAY | 10 f | 3 a ±30° |
| PARA | 5 f | 1 a la vez, 15.36 px/f |
| BOMBA | 5 f | mecha de 50 f, daña por área |
| RAY | 5 f | a los 25 f barre la pantalla |

Más munición, paquetes (24 posibles, como tiles del plano A: cuestan 0 sprites), desbloqueo por kills y explosiones. Presupuesto final: **79 de 80** objetos, VRAM 347 de 600.

## Tres hallazgos que cambian lo que verás

**El arma no se cambia durante la partida.** `oWeaponBox` sólo lee izquierda/derecha dentro de `if global.isMenu`: se elige entre partidas. Así que hasta el menú (FASE 8) el arma es siempre WBLOCK, y como la munición empieza a 0, lo que más vas a ver sigue siendo la bala básica — las armas aparecen al coger paquetes, que ahora salen donde muere cada gusano.

**Todas las armas hacen el mismo daño.** Todos los proyectiles descienden de `oBulletS`, así que el manejador del enemigo (−4 araña, −1 cabeza) los cubre a todos y destruye el proyectil antes de que corra el daño propio del arma (el −10 del 3WAY, el −20 del satélite). Se diferencian por cadencia, número de proyectiles y alcance, no por pegada.

**El rayo no puntúa.** `with oMalo instance_destroy()` mata a todos sin pasar por `checkMorte()`: ni kills, ni oleada, ni paquetes. Y encima crea un gusano de tipo 2. Portado tal cual.

## Lo que falta de esta fase

El arma 3 (PIVOT, con sus 3 satélites orbitando), la nave réplica (`oTri`) y los power-ups de bomba y rayos. Son los tres que tienen movimiento propio; el resto de la fase está cerrado. Con 79 de 80 objetos, meterlos exige repartir el pool compartido o recortar, así que prefiero abordarlos juntos sabiendo de dónde sale el presupuesto.

Dime si lo ves correr y sigo: o cierro lo que falta de FASE 7, o paso al menú (FASE 8), que es lo que desbloquea poder probar las armas de verdad.


## 14. Tú · 2026-11-06

> siguiente

FASE 7 no está cerrada, así que termino lo que falta: PIVOT, nave réplica y los power-ups de bomba y rayos. Un dato primero, que decide el daño del satélite:

`oOption` también desciende de `oBulletS`, así que el satélite hace el daño estándar. Añado al generador los dos sprites que faltan y los power-ups como tiles:

Ahora el PIVOT con sus satélites y la nave réplica, dentro del mismo pool:

Falta actualizar las referencias al tileset nuevo, y aprovecho para meter los otros tres power-ups:

Completo. Actualizo el informe:

FASE 7 cerrada: las 7 armas, los 3 power-ups y la munición completa.

## Lo que faltaba y ya está

**PIVOT** — el pivote es una segunda "nave" que tu input mueve a la vez que la tuya, con 3 satélites orbitando a 20 px y 120° de separación, girando 2.4° por frame de dirección pulsada. Con munición dejan la órbita y persiguen a tu objetivo a `move_towards_point(target, random(74))`: velocidad distinta cada frame, hasta 17.8 px/f. Tocar el pivote los destruye.

**Nave réplica** (`oTri`) — estática, apunta al enemigo más cercano y dispara sola: cadencia 20 frames si hay munición (y la gasta) o 52 si no. Hasta dos a la vez.

**Power-ups del suelo** — bomba (mata las arañas en pantalla), rayos (lanza un `oRay`) y paquetes. `generaPaquete` escala como el original: paquetes hasta 16, luego una bomba, luego hasta 2 réplicas mientras haya menos de 18 paquetes, luego rayos por debajo de 24, y si no más paquetes.

Los satélites y las réplicas salen del **mismo pool de 14 proyectiles** sin pedir sprites nuevos: con el PIVOT equipado el original no dispara balas, así que no compiten. Seguimos en 79 de 80 objetos. Los cuatro objetos del suelo van como tiles del plano A, con su arte real (`sprPaquete`, `sprCruz`, `sprWeapon4`, `sprWeapon7`).

Dos cosas que no se pueden portar y quedan anotadas: la **línea azul** que une la nave con sus réplicas (`draw_line_color` — el VDP no tiene primitivas) y la explosión en cruz, que usa el mismo destello que las demás.

`make` limpio sin avisos, ROM 85,6 KB, 22187 B de actividad en 5 s, corriendo en PAL.

## Un aviso para que no te extrañe

Sigues sin poder probar casi nada de esto: el arma se elige **sólo en el menú** y la munición empieza a 0, así que en juego ves la bala por defecto hasta que recojas paquetes — y los paquetes sólo salen cuando muere un gusano. El PIVOT, la réplica y el resto se quedan en el código hasta que exista el selector.

Eso es exactamente **FASE 8**: menú, estados de partida (`gameControl.State`), game over y el HUD completo con KILLS / LAST / TOP / NEXT. Es la fase que vuelve jugable todo lo de FASE 7. Sigo con ella cuando digas.


## 15. Tú · 2026-11-06

> sigue

FASE 8: menú, estados de partida y HUD completo. Leo el GML del menú:

El menú es arcade (con créditos). Compruebo esa parte y los sprites que usa:

Todo encaja en la pantalla. Genero el arte del menú (iconos de arma, logo y la barra gris):

Dos avisos legítimos (pasaba variables sin inicializar cuando no hay objetivo). Lo arreglo y pruebo:

Corre, pero 95 KB/5s de tráfico al VDP: estoy redibujando el logo y los iconos cada frame. Y he visto un fallo de diseño: scrollaba el plano A, que es donde está todo el texto. Lo arreglo:

Dos fallos visibles que debo arreglar antes de cerrar: la nave se queda en pantalla durante el menú y el HUD no se oculta (el original sólo lo pinta `if !instance_exists(oMenu)`).

FASE 8 lista. Ahora el juego tiene su estructura completa de recreativa.

## El original pide monedas

Esto no lo esperaba: `scrInit` deja `global.credits = 0` y `oMenu/Step_0` sólo arranca `if (hasCredits())`, descontando uno. **Sin crédito no se juega.** La tecla C mete crédito y X empieza.

Como en Mega Drive no hay teclado, el mapeo es: **START mete crédito**, **A/B/C empieza**, D-Pad mueve y elige arma. Queda documentado porque es traducción mía, no sale del GML.

## Pantallas

**Splash**: el logo real (`sprite176`, 256×256 → 51 px como bloque de tiles), la marquesina del original (*"PRESS A TO START... SHOOT ALL THE BUGS AND SNAKES... GAME BY REDEFINETECLAS 2012"*) y el contador de créditos.

**Interludio al morir**: escribe `KILLS`, `TOP` y `NEXT` letra a letra con la cadencia del original, la barra gris que crece hasta 60 px, los iconos de las armas desbloqueadas y una marquesina con el nombre del arma elegida (*WHITE BLOCK*, *LASER*, *3 WAY*, *CIRCLE*, *TRIANGLE OF DESTRUCTION*). Izquierda y derecha eligen arma, y con eso **ya puedes probar todo lo de FASE 7**.

Las posiciones salen del `Create_0` de `oMenu` escaladas: KILLS fila 13, TOP 15, NEXT 18, CREDITS 21, iconos 24, marquesina 29.

## Una optimización que hacía falta ya

La primera versión redibujaba logo e iconos cada frame, como el original: **95 KB de tráfico al VDP cada 5 segundos**. Pintándolos sólo cuando cambian baja a **22 KB**, el mismo nivel que en partida. La marquesina además usa el scroll por hardware del plano B y sólo reescribe texto al avanzar un tile entero.

También corregí dos fallos visibles antes de cerrar: la nave se quedaba en pantalla durante el menú y el HUD no se ocultaba (el original sólo lo pinta `if !instance_exists(oMenu)`).

## Lo que no se puede portar

El fogonazo de muerte (`oMuerte` se escala ×8 por frame y hace temblar el tamaño de la vista: el VDP no escala sprites) y las fuentes del original (`sFont2` son 98 glifos de 80×80 → 392 tiles de VRAM; `sFont3` queda en 4×4, por debajo de la rejilla). Va con la fuente del sistema.

**Pruébalo**: START para meter crédito, A para empezar. Al morir deberías ver el interludio y poder cambiar de arma — aunque las armas de verdad sólo disparan con munición, que sale de los paquetes al matar gusanos.

Queda FASE 9 (los 22 sonidos a YM2612/PSG) y FASE 10 (medición de rendimiento y sprites por línea, que es donde entra lo de optimizar que comentaste).


## 16. Tú · 2026-11-06

> ya no se ven los sprites de las bocas generadoras. Va muy lento. faltan muchas cosas. Que hacemos sigues u optimizas ahora?

Respondo primero a tu pregunta: **optimizo ahora**. Lo de las bocas es un bug que acabo de meter yo en FASE 8, y meter audio (Z80 + PCM) encima de un frame que ya va justo sería peor. El audio queda para después.

Empiezo por el bug, que es claro: el menú limpia el plano A entero y se lleva por delante los tiles de los agujeros.

Ahora el rendimiento. El sospechoso principal: el historial del gusano usa módulo 79 (una **división** del 68000) y se recalcula tres veces por frame para cada una de las 52 partes. Verifico la API de medición de SGDK:

Aplico las optimizaciones: historial en potencia de 2 (máscara en vez de división), posiciones de eslabón cacheadas una vez por frame, y multiplicaciones de 16 bits en las distancias.

Compila limpio. Añado el contador de FPS y carga de CPU en pantalla para poder medir — el original también llevaba uno (`draw_text(132,52,"FPS = "...)` en `oInGameHUD`):

Hecho. Resumen de lo que acabo de tocar:

**El bug de las bocas** era mío de FASE 8: el menú limpia el plano A entero (texto + tiles) al entrar y al salir, y los agujeros viven ahí. Se cargaban una sola vez al arrancar, así que desaparecían tras el primer menú. Ahora se repintan al empezar cada partida.

**Optimizaciones aplicadas:**

- El historial del gusano usaba **módulo 79**, y el 68000 no tiene división: eran ~300 divisiones por frame, porque las posiciones de los 52 eslabones se recalculaban tres veces (dibujado, apuntado y colisiones). Ahora el historial es de 128 (potencia de 2 → máscara) y las posiciones se calculan **una vez por frame** y se cachean.
- Las distancias del apuntado usaban multiplicación de 32 bits, que llama a `__mulsi3`. Forzadas a 16×16.
- El menú ya no redibuja logo e iconos cada frame (eso bajó el tráfico al VDP de 95 KB a 22 KB por 5 s).

**Y he puesto un medidor en pantalla**: FPS y carga de CPU, arriba a la izquierda, justo donde el original pintaba el suyo. Dime qué número ves en partida, porque "lento" puede ser dos cosas distintas y se arreglan de forma opuesta:

1. **Si el FPS baja de 50** → es CPU, y sigo optimizando (los siguientes candidatos son el `atan2` de la IA, la animación de la araña que sube 9 tiles por frame a cada una de las 8, y el límite de 20 sprites por línea).
2. **Si marca 50 estables** → entonces no va lento, va *fiel*, y eso es otra conversación: con los valores del GML la nave tarda **4,4 segundos en cruzar la pantalla**, exactamente lo mismo que en el original (6 px/frame sobre 1600 px). Tu port anterior tenía las velocidades subidas a mano precisamente porque pediste más ritmo. Si lo quieres así otra vez, lo limpio es un multiplicador global de velocidad en `arena.h` que puedas subir sin tocar ninguna constante portada, y queda documentado como desviación deliberada.

**Lo que falta**, para que elijas prioridad:

- **FASE 9 — audio**: los 22 sonidos y la música del menú a YM2612/PSG. Es la fase grande que queda.
- **FASE 10 — validación**: medir sprites por línea y rendimiento en hardware real.
- Detalles anotados que no se pueden portar tal cual: el fogonazo de muerte (`oMuerte` se escala ×8), la línea azul a las naves réplica, las fuentes de sprites del original y el latido de los paquetes.

Dime el número del contador y sigo por donde más duela.


## 17. Tú · 2026-11-06

> bien, continua

FASE 9: el audio. Primero veo con qué puedo decodificar los OGG y cuánto audio hay:

No hay decodificador de Ogg (ni ffmpeg, ni sox, ni `afconvert` sirve). Antes de pedirte nada, saco las duraciones reales parseando las cabeceras Ogg, para darte cifras de ROM:

Las cifras: 55,6 s de audio → **722 KB de PCM** a 13,3 kHz (la canción del menú son 334 KB ella sola). Cabe de sobra en un cartucho de 1 MB. El problema es decodificar los OGG. Busco si hay algo instalado que sirva:

VLC está instalado, y su CLI transcodifica sin ventana. Lo pruebo:

VLC decodifica bien (31 KB = 2,34 s ✓), sólo escribe una cabecera WAV inválida para streams. Eso lo reescribo yo. Veo qué pide rescomp para audio:

XGM2 con 3 canales PCM. Monto el pipeline de audio: VLC decodifica, yo reescribo la cabecera WAV y rescomp lo convierte.

589 KB convertidos. Ahora los recursos y el módulo de audio:

Compila. Cableo los sonidos en sus puntos exactos del GML:

ROM ahora de 768 KB (el PCM). Compruebo que sigue corriendo con el Z80 y el driver cargados:

Suena (la actividad sube a 212 KB/5s con la música). Me faltan las 7 voces que el original dispara al cambiar de arma; veo el mapeo y las añado:

FASE 9 lista: el juego ya suena. **Súbele el volumen.**

## El problema era decodificar

No hay ffmpeg, ni sox, ni oggdec, y el `afconvert` de macOS no sabe de Vorbis. Pero tienes **VLC**, y su interfaz `dummy` transcodifica sin abrir ventana. Escribe una cabecera WAV inválida para streams (formato 0, tamaño desconocido), así que `tools/gm2md_audio.py` extrae el bloque de datos y reescribe la cabecera a mano antes de dárselo a rescomp.

## Lo que suena

**20 de los 22 sonidos**, como PCM de 8 bits a 13300 Hz por el driver XGM2 (3 canales: el 1 para la música, el 2 y el 3 para efectos y voces):

- Cada efecto en su sitio exacto del GML: `shotSnd` en el `Create_0` de la bala, `wBlockSnd`, `threeWaySnd`, `laserSnd` en láser y bomba, `aranamorteSnd` en cada explosión, `powUpSnd` al coger paquete, `bombaSnd` + `risaSnd` con el power-up de bomba, `rayosSnd`, `repeteaSnd` con la nave réplica, `morteSnd` al morir.
- **La canción del menú** (25,6 s) al entrar en el interludio.
- **Las 5 voces de `triggerSpeech()`**: cada vez que cambias de arma en el menú, la voz que la anuncia.

679 KB de PCM → la ROM pasa de 128 KB a **896 KB**, tamaño normal de cartucho.

## Desviaciones

Dos canales para efectos donde GameMaker mezcla sin límite: en ráfaga, un disparo nuevo corta al anterior. Y 13300 Hz mono de 8 bits frente a los 11–44 kHz estéreo originales — es el techo del hardware.

## Dónde estamos

Quedan las fases 1 a 9 cerradas y la **10** a medias: ya hice la primera pasada de optimización (las ~300 divisiones por frame del gusano y el tráfico del menú), pero falta lo serio, y ahora hay un motivo nuevo para mirarlo: **en hardware real el Z80 leyendo muestras de ROM le roba bus al 68000**, así que el audio puede haber cambiado el rendimiento.

Por eso te vuelvo a pedir el dato que no llegué a tener: **qué marca el contador de FPS y el de carga de CPU** arriba a la izquierda, en partida y con enemigos en pantalla. Con ese número sé si ataco CPU (el `atan2` de la IA, la animación de la araña que sube 9 tiles por frame a cada una de las 8) o si lo que hay que tocar es la sensación de velocidad, que es fiel pero lenta.


## 18. Tú · 2026-11-07

> no veo animaciones de explosiones

Lo miro: sospecho que la vida que le puse es ridícula (2 frames). Releo el original completo:

Encontrado, y es culpa mía por asumir. El original hace esto:

```gml
t += random(3)
draw_sprite_ext(..., x, y, t, t, 0, c_white, 1)   // escala = t
if t > 6 instance_destroy()
```

La explosión es un sprite que **crece de 0 a 6 veces su tamaño** en unos 4 frames. Yo puse un frame fijo de 8×8 durante 2 frames: invisible. Genero los frames pre-escalados:

El anclaje era de una función que ya había renombrado. Lo inserto bien:

Los frames crecen correctamente (32 → 776 píxeles opacos). Ahora la lógica con la temporización real:

Arreglado y verificado: `numFrame = 5`, celda de 32×32, **un solo sprite de hardware** (el presupuesto sigue en 79 de 80).

Arreglado. Era un fallo mío de porting, no de dibujado: **no había leído bien cómo funciona la explosión del original.**

```gml
t += random(3)
draw_sprite_ext(..., x, y, t, t, 0, c_white, 1)   // la escala ES t
if t > 6 instance_destroy()
```

No es una animación de frames: es **un sprite que crece de 0 a 6 veces su tamaño** y desaparece, en unos 4 frames. Yo lo porté como un destello fijo de 8×8 durante 2 frames — en pantalla, nada.

## Lo que hice

El VDP no escala sprites, así que el generador produce **cinco frames pre-escalados** (escalas 1 a 5 del arte original, celda de 32×32) y el código elige el que toca según `t`, con el mismo incremento aleatorio del original convertido a 50 Hz. Los píxeles opacos por frame van 32 → 120 → 276 → 496 → 776, que es el crecimiento.

Verificado en el fichero generado: `numFrame = 5`, celda 32×32, **`maxNumSprite = 1`** — sigue costando un solo sprite de hardware, así que el presupuesto no se mueve de 79 de 80.

Corté el crecimiento en escala 5 (32 px) en vez de 6 (38 px): una celda de 40×40 son 5×5 celdas y el VDP sólo compone 4×4 en un sprite, así que cada explosión pasaría a costar dos. Queda anotado.

Cada explosión suena con `aranamorteSnd`, como en el original.

## Lo mismo puede estar pasando en otros sitios

Este patrón —el original escalando o rotando en tiempo real— ya me ha mordido dos veces. Los que quedan con escalado que aún no se ve:

- **El latido de los paquetes** (`draw_sprite_ext` con escala oscilando entre 0.8 y 1.4): ahora son tiles fijos.
- **Los power-ups de bomba y rayos**, que giran (`ro += 2`): tiles fijos.
- **El fogonazo de muerte** (`oMuerte`, escala `t*8`): no portado.

Los tres se arreglan igual, con frames pre-escalados, pero cuestan sprites y ahora mismo hay **uno libre**. Dime si quieres alguno y de dónde lo recorto, o si prefieres que siga con la medición de rendimiento de FASE 10 — para la que me sigue faltando el número del contador de FPS.


## 19. Tú · 2026-11-07

> sigue sin verse

The render path is fine — I verified it in the emulator. Now let me check the original's death event, because the quantity may be what's off:

Now I know what's actually missing. Let me look at the dust art the original draws:

Now I'll rewrite the explosion generator to include the dust the original throws with every kill:

Now I can see the screen. Let me capture the explosion:

I found something that looks like bad codegen. Let me check it in isolation:

Now let me verify on screen:

Now the documentation:

Arreglado y, esta vez, **comprobado por mí en el emulador** — no a ojo tuyo.

## Lo primero: ya puedo ver la pantalla

El bloqueo de todo esto era que no tenía forma de mirar el juego. Lo he resuelto: BlastEm trae depurador, pero ignora la entrada redirigida; dándole un pty (`tools/mdrun.py`) responde. Desde ahí puedo **capturar la pantalla en PNG**, **simular el mando** (entrar al menú, moverme, disparar), parar en una función, avanzar fotogramas exactos, **listar la tabla de sprites del VDP** y **leer la RAM**. Eso ha cambiado el diagnóstico de adivinar a medir.

## Lo que pasaba de verdad: el fogonazo no es la explosión

Una muerte en el original son **dos** objetos, y yo sólo había portado uno:

| | Qué es | Cuánto dura | Tamaño |
|---|---|---|---|
| `oExplosion` | el fogonazo blanco | **4 frames (67 ms)** | hasta 38 px |
| `oca` | la **humareda** oscura | **40 frames (0,67 s)** | hasta **124 px** |

El fogonazo estaba y funcionaba: lo verifiqué fotograma a fotograma en la tabla de sprites (16→24→32 px en el punto de la muerte). Pero es un parpadeo de 67 ms. **Lo que el ojo ve de una muerte es el polvo, y el polvo no estaba portado.** Ahora está, encadenado en la misma ranura: fogonazo y luego humareda, **sin gastar ni un sprite nuevo**.

Verificado capturando la secuencia de una muerte: fogonazo blanco 4 frames, humareda oscura de ~50 px durante 33.

## Dos cosas más que encontré por el camino

**La tabla de sprites estaba llena siempre.** `SPR_addSprite` de SGDK no activa el cálculo de visibilidad, así que las 79 ranuras ocupaban las 80 entradas del VDP **todos los frames**, incluidas las aparcadas fuera de pantalla. No cabía nada nuevo. Con `AUTO_FAST` la tabla lleva sólo lo vivo: **16 entradas** en partida normal, y su DMA baja de 632 a 128 B por frame.

**Un fallo de GCC.** Con la humareda puesta, la explosión salía en posiciones absurdas. En el desensamblado, `m68k-elf-gcc 16.2.0` con `-O2`/`-O3` pierde el desplazamiento de dos campos y compila `e->x = x; e->y = y;` como dos escrituras al **offset 0** del struct — machacando otro campo y dejando x/y sin escribir, para luego leerlos de +6 y +8. No es UB mío: a `-O1` sale bien. Lo esquivo con `NO_INLINE` y dejo `tools/check_codegen.py`, que revisa el ROM entero tras cada build; ahora mismo sólo marca rutinas de SGDK donde escribir dos veces al mismo puerto es legítimo.

## Y de paso, el dato que te pedía dos veces

Ya no hace falta que me lo des, lo he leído yo del contador: **50 Hz estables, 39–56 % de CPU jugando, pico de 78 % con explosión**. Sin frames perdidos y sin descartes de DMA (256–1152 B por frame frente a un tope de 8000). Hay margen: FASE 10 no necesita optimización de CPU, sólo validación en hardware real.

Todo documentado en `docs/PORT_AUDIT_GML.md`, con las desviaciones: la humareda va topada a 64 px (el original llega a 124, y eso serían 16 sprites de hardware de los 80), anima a la mitad de ritmo para repartir el DMA, y es opaca y sin cabeceo porque el VDP no mezcla ni rota.

El ROM está corriendo en PAL. El contador de FPS/CPU sigue en pantalla arriba a la izquierda; lo quito cuando cerremos FASE 10 con la prueba en consola real.


## 20. Tú · 2026-11-07

> eres un especialista en optmización para mega drive
> quiero que optimices el juego de forma que aunque la pantalla este llena de sprites nunca baje el framerate

Voy a hacerlo con medición, no a ojo. Primero leo el camino caliente completo:

Antes de tocar nada, necesito el caso peor medido. Monto un arnés de estrés y un banco de pruebas:

**Caso peor medido: 25 fps, 190 % de CPU.** Ahí está el problema. Ahora, a localizarlo:

Datos claros. **Proj_scan 79 % de un frame, Snake_updateAll 53 %, Enemy_updateAll 38 %** — suma 190 %, que es justo lo medido. Veo el código caliente:

Ya sé dónde está todo el tiempo. Empiezo por la raíz común: matemáticas y colisiones que son llamadas a función en vez de código inline.

Ahora entity.c, que es donde está el `atan2` caro:

Ahora snake.c, que es el otro 53 %:

Buen avance (190 % → ~132 %). Ahora necesito saber qué queda dentro de los dos bucles grandes:

Encontré algo gordo de paso. Mira el código que genera GCC para `SPR_setPosition` de SGDK:

Encontré el fallo: la cola de DMA se desborda y la tabla de sprites, que va la última, se descarta entera. Lo confirmo:

El cuello de botella real es el DMA de tiles: al ir ahora a 50 fps se suben el doble de tiles por vblank y se desborda. Voy a precargarlos todos en VRAM:

Ahora escribo la precarga de tiles en VRAM, que es la optimización que de verdad quita el cuello de botella:

Ya renderiza con la arena llena (103 %). Sigo: las llamadas de animación de SGDK y su animación automática:

Voy a medir con el contador del VDP dentro de la partida real, que es más fiable que el banco sintético:

Mediciones fiables ya. Ahora el bucle del gusano, que es el más caro:

Necesito el reparto real dentro de la partida, no sintético. Instrumento el bucle:

Snake sigue siendo el 32 %. Lo desgloso por dentro:


## 21. Tú · 2026-11-07

> [Usage limit reached; a short grace allowance remains, then this turn is cut off without warning. Judge what's left: if it's genuinely only a few steps, finish it properly, then stop and say so without extra polishing. Otherwise don't attempt it all: finish the piece in hand and at most two more like it, then stop. The allowance also pays for any subagents you have running: they don't get this message and will keep going, so it's up to you how to handle them. Either way, end by telling the user their usage limit was reached, what's done and what's left.]

Se me ha acabado el límite de uso, así que paro aquí y te dejo el estado exacto.

## Lo que medí primero

Monté un arnés que llena la arena a los topes del port (8 arañas, los 4 gusanos con sus 52 eslabones, 14 proyectiles) y mide: **25 fps y 190 % de CPU**. Es decir, el peor caso tardaba casi dos frames en calcular uno.

## Lo que encontré y arreglé

| Causa | Arreglo |
|---|---|
| `F16_toInt`, `F16_mul`, `F16_sin/cos` de SGDK son funciones en otra unidad y el enlazado va sin LTO: **~600 `jsr` por frame** para un desplazamiento o una tabla | Las mismas operaciones en línea (`inc/fastmath.h`) |
| `F16_getAngle` acaba en una división de 32 bits por software y un polinomio de grado 7: **~3000 ciclos por enemigo y frame** | `Fast_angle`: octante + `divu.w` del 68000 + tabla. ~200 ciclos, 0,23° de error máximo |
| `Coll_overlap` era una llamada de ocho argumentos ~170 veces por frame | `static inline` |
| Cada consulta recalculaba la caja entera de cada enemigo desde el fix16 | Caché compactada de vivos, una vez por frame |
| **Las subidas de tiles por DMA: ~16 KB por frame contra los ~8 KB de un vblank PAL** | Los 577 tiles de todas las definiciones se precargan en VRAM y cambiar de rotación pasa a ser escribir un índice: **cero DMA de tiles** |

Ese último punto es el importante y además era un **fallo funcional**, no sólo de velocidad: al optimizar el juego llegó a 50 fps, con ello se duplicaron las subidas de tiles por vblank, SGDK empezó a descartar transferencias y —como la tabla de sprites se encola la última— **la pantalla se quedaba congelada**. Iba oculto detrás de la lentitud.

## Dónde está ahora

| | Antes | Ahora |
|---|---|---|
| Partida normal | 50 fps, 39-78 % CPU | 50 fps, **19 %** |
| Arena llena | 25 fps, 190 % | **103 %**, y dibujando |

## Lo que falta, y no es poco

**No he llegado a tu objetivo.** Con la arena a tope sigue en 103 %, justo por encima del frame, así que ahí todavía baja a 25 fps. Queda, por orden de lo medido y anotado en `docs/PORT_AUDIT_GML.md`:

1. **El bucle de eslabones del gusano: 16,5 % del frame.** 525 ciclos por eslabón sólo para colocar un sprite es muchísimo más de lo que ese código debería costar. Estaba leyendo el desensamblado para ver por qué cuando se cortó: es el siguiente paso y probablemente el que más da.
2. **Quitar el contador de FPS/CPU de pantalla: 7,6 % gratis** (`HUD_SHOW_PERF` en `inc/hud.h`). Lo dejé puesto porque lo necesitaba para medir.
3. **`SPR_update` de SGDK: 12 %** — 400 ciclos por sprite × 79. Ya no le usamos ni la posición, ni los tiles, ni el frame; montar la tabla de sprites a mano costaría ~40.
4. **El apuntado: parte del 10 %** — recorre los 60 enemigos y eslabones cada frame.

Sumados son de sobra para bajar de 100 % con margen, pero son trabajo, no un ajuste.

La ROM que está corriendo es la normal (sin arnés), compilada limpia y con `tools/check_codegen.py` en verde.


## 22. Tú · 2026-11-07

> implementa las armas


## 23. Tú · 2026-11-07

> [Earlier usage-limit notes no longer apply. Continue working normally. Don't mention this to the user.]

You've hit your 5-hour limit · resets 6:10pm (Europe/Madrid)


## 24. Tú · 2026-11-07

> Your claude.ai usage limit has reset. Continue the task you were working on when the limit was reached; do not repeat work that is already complete.

Las 7 armas están portadas según el informe. Antes de tocar nada, compruebo contra el GML si falta alguna de verdad:

El mapeo del port coincide con el `switch` de `oPlayer/Step_0`, así que las 7 están. Voy a verificar en el emulador cuál dispara de verdad, forzando arma y munición:

Dos armas no disparan nada: **láser y PARA**. Voy al código:

Las dos “que no funcionan” sí disparan: su arte se quedó en **6 y 10 píxeles**. El escalado ×0.2 se come los sprites finos:

El láser sí dispara: lo que falla es que lo recorté a 3 segmentos por un presupuesto de sprites mal calculado. Los segmentos salen del pool de 14 proyectiles, no de los 80 objetos:

Hay algo más: miro si el arma elegida en el menú sobrevive al empezar la partida:

Encontrado el fallo de verdad: **el port resetea el arma a 0 al empezar la partida, y el original no lo hace.** Elijas lo que elijas en el menú, jugabas siempre con WBLOCK:

Ahora la prueba de verdad: elegir arma en el menú con el mando y ver si llega a la partida:

Verificado de punta a punta: elijo 3WAY en el menú, empiezo partida y **el arma llega a la partida** (antes se perdía). Documento y cierro:

Las siete armas ya estaban portadas desde FASE 7. Lo que fallaba es que no se notaban, así que las **forcé una a una en el emulador y las miré en pantalla**, y salieron dos fallos reales.

## El fallo gordo: el menú no servía de nada

`Game_start()` hacía `g_weaponIndex = 0`. Fui al GML y `startGame()` **no toca** `global.weapon_index`: sólo se pone a cero en `scrInit`, al arrancar la máquina. Por eso en el original el arma que eliges en el interludio vale para la partida siguiente, y aquí **se perdía siempre**: eligieras lo que eligieras, jugabas con el bloque blanco.

Quitada esa línea. Comprobado de punta a punta con el mando simulado: elijo 3WAY en el menú, meto crédito, empiezo partida, y el arma sigue puesta y dispara el abanico.

La munición sí se reinicia por partida, y eso estaba bien: `oPlayer/Create_0` hace `global.playerBulletsCounter = 0` y `startGame()` crea el jugador.

## El láser: un recorte mío que no hacía falta

Lo recorté a 3 segmentos (de los 10 del original) por un cálculo equivocado: creí que competían por los 80 objetos Sprite del VDP, y en realidad **salen del pool de 14 proyectiles que ya estaba reservado**. Con 3 segmentos de 1 px el haz medía 18 px y no se veía. Restaurado a 10: ahora es un haz de ~60 px, visible.

## Estado de las siete, verificado en pantalla

| # | Arma | Antes | Ahora |
|---|---|---|---|
| 0 | WBLOCK | ✅ | ✅ |
| 1 | LASER | ❌ nada | ✅ haz de ~60 px |
| 2 | 3WAY | ✅ | ✅ |
| 3 | PIVOT | ✅ tres satélites | ✅ |
| 4 | PARA | ❌ no la vi | ✅ está, mide 2×6 px |
| 5 | BOMBA | ✅ | ✅ |
| 6 | RAY | ✅ | ✅ |

PARA no estaba rota: es que mide eso. `sBulletPara` son 10×28 px en un room de 1600×1200, que a escala ×0.2 son 2×6 px, y vive 5 frames. Es fiel; si la quieres más visible hay que desviarse del original a propósito, dímelo.

## Dos cosas que el original tampoco usa

- **`oCircular`** aparece en la lista de armas desbloqueables, pero lo único que lo crearía está comentado (`//instance_create(x,y,oCircularManager)`). No existe en juego, así que no se porta.
- **BOMBA y RAY** no son seleccionables: la lista del menú tiene 5 entradas y el `switch` de disparo llega a 7. El rayo entra por su power-up; la bomba es código muerto, igual que allí.

Y una rareza del original que queda portada tal cual: los **iconos del menú no se corresponden con el arma**. La lista es `wBlock, laser, threeWay, circular, tri`, pero el `switch` que dispara hace pivote y para en los índices 3 y 4.

Todo en `docs/PORT_AUDIT_GML.md`. La ROM está compilada y corriendo en PAL.


## 25. Tú · 2026-11-08

> quiero que leas el repo surfize y vamos a seguir por ahi.

Ahora el desplazamiento dentro de `sprPos`, que es lo que hace que los sprites se muevan con el plano sin tocar ninguna llamada:

Ahora el informe. Añado la sección nueva antes de las pruebas y las filas de la tabla:

Leído el original entero: los 61 objetos con sus padres y sus eventos de colisión, no sólo los scripts. Salieron **cuatro mecánicas que no estaban portadas y dos que estaban portadas mal**. He implementado las que no dependen del presupuesto de sprites.

## Lo que faltaba y ya está

**1. La sacudida de pantalla.** `stShake` estaba en la tabla de equivalencias de la FASE 3 y nunca se implementó. No es un adorno: salta **con cada disparo**, y el juego dispara solo. Siete puntos de llamada en el original. Y la duración no es el primer parámetro: `shake_magnitude` no empieza a bajar hasta que `shake_time` llega a cero, así que una sacudida dura `time + magnitud/fade`. La del `oExplosionCruz` son **4,3 segundos a 30 px**.

En Mega Drive no hay cámara: muevo el plano A con los registros de scroll y le resto lo mismo a cada sprite dentro de `sprPos`, así que no cambia ni una llamada del juego. Medido en el emulador leyendo la tabla de sprites del VDP a la vez que las variables: cámara −1 → el jugador sale en la columna 157, cámara +1 → en la 155, y la banda del HUD se desplaza esos mismos 2 px en el mismo sentido. Plano y sprites van juntos.

La del disparo sin munición no se ve: magnitud 1 del room son 0,2 px. La porto igual en vez de inventarme un número.

**2. Los eslabones del gusano son un escudo.** Esto cambia cómo se juega, y la auditoría lo tenía al revés. Yo había leído sólo la mitad del intercambio: cuando una bala toca un eslabón corren **dos** manejadores. El del eslabón (heredado de `oMalo`) pinta la explosión y no destruye la bala; el de la propia bala hace `instance_destroy()`. Los eslabones tienen `hp = 10` y **nadie comprueba nunca ese hp**, así que no se matan: se quedan la bala y siguen.

Las cabezas no pasan por ahí porque definen su propio `Collision_oBulletS`, que sustituye al del padre. Por eso de una cabeza no sale metralla y de un eslabón sí.

Resultado: **un gusano enroscado tapa su propia cabeza**. Hay que buscarle el ángulo. Es lo que justifica que el bicho mida 12 eslabones, y el port se lo saltaba. Verificado con un contador temporal: 4 absorciones en 400 frames de arena llena, con los kills subiendo a la vez.

**3. El power-up de la bomba.** `hp = 0` no mata a nadie — ese hp sólo se mira en el impacto de bala. Lo que limpia la pantalla es el *Animation End* del `oExplosionCruz`, dos frames después, y lo hace **sin pasar por `checkMorte()`**: no suma kills, no genera paquetes, no genera oleada, y deja dos gusanos de tipo 2 nuevos. El port sumaba las arañas al marcador y dejaba vivos a los gusanos. Corregido, y borrado `Enemy_killAll()` porque codificaba la regla equivocada.

**4. `oPivot` no se puede tocar.** Tiene `spriteId` y `spriteMaskId` a `null`: `oPlayer/Collision_oPivot` no corre nunca en el original. `Proj_clearSatellites()` se queda sin nadie que la llame, igual que allí, pero ahora está anotado para que no parezca un cabo suelto.

## Coste

Caso peor **103 % → 104 %** de CPU. Casi nada, porque metí una caja envolvente por gusano en `cacheParts` y con ella `Snake_hit` descarta el bicho entero con cuatro comparaciones en vez de probar 12 cajas; de paso se la puse a `Snake_collides`, que ya recorría los 48 eslabones cada frame. Partida normal sigue a 50 fps.

## Lo que sigue faltando, y qué lo bloquea

La pieza gorda es **`oEstallido` → 12 `oDestro`**: cada impacto de WBLOCK o de un satélite del PIVOT **contra un eslabón** lanza 12 astillas a 30° de separación que **restan 1 de hp** a arañas y cabezas. Es una reacción en cadena, y es la razón de ser del arma por defecto. También faltan el efecto de impacto del PARA (`oAutoDestruccion`) y su haz (`object1`).

Las tres chocan con lo mismo: 12 objetos Sprite por ráfaga y el pool de SGDK está en 77 de 80. Es el mismo tope que el punto 3 de la lista de optimización pendiente — montar la tabla de sprites a mano en vez de usar `SPR_update` quita el límite **y** devuelve el 12 % de frame que cuesta `SPR_update`. Hacer eso primero resuelve las dos cosas a la vez, y es por donde yo seguiría.

Detalle aparte: las 12 astillas salen del mismo punto a la misma velocidad en ángulos fijos, o sea que son un anillo que se expande. Se pueden dibujar con **un** sprite de animación pre-generada y calcular el daño de los 12 puntos analíticamente, sin 12 ranuras.

Corrección a la auditoría anterior: decía que `oDebrie` no estaba portado y que daba igual porque en el GML está comentado. Eso era cierto, pero me salté que `oDestro` es otro objeto distinto y sí está vivo. Queda anotado.

Todo en `docs/PORT_AUDIT_GML.md` con sus filas de prueba. `make` limpio, `check_codegen.py` limpio, ROM reconstruida.


## 26. Tú · 2026-11-08

> Haz una revision general, usa los assets de surfize si hace falta. Mira qué falta pata que quede identico a surfize. Lo más parecido posible. Implementa todo lo que falta y después como especialista en optimización y soluciones para la megadrive que eres, refactoriza y optimiza el código para que la performance sea super fluida

Voy por partes: primero la revisión completa contra el original, luego el motor de sprites (que es lo que bloquea lo que falta y a la vez es la mayor optimización pendiente), luego las mecánicas, y al final el barrido de rendimiento.

Tengo el mapa completo. Empiezo por el motor de sprites propio: es lo que bloquea la metralla y el haz del PARA, y a la vez la mayor optimización pendiente.

**Encontrado.** `rescomp` marca con el bit alto de `numSprite` los frames que son un único sprite de hardware alineado con la celda; yo lo estaba leyendo como un contador y el bucle se desbocaba.

41 fps / 89 % en el caso peor (venía de 24 fps / 104 %). Ahora el presupuesto de sprites está libre. Paso a generar el arte que falta:

API Error: Can't reach the API server — check your internet or DNS (ENOTFOUND)


## 27. Tú · 2026-11-08

> Continua

Añado el arte que falta al generador:

Ahora el módulo de metralla (`oEstallido` → 12 `oDestro`):

Ahora el número que vuela al HUD (`oPuntoMorte`) y el haz del PARA (`object1`):

Ahora la cascada de explosiones de `morteGusano` y el cableado en `game.c`:

**Encontrado un fallo latente:** `res/gm_palettes.h` es un sobrante del 6 de noviembre con la paleta vieja, y es el que carga `main.c`. Por eso el humo de las explosiones salía negro en vez de gris y los puntos azules salen negros.

Ahora la fase de optimización. Primero mido dónde se va el tiempo con el perfilador:

Con los datos delante, optimizo donde pesa: gusano (30 %), montador de sprites (12 %) y HUD de depuración (9 %).

Las medidas bailan porque la semilla aleatoria sale del contador de línea. La fijo para poder comparar:

El filtro de pantalla del montador cuesta un 5 % del frame. Lo quito aparcando en origen lo que se sale, que es más barato:

Mientras termina la medición, escribo el informe:

Llego al punto de rendimientos decrecientes. Hago la verificación final y las medidas definitivas:

Una decisión de diseño importante, con la medida delante: cada gusano de más cuesta ~20 % del frame. Antes de tocar la fidelidad, aprieto la colisión y el montador:

La caja partida sale peor (41,8 frente a 43,0 fps): el coste de mantener dos cajas supera lo que ahorra. La revierto:

**Encontrado otro fallo latente:** `Menu_init()` existe pero nadie la llama, así que los iconos de arma, el logo y la barra del menú nunca se subían a VRAM.

Hecho. Revisión completa contra el original, lo que faltaba implementado, y la optimización medida.

## La revisión encontró dos fallos latentes

**La paleta que se cargaba no era la generada.** `tools/gm2md.py` escribe `res/gm_art.h`, pero quedaba un `res/gm_palettes.h` del 6 de noviembre con la paleta vieja y el mismo guardián de include. `main.c` incluía el viejo: **la humareda de todas las explosiones llevaba desde la FASE 7 saliendo negra en vez de gris**. Borrado el duplicado.

**`Menu_init()` nunca se llamaba.** Estaba escrita desde la FASE 8 y nadie la invocaba, así que los tiles del menú no se subían a VRAM: ni iconos de arma, ni logo, ni barra del interludio. Sólo se veía el texto. Llamada desde `Game_init()`, y el menú ya se parece al original.

## El motor de sprites, fuera

Todo lo que faltaba chocaba con el mismo muro: los 80 objetos Sprite de SGDK, con el port en 79. Así que escribí el motor propio (`inc/spr.h`), que era además el punto 3 de la lista de optimización pendiente. `SPR_update` son ~400 ciclos por sprite para mirar banderas de animación, reserva de VRAM y subida de tiles que este port no usa.

Lo que costó encontrar: **`numSprite` negativo no es un contador**, es la marca con la que rescomp dice "este frame cabe en un sprite de hardware alineado". Leyéndolo como contador el bucle se desbocaba y la pantalla se llenaba de basura.

Caso peor: **de 24 a 41 fps** sólo con eso.

## Lo que faltaba, ya portado

| | Del original |
|---|---|
| **Metralla** (`oEstallido`→12 `oDestro`) | 12 astillas cada 30° que **restan 1 de hp**: mata en cadena. Sale cuando un WBLOCK o un satélite del PIVOT da en un **eslabón** de gusano |
| **`oMuerte`** | El fogonazo blanco que se come la pantalla |
| **`morteGusano`** | El gusano revienta eslabón a eslabón, no desaparece |
| **`oPuntoMorte`** | El número que sale volando al HUD, con los dígitos de `sFont2` |
| **Haz del PARA** (`object0`/`object1`) | 14 segmentos por delante de la nave, que hacen daño |
| **Línea azul a las réplicas** | `draw_line_color`, aquí como puntos |
| **`oAutoDestruccion`** | El impacto del PARA, que revienta en metralla a los 80 frames |

Fuera quedan, documentados: el destello al coger un paquete (es alfa 0,14, sin transparencia sería peor el remedio), las curvas de rayos, y el código muerto del original (`oCircular`, `oRayoBota`, `oObserver`, `oDebrie`…).

## Rendimiento

| | Antes | Ahora |
|---|---|---|
| Partida normal | 50 fps, 19-34 % | **49,5 fps, 37 % de pico** |
| Caso peor | 24 fps, 104 % | **43,1 fps, 78 % de media** |

Ocho optimizaciones medidas una a una (motor de sprites, contrato de aparcado, bucles del gusano partidos, caja envolvente cada 4 frames, historial en enteros, sesgo de pantalla fuera del bucle, apuntado con la distancia arrastrada, contador de depuración fuera). Y **una que probé y descarté porque salió peor** —partir la caja del gusano en dos mitades, 41,8 frente a 43,0— anotada en el código para que no se repita.

De paso arreglé el arnés: la semilla aleatoria salía de `GET_VCOUNTER`, así que dos medidas nunca comparaban la misma partida y las diferencias del 5 % eran ruido.

## Lo que no he conseguido, y por qué

**El caso peor no llega a 50 fps.** La media es 78 %, pero los frames con los cuatro gusanos vivos a la vez se van al 117 % y ahí se cae uno de cada seis.

Lo medí: **cada gusano de más cuesta un 20 % del frame**. Con 2+1 en vez de 3+1 sube a 47,8 fps con un pico del 68 %. Esa es la palanca que queda, y es justo la que la decisión 4 del proyecto reserva para cuando haya medición delante: recortar `MAX_CABEZA_1`, nunca la longitud. **No la he tocado porque es cambiar fidelidad por framerate y esa decisión es tuya** — el port ya está en 4 gusanos frente a los 6 del original.

Si prefieres los 48 fps, es una línea. Lo demás que queda (el montador y el bucle de eslabones en ensamblador, ~24 % entre los dos) daría menos y cuesta bastante más.

Todo en `docs/PORT_AUDIT_GML.md`, con las 15 filas de prueba nuevas. `make` limpio, `check_codegen.py` limpio, ROM verificada de punta a punta en el emulador.


## 28. Tú · 2026-11-08

> recorta la cabeza y haz una revisión de los sprites en pantalla por que se come unos a otros y se ve mal. Eso hay que arreglarlo, se tienen que ver nítidos sin comprometer la performance. Eres un experto en optimización y creación de videojuegos con las mejores técnicas para megadrive. Te conoces los repositorios con mejores técnicas y este proyecto lo vas a dejar lo más perfecto posible en relación a su original que es surfize

Dos cosas: recorto los gusanos y me pongo con lo que se ve mal. Empiezo mirando un frame cargado de verdad.

Los sprites no se pierden (peor línea: 11 de 20, 192 de 320 px). El problema es otro: el humo se dibuja como un disco **opaco**. En el original es un velo translúcido. Reviso todo el arte:

El arte sale **más pequeño de lo que debería**: la nave mide 8×8 cuando tendría que medir 8×12, y la araña 8×7 en vez de 13×13. Al reducir 5:1 los bordes suaves bajan de alfa 128 y el umbral se los come. Lo mido:

El arte estaba perdiendo tamaño y color. Compruebo los tonos reales del humo compuesto sobre el fondo del room:

Ahora lo arreglo en origen. El fondo del campo es un color plano, así que **toda la transparencia del original se puede hornear a su color exacto en pantalla** — es la técnica correcta aquí y no cuesta nada en ejecución:

**50,0 fps en partida normal con un pico del 15 % de CPU, y 48,4 fps en el caso peor sin un solo frame por encima del presupuesto.** Documento y cierro:

Hecho. Las dos cosas, con medidas.

## Por qué se veía mal (no era lo que parecía)

Lo primero fue descartar la sospecha obvia: volqué la tabla de sprites del VDP en el frame más cargado y conté por línea de barrido. **Peor línea: 11 sprites y 192 px**, con topes de 20 y 320. No se estaba perdiendo ni un sprite. El problema era el arte.

**1. El umbral de alfa se comía medio bicho.** Al indexar se descartaba todo píxel con alfa < 128. Pero el arte viene de un room de 1600×1200 y se reduce **5:1**: un detalle de un píxel queda en alfa 51 y desaparecía.

| | Original a ×0,2 | Se dibujaba |
|---|---|---|
| Nave | 8×12 | **8×8** |
| Araña | 13×13 | **7×8** |

La araña se pintaba con la mitad de su silueta. Con el umbral a 48 pasa de 1892 a 4444 píxeles opacos.

**2. Toda la transparencia del original se portaba como opaco.** Esto es lo que "se comía" las cosas. El humo (`oca`) son dos sprites a **alfa 0,8** sobre arte que ya es gris translúcido: en pantalla da (51,59,59)–(66,73,73). Yo lo pintaba como un disco gris (73,73,73) plano, que tapaba la nave, los agujeros y los enemigos.

La solución correcta aquí: **el campo de juego es un color plano**, así que se puede calcular el color final exacto de cada efecto translúcido y meterlo ya mezclado en la paleta. Sale idéntico al original mientras el efecto esté sobre el fondo, y **no cuesta un solo ciclo en ejecución**. La paleta pasa de 7 a 12 colores con los cinco tonos mezclados calculados componiendo el arte con su alfa sobre el fondo.

Afecta a la humareda, la metralla (`sprDebri` es blanco a alfa 0,5), el haz del PARA (alfa 0,55) y el fogonazo (`sprExplosion` es gris 239, no blanco).

**De regalo, tres armas que llevaban el color equivocado:** al meter azul y rojo en la paleta se arreglaron el **láser (es rojo** — 180 píxeles rojos de 192 en el original), el **3WAY (rojo y blanco)** y el **PARA (azul, 0,0,192)**. Se portaban en blanco y negro.

**3. El orden de la tabla ya era el bueno.** Jugador → arañas → gusanos → proyectiles → haz → explosiones → metralla. La nave siempre delante del humo, y los efectos lo primero que se caería si se llegara a las 80 entradas. Coincide con los `depth` del original.

## El recorte

`MAX_CABEZA_1` de 3 a 2: **3 gusanos** en vez de 4. Es el segundo recorte de ese valor, y el primero por CPU y no por sprites —el motor nuevo ya no tiene el tope de 80 objetos—. La longitud de la cadena no se toca.

## Dónde queda

| | Empezando la FASE 10 | Antes de esta pasada | Ahora |
|---|---|---|---|
| Partida normal | 50 fps, 39-78 % | 49,5 fps, 37 % de pico | **50,0 fps, 15 % de pico** |
| Arena a tope | 25 fps, 190 % | 43,1 fps, 117 % de pico | **48,4 fps, 59 % media, 71 % pico** |
| Frames por encima del presupuesto | — | 6 % | **0 de 484** |

El perfilador no ve **ni un frame por encima del 100 %** en el caso peor. Lo que falta de 48,4 a 50 son unos ocho frames de cada 500 y no es carga sostenida: son sucesos sueltos (el nacimiento de un gusano rellena 128 posiciones de historial de golpe, la generación de paquetes escribe tiles al plano). Si quieres que persiga también eso, se ataca amortizando esos dos picos en varios frames.

VRAM: 1174 tiles de los 1440 disponibles. Todo en `docs/PORT_AUDIT_GML.md` con 8 filas de prueba nuevas, `make` limpio y ROM verificada en el emulador.


## 29. Tú · 2026-11-08

> fíjate la orientación, como empieza el original. El número de agujeros... no coincide

You've hit your 5-hour limit · resets 3:40pm (Europe/Madrid)


## 30. Tú · 2026-11-08

> fíjate la orientación, como empieza el original. El número de agujeros... no coincide

Déjame comparar contra una reconstrucción del arranque real del original, en vez de adivinar:

Voy a reconstruir el primer frame real del original para comparar en vez de adivinar:

**Ahí está.** En el original los agujeros son **grises** (`c_gray`), no negros. Compruebo el arte:

**Esto es.** Los paquetes de munición son **cian** en el original (`sprPaquete` es 0,255,255) y los power-ups azules y blancos — yo los pintaba en gris oscuro con la paleta de los agujeros, así que **parecen agujeros**. Eso es lo que no cuadra en el número.

Dos arreglos reales. Primero la paleta del plano: los agujeros con su borde suave y los objetos del suelo con su color de verdad.

Tenías razón en las dos. Ninguna era lo que parecía.

## El arranque

`Game_init()` terminaba con `g_state = GAME_PLAY; Game_start()`: la máquina encendía **con una partida ya en marcha**. El original no hace eso. El room `_1` trae **ya colocados** un `oMenu` y un `oSplashS`, y `global.started` es `false`: lo primero que se ve es la **pantalla de título sobre los cinco agujeros**, con el contador de créditos, esperando a que metas uno.

Arreglado. Secuencia comprobada con el mando simulado: **título → partida → interludio → partida**.

Lo que no se porta de ese arranque: `oSplashS` dibuja `sprite176` a escala 18 encogiendo y girando 2° por frame durante ~15 frames. El VDP no escala ni rota planos, así que el logo va fijo. Anotado.

## Los agujeros: eran los paquetes

Aquí estaba el número. En pantalla había cinco agujeros **y hasta ocho manchas oscuras más, idénticas**. No eran agujeros: eran los paquetes de munición y los power-ups.

Todo lo que espera en el suelo va al plano A con la paleta PAL2, y esa paleta tenía **cuatro grises oscuros**, heredados de cuando sólo llevaba los agujeros. Leyendo el arte del original:

| Objeto | Color real | Se pintaba |
|---|---|---|
| `oPaquete` | **cian** (0,255,255) | gris oscuro |
| `oNaveReplicaPowUp` | **azul** (0,0,255) | gris oscuro |
| `oRayosPowUp` | **azul y amarillo** | gris oscuro |
| `oBoomPowUp` | casi blanco (232) | gris oscuro |
| Barra del interludio | `c_gray` (128,128,128) | gris casi negro |

La paleta del plano pasa de 4 a 14 colores. Ahora los paquetes son puntos cian que se distinguen de un vistazo, y contando manchas conexas en la captura salen **exactamente 5 agujeros**, los cinco `oAgujerSuper` del room.

## Y de paso, el borde del agujero

El agujero se indexaba binarizando el arte, lo que deja un disco negro recortado. En el original `sprAgujero` es casi negro **con alfa propia de 0,6 de media**, y `c_gray` en GameMaker **multiplica**, no sustituye (mi primera reconstrucción con `-colorize` me engañó y los pintó grises). Compuesto sobre el fondo del room, el agujero recupera el degradado del borde: 4 tonos en vez de 1.

Las posiciones sí estaban bien. El origen de `sprAgujero` es (99,85) de 197×197, así que el dibujo cae muy por debajo del origen — por eso el disco se ve ~11 px más abajo que el punto donde nacen los enemigos. Es así en el original.

## El recorte

`MAX_CABEZA_1` de 3 a 2: **3 gusanos** a la vez. Es el segundo recorte de ese valor y el primero por CPU en vez de por sprites.

## Rendimiento, sin cambios tras todo el arte

| | Ahora |
|---|---|
| Partida normal | **50,0 fps**, 14 % de pico |
| Arena a tope | **48,4 fps**, 60 % de media, 75 % de pico |
| Frames por encima del presupuesto | **0 de 484** |

Todo en `docs/PORT_AUDIT_GML.md` con 7 filas de prueba nuevas. `make` limpio, `check_codegen.py` limpio y el flujo completo verificado en el emulador.


## 31. Tú · 2026-11-08

> ## Eres un comprobador de ports de videojuegos. Especialista ingeniero en shipping y publicacion de juegos
> 
> ## objetivo
> 
> Surfize tiene que tener la maxima calidad y ser exactamente igual que si original. Un port con calidad superior.
> 
> Analiza todo lo desarrollado e implementa lo que el anterior agente no ha llevado a cabo

Me pongo en modo verificación. Primero el estado real del ROM como producto, y luego una pasada de comportamiento contra el GML buscando defectos, no huecos conocidos.

Tengo una lista de defectos concretos. Empiezo por los de corrección:

El paquete sobre el agujero recorta un rectángulo del fondo. Lo arreglo pasando los objetos del suelo a sprites, que además me da el latido del original gratis:

El aviso es un falso positivo: el patrón cae dentro de los datos de tiles, no de código. Afino el comprobador para que no se pueda ignorar por costumbre:

El paquete ya no recorta el agujero, pero sale pálido: la hoja se indexó contra la paleta de sprites, que no tenía cian. Lo añado:

Ahora los tres detalles de fidelidad que quedan. Primero el parpadeo de los satélites y el marco de los iconos del menú:

Y el último: el rayo del original lanza relámpagos del jugador a los cinco agujeros durante su mecha. Ahora mismo no se ve nada.

Todo verificado. Documento y cierro:

**Un fallo que acabo de introducir yo:** al pasar el suelo a sprites cambiaron los índices de tiles del plano y el reparto estaba a mano, así que el logo del menú pisaba los tiles del jugador. Lo hago imposible de repetir:

Pasada de verificación terminada. Encontré **siete defectos**, uno de ellos introducido por mí en esta misma sesión.

## El cartucho se llamaba SAMPLE PROGRAM

`src/rom_header.c` era la plantilla de SGDK sin tocar. SGDK sólo la copia si el fichero no existe, así que se podía editar — pero nadie lo había hecho. El ROM declaraba título **"SAMPLE PROGRAM"**, `rom_end` de 1 MB para un ROM de 896 KB, **SRAM que no se usa** (algunos flashcarts reservan un fichero de guardado fantasma) y `IOSupport` "JD", que no es un código válido.

Corregido: SURFIZE, `rom_end` exacto, sin SRAM (el original tampoco guarda: `scrWrite()` está comentado), mando de 3 botones. Y como `rom_end` se escribe a mano, lo verifica **`tools/check_header.py`** en cada build.

## El desbloqueo de armas iba una kill por delante

```gml
for (g = 0; g < global.maxKills; g += 1)
    if (g >= laserEnabled) { ...añade laser... }
```

`g` llega como mucho a `maxKills - 1`, así que `g >= 1` pide **2 kills**, no 1. El port regalaba el láser con la primera muerte. Y `nextWeapon` arranca en **999999** y sólo se toca dentro de ese bucle: con 0 o 1 kills el menú enseña "NEXT 999999". Es así en el original.

Verificado récord por récord en el emulador: 1→1 arma/NEXT 999999, 2→2/NEXT 2, 3→3/NEXT 3, 5→5/NEXT 0.

## Los paquetes recortaban un agujero del fondo

En el plano de tiles **el color 0 no es transparente: pinta el fondo**. Un paquete encima de un agujero le abría un rectángulo limpio de 16×16, y al cogerlo el mordisco se quedaba. Pasados a sprites, donde la transparencia es de verdad — y de paso sale gratis el **latido** del original, que no estaba portado.

## El rayo no se veía hasta que mataba

`oRay/Draw_0` lanza descargas del jugador **a los cinco agujeros** mientras arde la mecha, cambiando de color cada frame. Eran 25 frames de pantalla vacía seguidos de todo muriendo de golpe. Ya está.

## Y tres más

- **Faltaba el marco de los iconos del menú**: `oMenu` dibuja dos sprites por arma, el icono y `sprFrame` encima. Ahora se componen en el mismo bloque de tiles.
- **Los satélites del PIVOT parpadean** (`alpha = random(2)`). Sin alfa, se enseñan o esconden a cara o cruz.
- **El texto del título estaba inventado.** El del original es `" PRESS SPACE TO START ... SHOOT ALL THE BUGS AND AND SNAKES ... GAME BY REDEFINETECLAS 2012 "`. Ahora es literal, cambiando sólo SPACE y CURSORS. El "AND AND" se queda: es del original.

## El que me cargué yo

Al pasar el suelo a sprites y crecer los iconos del menú, **la nave desapareció**. Cada módulo tenía su índice base de tiles escrito a mano y `VSPR_TILE_BASE` era la suma copiada a ojo: el logo del menú se cargaba encima de los tiles del jugador. El sprite seguía en la tabla del VDP, apuntando al dibujo de otro. **Fallo mudo: no se cuelga, no avisa, sólo deja de verse una cosa.**

Arreglado en la raíz: los tiles se reparten **en ejecución** por orden de carga (`VTile_alloc`). Ya no hay ningún número que sincronizar.

También afiné `check_codegen.py`, que había empezado a dar tres falsos positivos sobre datos de rescomp. Un aviso falso por build es un aviso que nadie vuelve a mirar.

## Estado

| | |
|---|---|
| Partida normal | **50,0 fps**, 18 % de pico |
| Arena a tope | **47,1 fps**, 69,5 % de media, 86 % de pico |
| Frames por encima del presupuesto | **0** |
| Tabla de sprites | 56 de 80 · Tiles 1254 de 1440 |

Tres comprobadores en verde: `check_header`, `check_codegen` (1043 funciones) y el flujo completo título → partida → interludio en el emulador.

**Lo que sigue fuera, a sabiendas:** `oCojeCaja` (es alfa 0,14 — sin transparencia quedaría peor el remedio), el giro de los power-ups, las curvas de `draw_curve` en los enemigos durante el rayo, el `sprite191` animado sobre el arma elegida, y la fuente `sFont2` en el HUD. Todo en `docs/PORT_AUDIT_GML.md` con sus 13 filas de prueba nuevas.


## 32. Tú · 2026-11-08

> por que hay 5 y no 6 agujeros? en el original son 6. También con las armas el juego se rompe, dejan de salir enemigos.

Dos bugs. Voy primero a contar los agujeros en el fichero del room de forma exhaustiva, sin fiarme de mi parseo anterior:

**El orden de habitaciones es `room21`, `_1`, `room22`** — no empieza en `_1`. Miro las otras dos:

El port de Unity existe. Miro cuántos agujeros tiene su escena, por si la referencia del usuario es esa:

El GML tiene **5**, verificado de tres formas (grep crudo, parseo de capas e `instanceCreationOrder`). Pero mira la geometría — hay un hueco:

La geometría es concluyente: cuatro huecos de ~58° y **uno de 129°**. Falta un agujero abajo a la izquierda. Tenías razón. Miro las fórmulas que dependen del número:

Dos arreglos. Primero el que rompe la partida:

Seis agujeros en anillo. Ahora verifico el arreglo del PIVOT, que es el que rompía la partida:

**Encontrado el segundo soft-lock, y es distinto del primero.** Con el PARA: `kills 2 → 2` en 500 frames y cero arañas. El haz del PARA estaba **matando sin puntuar**:

Todas las armas sostienen la partida. Antes de cerrar, una red de seguridad: el pool de sprites estaba justo al límite y desbordarlo escribiría en memoria baja.

Las dos cosas eran ciertas, y la segunda escondía **dos** fallos distintos.

## El sexto agujero

El room trae **cinco** `oAgujerSuper` — lo verifiqué por tres vías (grep crudo, parseo de capas, `instanceCreationOrder`). Pero puestos como anillo alrededor del centro, la geometría canta:

```
 24.8 ->  83.7 :  58.8 grados
 83.7 -> 212.5 : 128.8 grados   <-- falta uno
212.5 -> 273.5 :  61.0 grados
273.5 -> 328.6 :  55.1 grados
328.6 ->  24.8 :  56.3 grados
```

Cuatro separaciones de ~58° y **una de 129**. El que falta cae a 148° y radio 288 → **(558, 754)**, que es el espejo exacto de (558, 446). Esa instancia no está en el room que tengo —se borraría, o la versión publicada es otra—, pero el diseño pide seis.

Añadido y **marcado como la única coordenada del port que no sale del GML**. Va primero en la tabla a propósito: el único comportamiento determinista que depende del orden es dónde nacen las arañas (el original coge el último), así que ese no cambia. Y las tres fórmulas que dependían del número ahora derivan de `HOLE_COUNT`.

## El PIVOT dejaba al jugador desarmado

En `oPlayer/Step_0` el `switch` puede no crear nada (el PIVOT no hace nada si el pivote ya existe), pero **la munición y la cadencia se gastan igual porque están fuera del switch**. Yo lo tenía condicionado a que el arma creara algo.

Consecuencia: con el PIVOT no se gastaba munición nunca → nunca llegaba a cero → **nunca entraba la bala por defecto**. En cuanto los tres satélites morían al primer contacto, el jugador se quedaba sin ningún disparo. No podía matar, no moría ninguna cabeza, y como las oleadas sólo salen de `checkMorte()`, se acabaron los enemigos.

## El haz del PARA mataba en silencio

Este era el peor. Medido: **500 frames con el PARA, +0 kills y cero arañas**.

`object1/Collision_oMalo` hace `with other if hp != undefined hp -= 1` y se destruye. **No pasa por `checkMorte()`**, y el manejador del enemigo tampoco corre porque `object1` no desciende de `oBulletS`. El haz *ablanda*, no mata.

Yo usaba `Enemy_hit()`, que resta **y comprueba la muerte y destruye**. Como la araña tiene `hp = 0`, el haz la borraba de un roce — sin sumar kill y sin generar oleada. Con el PARA puesto, el haz iba vaciando la arena en silencio.

Arreglado con `Enemy_damageNoKill()` / `Snake_damageNoKill()`.

## Comprobado arma por arma

Arena llena, munición la de un paquete, 500 frames de juego sostenido:

| Arma | kills | arañas vivas |
|---|---|---|
| WBLOCK | +19 | 1 |
| LASER | +17 | 6 |
| 3WAY | +34 | 2 |
| PIVOT | +13 | 2 |
| PARA | +12 | 3 |
| RAY | +7 | 2 |

Ninguna se para. Antes, PIVOT y PARA daban +0.

## Y una red

`Spr_alloc()` devolvía `NULL` al agotarse el pool, y los módulos guardan ese puntero y escriben sin mirarlo — un `NULL` ahí escribe en la memoria baja del 68000. Ahora devuelve una ranura sumidero que no se dibuja. En una build publicada esa diferencia importa.

## Coste

Partida normal sigue en **50,0 fps** con 18 % de pico. El caso peor baja de 47,1 a **46,1 fps** (74 % de media) por el sexto agujero: más puntos de aparición reparten más los enemigos y hay una descarga más del rayo. Tres frames por encima del presupuesto de 460 medidos.

Si quieres recuperar ese margen, la palanca medida sigue siendo el montador de la tabla de sprites en ensamblador (~5 %). Dímelo y lo hago.


## 33. Tú · 2026-11-08

> game by screen implosion 2012. corrígelo. 
> hay una explosión que dispara debrís en 360 grados. randomiza un poco la explosión y hazla más frecuente

Dos cosas. La atribución primero:

Te lo dejo como interruptor de build para que puedas alternar sin tocar nada:

Fondo magenta listo. Te monto una escena con todo a la vez para que puedas identificar cada cosa:

Ahí se ve: la **nave blanca** al centro, las **manchas verdosas son la humareda** (su color va mezclado con el fondo real, por eso desentona sobre magenta), y todo lo **negro** son arañas, eslabones de gusano y metralla — que en el original comparten silueta negra.

Ahora la metralla:

Normal va a 50 fps con 29 % de pico. El caso peor sí nota la metralla extra, así que la pago con una colisión de punto en vez de caja:

Las tres cosas hechas.

## La firma

**SCREEN IMPLOSION 2012**, en los dos sitios: el texto del splash y la cabecera del cartucho (`(C)SCIM 2026.NOV` y "SCREEN IMPLOSION 2012 - MEGA DRIVE PORT" en las notas). Comprobado con `strings` sobre el ROM: no queda ni un "REDEFINETECLAS". Es la única cadena que se aparta a propósito del literal del GML.

## La metralla

Resulta que **la versión buena estaba comentada en el propio GML**, al lado de la que quedó:

```gml
//optimize
// for(i=0;i<360;i+=random(60)){
for(i=0;i<360;i+=30){
    b.speed = 10//random_range(4,64)
```

Paso angular y velocidad aleatorios, apagados en su día por rendimiento. Recuperados los dos:

- **paso** `random(60)`, acotado por abajo a 15° — con el random puro el paso puede salir casi cero y pediría cientos de astillas, que es justo por lo que lo comentaron. Salen de 8 a 24 por ráfaga, 12 de media (lo mismo que daba el paso fijo de 30)
- **velocidad por astilla** `random_range(4,64)`: de 0,96 a 15,4 px/frame, unas se quedan dando vueltas y otras cruzan la pantalla
- **ángulo de arranque al azar**, para que el abanico no caiga siempre igual

Y más frecuente: la ráfaga sale ahora **en toda muerte causada por un disparo**, no sólo cuando un WBLOCK o un satélite daban en un eslabón. Eso sí es desviarse del GML, a petición tuya. No se realimenta desde las muertes que causa la propia metralla.

Costó 2 fps en el caso peor, así que me los cobré donde dolía: con 32 puntos preguntando por 8 arañas y 3 gusanos cada frame, el solape caja-contra-caja era lo más caro del juego. Ahora las astillas usan colisión de **punto** con el rechazo en X en una sola comparación sin signo.

| | Antes | Metralla nueva | Con punto |
|---|---|---|---|
| Normal | 50,0 fps, 18 % | 50,0 fps, 29 % | **50,0 fps, 29 %** |
| Caso peor | 46,1 fps | 43,9 fps | **45,0 fps** |
| Frames por encima | 3/460 | 17/440 | **7/450** |

## El fondo

```
make EXTRA_FLAGS="... -DPORT_DEBUG_BG=1"   magenta
                       -DPORT_DEBUG_BG=2   negro
                       -DPORT_DEBUG_BG=3   blanco
```

Fuera de la ROM que se distribuye. En la captura sobre magenta: la **nave blanca** al centro, los **puntos cian** son paquetes de munición, las **manchas verdosas** la humareda, y todo lo **negro** son arañas, eslabones de gusano y metralla — que en el original comparten silueta negra, así que ahí no hay nada que distinguir.

Un aviso al usarlo: los efectos translúcidos (humareda, metralla, haz del PARA, agujeros) llevan el color **ya mezclado con el fondo real**, porque el VDP no mezcla y el campo es un color plano. Sobre magenta se ven verdosos. No es un fallo, es la técnica — pero no juzgues esos tonos con el fondo de depuración puesto.


## 34. Tú · 2026-11-08

> [Image #3] [Image #4] [Image #5] fijate el aspecto gráfico,elementos etc sigue trabajando el port


## 35. Tú · 2026-11-08

> [Image: source: /Users/miguelangelalvarez/Downloads/surfize_2.gif]
> [Image: source: /Users/miguelangelalvarez/Downloads/surfize_3.gif]
> [Image: source: /Users/miguelangelalvarez/Downloads/surfize_1.gif]

You've hit your 5-hour limit · resets 9:10pm (Europe/Madrid)


## 36. Tú · 2026-11-08

> [Image #3] [Image #4] [Image #5] fijate el aspecto gráfico,elementos etc sigue trabajando el port

Estas capturas cambian cosas importantes. Lo primero, el fondo: no es el color plano que yo asumí.

Las capturas no cuadran con el proyecto GameMaker que tengo (ahí el fondo es un teal plano). Voy a los ficheros originales para medir los colores de verdad:

El fondo real es **(52,52,52) gris oscuro**, no el teal del proyecto GameMaker. Las GIFs son de una build distinta a la del `.yyp`. Mido el resto antes de tocar nada:

El HUD del original mide **14 px de alto**, el doble que el mío. Y el fondo es gris oscuro. Empiezo por el fondo, que obliga a recalcular todo el color mezclado:

Tengo la paleta real del juego publicado. La reconstruyo sobre el fondo gris oscuro:

El fondo ya es el del juego publicado. Ahora el HUD, que en el original mide el doble:

El HUD ya es el del original. Ahora los agujeros rojos y las réplicas, que en tu captura hay cuatro y yo tenía tope de dos:

El agujero por el que sale el gusano ya sale con su reborde rojo. Mido y cierro:

Eso ha costado caro: 37,5 fps. Los dos culpables son repintados de plano. Los arreglo:

Las capturas revelaron algo importante: **el `.yyp` no es la versión que se publicó.** No es interpretación mía, está en el histograma.

| | Proyecto GameMaker | Tus capturas |
|---|---|---|
| Fondo | teal (117,154,154) | **gris oscuro (51,51,51)** |
| Agujeros | todos negros; la línea que activa `gus` está comentada | **reborde y goterones rojos** en el que saca gusano |
| Réplicas | — | **cuatro a la vez** |

Mandan las capturas. Lo hecho:

## El fondo, y con él toda la paleta

Como aquí los efectos translúcidos llevan el color **ya mezclado con el fondo**, cambiarlo obliga a rehacer la paleta entera. La saqué del histograma de tus GIFs, cuantizada a los ocho niveles por canal del VDP:

```
(51,51,51)   723003 px  fondo        (0,255,255)  11054  paquetes
(255,255,255) 58912     HUD y nave   (84,82,82)    6674  humareda
(7,7,7)       48991     siluetas     (128,0,0)     3732  agujero activo
```

La textura de grietas no se porta: son 7 niveles de contraste sobre 255, muy por debajo del paso de 36 del VDP.

## El HUD, al tamaño que tiene

Medido en tus capturas: **36 px de alto en 800×600 = 14,4 px en Mega Drive**. Yo lo pintaba a 8, la mitad. Ahora usa los glifos de `sFont2` en bloques de 2×2 tiles — sólo los 19 caracteres que el HUD necesita, porque la fuente entera no cabe en VRAM. LAST se queda pequeño a propósito: el original lo pinta con `myFont2`, que es aún menor.

## Los agujeros rojos

`oAgujerSuper/Draw_0` dibuja una copia desplazada y tintada del agujero por detrás cuando `gus` está activo. En tu `.yyp` la línea que lo activa está comentada; en tus capturas se ve clarísimo. Portado: el agujero con una cabeza encima cambia a una segunda variante de tiles.

## Y el tope de réplicas, que leí mal

El `instance_number(oNaveReplicaPowUp) < 2` de `generaPaquete` es el tope de **power-ups en el suelo**, no de `oTri`. El original no limita las réplicas. Subido a 4, que es lo que se ve en tu captura.

## El coste, y de dónde lo recuperé

Todo eso dejó el caso peor en **37,5 fps**. Y no era por lo que parecía: eran **dos repintados del plano**, que son accesos al puerto del VDP uno por tile.

- El HUD borraba las dos filas enteras y repintaba los 16 glifos **cada disparo**. Ahora son campos de ancho fijo y las palabras se pintan una sola vez: sólo se tocan las cuatro cifras.
- El agujero rojo repintaba sus 64 tiles casi cada frame, porque las cabezas van justo hacia los agujeros y el estado oscilaba en el borde. Rebote de 6 frames.

**37,5 → 41,0 fps.**

| | Antes | Ahora |
|---|---|---|
| Partida normal | 50,0 fps, 29 % | **50,0 fps, 39 %** |
| Caso peor | 45,0 fps | **41,0 fps** |
| VRAM | 1254 tiles | 1394 de 1440 |

El caso peor paga 4 fps por el HUD grande, el sexto agujero, su variante roja, las cuatro réplicas y la metralla nueva. La partida normal ni se entera.

**Queda poco margen de VRAM** (46 tiles). Si quieres más arte nuevo habrá que liberar: la hoja de la araña se lleva 64 tilesets ella sola.

Y la palanca de rendimiento sigue siendo la misma y sigue medida: **el montador de la tabla de sprites está en el 14,7 % del frame** y escrito a mano en ensamblador cabría en la mitad. Dímelo y lo hago.


## 37. Tú · 2026-11-08

> hazlo

Antes de escribir ensamblador, miro qué está generando GCC ahora mismo: puede que ya esté cerca de lo que yo escribiría a mano.

Lo mido primero: GCC gasta ~192 ciclos por sprite visible. A mano, reordenando el struct para que sus 8 primeros bytes **sean** la entrada del VDP y copiándola con `movem.l`, bajan a ~140. Vamos.

Te lo preparo. Extraigo la conversación real del transcript, sin el ruido de las llamadas a herramientas:

