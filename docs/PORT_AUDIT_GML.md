# Port arena → Mega Drive: auditoría del original GameMaker

Informe de FASE 1 (auditoría del GML) y FASE 2 (recursos). Fuente: `/Users/miguelangelalvarez/Desktop/___pendireve/game maker move/surfize_changes/` (GameMaker Studio 2, 60 fps, room `_1` de 1600×1200).

Todo valor de este documento está leído del GML. Lo que no sale del GML va marcado como **[INVENTADO]**.

## Estado de fases

| Fase | Estado |
|---|---|
| 1. Auditoría GML | ✅ completa (5155 líneas de GML, 60 objetos, 3 rooms) |
| 2. Recursos | ✅ inventario hecho (42 sprites / 572 PNG, 22 OGG, 1 tileset) |
| 3. Arquitectura MD | ✅ implementada y compilando (ver "Arquitectura") |
| 4. Mecánicas núcleo | ✅ jugador, wrap, apuntado, bala por defecto y colisiones con valores del GML |
| 5. Enemigos y oleadas | ✅ IA de 3 estados, agujeros, araña, los dos gusanos y `generaMalos` |
| 6. Gráficos | ✅ arte reconvertido del proyecto GameMaker, paletas, rotación por frames, agujeros y fondo |
| 7. Armas y power-ups | ✅ las 7 armas, munición, paquetes, los 3 power-ups, desbloqueo y explosiones |
| 8. HUD, menú y estados | ✅ splash, créditos, interludio con selección de arma, HUD |
| 9. Sonido | ✅ 20 de 22 sonidos como PCM por XGM2, música del menú y voces de arma |
| 10. Optimización y pruebas | ⏳ medido en emulador (50 Hz, 39–78 % de CPU); falta hardware real |

Desde el 2026-11-15 el port sigue a MonsterLite por bloques: su estado está en la sección "MonsterLite".

## Decisiones de base

- **PAL 320×240 @50 Hz.** El room es 1600×1200 (4:3) y la cámara abarca el room entero (`camera_set_view_size(view_camera, WIDTH_VIEW, HEIGHT_VIEW)` con `WIDTH_VIEW 1600`/`HEIGHT_VIEW 1200` en `scrInit`): **una sola pantalla, sin scroll**, con wrap toroidal. PAL conserva el 4:3 exacto sin recortar campo de juego.
- **Reglas de conversión:** posiciones y tamaños `×0.2`; velocidades px/frame `×0.24` (0.2 × 60/50); giro en grados/frame `×1.2`; contadores de frames `÷1.2`.
- **Arte desde los PNG de GameMaker**, no desde el arte de Unity que usan los sprites actuales del repo.
- **Longitud de gusano fiel**; si hay que recortar, se recortan las cantidades simultáneas, no la longitud.

## Mecánicas del original

**Bucle.** `oControles` arranca (`scrRead`, `scrInit`, `room_goto(_1)`) y lee input cada step. `oMenu` → `startGame()`: limpia enemigos, centra la cámara, `global.kills = 0`, `global.malosCounter = 0`, crea `oPlayer` en el centro del room y **un** `oCabeza1`. Morir (`mataPlayer`) vuelve al menú.

**Jugador** (`oPlayer`, 231 líneas). Movimiento directo sin inercia: `acc = 6` px/frame sumado por eje según D-Pad (el original lee flechas de teclado o joystick), **sin aceleración ni fricción**. Wrap en los cuatro bordes con margen `offSet = 15` (`stPlayerMovement`). Apunta **automáticamente** al enemigo más cercano (`instance_nearest(x,y,oMalo)` → `direction`) y dispara solo, sin botón. Mientras existe un `oLaser` el jugador queda inmóvil (`laserState`). **El port nunca lo implementó** (`Player_update` se llama siempre, sin mirar si hay láser) y la divergencia no estaba anotada; desde el 2026-11-15 coincide con MonsterLite, que tampoco congela (ver "MonsterLite", bloque 2). El sprite se dibuja rotado de forma continua (`image_angle = direction - 90`).

**Disparo.** Cadencia por contador `controlTime`. Con munición (`global.playerBulletsCounter > 0`) dispara el arma de `global.weapon_index` y gasta 1; sin munición dispara la bala por defecto `oBulletThin`, **infinita**, con `controlTime = 12`. Cada disparo con arma sacude la pantalla (`stShake`).

**Enemigos.** Todos heredan de `oMalo`. Tocar cualquiera mata al jugador (`oMalo/Collision_oPlayer` → `mataPlayer`). Aparecen en uno de los 5 agujeros (`oAgujerSuper`, posiciones fijas en el room), **invisibles e inmóviles durante 90 frames** (`alarm[0]`), y entonces `visible = 1; speed = velo`.

IA de tres estados (`state_machine` + scripts de estado), idéntica para arañas y cabezas de gusano:

| Estado | Duración | Comportamiento |
|---|---|---|
| `stSteadyW` | 6 frames | espera |
| `stAppearA1W` | 300 frames | persigue al jugador: `turn_towards_direction(point_direction(x,y,oPlayer.x,oPlayer.y), velo)` — giro gradual de `velo` grados/frame, avanzando a `speed` constante. Si no hay jugador, persigue un `oTri` |
| `stWalkingA1W` | 600 frames | vaga hacia un agujero al azar con giro de 2 grados/frame; al llegar se teletransporta a él. Vuelve a `stAppearA1W` |

**Araña** (`oSpider`): `velo = 3`, `hp = 0`; las balas restan 4 → **muere de un impacto**. Suma 1 a `global.kills`.

**Gusanos.** Dos tipos, cabeza + cadena de segmentos que siguen el recorrido con retardo (cola de posiciones, `stepdelay` frames):

| | `oCabeza1` | `oCabeza2b` |
|---|---|---|
| `velo` | `random_range(5, 7.8)` | `6.2` |
| `hp` | 4 → **5 impactos** | 2 → **3 impactos** |
| `bodylength` | 14 → 9 `oCorpo` + `oCu1` + `oCu2` + `oCabeza2` + cabeza = **13 partes** | 12 → 7 `oCorpo` + 3 + cabeza = **11 partes** |
| `stepdelay` | 7 frames | 6 frames |

Los segmentos **no son disparables**: `oMalo/Collision_oBulletS` sólo crea una explosión, no resta HP ni destruye la bala; sólo las cabezas y las arañas tienen manejador de daño propio. Pero tocarlos sí mata al jugador. (`oCorpo` tiene `hp = 10`, muerto en la práctica.)

**Oleadas.** No hay spawn por temporizador. Al morir una cabeza, `checkMorte()` dispara `morteGusano()` + `generaPaquete()` + `generaMalos()`. `generaMalos` sube `global.malosCounter` en 0.5 hasta un techo de 4 y crea esa cantidad de gusanos (primero `oCabeza1` hasta `MAX_CABEZA_1 = 4`, luego `oCabeza2b` hasta `MAX_CABEZA_2 = 2`), y además **3 arañas de golpe** si hay menos de `MAX_SPIDERS = 8`.

**Armas** (`global.weapon_index`, switch de `oPlayer/Step_0`):

| idx | Arma | Implementación en GML | Cadencia | Munición por paquete |
|---|---|---|---|---|
| 0 | WBLOCK | `oWBlock`, `vspeed = -32` en `direction` | 6 | +10 |
| 1 | LASER | 10 `oLaser` encadenados cada 32 px, `speed = 40`, vida 8 frames; congela al jugador | 6 | +4 |
| 2 | 3WAY | 3 × `o3way` a `direction - 30`, `direction`, `direction + 30` | **12** | +10 |
| 3 | PIVOT/TRI | crea un `oPivot` único (orbital) | 6 | +5 |
| 4 | PARA | `oBulletPara` único, `vspeed = -64`, + `object0` | 6 | +8 |
| 5 | BOMBA | `oBomba` a 100 px en `direction` | 6 | +1 |
| 6 | RAY | `oRay` a 100 px en `direction` | 6 | +1 |
| — | por defecto | `oBulletThin`, `vspeed = -32`, infinita | 12 | — |

**Desbloqueo por kills** (`oWeaponBox/Create_0` + `stCheckWeapon`, contra `global.maxKills`): LASER a 1, 3WAY a 2, CIRCULAR a 3, TRI a 4. `nextWeapon` se muestra en el menú.

**Power-ups** (`generaPaquete`, llamado en cada muerte de cabeza): `oPaquete` (munición) hasta 16; luego un `oBoomPowUp` (mata todas las arañas en pantalla y explota en cruz); luego hasta 2 `oNaveReplicaPowUp` (crea `oTri`, un satélite al que el jugador queda unido por una línea) mientras haya menos de 18 paquetes; luego 1 `oRayosPowUp` mientras haya menos de 24; si no, otro paquete.

**HUD y marcador** (`oInGameHUD`, `oMenu`): `kills` + "KILLS", `LAST`, `TOP` (`global.maxKills`), `NEXT` (arma siguiente), iconos de armas y créditos. `oPuntoMorte` anima el punto de la muerte hacia el HUD.

**Audio.** 22 sonidos OGG (`shotSnd`, `laserSnd`, `wBlockSnd`, `threeWaySnd`, `circularSnd`, `bombaSnd`, `boomSnd`, `rayosSnd`, `aranamorteSnd`, `morteSnd`, `dustSnd`, `powUpSnd`, `repeteaSnd`, `menuSong`, y voces: `bolasSpeechSnd`, `caralludoSnd`, `fantasmaSnd`, `laserSpeechSnd`, `retroUFOSpeechSnd`, `risaSnd`, `threeWaySpeechSnd`, `triRepeteaSnd`). El sistema FMOD original está comentado; usa `audio_play_sound`.

**Descartado con justificación** (no es jugabilidad, no existe en MD): `oVideoRecording` y `vcapture_*` (captura de vídeo del dev), `oAskScore` + `onscore_*` + `md5*` + sockets TCP/UDP (highscores online: no hay red ni SO), `cleanmem*`, librerías GM8 heredadas (`__view_*`, `__init_*`, `joystick_*` → pad de MD), surfaces (código ya comentado en `oControles`).

## Constantes medidas → valores Mega Drive (PAL 50 Hz)

Velocidades `×0.24`, giro `×1.2`, frames `÷1.2`, posiciones `×0.2`. `fix16` tiene precisión 1/64.

| Concepto | GML | MD | fix16 real | error |
|---|---|---|---|---|
| Jugador `acc` | 6 px/f | 1.44 | 92/64 = 1.4375 | −0.17% |
| Araña `velo` | 3 px/f | 0.72 | 46/64 = 0.71875 | −0.17% |
| Araña giro | 3 °/f | 3.6 | — | — |
| `oCabeza1` `velo` | 5–7.8 px/f | 1.2–1.872 | 77/64–120/64 | +0.26% |
| `oCabeza2b` `velo` | 6.2 px/f | 1.488 | 95/64 = 1.484375 | −0.24% |
| Giro en WANDER | 2 °/f | 2.4 | — | — |
| Bala (`oBulletThin`, `oWBlock`, `o3way`) | 32 px/f | 7.68 | 492/64 = 7.6875 | +0.10% |
| `oBulletPara` | 64 px/f | 15.36 | 983/64 | −0.004% |
| `oLaser` | 40 px/f | 9.6 | 614/64 = 9.59375 | −0.07% |
| Separación de láseres | 32 px | 6.4 px | — | — |
| Spawn de bomba/rayo | 100 px | 20 px | — | — |
| Wrap `offSet` | 15 px | 3 px | — | — |
| Aparición de enemigo | 90 f | 75 f | — | — |
| `stSteadyW` | 6 f | 5 f | — | — |
| `stAppearA1W` (ataque) | 300 f | 250 f | — | — |
| `stWalkingA1W` (vagar) | 600 f | 500 f | — | — |
| Cadencia por defecto / 3WAY | 12 f | 10 f | — | — |
| Cadencia resto de armas | 6 f | 5 f | — | — |
| Vida del láser | 8 f | 7 f (6.67) | — | +5% |
| `stepdelay` gusano 1 / 2 | 7 / 6 f | 6 / 5 f | — | +3% / 0% |
| Agujeros (5) | (820,272) (558,446) (832,888) (1088,424) (1094,736) | (164,54) (112,89) (166,178) (218,85) (219,147) | — | — |
| Inicio del jugador | (800,600) | (160,120) | — | — |
| Topes | `MAX_SPIDERS 8`, `MAX_CABEZA_1 4`, `MAX_CABEZA_2 2`, `malosCounter` +0.5 techo 4 | iguales (ver riesgo de sprites) | — | — |

## Tabla de equivalencias GameMaker → SGDK

| GameMaker / GML | Mega Drive / SGDK |
|---|---|
| `instance_create` / `instance_destroy` | pools estáticos dimensionados por los topes del GML + flag `active`; cero `malloc` |
| `speed` + `direction` | `dx`,`dy` en `fix16`, recalculados sólo al cambiar el ángulo |
| `point_direction` | `F16_getAngle` |
| `lengthdir_x/y` | `F16_mul(F16_cos(a), len)` / `F16_sin` |
| `image_angle` (rotación continua) | el VDP no rota sprites: N frames pre-rotados y elección del más cercano al ángulo real |
| `turn_towards_direction(dir, n)` | ángulo entero en grados, delta acotado a n, wrap 0–359 |
| `instance_nearest(x,y,oMalo)` | barrido del pool con distancia al cuadrado en enteros |
| `Collision_X` / `place_meeting` | AABB explícito entre pools (SPR no detecta colisiones) |
| `alarm[n]` | contador decreciente por entidad |
| `state_machine` + scripts de estado | `enum` de estado + `switch`, con `state_time` por entidad |
| `ds_queue` del historial del gusano | buffer circular estático por gusano (13 × `stepdelay` posiciones) |
| `random_range` | `random()` de SGDK escalado a entero |
| `draw_text` + `font_add_sprite` | `VDP_drawText` con fuente en tiles, o tiles de HUD en plano A |
| `draw_line` (jugador ↔ `oTri`) | sin primitivas: cadena de sprites o tiles alineados — **cambio a documentar en FASE 7** |
| `camera_set_view_pos` (shake) | `VDP_setHorizontalScroll` / `VDP_setVerticalScroll` del plano |
| Wrap de `stPlayerMovement` | comparación contra 0/320 y 0/240 con margen 3 px |
| `audio_play_sound` (22 OGG) | XGM en Z80: música + 4 canales PCM; SFX con prioridad |
| `global.*` | variables globales estáticas (`global.kills`, `malosCounter`, `weapon_index`, `playerBulletsCounter`, `maxKills`, `lastScore`) |
| Fondo `background42_tileset` + `sprAgujero` ×1.3 | plano B de tiles; los 5 agujeros (≈51 px) como tiles del fondo, no como sprites |

## Divergencias del port actual (a corregir en FASES 4–6)

El port existente salió del proyecto **Unity**, con constantes puestas a ojo. Lo que hay que rectificar:

| Ahora | Verdad del GML |
|---|---|
| ~~`SPEED 3` (jugador)~~ | ✅ FASE 4: `PLAYER_ACC` = 1.4375 |
| ~~`ENEMY_SPEED 2`~~ | ✅ FASE 5: 0.7188 px/f + giro 3.59 °/f |
| ~~`SNAKE_SPEED 2`~~ | ✅ FASE 5: 1.203–1.875 (tipo 1), 1.484 (tipo 2) |
| ~~`BULLET_SPEED 6`~~ | ✅ FASE 4: `BULLET_SPEED_THIN` = 7.6875 |
| ~~`SHOOT_INTERVAL 14`~~ | ✅ FASE 4: 10 f para la bala por defecto (5 con arma en FASE 7) |
| ~~`SPAWN_INTERVAL` / `SNAKE_SPAWN_INTERVAL`~~ | ✅ FASE 5: oleadas en `checkMorte` → `generaMalos`, con `malosCounter` |
| ~~`MAX_ENEMIES 6`~~ | ✅ FASE 3/5: pool a 8 y creación de 3 en 3 |
| ~~`MAX_SNAKES 2`, `SNAKE_SEGMENTS 3`, `SNAKE_HEAD_HP 3`~~ | ✅ FASE 5: 13 y 11 partes, hp 4 y 2 (cantidades recortadas a 3+1, ver desviaciones) |
| ~~Enemigos en línea recta~~ | ✅ FASE 5: 3 estados con giro gradual |
| ~~Spawn por el borde~~ | ✅ FASE 5: los 5 agujeros, 75 f invisible |
| ~~320×224 NTSC~~ | ✅ FASE 3: 320×240 PAL |
| Rotación en 8 frames | ✅ FASE 6: 8 rotaciones generadas por sprite, con el rumbo natural de cada arte corregido |
| ~~Sprites trazados del arte Unity~~ | ✅ FASE 6: reconvertidos de los PNG de GameMaker con `tools/gm2md.py` |
| Sin armas, power-ups, HUD, menú ni sonido | 7 armas, 4 power-ups, HUD, menú, 22 sonidos |
| ~~`MAX_BULLETS 8`~~ | ✅ FASE 3/5: cota derivada; 8 mientras sólo exista la bala por defecto |

Añadido en FASE 4: el jugador dispara aunque no haya enemigos, y las posiciones son el origen del sprite con cajas de colisión sacadas de las máscaras.

Coincide ya con el GML: muerte de la araña de un impacto, segmentos del gusano no disparables pero letales al contacto, wrap de pantalla, disparo automático al más cercano, bala por defecto infinita.

## Recursos (FASE 2)

| Original | Tamaño | ×0.2 | Plan MD |
|---|---|---|---|
| `sprite173` (nave) | 38×58, **1 frame** | 7.6×11.6 | celda 24×24 (la rotación alrededor del origen barre 8.4 px), 8 rotaciones |
| `sprite185` (araña) | 64×64, **8 frames** | 12.8 | celda 24×24, 8 animaciones × 8 rotaciones |
| `spr_rtype_outslay_head` | 64×64 | 12.8 | 16×16 |
| `spr_rtype_outslay_body` (`oCorpo`) | 51×64 | 10.2×12.8 | 16×16 |
| `spr_rtype_outslay_tail` (`oCu1`/`oCabeza2`) | 64×64 | 12.8 | 16×16 |
| `sprAgujero` (×1.3 en pantalla) | 197×197 | ≈51 | tiles del plano B |
| `sBulletThin` | 20×28 | 4×5.6 | 8×8 |
| `sprExplosion`, `sprExplosion_2` | 32×32 | 6.4 | 8×8 |
| `sprDebri` | 8×8 | 1.6 | 8×8 (mínimo del hardware) |
| `sprWeapon1..7` (iconos HUD) | 110×110 / 64×64 | 22 / 12.8 | 16×16 o 24×24 en el HUD |
| `sFont2` / `sFont3` | 80×80 / 20×20 | — | fuente en tiles |
| 22 OGG | — | — | XGM / PCM |

**Riesgo de sprites por hardware (a medir en FASE 10).** En el peor caso del GML: 4 × 13 + 2 × 11 = 74 partes de gusano + 8 arañas + jugador + balas ≈ **91 sprites**, por encima del límite de **80 por frame** del VDP (y del de 20 por línea, que se rompe antes si varios gusanos cruzan la misma franja). Según la decisión tomada, la longitud del gusano no se toca: se recortarán `MAX_CABEZA_1`/`MAX_CABEZA_2` (p. ej. 3 + 1 → 50 partes, ≈67 sprites totales) sólo con la medición delante, y se anotará aquí.

## Arquitectura Mega Drive (FASE 3)

### Capa de conversión

`inc/arena.h` es la única fuente de las constantes portadas. Cada valor del GML entra por una macro que aplica la regla de conversión en tiempo de compilación, así que en ejecución no hay ni un flotante:

```c
#define GML_POS(v)     ((s16)(((v) * GML_SCALE) + 0.5))        // x0.2
#define GML_SPEED(v)   FIX16R((v) * GML_SCALE * GML_RATE)      // x0.24
#define GML_TURN(v)    FIX16R((v) * GML_RATE)                  // x1.2, grados/f
#define GML_FRAMES(v)  ((u16)(((v) / GML_RATE) + 0.5))         // /1.2
```

`FIX16R` redondea al 1/64 más cercano en vez de truncar como el `FIX16()` de SGDK: truncar desviaba las velocidades lentas más de lo admisible (1.872 px/f pasaba a 1.859 en vez de 1.875).

Así, `#define SPIDER_SPEED GML_SPEED(3)` deja en el código el 3 del GML a la vista y el 0.72 lo calcula el compilador: no hay constantes de balance escritas a mano.

### Módulos

| Módulo | Porta | Estado |
|---|---|---|
| `arena.h` | `scrInit` + constantes de todos los objetos | ✅ |
| `entity.[ch]` | modelo de instancia: `x`/`y`/`speed`/`direction`, `alarm[0]`, `state_machine`, `turn_towards_direction`, `point_direction`, wrap de `stPlayerMovement` | ✅ |
| `collision.[ch]` | `Collision_oX` / `place_meeting` como AABB entre pools | ✅ |
| `hole.[ch]` | los 5 `oAgujerSuper` del room y su elección al azar | ✅ |
| `game.[ch]` | globales de `scrInit`, `startGame()`, `mataPlayer()`, bucle de partida | ✅ |
| `hud.[ch]` | `oInGameHUD/Draw_0`: BULLETS, KILLS, LAST | ✅ |
| `player.[ch]`, `enemy.[ch]`, `snake.[ch]`, `bullet.[ch]` | pools a los topes del GML; **lógica todavía la del port anterior** | ⏳ FASES 4–5 |
| `background.[ch]` | fondo; cubre ya las 30 filas de 240 px | ⏳ FASE 6 |
| `weapons`, `powerup`, `menu`, `audio` | — | ⬜ FASES 7–9 |

Lo marcado `[PREVIO AL PORT]` en el código es lógica heredada del port basado en Unity que sigue viva sólo para que la ROM arranque: velocidad del jugador (`SPEED 3`), los dos temporizadores de spawn inventados, y los 3 segmentos / 3 HP del gusano. Las fases 4 y 5 los sustituyen por las constantes de `arena.h`.

### Presupuesto medido

**RAM: 3168 B de 65536 (4,8%).** Mayores consumidores: `vdpSpriteCache` 768 B (SGDK), `snakes` 540 B, `allocStack` 320 B, `bullets` 224 B, `enemies` 80 B. El historial de posiciones del gusano fiel (13 partes × 6 frames de `stepdelay` = 79 posiciones × 4 B = 316 B por gusano, 1,9 KB los seis) entra sin problema en FASE 5.

**ROM: 58,6 KB** de código y datos, en un cartucho de 128 KB.

**VRAM: 1536 tiles** (los mapas empiezan en 0xC000). Reparto de SGDK 2.12: 16 de sistema, 96 de fuente, 420 del pool de sprites (`SPR_init` por defecto), ≈1004 para tiles de fondo. Uso actual de sprites: 272 tiles de 420 (jugador 16 + 8 arañas × 9 + 6 cabezas × 16 + 18 segmentos × 4 + 16 balas × 1). Con los tamaños fieles de FASE 6 (todo a 16×16 o menos) baja a menos de 150.

**Sprites de hardware: 49 de 80** con los pools actuales. El peor caso fiel de FASE 5 (74 partes de gusano + 8 arañas + jugador + 16 balas ≈ 99) **excede el límite de 80 por frame**, y el de 20 por línea se rompe antes: es la medición pendiente de FASE 10, y la decisión tomada es recortar `MAX_CABEZA_1`/`MAX_CABEZA_2`, nunca la longitud del gusano.

### Pipeline de recursos

`res/resources.res` → `rescomp` → `out/release/res/resources.o`, vía `make`. En FASE 6 los PNG de GameMaker (`sprites/<nombre>/<uuid>.png`, un fichero por frame) se convierten con un script de reducción a `res/sprite/` antes de pasar por `rescomp`; los agujeros y el fondo van como tileset del plano B, no como sprites.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| El campo de juego mide 320×240 en lógica siempre, pero en consola NTSC el VDP sólo muestra 224 líneas (`VDP_setScreenHeight240` "only work on PAL system") | La decisión de base es PAL; mantener la geometría constante conserva las distancias y las posiciones de los agujeros del original | En NTSC no se ve la franja inferior de 16 px (los 5 agujeros siguen visibles: el más bajo está en y=178) y el juego corre un 20% más rápido, porque las constantes están calculadas para 50 Hz |
| Ángulos en la convención de SGDK (horaria, Y hacia abajo) en vez de la de GameMaker (antihoraria, Y hacia arriba) | Permite usar las tablas `F16_sin`/`F16_cos`/`F16_getAngle` sin conversión por frame | Ninguna: las dos convenciones son espejo (`a_SGDK = 360 − dir_GML`), así que trayectorias y giros son idénticos; sólo cambia el número del ángulo, que nunca se muestra |
| El rumbo efectivo queda cuantizado a 1 grado | `F16_sin`/`F16_cos` de SGDK resuelven con tabla de 1 grado, aunque el ángulo se acumula en 1/64 de grado | Desviación máxima de medio grado frente al flotante del original; el giro gradual sí conserva su velocidad fraccionaria (3.6, 2.4, 6–9.36 grados/frame) |
| `MAX_BULLETS 16` donde el GML no tiene tope | El hardware necesita un pool estático | Cota **derivada**, no inventada: la bala por defecto vive 42 frames y sale cada 10 (5 en vuelo), 3WAY mete 3 cada 10 frames (13 en vuelo), LASER son 10 segmentos de 7 frames. Se mide en FASE 10 |

## Mecánicas núcleo (FASE 4)

Portados `oPlayer` (movimiento, wrap, apuntado, cadencia), `oBulletThin` y las cajas de colisión de ambos, con los valores de `arena.h`:

| | Valor compilado |
|---|---|
| `PLAYER_ACC` | 92/64 = 1.4375 px/f (de `acc = 6`) |
| `BULLET_SPEED_THIN` | 492/64 = 7.6875 px/f (de `vspeed = -32`) |
| `SHOOT_INT_DEFAULT` | 10 f (de `controlTime = 12`) |
| `SHOOT_INT_FIRST` | 25 f (de `controlTime = 30` en Create) |
| `PLAYER_WRAP_OFFSET` | 3 px (de `offSet = 15`) |
| Caja del jugador | origen + (−4,−4), 7×11 |
| Caja de la bala | origen + (−2,−3), 4×5 |

### Hallazgos de esta fase

**La posición de una instancia es el origen de su sprite, no su esquina.** Y los orígenes del original no están centrados: la nave (`sprite173`, 38×58) tiene el origen en (19,19), la araña (`sprite185`, 64×64) en (30,40). El wrap, el apuntado, el punto de disparo y las colisiones se miden desde ahí, así que el port pasa a guardar el origen y a restarlo sólo al dibujar. Las cajas de colisión salen de las `bbox` de las máscaras, no del tamaño del sprite.

**El jugador dispara siempre, haya enemigos o no.** En `oPlayer/Step_0` el apuntado está dentro de `if instance_exists(oMalo)`, pero el bloque de disparo va aparte y es incondicional: sin enemigos la nave sigue disparando al último rumbo (y al empezar la partida, al rumbo 0 de GameMaker, que es a la derecha). El port anterior sólo disparaba con objetivo a la vista.

**Diagonales más rápidas, a propósito.** El original suma `acc` a cada eje por separado y no normaliza, así que en diagonal la nave va a 1.41 veces su velocidad. Portado tal cual.

**Daño de la bala: ambigüedad resuelta y anotada.** Hay dos manejadores para el mismo impacto: el del enemigo (`oCabeza1/Collision_oBulletS`: `hp -= 1` + `checkMorte()` + destruye la bala) y el de la bala (`oBulletThin/Collision_oMalo`: `with other hp -= 1` + `instance_destroy()`). Si los dos corrieran, cada bala restaría 2. GameMaker procesa los eventos por orden de instancia y los enemigos existen antes que la bala que los alcanza, así que el del enemigo corre primero y destruye la bala, que ya no llega a ejecutar el suyo: **1 de daño por impacto**, que es lo que da los 5 impactos de `oCabeza1` y los 3 de `oCabeza2b`. Asunción documentada, no verificada en ejecución del original.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Colisión por AABB donde el original usa máscaras precisas (nave `collisionKind` 4, araña 0) o en diamante (gusano 3) | Las máscaras precisas por frame no caben en el presupuesto de CPU del 68000 | El contacto es algo más generoso en las esquinas de la caja; las dimensiones y el centro sí son los del original |
| ~~El sprite del jugador es el de 32×32 de Unity con caja de 7×11~~ | — | ✅ resuelto en FASE 6: el arte es el del original a escala x0.2 |
| ~~Cajas de araña y gusano del port anterior~~ | — | ✅ resuelto en FASE 5 con `SPIDER_BOX_*` y `SNAKE_*_BOX_*` |

## Enemigos y oleadas (FASE 5)

Portados `oSpider`, `oCabeza1`, `oCabeza2b`, los scripts de estado (`stInit`, `stSteadyW`, `stAppearA1W`, `stWalkingA1W`), `oAgujerSuper` y `generaMalos` / `checkMorte`. Araña y cabezas comparten la misma IA porque en el original corren literalmente los mismos scripts.

| | Valor compilado |
|---|---|
| Araña | 0.7188 px/f, giro 3.59 °/f, hp 0 → 1 impacto |
| Gusano tipo 1 | 1.203–1.875 px/f, giro 6.00–9.36 °/f, hp 4 → 5 impactos, 13 partes, `stepdelay` 6 |
| Gusano tipo 2 | 1.484 px/f, giro 7.44 °/f, hp 2 → 3 impactos, 11 partes, `stepdelay` 5 |
| Estados | quieto 5 f, ataque 250 f, vagar 500 f (giro 2.4 °/f) |
| Aparición | 75 f invisible e inmóvil |
| Agujeros | (164,54) (112,89) (166,178) (218,85) (219,147) |
| Historial del gusano | 79 posiciones, 316 B por gusano |

### Hallazgos de esta fase

**La máquina de estados corre durante los 90 frames de aparición.** `Step_1` (Begin Step) se ejecuta aunque la instancia esté invisible; lo único que la frena es `speed = 0` hasta que salta `alarm[0]`. Así que el enemigo emerge ya girado hacia el jugador y con 75 de sus 250 frames de ataque gastados.

**La IA corre antes del jugador.** En el original está en Begin Step (`Step_1`) y la lógica del jugador en Step (`Step_0`), así que los enemigos apuntan a la posición del jugador del frame anterior. El port respeta ese orden.

**`velo` es un solo sorteo que sirve de velocidad y de giro.** `turn_towards_direction(..., velo)` recibe el mismo número que `speed = velo`, así que un gusano rápido gira rápido. El port sortea un índice y lo interpola en las dos escalas; dos sorteos independientes habrían dado gusanos que giran a un ritmo ajeno a su velocidad.

**Los bucles de elección de agujero del original están sesgados, y portarlos fielmente cambia el juego.** Los tres usan `instance_find` con un índice calculado de forma descuidada y lo que vale es el valor de la última iteración:

| Dónde | Expresión efectiva | Resultado |
|---|---|---|
| `oSpider/Create_0` | `instance_find(oAgujerSuper, i)` con `i` acabando en 4 | **todas** las arañas nacen siempre en el agujero 4 |
| `oCabeza1/Create_0` | `trunc(4 - random(4))` | gusanos en los agujeros 0–3, nunca el 4 |
| `stWalkingA1W` | `trunc(4 - random(3))`, **re-sorteado cada frame** | destino de vagar 1, 2 o 3; nunca 0 ni 4 |

Portado tal cual: las arañas salen todas del mismo punto y el rumbo de los que vagan tiembla, porque el destino cambia cada frame. Si prefieres corregirlo, es un cambio de una línea en `hole.c`, pero sería apartarse del original.

**Al tocar cualquier agujero mientras vaga, el enemigo salta al agujero 0.** `if place_meeting(x,y,oAgujerSuper) {x = oAgujerSuper.x; y = oAgujerSuper.y}`: `oAgujerSuper` sin cualificar es la primera instancia, no la que ha tocado.

**La máscara del agujero es diminuta.** `sprAgujero` mide 197×197 y se dibuja a escala 1.3, pero su `bbox` es de 15×12 px del room (3×2 en Mega Drive): el salto sólo ocurre pasando casi por el centro exacto.

**El apuntado del jugador cuenta los eslabones del gusano.** `instance_nearest(x, y, oMalo)` y `oCorpo` desciende de `oMalo`, así que la nave puede quedarse disparando a un eslabón invulnerable mientras la cabeza sigue libre. Portado.

**No hay arañas hasta que muere el primer gusano.** Sólo `generaMalos` las crea, y sólo `checkMorte` (muerte de una cabeza) lo llama. La partida empieza con un único gusano de tipo 1.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Topes de gusanos recortados: 3 de tipo 1 y 1 de tipo 2, frente a 4 y 2 | El pool de objetos Sprite de SGDK es de 80 (`MAX_SPRITE`) y el VDP no muestra más de 80 por frame. Con los topes del original harían falta 103 objetos y `SPR_addSprite` devolvería NULL. Conforme a la decisión tomada se recorta la cantidad, nunca la longitud: jugador 1 + balas 8 + arañas 8 + 4 cabezas + 52 eslabones = 69 asignados, 67 visibles como máximo | Hasta 4 gusanos simultáneos en vez de 6; las oleadas saturan antes su techo de población. El límite de 20 sprites por línea del VDP sigue sin medir: si un gusano cruza una franja a lo ancho puede perder eslabones |
| Pool de balas a 8 (antes 16) | Hacía falta sitio en el pool de 80 objetos, y con sólo la bala por defecto el máximo real en vuelo es 5 | Ninguna ahora; FASE 7 (3WAY, LASER) obligará a recortar en otro sitio |
| Pool de VRAM de sprites subido a 600 tiles con `SPR_initEx` | Los 69 sprites gastan 368 tiles y el pool por defecto son 420: demasiado justo para arriesgar un fallo de asignación por fragmentación. El fondo sólo usa 6 tiles de usuario | Ninguna |
| ~~Todos los eslabones con el mismo sprite~~ | — | ✅ resuelto en FASE 6: `oCabeza2` y `oCu1` con la silueta de cola, `oCu2` con la de cabeza y los `oCorpo` con la de cuerpo |

## Gráficos (FASE 6)

Arte reconvertido desde el proyecto GameMaker con **`tools/gm2md.py`**, que es ya el pipeline de recursos: lee el origen y los frames de cada `.yy`, compone sobre un lienzo cuyo centro es el origen del sprite, rota, escala x0.2 exacto y escribe PNG indexados de 16 colores con paleta fija, más el header `res/gm_palettes.h`. Sólo necesita ImageMagick.

| Recurso | Origen GameMaker | Celda | Frames |
|---|---|---|---|
| `player.png` | `sprite173` 38×58, origen (19,19) | 24×24 | 8 rotaciones |
| `spider.png` | `sprite185` 64×64, origen (30,40) | 24×24 | 8 animaciones × 8 rotaciones |
| `snake_head1.png` | `spr_rtype_outslay_head` 64×64 | 24×24 | 8 rotaciones |
| `snake_head2.png` | `spr_rtype_outslay_head_2` 70×70 | 24×24 | 8 rotaciones |
| `snake_tail.png` | `spr_rtype_outslay_tail` 64×64 | 24×24 | 8 rotaciones |
| `snake_body.png` | `spr_rtype_outslay_body` 51×64 | 16×16 | 1 (en el original no rota) |
| `bullet.png` | `sBulletThin` 20×28 | 8×8 | 8 rotaciones |
| `hole.png` | `sprAgujero` 197×197 a escala 1.3 y tintado `c_gray` | 64×64 | bloque de tiles del plano A |

### Hallazgos de esta fase

**El arte es de siluetas, no de color.** Igual que el de Unity, estos PNG llevan la forma en el canal alfa y el RGB en negro (enemigos) o negro y blanco (la nave). El GML no los tinta —sólo `oObserver` con `c_teal` y `oAgujerSuper` con `c_gray`—, así que el juego original muestra **bichos negros** sobre el fondo. El port hace lo mismo: una sola paleta de sprites con transparente, negro, blanco y el amarillo de las balas, en PAL1.

**El fondo del original es un color plano, y su textura no llega a la resolución del VDP.** El room tiene una capa de color `0xFF9A9A75` (teal grisáceo (117,154,154)) y encima `background38` repetido, que son dos grises (128 y 30) **al 6% de opacidad**. Compuestos dan (118,152,152) y (112,147,147): en la rejilla de 3 bits por componente del VDP los dos caen en el mismo color. Así que el fondo fiel es ese teal plano —(109,146,146) cuantizado— y la textura no se porta porque es invisible en este hardware. Con esto desaparece el campo de estrellas que el port anterior se había inventado.

**Cada objeto rota con una fórmula distinta, y eso fija qué dirección mira el arte sin rotar:** `oPlayer` y `oSpider` usan `image_angle = direction - 90` (arte hacia arriba), las cabezas y los eslabones de cola `image_angle = direction` o `= lookdir` (arte hacia la derecha), y `oBulletThin` `image_angle = direction + 90` (arte hacia abajo). El generador aplica esa corrección por sprite, así que el frame k de cada hoja corresponde al mismo rumbo real.

**Las celdas son grandes para lo que ocupa el arte porque el original rota alrededor del origen, no del centro.** La nave mide 38×58 con el origen en (19,19): la cola barre un radio de 42 px del room (8.4 en Mega Drive), así que la celda tiene que ser de 24×24 para un dibujo de unos 8×12. Lo mismo con la araña (origen (30,40), radio 52).

**Corrección del inventario de FASE 2:** la araña tiene **8** frames de animación, no 32, y la nave **1**, no 4. Mi primer conteo salió de una expresión regular mal hecha sobre el `.yy`; los valores correctos están leídos con el fichero parseado como JSON. La animación de la araña corre a 1 frame por frame de juego (`playbackSpeedType` 1, `playbackSpeed` 1.0), que en PAL son 1.2 para que el ciclo dure lo mismo en tiempo real.

### Trampa de rescomp: una fila de la hoja es una ANIMACIÓN, no un frame

Las hojas generadas llevan las rotaciones en filas y la animación en columnas. rescomp convierte **cada fila en una animación distinta**: la nave queda con 8 animaciones de 1 frame y la araña con 8 animaciones de 8 frames. `SPR_setFrame()` indexa dentro de la animación en curso, así que pedirle el frame 3 de una animación de un solo frame se sale del array; SGDK lo registra como error pero **sigue adelante** y dereferencia el puntero basura, lo que cuelga la máquina en el primer frame.

La forma correcta con este reparto: `SPR_setAnim()` elige la rotación, `SPR_setFrame()` el frame de animación, y `SPR_setAnimAndFrame()` pone los dos de una vez sin que al girar se reinicie la animación. Verificado comparando la actividad de hardware del emulador (`blastem -e`): 1691 B en 4 s con el cuelgue, 15554 B con el arreglo.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| La textura `background38` no se porta: el fondo es el color plano del room | Sus dos grises al 6% de opacidad caen en el mismo color en la rejilla de 3 bits del VDP | Ninguna visible; el fondo del original también se ve prácticamente plano |
| Los agujeros van como bloque de 8×8 tiles en el plano A, ajustados a la rejilla | Cinco sprites de 51×51 serían 20 sprites de hardware de los 80 disponibles | El dibujo del agujero queda hasta 4 px desplazado respecto a su posición exacta. Su máscara de colisión (3×2 px) sigue en el punto correcto |
| Rotación en 8 pasos de 45° donde el original es continua | El VDP no rota sprites | El rumbo dibujado se redondea a 45°: a este tamaño (8–13 px) la diferencia es de uno o dos píxeles |
| `oCabeza2` (primer eslabón) usa rotación fija | En el original su `point_direction` está comentado y conserva el `lookdir` que recibe al nacer | Fiel |

## Armas y power-ups (FASE 7)

Portados `oWBlock`, `oLaser`, `o3way`, `oBulletPara`, `oBomba`, `oRay`, la munición (`global.playerBulletsCounter`), los paquetes (`oPaquete` + `generaPaquete`), el desbloqueo por kills (`stCheckWeapon`) y las explosiones.

| Arma | Cadencia | Proyectiles | Munición por paquete |
|---|---|---|---|
| 0 WBLOCK | 5 f | 1 a 7.69 px/f | +10 |
| 1 LASER | 5 f | 10 segmentos a 9.59 px/f, vida 7 f, uno cada 6 px | +4 |
| 2 3WAY | 10 f | 3 a ±30° | +10 |
| 3 PIVOT | — | 3 satélites a 20 px y 120°, que persiguen con munición | +5 |
| 4 PARA | 5 f | 1 a la vez, 15.36 px/f, vida 5 f | +8 |
| 5 BOMBA | 5 f | a 20 px, mecha de 50 f, daña por área | +1 |
| 6 RAY | 5 f | a 20 px, a los 25 f barre la pantalla | +1 |
| — | 10 f | bala por defecto, infinita | — |

### Hallazgos de esta fase

**El arma no se cambia durante la partida.** `oWeaponBox/Step_0` sólo lee izquierda y derecha dentro de `if global.isMenu`, con detección de flanco por registro de desplazamiento de 2 bits. Así que `global.weapon_index` queda fijo durante toda la partida y se elige entre partidas: el selector llega con el menú (FASE 8). Hasta entonces el arma en juego es siempre WBLOCK, y como la munición empieza a 0, lo que más se ve es la bala por defecto.

**Todas las armas hacen el mismo daño.** Todos los proyectiles descienden de `oBulletS`, así que el manejador del enemigo (`Collision_oBulletS`: −4 la araña, −1 la cabeza) los cubre a todos y destruye el proyectil. Los daños propios de cada arma (`−1` del WBLOCK, **−10** del 3WAY, `−20` del satélite) están en el manejador del proyectil, que bajo el orden de eventos documentado no llega a ejecutarse. Las armas se diferencian por cadencia, número de proyectiles y alcance, no por daño.

**Por lo mismo, el efecto de congelar del PARA no llega a ocurrir.** `oBulletPara/Collision_oMalo` hace `with other { speed = 0 }`, pero el enemigo tiene su propio `Collision_oBulletPara` que resta vida y destruye la bala antes. Portado como bala rápida sin congelación, con la misma asunción de orden de eventos.

**El rayo no suma puntos.** `oRay/Alarm_0` hace `with oMalo instance_destroy()`: destruye a todos sin pasar por `checkMorte()`, así que no suma kills ni genera oleada ni paquetes. Además crea un gusano de tipo 2. Portado tal cual.

**La primera ráfaga de láser no dispara nada.** `tLaser` arranca en 10 y el bucle sólo crea segmento cuando baja de 0, así que la primera vez se queda sin crear ninguno y a partir de la segunda salen todos. Portado con su estado.

**rescomp deduplica frames idénticos, y eso me costó un ADDRESS ERROR.** Al convertir `sprExplosion` a 8×8 px sus 2 frames salían iguales y rescomp dejó uno solo; pedirle el frame 1 a esa animación corrompió el puntero de animación y la excepción saltó dentro de `SPR_update` → `updateFrame`. Localizado mapeando el PC del volcado (0x97DE) contra `out/release/symbol.txt`. Conviene comprobar `numFrame` en `resources.s` antes de indexar frames.

**Y la explosión en sí estaba mal portada: el original la ESCALA.** `oExplosion/Draw_0` hace `t += random(3)` y dibuja con `draw_sprite_ext(..., t, t, ...)`, destruyendo la instancia cuando `t > 6`: el sprite crece de 0 a 6 veces su tamaño en unos 4 frames. Yo lo había portado como un destello fijo de 8×8 durante 2 frames, que en pantalla no se veía. Ahora `tools/gm2md.py` genera cinco frames pre-escalados (escalas 1 a 5, celda de 32×32) y el código elige el que toca según `t`, con el mismo incremento aleatorio convertido a 50 Hz. El crecimiento se corta en escala 5 (32 px) en vez de 6 (38 px) porque una celda de 40×40 son 5×5 celdas de hardware y el VDP sólo compone 4×4 en un sprite: costaría dos sprites por explosión.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Pool único de 14 proyectiles | Sólo hay un arma activa a la vez, así que comparten objetos | El 3WAY puede perder algún proyectil en ráfaga sostenida (llegaría a 13 en vuelo) |
| Los paquetes se pintan como tiles del plano A | Son estáticos en el original; así caben los 24 del tope sin gastar sprites | Se pierde el latido (`draw_sprite_ext` con escala oscilante) y la posición se ajusta a la rejilla de 8 px |
| 4 explosiones simultáneas, con crecimiento hasta escala 5 en vez de 6 | Presupuesto de sprites, y el límite de 4×4 celdas por sprite de hardware | Si coinciden más de 4 impactos, la nueva sustituye a la más antigua; la explosión llega a 32 px en vez de 38 |
| El polvo (`oca`) y las astillas de cada explosión no están portados | Son instancias con movimiento propio y no quedan sprites | Las explosiones son un destello sin partículas. **Corregido después**: el polvo sí está portado (ver la corrección de FASE 7) y las astillas que faltan son las de `oDestro`, no las de `oDebrie` (ver la relectura del original) |
| `generaPaquete` sólo crea paquetes | Los power-ups de bomba, nave réplica y rayos están pendientes | Por encima de 16 paquetes el original cambia a esos power-ups; aquí sigue creando paquetes hasta 24 |

### Satélites, réplica y power-ups

**PIVOT** (`oPivot` + 3 `oOption`): el pivote es una segunda "nave" que el input del jugador mueve a la vez que la suya (`acce = 6`, con el mismo wrap), y los satélites orbitan a 20 px y 120° de separación girando 2.4 grados por frame de input. Con munición dejan la órbita y persiguen al objetivo del jugador a `move_towards_point(target, random(74))`, es decir a una velocidad distinta cada frame, hasta 17.8 px/f. Tocar el pivote destruye satélites y réplicas (`oPlayer/Collision_oPivot`).

**Nave réplica** (`oNaveReplicaPowUp` → `oTri`): no se mueve. Apunta al enemigo más cercano y dispara la bala por defecto con cadencia 20 frames si hay munición (gastándola) y 52 si no. Hasta dos a la vez.

**Power-ups del suelo**: `oBoomPowUp` mata las arañas en pantalla y explota en cruz; `oRayosPowUp` lanza un `oRay`; `oPaquete` da munición según el arma. `generaPaquete()` escala igual que el original: paquetes hasta 16, luego una bomba, luego hasta 2 réplicas mientras haya menos de 18 paquetes, luego rayos mientras haya menos de 24, y si no más paquetes.

Satélites y réplicas salen del mismo pool de 14 proyectiles: con el PIVOT equipado el original no dispara balas, así que no compiten por los objetos.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| La línea que une al jugador con sus réplicas (`draw_line_color`) no se porta | El VDP no tiene primitivas de dibujo | Las réplicas aparecen sueltas, sin el cordón azul del original |
| La explosión en cruz (`oExplosionCruz`) usa el mismo destello que las demás | No hay sprites para una segunda forma | Visual |

## Las armas, comprobadas una a una (revisión)

Las siete estaban portadas desde FASE 7, pero en el mando no se notaban. Las
forcé una a una en el emulador (`set [dirección de g_weaponIndex]` desde
`tools/mdrun.py`, con munición) y las miré en pantalla:

| # | Arma | Antes | Ahora |
|---|---|---|---|
| 0 | WBLOCK | ✅ bloques blancos | ✅ |
| 1 | LASER | ❌ no se veía nada | ✅ haz de ~60 px |
| 2 | 3WAY | ✅ abanico de tres | ✅ |
| 3 | PIVOT | ✅ tres satélites en órbita | ✅ |
| 4 | PARA | ❌ no se veía | ✅ (es de 2×6 px: el original mide 10×28 en un room de 1600×1200) |
| 5 | BOMBA | ✅ | ✅ (no seleccionable, igual que en el original) |
| 6 | RAY | ✅ | ✅ (llega por el power-up `oRayosPowUp`) |

### El selector del menú no llegaba a la partida

`Game_start()` hacía `g_weaponIndex = 0`. En el original, `startGame()` **no
toca** `global.weapon_index`: sólo se pone a cero en `scrInit`, al arrancar la
máquina. Por eso allí el arma elegida en el interludio vale para la partida
siguiente, y aquí se perdía siempre: daba igual lo que eligieras, jugabas con
el bloque blanco. Quitada esa línea.

La munición sí se reinicia, y eso era correcto: `oPlayer/Create_0` hace
`global.playerBulletsCounter = 0` y `startGame()` crea el jugador, así que
cada partida empieza sin balas y disparando la bala por defecto.

### El láser vuelve a sus 10 segmentos

Lo había recortado a 3 por un cálculo mío equivocado: creí que los segmentos
competían por los 80 objetos Sprite del VDP, y en realidad salen del pool de
14 proyectiles que ya estaba reservado. Con `largo = 10`, cadencia 5 y vida 7
puede haber ~14 segmentos en vuelo, justo el tamaño del pool: en fuego
sostenido se puede perder alguno, como ya le pasa al 3WAY. Restaurado al
valor del original y comprobado en pantalla.

### Armas que el original tampoco usa

- `oCircular` y `oCircularManager` sólo aparecen en una línea comentada
  (`//instance_create(x,y,oCircularManager)` en
  `oPlayer/Collision_oNaveReplicaPowUp`): están en el proyecto pero el juego
  no las crea nunca. No se portan.
- La lista del menú (`oWeaponBox/Create_0`) es `wBlock, laser, threeWay,
  circular, tri`, pero el `switch` que dispara (`oPlayer/Step_0`) hace otra
  cosa en los índices 3 y 4 (pivote y para). El icono y el arma no coinciden
  en el original; portado tal cual.

## HUD, menú y estados de partida (FASE 8)

Portados `oMenu` (splash e interludio), `creditsManager`, `mataPlayer()` → `menu()` y la selección de arma de `oWeaponBox/Step_0`.

### El original es una recreativa

`scrInit` deja `global.credits = 0` y `oMenu/Step_0` sólo arranca partida `if (hasCredits())`, descontando uno con `removeCredit()`. Sin crédito no se juega. La tecla C mete crédito, X (o el botón 1 del joystick) empieza.

Mapeo en Mega Drive, al no haber teclado: **START mete crédito**, **A/B/C empieza**, D-Pad mueve y elige arma. Es la traducción del esquema de teclas del original, documentada aquí porque no sale del GML.

### Pantallas

**Splash** (sin partida jugada): logo (`sprite176`, 256×256 → 51 px, como bloque de 7×7 tiles), marquesina de texto y contador de créditos.

**Interludio** (al morir, `global.isMenu`): escribe `KILLS`, `TOP` y `NEXT` letra a letra (`iText` + `alarm[0] = 4`, aquí 3 frames), pinta la barra gris que crece 12 px del room por frame hasta 300 (2.4 y 60 px en Mega Drive), los iconos de las armas desbloqueadas separados 26 px, y una marquesina con el nombre del arma elegida. Izquierda y derecha cambian de arma dentro de las desbloqueadas.

Posiciones, todas del `Create_0` de `oMenu` escaladas ×0.2: KILLS en la fila 13, TOP en la 15, NEXT en la 18, CREDITS en la 21, iconos en la 24, marquesina en la 29, columna de texto la 10.

### Decisiones técnicas

**La marquesina usa el scroll por hardware del plano B.** El original la mueve 2 px por frame redibujando el texto; aquí el texto se reescribe sólo cuando avanza un tile entero (cada 4 frames) y el movimiento fino lo hace `VDP_setHorizontalScroll`. El plano A queda para el texto y los tiles del menú, que no se mueven.

**Los elementos estáticos se pintan una vez.** La primera versión redibujaba logo e iconos en cada frame, como el original: 95 KB de tráfico al VDP cada 5 segundos. Pintándolos sólo al cambiar baja a 22 KB, el mismo nivel que en partida.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Texto con la fuente de 8×8 de SGDK | Las fuentes del original son de sprites: `sFont2` son 98 glifos de 80×80 (16×16 al escalar, 392 tiles de VRAM) y `sFont3` de 20×20 queda en 4×4, por debajo de la rejilla del hardware | El menú y el HUD se leen con la tipografía del sistema, no con la del original |
| El marcado del arma elegida es un carácter debajo del icono | El original le superpone `sprite191`, un marco animado de 4 frames de 128×128 | Visual |
| El efecto de muerte (`oMuerte`) no se porta | Es un sprite que se escala ×8 por frame y hace temblar el tamaño de la vista; el VDP no escala sprites | Se pasa al menú sin el fogonazo de muerte |
| START mete crédito y A/B/C empieza | El original usa teclado (C y X) y no hay mapeo de pad en el GML | Ninguna: es la traducción obligada del esquema |

## Sonido (FASE 9)

Los 22 sonidos del original son muestras Ogg Vorbis, no música de registros, así que en Mega Drive van como **PCM de 8 bits a 13300 Hz por el driver XGM2**, que da tres canales: el 1 para la música del menú y el 2 y el 3 para efectos y voces.

| | |
|---|---|
| Convertidos | 20 de 22 (14 efectos, la música del menú y las 5 voces de arma) |
| PCM total | 679 KB |
| ROM | 896 KB (de 128 KB) |

### El pipeline, sin ffmpeg

En el sistema no hay ffmpeg, ni sox, ni oggdec, y el `afconvert` de macOS no decodifica Vorbis. Sí hay **VLC**, cuya interfaz `dummy` transcodifica sin abrir ventana. VLC escribe una cabecera WAV inválida para streams (formato 0 y tamaño desconocido), así que `tools/gm2md_audio.py` extrae el bloque `data` y reescribe la cabecera a mano antes de pasárselo a rescomp.

### Dónde suena cada cosa

Cada sonido va donde lo lanza el GML: `shotSnd` en el `Create_0` de `oBulletThin` y `oBulletPara`, `wBlockSnd` en `oWBlock`, `threeWaySnd` en `o3way`, `laserSnd` en `oLaser` y `oBomba`, `aranamorteSnd` en `oExplosion`, `powUpSnd` al coger paquete, `bombaSnd` + `risaSnd` con el power-up de bomba, `rayosSnd` con el de rayos, `repeteaSnd` con la nave réplica, `morteSnd` al morir, `menuSong` al entrar en el menú y las cinco voces de `triggerSpeech()` al cambiar de arma.

### Desviaciones que introduce esta fase

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Dos canales de efectos | XGM2 da 3 y uno lo ocupa la música del menú | GameMaker mezcla voces sin límite: allí los disparos se solapan, aquí un efecto nuevo corta al más antiguo. Con la cadencia de disparo se nota en ráfaga |
| 13300 Hz y 8 bits mono | Es lo que reproduce el driver; los originales van de 11 a 44 kHz y la mitad en estéreo | Sonido más sucio y sin estéreo, el techo del hardware |
| `circularSnd`, `boomSnd` y `dustSnd` convertidos pero sin punto de llamada | Sus objetos (arma circular, `oBoomPowUp` propio, `oca`) no están portados o no tienen evento de sonido propio | Ninguna: están listos para cuando se porten |
| `caralludoSnd` y `triRepeteaSnd` sin convertir | No tienen llamada en el GML en uso | Ninguna |
| La ROM pasa a 896 KB | 679 KB de PCM | Ninguna: es tamaño normal de cartucho. En hardware real, el Z80 leyendo muestras de ROM roba algo de bus al 68000, y eso toca medirlo en FASE 10 |

## Explosiones: el fogonazo no es la explosión (corrección de FASE 7)

Reportado como “no veo animaciones de explosiones”, y el fallo resultó ser de
lectura del original, no de dibujado. Una muerte en el GML son **dos** objetos:

```gml
// oExplosion/Create_0
audio_play_sound(aranamorteSnd,.7,false); t = 0
if (global.explosions) { u = instance_create(x,y,oca); u.direction = random(360); u.speed = random(4) }

// oExplosion/Draw_0
t += random(3)
draw_sprite_ext(choose(sprExplosion,sprExplosion_2),image_index,x,y,t,t,0,c_white,1)
if t > 6 instance_destroy()

// oca/Create_0        alarm = 40;  size = random(30*0.1);  audio_play_sound(dustSnd,.5,false)
// oca/Draw_0          xf += 3;  yf += choose(-6,6)
//                     draw_sprite_ext(expl_4,image_index,xf,yf,size,size,angle,c_white,0.8)
//                     draw_sprite_ext(expl_1,image_index,xf,yf,size,size,angle,c_white,0.8)
```

El fogonazo (`oExplosion`) crece de escala 0 a 6 y se destruye en ~4 frames:
**67 ms, un parpadeo**. Lo que el ojo registra de una muerte es el polvo
(`oca`), que vive **40 frames (0,67 s)** y dibuja `expl_4` (206×206) y `expl_1`
(128×128) superpuestos a escala `random(3)`: una humareda oscura de hasta
124 px en pantalla. El port tenía el fogonazo y no tenía el polvo, así que de
una muerte no se veía prácticamente nada.

Ahora los dos están en la misma ranura, encadenados: fogonazo (4 frames) y
luego humareda (33 frames). `res/sprite/explosion.png` es una hoja de 4 filas
(= 4 animaciones de rescomp) de 5 celdas de 64×64: fila 0 el fogonazo en sus
cinco escalas, filas 1–3 la humareda en tres tamaños. Coste: **0 objetos de
sprite nuevos**, 53 tiles de VRAM por ranura y 256–1152 B de DMA por frame
(tope del hardware: 8000).

### Las 80 entradas de la tabla de sprites estaban siempre llenas

Al mirar la tabla del VDP con el depurador aparecieron las 79 ranuras **todos
los frames**, también las aparcadas fuera de pantalla. `SPR_addSprite` de SGDK
no activa el cálculo de visibilidad (`SPR_FLAG_AUTO_VISIBILITY`), así que una
ranura escondida en −96 seguía ocupando sitio en una tabla que sólo tiene 80
entradas: no cabía nada nuevo y la humareda (4 sprites de hardware por puff)
se habría quedado fuera. Con `SPR_setVisibility(spr, AUTO_FAST)` en los cinco
módulos que crean sprites, la tabla pasa a llevar sólo lo vivo: **16 entradas**
en una partida normal en vez de 79, y el DMA de la tabla baja de 632 a 128 B
por frame.

### Un fallo de generación de código de GCC

Con la humareda metida, la explosión seguía apareciendo en sitios absurdos.
En el desensamblado:

```
clr.w 2(%a2)              ; e->t = 0        (correcto)
move.w 30(%sp),(%a2)      ; e->x = x        (¡sin desplazamiento!)
move.w 34(%sp),(%a2)      ; e->y = y        (¡al mismo sitio!)
...
move.w (8,%a4),...        ; lee e->y en +8
move.w (6,%a4),...        ; lee e->x en +6
```

`m68k-elf-gcc 16.2.0` con `-O2`/`-O3` pierde el desplazamiento de los dos
campos al integrar `place()` en `Effect_spawn`: escribe `x` e `y` en el offset
0 de la ranura (machacando `phase`) y luego los lee de +6 y +8, que nunca se
escribieron. No es UB del código: el mismo fuente a `-O1` sale correcto, y
`place()` marcada `NO_INLINE` también. Se usa esa marca, con el porqué en el
código. `tools/check_codegen.py` recorre el ROM enlazado buscando el patrón
(dos escrituras seguidas al mismo `(%aN)` sin desplazamiento) y hoy sólo
aparece en rutinas de SGDK que hablan con el hardware, donde es legítimo.

### Desviaciones que introduce esta corrección

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Humareda topada a 64 px | `size = random(3)` llega a 124 px en pantalla; un sprite del VDP compone como mucho 4×4 celdas y uno de 128 px costaría 16 sprites de hardware de los 80 | Los tres tamaños sorteados llegan hasta 1,55 de escala en vez de 3: la humareda es más pequeña que en el original, sobre todo en los casos grandes |
| La animación del humo avanza cada 2 frames | A uno por frame son 4 subidas de tiles por frame; así se reparte el DMA | El humo “se mueve” a la mitad de ritmo; en un borrón de 0,67 s no se distingue |
| Sin el giro por frame (`angle = random(30)`) ni el alpha 0,8 | El VDP no rota sprites ni mezcla | La humareda es opaca y no cabecea. El color sale del arte (gris oscuro), que sobre el fondo es casi lo mismo que el original translúcido |
| El polvo empieza 4 frames después del fogonazo | Comparten ranura de sprite para no gastar objetos nuevos | Imperceptible: el fogonazo dura esos mismos 4 frames |
| `oDebrie` (astillas) sigue sin portar | En el GML ya está comentado (“optimize”), no se crea | Ninguna: el original en uso tampoco las lanza |

## Ver y medir el emulador desde la línea de comandos (FASE 10)

macOS no da permiso de grabación de pantalla al terminal, así que hasta ahora
las comprobaciones visuales dependían de ti. BlastEm trae depurador, pero
ignora la entrada redirigida. `tools/mdrun.py` se lo sirve por un pty, y con
eso se puede:

- **capturar la pantalla** (`bindup "ui.screenshot"` deja un PNG en `$HOME`),
- **jugar sin manos** (`binddown "gamepads.1.start"`, direcciones, botones),
- **parar en una función** (`breakpoint 0x<dir>` con las direcciones de
  `out/release/symbol.txt`) y **avanzar frames exactos** (`frames N`),
- **listar la tabla de sprites del VDP** (`vs`): qué se dibuja, dónde y de qué
  tamaño, que es como se encontró lo de las 80 entradas,
- **leer la RAM** (`p/d [0x<dir>]`): variables del juego y de SGDK, que es como
  se midieron la cola de DMA y su tope.

## Plan de las fases siguientes

- **FASE 4 — Núcleo.** Jugador con `acc` 1.44 y wrap de 3 px, apuntado automático al más cercano, `oBulletThin` infinita a 7.68 con cadencia 10, AABB y muerte al contacto.
- **FASE 5 — Enemigos.** Los 5 agujeros, 75 frames de aparición, máquina de 3 estados con giro gradual, araña, los dos gusanos con buffer circular, oleadas por `malosCounter`.
- **FASE 7 — Armas y power-ups.** Las 7 armas con sus cadencias, munición por paquete y desbloqueo por kills; `oTri` y su línea (cambio a documentar).
- **FASE 8 — HUD, menú, estados.** KILLS/LAST/TOP/NEXT, menú, muerte, `oPuntoMorte`.
- **FASE 9 — Audio.** 22 sonidos a YM2612/PSG con prioridades; `menuSong`.
- **FASE 10 — Optimización y pruebas.** Medición de framerate y sprites por línea con los topes del GML, ajuste documentado, validación en hardware real.

## Optimización (FASE 10)

Medido antes de tocar nada, con la arena llena a los topes del port (8 arañas,
los 4 gusanos con sus 52 eslabones, 14 proyectiles y los paquetes): **25 fps y
190 % de CPU**. El arnés está en `inc/bench.h` + `src/bench.c`:
`PORT_STRESS=1` llena la arena y hace inmortal al jugador, `PORT_BENCH=1` mide
cada módulo llamada a llamada, y `PORT_PROF=1` perfila las etapas del bucle
dentro de la partida con el contador de línea del VDP. Ninguno entra en la
ROM que se distribuye.

### De dónde salía el tiempo

| Causa | Qué pasaba | Arreglo |
|---|---|---|
| `F16_toInt`, `F16_mul`, `F16_sin/cos`, `F16_normalizeAngle` | SGDK las define en otra unidad de compilación y el enlazado va sin LTO: cada uso era un `jsr` con argumentos por pila, ~600 veces por frame, para un desplazamiento o una consulta de tabla | `inc/fastmath.h`: las mismas operaciones en línea |
| `F16_getAngle` | Acaba en `F16_atan2`: división de 32 bits por software y polinomio de grado 7. ~3000 ciclos, una vez por enemigo y frame | `Fast_angle`: plegado al primer octante, `divu.w` del 68000 y tabla de 257 entradas. ~200 ciclos, 0,23° de error máximo |
| `Coll_overlap` | Función con ocho argumentos llamada ~170 veces por frame | `static inline` en `inc/collision.h` |
| Cajas de colisión | Cada consulta (14 proyectiles, jugador, apuntado) recalculaba la caja entera de cada enemigo desde el fix16 | Caché compactada de vivos, una vez por frame, en `enemy.c` y `snake.c` |
| Subida de tiles por DMA | Cada cambio de rotación sube 4-9 tiles. Con la pantalla llena, ~16 KB por frame contra los ~8 KB que caben en un vblank PAL: SGDK descartaba transferencias y, como la tabla de sprites se encola la última, **la pantalla se congelaba** | `inc/vramspr.h`: los 577 tiles de todas las definiciones se suben una vez a VRAM y cambiar de frame es escribir el índice. Cero DMA de tiles por frame |
| `SPR_setPosition` / `SPR_setAnim` | ~200 y ~380 ciclos por llamada, unas 75 y 60 veces por frame | `sprPos` / `sprAnimFrame` en línea (`inc/fastspr.h`) |
| `cacheParts` del gusano | Una multiplicación por eslabón para indexar el historial | Índice arrastrado restando `stepdelay` |

### Resultado

| | Antes | Ahora |
|---|---|---|
| Caso peor (arena llena) | 25 fps, 190 % de CPU | 103 % de CPU, y **se dibuja** (antes de precargar los tiles la pantalla se quedaba congelada) |
| Partida normal | 50 fps, 39-78 % | 50 fps, **19 %** |

Reparto que queda en el caso peor (perfilado en partida, 314 frames):
gusanos 33 %, `SPR_update` de SGDK 12 %, jugador y apuntado 10 %, HUD 7,6 %
(es el contador de FPS/CPU de depuración), vblank 7 %, proyectiles 2,7 %.

### Lo que falta

El caso peor sigue justo por encima del frame (103 %), así que con la arena a
tope el juego va a 25 fps. Lo que queda por hacer, por orden de lo que medí:

1. **El bucle de eslabones del gusano (16,5 % del frame)**: 525 ciclos por
   eslabón para colocar un sprite es mucho más de lo que debería; estaba
   mirando el desensamblado cuando paré.
2. **Quitar el contador de pantalla**: 7,6 % gratis (`HUD_SHOW_PERF` en
   `inc/hud.h`), en cuanto no haga falta para medir.
3. **`SPR_update` de SGDK (12 %)**: ~400 ciclos por sprite y 79 sprites. Ya no
   usamos de SGDK ni la posición, ni los tiles, ni el frame: montar la tabla
   de sprites a mano costaría ~40 ciclos por sprite.
4. **El apuntado (parte del 10 %)**: recorre los 60 enemigos y eslabones cada
   frame buscando el más cercano.

## Relectura del original: lo que la auditoría se había dejado (FASE 10)

Tras volver a leer `surfize_changes` entero —los 61 objetos, sus padres y sus
eventos, no sólo los scripts— salen cuatro cosas del original que este port no
tenía, y dos que tenía mal. Las dos primeras ya están portadas; las otras
quedan listadas al final con lo que hace falta para hacerlas.

### 1. La sacudida de pantalla no estaba (ya portada)

`stShake` aparecía en la tabla de equivalencias de la FASE 3 y nunca se llegó a
implementar. No es un adorno suelto: **salta con cada disparo**, y el juego
dispara solo. El original la llama desde siete sitios:

| Llamada | Dónde | Duración real |
|---|---|---|
| `stShake(4, 5, 0.2)` | `oPlayer/Step_0`, cada disparo con arma | 4 + 25 frames |
| `stShake(1, 1, 0.1)` | `oPlayer/Step_0`, bala por defecto | 1 + 10 frames |
| `stShake(5, 15, 1)` | `oEstallido/Create_0`, la metralla | 5 + 15 frames |
| `stShake(5, 5, 0.2)` | `oTri/Step_0`, disparo de la réplica | 5 + 25 frames |
| `stShake(50, 20, 4)` | `oRay/Create_0` | 50 + 5 frames |
| `stShake(10, 150, .6)` | `oExplosionCruz/Create_0` | 10 + 250 frames |
| `stShake(2, 2, 0.2)` | `oSplashS/Step_0` | no corre nunca |

La duración no es `time`: `shake_magnitude` no empieza a bajar hasta que
`shake_time` llega a 0, así que una sacudida vive `time + magnitud/fade`
frames. La del `oExplosionCruz` dura **4,3 segundos** a 30 px de amplitud
inicial. Y el salto es siempre a `choose(-mag, +mag)` exacto: nunca a un valor
intermedio, nunca a cero, que es lo que le da el temblor duro.

La del splash no corre: `stShake` hace `with (oPlayer)` y durante el splash no
hay jugador. No se porta.

**Cómo se hace en Mega Drive.** No hay cámara: mover la vista es desplazar el
plano (que en partida lleva los agujeros, los paquetes y el HUD) con los
registros de scroll, y restarle el mismo desplazamiento a cada sprite. El
desplazamiento se aplica dentro de `sprPos` (`inc/fastspr.h`), así que ninguna
llamada del juego cambia: son dos restas por sprite y frame.
`camera_set_view_pos(cam, cx, cy)` equivale a `hscroll = -cx` y `vscroll = +cy`.

Medido en el emulador, con el jugador quieto y leyendo la tabla de sprites del
VDP a la vez que `g_shakeX`/`g_shakeY`: con la cámara en -1 el sprite del
jugador sale en la columna 157 y con la cámara en +1 en la 155, y la banda del
HUD del plano se desplaza exactamente los mismos 2 px en el mismo sentido.
Plano y sprites van juntos.

| Desviación | Por qué | Consecuencia en juego |
|---|---|---|
| La sacudida de la bala por defecto no se ve | Magnitud 1 del room son 0,2 px a escala x0,2, por debajo del píxel | El disparo sin munición no tiembla; el disparo con arma sí (1 px) |
| El desplazamiento se calcula al principio del frame, no al final del Step del jugador | Para que el plano y los sprites usen el mismo valor dentro del mismo frame | Ninguna: un frame de desfase |
| Al morir el jugador se recentra la vista | El original deja la cámara donde estuviera, porque `oPlayer` deja de existir y nadie la devuelve a su sitio | El menú no sale torcido |
| El plano se envuelve por el borde en vez de descubrir el exterior del room | El VDP no tiene "fuera del plano" | Ninguna visible: en partida el plano está vacío salvo agujeros, paquetes y HUD, así que por el borde asoma el color de fondo |

### 2. Los eslabones del gusano son un escudo (ya portado)

Esto cambia cómo se juega. La auditoría decía que las balas **atraviesan** los
seguidores. Es falso, y el error estaba en haber leído sólo la mitad del
intercambio de eventos.

Cuando una bala toca un eslabón (`oCorpo`, `oCu1`, `oCu2`, `oCabeza2`) corren
**dos** manejadores, uno detrás del otro:

1. el del eslabón, heredado de `oMalo/Collision_oBulletS`:
   `if (global.explosions) instance_create(x, y, oExplosion)`. Pinta la
   explosión y **no destruye la bala**;
2. el de la propia bala, `oBulletThin`/`oWBlock`/`o3way`/`oLaser`
   `/Collision_oMalo`: `with other if hp != undefined hp -= 1` y
   `instance_destroy()`.

Los eslabones tienen `hp = 10` en su `Create_0` y **ese hp no se comprueba en
ningún sitio**: no se pueden matar. Así que el eslabón se queda la bala y
sigue entero.

Las cabezas no pasan por ahí: `oSpider`, `oCabeza1` y `oCabeza2b` definen su
propio `Collision_oBulletS`, que SUSTITUYE al de `oMalo` y destruye la bala
antes de que llegue a correr el manejador de ella. Por eso de una cabeza o de
una araña no sale metralla, y de un eslabón sí.

Consecuencia: **un gusano enroscado tapa su propia cabeza**. Hay que buscarle
el ángulo. Es lo que le da sentido a que el bicho mida 12 eslabones, y el port
se lo estaba saltando.

Coste: `Snake_hit` tenía que pasar de 4 cajas por frame a 4 x 12. Para que no
se note se guarda en `cacheParts` la caja que envuelve cabeza y eslabones de
cada gusano, y el gusano entero se descarta con cuatro comparaciones. El mismo
descarte se le ha puesto a `Snake_collides`, que ya recorría los 48 eslabones
cada frame, así que la cuenta sale casi a cero: el caso peor pasa de 103 % a
104 % de CPU.

### 3. El power-up de la bomba no mataba como creía este port (ya corregido)

`oPlayer/Collision_oBoomPowUp` hace `with (oMalo) { oExplosion; hp = 0 }`.
**Poner `hp = 0` no mata a nadie**: ese hp sólo se mira en el impacto de bala.
Lo que limpia la pantalla es el *Animation End* del `oExplosionCruz` que crea,
dos frames después (`sprExplosion` son 2 frames a 1 por frame):

```
with oCorpo/oCu2/oCu1/oCabeza1/oCabeza2/oMalo instance_destroy()
repeat(2) instance_create(-10, -10, oCabeza2b)
```

Destruye **sin pasar por `checkMorte()`**, así que no suma kills, no genera
paquetes y no genera oleada, igual que el rayo. Y deja dos gusanos de tipo 2
recién nacidos.

El port sumaba al marcador las arañas que mataba y dejaba vivos a los gusanos.
Corregido: ahora barre arañas y gusanos sin puntuar y crea los dos gusanos.
`Enemy_killAll()` se ha borrado porque codificaba la regla equivocada. Con el
tope del port (`MAX_CABEZA_2` = 1) sólo nace uno de los dos.

### 4. `oPivot` no se puede tocar

`Proj_clearSatellites()` porta `oPlayer/Collision_oPivot`, pero ese evento **no
corre nunca en el original**: `oPivot` tiene `spriteId` y `spriteMaskId` a
`null`, así que no tiene máscara de colisión. Se deja la función, sin nadie que
la llame, igual que allí. Anotado en el código para que no parezca un cabo
suelto.

### Lo que sigue faltando del original

| Qué | Qué hace en el original | Qué hace falta para portarlo |
|---|---|---|
| `oEstallido` -> 12 `oDestro` | Cada impacto de `oWBlock` o de un satélite del PIVOT **contra un eslabón** lanza 12 astillas a 30 grados de separación, a 10 px/f, que mueren al salir de la vista y **restan 1 de hp** a arañas y cabezas. Es una reacción en cadena, y es la razón de ser del arma por defecto | 12 objetos Sprite por ráfaga y el pool de SGDK está en 77 de 80. Las 12 astillas salen del mismo punto, a la misma velocidad y en ángulos fijos: son un **anillo que se expande**, o sea que se pueden dibujar con UN solo sprite de animación pre-generada y calcular el daño de los 12 puntos analíticamente |
| `oAutoDestruccion` | Efecto de impacto del arma PARA: una bola de rayos (`draw_curve`) que crece hasta escala 20 y a los 80 frames estalla en `oEstallido` | `draw_curve` no existe en el VDP; habría que generar la animación en `tools/gm2md.py` |
| `object0` / `object1` | El haz del arma PARA: 14 segmentos encadenados a lo largo del rumbo del jugador, que laten, cambian de color y hacen daño al tocar un `oMalo` | Depende del presupuesto de sprites, igual que la metralla |
| `oMuerte` | Fogonazo de muerte del jugador | Ya anotado de antes |
| `oDebrie` | Nada: está comentado en el GML | Nada. **Ojo**: la entrada anterior de este informe daba por buena esa conclusión y se saltaba `oDestro`, que es otro objeto y sí está vivo (ver la fila de arriba) |

El tope de 80 objetos Sprite de SGDK (`MAX_SPRITE`) es lo que bloquea la
metralla y el haz del PARA. Es el mismo tope que el punto 3 de la lista de
optimización pendiente: montar la tabla de sprites a mano en vez de usar
`SPR_update` quita el tope y de paso devuelve el 12 % de frame que cuesta
`SPR_update`. Hacer eso primero resuelve las dos cosas a la vez.

## Revisión general contra el original: lo que faltaba y el motor de sprites propio

Segunda pasada completa por `surfize_changes`, esta vez objeto a objeto con sus
padres y sus eventos de colisión, más el inventario de los 42 sprites y los 22
sonidos. De ahí salió una lista de huecos que compartían todos el mismo tope:
los **80 objetos Sprite de SGDK**, con el port ya en 79. Así que lo primero fue
quitar ese tope, que además era el punto 3 de la lista de optimización
pendiente.

### El motor de sprites de SGDK, fuera (`inc/spr.h`, `src/spr.c`)

`SPR_update` son unos 400 ciclos por sprite: recorre una lista enlazada, mira
banderas de animación, temporizadores, reserva de VRAM, subida de tiles,
cálculo de visibilidad y actualización diferida. De todo eso este port no usa
nada: los tiles están precargados (`inc/vramspr.h`), no hay animación
automática, no hay reserva dinámica y la posición y el frame se escriben a
mano. Lo que queda por hacer es recorrer un array y escribir cuatro palabras
por sprite de hardware.

El motor nuevo son 160 ranuras lógicas de 22 bytes y un montador que vuelca
las 80 primeras que se vean. Detalles que costó encontrar:

- **`numSprite` negativo no es un contador.** rescomp marca con el bit alto
  los frames que caben en UN sprite de hardware alineado con la celda, sin
  desplazamientos (`sprite_eng.c`: `if (numSprite < 0)`). Leyéndolo como
  contador el bucle se desbocaba y la pantalla se llenaba de basura. Es el
  caso de todo este juego menos la explosión, cuya celda de 64x64 no cabe en
  los 32x32 del VDP y se parte en cuatro.
- **El reparto en sprites de hardware se resuelve al cambiar de frame**, no en
  el montador: unas pocas veces por sprite y partida en vez de una por sprite
  y frame.
- **Contrato de aparcado.** El montador ya no comprueba si el sprite cae
  dentro de la pantalla. Esa caja eran cuatro comparaciones por sprite y
  frame, un 5 % del frame con la arena llena, y daba verdadero casi siempre
  porque aquí todo lo vivo está dentro del campo. A cambio, los tres módulos
  cuyos sprites pueden salirse (los números que vuelan al HUD, el haz del PARA
  y la humareda, que va a la deriva) aparcan ellos mismos lo que se sale.

### Lo que faltaba, ya portado

| Qué | Del original | Cómo queda |
|---|---|---|
| **Metralla** (`oEstallido` → 12 `oDestro`) | Cada impacto de WBLOCK o de un satélite del PIVOT **contra un eslabón de gusano** lanza 12 astillas cada 30°, a 10 px/f, que mueren al salir de la vista y **restan 1 de hp** a arañas y cabezas | `src/debris.c`, pool de 24 (dos ráfagas). Mata en cadena, como allí |
| **`oMuerte`** | Fogonazo de la muerte: `sprExplosion_2` a escala `t*8` con t de 1 a 20, que al cuarto frame ya tapa el room, más una deformación aleatoria de la vista | Tres tamaños como sprite y a partir de ahí el fondo en blanco, que es lo que queda de ese sprite gigante. La deformación de vista se cambia por la sacudida más fuerte del juego |
| **`morteGusano`** | El gusano no desaparece: crea un `oExplosion` por eslabón y suena `aranamorteSnd` una vez | `blowUp()` en `src/snake.c`. El pool de explosiones sube de 4 a 8, así que se ve la cola de la cascada |
| **`oPuntoMorte`** | El número de kills o de balas sale volando y acelerando hacia su esquina del HUD | `src/flyer.c`, con los dígitos sacados de `sFont2`, la fuente del original |
| **`object0`/`object1`** | El haz del arma PARA: 14 segmentos encadenados por delante de la nave que restan 1 y se borran al tocar | `src/beam.c` |
| **La línea a las réplicas** | `oPlayer/Draw_0`: `draw_line_color(x, y, oTri.x, oTri.y, c_blue, c_blue)` | Seis puntos azules por línea: el VDP no dibuja líneas |
| **`oAutoDestruccion`** | El impacto del PARA: bola de rayos que crece hasta escala 20 y a los 80 frames revienta en metralla | Fase nueva de `src/effect.c`, con el parpadeo de tamaño y el `oEstallido` al final |

### Un fallo latente: la paleta que se cargaba no era la generada

`tools/gm2md.py` escribe `res/gm_art.h`, pero en el repo había un
`res/gm_palettes.h` del 6 de noviembre con la paleta vieja, y los dos usaban el
mismo guardián de include. `main.c` incluía el viejo, así que **la paleta que
se cargaba en PAL1 no tenía el gris de humo**: la humareda de todas las
explosiones salía negra en vez de gris desde que se portó. Borrado el fichero
duplicado y arreglado. Se ve en las capturas: el humo ahora es gris.

### Lo que se queda fuera, y por qué

| Qué | Por qué |
|---|---|
| `oCojeCaja` (el destello al coger un paquete) | Es `sprPaquete` creciendo hasta escala 80 con **alfa 0,14**. El VDP no mezcla: sin transparencia sería un rectángulo blanco tapando media pantalla, que es peor que no ponerlo |
| Las curvas de rayos de `oAutoDestruccion` | `draw_curve` no tiene equivalente |
| El latido de escala y el desvanecido del haz del PARA | El VDP no escala ni mezcla |
| `oExplosion` eligiendo entre dos sprites (`choose(sprExplosion, sprExplosion_2)`) | Duplicaría los tiles del efecto más repetido del juego |
| La fuente del original en el HUD (`sFont2`) | El HUD va en texto de plano con la fuente del sistema. Los dígitos de `sFont2` sí están portados, para los números que vuelan |
| `oCircular`, `oBomba`, `oRayoBota`, `oObserver`, `oDebrie`, `oParal`, `oca2` | Código muerto en el original: o están comentados, o nada los crea, o no son alcanzables desde el menú |
| `oPlayer/Collision_oPivot` | `oPivot` tiene `spriteId` y `spriteMaskId` a `null`: no tiene máscara, así que ese evento no corre nunca. Portado pero sin nadie que lo llame, igual que allí |

### Otro fallo latente: el menú nunca subía sus tiles

`Menu_init()` estaba escrita desde la FASE 8 y **nadie la llamaba**. El menú
pintaba su mapa de tiles apuntando a una zona de VRAM que no se había cargado,
así que no se veían ni los iconos de arma, ni el logo del splash, ni la barra
del interludio: sólo el texto, que va con la fuente del sistema. Llamada desde
`Game_init()`.

### Optimización, con el perfilador delante

El arnés creció para poder medir de verdad: `PORT_PROF=1` deja sólo los
contadores de frame y de carga (que casi no cuestan) y `PORT_PROF=2` añade el
cronómetro por etapas. Y la semilla aleatoria pasa a ser fija con el arnés
puesto: salía de `GET_VCOUNTER`, así que dos medidas nunca comparaban la misma
partida y las diferencias de un 5 % eran ruido.

Lo que se midió y se hizo, por orden de lo que dio:

| Cambio | Por qué | Medido |
|---|---|---|
| Motor de sprites propio en vez de `SPR_update` | ~400 ciclos por sprite para montar una tabla de 80 entradas de 8 bytes | caso peor de 24 a 41 fps |
| Contrato de aparcado: fuera la caja de pantalla del montador | Cuatro comparaciones por sprite y frame que daban verdadero casi siempre | 41,3 → 42,5 fps |
| Bucle de eslabones partido en tramos | Tres comparaciones por vuelta para saber si el eslabón rota | `S_PARTS` 10,3 % → 7,1 % |
| Caja envolvente del gusano, recalculada cada 4 frames con margen | Cuatro comparaciones por eslabón y frame para una caja que sólo sirve de filtro | `S_CACHE` 9,8 % → 7,2 % |
| Historial del gusano en píxeles enteros | Dos desplazamientos de 6 bits por eslabón y frame; en el 68000 son 18 ciclos cada uno | 42,5 → 42,8 fps |
| Sesgo de pantalla fuera del bucle (`sprPosBias`) | `g_shakeX/Y` son globales: el compilador las relee en cada vuelta porque no puede descartar el aliasing | `S_PARTS` 7,1 % → 6,6 % |
| Apuntado con la mejor distancia arrastrada | `Enemy_closest` y `Snake_closest` buscaban su mínimo por separado, así que los 52 eslabones se recorrían siempre | 42,8 → 43,0 fps |
| Quitar el contador de FPS/CPU de pantalla | Es depuración | 39,6 → 41,9 fps |

Y una que se probó y **se descartó por la medida**: partir la caja envolvente
del gusano en dos mitades para afinar el descarte. Salió peor (41,8 frente a
43,0 fps): mantener y comprobar dos cajas cuesta más de lo que ahorra saltarse
seis eslabones. Queda anotada en el código para que nadie la vuelva a intentar.

### Dónde está ahora

| | Antes de esta pasada | Ahora |
|---|---|---|
| Partida normal | 50 fps, 19-34 % de CPU | **49,5 fps, 37 % de pico** |
| Caso peor (arena a los topes del port, jugador inmortal, munición infinita) | 24 fps, 104 % | **43,1 fps, 78 % de media, 117 % de pico** |
| Objetos Sprite | 79 de 80, sin sitio para nada más | 143 de 160 ranuras, 51 entradas de hardware en el caso peor |
| Tiles de sprite en VRAM | 577 | 727 |

El caso peor sigue sin llegar a 50 fps: la media es del 78 % pero los frames en
los que están vivos los cuatro gusanos a la vez se van al 117 %, y ahí se cae
uno de cada seis. **Medido: cada gusano de más cuesta un 20 % del frame**
(con 2 + 1 gusanos en vez de 3 + 1 la medida sube a 47,8 fps con un pico del
68 %). O sea que la palanca que queda es la que ya marcaba la decisión 4 del
proyecto —recortar `MAX_CABEZA_1`/`MAX_CABEZA_2`, nunca la longitud—, y por eso
no se toca sin decirlo: es cambiar fidelidad por framerate.

Lo siguiente, por orden de lo que daría:

1. **`MAX_CABEZA_1` de 3 a 2** (de 4 gusanos a 3): 43,1 → 47,8 fps medidos. Es
   una decisión de diseño, no una optimización.
2. **El montador de la tabla de sprites en ensamblador** (~10 % del frame en el
   caso peor): el código que genera GCC ya es ajustado, pero por sprite visible
   son unos 250 ciclos y a mano caben en ~130.
3. **El bucle de eslabones en ensamblador** (`S_CACHE` + `S_PARTS`, ~14 %).

## Revisión de los sprites en pantalla: por qué se veían mal

Mirando el juego a 3 aumentos salieron tres cosas, y ninguna era la que
parecía. Lo primero fue descartar la sospecha obvia: **no se estaban
perdiendo sprites**. Volcando la tabla del VDP en el frame más cargado y
contando por línea de barrido, la peor línea tenía 11 sprites y 192 px, muy
por debajo de los topes del hardware (20 sprites y 320 px por línea). Lo que
se veía mal se veía mal por el arte, no por el VDP.

### 1. El umbral de alfa se estaba comiendo medio bicho

`to_indices()` descartaba todo píxel con alfa < 128. Suena razonable, pero el
arte viene de un room de 1600x1200 y se reduce **5:1**: un detalle de un píxel
del original queda en alfa 51 y desaparecía. Medido frame a frame:

| Sprite | Tamaño del original a x0,2 | Lo que se dibujaba |
|---|---|---|
| Nave (`sprite173`) | 8x12 | **8x8**, 37 píxeles |
| Araña (`sprite185`) | 13x13 | **7x8**, 34 píxeles |
| Cabeza de gusano | 13x13 | 12x10 |

La araña se estaba dibujando con **la mitad de su silueta**. Con el umbral a
48 (19 % de cobertura) la araña pasa de 1892 a 4444 píxeles opacos en la hoja
y recupera su tamaño. Es el ajuste que más cambia cómo se ve el juego.

### 2. La transparencia del original, horneada en la paleta

Esta es la técnica que arregla lo de "se comen unos a otros". El campo de
juego es un **color plano** (la capa Compatibility_Colour del room, un
(109,146,146)), y eso permite algo que normalmente no se puede en Mega Drive:
el VDP no mezcla, pero si el fondo es siempre el mismo se puede calcular el
color final EXACTO de cada efecto translúcido y meterlo ya mezclado en la
paleta. Sale idéntico al original mientras el efecto esté sobre el fondo, y
no cuesta ni un ciclo en ejecución.

Antes, todo lo que el original dibuja con alfa se estaba portando como
**opaco**, y por eso tapaba:

| Objeto | Cómo lo dibuja el original | Color real en pantalla | Se portaba como |
|---|---|---|---|
| `oca` (humareda) | `expl_4` + `expl_1`, los dos a **alfa 0,8**, sobre arte que ya es gris oscuro translúcido | (51,59,59) a (66,73,73) | gris (73,73,73) plano |
| `oDestro` (metralla) | `sprDebri`, blanco a **alfa 0,5**, tintado de gris oscuro o negro | dos grises medios | negro puro |
| `object1` (haz del PARA) | `sprite0` a **alfa 0,55**, tintado | azul y rojo apagados | azul y rojo saturados |
| `oExplosion` (fogonazo) | `sprExplosion`, que es gris 239 con **0,82 de alfa** | (219,219,219) | blanco puro |

La paleta de sprites pasa de 7 colores a 12, con los cinco tonos mezclados
calculados componiendo el arte del original con su alfa sobre el fondo y
cuantizando al paso de 3 bits del VDP. La humareda deja de ser un disco gris
que tapa la pantalla y pasa a ser lo que es en el original: un velo oscuro
sobre el que se siguen viendo la nave y los enemigos.

De paso, al añadir azul y rojo a la paleta se arreglaron tres armas que se
estaban pintando del color equivocado: **`sprLaser` es rojo** (180 píxeles
rojos de 192 en el original), **`spr3way` es rojo y blanco** y
**`sBulletPara` es azul** (0,0,192). Se estaban portando en blanco y negro.

### 3. El orden de la tabla de sprites ya era el bueno

En el VDP la prioridad la da el orden en la tabla: la entrada más temprana se
dibuja encima. El reparto del pool es jugador → arañas → gusanos →
proyectiles → haz → explosiones → metralla → números, así que la nave queda
siempre por delante de la humareda y los efectos son lo primero que se cae si
alguna vez se llegara a las 80 entradas. Coincide con el original, donde los
agujeros van a `depth = 999` (al fondo) y la humareda a `depth = choose(0,-1)`.

## Segundo recorte de gusanos, y el resultado final

Con el motor de sprites propio el tope de 80 objetos dejó de existir, así que
la única razón para limitar los gusanos pasó a ser la CPU. Medido: **cada
gusano cuesta un 20 % del frame** (trece posiciones que leer del historial,
trece sprites que colocar y trece cajas más en cada consulta de colisión).

`MAX_CABEZA_1` baja de 3 a 2, o sea 2 + 1 = **3 gusanos a la vez** en vez de
los 6 del original. Es el segundo recorte de este valor y el primero por
tiempo de CPU y no por sprites; la longitud de la cadena (13 y 11 eslabones)
no se toca, conforme a la decisión 4 del proyecto.

| | Al empezar la FASE 10 | Antes de esta pasada | Ahora |
|---|---|---|---|
| Partida normal | 50 fps, 39-78 % | 49,5 fps, 37 % de pico | **50,0 fps, 15 % de pico** |
| Caso peor (arena a los topes, jugador inmortal, munición infinita) | 25 fps, 190 % | 43,1 fps, 117 % de pico | **48,4 fps, 59 % de media, 71 % de pico, cero frames por encima del presupuesto** |
| Sprites por línea en el peor frame | — | — | 11 de 20, 192 px de 320 |
| Tiles de sprite en VRAM | 577 | 727 | 1174 de 1440 |

Lo que queda de los 48,4 fps a los 50 son unos ocho frames de cada 500, y no
es carga sostenida: el perfilador no ve ni un frame por encima del 100 %, así
que son sucesos sueltos (el nacimiento de un gusano rellena 128 posiciones de
historial de golpe, y la generación de paquetes escribe tiles al plano).

## Cómo empieza el original, y los agujeros que no eran agujeros

Dos cosas que se veían mal de verdad y que no eran lo que parecían.

### El port empezaba jugando; el original no

`Game_init()` terminaba con `g_state = GAME_PLAY; Game_start();`, o sea que la
máquina arrancaba con una partida en marcha. El original no hace eso: el room
`_1` trae **ya colocados** un `oMenu` y un `oSplashS`, y `global.started` es
`false`. Lo primero que se ve es la **pantalla de título sobre los cinco
agujeros**, con el contador de créditos, esperando a que metas uno. La partida
sólo arranca cuando `oMenu` ve el botón de disparo habiendo crédito.

Arreglado: `Game_init()` pinta los agujeros, deja al jugador aparcado y entra
en `GAME_SPLASH`. La secuencia de estados comprobada en el emulador es ahora
**título → partida → interludio → partida**, que es la del original.

Lo que no se porta de ese arranque: `oSplashS` dibuja `sprite176` a escala 18
encogiendo y girando dos grados por frame hasta escala 1 (unos 15 frames de un
logo gigante dando vueltas). El VDP no escala ni rota planos, así que el logo
va como bloque de tiles fijo. Anotado.

### Los paquetes parecían agujeros

Esto explica el "el número de agujeros no coincide". En pantalla había cinco
agujeros **y hasta ocho manchas oscuras más**, todas iguales. No eran
agujeros: eran los paquetes de munición y los power-ups.

Todo lo que espera en el suelo se dibuja en el plano A con la paleta PAL2, y
esa paleta tenía cuatro grises oscuros, heredados de cuando sólo llevaba los
agujeros. Leído el arte del original:

| Objeto | Color real en el original | Se portaba como |
|---|---|---|
| `oPaquete` (`sprPaquete`) | **cian** (0,255,255) con reborde casi blanco | gris oscuro |
| `oBoomPowUp` (`sprCruz`) | casi blanco (232) | gris oscuro |
| `oNaveReplicaPowUp` (`sprWeapon4`) | **azul** (0,0,255) | gris oscuro |
| `oRayosPowUp` (`sprWeapon7`) | **azul y amarillo** | gris oscuro |
| Barra del interludio | `c_gray` = (128,128,128) | gris casi negro |

La paleta del plano pasa de 4 a 14 colores y cada objeto recupera el suyo. Los
paquetes ahora son puntos cian que se distinguen de un vistazo, y en pantalla
se cuentan **cinco agujeros**, los cinco `oAgujerSuper` del room (verificado
contando manchas conexas en la captura).

### Y los agujeros, con su borde

De paso, el agujero se indexaba binarizando el arte, que deja un disco negro
recortado. En el original `sprAgujero` es casi negro **con alfa propia de 0,6
de media**, tintado con `c_gray` (que en GameMaker MULTIPLICA, no sustituye):
los goterones del borde apenas tapan. Compuesto sobre el fondo del room, el
agujero recupera el degradado del borde con los cuatro tonos oscuros de la
paleta en vez de uno solo.

Las posiciones están bien: el origen de `sprAgujero` es (99,85) de 197x197, o
sea que el dibujo cae muy por debajo del origen, y eso es lo que hace que el
disco se vea unos 11 px más abajo que el punto donde nacen los enemigos. Es
así en el original.

## Revisión de calidad para publicación

Pasada de verificación sobre todo lo anterior, buscando defectos en vez de
huecos conocidos. Salieron seis, tres de ellos de los que no se ven hasta que
alguien se pone a comparar con el original línea a línea.

### El cartucho se llamaba SAMPLE PROGRAM

`src/rom_header.c` era la plantilla de SGDK sin tocar. SGDK la copia sólo si
el fichero no existe (es una dependencia *order-only* en `makefile.gen`), así
que se puede editar y no se pisa — pero nadie la había editado. El ROM
declaraba:

- título **"SAMPLE PROGRAM"** y notas **"DEMONSTRATION PROGRAM"**, que es lo
  que verían el menú de un flashcart y cualquier base de datos de ROMs;
- `rom_end` = 0x0FFFFF, o sea 1 MB, cuando el ROM son 896 KB;
- **SRAM declarada** ("RA", 0xF820, 0x200000-0x20FFFF) sin usarla, lo que hace
  que algunos flashcarts reserven un fichero de guardado fantasma;
- `IOSupport` "JD", y la D no es un código válido.

Corregido todo: SURFIZE, `rom_end` exacto, sin SRAM (el original tampoco
guarda: `scrWrite()` está comentado y el récord se pierde al apagar, como en
una recreativa) y "J" de mando de tres botones. Y como `rom_end` se escribe a
mano y se quedaría atrás en cuanto el ROM crezca, lo comprueba
**`tools/check_header.py`** en cada build.

### El desbloqueo de armas iba una kill por delante

`stCheckWeapon()` tiene un desfase que es fácil leer mal:

```gml
for (g = 0; g < global.maxKills; g += 1) {
    if (g >= laserEnabled) { ...añade laser...; nextWeapon = threewayEnabled }
```

El mayor valor que toma `g` es `maxKills - 1`, así que `g >= 1` pide
**maxKills >= 2**. Los umbrales de verdad son 2, 3, 4 y 5 kills, no 1, 2, 3 y
4: el port regalaba el láser con la primera muerte. Y `nextWeapon` es una
globalvar que arranca en **999999** y que sólo se toca dentro de ese bucle,
así que con 0 o 1 kills de récord el menú enseña "NEXT 999999". Es así en el
original y así se porta; si se prefiere que enseñe el umbral que falta, es
una línea en `checkWeapon()`.

Comprobado en el emulador, récord por récord: 1 kill → 1 arma / NEXT 999999;
2 → 2 armas / NEXT 2; 3 → 3 / NEXT 3; 5 → 5 / NEXT 0. Igual que la tabla que
sale de leer el GML.

### Los paquetes recortaban un agujero del fondo

En el plano de tiles el color 0 **no es transparente: pinta el color de
fondo**. Los objetos del suelo iban al plano A, así que un paquete encima de
un agujero le recortaba un rectángulo limpio de 16x16 (se ve en la captura de
antes y después). Y al cogerlo, el borrado dejaba el mordisco permanente.

Arreglado pasándolos a **sprites**, donde la transparencia sí es
transparencia. De paso sale gratis el **latido** que tienen en el original y
que no estaba portado (`oPaquete/Draw_0`: `t += 0.05` rebotando entre 0,8 y
1,4; los otros tres igual, aunque su giro no se porta). Coste: 28 ranuras de
sprite y unos 6 puntos de CPU en el caso peor.

### El rayo no se veía hasta que mataba

`oRay/Draw_0` dibuja, mientras arde la mecha, **tres descargas del jugador a
cada uno de los cinco agujeros**, todas del mismo color y cambiándolo cada
frame entre azul, plata y amarillo. En el port eran 25 frames de nada seguidos
de una pantalla que se vacía de golpe. Ahora hay una descarga de puntos por
agujero, con el mismo cambio de color por frame. Lo que no se porta son las
descargas cortas que el original pinta además en cada enemigo
(`with oMalo { draw_curve(...) }`): serían hasta 60 cadenas más.

### Faltaba el marco de los iconos del menú

`oMenu/Draw_0` dibuja **dos** sprites por arma en el mismo sitio: el icono y
encima `sprFrame`, un marco. El port sólo pintaba el icono. Como los dos van
juntos y no se mueven, ahora se componen en el mismo bloque de tiles: el marco
sale gratis. (Lo que sigue sin portarse es el `sprite191` animado que el
original pone sobre el arma seleccionada; aquí se marca con un cursor debajo.)

### Y dos detalles más

- **Los satélites del PIVOT parpadean**: `oOption/Draw_0` los pinta con
  `alpha = random(2)`, o sea que la mitad de los frames salen translúcidos o
  invisibles. Sin alfa, lo más parecido es enseñarlos o esconderlos a cara o
  cruz, que es lo que hacen ahora.
- **El texto de la pantalla de título** era inventado. El del original es
  `" PRESS SPACE TO START ... CURSORS TO MOVE ... SHOOT ALL THE BUGS AND AND
  SNAKES ...SURVIVE FOR GLORY... GAME BY REDEFINETECLAS 2012 "`. Ahora es ese,
  literal, cambiando sólo SPACE y CURSORS por los controles de Mega Drive. El
  "AND AND" es del original y se queda.
- **El logo del título** estaba pegado arriba. `oMenu` está en (832, 608) del
  room, o sea en el centro, y su sprite se dibuja ahí: ahora va centrado entre
  los agujeros, como en el original.

### El reparto de VRAM estaba escrito a mano, y se rompió

Al pasar los objetos del suelo a sprites y crecer los iconos del menú de 3x3
a 4x4 tiles, **la nave desapareció de la pantalla**. No era el motor de
sprites: cada módulo tenía su índice base de tiles escrito a mano y
`VSPR_TILE_BASE` era "TILE_USER_INDEX + 176", la suma de todos ellos copiada
a ojo. Con el reparto nuevo el logo del menú se cargaba encima de los tiles
del jugador, así que el sprite seguía en la tabla del VDP pero apuntando al
dibujo de otro. Es un fallo mudo: no se cuelga, no avisa, sólo deja de verse
una cosa.

Arreglado en la raíz: los índices se reparten **en ejecución** y por orden de
carga (`VTile_alloc()` en `inc/vramspr.h`). Ya no hay ningún número que
mantener sincronizado, y el presupuesto se lee con `VTile_used()`.

### Un aviso que se iba a acabar ignorando

`tools/check_codegen.py` empezó a dar tres falsos positivos en cuanto entró la
hoja de sprites nueva: `objdump -d` desensambla la sección entera y los datos
de rescomp viven ahí dentro, así que esos bytes leídos como instrucciones
disparaban el patrón. Un aviso falso por build es un aviso que nadie vuelve a
mirar. Ahora filtra los blobs de rescomp y dice cuántas funciones ha revisado
de verdad (1042).

### Dónde queda el port

| | Antes de esta revisión | Ahora |
|---|---|---|
| Partida normal | 50,0 fps, 15 % de pico | **50,0 fps, 18 % de pico** |
| Arena a tope | 48,4 fps, 59 % de media | **47,1 fps, 69,5 % de media, 86 % de pico** |
| Frames por encima del presupuesto | 0 | **0** |
| Tabla de sprites del VDP | 39 de 80 | 56 de 80 |
| Ranuras de sprite | 143 | 188 de 224 |
| Tiles en VRAM | 1174 | 1236 de 1440 |

El caso peor pierde 1,3 fps y gana el latido de los paquetes, su transparencia
y los relámpagos del rayo. Sigue sin un solo frame por encima del presupuesto.

## El sexto agujero, y las dos armas que rompían la partida

Dos avisos del autor, los dos ciertos, y el segundo tapaba dos fallos
distintos.

### El sexto agujero

El room `_1` de `surfize_changes` trae **cinco** instancias de
`oAgujerSuper`. Verificado de tres formas: `grep -c` sobre el `.yy` (5), el
parseo de las capas (5) y `instanceCreationOrder` (10 instancias en total, de
las que 5 son agujeros). Nada las crea en ejecución.

Pero puestas como anillo alrededor del centro del room, cantan:

```
      24.8 ->  83.7 :  58.8 grados
      83.7 -> 212.5 : 128.8 grados   <-- falta uno
     212.5 -> 273.5 :  61.0 grados
     273.5 -> 328.6 :  55.1 grados
     328.6 ->  24.8 :  56.3 grados
```

Cuatro separaciones de 55-61 grados y **una de 129**. El que falta cae a 148
grados y radio 288, o sea (556, 752), que es prácticamente el espejo de
(558, 446) respecto del centro: **(558, 754)**. En el room que hay aquí esa
instancia no está —se borraría en algún momento, o la versión publicada es
otra—, pero el diseño pide seis y con seis el anillo queda regular.

Añadido, y **marcado como reconstrucción**: es la única coordenada de todo el
port que no sale del GML. Va la primera de la tabla a propósito, porque el
único comportamiento determinista que depende del orden es dónde nacen las
arañas (el original coge el ÚLTIMO agujero) y así ese sigue siendo el mismo.
Las tres fórmulas que dependen del número —nacimiento de araña, de gusano y
destino del estado de vagar— pasan a derivarse de `HOLE_COUNT` en vez de
llevar el 4 escrito a mano, así que cambiar el número vuelve a ser una línea.

### El PIVOT dejaba al jugador sin disparo

Este rompía la partida del todo. En `oPlayer/Step_0`, el `switch` de las armas
puede no crear nada —el PIVOT no hace nada si el pivote ya está puesto, el
PARA si ya hay una bala viva— pero **la munición y la cadencia se gastan
igual**, porque están FUERA del switch:

```gml
switch (global.weapon_index) { ... case 3: if !instance_exists(oPivot) {...} break; }
if (global.weapon_index == 2) controlTime = 12 else controlTime = 6;
if global.playerBulletsCounter >= 0 {global.playerBulletsCounter -= 1}
```

El port lo tenía condicionado a que el arma hubiera creado algo. Con el PIVOT
eso significa que **no se gastaba munición nunca**, la munición no llegaba a
cero y por tanto no entraba la bala por defecto. En cuanto los tres satélites
morían contra un enemigo —y mueren al primer contacto, también contra un
eslabón de gusano— el jugador se quedaba **sin ningún disparo**: no podía
matar, no moría ninguna cabeza, y como las oleadas sólo salen de
`checkMorte()`, dejaban de aparecer enemigos para siempre.

### El haz del PARA mataba en silencio

Y este era peor, porque parecía que el juego simplemente se vaciaba. Medido
con el arnés: 500 frames con el PARA puesto, **cero kills nuevos y cero
arañas**.

`object1/Collision_oMalo` hace `with other if hp != undefined hp -= 1` y se
destruye. Nada más: **no pasa por `checkMorte()`**, y el manejador del enemigo
tampoco corre, porque `object1` no desciende de `oBulletS`. O sea que el haz
ABLANDA pero no mata; el enemigo se queda con el hp en negativo hasta que le
entra una bala, y es esa la que dispara la muerte.

El port usaba `Enemy_hit()`, que además de restar comprueba la muerte y
destruye. Como la araña tiene `hp = 0`, el haz la borraba de un roce, sin
sumar kill y sin pasar por `checkMorte()`. Con el PARA puesto el haz iba
limpiando la arena en silencio y las oleadas no volvían a salir nunca.

Arreglado con `Enemy_damageNoKill()` y `Snake_damageNoKill()`, que es lo que
el original hace de verdad.

### Comprobado arma por arma

Con el arnés de arena llena, munición la de un paquete y 500 frames de juego
sostenido:

| Arma | kills en 500 frames | arañas vivas al final |
|---|---|---|
| WBLOCK | +19 | 1 |
| LASER | +17 | 6 |
| 3WAY | +34 | 2 |
| PIVOT | +13 | 2 |
| PARA | +12 | 3 |
| RAY | +7 | 2 |

Ninguna se para. Antes del arreglo, PIVOT y PARA daban +0 con la arena vacía.

### Una red para el pool de sprites

De paso: `Spr_alloc()` devolvía `NULL` al agotarse el pool, y los módulos
guardan ese puntero y escriben la posición sin mirarlo. Un `NULL` ahí acaba
escribiendo en la memoria baja del 68000. Ahora devuelve una ranura
**sumidero** que el montador no recorre: lo peor que puede pasar es que ese
sprite no se vea. En una build publicada esa diferencia importa.

## Firma, metralla y un fondo de contraste

### La firma

La marquesina del GML dice "GAME BY REDEFINETECLAS 2012". El autor ha
confirmado que el nombre bueno es **Screen Implosion**, así que es el que va:
en el texto del splash y en la cabecera del cartucho (`(C)SCIM 2026.NOV` y
"SCREEN IMPLOSION 2012 - MEGA DRIVE PORT" en las notas). Es la única cadena
del port que se aparta a propósito del literal del GML, y se aparta porque la
atribución la decide quien firma, no el fichero viejo.

### La metralla, como estaba pensada

`oEstallido/Create_0` tiene la versión buena **comentada al lado** de la que
quedó:

```gml
//optimize
// for(i=0;i<360;i+=random(60)){
for(i=0;i<360;i+=30){
    b = instance_create(x,y,oDestro)
    b.speed = 10//random_range(4,64)
    b.direction = i
}
```

O sea: paso angular aleatorio y velocidad aleatoria por astilla, las dos cosas
apagadas por rendimiento en GameMaker. Aquí se recuperan, que es lo que hace
que la ráfaga no salga nunca igual:

- **paso angular** `random(60)`, acotado por abajo a 15 grados. Con el
  `random(60)` puro el paso puede salir casi cero y la ráfaga pediría cientos
  de astillas (por eso lo comentaron). Entre 15 y 45 salen de 8 a 24 astillas,
  12 de media, que es justo lo que daba el paso fijo de 30;
- **velocidad por astilla** `random_range(4, 64)`, o sea de 0,96 a 15,4 px por
  frame: unas se quedan dando vueltas y otras cruzan la pantalla;
- **ángulo de arranque** al azar, para que el abanico no caiga siempre en los
  mismos rumbos.

Y más a menudo, que es lo que se pedía: **la ráfaga sale ahora en toda muerte
causada por un disparo**, no sólo cuando un WBLOCK o un satélite daban en un
eslabón de gusano. Eso es una desviación del GML, pedida a propósito. No se
dispara desde las muertes que causa la propia metralla, para que no se
realimente sola.

El pool sube de 24 a 32 astillas. Y como con 32 puntos preguntando por 8
arañas y 3 gusanos en cada frame el solape caja-contra-caja se convertía en lo
más caro del juego, la metralla pasa a usar una colisión de **punto** con el
rechazo en X resuelto en una sola comparación sin signo
(`(u16)(x - caja) < ancho`): el caso peor recupera 1,1 fps de los 2 que costó
la metralla nueva.

| | Antes | Con la metralla nueva | Con la colisión de punto |
|---|---|---|---|
| Partida normal | 50,0 fps, 18 % de pico | 50,0 fps, 29 % | **50,0 fps, 29 %** |
| Caso peor | 46,1 fps | 43,9 fps | **45,0 fps** |
| Frames por encima | 3 de 460 | 17 de 440 | **7 de 450** |

### Un fondo de contraste para mirar los sprites

`PORT_DEBUG_BG` cambia el color de fondo para poder identificar qué es cada
cosa sin que el fondo del juego se coma las siluetas:

```
make EXTRA_FLAGS="... -DPORT_DEBUG_BG=1"   magenta (ningún sprite lo usa)
                       -DPORT_DEBUG_BG=2   negro
                       -DPORT_DEBUG_BG=3   blanco
```

No entra en la ROM que se distribuye. **Ojo al mirarlo**: los efectos
translúcidos —humareda, metralla, haz del PARA, agujeros— llevan su color ya
mezclado con el fondo de verdad, porque el VDP no mezcla y el campo es un
color plano. Con otro fondo esos tonos dejan de cuadrar y se ven verdosos
sobre el magenta. Eso no es un fallo: es exactamente la técnica.

## Las capturas del juego publicado: el .yyp no es la versión que salió

El autor pasó tres GIFs del Surfize que se publicó. Comparadas con lo que
produce `surfize_changes`, **no son el mismo juego**. Las diferencias no son
de interpretación mía: están en el histograma.

| | `surfize_changes` (el .yyp) | Las capturas |
|---|---|---|
| Fondo | teal (117,154,154), capa Compatibility_Colour | **gris oscuro (51,51,51)** con una textura de grietas a 7 niveles |
| Agujeros | todos grises/negros; la línea que pone `gus = 1` está comentada | **el agujero por el que sale un gusano va con reborde y goterones rojos** |
| Réplicas | — | **cuatro a la vez** |

Manda la captura, que es lo que el autor llama "el original". Lo que se ha
hecho con eso:

### El fondo, y con él toda la paleta

`ROOM_BG` pasa de (109,146,146) a **(51,51,51)**, que en la rejilla de 3 bits
del VDP es (36,36,36). Y como en este port los efectos translúcidos llevan su
color YA MEZCLADO con el fondo, cambiarlo obliga a rehacer la paleta entera:
los tonos nuevos salen del **histograma de las capturas**, cuantizados a los
ocho niveles por canal:

```
(51,51,51)    723003 px  fondo
(255,255,255)  58912     HUD, nave, oWBlock
(7,7,7)        48991     agujeros, arañas, gusanos
(0,255,255)    11054     paquetes
(84,82,82)      6674     humareda
(79,21,21)      4143     borde del agujero activo
(128,0,0)       3732     rojo del agujero activo
(0,0,253)       2591     líneas a las réplicas
(147,147,148)   1757     humareda clara
(255,255,0)      548     oBulletThin
```

La textura de grietas del fondo NO se porta: son siete niveles de 255 de
contraste, muy por debajo del paso de 36 que da el VDP. O no se vería, o se
vería como una mancha.

### El HUD, al tamaño que tiene

`oInGameHUD` pinta BULLETS y KILLS con `global.myFont`, que es
`font_add_sprite(sFont2, ...)`: glifos de 80x80 del room, o sea **16 px en
pantalla**. Medido sobre las capturas: 36 px de alto en 800x600, que son 14,4
px en 320x240. El port los pintaba con la fuente de 8 px del sistema, **la
mitad**, y el HUD es de lo primero que se ve.

Ahora van con los glifos de `sFont2` en bloques de 2x2 tiles. Sólo los 19
caracteres que el HUD usa de verdad (BULETSKIA y las diez cifras): la fuente
entera no cabría en VRAM. LAST se queda con la fuente del sistema a
propósito, porque el original lo pinta con `myFont2` (sFont3, 4 px), aún más
pequeño.

### Los agujeros rojos

`oAgujerSuper/Draw_0` tiene, antes del agujero normal:

```gml
if gus draw_sprite_ext(sprite_index, image_index, x + choose(-8,8), ...,
                       choose(c_white,c_red,c_yellow), 0.8)
```

una copia del mismo agujero, desplazada y tintada, asomando por detrás. En el
.yyp la línea que pone `gus = 1` (`if place_meeting(x,y,oCabeza1)`) está
comentada; en las capturas el efecto está clarísimo. Portado: el agujero que
tiene una cabeza de gusano encima se pinta con una segunda variante de tiles,
con reborde y goterones rojos.

### Y el tope de réplicas, que no existía

`MAX_TRI` estaba en 2 porque leí mal: el `instance_number(oNaveReplicaPowUp) < 2`
de `generaPaquete` es el tope de POWER-UPS en el suelo, no de `oTri`.
`oPlayer/Collision_oNaveReplicaPowUp` crea uno cada vez sin mirar cuántos hay,
y en las capturas se ven cuatro. Subido a 4, que es lo que da el pool de
proyectiles que comparten.

### Lo que costó, y de dónde se recuperó

Añadir todo eso dejó el caso peor en **37,5 fps**, desde 45,0. Y no era por
lo que parecía: eran dos repintados del PLANO, que son accesos al puerto del
VDP uno por tile.

| Causa | Arreglo | Medido |
|---|---|---|
| El HUD borraba las dos filas enteras (80 tiles) y repintaba los 16 glifos cada vez que cambiaba la munición, o sea cada disparo | Campos de ancho fijo, sin borrar; y las palabras se pintan una vez, porque no cambian: sólo se tocan las cuatro cifras | 37,5 → **41,0 fps** |
| El agujero rojo repintaba sus 64 tiles casi cada frame: las cabezas van justo hacia los agujeros y el estado oscilaba en el borde | Rebote de 6 frames antes de cambiar | incluido arriba |

| | Antes de las capturas | Ahora |
|---|---|---|
| Partida normal | 50,0 fps, 29 % de pico | **50,0 fps, 39 % de pico** |
| Caso peor | 45,0 fps | **41,0 fps** |
| Tiles en VRAM | 1254 | 1394 de 1440 |

El caso peor paga 4 fps por el HUD grande, el sexto agujero, su variante
roja, las cuatro réplicas y la metralla nueva. La partida normal no se entera.
La palanca que queda sigue siendo la misma y sigue medida: el montador de la
tabla de sprites está en el 14,7 % del frame y a mano cabría en la mitad.

## El montador de sprites en ensamblador, y por qué rindió la mitad de lo previsto

El perfilador lo tenía fichado desde hacía rato: montar la tabla de sprites
del VDP se llevaba el **14,4 % del frame** en el caso peor, el segundo gasto
después del gusano. Escrito a mano en 68000, `src/spr_a.s`.

### Lo que se hizo

Primero se midió lo que ya generaba GCC, que es lo que manda. El camino del
sprite visible eran unos **192 ciclos**, y casi todo accesos sueltos a
memoria: cinco lecturas del struct (y, parts, size, attr, x) y cuatro
escrituras de palabra a la tabla.

La idea no fue "reescribirlo en ensamblador" sino **reordenar el dato**: los
ocho primeros bytes de `MDSpr` son ahora, byte a byte, una entrada del VDP
(`y, size_link, attribut, x`). Con eso la entrada se copia de un tirón:

```
    movem.l (%a0),%d2-%d3       /* y|sizeLink y attr|x, de golpe */
    addq.w  #1,%d0
    or.w    %d0,%d2             /* el enlace a la siguiente */
    movem.l %d2-%d3,(%a1)
```

El camino del sprite visible baja a **146 ciclos**, un 24 % menos.

### Cómo se comprobó que hace lo mismo

Esto es lo que no se puede dar por bueno de palabra: una tabla de sprites mal
montada no se cuelga, se ve raro y a lo mejor sólo en un caso. La versión en C
se queda en el repo como **implementación de referencia**, detrás de
`PORT_SPR_C=1`, y las dos se comparan volcando la tabla del VDP con el
comando `vs` del depurador en la misma escena, con la semilla fija.

Hay un detalle que costó un intento: en el arnés de arena llena los dos builds
corren a fps distinto, así que a los mismos frames de EMULADOR no van por el
mismo frame de JUEGO y las escenas divergen. La comparación hay que hacerla en
partida normal, donde los dos van clavados a 50.

Resultado: **tablas idénticas en los cuatro volcados**, 24 entradas, y entre
ellas tamaños irregulares (16x8, 24x8, 8x24) que sólo salen de la rama de
frames partidos en varios sprites de hardware, o sea que esa rama también
queda comprobada.

### Lo que rindió, y por qué menos de lo dicho

| | En C | En ensamblador |
|---|---|---|
| Etapa SPRFLUSH | 14,4 % del frame | **13,5 %** |
| Caso peor | 39,8 fps | **40,3 fps** |

Yo había dicho "a mano cabría en la mitad". **No era verdad, y conviene dejar
escrito por qué**: de esos 20.700 ciclos, unos 8.000 no son montar nada, son
RECORRER las ranuras aparcadas. Hay 214 ranuras repartidas y sólo unas 66
acaban en la tabla; las otras 148 cuestan 44 ciclos cada una sólo por
comprobar que están aparcadas. El ensamblador mejora el camino del sprite que
se ve, que es el 60 % del gasto, y ahí sí da el 24 % prometido; sobre el total
eso es un 6 %.

Lo que de verdad queda por atacar en esa etapa no es el código, es el
recorrido: 97 de esas ranuras (los puntos del rayo, los de las líneas, el haz,
los números) están aparcadas casi siempre. Saltárselas pide un índice de
"hasta aquí hay algo vivo", y no vale el truco fácil de marcarlo desde
`sprPos`, porque hay sprites que se quedan quietos varios frames sin que nadie
los recoloque (los paquetes del suelo) y desaparecerían.

## Las franjas del interludio y el arma de las bolas

### Las franjas eran el texto agujereando la barra

El interludio salía con bandas grises a rayas en vez de una barra. Dos causas,
y las dos son la misma cosa mal entendida: **en un plano el color 0 no es
transparente, pinta el fondo**.

El original hace `draw_line_width_color(0, ..., t, c_gray, c_gray)`: una sola
línea gruesa, y el texto se escribe ENCIMA. Aquí el texto son tiles del plano
A, así que cada letra abría un agujero oscuro en la barra; y encima el port se
saltaba a propósito las filas del texto "para no taparlo", que es lo que
remataba el efecto de rayas.

Arreglado poniendo la barra en el **plano B**, que va detrás: el hueco de cada
letra deja ver la barra y queda la banda entera con el texto encima, como en
el original. El plano B lo usa también la marquesina, que lo desplaza en
horizontal, pero da igual: una banda que ocupa la fila entera se ve igual esté
donde esté el scroll.

### El arma de las bolas sólo salía una vez

`oPivot` son tres bolas (`oOption`) orbitando al jugador. En el port aparecían
una vez y no volvían en toda la partida. La respuesta está en la **primera
línea** de `oPivot/Step_0`, que no estaba portada:

```gml
if instance_number(oOption) < 1 instance_destroy()
```

**Cuando cae la última bola, el pivote se mata a sí mismo.** Y eso es lo que
hace que el arma vuelva: el disparo siguiente encuentra que ya no hay `oPivot`
y crea uno nuevo con sus tres bolas. En el port el pivote vivía para siempre,
así que las tres bolas morían al primer contacto —también contra un eslabón de
gusano, que es facilísimo— y no salían más.

Y un segundo fallo en el mismo objeto: `oPivot/Step_0` llama a
`stPlayerMovement()`, o sea que **el pivote hace el mismo wrap de pantalla que
la nave**. El port no se lo hacía, así que en cuanto la nave cruzaba un borde
y aparecía por el otro lado, el pivote se quedaba atrás y las bolas orbitaban
un punto cualquiera lejos del jugador.

Comprobado con el arnés, muestreando el pivote cada 25 frames con el arma
puesta: `#####..........###` — vive, cae, y vuelve. Las tres bolas orbitando
confirmadas en pantalla.

## El fondo de bosque: una imagen de 1200 celdas en 168 tiles

**Esto no sale del original.** El campo de `surfize` es un color plano (ver la
sección del fondo más arriba). El autor ha pasado una imagen de pixel art de un
claro de bosque de 1448x1086 y ha pedido verla puesta en el juego. Queda
anotada aquí como divergencia deliberada: el interruptor para quitarla es
`PORT_NO_BG=1`, que devuelve el campo al color plano.

### Por qué no basta con convertir la imagen

320x240 son 40x30 = **1200 celdas de 8x8**. Guardarlas todas son 38.400 bytes,
y la VRAM entera son 65.536 con los mapas de plano dentro.

`rescomp` deduplica tiles idénticos y sus volteos, que es lo que funciona con
arte hecho de tileset. Con una textura orgánica no sirve de nada: **medido,
las 1200 celdas dan 1200 tiles distintos, cero repeticiones**, con cualquier
filtro de reducción y con o sin difuminado de color.

### El diccionario con tolerancia

`tools/bg2md.py` recorre las celdas en orden y cada una busca la entrada más
parecida de las que ya hay en el diccionario, probando las cuatro
orientaciones que el VDP dibuja gratis (normal, volteo H, volteo V y los dos).
Si el error cuadrático en RGB queda por debajo de un umbral, la celda reutiliza
esa entrada y se queda con los bits de volteo; si no, se da de alta. El umbral
se busca por bisección hasta que el diccionario cabe justo en el presupuesto.

Lo importante es que **el mapa de 40x30 sigue siendo único**: la composición
(los árboles del borde, las piedras, los claros de tierra) se conserva aunque
el diccionario sea pequeño. Lo que se degrada es la textura fina de la hierba,
que es ruido y perdona.

| Diccionario | RMSE por canal | Aspecto |
|---|---|---|
| 1200 (sin tocar) | 0 | no cabe ni de lejos |
| 639 | 8,5 | indistinguible del original reducido |
| 440 | 11,9 | muy bueno |
| 168 | 17,3 | **el que entra**: composición intacta, hierba repetitiva si se busca |
| 47 | 21,0 | se ve el patrón de repetición a simple vista |

### El presupuesto de VRAM, medido

El primer intento (639 tiles) **rompió la pantalla**: basura por todas partes y
hasta la fuente del sistema corrupta. `VTile_alloc()` no comprobaba el tope y
los tiles se escribieron encima de los mapas de plano.

Medido en el emulador con `PORT_VRAM_REPORT=1`, que imprime `VTile_used()`
contra `TILE_USER_LENGTH`: **el juego ya gastaba 1378 tiles de los 1425 que
hay**. Quedaban 47.

Los 1425 no se pueden subir. SGDK coloca el plano B en 0xC000 y el A en 0xE000,
y los planos van alineados a 0x2000, así que 0xC000 es el suelo del espacio de
tiles pase lo que pase; mover la ventana (que no se usa) no gana nada porque el
plano B está por debajo de ella.

De donde sí salen tiles es del **reparto por turnos**: los tres tilesets del
menú (iconos de arma 80, logo 49 y barra 1, total 130) y el fondo **nunca se
ven a la vez**, porque el menú borra el campo y el campo no se dibuja mientras
hay menú. Comparten región (`VTile_shared()`, inc/vramspr.h) y cada uno vuelve
a subir los suyos al entrar: `Menu_loadTiles()` al abrir el menú y
`Bg_loadTiles()` al empezar partida. Son ~5 KB de DMA en un cambio de pantalla.

47 + 130 = 177, y de ahí sale el presupuesto de 168 (múltiplo de 8 para que la
hoja de tiles no desperdicie ninguno). Total: **1416 de 1425**, nueve de sobra.

`Bg_init()` comprueba el tope en ejecución: si el diccionario no cupiera, el
fondo se queda fuera y el campo vuelve al color plano, en vez de corromper la
VRAM en silencio.

### Cómo encaja con el resto

- El fondo va al **plano B**, debajo del A. Los agujeros y el texto siguen en
  el A, y como en un plano el color 0 no se pinta y deja ver el plano de
  atrás, los agujeros se recortan solos sobre la hierba sin tocar su arte.
- El plano se rellena entero (64x32 celdas, no 40x30) repitiendo el borde:
  la sacudida de pantalla mueve el plano hasta 3 px y si no se asomaba una
  franja del color de fondo. La sacudida se aplica ahora **a los dos planos**,
  o los agujeros parecerían deslizarse sobre el campo.
- El menú limpia el plano B al entrar y al salir, así que sigue viéndose como
  antes, con su barra y su marquesina sobre gris.
- La paleta del fondo es la **línea 3**, que estaba libre. Se cuantiza a 15
  colores (sólo salen 12) y el índice 0 no se usa, para que no haya ni un
  píxel transparente en el campo.

### Lo que esto se lleva por delante

La **translucencia horneada** —la mejor técnica que hay en este port— depende
de que el campo sea un color plano: cada efecto con alfa del original se
compone contra (51,51,51) en tiempo de conversión y se guarda su color final
exacto. Sobre hierba esos colores ya no son los que tocan: **la humareda de
las explosiones y el reborde de los agujeros se ven como manchas grises sobre
el verde**, no como algo translúcido. Con el campo plano eran exactos.

No tiene arreglo sin dejar de hornear: el VDP no mezcla. Si el fondo se queda,
la alternativa es rehacer esos efectos con tramado (patrón de medio punto),
que en Mega Drive es lo que se usaba, y que es un trabajo aparte.

Y un aviso de legibilidad: las arañas, los gusanos y los agujeros son siluetas
**negras**, y los bordes de la imagen son copas de árbol oscuras. Sobre la
hierba clara del centro se leen perfectamente; contra los árboles del borde,
menos.

## Pulido de gráficos: lo que fallaba de consola y la rotación continua

Pasada de revisión con el juego a 2 y 4 aumentos en BlastEm, frame a frame,
buscando fallos propios de la consola (no huecos del port). Todo medido con
`tools/mdrun.py`; el caso peor con `PORT_STRESS=1` y semilla fija, que hace
las partidas **deterministas por build**: la misma secuencia de mando da la
misma escena, y eso permite comparar dos builds píxel a píxel.

### Cinco fallos, con su causa

| Lo que se veía | Causa | Arreglo |
|---|---|---|
| En las sacudidas los agujeros se deslizaban bajo la nave: **el plano iba un frame por delante de los sprites** | `Shake_update` escribía los registros de scroll en el acto, a mitad de frame, y la tabla de sprites no sube hasta el vblank siguiente | `VDP_set*ScrollVSync`: el scroll se escribe en el mismo vblank que la tabla (`src/shake.c`, y la marquesina del menú) |
| **Los paquetes del suelo no se sacudían**: se quedaban clavados mientras todo lo demás se movía | su sprite se colocaba una sola vez, al nacer, con la cámara de ese frame | se recolocan cuando cambia la cámara; lo mismo el fogonazo, el `oAutoDestruccion` y el sprite de `oMuerte`, que arranca justo con la sacudida más fuerte del juego |
| **Paquetes y balas tapaban las cifras del HUD** | todos los sprites llevaban prioridad alta y el HUD baja. En el GML `oInGameHUD` vive en `Instances_1`, la capa más alta del room (depth −2147482300): nada se le pone encima | sprites a prioridad **baja**, HUD y menú a **alta**. En el VDP el orden es sprite alto > A alto > B alto > sprite bajo > A bajo > B bajo: los sprites siguen por encima de agujeros y fondo pero bajo el HUD |
| **Hueco oscuro de hasta 7 px en el borde derecho de la banda del menú**, bailando con la marquesina | la banda pintaba 40 columnas de un plano B de 64 que se desplaza con el texto | las 64 columnas |
| El blanco de `oMuerte` se escribía en la CRAM a mitad de frame (frame partido, y en consola real un punto de color en la línea de la escritura) | `PAL_setColor` | `PAL_setColors(..., DMA_QUEUE)`, en el vblank |

Cómo se midió la sacudida: reconstruyendo el fondo de bosque desde
`res/bg_art.h` y buscando, por franjas, el desplazamiento del plano en cada
captura; y aislando la nave (blanco puro, 29-31 píxeles) y los paquetes (cian)
para sumarles ese desplazamiento. Si plano y sprite van sincronizados la suma
es constante.

| | Antes | Después |
|---|---|---|
| nave + desplazamiento del fondo, 16 frames con sacudida | varía hasta **±6 px** | constante (158,6-160,4 / 118,6-120,2; la variación que queda es el cambio de frame de rotación) |
| paquete + desplazamiento del fondo | 39,3 → 42,3 → 36,0… (el paquete no se movía) | 39,2 ± 0,2 |
| borde derecho de la banda del menú | 6 píxeles oscuros × 64 líneas | 0 en seis capturas |

Comprobado y **descartado** como fallo: el HUD se recorta por arriba en las
sacudidas, pero el original hace exactamente lo mismo (lo dibuja en
coordenadas del room, no de la GUI, y la cámara se mueve). Tampoco se pierden
sprites: 55-77 entradas de hardware en el caso peor, de 80.

### La rotación continua: 32 direcciones con los frames en ROM

La nave y las cabezas de gusano giraban a saltos de 45° (8 frames
prerrotados) y en el original giran de forma continua. Es lo que más cantaba.
Ahora van a **32 direcciones** (pasos de 11,25°), y la forma de pagarlo es lo
que importa, porque la VRAM estaba a 1416 de 1425 tiles y tres hojas de 32
frames no caben precargadas.

`VStream` (`inc/vramspr.h`): cada uno de esos sprites tiene SU hueco de VRAM
del tamaño del frame más gordo de su hoja, y cuando cambia de frame se encola
un DMA con los tiles de ese frame desde la ROM. Son como mucho 4 sprites
(nave y tres cabezas), ≤288 bytes por cambio y ~1,2 KB por frame en el peor
caso de los 8 KB que SGDK reparte por vblank en PAL. El DMA y la tabla de
sprites se vuelcan en el mismo vblank, así que el dibujo y el reparto en
sprites de hardware del frame cambian a la vez. Las hojas van `NONE` en
`resources.res` (el DMA copia tal cual lo que hay en ROM).

El último eslabón del gusano (`oCu2`) se dibuja con el arte de la cabeza pero
sigue precargado a 8 direcciones: es su propia hoja (`snake_cu2.png`), para
no tener que cargar las 32. Las arañas se quedan en 8: son 13 px y van ocho
a la vez.

| | Antes | Después |
|---|---|---|
| Direcciones de nave y cabezas | 8 | **32** |
| Tiles de VRAM (con el fondo de bosque) | 1416 / 1425 | **1376 / 1425** |
| ROM | 917.504 bytes | 917.504 bytes (las hojas sin comprimir caben en el relleno) |
| Velocidad del caso peor | 11 kills / 0 balas a +300 frames, 12 a +400 | **idéntico** |

### Cómo NO medir el caso peor (y cómo sí)

Esto costó dieciséis builds y conviene dejarlo escrito. Con el contador de
pantalla (`PORT_VRAM_REPORT=1`) la versión de 32 direcciones marcaba 25 fps y
100-140 % de CPU, y el perfilador por etapas señalaba al gusano. Era falso: el
caso peor va **justo al borde del frame**, y los dos `sprintf` y las cuatro
escrituras de texto del propio contador bastan para empujar frames por encima
del 100 %. Con vsync, el 99 % da 50 fps y el 101 % da 25.

Se descartaron, uno a uno y con un build cada uno: el DMA de los frames, la
prioridad de los sprites, el camino de dibujo (con el antiguo de 8 direcciones
seguía igual de lento), las estructuras, el arte de 32 filas (regenerado a 8
seguía lento), el audio, y los `VDP_showFPS`/`VDP_showCPULoad` de SGDK. Lo
que lo tiraba era el bloque de texto entero; sólo con los dos `sprintf` ya se
notaba (9 kills a +300 en vez de 11).

La medida que sí vale: como las partidas son deterministas por build, se
captura el HUD en un frame emulado fijo tras pulsar START+A y se compara la
**firma de la partida** (kills y munición). Un build más lento va atrasado
(menos kills); uno igual de rápido da la misma escena. El de referencia da 11
kills / 0 balas a +300 y 12 a +400, y la versión final da exactamente eso.

El perfilador por muestreo de vblanks que se probó también se quitó: el
vblank cae siempre en la misma fase de un bucle que arranca en el vblank,
así que no muestrea nada.

De paso se arregló `src/bench.c`, que no compilaba desde que `Enemy_hit` y
`Snake_hit` cambiaron de firma, y ahora `PORT_BENCH=1` con `PORT_VRAM_REPORT=1`
imprime los resultados en pantalla en vez de depender del depurador.

### Lo que queda pendiente de esta pasada

- La humareda y los efectos translúcidos sobre un fondo con textura. El autor
  ha dicho que el bosque es un **placeholder** y que habrá varios fondos
  seleccionables, así que la técnica de hornear el color contra el fondo deja
  de valer en general. La solución que no depende del fondo es el modo
  sombra/realce del VDP para la humareda (oscurece lo que hay debajo, exacto
  en consola y en emulador) y un color sólido para los rebordes finos; queda
  para cuando se concreten los fondos.

## Cuatro fallos de partida contra el original (revisión del autor)

### 1. La nave no disparaba a nada hasta que salía el primer enemigo

Medido: sí disparaba desde el frame 25 (el `controlTime = 30` del
`oPlayer/Create_0`), pero hacia arriba, a la nada. En el original el gusano que
crea `startGame()` es un `oMalo` desde su `Create_0`, aunque espere invisible
90 frames en su agujero, y `oPlayer/Step_0` apunta con
`instance_nearest(x, y, oMalo)`, que **no mira la visibilidad**: la nave se gira
hacia ese agujero y le dispara desde el principio. `Snake_closest` y
`Enemy_closest` descartaban lo invisible; ahora lo cuentan para apuntar (sólo
para apuntar: la lista de colisión sigue siendo la de lo visible). Comprobado:
la primera bala sale en el frame 35 hacia el agujero rojo.

**Desviación que queda**: en GameMaker la visibilidad tampoco afecta a las
colisiones, así que en el original las balas pueden dañar al gusano escondido
en el agujero (y el jugador morir tocándolo). En el port lo escondido no
colisiona. No se ha cambiado porque altera el reparto de muertes de la
partida; queda anotado.

### 2. Lo que hay en el suelo y las réplicas sobreviven a la muerte

`mataPlayer()` sólo hace `with oPlayer instance_destroy()`, y `startGame()`
sólo `with oMalo instance_destroy()` y `with oMenu instance_destroy()`. Los
paquetes, la bomba, las réplicas y los rayos del suelo, y las naves réplica
`oTri`, no son ninguno de esos: **se quedan en pantalla durante el menú y
siguen en la partida siguiente**. El port los borraba al morir y al empezar.

Ahora `Pow_replaceAll()` y `Proj_clearKeepTris()` los conservan (y los
recolocan con la cámara de la sacudida ya recentrada). Lo que sí se borra son
las balas en vuelo, el láser, la bomba con la mecha encendida y los satélites
del PIVOT, que viven del jugador. Como consecuencia, el escalado de
`generaPaquete()` (que cuenta lo que hay en el suelo) también continúa de una
partida a otra, igual que en el original.

La banda gris del menú pasa a prioridad alta: `oMenu` está a depth −100, por
delante de los objetos del room, así que la banda tapa lo que hay en el suelo
detrás de ella.

Los enemigos tampoco se borran al morir (lo hace `startGame()`): ver la
sección siguiente, "El menú encima de la partida". Lo que sigue quieto durante
el menú son las réplicas y los paquetes; en el original las réplicas siguen
disparando a los bichos.

### 3. La marquesina temblaba

Causa: la regresión de pasar el scroll al vblank (ver "Pulido de gráficos").
El texto se reescribía en el acto, a mitad de frame, y el scroll que lo
compensa iba en el vblank: un frame de cada cuatro el texto saltaba 8 px
adelante y volvía. Ahora la fila de texto también sube por la cola de DMA
(`VDP_drawTextEx(..., DMA_QUEUE)`), en el mismo vblank que el scroll.
`Menu_leave()` vacía la cola antes de limpiar el plano; si no, la última fila
encolada se escribía después y se quedaba pintada en la partida.

| | Antes | Después |
|---|---|---|
| Avance de la marquesina por frame (16 frames seguidos) | +6, −2, −2, −10, … | −2 constante |

### 4. Los umbrales de desbloqueo eran de prueba

El `.yyp` de `surfize_changes` trae `laserEnabled = 1`, `threewayEnabled = 2`,
`circularEnabled = 3`, `triEnabled = 4` y `nextWeapon = 999999`: valores de
prueba, y por eso el menú enseñaba "NEXT 999999" tras la primera partida. El
`surfize.rar` del autor trae varias versiones del proyecto en GameMaker 1.x, y
ninguna coincide con otra:

| Proyecto | LASER | 3WAY | PIVOT | PARA | `nextWeapon` inicial |
|---|---|---|---|---|---|
| `surfize_changes` (.yyp) | 1 | 2 | 3 | 4 | 999999 |
| `neu_swarm2_40` | **40** | **60** | **80** | **100** | **40** |
| `neu_swarm2_60` | 40 | 80 | 120 | 180 | 400 |
| `neu_swarm2_63_pospoAudio_last_score` | 100 | 200 | 300 | 400 | 999999 |
| `surfize_mac` / `surfize_mac8` | 40 | 60 | 80 | — | 40 |

**El autor eligió `neu_swarm2_40`** (2026-11-12). Es la única con las cuatro
armas y con NEXT coherente desde la primera partida. La lógica de
`stCheckWeapon` es la misma en todas (`for g < maxKills: if g >= umbral`, o sea
hace falta un récord ESTRICTAMENTE mayor que el umbral) y no cambia. Al
desbloquear la última, esa versión pone `nextWeapon = 're'`, una cadena, y el
menú enseña "NEXT re": se porta tal cual.

`7z` no abre el RAR (es RAR5); se extrae con `bsdtar` sólo los ficheros que
hacen falta.

### 5. El menú encima de la partida (petición del autor, 2026-11-12)

"Deja siempre el background y los bichos: sólo muere el jugador y se va al
menú." Es lo que hace el original: `mataPlayer()` sólo destruye `oPlayer` y
crea `oMenu`, que se dibuja ENCIMA del room. El port, en cambio, borraba el
fondo (plano B) y los enemigos al abrir el menú.

Qué impedía dejarlos:

- **VRAM.** El fondo y los tiles del menú (iconos, logo y barra: 130 tiles)
  compartían una región de 168, y no cabían a la vez: quedaban 49 libres.
  Pero por encima de 0xC000, donde SGDK da por acabado el espacio de tiles,
  hay dos trozos sin usar: el mapa del plano window (0xD000-0xDFFF, 128
  tiles; la ventana está apagada) y la cola tras la tabla de sprites
  (0xF680-0xFFFF, 76 tiles). Un tile vale en cualquier dirección (el índice es
  de 11 bits), así que los del menú van ahí (`VTile_allocSpare`) y se suben una
  sola vez al arrancar. El fondo ya no se recarga nunca.
- **La marquesina vivía en el plano B** y se movía con el scroll del plano
  entero, que ahora movería el fondo. Pasa a la fila 29 del plano A, encima
  del fondo, y durante el menú el scroll horizontal va POR FILA DE TILES
  (`HSCROLL_TILE`): sólo se desplaza su fila. En partida vuelve a ser de plano
  entero, que es el que usa la sacudida. El cambio de modo se hace con toda
  la tabla a cero y sin sacudida, así que no se nota aunque caiga a mitad de
  frame.
- **La banda** sigue en el plano B, en prioridad alta, y pisa las filas del
  fondo que tapa (8 de 30, igual que en el original, donde es opaca). Al salir
  se repintan sólo esas filas.
- **El texto y los agujeros comparten plano A.** Al salir del menú cada fila se
  reescribe de una vez con los agujeros que la cruzan (`Hole_rowTiles`), y la
  línea de créditos, que cruza dos, se compone igual: antes borrarla abría una
  franja en los agujeros. Durante el menú no se repintan los rebordes rojos de
  los agujeros (`Hole_holdDraw`), porque se comerían el texto y los iconos; el
  estado se sigue llevando y se pinta al salir.

Los enemigos se actualizan durante el menú con `hasPlayer = FALSE` (la IA no
pasa a atacar, como en el original sin `oPlayer`) y se borran en `startGame()`,
como en el original. Van por debajo de la banda, que tiene prioridad alta.

De paso, dos fallos del splash: pintaba BULLETS y KILLS (el original no dibuja
el HUD mientras existe `oMenu`) y la marquesina no escribía nada hasta que el
scroll llegaba a −8, así que la fila estaba vacía los primeros 164 frames.
Ahora entra deslizándose por la derecha, recortada a lo que ya ha entrado
para que el plano, que da la vuelta a las 64 columnas, no asome el final del
texto por la izquierda.

**Desviaciones que quedan**: durante el menú las réplicas y los paquetes están
quietos (en el original las réplicas siguen disparando y podrían sumar kills),
la metralla y el rayo en curso se cortan al morir, y los agujeros no cambian
de reborde mientras el menú está abierto.

### 6. Fondos seleccionables (2026-11-12)

El autor ha puesto dos fondos nuevos: `Grayscale Pixel-Art Ruins and Pits.png`
y `Monochrome Pixel Ruined Battlefield.png` (1494x1052). Están copiados en
`res/bg_src/` como `ruins.png` y `battlefield.png`, junto al bosque de antes
(`forest.png`, que se queda como tercera opción porque quitarlo no está
autorizado). Siguen sin salir del original, que pinta el campo de un color
plano.

- **Conversión.** `tools/bg2md.py` admite `--name` (un `.h` y un tileset por
  fondo, con símbolos propios) y `--crop43`: las dos imágenes nuevas no son
  4:3 y se recortan al centro en vez de estirarse (antes se estiraban). Los
  comandos exactos están en `res/resources.res`.
- **Presupuesto.** Al sacar los tiles del menú de la región del fondo (punto 5)
  quedaron 48 tiles libres, y la región pasa de 168 a **216**: la VRAM queda
  en 1424 de 1425. Ruinas 215 tiles (RMSE 13,5 por canal, 8 colores), campo
  de batalla 216 (RMSE 22,2, 7 colores), bosque 216 (RMSE 16,4, 12 colores).
  Las dos nuevas son en grises y el VDP sólo tiene 8 grises puros (3 bits
  por canal), así que salen con 7-8 tonos.
- **Selección, provisional.** Arriba y abajo en el splash y en el menú recorren
  los fondos; el elegido se queda para las partidas siguientes. Arranca en
  ruinas. El autor concretará cómo se eligen.
- **El cambio, sin frame a medias.** Los tiles nuevos no caben en la VRAM con
  los viejos, así que el cambio va en tres frames (`Bg_update`): paleta del
  fondo a negro en el vblank, subida de tiles y mapa con el fondo ya negro, y
  paleta nueva en el vblank. Se ven dos frames de campo negro con los agujeros
  y el texto encima. La banda del menú no se toca (`Bg_setCover`).

**Pendiente del autor**: los dos fondos nuevos traen agujeros pintados (cinco
en ruinas, cinco en el campo de batalla) que no coinciden con los seis del
juego, así que en pantalla se ven diez u once agujeros y sólo seis son de
verdad.

### 7. Sombras de la nave y los enemigos (2026-11-12)

Petición del autor. **No salen del original**, que no pinta sombras (lo más
parecido son los `draw_sprite_ext(..., c_black, ...)` de `oBulletMala` y
`oDebrie`, que tiñen el propio sprite): es una mejora, anotada como
divergencia.

Cómo se hacen y por qué así:

- **Pintadas en el arte de cada frame** (`tools/gm2md.py`, `add_shadow`): la
  silueta del frame ya girado, desplazada abajo a la derecha (luz fija desde
  arriba a la izquierda para todas las rotaciones), en negro tramado al 50 % y
  sólo en píxeles transparentes del frame, así que nunca tapa al objeto. La
  nave, que vuela, a 3 px; arañas y gusano (cabezas, eslabones, cola), que van
  por el suelo, a 2 px.
- **No como sprites aparte**: una sombra por objeto serían hasta 60 sprites más
  (los 52 eslabones), y el VDP no pasa de 80 por frame ni de 20 por línea. Se
  caerían sprites, que es justo el tipo de fallo que no queremos.
- **No con shadow/highlight**: ese modo oscurece todo píxel cuyos dos planos
  estén en prioridad baja, o sea el fondo y los agujeros enteros. Evitarlo
  obliga a subir de prioridad el fondo (y entonces tapa a los sprites) o los
  sprites (y entonces tapan el HUD y la banda del menú).
- **Tramadas y no sólidas**: las arañas y el gusano ya son negros. Una sombra
  sólida los engordaría; tramada se lee como sombra sobre cualquiera de los
  tres fondos.

Coste: 0 sprites, 0 CPU (la firma del caso peor es la misma, 11/12 kills).
VRAM: algunas sombras rompen repeticiones de tileset entre frames y suben 2
tiles, que no cabían (1426 de 1425: el tile de más caía encima del mapa del
plano B y llenaba de basura los espacios del texto). La región del fondo baja
de 216 a **208** tiles (múltiplo de 8, porque rescomp cuenta la última fila
de la hoja entera) y la VRAM queda en **1418 de 1425**. RMSE de los fondos:
ruinas 13,6, campo de batalla 22,7, bosque 16,5. Y desde ahora
`VTile_check()` para el juego con un mensaje si lo repartido se pasa, en vez
de pintar basura.

Limitaciones: la sombra es parte del sprite, así que va siempre a la misma
distancia (no "despega" al volar) y la sombra de un eslabón puede caer
encima del siguiente, aunque al ser negro sobre negro no se ve.

**Revisión del autor (mismo día)**: "despega más la sombra". La de la nave (y
la de las réplicas) pasa de 3 a **5 px**, lo máximo que cabe en la celda de 24
sin recortarla en ninguna de las 32 rotaciones: los píxeles de la nave llegan
a la columna y la fila 18. Los bichos siguen a 2 px.

### 8. La réplica era blanca, como la nave del jugador

"Cuando muere la nave, la nave blanca no puede quedar en el escenario; los
clones azules, sí." La nave que quedaba era la réplica (`oTri`): el port la
pintaba con el arte de la nave tal cual, en blanco, y tras morir parecía que
el jugador seguía en el campo. El original la dibuja con
`draw_sprite_ext(..., c_blue, 0.6)`: teñida de azul. Ahora la hoja la tiñe
igual (el tinte de GameMaker multiplica el color: el blanco sale azul y los
grises, azul oscuro) y lleva la sombra de la nave. La transparencia de 0,6 no
se porta, porque el VDP no mezcla colores.

El tinte rompe una repetición de tileset y la VRAM se pasó otra vez (1426 de
1425): esta vez lo paró `VTile_check` con su mensaje, en vez de pintar basura.
Arreglo de fondo: los tilesets pequeños de los sprites precargados van primero
a la VRAM sobrante de encima de 0xC000 (los 74 tiles que dejó libres el
menú), y sólo lo que no cabe va al espacio normal. La VRAM normal baja a
**1360 de 1425**, y los fondos vuelven a 216 tiles.

De paso, el menú: con la arena llena (8 arañas y 4 gusanos moviéndose debajo)
perdía 2 de cada 9 frames. Reescribía KILLS/TOP/NEXT (tres `sprintf` y tres
`VDP_drawText`) en cada frame; ahora sólo cuando entra una letra. Queda en 1
de cada 5 frames perdido en ese caso extremo, que cuesta lo mismo que la
partida en el caso peor. Con uno o dos bichos el menú va a 50 fps.

**Sobre la medición**: una pasada del caso peor dio 10 kills y 4 balas a +300
en vez de 11/0. La misma ROM, repetida, dio 11/12, y las variantes sin fondo
y con el fondo de 208 tiles también. La firma no es 100 % reproducible entre
pasadas de BlastEm: ante una diferencia, hay que repetir antes de concluir
nada.

### 9. El blancazo de la bomba, los puntos de las réplicas y el menú grande (2026-11-12)

**La bomba.** `oExplosionCruz/Draw_0`: `t += 1; draw_sprite_ext(sprExplosion,
..., t*8, t*8, 0, c_white, 0.5); if t > 60 instance_destroy()`. El sprite crece
a t*8 y al tercer frame ya tapa el room entero: lo que se ve es la pantalla
cubierta de un blanco al 50 % durante 60 frames (50 aquí), que se quita de
golpe. El port sólo hacía la explosión pequeña y la sacudida. Ahora
`Effect_bombFlash` lleva las cuatro líneas de paleta (64 colores) a medio
camino del blanco, que es lo mismo que un blanco al 50 % por encima, y
`bombUpdate` devuelve la paleta guardada a los 50 frames, todo por la cola de
DMA. Si el jugador muere durante el blancazo, la paleta vuelve antes de que la
muerte ponga su blanco en el color de fondo. Desviación: el HUD también se
aclara, porque comparte paleta con los agujeros; es blanco y casi no se
nota. Para probarlo sin ir a por el paquete: `PORT_TEST_BOMB` con
`PORT_STRESS`.

**Los puntos de las réplicas pisaban otros gráficos.** Los puntos de las líneas
a las réplicas (6 por réplica, hasta 24) y los de las descargas del rayo se
repartían después de los proyectiles pero ANTES de los efectos y los paquetes.
En el VDP lo que va antes en la tabla se pinta encima, y cuando una línea de
pantalla pasa de 20 sprites el VDP deja de pintar los que van detrás. Así que
los puntos se pintaban encima de la humareda y de los paquetes, y una línea de
puntos casi horizontal se comía el cupo de su franja y hacía desaparecer los
paquetes y los efectos que pasaban por ella. Ahora se reparten los últimos
(`Beam_initDots`): van por debajo de todo y, si sobra algo, sobran ellos.
Además, los que se salen de la pantalla se aparcan.

Pendiente: `oTri/Draw_0` también dibuja una línea de cada réplica a cada otra
réplica. Con 4 réplicas serían 6 líneas más (36 sprites de puntos), y no
caben: se queda sin portar.

**El menú, con letra grande.** `oMenu/Draw_0` pinta KILLS, TOP y NEXT con
`setFontInterlude()`, que es `global.myFont` (sFont2): la letra grande del HUD,
no la pequeña de la marquesina. El port las pintaba con la fuente de 8 px del
sistema. Ahora van con los glifos de sFont2 (`HUD_drawBig`), a los que se han
añadido O, P, N, X y la "re" del NEXT final: 6 glifos, 24 tiles más. Con
`fa_center` la 'y' del original es el centro de la línea: filas 13, 15 y 17.
CREDITS y la marquesina siguen con la del sistema (el original usa sFont3, que
es aún más pequeña).

**La cuenta de los números** (petición del autor, NO está en el original, que
escribe el número ya hecho letra a letra): en cuanto la línea ha escrito su
etiqueta, KILLS y TOP suben desde 0 **de uno en uno**, un número por frame,
con un tick en cada paso. Sólo si el valor tardara más de 3 s (150 frames)
da pasos mayores. La primera versión subía en 25 frames dando saltos (de 6 en
6 con 137 kills) y el autor la pidió "más precisa".

- **El tick** es un golpe de ruido blanco del PSG de 2 frames, flojo y bajando
  (atenuación 9 y luego 12, de 15): `Audio_tick`/`Audio_update`. No sale del
  original ni es un sample nuevo: el PSG está libre porque todo el audio del
  juego es PCM por XGM2, que sin música XGM no lo toca.
- **Coste.** Repintar las dos líneas enteras en cada paso eran unas 140
  escrituras al VDP por frame, y con la arena llena debajo la cuenta subía un
  número cada 2-3 frames. Ahora, entre letra y letra, sólo se reescriben las
  cifras que cambian. Con 8 arañas y 4 gusanos debajo, la cuenta sube 1 por
  frame salvo algún frame suelto (12, 12, 12, 13, 14, 15).

**NEXT parpadea** (petición del autor, tampoco está en el original): cuando la
línea ya está escrita entera, se enciende y se apaga cada 16 frames (0,32 s,
invención). Sólo se repinta esa línea en cada cambio; apagada son tiles vacíos
que dejan ver la banda.

### 10. Depurador de gráficos en el juego (2026-11-12)

Para encontrar por qué desaparecen trozos de gráficos. Está en
`inc/gfxdebug.h` + `src/gfxdebug.c`, y sólo entra con `make GFXDEBUG=1` (antes,
`touch src/*.c inc/*.h src/*.s`). En partida: START pasa el panel por
apagado → números → números + gráfica; A, B y C encienden y apagan los
sprites, el plano B (fondo) y el plano A (agujeros y HUD). Los planos se apagan
poniendo su línea de paleta al color de fondo, porque el VDP no tiene un
interruptor por plano.

Mide, sobre la tabla de sprites que va de verdad al VDP: sprites y celdas por
línea contra los límites de H40 (20 y 40), cuántas líneas los pasan, sprites
de máscara (x = 0), sprites pedidos contra los 80 de la tabla, la cola de DMA
del frame y el bit de desbordamiento del registro de estado del VDP. El análisis
cuesta CPU: con el panel encendido el juego va más lento, así que no sirve para
medir velocidad.

**Primer resultado, con el caso peor (`PORT_STRESS=1`)**: hasta **29 sprites y
69 celdas en una misma línea** (línea 113, a la altura de la nave) y unas 270
líneas por encima de algún límite en 8 frames. La tabla no se llena (62 de 80),
no hay máscaras y la DMA va holgada (1008 de 8000 bytes). O sea: lo que
desaparece son los sprites que quedan detrás en la tabla en las líneas donde
se juntan los eslabones de los gusanos y las arañas. El VDP corta por el
límite de 320 px de sprite por línea, a veces a mitad de un sprite, y eso se
ve como un trozo que falta.

**Arreglo (mismo día), en dos partes:**

1. **Rotación de la tabla cuando hay desbordamiento** (`src/spr.c`). Es lo que
   hacen los juegos comerciales. La tabla salía siempre en el mismo orden, así
   que en una línea atascada los que sobraban eran siempre los mismos y
   desaparecían del todo. Ahora, si alguna franja de 8 líneas pasa de 20
   sprites o de 40 celdas, cada frame la lista arranca por un sitio distinto
   (paso 7) y lo que sobra va cambiando: en vez de desaparecer, parpadea. La
   entrada 0 (la nave) no rota. Sin desbordamiento no se rota, porque rotar
   cambia qué sprite queda encima de cuál y daría un parpadeo gratuito donde
   dos objetos se solapan. Son tres enlaces reescritos.
   Coste medido: mirar el desbordamiento en cada frame tiraba el caso peor
   (8 kills en vez de 11 a +300, igual en dos pasadas, y 11 sin la rotación).
   Se mira uno de cada 4 frames y el resultado vale para los otros tres: 11/12
   kills, como antes.
2. **Sprites recortados a 16x16**: la araña, la cabeza de tipo 1, la cola
   (oCu1 y oCabeza2) y el último eslabón (oCu2). Sus frames de 24x24 tenían el
   dibujo (con la sombra) en como mucho 16x16, en un sitio distinto según la
   rotación. `tools/gm2md.py` (`trim=16`) recorta cada frame alrededor de su
   dibujo y apunta en `gm_art.h` dónde cae la esquina respecto al centro
   (`spiderOffX/Y`, `snakeHead1OffX/Y`, `snakeTailOffX/Y`, `snakeCu2OffX/Y`); el
   código coloca cada sprite con el desplazamiento de su frame. El dibujo
   queda en el mismo píxel que antes, y cada uno de esos sprites pasa de 3 a 2
   celdas de ancho por línea (y de 9 a 4 tiles). La cabeza de tipo 2 llega a
   17 px en algunas rotaciones y se queda en 24x24.
   Con el caso peor, el máximo de celdas en una línea baja de 69 a 63: el
   grueso son los eslabones del cuerpo (ya de 16x16) de cuatro gusanos
   cruzándose. Ahí sigue habiendo desbordamiento, que ahora parpadea en vez
   de desaparecer.

### 11. El recorte cuadrado entre agujeros cercanos (2026-11-12)

Cada agujero es un bloque de 8x8 tiles en el plano A, y donde dos están
cerca sus bloques comparten celdas. Una celda del plano sólo tiene un tile,
así que el agujero que se pintaba después ponía el suyo (la esquina de su
bloque, casi transparente) y se comía los goterones del otro: se veía un
recorte recto con el borde del bloque. En el original no pasa porque los
agujeros son sprites y se componen píxel a píxel.

Ahora `src/hole.c` busca al arrancar las celdas compartidas (33 con las
posiciones del room, nunca más de dos agujeros por celda) y les da un tile
propio en VRAM con los agujeros que la tocan compuestos uno encima de otro, en
el orden del room (el que va después, encima). Se recompone cuando cambia el
reborde rojo de alguno, y al salir del menú. Coste: 33 tiles de VRAM y
recomponer las celdas de un agujero cuando cambia, que es raro. Caso peor:
11/12 kills, igual que antes.

### 12. Fondos difuminados (2026-11-13)

"¿Por qué el fondo tiene poca calidad? El píxel está difuminado." Dos causas:

1. **El desenfoque de la conversión.** `bg2md.py` reducía la imagen con filtro
   Triangle y un `-blur 0x0.8` puesto a propósito. Las imágenes nuevas son pixel
   art ampliado a un tamaño que no es entero: midiendo la autocorrelación de
   los bordes, ruinas tiene el píxel de 5,90 x 5,73 (unos 238x184 píxeles de
   arte tras el recorte a 4:3), campo de batalla de 6,57 x 6,46 (unos 213x163)
   y bosque de 4,34 x 4,31 (334x252). Las dos nuevas tienen MENOS píxeles que
   la pantalla, y filtrarlas para ampliar las difumina. Nueva opción
   `--pixel PX[xPY]`: toma un píxel por celda de la rejilla real (`-sample`) y
   amplía por vecino más cercano. Nítido.
2. **El diccionario de tiles.** 1200 celdas en 216 tiles. El voraz se quedaba
   con las primeras celdas que no se parecían a nada, que en arte nítido son
   las más ruidosas, y todo se arrimaba a ellas: parches y puntos por todo el
   fondo. Ahora, después del voraz, se refina (k-modas, 20 vueltas como
   mucho): cada celda va a la entrada y el volteo más parecidos, y cada entrada
   pasa a ser, píxel a píxel, el color más frecuente de sus celdas (la moda,
   no la media, que difumina).

Resultado, eligiendo por fondo lo que se ve mejor (los comandos exactos están
en `res/resources.res`):

| Fondo | Antes (RMSE) | Ahora | Cómo |
|---|---|---|---|
| Ruinas | 13,5 (pero difuminado) | 12,6 contra la imagen nítida | `--pixel 5.904x5.728` + refinado |
| Campo de batalla | 22,2 | 18,6 | filtro + refinado |
| Bosque | 16,4 | 13,2 | filtro + refinado |

Con `--pixel`, el campo de batalla y el bosque tienen tanto detalle fino que
216 tiles no dan para tenerlo nítido, y salen a parches (RMSE 39,3 y 30,9):
para esos dos queda mejor el filtro, ahora con el diccionario refinado. Para
que un fondo salga nítido en Mega Drive, lo ideal es que el arte se haga a
320x240 píxeles de verdad y con zonas que se repitan (el suelo liso), que
es lo que deja sitio en el diccionario al detalle.

## MonsterLite

Desde el 2026-11-15 la fuente de verdad es **MonsterLite** (Unity, C#); el GML pasa a segunda referencia (ver `.claude/commands/port-arena.md`). El inventario y los bloques están en `docs/MONSTERLITE_GAP.md`. Rutas abreviadas: `S/` = `~/MonsterLite/MonsterLite_ultimo_commit/Assets/Scripts/`.

### Estado de los bloques

| Bloque | Estado |
|---|---|
| 1. Estructura de partida (puntuación `u32`, niveles, oleadas por tiempo) | ✅ hecho (2026-11-15) |
| 2. Jugador | ✅ hecho (2026-11-15), Q8 incluida tras la decisión del usuario sobre el depurador |
| 3. Armas y munición | ⏳ pendiente (Q9-Q12) |
| 4. Economía en partida | ⏳ pendiente (Q11) |
| 5. Pantallas | ⏳ pendiente (Q13-Q16) |
| 6. Guardado en SRAM | ⏳ pendiente |
| 7. Power-ups | ⏳ pendiente (Q17) |
| 8. Enemigos (IA, gusano) | ⏳ pendiente |
| 9. HUD, mensajes y fondo por nivel | ⏳ pendiente (Q16) |
| 10. Logros | ⏳ pendiente (Q18) |
| 11. Tutorial | ⏳ pendiente (Q19) |
| 12. Efectos y sonido | ⏳ pendiente (Q17, Q20, Q21) |

### Bloque 1: estructura de partida

Respuestas del usuario (2026-11-15): Q1 apaisado, 20 px/u; Q2 se quedan los dos gusanos del GML con su longitud; Q3 topes de MonsterLite recortados por medición; Q4 tope de 8 arañas; Q5 se corrige el doble de la bala fina.

**Lo que hace ahora el port:**

| Qué | MonsterLite | Port | Dónde |
|---|---|---|---|
| Puntuación | `globales.kills += level` por baja, `int` | `g_score` (`u32`) `+= bajas × g_level`; se pasa de 65535 en el nivel 14 | `src/game.c` (`playUpdate`) |
| Niveles | `levelUpgrade()` en cada `Update`: `if (kills / factorLevel > level) { level = kills / factorLevel; factorLevel *= 2; }` | `Game_levelUpgrade()` cada frame, misma regla. Se compara `score >= factor · (level + 1)`, que con enteros es lo mismo que `score / factor > level`, y sólo se divide al subir | `src/game.c` |
| Inicio | `WaitForSeconds(0.8)` → nave; `WaitForSeconds(1)` → 1 gusano; bucle de oleadas sin espera previa | nave a los **40 f**; a los **90 f** un gusano y, en el mismo frame, la primera oleada | `startSequence()` |
| Oleadas | cada 0,4 s, `r = Random.Range(0, kills % 40)` y `switch (r)` | cada **20 f**, tabla `mlWaves[41]` en ROM con arañas, gusanos y gusanos extra si `level > 5` (27, 31, 36) | `mlWave()` |
| Tope de gusanos | `(level / 10) · 3 + 2` cabezas | `min((level / 10) · 3 + 2, MAX_SNAKES = 3)` | `mlSnakeCap()` |
| Tope de arañas | ninguno | 8 (`MAX_SPIDERS`, Q4) | `Enemy_spawn` |
| Velocidad de avance | `setSpeed`: `f = (level % 10) + 0.8`, `Random.Range(f, maxSpeed + f/10)` u/s; maxSpeed 1 (araña) y 4 (gusano) [prefab] | igual al crear el enemigo, × 0.4 → px/f en fix16 | `Entity_mlSpeed()` (`src/entity.c`) |
| `generaMalos()` del GML | no existe | ya no se llama (borrado, con `SPIDERS_PER_WAVE` y `MALOS_COUNTER_MAX`) | — |
| HUD | SCORE, BEST, LEVEL, UPGRADE AT (bloque 9) | provisional: puntuación en 7 cifras grandes a la derecha (donde iban las KILLS); fila 2 con la fuente del sistema "LV n UP n LAST n … SCORE" | `src/hud.c` |

**Qué tipo de gusano sale.** MonsterLite tiene un solo gusano y la tabla de oleadas pide "un gusano". Ni MonsterLite ni el GML dicen cómo elegir entre los dos tipos del GML en esta tabla, así que se toma el único criterio que hay en las fuentes, el de `generaMalos()` del GML (`scripts/generaMalos`): **tipo 1 hasta llenar su tope (`MAX_CABEZA_1`, 2) y, si está lleno, tipo 2 (`MAX_CABEZA_2`, 1)**, siempre dentro del tope total de MonsterLite. Consecuencia: en los niveles 1-9 (tope 2) sólo salen gusanos de 12 eslabones; el de 10 aparece desde el nivel 10 (tope 3) y, como antes, con la bomba y el rayo del GML, que lo crean ellos sin mirar el tope de MonsterLite (bloque 7). No es balance inventado: es el orden del GML aplicado a la cantidad de MonsterLite.

**Velocidad.** Sólo la velocidad de avance: el giro sigue siendo el del GML (grados fijos, el gusano de tipo 1 con su sorteo `velo`) y la IA la de 3 estados hasta el bloque 8. Comprobado con la fórmula: araña en nivel 1 de 30/64 a 46/64 px/f (MonsterLite 0,472-0,72), gusano en nivel 9 de 127/64 a 251/64 (1,99-3,92). El sorteo es en milésimas de u/s y se redondea al 1/64 de fix16: error de hasta medio paso, 0,0078 px/f, que en la araña más lenta (0,472) es un **1,7 %**. Como el gusano de MonsterLite puede ir hasta a 3,92 px/f (antes el máximo era 1,875), la caja envolvente del gusano, que se recalcula cada 4 frames, se infla ahora 16 px en vez de 8 (`SNAKE_BB_MARGIN`, `src/snake.c`).

**Medición del tope de gusanos (Q3).** `tools/measure.sh` (caso peor, `PORT_STRESS=1`, vueltas del bucle en 600 frames y bajas). Las variantes de 4 y 5 gusanos se compilaron en el scratchpad, no en el repo, con el tope forzado al pool:

| Variante | Vueltas de 600 | Bajas |
|---|---|---|
| 3 gusanos (2 + 1), el pool actual | 484 | 21 |
| 3 gusanos, con el fondo a 496 tiles (control) | 485 | 21 |
| 4 gusanos (3 + 1), fondo a 496 tiles | **365** | 14 |
| 5 gusanos (3 + 2, el tope de MonsterLite en el nivel 10), fondo a 496 | **348** | 15 |

Un cuarto gusano además **no cabe en VRAM**: arranca con `VRAM: 1436 de 1425 tiles` (`VTile_check`), así que para medirlo hubo que bajar el presupuesto del fondo de 528 a 496 (con eso el fondo no se pinta, que no cambia el coste por frame: el control de 3 gusanos da lo mismo, 485 frente a 484). Con 4 gusanos se pierde el 39 % de los frames frente al 19 % con 3. **El tope se queda en 3**: el de MonsterLite (2) cabe tal cual en los niveles 1-9; del 10 en adelante se recorta de 5 (y 8 desde el 20) a 3.

**Rendimiento del bloque.** `tools/measure.sh`, antes 478/600 vueltas y 23 bajas; después **540/600 y 17 bajas**. No es una optimización: el arnés llena la arena con 3 gusanos al empezar, pero ahora las oleadas sólo la rellenan hasta el tope de MonsterLite del nivel en curso (2 hasta el nivel 9) en vez de hasta 3, y los primeros 40 f no hay nave que dispare. Con el tope forzado a 3, como del nivel 10 en adelante, sale 484/600 y 21 bajas: el mismo coste que antes (478) con la misma población. Las bajas no se pueden comparar entre builds: cambian las oleadas, la velocidad de los enemigos y el arranque.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| **Toda baja suma `level`; la araña muerta por bala fina no da `2 · level`** (`S/bulletThinSrc.cs:49`) | Decisión del usuario (Q5): es un fallo del original y se corrige. **Divergencia deliberada** | Puntuación algo más baja que en MonsterLite cuando se juega sin munición contra arañas, y por tanto niveles que llegan un poco más tarde |
| Tope de gusanos 3 en vez de 5 (nivel 10-19) y 8 (20+) | CPU y VRAM, medido arriba (Q3) | Del nivel 10 en adelante hay menos gusanos que en el original |
| Tope de 8 arañas; MonsterLite no tiene | Decisión del usuario (Q4); el pool de 8 ya existía | Las oleadas que piden araña con 8 vivas no crean nada |
| El tipo de gusano se elige con el orden de `generaMalos()` del GML | MonsterLite tiene un solo gusano (Q2, decisión 4) | En los niveles 1-9 sólo salen gusanos de 12 eslabones |
| Las oleadas de power-ups (3, 4, 6, 16, 19-21) no crean nada todavía | Son del bloque 7 | De momento la bomba, los rayos y las réplicas siguen saliendo por `generaPaquete()` del GML al morir cada cabeza |
| La oleada 27 no enseña "!!!" | Es del bloque 9 | — |
| La metralla del GML (`oDestro`) también puntúa `level` | No existe en MonsterLite; con la regla de Q5 toda baja suma `level` | — |
| La nave aparece en el centro del GML, no en (160, 69) | Es del bloque 2 (Q6) | Resuelto en el bloque 2: nace en (160, 17), ver abajo |
| Giro, IA, aparición de 75 f y hp siguen siendo del GML | Son del bloque 8 | El enemigo va a la velocidad de MonsterLite pero gira como en el GML. Los eslabones del gusano siguen separados por `stepdelay` frames, así que a la velocidad del nivel 9 (hasta 3,92 px/f) quedan más separados que antes (hasta ~24 px); las migas de MonsterLite, cada 8 px, son del bloque 8 |
| HUD provisional: cifras grandes sin la palabra SCORE en grande, y "LV / UP / LAST / SCORE" con la fuente del sistema | La fuente grande no tiene C ni R; el HUD definitivo es el bloque 9 (Q16) | Se lee mal sobre los fondos con mucho detalle, igual que el LAST de antes. LAST pasa a ser la mejor puntuación de la sesión |
| La cuenta de bajas (`g_kills`) sigue existiendo aparte de la puntuación | La usan el desbloqueo por récord de las armas del GML (40/60/80/100), que según Q9 **convive** con la tienda de MonsterLite y se queda, y el interludio KILLS/TOP hasta el bloque 5 | El interludio y el número volador de la cabeza enseñan bajas, no puntuación |

### Bloque 1: dos correcciones tras la verificación (2026-11-15)

**La caja envolvente del gusano dejaba fuera la cabeza tras un salto.** En el estado de vagar del GML (`stWalkingA1W`), al tocar un agujero la IA lleva la cabeza al agujero 0 (`Entity_updateAI`). La caja del gusano (`bbX0..bbY1`) se recalcula cada 4 frames con un margen que sólo cubre el avance, así que tras el salto la cabeza, y luego cada eslabón que pasa por el salto, podían quedar hasta 3 frames fuera de la caja. Durante esos frames no chocaban con la nave (`Snake_collides`) ni con las balas (`Snake_hit*`), y la nave no podía apuntarles (`Snake_closest`). Lo encontró el verificador del bloque 1 (partes a 48 px fuera); el comentario de `src/snake.c` decía que nunca quedaba fuera un contacto.

Arreglo: `Entity_updateAI` devuelve si ha llevado la entidad al agujero 0, y el gusano recalcula la caja **cada frame** desde el salto hasta que ha recorrido el cuerpo entero (eslabones × `stepdelay`: 72 y 50 frames). Sólo cuenta el primer salto de una racha. Mientras el gusano vaga encima del agujero 0, la IA lo devuelve al centro cada frame, pero esas posiciones caben en ±4 px y el margen las cubre. Recalcular durante toda la racha (hasta 500 frames) costaría sin hacer falta.

Comprobado con la instrumentación del verificador ampliada (cabeza y eslabones contra la caja, los cuatro lados, cada frame), en el arnés de estrés con el nivel fijado en 9 (la velocidad máxima) durante 6000 frames: **0 partes fuera** en 1525 saltos (con el anillo del GML), en 2625 (con el hexágono del bloque 2) y en 3755 (bloque 2 terminado). Control con el HEAD y la misma instrumentación: 14 fuera en 1196 saltos, todas de la cabeza, hasta 107 px. El piloto del verificador (`v_pilot_bb.py`) no sirve para esto: sus partidas duran unos 180 frames y el gusano no llega a vagar (antes ataca 250).

**`tools/measure.sh` ya no medía el caso peor.** El arnés llena la arena con el pool de gusanos (3) al empezar, pero las oleadas sólo lo reponían hasta el tope de MonsterLite del nivel en curso (2 hasta el nivel 9). Con `PORT_STRESS` el tope es ahora el pool (`mlSnakeCap`, `src/game.c`). Medido: 540/600 y 17 bajas antes; **483/600 y 21 bajas** después, igual que la medida con el tope forzado del bloque 1 (484/21). La caja exacta tras el salto no cambia la cifra. La línea base del verificador (`.claude/agents/verificador.md`) pasa a la del bloque 2, abajo.

### Bloque 2: jugador

Respuestas del usuario (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`): Q6 hexágono de MonsterLite centrado en pantalla, con la nave en la misma posición respecto al hexágono; Q7 4 px/f normalizado; Q8 el tiempo sólo corre con B pulsado. Q8 se paró en una primera entrega porque B ya tenía uso en partida (el depurador de gráficos). El usuario decidió después, en las "Decisiones añadidas durante el bloque 2": con el panel del depurador abierto el tiempo no se congela y B sigue siendo del depurador. Confirmó además la aparición en (160, 17), aunque quede bajo el HUD provisional hasta el bloque 9, y la nave mirando arriba al nacer.

**Lo que hace ahora el port:**

| Qué | MonsterLite | Port | Dónde |
|---|---|---|---|
| Velocidad | rama de teclas: `speed` 0.2 u por tick de `FixedUpdate` [prefab player] en cada eje, sin inercia; en diagonal no normaliza (5,66 px/f) | 4 px/f (exacto) en horizontal y vertical; en diagonal 181/64 = 2,828 px/f por eje, 3,9996 px/f en total (−0,01 %), con constantes (Q7) | `Player_padDelta` (`src/player.c`), `ML_PLAYER_SPEED*` (`inc/arena.h`) |
| Wrap | toroidal en los bordes exactos de la cámara (`S/playerMovement.cs:129-146`) | margen 0 (`PLAYER_WRAP_OFFSET`, antes 3 px del GML) | `Entity_wrapPlayfield` |
| Aparición | `highPos` = (W/2, H/1.4) (`S/gameControl.cs:156`); el hexágono va en `posH` = (W/2, H − H/1.4): la nave nace (2/1.4 − 1) × 12 u = 5,14 u = **102,9 px por encima del centro del hexágono** | hexágono centrado (Q6) → **(160, 17)** | `ML_PLAYER_SPAWN_*`, `Player_init` |
| Colisión | `SphereCollider` de radio 0.18 [prefab], escala 1 → 3,6 px | círculo de 3,6 px: con distancias enteras, `d² ≤ 12` (exacto: no hay ningún d² entero entre 12 y 12,96), contra las cajas de los enemigos y de lo que hay en el suelo. La distancia es al último píxel de la caja (semiabierta, `bx + bw − 1`): en la primera entrega se medía a `bx + bw` y la nave llegaba a 4 px por la derecha y por abajo frente a 3 por la izquierda y por arriba (lo midió el verificador) | `Coll_circleBox` (`inc/collision.h`), `Enemy_collides`, `Snake_collides`, `Pow_collect` |
| Muere al tocar | `arana`, `snake`, `snakeBody` | igual (ya lo era) | — |
| Agujeros | `AgujerosPadre`: seis hijos (escala 1.1 cada uno; la del padre es 1) en (−2, 1), (0, −2), (0, 2), (2, −1), (−2, −1), (2, 1) u, por orden de `m_Children`; el grupo en `posH` | los seis, en ese orden, con el hexágono centrado: (120, 100), (160, 160), (160, 80), (200, 140), (120, 140), (200, 100). No matan (ya era así) | `src/hole.c` |
| Láser | `isLaserState` nunca se pone a `true`: las dos líneas que lo hacen están comentadas (`S/disparo.cs:76, 84`), así que la nave no se congela | no se congela. **Nunca lo hizo**: el GML sí (`laserState`), pero el port no lo implementó y no estaba anotado (ver "Mecánicas del original") | — |
| Congelación al soltar | sin dedo en pantalla y pasado el "ARENA!", `Time.timeScale = 0` (ver abajo) | sin B pulsado y pasados 25 f de partida, se para todo menos el HUD, con el aviso "HOLD B" (Q8) | `timeStopped`, `stoppedUpdate` (`src/game.c`) |
| Dónde nacen los enemigos | `posHoles[Random.Range(0, posHoles.Count)]`, un agujero al azar, arañas (`S/enemyController.cs:124`) y gusanos (`:150`) | igual. Antes las arañas nacían todas en el último agujero, la regla del GML (`oSpider/Create_0`) | `Hole_spiderSpawn`, `Hole_snakeSpawn` |

**El pivote del GML** (`oPivot`, arma de las bolas) se movía con `acc = 6`, igual que la nave, para ir pegado a ella. Ahora se desplaza con `Player_padDelta`, lo mismo que la nave.

**Los agujeros, en VRAM.** Con el algoritmo de antes, el hexágono pedía 108 celdas compartidas (hoy eran 33) y quedaba 1 tile libre. Se cambian dos cosas en `src/hole.c`:

- Sólo se suben y se pintan las celdas del bloque de 8×8 que tienen algún píxel en la variante normal o en la roja: 43 de 64 por variante. Las esquinas vacías ya no tapan al vecino.
- Una celda sólo se compone si **dos agujeros tienen dibujo** en ella: 30 con el hexágono.

Total de los agujeros: de 64 + 64 + 33 = 161 tiles a 43 + 43 + 30 = **116**. Más 10 para el texto encima de los agujeros (abajo). `nextTile` pasa de 1440 a **1405**: 1389 de 1425 tiles de usuario, **36 libres** (antes 1). `loadedCount` sigue en 105 (`MAX_LOADED` 160). Las dos zonas de VRAM sobrante (`VTile_allocSpare`) están llenas.

**Lo que el menú pinta encima de los agujeros.** Con el hexágono en el centro, el menú pinta sobre los agujeros en tres sitios. Una celda del plano sólo tiene un tile, así que donde lo pintado es transparente se veía el plano B y el agujero salía cortado en rectángulo:

- **El logo del splash** (7×7 celdas, columnas 17-23 y filas 11-17): 40 de sus 49 celdas caen sobre algún agujero, y salían los seis cortados. Lo vio el verificador. Ahora cada una de esas celdas se compone (agujeros debajo, logo encima: comparten PAL2) y se sube **encima del propio tile del logo**. El logo sólo se ve en el splash y nadie más usa sus tiles: **0 tiles nuevos**. Se compone siempre desde el logo de la ROM. Esas celdas pasan enteras a prioridad alta, también la parte de agujero; en el splash no hay sprites. `drawLogo`, `src/menu.c`.
- **La fila de CREDITS** (fila 21, en el splash y en el interludio): cruza la parte de abajo de tres agujeros, con 8 letras sobre dibujo de agujero. En el HEAD ya cortaba los dos agujeros del anillo por los que pasaba.
- **El aviso HOLD B** (fila 20): cae sobre el agujero de abajo, 5 letras.

Para CREDITS y HOLD B, `Hole_textRow` compone cada letra que cae sobre un agujero: los agujeros debajo y la letra encima. La letra se lee de la VRAM, donde SGDK dejó la fuente (en la ROM va comprimida). La fuente del sistema sólo usa el índice 15 y se ve blanca, así que la letra se pinta con el blanco de la paleta de los agujeros (0x0EEE, índice 8). Los dos textos nunca están a la vez en pantalla, así que comparten **10 tiles** (`HOLE_TEXT_TILES`). Es el único gasto de VRAM de esta parte, y servirá también para los mensajes del bloque 9 sobre el hexágono. Si un texto pidiera más, las letras que sobran salen con el tile de la fuente, como antes.

Lo demás del menú no corta nada que se vea:

- **La banda del interludio** va en el plano B con prioridad alta, filas 12-19, y tapa los agujeros por diseño (el menú del original se dibuja encima del room).
- **KILLS, TOP y NEXT** (filas 13-18) quedan dentro de la banda. Mientras la banda crece (unos 23 frames) las primeras letras pueden cortar el borde de los agujeros laterales. Ya pasaba en el HEAD; no se ha cambiado.
- **Los iconos de arma** (filas 24-27), la marca `^` (fila 28) y la marquesina (fila 29) quedan por debajo del último agujero (fila 23).

El plano A se mueve de 8 en 8 px. En X los tres valores del hexágono caen en la rejilla, pero los cuatro agujeros de los lados (y = 100 y 140) quedan a 4 px de una fila. Se pintan **4 px hacia fuera** del centro (a 96 y 144): el hexágono queda simétrico. Redondeando hacia dentro, los dos agujeros de cada lado se montaban 19 px (mirado en una maqueta). La posición lógica (nacimiento, IA, colisión) es la exacta.

**Rendimiento.** `tools/measure.sh`:

| Build | Vueltas de 600 | Bajas |
|---|---|---|
| Sólo las correcciones del bloque 1 | 483 | 21 |
| Bloque 2, primera entrega | 522 | 17 |
| Bloque 2 terminado | **484** | **24** |

Ninguna de estas cifras mide coste de código: cambia la partida del arnés. En la primera entrega cambiaron los agujeros (juntos en el centro), la nave (arriba) y la recogida con el círculo, que cambia qué paquetes coge y con ellos la munición. En la segunda, las arañas pasan a nacer repartidas por los seis agujeros en vez de todas en uno: hay más bajas y más trabajo por frame. Es el caso peor del juego tal como queda. Controles en copias del scratchpad:

- nave en el centro y hexágono: 518/19;
- nave en el centro y el anillo viejo: 494/22;
- bloque 2 terminado, pero con las arañas en el último agujero: **518/17**, lo mismo que la primera entrega.

La congelación, el texto compuesto y el arreglo del radio no cuestan nada medible. Con `PORT_STRESS` el tiempo no se congela. **Línea base nueva: 484/24.**

Visto de paso, sin relación con este bloque: al ponerse rojo el agujero del primer gusano (unos 97 frames tras empezar) se pierden 2 frames seguidos. En el HEAD se perdían 3 en el mismo momento.

**Q8: el tiempo sólo corre con B pulsado.** Qué hace MonsterLite: en partida, si no hay ningún dedo en pantalla y `HUD.counter >= 60`, `Time.timeScale = 0`. Cualquier toque, o pasar al GAMEOVER, lo devuelve a 1 (`S/gameControl.cs:265-301`). Con `timeScale` a 0, Unity deja de llamar a `FixedUpdate`:

- no corren `moveEnemies`, `move()`, `rotateShipDirection` ni `dispara` (`S/gameControl.cs:333-350`): **la nave no se puede mover ni girar ni disparar**, y los enemigos no se mueven;
- la física se para: las balas, con `Rigidbody`, se quedan quietas;
- las corrutinas con `WaitForSeconds` (las esperas de `startGame`, el bucle de oleadas y el cartel de nivel) también se paran;
- las animaciones (`Animator`) y lo que se mueve con `Time.deltaTime` se paran, porque `deltaTime` vale 0.

`Update` sigue corriendo cada frame, y lo que cuenta frames y no tiempo **no se para**:

- el temporizador de IA de `enemyMov`;
- la vida de paquetes y monedas (`S/paqueteScr.cs:39`, `S/Coin.cs:108`);
- el impacto instantáneo de LASER y MOIRE (`S/RaycastScr.cs:33`);
- las bolas CIRCULAR sueltas (`S/circularScr.cs:57`);
- los rayos, `HUD.counter` y las sacudidas (corrutinas con `yield return 0`, un paso por frame, `S/cameraScript.cs:15-76`).

La música baja de tono 0,1 por frame. El aviso es la textura `dedo` más "KEEP YOUR FINGER\n ON SCREEN!" en y = H − H/2.9 = 157 px (`S/HUD.cs:489-497`).

**En el port** (`timeStopped` y `stoppedUpdate`, `src/game.c`):

- **Cuándo.** Sin B pulsado (`ML_TIME_BUTTON`) y pasados los **25 primeros frames** de cada partida (`ML_FREEZE_GRACE_F`). `HUD.counter` vuelve a 0 fuera de partida y sube una vez por llamada a `OnGUI` mientras sale "ARENA!", hasta 60 (`S/HUD.cs:163-166, 470-472`). Unity llama a `OnGUI` al menos dos veces por frame (Layout y Repaint): 30 frames de 60 Hz, 25 f. Es **deducido, no medido**: con una sola llamada por frame serían 50 f, y con toques, menos de 25. Como la nave entra a los 40 f y la espera de arranque también se para, sin B la nave no aparece hasta que se pulsa. En MonsterLite pasa lo mismo: la espera de 0,8 s es escalada.
- **Qué se para.** Todo lo que hay hoy en el port menos el HUD:
  - los enemigos (movimiento, IA del GML y su aparición de 75 f);
  - la nave (no se mueve, no apunta, no dispara);
  - las balas, la metralla, el pivote y las réplicas;
  - la secuencia de arranque y las oleadas;
  - las explosiones;
  - los números que vuelan al HUD (en MonsterLite van con `deltaTime`);
  - el latido de lo que hay en el suelo (en MonsterLite, `Animator`);
  - la sacudida.

  Las colisiones tampoco se comprueban (con la física parada, en Unity no salta ningún trigger).
- **Lo de `Update` que no se para en MonsterLite todavía no existe en el port**: caducidad de paquetes y monedas (bloque 4), LASER y MOIRE de impacto instantáneo y bolas CIRCULAR (bloque 3), rayos (bloque 7) e IA de `enemyMov` (bloque 8). Cada uno de esos bloques tiene que sacar lo suyo del corte de `playUpdate`. Está escrito en el comentario de `timeStopped`.
- **La nave no se agranda al congelar.** En MonsterLite, cada frame congelado reproduce `landingAnim` (`gameControl.cs:270`). Como el `Animator` de `player.prefab` va en modo Normal (`m_UpdateMode` 0), la nave se queda a escala ×4 (4,00 × 3,82) mientras el tiempo está parado, y al reanudar vuelve a ×1 en 0,23 s. El VDP no escala sprites: queda pendiente para el bloque 12 (efectos), con frames pre-escalados o descartado. **Divergencia**.
- **La sacudida** de MonsterLite seguiría, pero la del port es todavía la del GML (`stShake`, que vive en el Step de `oPlayer`) y se para con la nave. El desplazamiento se aplica al colocar cada sprite (`inc/spr.h`), así que mover el plano con los sprites quietos los descuadraría. La sacudida de MonsterLite es del bloque 12.
- **El aviso**: "HOLD B" con la fuente del sistema, centrado, en la fila 20 del plano A (160 px; MonsterLite empieza en 157). Va compuesto encima del agujero de abajo (ver arriba). La mano (`dedo`) no tiene equivalente con el pad y no se porta. La música de partida no existe todavía (bloque 12), así que tampoco hay cambio de tono.
- **No se congela con `PORT_STRESS` ni con `PORT_BENCH`**: el arnés mide el caso peor con todo en marcha y `tools/measure.sh` no pulsa B, así que mide lo mismo que antes sin cambiar el script. Los pilotos que juegan una partida normal por el depurador (los del verificador) tienen que mantener B pulsado.
- **Con el panel del depurador de gráficos abierto** (`GFXDEBUG=1`) tampoco se congela, por decisión del usuario: B sigue siendo del depurador (plano B). Con el panel cerrado, B vuelve a ser el del tiempo. `GfxDebug_panelOpen`, `src/gfxdebug.c`.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| En diagonal, 4 px/f en vez de 5,66 | Decisión del usuario (Q7). **Divergencia deliberada** | La diagonal es un 29 % más lenta que en MonsterLite; las ocho direcciones van igual |
| Hexágono centrado, no 51 px por debajo del centro | Decisión del usuario (Q6) | Todo queda 51 px más arriba que en el dispositivo apaisado |
| La nave nace en (160, 17), debajo del HUD provisional | Consecuencia de Q6: 102,9 px por encima del centro del hexágono. Confirmado y aceptado por el usuario hasta el bloque 9 | Al empezar, la nave queda tapada en parte por "LAST" (fila 2) y la cifra de BULLETS: blanco sobre blanco, sólo se distingue la sombra. En cuanto se mueve se ve |
| Los cuatro agujeros de los lados se pintan 4 px más lejos del centro | Rejilla de 8 px del plano A | Sólo visual: nacimiento, IA y colisión usan la posición exacta |
| Círculo de la nave contra las **cajas** de los enemigos del GML | Las esferas de los enemigos (8,4 y 9,6 px) son del bloque 8 | El contacto llega a 3 px del borde de la caja en recto, por los cuatro lados, y a 2 + 2 en diagonal. Desde arriba la caja vieja de 7×11 ya mataba a 6 px: ahora se puede pasar más cerca por arriba. Los 12 px entre centros de MonsterLite (araña) llegan con el bloque 8 |
| La recogida usa el círculo contra la caja de paquete del GML (11×11) | La caja de 20×20 de MonsterLite es del bloque 4 | Hay que pasar más cerca que en MonsterLite, y que antes con la caja de 7×11 |
| El orden de los agujeros es el de `m_Children` del prefab | MonsterLite no depende del orden; la IA del GML sí (bloque 8) | El enemigo que vaga salta a (120, 100), el agujero 0. Las arañas ya no dependen del orden: nacen en uno al azar, como en MonsterLite (resuelto en esta entrega) |
| La nave nace **mirando arriba**; en MonsterLite, a la derecha (`Quaternion.identity`) | Decisión del usuario (bloque 2). **Divergencia deliberada** | Sólo se ve los 50 f hasta que sale el primer gusano |
| La congelación es con B y no con el dedo; aviso "HOLD B" sin la mano | Decisión del usuario (Q8): no hay pantalla táctil | Hay que mantener B pulsado para jugar |
| La gracia antes de la primera congelación es de 25 f | Deducida (dos llamadas a `OnGUI` por frame), no medida: en MonsterLite depende de cuántas veces llame Unity a `OnGUI` | Entre 0 y 25 f de diferencia con el original al empezar |
| Con el tiempo parado también se paran la sacudida (todavía la del GML) y las cosas del GML que aún no tienen su versión de MonsterLite | Ver arriba: lo de `Update` llegará con sus bloques | Nada cuenta mientras está parado: lo del suelo no caduca (tampoco caduca con el tiempo en marcha, bloque 4) |
| Con el panel de `GFXDEBUG=1` abierto no se congela | Decisión del usuario | Sólo en la build de depuración |
| El texto sobre los agujeros (CREDITS, HOLD B) va en blanco de la paleta de los agujeros y sus celdas enteras con prioridad alta | Una celda del plano sólo tiene un tile y una paleta | En esas celdas, un sprite que pase por encima del agujero queda debajo del dibujo del agujero mientras se ve el texto |
| El sexto agujero reconstruido del GML (558, 754) ya no se usa | Lo sustituye el hexágono de MonsterLite | — |

## Pruebas realizadas

| Fecha | Prueba | Resultado |
|---|---|---|
| 2026-11-15 | Bloque 2 terminado: `make` completo, `GFXDEBUG=1` y `PORT_BENCH=1` | ✅ 0 avisos en los tres |
| 2026-11-15 | Q8 por RAM (`startFrames`, `graceFrames`, `holdShown`, nave y cabeza de gusano) | ✅ sin B se para a los 25 f (la secuencia de arranque queda en 25 y la nave no entra); con B sigue; al soltar B la cabeza no se mueve en 60 f y el aviso está puesto; al volver a pulsar, sigue y el aviso se quita |
| 2026-11-15 | Q8 con `GFXDEBUG=1` | ✅ con el panel abierto y sin B la cabeza se mueve; B enciende y apaga el plano B; al cerrar el panel, sin B se congela con el aviso |
| 2026-11-15 | Logo del splash sobre el hexágono | ❌ seis agujeros cortados en rectángulo (verificador); ✅ compuesto en los tiles del logo, agujeros enteros, 0 tiles nuevos |
| 2026-11-15 | CREDITS y HOLD B sobre los agujeros | ✅ las letras van encima del agujero sin cortarlo, en el splash, el interludio y la partida; con B pulsado la fila vuelve a tener sólo agujeros |
| 2026-11-15 | Radio con la caja semiabierta, en las cinco direcciones | ✅ muere a d² = 9 (3 px) por arriba, abajo, izquierda y derecha, y a d² = 8 en diagonal (vive con 16 y con 18) |
| 2026-11-15 | Movimiento con B pulsado, frame a frame contra el modelo | ✅ 113 frames, 0 fuera del modelo, wraps por los cuatro bordes sin margen |
| 2026-11-15 | `tools/measure.sh`, bloque 2 terminado | ✅ **484/600, 24 bajas** (las arañas nacen repartidas); control con las arañas en el último agujero, 518/17 |
| 2026-11-15 | VRAM, bloque 2 terminado | ✅ `nextTile` 1405 de 1440 (36 libres), `loadedCount` 105 |
| 2026-11-15 | MonsterLite, bloque 2 y correcciones del bloque 1: `make` | ✅ sin avisos |
| 2026-11-15 | Caja del gusano tras el salto al agujero 0: arnés de estrés con el nivel fijado en 9, 6000 f, cabeza y eslabones contra la caja cada frame | ✅ 0 fuera en 1525 saltos (anillo del GML), en 2625 (hexágono) y en 3755 (bloque 2 terminado); ❌ control con el HEAD: 14 fuera (la cabeza, hasta 107 px) en 1196 saltos |
| 2026-11-15 | `tools/measure.sh` (primera entrega del bloque 2) | 540/600, 17 bajas (HEAD) → 483/21 (arnés con el pool de 3 gusanos) → ✅ 522/17 (bloque 2); controles: nave en el centro 518/19, nave en el centro y anillo viejo 494/22 |
| 2026-11-15 | Movimiento de la nave por RAM, frame a frame, contra un modelo en Python (213 frames en dos secuencias) | ✅ 0 frames fuera del modelo: 4 px/f en recto, 181/64 por eje en diagonal; wraps sin margen 320 → 0, 0 → 320, 3,42 → 240 y 241,9 → 0. 2 frames perdidos al ponerse rojo el agujero del primer gusano (3 en el HEAD) |
| 2026-11-15 | Aparición de la nave | ✅ (160, 17) leída por RAM a los 45 f; captura: tapada en parte por "LAST" del HUD provisional, visible al bajar a (160, 57) |
| 2026-11-15 | Radio de 3,6 px: nave puesta por RAM junto a la cabeza de un gusano, cada vez más cerca | ✅ desde arriba vive con d² = 16 y muere con d² = 9; en diagonal vive con 18 y muere con 8 (la caja vieja mataba ya a 6 px por arriba y a 3 + 3 en diagonal) |
| 2026-11-15 | VRAM con el hexágono (primera entrega del bloque 2) | ✅ `nextTile` 1440 → 1395 (46 tiles libres, antes 1); `loadedCount` 105; 30 celdas compuestas |
| 2026-11-15 | Capturas: splash, partida, interludio y segunda partida | ✅ hexágono centrado, sin recortes entre agujeros, reborde rojo al salir un gusano; el interludio y la vuelta a partida reponen los agujeros sin restos de texto |
| 2026-11-15 | MonsterLite, bloque 1: `make` | ✅ sin avisos |
| 2026-11-15 | Regla de niveles por RAM (`g_level`, `g_factorLevel`, `g_score` escritos con `set`, un frame) contra un modelo en Python | ✅ 9 de 9: (1,1,2)→2/2, UP 6; (1,1,3)→3/2, UP 8; (2,2,9)→4/4, UP 20; (2,2,5) sin cambio; (13,4096,70000)→17/8192; (14,8192,70000) sin cambio, UP 122880; (14,8192,122880)→15/16384 |
| 2026-11-15 | Oleadas con `kills % 40` fijado (arnés en el scratchpad, 3000 f) | ✅ índice 0: 138 oleadas, 277 gusanos pedidos (1 + 2 × 138), 0 arañas; índice 39: 127 oleadas, 113 gusanos (esperado 115), 93 arañas (esperado 120; histograma de `r` sin huecos sistemáticos, el xorshift de SGDK da 0,947 arañas por oleada en su periodo entero) |
| 2026-11-15 | Arranque en partida normal, capturas | ✅ a los 20 f sin nave; a los 60 f nave y ningún enemigo; a los 120 f dos agujeros en rojo (dos gusanos naciendo, 75 f ocultos); hacia los 200 f un gusano alcanza a la nave quieta y entra el interludio con KILLS 0 |
| 2026-11-15 | Arnés de estrés, 3000 f | ✅ nivel 8, puntuación 536, factor 64 (subió por salto), HUD "LV 8 UP 576 LAST 536" |
| 2026-11-15 | `tools/measure.sh` antes / después | ✅ 478/600, 23 bajas → 540/600, 17 bajas (menos población hasta el nivel 10, ver "MonsterLite") |
| 2026-11-15 | Tope de gusanos medido: 3 / 4 / 5 | ✅ 484 / 365 / 348 vueltas; 4 gusanos no caben en VRAM (1436 de 1425 tiles): tope 3 |
| 2026-11-13 | Fondos con rejilla real y diccionario refinado | ✅ los tres en el emulador; RMSE ruinas 12,6 (nítido), campo 22,2 → 18,6, bosque 16,4 → 13,2 |
| 2026-11-12 | Agujeros con celdas compartidas | ❌ recorte cuadrado en los goterones; ✅ 33 celdas compuestas, goterones enteros; caso peor 11/12 |
| 2026-11-12 | Variante `PORT_NO_SMOKE=1` (sin humareda gris) | ✅ caso peor sin humo; quedan el fogonazo blanco, la metralla y el reloj de oAutoDestruccion |
| 2026-11-12 | Rotación de la tabla con desbordamiento | ❌ comprobando en cada frame: 8/11 kills (dos pasadas iguales; 11/12 sin rotación); ✅ comprobando 1 de cada 4 frames: 11/12 kills |
| 2026-11-12 | Araña, cabeza 1, cola y cu2 a 16x16 | ✅ máx. de celdas por línea en el caso peor de 69 a 63; dibujo en el mismo sitio |
| 2026-11-12 | Depurador de gráficos (`GFXDEBUG=1`) con el caso peor | ✅ panel, gráfica y apagado de sprites y planos; mide 29 sprites y 69 celdas en la línea 113 (límites 20 y 40), tabla 62/80, sin máscaras, DMA 1008/8000 |
| 2026-11-12 | Cuenta de uno en uno con tick (137 kills, arena llena) | ❌ un número cada 2-3 frames repintando las líneas enteras; ✅ 1 por frame repintando sólo las cifras, con algún frame suelto perdido; el tick no se ha escuchado (sin audio en el arnés) |
| 2026-11-12 | Parpadeo de NEXT | ✅ encendida y apagada a intervalos de 16 frames en 6 capturas cada 8 frames; KILLS y TOP quietas |
| 2026-11-12 | Blancazo de la bomba (`PORT_TEST_BOMB`) | ✅ la pantalla entera a medio blanco y, 50 frames después, la paleta de antes |
| 2026-11-12 | Puntos de la réplica contra la humareda y los paquetes | ✅ los puntos quedan por debajo |
| 2026-11-12 | Menú: letra grande y cuenta | ✅ KILLS/TOP/NEXT con sFont2 en las filas 13/15/17; con 137 kills la cuenta pasa por 1, 24, 36, 48 y 60 cada 7 frames hasta 137 |
| 2026-11-12 | Caso peor tras los tres cambios | ✅ 11/12 kills, idéntico |
| 2026-11-12 | Muerte con una réplica (`PORT_STRESS_MORTAL`) | ✅ la nave blanca desaparece; la réplica queda, azul y con sombra |
| 2026-11-12 | Sombra de la nave a 5 px | ✅ sin recorte en las 32 rotaciones (píxeles de la nave hasta la columna y la fila 18) |
| 2026-11-12 | VRAM con la réplica teñida | ❌ 1426/1425, lo paró `VTile_check`; ✅ tilesets pequeños a la VRAM sobrante: 1360/1425, con fondos de 216 |
| 2026-11-12 | Menú con 8 arañas y 4 gusanos | ❌ 2 de 9 frames perdidos; ✅ 1 de 5, tras escribir el texto sólo al cambiar |
| 2026-11-12 | Caso peor (`PORT_STRESS=1`) | ✅ 11/12 kills; una pasada suelta dio 10/12 y no se repitió |
| 2026-11-12 | Sombras pintadas en nave, arañas y gusano | ✅ se ven tramadas abajo a la derecha sobre ruinas; sólo cambian las 7 hojas tocadas (las demás salen idénticas byte a byte); VRAM 1418/1425; caso peor 11/12 kills, idéntico |
| 2026-11-12 | VRAM con las sombras y fondos de 216 tiles | ❌ 1426/1425, basura en el texto; ✅ fondos a 208 y `VTile_check` |
| 2026-11-12 | Fondos ruinas / campo de batalla / bosque con arriba-abajo en el splash | ✅ los tres se ven; el cambio pasa por dos frames de campo negro, sin frame a medias; la partida empieza con el elegido |
| 2026-11-12 | Menú tras morir (`PORT_STRESS_MORTAL=1`) | ❌ el menú borraba el fondo y los enemigos; ✅ fondo, agujeros, 8 arañas y gusanos siguen, y los bichos se mueven entre dos capturas a 100 frames |
| 2026-11-12 | Splash | ❌ salían BULLETS/KILLS y la marquesina vacía 164 frames; ✅ sin HUD, la marquesina entra por la derecha desde el primer frame |
| 2026-11-12 | Velocidad del caso peor con el menú nuevo (`PORT_STRESS=1`) | ✅ 11 kills a +300, 12 a +400: idéntico |
| 2026-11-12 | Paso menú → partida | ✅ fondo y agujeros repuestos sin frame a medias; el HUD sale completo en un frame |
| 2026-11-12 | Umbrales de desbloqueo | ❌ eran los de prueba del .yyp (1/2/3/4, NEXT 999999); ✅ los de neu_swarm2_40 (40/60/80/100), el menú enseña NEXT 40 tras la primera partida |
| 2026-11-12 | Disparo al empezar la partida | ❌ disparaba hacia arriba: el apuntado ignoraba al gusano oculto, que en el original es oMalo; ✅ primera bala en el frame 35 hacia su agujero |
| 2026-11-12 | Muerte con 8 paquetes y una réplica (`PORT_STRESS_MORTAL=1`) | ❌ se borraban; ✅ siguen en el menú y en la partida siguiente |
| 2026-11-12 | Paquete detrás de la banda del menú | ❌ se pintaba encima; ✅ banda a prioridad alta, lo tapa |
| 2026-11-12 | Marquesina, avance por frame | ❌ +6/−2/−2/−10 (temblor de 8 px); ✅ −2 constante |
| 2026-11-12 | Marquesina al empezar partida | ❌ la última fila encolada se quedaba pintada sobre el campo; ✅ se vacía la cola en `Menu_leave` |
| 2026-11-11 | Sacudida: nave + desplazamiento del fondo en 16 frames consecutivos (plano reconstruido de `res/bg_art.h`) | ❌ variaba hasta ±6 px: el scroll se escribía a mitad de frame y el plano iba un frame por delante de los sprites |
| 2026-11-11 | Lo mismo con `VDP_set*ScrollVSync` | ✅ constante (158,6-160,4 / 118,6-120,2) |
| 2026-11-11 | Paquetes del suelo durante la sacudida | ❌ no se movían (colocados sólo al nacer); ✅ tras recolocarlos al cambiar la cámara: 39,2 ± 0,2 |
| 2026-11-11 | Prioridad: paquete sobre las cifras del HUD | ❌ los sprites tapaban el HUD; ✅ sprites a prioridad baja, HUD y menú a alta |
| 2026-11-11 | Borde derecho de la banda del menú en 6 capturas | ❌ 6 px oscuros × 64 líneas; ✅ 0 con las 64 columnas |
| 2026-11-11 | HUD recortado por arriba en la sacudida | ✅ no es fallo: el original dibuja el HUD en coordenadas del room y la cámara se mueve |
| 2026-11-11 | Rotación a 32 direcciones (nave y cabezas) con los frames en ROM por DMA | ✅ se ven las direcciones intermedias en frames consecutivos; VRAM 1416 → 1376 de 1425 |
| 2026-11-11 | Velocidad del caso peor con `PORT_VRAM_REPORT=1` | ❌ 25 fps, 100-140 %: el propio contador empuja el frame por encima del 100 % (16 builds de experimento para aislarlo) |
| 2026-11-11 | Velocidad del caso peor por firma de partida (kills/munición a +300 y +400 frames, semilla fija) | ✅ 11/0 y 12, idéntico al build anterior a la rotación |
| 2026-11-11 | Determinismo de BlastEm con semilla fija | ✅ dos ejecuciones del mismo build dan capturas idénticas (md5) |
| 2026-11-11 | `src/bench.c` | ❌ no compilaba (firmas de `Enemy_hit`/`Snake_hit`); ✅ arreglado, resultados en pantalla |
| 2026-11-10 | Fondo: deduplicación de tiles de la imagen tal cual (rescomp) | ❌ 1200 celdas → 1200 tiles distintos, cero repeticiones, con todos los filtros probados |
| 2026-11-10 | Fondo: primer diccionario de 639 tiles | ❌ basura por toda la pantalla y fuente corrupta: 1378 + 639 se sale de los 1425 tiles de usuario |
| 2026-11-10 | Presupuesto de VRAM medido con `PORT_VRAM_REPORT=1` | ✅ 1378 de 1425 usados antes del fondo; 47 libres |
| 2026-11-10 | Región compartida menú/fondo (`VTile_shared()`) | ✅ 1416 de 1425; splash, interludio y partida sin corrupción, los tiles del menú vuelven correctos tras cada partida |
| 2026-11-10 | Fondo de 168 tiles en el plano B, partida real | ✅ imagen reconocible, agujeros recortados sobre la hierba, HUD legible |
| 2026-11-10 | Coste del fondo con `PORT_STRESS=1`, con y sin él | ✅ 50 fps en los dos casos; CPU 82 % con fondo contra 85 % sin él (el plano B lo lee el VDP igual esté vacío o no) |
| 2026-11-08 | Relectura completa del GML: 61 objetos, padres y eventos de colisión | ✅ cuatro mecánicas sin portar y dos portadas mal (ver la sección de la relectura) |
| 2026-11-08 | `stShake` portado: plano A por scroll + sprites por `sprPos` | ✅ `make` exit 0 sin avisos |
| 2026-11-08 | Sacudida medida en el emulador: `g_shakeX`/`g_shakeY` frente a la tabla de sprites del VDP | ✅ cámara -1 → jugador en la columna 157, cámara +1 → columna 155; salta siempre a ±1 exacto, nunca a 0 |
| 2026-11-08 | Sacudida: desplazamiento del plano medido sobre capturas consecutivas (banda del HUD) | ✅ (dx,dy) = (-2,+2), (+2,0) y (-2,-2), coincidiendo exactamente con el cambio de cámara de cada frame; 0,0 % de píxeles distintos en el desplazamiento correcto |
| 2026-11-08 | Eslabones del gusano absorbiendo balas, con contador temporal en `Snake_hit` | ✅ 4 absorciones en 400 frames de arena llena, con los kills subiendo a la vez (las cabezas siguen siendo matables) |
| 2026-11-08 | Coste de las dos mecánicas, contador en pantalla con `PORT_STRESS=1` | ✅ caso peor 103 % → 104 % de CPU; partida normal 50 fps (28 % con cuatro balas en vuelo) |
| 2026-11-08 | `tools/check_codegen.py` sobre el ROM nuevo | ✅ sin escrituras duplicadas: la resta de `g_shakeX`/`g_shakeY` sobrevive al compilador |
| 2026-11-08 | Revisión general: 61 objetos, 42 sprites y 22 sonidos del original contra el port | ✅ siete mecánicas sin portar, dos fallos latentes y una lista de código muerto |
| 2026-11-08 | Motor de sprites propio (`inc/spr.h`) sustituyendo a `SPR_update` | ❌ primer intento: basura en pantalla por leer `numSprite` negativo como contador (es la marca de "un solo sprite alineado" de rescomp) |
| 2026-11-08 | Motor de sprites, tras el arreglo | ✅ caso peor de 24 a 41 fps; tope de 80 objetos Sprite levantado a 160 ranuras |
| 2026-11-08 | Metralla (`oEstallido`/`oDestro`) en el emulador | ✅ ráfagas de 12 confirmadas por el índice del pool, con daño en cadena |
| 2026-11-08 | Fogonazo de muerte (`oMuerte`) fotograma a fotograma | ❌ invisible: el arte de `sprExplosion_2` es blanco con alfa de 0,03 a 0,56 y el umbral lo dejaba todo transparente |
| 2026-11-08 | `oMuerte` con el alfa binarizado al 4 % y el frame de más cobertura | ✅ crece y la pantalla se queda en blanco, como el sprite gigante del original |
| 2026-11-08 | Número que vuela al HUD (`oPuntoMorte`) | ✅ visto en pantalla con los dígitos de `sFont2` |
| 2026-11-08 | Haz del arma PARA (`object1`) | ✅ cadena de 14 segmentos por delante de la nave |
| 2026-11-08 | Línea azul a las réplicas (`draw_line_color`) | ❌ los puntos salían negros: `res/gm_palettes.h` era un duplicado viejo con la paleta de antes del gris de humo, y era el que cargaba `main.c` |
| 2026-11-08 | Paleta, tras borrar el duplicado | ✅ los puntos salen azules y la humareda gris, no negra |
| 2026-11-08 | Iconos de arma, logo y barra del menú | ❌ no se veían: `Menu_init()` estaba escrita desde la FASE 8 y nadie la llamaba |
| 2026-11-08 | Menú tras llamar a `Menu_init()` desde `Game_init()` | ✅ barra, iconos y cursor en pantalla |
| 2026-11-08 | Caja envolvente del gusano partida en dos mitades | ❌ peor: 41,8 fps frente a 43,0. Revertido y anotado |
| 2026-11-08 | Medida final con semilla fija | ✅ partida normal 49,5 fps con un pico del 37 %; caso peor 43,1 fps, 78 % de media |
| 2026-11-08 | Coste de un gusano, medido cambiando el tope | ✅ 3 + 1 gusanos: 43,1 fps / 117 % de pico. 2 + 1: 47,8 fps / 68 % |
| 2026-11-08 | Sprites por línea de barrido, volcando la tabla del VDP en el frame más cargado | ✅ peor línea 11 sprites y 192 px, con topes de 20 y 320: no se pierde ningún sprite |
| 2026-11-08 | Tamaño del arte dibujado contra el del original a escala | ❌ la araña salía de 7x8 cuando mide 13x13 y la nave de 8x8 cuando mide 8x12: el umbral de alfa 128 se comía los bordes de una reducción 5:1 |
| 2026-11-08 | Umbral de alfa bajado a 48 | ✅ la araña pasa de 1892 a 4444 píxeles opacos y recupera su silueta |
| 2026-11-08 | Color real de los efectos translúcidos, compuesto sobre el fondo del room | ❌ la humareda se portaba como gris opaco (73,73,73) cuando en pantalla es (51,59,59)-(66,73,73); la metralla como negro puro y el haz del PARA como azul saturado |
| 2026-11-08 | Paleta con los cinco tonos ya mezclados | ✅ la humareda deja de tapar; se arreglan de paso el láser (rojo), el 3WAY (rojo) y el PARA (azul), que se pintaban en blanco y negro |
| 2026-11-08 | `MAX_CABEZA_1` de 3 a 2 por tiempo de CPU | ✅ caso peor 43,1 → 48,4 fps, media del 59 %, pico del 71 %, **cero frames por encima del 100 %** |
| 2026-11-08 | Partida normal tras todo lo anterior | ✅ 50,0 fps con un pico del 15 % de CPU |
| 2026-11-08 | Presupuesto de VRAM tras engordar el arte | ✅ 1174 tiles de los 1440 que deja SGDK |
| 2026-11-08 | Arranque del port contra el del original | ❌ el port entraba directo a jugar; el room `_1` trae un oMenu y un oSplashS y `global.started` es false: se empieza en la pantalla de título |
| 2026-11-08 | Secuencia de estados tras el arreglo, con el mando simulado | ✅ título → partida → interludio → partida |
| 2026-11-08 | Recuento de agujeros en pantalla (manchas conexas en la captura) | ✅ 5, los cinco `oAgujerSuper` del room; las manchas de más eran los paquetes pintados con la paleta de los agujeros |
| 2026-11-08 | Color real de lo que espera en el suelo | ❌ `sprPaquete` es cian, `sprWeapon4` azul, `sprWeapon7` azul y amarillo, `sprCruz` casi blanco y la barra del menú `c_gray`: todo se portaba en gris oscuro |
| 2026-11-08 | Paleta del plano de 4 a 14 colores | ✅ los paquetes se ven cian y ya no se confunden con agujeros |
| 2026-11-08 | Agujero compuesto sobre el fondo en vez de binarizado | ✅ recupera el degradado del borde (4 tonos en vez de 1) |
| 2026-11-08 | Rendimiento tras todo el cambio de arte | ✅ sin cambio: 50,0 fps en partida normal (14 % de pico), 48,4 fps en el caso peor (60 % de media, 0 frames por encima) |
| 2026-11-08 | Cabecera del cartucho | ❌ era la plantilla de SGDK: "SAMPLE PROGRAM", `rom_end` de 1 MB para un ROM de 896 KB y SRAM declarada sin usar |
| 2026-11-08 | Cabecera corregida + `tools/check_header.py` | ✅ SURFIZE, `rom_end` 0x0DFFFF exacto, sin SRAM, mando de 3 botones |
| 2026-11-08 | Umbrales de desbloqueo contra `stCheckWeapon()` | ❌ el port iba una kill por delante: el bucle llega a `maxKills - 1`, así que el láser pide 2 kills, no 1 |
| 2026-11-08 | Desbloqueo corregido, récord por récord en el emulador | ✅ 1→1 arma/NEXT 999999, 2→2/NEXT 2, 3→3/NEXT 3, 5→5/NEXT 0 |
| 2026-11-08 | Paquete encima de un agujero | ❌ le recortaba un rectángulo: en el plano el color 0 pinta el fondo, no es transparente |
| 2026-11-08 | Objetos del suelo pasados a sprites | ✅ transparencia correcta y, de paso, el latido del original |
| 2026-11-08 | Mecha del rayo | ❌ 25 frames sin nada en pantalla; el original lanza descargas a los cinco agujeros |
| 2026-11-08 | Relámpagos del rayo portados | ✅ una descarga de puntos por agujero, cambiando de color cada frame |
| 2026-11-08 | Iconos del menú contra `oMenu/Draw_0` | ❌ faltaba `sprFrame`, el marco que el original dibuja encima de cada icono |
| 2026-11-08 | Texto de la pantalla de título | ❌ estaba inventado; ahora es el literal del original con los controles adaptados |
| 2026-11-08 | Posición del logo | ❌ pegado arriba; `oMenu` está en el centro del room y ahí va su sprite |
| 2026-11-08 | `tools/check_codegen.py` con la hoja de sprites nueva | ❌ tres falsos positivos sobre datos de rescomp; filtrado por símbolo, 1042 funciones revisadas |
| 2026-11-08 | Arranque tras pasar el suelo a sprites | ❌ la nave desaparecía: el reparto de tiles de VRAM estaba escrito a mano y el logo del menú se cargaba encima de los suyos |
| 2026-11-08 | Reparto de tiles en ejecución (`VTile_alloc`) | ✅ la nave vuelve; 1254 tiles de 1440 y ningún índice que mantener a mano |
| 2026-11-08 | Medida final | ✅ 50,0 fps en partida normal (18 % de pico), 47,1 fps en el caso peor (69,5 % de media, 0 frames por encima) |
| 2026-11-08 | Recuento de `oAgujerSuper` en el room, por tres vías | ✅ cinco; pero la geometría del anillo deja un hueco de 129 grados donde los demás son de 55-61 |
| 2026-11-08 | Sexto agujero reconstruido en (558, 754) y fórmulas derivadas de `HOLE_COUNT` | ✅ anillo regular de seis; marcado como la única coordenada del port que no sale del GML |
| 2026-11-08 | PIVOT equipado, 500 frames de arena llena | ❌ +0 kills y arena vacía: la munición no se gastaba porque el port la condicionaba a que el arma creara algo, así que nunca entraba la bala por defecto |
| 2026-11-08 | PARA equipado, 500 frames de arena llena | ❌ +0 kills y cero arañas: el haz usaba `Enemy_hit`, que mata, cuando `object1` sólo resta hp sin pasar por `checkMorte()` |
| 2026-11-08 | Las seis armas tras los dos arreglos, 500 frames cada una | ✅ +19/+17/+34/+13/+12/+7 kills, con enemigos vivos en todas |
| 2026-11-08 | `Spr_alloc` con el pool agotado | ❌ devolvía NULL y los módulos escriben sin comprobar; ahora devuelve una ranura sumidero |
| 2026-11-08 | Medida con los seis agujeros | ✅ 50,0 fps en partida normal (18 % de pico); caso peor 46,1 fps, 74 % de media, 3 frames por encima de 460 |
| 2026-11-08 | Firma del juego | ✅ "SCREEN IMPLOSION 2012" en el splash y en la cabecera; no queda ningún "REDEFINETECLAS" en el ROM |
| 2026-11-08 | Metralla con el paso y la velocidad aleatorios que el GML tiene comentados | ✅ de 8 a 24 astillas por ráfaga, velocidad de 0,96 a 15,4 px/f, ángulo de arranque al azar |
| 2026-11-08 | Ráfaga en toda muerte por disparo (desviación pedida) | ✅ explosiones mucho más frecuentes; sin realimentación desde las muertes de la propia metralla |
| 2026-11-08 | Coste de la metralla nueva | ❌ caso peor de 46,1 a 43,9 fps, 17 frames por encima |
| 2026-11-08 | Colisión de punto para las astillas | ✅ 45,0 fps y 7 frames por encima de 450; partida normal intacta a 50,0 fps |
| 2026-11-08 | `PORT_DEBUG_BG` para identificar sprites | ✅ magenta, negro y blanco; fuera de la ROM que se distribuye |
| 2026-11-08 | Histograma de las capturas del juego publicado | ❌ el fondo no es el teal del .yyp sino gris (51,51,51); hay agujeros con reborde rojo y cuatro réplicas a la vez |
| 2026-11-08 | Fondo y paleta rehechos sobre el histograma | ✅ gris (36,36,36) en el VDP y catorce tonos medidos |
| 2026-11-08 | Alto del HUD en las capturas | ❌ 36 px de 600 = 14,4 px en Mega Drive; el port lo pintaba a 8 |
| 2026-11-08 | HUD con los glifos de sFont2 a 16 px | ✅ 19 glifos de 2x2 tiles, 76 tiles |
| 2026-11-08 | Agujero activo con reborde rojo | ✅ segunda variante de tiles, conmutada por la cabeza que tenga encima |
| 2026-11-08 | `MAX_TRI` contra el GML | ❌ el "< 2" era el tope de power-ups en el suelo, no de réplicas; subido a 4 |
| 2026-11-08 | Coste de todo lo anterior | ❌ caso peor de 45,0 a 37,5 fps, por dos repintados de plano |
| 2026-11-08 | HUD por campos fijos + rebote del agujero rojo | ✅ 41,0 fps; partida normal intacta a 50,0 con 39 % de pico |
| 2026-11-08 | Coste real del montador en C, contado sobre el desensamblado | ✅ ~192 ciclos por sprite visible, casi todo accesos sueltos a memoria |
| 2026-11-08 | `src/spr_a.s`: struct reordenado como entrada del VDP y copia con movem.l | ✅ ~146 ciclos, un 24 % menos en el camino del sprite visible |
| 2026-11-08 | Tabla del ensamblador contra la de la referencia en C (`PORT_SPR_C=1`) | ❌ primer intento inválido: en el arnés los dos builds van a fps distinto y las escenas divergen |
| 2026-11-08 | Misma comparación en partida normal, cuatro volcados | ✅ TABLAS IDÉNTICAS, incluidos los subsprites con desplazamiento de la rama múltiple |
| 2026-11-08 | Ganancia medida | ✅ etapa SPRFLUSH 14,4 % → 13,5 %; caso peor 39,8 → 40,3 fps. Menos de lo que yo había estimado: 8.000 de los 20.700 ciclos son recorrer ranuras aparcadas, que el ensamblador no cambia |
| 2026-11-08 | Conversación del proyecto exportada a `docs/CONVERSACION.md` | ✅ 37 turnos, sin las llamadas a herramientas |
| 2026-11-09 | Franjas grises en el interludio | ❌ el texto del plano A agujereaba la barra (color 0 pinta el fondo) y además se saltaban las filas del texto |
| 2026-11-09 | Barra movida al plano B | ✅ banda sólida con el texto encima, como el draw_line_width_color del original |
| 2026-11-09 | Arma de las bolas (oPivot) | ❌ salía una vez por partida: faltaba "if instance_number(oOption) < 1 instance_destroy()", que es lo que hace que el siguiente disparo cree un pivote nuevo |
| 2026-11-09 | Wrap del pivote | ❌ no lo hacía; al cruzar la nave un borde, las bolas se quedaban orbitando lejos |
| 2026-11-09 | Ciclo del pivote muestreado cada 25 frames | ✅ `#####..........###`: vive, cae y vuelve. Tres bolas orbitando confirmadas en pantalla |
| 2026-11-06 | `make` con SGDK en `$GDK` | ✅ `out/rom.bin`, 128 KB |
| 2026-11-06 | `blastem out/rom.bin` | ✅ arranca y es jugable (estado previo del port, NTSC 320×224) |
| 2026-11-06 | Auditoría del GML contra este informe | ✅ 5155 líneas revisadas; constantes de la tabla leídas del fuente |
| 2026-11-06 | FASE 3: `make` con la arquitectura nueva | ✅ sin errores ni avisos |
| 2026-11-06 | FASE 3: `blastem -r E out/rom.bin` (región PAL) | ✅ arranca y sigue jugable; la confirmación de que se ven las 240 líneas es visual, en pantalla |
| 2026-11-06 | FASE 3: presupuesto de RAM/ROM/VRAM/sprites | ✅ 3168 B de RAM, 58,6 KB de ROM, 272 de 420 tiles de sprite, 49 de 80 sprites de hardware |
| 2026-11-06 | FASE 4: `make` tras portar jugador y bala | ✅ sin errores ni avisos propios |
| 2026-11-06 | FASE 4: constantes compiladas contra la tabla de la auditoría | ✅ 1.4375 / 7.6875 px-f, 10 y 25 f, margen 3 px, cajas 7×11 y 4×5 |
| 2026-11-06 | FASE 4: `blastem -r E out/rom.bin` | ✅ arranca y sigue corriendo; jugabilidad pendiente de tu vista |
| 2026-11-06 | FASE 5: `make` tras portar enemigos y oleadas | ✅ sin errores ni avisos propios |
| 2026-11-06 | FASE 5: constantes compiladas contra el GML | ✅ araña 0.7188/3.59, gusanos 1.203–1.875/6.0–9.36 y 1.484/7.44, estados 5/250/500 f, aparición 75 f, agujeros en sus 5 posiciones |
| 2026-11-06 | FASE 5: presupuesto de sprites recalculado | ✅ 69 objetos de 80, 368 tiles de VRAM (pool subido a 600); 20 sprites por línea sin medir |
| 2026-11-06 | FASE 5: RAM tras los historiales de gusano | ✅ 4216 B de 65536 |
| 2026-11-06 | FASE 5: `blastem -r E out/rom.bin` | ✅ arranca y sigue corriendo |
| 2026-11-06 | FASE 6: `tools/gm2md.py` sobre el proyecto GameMaker | ✅ 8 recursos generados; frames verificados uno a uno con píxeles opacos (nave 36–37 px por rotación) |
| 2026-11-06 | FASE 6: `make` | ✅ exit 0; ROM 70,5 KB de código y datos (+8,3 KB de arte) |
| 2026-11-06 | FASE 6: paletas generadas | ✅ fondo 0x0886 = (109,146,146); sprites transparente/negro/blanco/amarillo |
| 2026-11-06 | FASE 6: `blastem -r E out/rom.bin` | ⚠️ arrancaba pero se colgaba en el primer frame por `SPR_setFrame` fuera de rango (ver trampa de rescomp) |
| 2026-11-06 | FASE 6: arreglo con `SPR_setAnim`/`SPR_setAnimAndFrame` | ✅ actividad de hardware del emulador de 1691 B/4 s a 15554 B/4 s |
| 2026-11-06 | FASE 7: `make` tras armas, munición, paquetes y explosiones | ✅ exit 0, ROM 79,9 KB |
| 2026-11-06 | FASE 7: primer arranque | ❌ ADDRESS ERROR en `SPR_update`→`updateFrame` por pedir el frame 1 de una animación deduplicada a 1 frame |
| 2026-11-06 | FASE 7: tras el arreglo | ✅ 22047 B de actividad en 5 s; confirmado por el usuario que se mueve |
| 2026-11-07 | Explosiones: frames pre-escalados (el original escala el sprite con `t`) | ✅ 5 frames de 32×32, `numFrame` verificado en `resources.s`, 1 sprite de hardware por explosión |
| 2026-11-07 | `tools/mdrun.py`: captura de pantalla y mando simulado por el depurador de BlastEm | ✅ PNG del fotograma en `$HOME`, `binddown`/`bindup` mueven y disparan |
| 2026-11-07 | Tabla de sprites del VDP con `vs` | ❌ 79 de 80 entradas ocupadas todos los frames, también las ranuras fuera de pantalla |
| 2026-11-07 | `SPR_setVisibility(..., AUTO_FAST)` en los 5 módulos que crean sprites | ✅ 16 entradas en partida normal; DMA de la tabla de 632 a 128 B por frame |
| 2026-11-07 | Relectura del GML: `oExplosion` crea un `oca` que vive 40 frames | ✅ confirmado el hueco real: el polvo no estaba portado |
| 2026-11-07 | Humareda generada desde `expl_4` + `expl_1` (3 tamaños × 5 frames de 64×64) | ✅ `maxNumTile` 53, `maxNumSprite` 4, sin objetos de sprite nuevos |
| 2026-11-07 | Posición de la explosión en la tabla de sprites | ❌ aparecía fuera de pantalla: GCC 16.2.0 escribía `e->x`/`e->y` en el offset 0 del struct |
| 2026-11-07 | `place()` marcada `NO_INLINE` + `tools/check_codegen.py` sobre el ROM | ✅ `move.w 18(%sp),6(%a2)` / `22(%sp),8(%a2)`; sin más escrituras duplicadas en todo el ROM |
| 2026-11-07 | Secuencia de una muerte, fotograma a fotograma en el emulador | ✅ fogonazo blanco de 16→32 px durante 4 frames y humareda oscura de ~50 px durante 33, en el punto de la muerte |
| 2026-11-07 | Carga de CPU medida en el contador de pantalla, jugando | ✅ 39–56 % en juego normal, pico de 78 % con explosión; 50 Hz sin frames perdidos |
| 2026-11-07 | Cola de DMA durante la humareda (`queueTransferSize`/`queueIndexLimit`) | ✅ 256–1152 B y 2–3 transferencias por frame, tope 8000 B, sin descartes |
| 2026-11-07 | Las 7 armas forzadas una a una en el emulador | ❌ láser y PARA no se veían; las otras cinco sí |
| 2026-11-07 | `Game_start()` contra `startGame()` del GML | ❌ el port reiniciaba `g_weaponIndex`, el original no: el selector del menú no servía de nada |
| 2026-11-07 | Láser restaurado a los 10 segmentos del original | ✅ haz visible de ~60 px; los segmentos caben en el pool de 14 proyectiles |
| 2026-11-07 | Prueba de punta a punta con el mando simulado | ✅ elegir 3WAY en el menú, meter crédito, empezar: `g_weaponIndex` sigue a 2 en partida y dispara el abanico |
| 2026-11-06 | FASE 9: conversión de los 20 sonidos con VLC + cabecera WAV propia | ✅ 679 KB de PCM a 13300 Hz |
| 2026-11-06 | FASE 9: `make` con XGM2 | ✅ exit 0 sin avisos, ROM 896 KB |
| 2026-11-06 | FASE 9: arranque con audio | ✅ actividad del emulador de 22 KB a 211 KB por 5 s con la música sonando |
| 2026-11-06 | FASE 10 (parcial): optimización del gusano | ✅ historial a potencia de 2 (máscara en vez de división) y posiciones cacheadas: ~300 divisiones por frame eliminadas |
| 2026-11-06 | FASE 10 (parcial): tráfico del menú | ✅ de 95 KB a 22 KB por 5 s |
| 2026-11-06 | FASE 8: splash, créditos, interludio y selección de arma | ✅ `make` exit 0 sin avisos, ROM 91,5 KB |
| 2026-11-06 | FASE 8: tráfico al VDP del menú | ✅ de 95226 a 22473 B por 4–5 s tras pintar los elementos estáticos una sola vez |
| 2026-11-06 | FASE 7: PIVOT, réplica y los 3 power-ups | ✅ `make` exit 0 sin avisos, ROM 85,6 KB, 22187 B de actividad en 5 s |
| 2026-11-06 | FASE 7: presupuesto de sprites | ✅ 79 de 80 objetos (jugador 1 + proyectiles 14 + explosiones 4 + arañas 8 + gusanos 52), VRAM 347 de 600 tiles |
| 2026-11-06 | FASE 3: API de SGDK 2.12 comprobada en `$GDK/inc` | ✅ `VDP_setScreenHeight240` (sólo PAL), `IS_PAL_SYSTEM`, `F16_normalizeAngle` admite negativos, `F16_getAngle` da 90 para dy>0 (Y hacia abajo), tabla trigonométrica de 1 grado |
