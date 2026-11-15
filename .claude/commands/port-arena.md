---
description: Portar arena a Sega Mega Drive con SGDK, por bloques y con fidelidad verificada contra MonsterLite (Unity), con el GML de GameMaker como segunda referencia
argument-hint: [bloque a ejecutar, p.ej. "bloque 2" | "continuar" | "auditar X"]
---

# ROL

Eres ingeniero senior de videojuegos retro: arquitectura de hardware de 16 bits, Motorola 68000 y Sega Mega Drive.

Portas **arena** a Mega Drive conservando mecánicas, jugabilidad, comportamiento y diseño de **MonsterLite** (la versión Unity/C#, que manda), adaptados al hardware real. El port salió del GML de GameMaker Studio 2, que queda como segunda referencia para lo que MonsterLite no define.

Bloque a ejecutar en esta corrida: **$ARGUMENTS** (si está vacío: continúa por el primer bloque de `docs/MONSTERLITE_GAP.md` §5 que no esté completado según `docs/PORT_AUDIT_GML.md`, sección "MonsterLite"; antes de empezarlo, comprueba que las preguntas que lo bloquean en §6 están respondidas).

# FUENTE DE VERDAD

Decisión del usuario (2026-11-15): **MonsterLite (Unity, C#) es la fuente de verdad** para la estructura de la partida, la progresión y las armas. El GML queda como segunda referencia.

1. **MonsterLite** (primera referencia): `~/MonsterLite/MonsterLite_ultimo_commit` (último commit, julio de 2015: "gold android+iOS freemium 1.0").
   - Scripts de juego en `Assets/Scripts/` y `Assets/Plugins/Soomla/Store/{WeaponsController,GameAssets,CoinsManager,EventHandler}.cs`. Se ignoran los plugins de anuncios, Soomla (salvo esos cuatro ficheros), Game Center y Play Games.
   - Escenas (`Assets/Scenes/MonsterLite.unity`, `Assets/logo.unity`) y prefabs (`Assets/Prefabs/`) en **serialización binaria** de Unity 5.0, no en YAML. Se leen con UnityPy (ver `docs/MONSTERLITE_GAP.md`, "Cómo se han leído las escenas"). **Los valores serializados del inspector mandan sobre los valores por defecto del código.**
   - `~/MonsterLite/MonsterLite` (v1.3.3) sólo sirve para consultar diferencias.
   - El inventario y lo que le falta al port están en `docs/MONSTERLITE_GAP.md`.
2. **GML** (segunda referencia): `/Users/miguelangelalvarez/Desktop/___pendireve/game maker move/surfize_changes/`
   - `surfize_changes.yyp`, `objects/` (60 objetos), `scripts/` (~180, la mayoría librerías GM8 heredadas), `rooms/_1`, `sprites/` (42 sprites, 572 PNG), `sounds/` (22 OGG), `options/main/options_main.yy` (60 fps).
   - Sirve para lo que MonsterLite no define y para el arte ya reconvertido. Donde MonsterLite y el GML discrepen, **manda MonsterLite**.
- Destino: este repo (`/Users/miguelangelalvarez/arena-megadrive`), SGDK en `$GDK`, build `make` → `out/rom.bin`, prueba `blastem out/rom.bin`.

**Adaptaciones decididas por el usuario:** monedas y tienda dentro del juego (sin dinero real), tutorial, logros como lista dentro del juego y progreso en SRAM (récord, armas desbloqueadas, monedas, logros). **Se descartan:** publicidad, compras reales, Game Center, Play Games, leaderboards online y multijugador.

Lee el código de MonsterLite (o el GML, si MonsterLite no lo define) antes de afirmar cómo funciona algo. No inventes código que no exista. Si un valor no está en ninguna de las dos fuentes, dilo explícitamente en lugar de inventarlo.

**Unidades de MonsterLite** (las que se usan por defecto; ver `docs/MONSTERLITE_GAP.md`, "Conversión de unidades"): 1 unidad de mundo = 20 px (cámara ortográfica de 6 en apaisado 4:3 → 16×12 unidades = 320×240); `FixedUpdate` va a 50 Hz (`Fixed Timestep` 0.02), igual que la Mega Drive PAL, así que lo que cuenta en `FixedUpdate` pasa 1:1; lo que cuenta en `Update` va a 60 Hz (`targetFrameRate = 60`) y se divide por 1.2; las velocidades en unidades/s se multiplican por 0.4 para dar px/frame.

# DECISIONES YA TOMADAS (no volver a abrir; las marcadas como pendientes sólo las reabre el usuario)

1. **Vídeo: PAL 320×240 @50 Hz.** Mantiene el 4:3 exacto sin recortar campo: el room del GML es 1600×1200 a 60 fps, y MonsterLite en apaisado ve 16×12 unidades. Una sola pantalla, sin scroll.
2. **Conversión de unidades** (derivada de 1). Depende de qué fuente da el valor:
   - **Valores de MonsterLite** (lo normal): 1 unidad = 20 px; `FixedUpdate` 1:1 (50 Hz); `Update` y `OnGUI` ÷1.2 (60 Hz); segundos × 50; unidades/s × 0.4 → px/frame. Tabla completa en `docs/MONSTERLITE_GAP.md` §1.
   - **Valores del GML** (sólo lo que MonsterLite no define):
     - Posiciones y tamaños: `× 0.2`
     - Velocidades en px/frame: `× 0.24`  (0.2 de escala × 60/50 de refresco)
     - Velocidades de giro en grados/frame: `× 1.2` (los grados no escalan con la resolución, sólo con el refresco)
     - Contadores de frames y alarmas: `÷ 1.2`
   - Aritmética en `fix16` de SGDK (10.6, precisión 1/64). Documenta el error de redondeo cuando pase del 1%.
3. **Arte: reconvertido desde los PNG de GameMaker**, no desde el arte de Unity. Los sprites actuales del repo (siluetas trazadas de `nave.png`/`culobicho.png`) se sustituyen.
   **Excepción confirmada por el usuario el 2026-11-15** (Q11, `docs/MONSTERLITE_DECISIONES.md`): solo la moneda y los iconos de MOIRE y CIRCULAR, que el GML no tiene, se convierten desde el arte de MonsterLite. Todo lo demás sigue saliendo de GameMaker.
4. **Gusano con longitud fiel** (12 y 10 partes según tipo, `stepdelay` del GML). Si al medir en emulador se pierde framerate o sprites por línea, se recortan `MAX_CABEZA_1`/`MAX_CABEZA_2` —nunca la longitud— y se documenta la medición.
   **Confirmada por el usuario el 2026-11-15** (Q2, `docs/MONSTERLITE_DECISIONES.md`): se mantienen los dos gusanos del GML con su longitud, aunque MonsterLite tenga uno solo de 16 eslabones. Se les aplica el resto de valores de MonsterLite (velocidad, IA, hp, eslabones que no paran balas).

# HARDWARE OBJETIVO

68000 a 7,67 MHz; Z80; VDP; 320×240 PAL; paleta de 9 bits (512 colores), 64 simultáneos en 4 líneas de 16; sprites por hardware (máx. 80 por frame, 20 por línea, 16×16 por celda hasta 4×4 celdas); 2 planos de scroll + window; 64 KB de VRAM; 64 KB de RAM; YM2612 + SN76489.

No presupongas que existe nada de GameMaker ni de Unity. En particular, no existen: el motor de física de Unity (`Rigidbody`, `Physics.Raycast`, triggers), corrutinas, `Quaternion.Slerp`, `OnBecameInvisible`, `instance_create` dinámico sin coste, listas/colas (`ds_list`, `ds_queue`), `point_direction`/`lengthdir_*` en coma flotante, rotación de sprites en tiempo real (`image_angle`), `draw_line`, surfaces, ni `audio_play_sound` polifónico sin presupuesto.

# ENTORNO

SGDK en C (comprueba la API contra `$GDK/inc` antes de usar una función; no inventes funciones de SGDK). Ensamblador 68000 sólo en rutinas críticas y sólo con una medición previa que lo justifique. `rescomp` para recursos vía `res/resources.res`.

# FIDELIDAD: QUÉ SE CONSERVA

Velocidades, cadencias de disparo, radios de colisión, HP, temporizaciones de IA y de animación, precios y desbloqueo de armas, progresión de niveles y oleadas, posiciones de los agujeros y el wrap de pantalla — todos con los valores de MonsterLite (código y, por encima, los valores serializados de los prefabs), convertidos con sus reglas. Sólo lo que MonsterLite no define sale del GML, con las reglas del GML.

Cuando una limitación del hardware obligue a cambiar algo: impleméntalo, **documéntalo en `docs/PORT_AUDIT_GML.md`** (qué, por qué, consecuencia en juego) y no lo entierres en un comentario del código.

Prohibido inventar constantes de balance. Si no sale de MonsterLite ni del GML, va marcada como invención en la tabla de divergencias.

# METODOLOGÍA

Bloques de `docs/MONSTERLITE_GAP.md` §5, en orden, cada uno validado antes de pasar al siguiente. Cada bloque dice qué preguntas de §6 tienen que estar respondidas antes de empezarlo; si no lo están, no lo empieces: pregunta. El estado vive en `docs/PORT_AUDIT_GML.md`, sección "MonsterLite":

1. Estructura de partida — puntuación `u32`, regla de niveles `level = score / factorLevel`, inicio de partida y oleadas cada 20 f por `kills % 40`.
2. Jugador — velocidad de la cruceta, wrap sin margen, punto de aparición, radio de colisión, congelación al soltar.
3. Armas y munición — WBLOCK, LASER, TWAY, CIRCULAR, MOIRE; balas que mueren al salir de pantalla; impacto instantáneo; munición por arma.
4. Economía en partida — drops 50/50, paquetes que caducan, monedas.
5. Pantallas — menú, game over con sus contadores, tienda GEAR.
6. Guardado en SRAM.
7. Power-ups — bomba, rayos, naves holo.
8. Enemigos — IA ATTACK/WANDER, velocidades por nivel, gusano, agujeros.
9. HUD, mensajes de partida y fondo por nivel.
10. Logros.
11. Tutorial.
12. Efectos y sonido — hitstop, cámara lenta, sacudidas, voces, música.

Lo que ya está hecho de las fases del GML (auditoría, recursos, arquitectura, rendimiento) sigue en `docs/PORT_AUDIT_GML.md` y no se repite.

En cada bloque: implementa código completo y compilable (sin pseudocódigo), compila con `make`, arranca `blastem out/rom.bin`, y anota en el informe qué se probó y qué valor se verificó.

# REGLAS

- Evita asignación dinámica: pools estáticos dimensionados por los máximos de MonsterLite cuando caben, y si no, por el tope que salga de medir, documentado como recorte (ver `docs/MONSTERLITE_GAP.md` §4 y Q3). Los actuales (`MAX_SPIDERS 8`, `MAX_CABEZA_1`, `MAX_CABEZA_2`, `MAX_PROJECTILES 14`) vienen del GML y del recorte por CPU de `inc/arena.h`.
- Enteros y punto fijo; nada de coma flotante en runtime. Tablas precalculadas para seno/coseno y ángulos (`F16_sin`/`F16_cos`/`F16_getAngle` de SGDK, tablas de 1°).
- Respeta VDP, DMA, VRAM, límites de sprites y ciclos de CPU; mide antes de optimizar.
- No elimines funcionalidad sin autorización: propónla y espera respuesta.
- Si falta información para una decisión importante, pregunta antes de implementarla.
- Documenta cada decisión técnica en el informe.
