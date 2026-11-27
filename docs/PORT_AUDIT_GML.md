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
| 2. Jugador | ✅ hecho (2026-11-15), Q8 incluida tras la decisión del usuario sobre el depurador; agujeros, aparición y movimiento revertidos al port el 2026-11-16 |
| 3. Armas y munición | ✅ hecho (2026-11-16); la compra queda para los bloques 4-5 (hasta entonces las cinco armas de MonsterLite están disponibles) |
| 4. Economía en partida | ✅ hecho (2026-11-16) |
| 5. Pantallas | ✅ hecho (2026-11-16), con el logo de Screen Implosion convertido de MonsterLite |
| 6. Guardado en SRAM | ✅ hecho (2026-11-16); **retirado el 2026-11-21** por decisión del usuario (2026-11-20): sin SRAM, el progreso vive en RAM hasta apagar (ver "Sin SRAM") |
| 7. Power-ups | ✅ hecho (2026-11-17) |
| 8. Enemigos (IA, gusano) | ✅ hecho (2026-11-20), con las migas de 8 px por decisión del usuario, los dos arreglos de la verificación y la holo inactiva que muere al contacto (decisión del usuario) |
| 9. HUD, mensajes y fondo por nivel | ✅ hecho (2026-11-20), con el deslizamiento, la sombra en todas las fuentes y SCORE en grande que pidió el usuario; el color de fondo por nivel no se porta (Q16); Q22 resuelta ("!!!" en la fila 12); pregunta abierta: balas que no caben junto a la puntuación (Q23) |
| 10. Logros | ✅ hecho (2026-11-21), con la lista y las recompensas del usuario (Q18 cerrada). Todo invención: MonsterLite no tiene logros |
| 11. Tutorial | ⏳ pendiente (Q19) |
| 12. Efectos y sonido | ✅ hecho (2026-11-21; cuarta entrega 2026-11-23; quinta 2026-11-26), con las correcciones de la verificación y las decisiones del usuario: líneas holo en tramos de puntos (fondo de 504 tiles) en su propio trozo de la tabla del VDP, fuera la muerte del GML y los PCM sin uso, sin música en partida (Q21 revocada), la bomba y los rayos en una muestra cada uno, y la sacudida más fuerte manda. Plan de 50 fps: paso 1 (montador) hecho; el resto, pendiente |

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

**Deslizamiento, sombra y SCORE en grande (decisión del usuario, 2026-11-20).** Comprobado:

- **Entrada** (ROM normal, capturas frame a frame): nada en los frames 31-33; UPGRADE AT asoma en la fila 0 hacia el 34; el grupo baja una fila por frame y queda en su sitio en el 39. ARENA! (fuera del grupo) se va a los 25.
- **Salida** (ROM normal, manteniendo B hasta morir; `slideY` por RAM): baja 15 octavos de píxel por frame (1,875 px/f) y el grupo sale en 5 escalones en **18 f**: `slideY >> 6` redondea hacia abajo, así que en el primer frame ya sube una fila y la fila 4 se va a los 17,1 f. Es fiel a MonsterLite, donde el contenido del grupo acaba a ~33 px de la parte de arriba (lo calculó el verificador); COINS y los mensajes se quitan al entrar en el game over.
- **Tienda y vuelta**: al abrirla el HUD se quita y las minúsculas de la tienda salen bien (con su sombra); PLAY desde la tienda empieza otra partida con el HUD entrando otra vez.
- **Línea de arriba**, en PAL y en NTSC (BlastEm con `-r U`): "BULLETS7 42SCORE", "BULLETS5 1234SCORE", "BULLETS9 123456SCORE" (35 balas recortadas, Q23), "BULLETS9 999999SCORE".
- **Por qué el usuario no veía SCORE.** En el HEAD y en la primera entrega era la fuente del sistema, de 8 px, en la fila de abajo de las cifras, y con el panel de `GFXDEBUG=1` abierto la gráfica del depurador pisa las columnas 38-39 de todas las filas. En las capturas con la configuración de BlastEm del usuario (la misma que usa `tools/mdrun.py`, con su recorte de bordes) se veía, pero pequeña al lado de los glifos de 16 px. Ahora es de la fuente grande.
- **Un fallo del compilador.** Con `bigAttr[(u8)c & 0x7F]`, GCC 16 a `-O3` leía el carácter de `shownS[]` con un `and.l` de 32 bits en una dirección impar (`shownS + 5`) y el 68000 paraba con ADDRESS ERROR al primer cambio de puntuación. Sin la máscara (tabla de 256) lee byte a byte; revisado en el desensamblado de toda la ROM que no queda ningún acceso de palabra a una dirección impar de la RAM.

**La medida depende de la SRAM de BlastEm (encontrado al cerrar el bloque).** `tools/measure.sh` arranca un `rom.bin` y BlastEm guarda la SRAM por nombre de ROM (`~/.local/share/blastem/rom/save.sram`): la comparten todas las pruebas de `out/rom.bin`. Al arrancar, el juego carga de ahí el arma equipada, el récord y el saldo (bloque 6), así que el arnés juega con el arma que dejara la última prueba. Las pruebas de la tienda de este bloque la dejaron en WBLOCK (antes LASER): con eso `measure.sh` pasó de 509/37 a **505/34**, y el HEAD de **508/37 a 508/34** (las bajas cambian: es otra escena). No he tocado ese fichero (no tengo permiso, y es del usuario). Para comparar en condiciones fijas, una copia de `measure.sh` en el scratchpad que no lee ni escribe la SRAM y fija arma, récord (780) y saldo (1550):

| Escena fija | HEAD | Bloque 9 |
|---|---|---|
| LASER (la de la referencia 508/37 y 509/37) | 509 / 37 | **510 / 37** |
| WBLOCK (la que deja ahora la SRAM) | 508 / 34 | **505 / 34** |
| WBLOCK, sin el mensaje de nivel | — | 508 / 34 |
| WBLOCK, sin HUD ni texto | 518 / 35 | 521 / 35 |

Con WBLOCK, las 3 vueltas que se pierden son del mensaje de nivel (5 subidas en 600 frames del arnés: el frame que sube de nivel compone sus filas encima de las bajas). Se probaron sin ganancia: componer una fila por frame (505, y 509 en LASER) y guardar los agujeros de cada fila de texto para no recalcularlos (se queda, no cuesta: `textUnder`). Sin HUD ni texto el bloque 9 va 3 vueltas por delante del HEAD. Para que `measure.sh` vuelva a medir la escena de la referencia basta equipar LASER en la tienda (GEAR) con `out/rom.bin`; para que no vuelva a pasar, `measure.sh` podría fijar la partida como la copia del scratchpad (propuesta, no hecha). **Resuelto el 2026-11-21** al quitar la SRAM (ver "Sin SRAM"): el juego ya no lee nada de `save.sram`, así que `measure.sh` mide siempre la escena de encender (WBLOCK, récord y saldo a 0): **505/34**.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| **Toda baja suma `level`; la araña muerta por bala fina no da `2 · level`** (`S/bulletThinSrc.cs:49`) | Decisión del usuario (Q5): es un fallo del original y se corrige. **Divergencia deliberada** | Puntuación algo más baja que en MonsterLite cuando se juega sin munición contra arañas, y por tanto niveles que llegan un poco más tarde |
| Tope de gusanos 3 en vez de 5 (nivel 10-19) y 8 (20+) | CPU y VRAM, medido arriba (Q3) | Del nivel 10 en adelante hay menos gusanos que en el original |
| Tope de 8 arañas; MonsterLite no tiene | Decisión del usuario (Q4); el pool de 8 ya existía | Las oleadas que piden araña con 8 vivas no crean nada |
| El tipo de gusano se elige con el orden de `generaMalos()` del GML | MonsterLite tiene un solo gusano (Q2, decisión 4) | En los niveles 1-9 sólo salen gusanos de 12 eslabones |
| Las oleadas de power-ups (3, 4, 6, 16, 19-21) no crean nada todavía | Son del bloque 7 | De momento la bomba, los rayos y las réplicas siguen saliendo por `generaPaquete()` del GML al morir cada cabeza |
| La oleada 27 no enseña "!!!" | Es del bloque 9 | **Resuelto en el bloque 9** |
| La metralla del GML (`oDestro`) también puntúa `level` | No existe en MonsterLite; con la regla de Q5 toda baja suma `level` | — |
| La nave aparece en el centro del GML, no en (160, 69) | Es del bloque 2 (Q6) | Resuelto en el bloque 2: nace en (160, 17), ver abajo. Revertido el 2026-11-16: vuelve al centro, (160, 120) |
| Giro, IA, aparición de 75 f y hp siguen siendo del GML | Son del bloque 8 | El enemigo va a la velocidad de MonsterLite pero gira como en el GML. Los eslabones del gusano siguen separados por `stepdelay` frames, así que a la velocidad del nivel 9 (hasta 3,92 px/f) quedan más separados que antes (hasta ~24 px); las migas de MonsterLite, cada 8 px, son del bloque 8 **Resuelto en el bloque 8** (IA, giro, aparición inmediata y migas cada 8 px; el hp de MonsterLite ya venía del bloque 3). |
| HUD provisional: cifras grandes sin la palabra SCORE en grande, y "LV / UP / LAST / SCORE" con la fuente del sistema | La fuente grande no tiene C ni R; el HUD definitivo es el bloque 9 (Q16) | Se lee mal sobre los fondos con mucho detalle, igual que el LAST de antes. LAST pasa a ser la mejor puntuación de la sesión. **Resuelto en el bloque 9**: HUD de MonsterLite (SCORE, BEST, LEVEL, UPGRADE AT); LAST desaparece del HUD |
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
- **Lo de `Update` que no se para en MonsterLite todavía no existe en el port**: caducidad de paquetes y monedas (bloque 4), LASER y MOIRE de impacto instantáneo y bolas CIRCULAR (bloque 3), rayos (bloque 7) e IA de `enemyMov` (bloque 8). Cada uno de esos bloques tiene que sacar lo suyo del corte de `playUpdate`. Está escrito en el comentario de `timeStopped`. **Bloque 8**: el temporizador de `enemyMov` ya sigue con el tiempo parado (`Enemy_tickFrozen`, `Snake_tickFrozen`).
- **La nave no se agranda al congelar.** En MonsterLite, cada frame congelado reproduce `landingAnim` (`gameControl.cs:270`). Como el `Animator` de `player.prefab` va en modo Normal (`m_UpdateMode` 0), la nave se queda a escala ×4 (4,00 × 3,82) mientras el tiempo está parado, y al reanudar vuelve a ×1 en 0,23 s. El VDP no escala sprites: queda pendiente para el bloque 12 (efectos), con frames pre-escalados o descartado. **Divergencia**.
- **La sacudida** de MonsterLite seguiría, pero la del port es todavía la del GML (`stShake`, que vive en el Step de `oPlayer`) y se para con la nave. El desplazamiento se aplica al colocar cada sprite (`inc/spr.h`), así que mover el plano con los sprites quietos los descuadraría. La sacudida de MonsterLite es del bloque 12.
- **El aviso**: "HOLD B" con la fuente del sistema, centrado, en la fila 20 del plano A (160 px; MonsterLite empieza en 157). Va compuesto encima del agujero de abajo (ver arriba). La mano (`dedo`) no tiene equivalente con el pad y no se porta. La música de partida no existe todavía (bloque 12), así que tampoco hay cambio de tono.
- **No se congela con `PORT_STRESS` ni con `PORT_BENCH`**: el arnés mide el caso peor con todo en marcha y `tools/measure.sh` no pulsa B, así que mide lo mismo que antes sin cambiar el script. Los pilotos que juegan una partida normal por el depurador (los del verificador) tienen que mantener B pulsado.
- **Con el panel del depurador de gráficos abierto** (`GFXDEBUG=1`) tampoco se congela, por decisión del usuario: B sigue siendo del depurador (plano B). Con el panel cerrado, B vuelve a ser el del tiempo. `GfxDebug_panelOpen`, `src/gfxdebug.c`.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| En diagonal, 4 px/f en vez de 5,66 | Decisión del usuario (Q7). **Divergencia deliberada**. **Revertido el 2026-11-16**: movimiento del GML, ver "Movimiento del GML y fondo verde" | La diagonal es un 29 % más lenta que en MonsterLite; las ocho direcciones van igual |
| Hexágono centrado, no 51 px por debajo del centro | Decisión del usuario (Q6) **Revertido el 2026-11-16**, ver "Reversión de agujeros y aparición". | Todo queda 51 px más arriba que en el dispositivo apaisado |
| La nave nace en (160, 17), debajo del HUD provisional | Consecuencia de Q6: 102,9 px por encima del centro del hexágono. Confirmado y aceptado por el usuario hasta el bloque 9 **Revertido el 2026-11-16**, ver "Reversión de agujeros y aparición". | Al empezar, la nave queda tapada en parte por "LAST" (fila 2) y la cifra de BULLETS: blanco sobre blanco, sólo se distingue la sombra. En cuanto se mueve se ve |
| Los cuatro agujeros de los lados se pintan 4 px más lejos del centro | Rejilla de 8 px del plano A **Revertido el 2026-11-16**, ver "Reversión de agujeros y aparición". | Sólo visual: nacimiento, IA y colisión usan la posición exacta |
| Círculo de la nave contra las **cajas** de los enemigos del GML | Las esferas de los enemigos (8,4 y 9,6 px) son del bloque 8 | El contacto llega a 3 px del borde de la caja en recto, por los cuatro lados, y a 2 + 2 en diagonal. Desde arriba la caja vieja de 7×11 ya mataba a 6 px: ahora se puede pasar más cerca por arriba. Los 12 px entre centros de MonsterLite (araña) llegan con el bloque 8 **Resuelto en el bloque 8**: esfera de 3,6 px contra las de 8,4 y 9,6 px (12 y 13,2 px entre centros). |
| La recogida usa el círculo contra la caja de paquete del GML (11×11) | La caja de 20×20 de MonsterLite es del bloque 4 | Hay que pasar más cerca que en MonsterLite, y que antes con la caja de 7×11 |
| El orden de los agujeros es el de `m_Children` del prefab | MonsterLite no depende del orden; la IA del GML sí (bloque 8) **Revertido el 2026-11-16**, ver "Reversión de agujeros y aparición". | El enemigo que vaga salta a (120, 100), el agujero 0. Las arañas ya no dependen del orden: nacen en uno al azar, como en MonsterLite (resuelto en esta entrega) **Bloque 8**: la IA del GML ya no existe, así que el orden de los agujeros ya no importa. |
| La nave nace **mirando arriba**; en MonsterLite, a la derecha (`Quaternion.identity`) | Decisión del usuario (bloque 2). **Divergencia deliberada** | Sólo se ve los 50 f hasta que sale el primer gusano |
| La congelación es con B y no con el dedo; aviso "HOLD B" sin la mano | Decisión del usuario (Q8): no hay pantalla táctil | Hay que mantener B pulsado para jugar |
| La gracia antes de la primera congelación es de 25 f | Deducida (dos llamadas a `OnGUI` por frame), no medida: en MonsterLite depende de cuántas veces llame Unity a `OnGUI` | Entre 0 y 25 f de diferencia con el original al empezar |
| Con el tiempo parado también se paran la sacudida (todavía la del GML) y las cosas del GML que aún no tienen su versión de MonsterLite | Ver arriba: lo de `Update` llegará con sus bloques | Nada cuenta mientras está parado: lo del suelo no caduca (tampoco caduca con el tiempo en marcha, bloque 4) |
| Con el panel de `GFXDEBUG=1` abierto no se congela | Decisión del usuario | Sólo en la build de depuración |
| El texto sobre los agujeros (CREDITS, HOLD B) va en blanco de la paleta de los agujeros y sus celdas enteras con prioridad alta | Una celda del plano sólo tiene un tile y una paleta | En esas celdas, un sprite que pase por encima del agujero queda debajo del dibujo del agujero mientras se ve el texto |
| El sexto agujero reconstruido del GML (558, 754) ya no se usa | Lo sustituye el hexágono de MonsterLite **Revertido el 2026-11-16**, ver "Reversión de agujeros y aparición". | — |

### Reversión de agujeros y aparición (2026-11-16)

Decisión del usuario al cerrar el bloque 2 (`docs/MONSTERLITE_DECISIONES.md`), que **sustituye a Q6** y a la aparición en (160, 17): los agujeros y la aparición de la nave vuelven a como estaban en `4628c56`, antes del bloque 2. El resto del bloque 2 se queda: velocidad, wrap, radio, congelación con B y arañas naciendo en un agujero al azar.

- **Agujeros** (`src/hole.c`): otra vez el anillo del port, los cinco `oAgujerSuper` del room `_1` más el sexto reconstruido (558, 754), en el mismo orden que antes (el reconstruido primero), con `GML_POS`. Cada bloque se pinta en la celda más cercana, `(v − 32 + 4) / 8`, como antes del bloque 2 (hasta 4 px de diferencia).
- **Aparición** (`PLAYER_SPAWN_X/Y` en `inc/arena.h`, `src/player.c`): en el centro de la pantalla, (160, 120), como `startGame()` del GML. Desaparecen `ML_HOLES_CX/CY`, `ML_UNIT_PX` y `ML_PLAYER_SPAWN_*`.
- **Se conserva lo que el bloque 2 añadió a los agujeros sin depender del hexágono**: sólo se suben y se pintan las celdas con dibujo (43 por variante), una celda sólo se compone si tiene dibujo de dos agujeros, y el texto (CREDITS, HOLD B) y el logo se componen encima de los agujeros. Con el anillo hay **4 celdas compuestas** (30 con el hexágono, 33 con el algoritmo anterior al bloque 2), así que quedan más tiles libres.

**Divergencias que introduce:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Agujeros en el anillo del port, no en el hexágono de MonsterLite | Decisión del usuario al cerrar el bloque 2. **Divergencia deliberada** | Otra disposición: anillo de ~58 px de radio alrededor del centro, frente al hexágono de 40-45 px |
| El sexto agujero sigue siendo una reconstrucción, no sale del GML ni de MonsterLite | El room del GML sólo trae cinco (ver "El sexto agujero") | — |
| La nave nace en el centro de la pantalla, no en `highPos` | Decisión del usuario. **Divergencia deliberada** | Empieza en medio del anillo y fuera del HUD |

### Movimiento del GML y fondo verde (2026-11-16)

Decisiones del usuario (`docs/MONSTERLITE_DECISIONES.md`, 2026-11-16):

- **Movimiento de la nave como en el GML**, sustituyendo a Q7 y al wrap sin margen del bloque 2: `PLAYER_ACC = GML_SPEED(6)` = 92/64 = 1,4375 px/f por eje (1,44 en el original, −0,17 %), sin normalizar la diagonal (2,03 px/f en diagonal, un 41 % más), y `PLAYER_WRAP_OFFSET = GML_LEN(15)` = 3 px (`inc/arena.h`, `src/player.c`). El pivote del GML sigue usando `Player_padDelta`, así que va pegado a la nave. Desaparecen `ML_PLAYER_SPEED` y `ML_PLAYER_SPEED_DIAG`. El radio de 3,6 px del bloque 2 se queda.
- **Fondo en verde oscuro y con menos brillo**: `src/bg.c` usa su propia paleta (`bgRuinsGreen`) en vez de la gris que genera `tools/bg2md.py`. Los mismos 8 tonos, en el mismo orden y todos distintos, de 0x000 a 0x282 (antes de 0x000 a 0xEEE). Regenerar el fondo con `bg2md.py` no la pisa. Es una **invención estética** pedida por el autor: no sale ni del GML ni de MonsterLite.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Nave a 1,44 px/f por eje (2,03 en diagonal), no a 4 px/f de MonsterLite | Decisión del usuario. **Divergencia deliberada** de MonsterLite; igual que el GML | La nave va a un 36 % de la velocidad de MonsterLite |
| Wrap con margen de 3 px | Decisión del usuario; igual que el GML | Reaparece 3 px antes del borde |
| Fondo verde oscuro | Petición del autor. **Invención** | Sólo visual |
| Agujeros verdes: relleno 0x020 (antes negro) y borde 0x042 (antes grises 0x222/0x444); reborde rojo intacto | Petición del autor. **Invención** | Sólo visual. Como PAL2 la comparten el logo, el HUD y los iconos de arma, `src/hole.c` pasa los índices 1-3 de los agujeros a 14 y 15, que no usaba nadie (`greenTile`), y el resto no cambia de color |

### Bloque 3: armas y munición (2026-11-16)

Decisiones que aplica: Q9 tal como la matizó el usuario al cerrar el bloque 2 (una sola versión de WBLOCK, LASER y TWAY, con los valores de MonsterLite y por compra; PIVOT y PARA siguen del GML y por récord), Q10 (la munición se conserva hasta apagar), Q11 (iconos de CIRCULAR y MOIRE desde MonsterLite) y Q12 (cadencia propia de CIRCULAR, 3 bolas y WBLOCK hacia donde apunta; la de las monedas es del bloque 4).

**Qué cambia:**

- **Armas** (`inc/weapon.h`): WBLOCK, LASER, TWAY, PIVOT, PARA, CIRCULAR, MOIRE, en el orden del selector. Las de MonsterLite usan sus valores (`ML_SHOT_*`, `ML_BULLET_*`, `ML_LASER_*`, `ML_CIRC_*` en `inc/arena.h`); el arte sigue siendo el del GML (decisión 3): la bala del WBLOCK con el dibujo de `oWBlock` y la del TWAY con el de `o3way`. El GML LASER (segmentos de 7 f) y el 3WAY del GML desaparecen.
- **Disparo** (`src/game.c`, `S/disparo.cs:32-160`): sólo cuenta y dispara si hay enemigo; `shootTime` empieza en 4 y baja 1 por frame; al llegar a 1 dispara el arma con munición (y gasta 1) o la bala fina sin ella. Cadencias 8 (fina) / 4 (WBLOCK) / 5 (LASER) / 3 (TWAY) / 2 (MOIRE) / 4 (CIRCULAR, Q12) f; PIVOT y PARA, 5 f del GML. Desaparecen el primer disparo a los 25 f y la bala fina extra del frame en que se acaba la munición (GML).
- **Apuntado**: el objetivo es el de `getClosest` (`S/enemyController.cs:194`): arañas y **cabezas**, no eslabones. Con el del GML (eslabón más cercano) y balas que atraviesan eslabones, la bala fina hizo **0 bajas en 600 f**. Las ocultas en su agujero siguen contando, como antes.
- **Balas** (fina y gorda): 12 px/f, caja de 16×16 centrada, mueren al salir de pantalla. **Atraviesan los eslabones.** Contra la cabeza, regla de MonsterLite: si `hp < 0` muere; si no, resta (fina 1, gorda 3). La gorda que no mata sigue y no vuelve a tocar esa cabeza (`OnTriggerEnter` sólo salta al entrar: máscara `ignore`). Con `hp` 4 (`ML_SNAKE_HP`, ahora en los dos gusanos; el GML daba 4 y 2) la fina mata al 6.º impacto y la gorda al 3.º. La araña muere de cualquier impacto. Cada impacto hace explosión, y cada baja, la metralla (petición del autor).
- **LASER y MOIRE** (`S/RaycastScr.cs`): impacto instantáneo. Mientras viva el rayo (25 f el del LASER, 7 f el del MOIRE) se lanza cada frame desde la nave en su rumbo, sin distancia máxima, y mata al primer objeto de la línea si es araña o cabeza. Tapan: eslabones, agujeros (17,6 px), lo que hay en el suelo y las balas finas en vuelo (`Coll_rayCircle`, `*_rayFirst`). El haz visible son 16 puntos amarillos (`dot.png` del GML) cada 20 px hasta el borde, 2 f (LASER, quieto donde se disparó) o 7 f (MOIRE, sigue a la nave).
- **CIRCULAR** (`S/circCont.cs`, `S/circularScr.cs`): 3 bolas a 20 px de la nave (0°, 120°, 240°). Con munición se sueltan y van a 24 px/f a la araña o cabeza visible más cercana a ellas; sin munición orbitan a 2,4°/f. Una suelta no vuelve a orbitar. Esfera de 4 px; resta 3 a la cabeza y muere en el impacto; la explosión sólo sale si queda menos de 2 enemigos.
- **Munición por arma** (`g_ammo[]`): paquete de 10 / 4 / 10 / 4 / 3 (WBLOCK, LASER, TWAY, CIRCULAR, MOIRE) y 5 / 8 (PIVOT, PARA, GML). La del arma equipada vive en `g_playerBullets` durante la partida y vuelve a `g_ammo` al morir; no se reinicia al empezar (Q10).
- **Selector del menú**: siete iconos; el cursor salta las armas que no se pueden elegir. PIVOT (récord > 80) y PARA (> 100) siguen el desbloqueo por récord; NEXT enseña el umbral del siguiente. Marquesinas nuevas: " CIRCLES " y " MOIRE " (nombres de la tienda). Voz sólo para las cinco del GML.
- **Iconos** (`tools/ml_icons.py` → `res/sprite/weapons_ml.png`): el glifo blanco de `iconCirculo.png` e `iconMoire.png`, recortado y reducido a 18 px, dentro del marco de 32×32 de los iconos del GML y con sus grises. 32 tiles de VRAM normal (la sobrante está llena).
- **Pool de proyectiles**: 14 → 36 (TWAY ≤ ~35 desde una esquina, §2.5). Medido con 24: el caso peor no mejora (388 frente a 384), así que no se recorta.

**Divergencias que introduce:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| ~~Las cinco armas de MonsterLite están disponibles sin comprar~~ | Resuelto en el bloque 5: se compran en la tienda | — |
| En el frame en que el rayo mata, la baja cuenta una vez | En el original todos los `RaycastObj` vivos golpean al mismo enemigo (Destroy diferido) y cada uno suma y suelta premio | Menos puntos y premios que el original con LASER y MOIRE en fuego sostenido |
| El rayo se ordena por la proyección del centro de cada collider, no por el punto de entrada | Más barato; sólo cambia el orden de dos objetos casi solapados en la línea | — |
| El haz son puntos amarillos, no una línea | El VDP no dibuja líneas; el segmento de láser del GML a escala 0,2 (1×6 px) no se veía | Línea de puntos cada 20 px |
| Las bolas sólo persiguen enemigos visibles | Los enemigos que el port esconde 75 f en su agujero (GML, hasta el bloque 8) no se pueden tocar; las bolas se quedaban encima y llenaban el pool (caso peor 340/600) | Sin enemigo visible, la bola se destruye (como sin enemigos en MonsterLite) **Bloque 8**: los enemigos ya nacen visibles; las bolas persiguen a cualquiera (`Enemy_closest`). |
| Las cajas de los enemigos siguen siendo las del GML; el rayo usa los radios de MonsterLite (8,4 / 9,6 / 17,6 px) | Las esferas de los enemigos son del bloque 8 | Una bala y el rayo pueden no coincidir en el borde de un enemigo **Resuelto en el bloque 8**: balas y rayo usan las mismas esferas. |
| Desplazamiento del WBLOCK de 6 px (5,8) | `sprite.bounds.size.x / 2` del sprite de la NAVE (`disparo` está en ella): `nave.png`, 58 px a 100 px/u, leído con UnityPy el 2026-11-16 (la primera entrega usaba 12 px, deducidos de la bala) | Redondeo de 0,2 px |
| Sin premio (moneda o paquete) por baja ni por bola contra araña; sigue el `generaPaquete()` del GML por cabeza | Los drops son del bloque 4 | — |
| Sin hitstop de 30 ms por baja con bala, sin la sacudida pequeña de MonsterLite (siguen las del GML) y sin los sonidos de MonsterLite (siguen `laserSnd`, `shotSnd`, `wBlockSnd`, `threeWaySnd`) | Bloque 12 (Q17) | — |
| La metralla del GML sigue restando 1 con la regla del GML (resta y luego mira) | Es del GML, a petición del autor | Un gusano tocado por la metralla muere con menos impactos de bala (medido: 3 o 5 con la fina) |
| El tipo del paquete se decide al recogerlo, no al aparecer | El arma no se puede cambiar en partida | Ninguna |

**Medición del caso peor** (`PORT_STRESS=1`, como `tools/measure.sh` pero forzando arma y munición por el depurador; vueltas de 600, más es mejor):

| Arma | Vueltas | Bajas |
|---|---|---|
| Bala fina (sin munición) | 465 | 27 |
| WBLOCK | 491 | 28 |
| LASER | 491 | 28 |
| TWAY | **384** | 24 |
| CIRCULAR (antes de limitar las bolas a enemigos visibles: 340) | 516 | 27 |
| MOIRE | 448 | 24 |

Referencia: 533/15 con la bala del GML antes del bloque 3, pero con menos bajas no es la misma partida (las bajas traen explosiones y metralla). Con `PORT_PROF=2`, el TWAY gasta en proyectiles ~85 líneas por frame frente a ~39 de la fina: 36 balas comprobando 8 arañas y 3 cabezas cada una. Es el arma más cara; si hace falta, la palanca es el barrido de colisiones de las balas, no el tamaño del pool.

### Bloque 4: economía en partida (2026-11-16)

**Qué cambia:**

- **Lo que suelta un enemigo** (`Pow_drop`, `leavePow` de MonsterLite): por cada baja con disparo, 50 % moneda y 50 % paquete, en la posición de la bala (o del enemigo, con el rayo). La bola CIRCULAR contra una araña suelta siempre paquete. **`generaPaquete()` del GML ya no se usa**: no salen paquetes por cabeza muerta ni los power-ups del GML (bomba, réplicas, rayos) hasta que lleguen con sus oleadas (bloque 7). Las muertes por metralla no sueltan nada.
- **Paquete**: la munición es la del arma equipada **al aparecer** (`bulletType`), con los valores del bloque 3. Caduca a los 167 f (`lifeTime` 200 de Update). Al recogerlo da la munición en el acto, sube a 2,4 px/f y se acaba en pocos frames (la vida baja 31 por frame). Caja de 20×20 centrada. Ya no muere sobre un agujero (eso era del GML; los power-ups del GML lo conservan).
- **Moneda** (`tools/ml_coin.py` → `res/sprite/coin.png`, Q11): `coinAnimation.png`, 6 frames reducidos a 13 px (12,8 en MonsterLite) en celdas de 16×16, con el tinte dorado del prefab (`m_Color` 0,937 / 0,788 / 0,302, leído con UnityPy) y la paleta de sprites del port; anima a 12 por segundo (`coinAnim`, un frame cada 4,17 f). Vale 50 o 60 hasta el nivel 10 y 50, 60, 150, 180 o 100 desde el 11, con el nivel de cuando aparece. Caduca a los 167 f. Al recogerla suma al saldo (`g_coins`) y a las de la partida (`g_coinsEarned`, a 0 al empezar) y vuela hacia una pantalla por debajo del centro de abajo a 3,2 px/f (sale de la pantalla y se acaba). Pool de 16.
- **Q12**: la moneda que nace sobre un agujero u otra moneda se queda donde cae (en el original salía disparada 900 px y se perdía).
- **Tiempo parado** (Q8): en MonsterLite la caducidad y la subida del paquete van en `Update` sin `deltaTime`, así que siguen con el tiempo parado; el vuelo de la moneda usa `deltaTime` y se para. Aquí igual (`Pow_tickFrozen`). Con el menú puesto también caducan.
- **El rayo** de LASER y MOIRE choca también con las monedas.
- **HUD**: `COINS <saldo>` en la última fila, desde el centro, con la fuente del sistema (provisional hasta el bloque 9).

**Divergencias que introduce:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Las armas del GML (PIVOT, PARA) y las balas de las réplicas también sueltan moneda o paquete | Sin `generaPaquete()` no tendrían de dónde sacar munición | — |
| La metralla no suelta nada | No es un disparo de MonsterLite | Algunas bajas sin premio (8 de 119 en 3000 f con WBLOCK) |
| La moneda no crece al recogerla (×1,1 por frame hasta ×2) | El VDP no escala sprites | — |
| COINS en blanco y en la fila 29 | Fuente del sistema; en MonsterLite va en amarillo | No se ve en NTSC (224 líneas). **Resuelto en el bloque 9**: el saldo en amarillo, en la fila 28 (26 en NTSC) |
| Sin `Thread.Sleep` ni sonidos de MonsterLite al recoger | Bloque 12 | Suena el `powUpSnd` del GML |

**VRAM**: la moneda precarga sus 6 frames, 24 tiles: `nextTile` 1433 de 1440, **quedan 7**. El HUD del bloque 9 tendrá que sacar tiles del fondo (`bg2md.py` con menos presupuesto) o de lo que libere el GML.

### Bloque 5: pantallas (2026-11-16)

**Qué cambia:**

- **Splash sin créditos** (Q13): el del GML (logo y marquesina) con "PRESS START" parpadeando donde iban los créditos (21 f encendido y 21 apagado: el "TAP TO START" de MonsterLite, `t % 100 < 50` contado por `OnGUI`). START empieza. La marquesina dice "PRESS START". `g_credits` desaparece.
- **Game over de MonsterLite** (`S/gameOverSrc.cs`) en vez del interludio KILLS/TOP/NEXT del GML, dentro de la misma banda y con la fuente del sistema (Q16): SCORE, COINS COLLECTED, LEVEL: n!, NEW RECORD! (si se supera el récord de antes de la partida, `g_bestAtStart`), BEST SCORE, y las opciones PLAY / GEAR (izquierda y derecha; A, C o START eligen). PLAY no aparece hasta que SCORE ha terminado. "LAST SCORE" no se dibuja (Q15).
- **La cuenta** (Q15, el ritmo acelerado del original): en vez de corrutinas, se cuenta cuántas hay en el bucle de SCORE y cuántas en el de COINS, agrupadas por el frame (módulo 24) en que despiertan; corre a 60 Hz (6 pasos cada 5 frames). Simulada contra §2.8: 100/60, 1000/300, 5000/500 y 20000/3000 acaban exactamente en los frames de Update de la tabla. Un tick del PSG por frame en que sube SCORE (el "ding", uno por frame en vez de uno por corrutina).
- **Fogonazo rojo**: con más de 10 puntos el fondo se pone rojo puro al entrar y vuelve a su paleta con la fracción `counterK / 60` por frame de Update (~0,2 s). Q16: es la imagen de ruinas la que se tiñe.
- **Tienda GEAR** (`S/StoreMenu.cs`, versión gold), en una banda de las filas 3 a 27: las 4 armas de pago con su nombre y descripción de la tienda (`W/GameAssets.cs`) y "<precio> COINS" (Q14: 6000 / 12000 / 24000 / 48000) o "AVAILABLE!"; "YOUR COINS"; el estante con los iconos de las armas que se pueden elegir y "SELECT YOUR GEAR: <arma>" (izquierda y derecha); "NEXT GEAR AT n KILLS" para PIVOT y PARA (récord del GML); SOUND ON/OFF, MUSIC ON/OFF y PLAY. Arriba y abajo recorren; A, C o START eligen. Comprar descuenta y **equipa** (`setMeOnly`); sin saldo no pasa nada. WBLOCK, comprada de serie. Se descartan el leaderboard, el GEAR de la tienda (no hace nada) y los iconos decorativos de bomba y rayos.
- **El selector de armas pasa a la tienda** (el estante): el game over ya no lo tiene, como en MonsterLite.
- **Sonido y música** (`g_soundOn`, `g_musicOn`, `src/audio.c`): apagan efectos, voces y tick, y la música del menú. Se guardaron en SRAM en el bloque 6 hasta que se quitó (ver "Sin SRAM"); ahora duran hasta apagar.

**Divergencias que introduce:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| ~~Sin el logo de Screen Implosion~~ | Resuelto: el usuario reabrió la decisión 3 para él (ver "Logo de Screen Implosion") | — |
| El splash sigue siendo el del GML (logo SURFIZE y marquesina), no la textura `splash` de MonsterLite con su oscilación | Decisión 3 (arte del GML) | — |
| Un "ding" por frame | Q15 | — |
| La cuenta empieza unos 5 f tarde | Los primeros frames del game over cargan con la muerte (medido con 5000/500: 88 y 150 f frente a 83 y 144) | Imperceptible |
| Sin la entrada deslizándose a 1600 px/s, ni voces "AWESOME" / "CARALLUDO", ni la sacudida al tocar START | Bloque 12 y animación de `OnGUI` | — |
| PLAY y GEAR se eligen con el pad | No hay pantalla táctil | — |

**Rendimiento**: la banda del menú se repintaba entera, celda a celda, en cada frame mientras crecía (hasta 512 escrituras al VDP por frame) y la cuenta del game over iba al 37 % de su ritmo (`updTick` +22 en 50 f). Ahora sólo se pintan las filas nuevas, de una escritura por fila: +60 en 50 f. Los números se escriben con una conversión propia (`fmtU32`) en vez del `sprintf("%lu")` de SGDK.

### Logo de Screen Implosion (2026-11-16)

Decisión del usuario: el logo se convierte desde el arte de MonsterLite. `tools/ml_logo.py` → `res/sprite/silogo.png` (IMAGE `silogoImage`): `screenImplosionLogo_inv_Square_1800.png` (blanco sobre transparente) reducido por áreas a 108×108 px en 16 grises. Medidas leídas con UnityPy de `logo.unity`: el sprite mide 18 u (1800 px a 100 px/u) con una cámara ortográfica de tamaño 20 (40 u de alto) → 108 px de 240, y va 2 u (12 px) por debajo del centro de la cámara. `Logo_show` (`src/logo.c`) lo enseña al arrancar, sobre negro, 40 f (`WaitForSeconds(0.8)`), con el fundido de entrada del original (alfa de 0 a 0,8, `fadeTime += deltaTime`) hecho con la paleta, y pasa al splash de golpe (el fundido de salida está comentado en el original). Va antes de `Game_init`, con la VRAM libre: no gasta tiles del juego. Usa PAL0, la de la fuente del sistema, y la repone al acabar. Las builds de medida (`PORT_STRESS`, `PORT_BENCH`, `PORT_PROF`) se lo saltan para que sus frames sigan siendo los mismos.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| El fundido va en los 8 niveles del VDP | Paleta de 3 bits por componente | Sube a saltos |
| Empieza unos 15 f después de encender | Lo que tarda en cargar el driver de sonido (`Audio_init`) | — |

**Cambios del 2026-11-27 (decisión del usuario: "los fundidos del principio del logo... se va al corte").**
- `Logo_show`: entrada de 40 f hasta el blanco entero (antes llegaba a 0,8, como el original), 30 f quieto y salida a negro en 25 f. Al acabar apaga la pantalla (`VDP_setEnable(FALSE)`), que enseña el color 0, todavía negro.
- `src/main.c`: sin el arnés, el color 0 no se pone en `ROOM_BG_COLOR` al arrancar. Tras `Game_init` se vacía la cola de DMA, se copian las 64 entradas de la CRAM (con el 0 en `ROOM_BG_COLOR`), se pone todo en negro, se enciende la pantalla y `PAL_fadeInAll` (asíncrono, `MENU_FADE_IN_FRAMES` = 20) funde el menú desde negro. Antes se veía el fondo gris y el menú montándose durante ~30 f.
- Capturas de la secuencia (BlastEm): logo subiendo, logo, logo bajando, negro, menú fundiendo, menú.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| El logo llega al blanco y sale fundiendo; el menú entra fundiendo | Decisión del usuario | El arranque dura ~1,9 s más fundido del menú (0,4 s), en vez de 0,8 s con corte |

### Logo ARENA en el menú (2026-11-27)

Decisión del usuario: en el menú va el logo de ARENA, no el de SURFIZE (sprite176 del GML). Es el de MonsterLite: `S/menu.cs` dibuja la textura `splash` del prefab `Prefabs/States/Menu.prefab`, que es `Assets/Art/splash_alt.png` (ARENA blanco sobre transparente, dibujo de 974×818 en 1200×1200; referencia encontrada por GUID en el prefab). `tools/ml_arena_logo.py` → `res/sprite/logo.png` (56×56, 7×7 tiles, grises de HOLE_PAL); `tools/gm2md.py` ya no escribe ese fichero. El código del menú (`drawLogo`, composición con los agujeros) no cambia.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| ARENA a 56×47 px en el hueco de 7×7 del logo del GML; MonsterLite lo pinta a pantalla completa (`ScaleToFit`) | No queda VRAM: los tiles del menú van a la VRAM sobrante y el logo reutiliza sus 49 | Logo pequeño en el centro del menú |
| El arte sale de MonsterLite y no del GML | Decisión del usuario (como la moneda y los iconos, Q11) | — |

### Bloque 6: guardado en SRAM (2026-11-16)

`inc/save.h`, `src/save.c`. Se guarda: firma, versión, secuencia, récord de puntuación (`g_lastScore`, que pasa a ser el récord guardado), saldo, armas compradas, arma equipada, partidas jugadas, sonido y música, el récord de bajas del GML (`g_maxKills`, que desbloquea PIVOT y PARA) y hueco para los logros del bloque 10. La munición no (Q10).

- **Dos copias alternas** de 64 bytes en los bytes impares, con número de secuencia y suma de control (Fletcher de 16 bits). Al arrancar se carga la válida con la secuencia más alta; si ninguna vale (cartucho nuevo, SRAM corrupta, otra versión), los valores por defecto.
- **Cuándo**: al entrar en el game over (`setData` de MonsterLite, con `numberOfGames += 1`), al recoger cada moneda (el saldo de Soomla se guardaba en el acto), al comprar y al salir de la tienda con PLAY.
- **Cabecera** (`src/rom_header.c`): SRAM "RA", tipo 0xF820, 0x200001-0x203FFF (8 KB). `tools/check_header.py` lo comprueba, y también que el ROM no se solape con ella. **El ROM había crecido a 1 MB** (0x100000) en los bloques 3-5 y `rom_end` seguía en 0x0DFFFF: corregido a 0x0FFFFF.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Al salir de la tienda también se guarda | En MonsterLite el arma y el sonido esperaban al game over siguiente | No se pierde si se apaga a mitad de partida |
| Se guarda el récord de bajas | Desbloquea PIVOT y PARA (GML); el GML no guardaba nada | Esas dos armas siguen desbloqueadas al volver a encender |
| Hasta el bloque 6 la munición se conservaba hasta apagar; sigue igual | Q10 | — |

**Aviso para probar**: BlastEm guarda la SRAM en `~/.local/share/blastem/<nombre del ROM>/save.sram`. La carpeta `rom/` (la de `out/rom.bin`) ya tenía un `save.sram` de 32 KB del 8 de noviembre, de cuando la plantilla de SGDK declaraba SRAM: no lleva la firma, así que el juego arranca con los valores por defecto y lo sobrescribe en el primer guardado. Las pruebas de este bloque se hicieron con una copia del ROM llamada `savetest.bin` (carpeta `savetest/`) para no tocar ese fichero.

### Sin SRAM (2026-11-21)

Decisión del usuario del 2026-11-20 (`docs/MONSTERLITE_DECISIONES.md`): **el juego no usa SRAM**. El progreso se guarda sólo en RAM y se pierde al apagar. Se quita el bloque 6 entero:

- Fuera `src/save.c` e `inc/save.h`. `Save_load` y `Save_write` desaparecen (todo lo que guardaban ya eran variables en RAM: `g_lastScore`, `g_coins`, `g_weaponsBought`, `g_weaponIndex`, `g_maxKills`, `g_soundOn`, `g_musicOn`). También `g_gamesPlayed` (`numberOfGames`), que sólo se contaba para guardarlo y no se enseña en ningún sitio.
- Al encender, los valores de una partida nueva, los mismos que ponía `Game_init` antes de cargar: récord 0, récord de bajas 0, saldo 0, sólo WBLOCK comprada y equipada, sonido y música encendidos.
- **Cabecera** (`src/rom_header.c`): `sram_sig`, `sram_type`, `sram_start` y `sram_end` en blanco (12 espacios), que es como se marca un cartucho sin RAM de respaldo. `tools/check_header.py` comprueba ahora que la cabecera **no** declara SRAM. El único símbolo de SRAM que queda en la ROM es `SRAM_disable`, que llama el arranque de SGDK (`$GDK/src/sys.c`).
- **`tools/measure.sh`** ya no depende de `~/.local/share/blastem/rom/save.sram`: el juego no lee la SRAM y, sin "RA" en la cabecera, BlastEm no la monta ni la escribe (el fichero no cambia de hash ni de fecha tras una medida). Mide siempre la escena de encender: **505/34**, igual que con la SRAM en WBLOCK. Ojo: un HEAD anterior a este cambio sí la lee.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Récord, saldo, armas compradas, arma equipada, sonido y música no se guardan | Decisión del usuario (2026-11-20); en MonsterLite iban a PlayerPrefs y al inventario de Soomla | Se pierden al apagar la consola; mientras está encendida se conservan entre partidas |
| No se cuentan las partidas jugadas (`numberOfGames`) | Sólo servían para guardarlas | Ninguna |

### Botón RESET (2026-11-21)

**Fallo (ya existía antes de quitar la SRAM):** al pulsar RESET salía el error fatal "VRAM: 1753 de 1425 tiles". SGDK entra tras un RESET por `_reset_entry` (`$GDK/src/sys.c:572`), que llama a `main(FALSE)` **sin borrar la RAM ni volver a copiar `.data`**. `nextTile` (`src/vramspr.c`) seguía contando desde donde lo dejó el arranque anterior (1432 → 1769) y `VTile_check` paraba el juego. Además, `Game_init` no repone `g_coins`, `g_ammo`, `g_soundOn` ni `g_musicOn`, y hay más estáticas que dan por hecho la RAM recién puesta (`VTile_allocSpare`, la caché de tilesets subidos, `shownCoins` del HUD...).

**Arreglo** (`src/main.c`, primera línea de `main`): `if (!hardReset) SYS_hardReset();`. `SYS_hardReset` (`$GDK/inc/sys.h:203`, `$GDK/src/sys_a.s`) vuelve a poner la pila y salta a `_start_entry`, que borra los 64 KB de RAM, copia `.data` y llama a `main(TRUE)`: RESET pasa a ser lo mismo que encender (decisión del usuario del 2026-11-21). Se prefirió a reponer a mano cada variable en `Game_init` porque eso obliga a encontrar todas las estáticas con inicializador y a mantener la lista, y un olvido sólo se ve al pulsar RESET. Coste: ninguno en el bucle (sólo se ejecuta al entrar en `main`); el logo de SGDK está desactivado (`ENABLE_LOGO 0`), así que no aparece nada nuevo; sí vuelve a salir `logo.unity` (`Logo_show`), igual que al encender.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| RESET reinicia como al encender | Sin SRAM no hay nada que conservar; decisión del usuario (2026-11-21) | Récord, saldo, armas compradas, arma equipada, munición, sonido y música vuelven a sus valores por defecto |

### Bloque 7: power-ups (2026-11-17)

`inc/mlpow.h` y `src/mlpow.c` (nuevos: bomba y rayos), `src/powerup.c` (lo del suelo), `src/projectile.c` (la nave holo), `src/beam.c` (líneas y rayos), `src/game.c` (oleadas, recogida, congelación), `src/enemy.c` y `src/snake.c` (matar todo, posición por ranura, contacto con un círculo), `inc/arena.h` (`ML_BOMB_*`, `ML_RAYS_*`, `ML_HOLO_*`, `ML_POW_FREEZE_F`, `ML_GROUND_POW_MAX`).

**Oleadas** (`mlPowWave`, `S/gameControl.cs:477-571`): la 3 pone una bomba si `Random.Range(0, 6) == 1`; la 4, unos rayos si `Random.Range(0, 7) == 1`; la 6, 16, 19, 20 y 21, una nave holo si hay menos de 10. Todo en `getRandomPos()`: entre el 10 y el 90 % de la pantalla (x 32-287, y 24-215). Nada de MonsterLite mira los agujeros: ya no mueren al caer encima, como los power-ups del GML.

**Bomba** (`Bomba.prefab`, caja 14 px; `S/playerMovement.cs:199-210`): al tocarla, `clearEnemies()` (`S/enemyController.cs:170-186`): cada araña y cada gusano vivo explota y suma `level`, sin soltar nada. También los que aún salen de su agujero: en MonsterLite no hay agujero que esperar (bloque 8). Ya no crea los dos gusanos de tipo 2 de la bomba del GML. Desde el bloque 8 no hay agujero que esperar en el port tampoco.

**Rayos** (`RayosBomba.prefab`, caja 20 px; `S/triggerSplashBomb.cs`, `S/RayoScr.cs`): al tocarlos, un rayo de la nave a cada araña y a cada cabeza (sin enemigos, ninguno). Cada rayo sigue a su objetivo mientras vive y se queda donde estaba si muere. A los 26 f, `clearEnemies()`: mata todo lo que haya entonces, también lo nacido después, y suma `level` por cada uno. `timerRays` es 30 y `RayoScr` hace `timer--; if (timer < 0)`: dispara en el Update 31 = 25,8 f. §2.7 decía 25. El temporizador corre también con el tiempo parado (es `Update`), y puede limpiar la pantalla con el juego congelado, como en MonsterLite.

**Congelación** (Q17): `Thread.Sleep(100)` al tocar la bomba o los rayos = **5 frames** sin correr nada (`hitstop` en `playUpdate`). Medido en el arnés: la X de la nave queda fija mientras las bajas pasan de 0 a 11.

**Nave holo** (`naveProxy.prefab`; vive en el pool de proyectiles como `PROJ_TRI`, con el arte de la réplica del GML):
- inactiva: gira 1,2°/f (también con el tiempo parado);
- al tocarla (esfera de 3,6 + 5,4 px = 9 px) se activa para siempre, con la voz repetea;
- activa: apunta al enemigo más cercano **a ella** (arañas y cabezas) y dispara `BulletThin` a **6,4 px/f** cada **33 f**, la primera a los 17. No gasta munición, sólo cuenta con enemigo y no dispara con el tiempo parado (`!slowMotion`);
- activa, muere al tocarla una araña o una cabeza, con una explosión; **desde el bloque 8 (decisión del usuario, 2026-11-20) también la inactiva**, ver "Bloque 8";
- una línea azul de puntos de la nave a cada holo activa;
- sobreviven a la muerte y a la partida siguiente, inactivas y activas (`clearPowerUps()` no se llama).

**Rendimiento** (`tools/measure.sh` con arma y munición forzadas; vueltas de 600, más es mejor). La referencia es una copia del código anterior al bloque 7 que había en el scratchpad (16-nov):

| Build | fina | TWAY | MOIRE | LASER | CIRCULAR |
|---|---|---|---|---|---|
| Antes del bloque 7 | 473 | 375 | 429 | 407 | — |
| Bloque 7 sin las oleadas de power-ups | 483 | 377 | 429 | 406 | 509 |
| Bloque 7 completo | 471 | 356 | 362 | 397 | 503 |
| 10 holo activas desde el principio (`PORT_STRESS_HOLO`) | 314 | 303 | — | — | — |
| 6 / 4 holo activas | 378 / 356 | 313 / 331 | — | — | — |
| 10 holo inactivas | 403 | — | — | — | — |

- **El código nuevo no cuesta nada** (segunda fila contra la primera). La primera versión sí: con los pools agrandados y todo el pool de puntos escondido en cada frame, 427/347. Ahora sólo se esconden los puntos que se enseñaron el frame anterior, y las búsquedas de holos se saltan cuando no hay ninguna (`holoN`).
- **Lo que baja en el build completo es la escena**: las oleadas dejan bombas, rayos y hasta 10 holos inactivas en el campo (el arnés no se mueve y no las recoge). Las holos inactivas costaban 336 y ahora 403. Su giro sólo recalcula el dibujo uno de cada 8 frames (cambia cada ~37), y el toque descarta por el cuadrado antes de multiplicar. Que el rayo de MOIRE las vea no es lo que cuesta: sin eso da 366.
- **10 holo activas** cuestan como un arma de las caras: sus balas (sin disparar, 347) y sus líneas (sin líneas, 326), más las bajas y explosiones que provocan. El contacto se mira uno de cada 2 frames y el apuntado uno de cada 4, además del frame en que dispara. Bajar el tope a 6 o a 4 no mejora de forma fiable (la escena cambia más que el coste). **Se queda el tope de MonsterLite, 10 (Q3)**: el caso es el peor posible (arena llena, 10 holos activas) y se nota como ralentización, no como fallo.

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Bomba y rayos: como mucho 6 entre los dos en el suelo (`ML_GROUND_POW_MAX`, invención) | No caducan y comparten pool con los paquetes; MonsterLite no los limita | Con 6 esperando, las oleadas 3 y 4 no crean más |
| Rayos: 3 puntos por rayo (vértices 2, 4 y 6 de 9), color al azar por rayo y frame entre azul, plata y amarillo | El VDP no dibuja líneas; con hasta 11 rayos, 7 puntos serían 77 sprites de 80 | Rayos de puntos con ruido de ±8 px |
| Línea a las holo: 3 puntos (antes 6 a cada réplica) | 10 líneas × 6 = 60 sprites | Línea más espaciada |
| La congelación va después del estallido, no antes | En MonsterLite el `Sleep` va primero y el estallido sale al acabar; aquí se congela con las explosiones ya puestas | Ninguna apreciable (100 ms) |
| Los rayos en curso se pierden si la nave muere | En MonsterLite seguirían y sumarían bajas a una partida ya acabada | El game over no cambia después de morir |
| Sin explosión azul de la holo ni salpicadura de la bomba (`bombaSplash`): explosión normal | El arte es del GML (decisión 3) y no hay equivalente | — |
| Sacudidas y voces ("BOOM", "EXPLOSIONS", "RAYS"): las del GML de la bomba y el rayo, y sus sonidos | Son del bloque 12 (Q20) | Hasta entonces la bomba tiembla como la del GML |
| La holo apunta a su enemigo cada 4 frames, y siempre al disparar; mira el contacto cada 2 | Coste con 10 activas (ver arriba) | Su dibujo puede ir hasta 3 frames tarde |
| Si un enemigo muere y otro nace en su misma ranura mientras el rayo vive, el rayo pasa a seguir al nuevo | El port no guarda la identidad del objetivo | Sólo cambia hacia dónde se dibuja el rayo: al acabar mata a todos igual |
| Los enemigos no van a por las holo sin jugador | Es IA de enemigos, bloque 8 | — **Resuelto en el bloque 8** (`enemiesNoPlayer` en `src/game.c`). |

Se retiran del juego el rayo (`oRay`) y la réplica (`oTri`) del GML y su cadencia, que sustituyen los de MonsterLite.


### Bloque 8: enemigos (2026-11-20)

`src/entity.c` e `inc/entity.h` (la IA de `enemyMov`), `src/enemy.c` e `inc/enemy.h` (arañas), `src/snake.c` e `inc/snake.h` (gusanos: IA, migas y esferas), `inc/collision.h` (`Coll_circleCircle`), `src/projectile.c` e `inc/projectile.h` (balas y bolas contra esferas, `Proj_anyHolo`), `src/game.c` (destino sin jugador, temporizador con el tiempo parado), `src/hole.c` e `inc/hole.h` (fuera `Hole_wanderTarget` y `Hole_touching`, que sólo usaba la IA del GML), `src/bench.c` e `inc/arena.h` (`ML_TURN_K_*`, `ML_AI_TIMER_*`, radios y `ML_SNAKE_CRUMB_*`; fuera las constantes de la IA del GML y sus máscaras de colisión).

Decisiones del usuario que aplican: Q2 (los dos gusanos del GML con su longitud, con velocidad, IA, hp y eslabones de MonsterLite), Q4 (tope de 8 arañas) y la reversión de agujeros del 2026-11-16 (el anillo del port, con los enemigos naciendo en un agujero al azar).

**Lo que hace ahora el port:**

| Qué | MonsterLite (`S/enemyMov.cs`, `S/headScr.cs`) | Port | Dónde |
|---|---|---|---|
| Al nacer | `Start`: rotación `Random.Range(0, 360)`, `randomPos = getRandomPos()`, `setSpeed()`, estado `Random.Range(0, 2)`; visible y en marcha desde el primer tick | igual: rumbo en 1/64 de grado (0-23039), `randomPos` entre el 10 y el 90 % de la pantalla, velocidad por nivel (bloque 1), giro y estado al azar. Sin la espera de 75 f invisible e inmóvil del GML (`alarm[0]`) | `Entity_spawnML` |
| Estado | ATTACK o WANDER | igual (`ENT_ATTACK`, `ENT_WANDER`); la máquina de 3 estados del GML (quieto 5 f, ataca 250, vaga 500) desaparece | `inc/entity.h` |
| Temporizador | `timer = 200` [prefab], `timer -= 1` en `Update` (60 Hz); al pasar de 0, `resetMovement()` (estado y `randomPos` nuevos) y `timer = 30`. En WANDER `move()` (FixedUpdate) resta además `level % 100` por tick | en quintos: 1000, −6 por frame (1,2 Update), reinicio a 150; WANDER −5 × (`level % 100`) por frame. **Primer re-sorteo a los 167 f** (MonsterLite 167,5), luego **cada 26 f** (25,8). En WANDER: 14 f en el nivel 1 (150 / 11) y 3 f en el nivel 9 (150 / 51) | `Entity_tickML`, `Entity_moveML` |
| Destino | ATTACK: el jugador; si no hay, `FindGameObjectWithTag("NaveProxy")`; si tampoco, `target` no cambia. WANDER: `randomPos` | igual. La holo es la de la ranura más baja del pool, activa o no (en Unity, una cualquiera de las que llevan el tag, que son todas). Se busca una vez por frame para todos (`enemiesNoPlayer`) | `src/game.c` |
| Avance | `Translate(right · speed · 0,02)` con el rumbo de antes de girar | igual: `Entity_advance` antes del giro, con el ángulo calculado desde la posición de antes de avanzar | `Entity_moveML` |
| Giro | `Slerp(rotación, haciaElDestino, turningSpeed · 0,02)`: una fracción del ángulo que falta; `turningSpeed = Random.Range(1, turningSpeedMax / 1.4)`, máx. 8 araña y 10 gusano [prefab] | fracción `turnK / 4096` por frame, sorteada entera al nacer en [82, 468] (araña) o [82, 585] (gusano), por el camino corto. Un `muls.w` por enemigo y frame | `Entity_moveML` |
| Con el tiempo parado | `FixedUpdate` no corre (no se mueven) pero `Update` sí: el temporizador sigue bajando y re-sorteando | igual: `stoppedUpdate` llama a `Enemy_tickFrozen` y `Snake_tickFrozen`. Con la congelación de la bomba y los rayos (`Thread.Sleep`) no corre nada, tampoco esto | `src/game.c` |
| Wrap | no | no (ya era así) | — |
| Colisión | `SphereCollider`: araña 0.28 × 1.5 = **8,4 px**; cabeza y eslabón 0.32 × 1.5 = **9,6 px** | círculos con distancias enteras al cuadrado (`ML_R2_LT`): nave contra araña d² ≤ 143 (12 px), contra gusano d² ≤ 174 (13,2 px); holo contra araña 190 (13,8 px) y cabeza 224 (15 px); bola CIRCULAR (4 px) contra araña 153 (12,4 px) y cabeza 184 (13,6 px); bala de 16×16 contra la esfera, el punto de la caja más cercano (`Coll_circleBox`); rayo, los mismos radios | `inc/arena.h`, `inc/collision.h` |
| hp del gusano | 4 [prefab culobicho] | 4 en los dos tipos (ya desde el bloque 3; el tipo 2 del GML tenía 2) | `ML_SNAKE_HP` |
| Cuerpo | 16 eslabones; migas: cuando la cabeza se aleja `segmentSpacing` = 0,4 u (**8 px**) [prefab] de la última, se pone otra en la cabeza y cada eslabón salta a la suya (Lerp con t = 0, sin suavizado). Corre en `Update` | **12 y 10 eslabones** (Q2) con las migas de MonsterLite: la distancia se mide en 1/64 de píxel contra la miga exacta; el eslabón n está en la miga n + 1, así que poner una miga es correr la lista de eslabones un puesto. El historial de 128 posiciones por gusano del GML (`stepdelay`) desaparece | `crumbStep` (`src/snake.c`) |

**Comprobado por RAM** (`p/d` sobre `enemies` y `snakes` de `out/release/symbol.txt`; scripts del scratchpad, no del repo):

- **Nivel 1** (arnés de estrés, araña 0, en WANDER): velocidad 40/64 = 0,625 px/f (MonsterLite 0,472-0,72); paso medido entre frames 0,60-0,62 px (el rumbo va a grados enteros en la tabla de seno). El rumbo pasa de 63,33° a 60,97° en el primer frame: le faltaban 21,7° hacia su destino (245, 109) y gira 444/4096 = 10,8 % de eso; converge a 37,6°, el ángulo al destino. El temporizador baja 11 quintos por frame (6 + 5 × 1).
- **Nivel 9** (copia con `g_level` fijo a 9, sólo en el scratchpad): las 8 arañas a 0,844 / 0,891 / 1,453 / 2,141 / 2,891 / 3,672 / 3,719 / 3,797 px/f (MonsterLite 0,79-3,92); fracciones de giro 109-444 (rango 82-468); en WANDER el temporizador baja 51 por frame (6 + 5 × 9).
- **Re-sorteos** (copia en la que la nave no dispara, para que no mueran): el primero a los 181 frames emulados, que son los 167 del juego más los que pierde el arnés de estrés; después cada 26 frames en ATTACK y cada 14 en WANDER, con estados al azar.
- **Tiempo parado** (ROM normal, soltando B con un gusano en pantalla): la cabeza se queda en (204,06, 122,14) 40 frames seguidos mientras su temporizador baja de 736 a 508, 6 por frame; al volver a pulsar B sigue.
- **Capturas**: arnés de estrés a +60, +120 y +300 frames de START+A: el gusano de 12 eslabones a 8 px entre ellos y las arañas repartidas por la pantalla, sin esperar en los agujeros.

**Rendimiento** (`tools/measure.sh` y la misma medida con el arma y la munición forzadas o con 10 holo activas, como en el bloque 7; vueltas de 600, más es mejor; la referencia es una copia del árbol antes del bloque, en el scratchpad):

| Build | fina | TWAY | MOIRE | LASER | CIRCULAR | 10 holo activas |
|---|---|---|---|---|---|---|
| Antes del bloque 8 | 482 (21 bajas) | 356 (23) | 362 (23) | 397 (30) | 503 (26) | 321 (17) |
| Bloque 8 | **501 (32 bajas)** | 435 (34) | 411 (29) | 411 (38) | 505 (47) | 334 (26) |

`tools/measure.sh` da lo mismo en dos pasadas (470/32 con la primera versión, 501/32 con la final). Las partidas no son la misma escena (cambian la IA, la aparición y los radios), así que las bajas no se comparan entre las dos filas, y por eso tampoco la firma de +300/+400.

- **La primera versión daba 470/600** (frente a 482). Perfilado por etapas (`PORT_PROF=2`, líneas de VDP por frame): el código del gusano bajaba de 48,7 a 37,5 (la caché de partes, de 13,7 a 4,0, sin el historial de 128 posiciones), pero la escena costaba más: con los enemigos visibles desde que nacen hay más que dibujar (montaje de sprites de 60,7 a 71,8) y con las esferas más grandes que las cajas del GML hay más bajas (efectos de 12,0 a 18,9).
- **Arreglo**: los eslabones sólo se mueven al poner una miga (cada 2 a 11 frames según la velocidad), así que sus sprites sólo se recolocan entonces o cuando cambia la cámara de la sacudida (`partsDirty`, `drawBX/drawBY`). El gusano queda en 29,9 líneas por frame (de 48,7 antes del bloque) y el caso peor en **501/600**.
- **RAM**: el historial del GML eran 2 × 128 × 2 bytes por gusano; las migas, 4 bytes más. Unos 1,5 KB menos.
- No hay divisiones nuevas en el bucle: `level % 100` sólo se calcula cuando cambia el nivel (`wanderStep`).

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| **Los gusanos siguen siendo dos, de 12 y 10 eslabones**, en vez de uno de 16 | Q2 (decisión del usuario) | Gusanos más cortos que en MonsterLite: 96 y 80 px de cuerpo frente a 128 |
| **Los eslabones siguen las migas de 8 px de MonsterLite, no el `stepdelay` del GML** | **Decisión del usuario (2026-11-20, `docs/MONSTERLITE_DECISIONES.md`)**. Con la velocidad de MonsterLite (hasta 3,92 px/f) el `stepdelay` de 6 f separaba los eslabones hasta ~24 px: el gusano de 12 llegaba a medir ~280 px, casi la pantalla. Con las migas mide lo que medía en el GML a su velocidad (7-11 px por eslabón, 84-131 px) | Los eslabones avanzan a saltos de 8 px (como en MonsterLite, que no suaviza) en vez de deslizarse; separación constante a cualquier velocidad |
| Las migas se miran una vez por frame de 50 Hz, no en `Update` (60 Hz) | No hay `Update` aparte | Separación de 8 a 11,9 px (MonsterLite: 8 a 11,3) |
| El giro deja de avanzar cuando faltan menos de 2048 / `turnK` sesentaicuatroavos de grado (0,39° con el giro más lento) | Redondeo de la fracción en Q12; `Slerp` en flotante converge sin fin | Ninguna visible: el rumbo se usa a grados enteros |
| No se reproduce que `Quaternion.Euler(pos)` reciba la posición X e Y del enemigo como ángulos de inclinación (`S/enemyMov.cs:107`: `pos = (x, y, ángulo)`) | Es un efecto colateral: inclina el sprite unos grados en X e Y (hasta 8° y 6°, la posición en unidades) y `lockZ` sólo fija la posición | En MonsterLite el enemigo avanza hasta un 1 % menos cerca de los bordes y se ve ligeramente escorzado; aquí no |
| No se porta `if (position == target) resetMovement()` (`S/enemyMov.cs:63`) | Comparación exacta de flotantes: en la práctica no salta nunca | — |
| `resetMovement` pone `target` en el jugador; aquí no | No se ve: en ATTACK `move()` lo vuelve a poner en el jugador en el tick siguiente, y en WANDER lo pisa `randomPos` | — |
| `target` sin inicializar es (0, 0) del mundo: el centro de la pantalla | Cámara en el origen (`S/cameraScript.cs`) | Sólo cuenta si nace en ATTACK sin jugador ni holo |
| Las armas del GML (PIVOT, PARA, metralla, explosión de la bomba del GML) golpean ahora las esferas de MonsterLite con sus cajas, en vez de caja contra caja | Un enemigo tiene una sola forma de colisión | Contacto algo más generoso que con las cajas del GML (12×12 frente a un círculo de 16,8 px de diámetro) |
| Los eslabones siguen parando las armas del GML (escudo) y siguen sin parar las balas de MonsterLite | Como en el bloque 3 | — |
| La bola CIRCULAR choca como esfera de 4 px (antes, su caja de 8×8) | La esfera de `circularBullet` | Contacto en diagonal algo más corto que con la caja |
| El arnés de estrés es más caro de jugar | Con los enemigos visibles desde el principio y radios mayores hay más bajas y más que dibujar | Ver la tabla: la medida mejora igual por el ahorro del gusano |

**Migas frente a `stepdelay`: resuelto.** La decisión 4 de `port-arena.md` decía "longitud fiel (12 y 10 partes según tipo, `stepdelay` del GML)" y Q2 no nombraba el seguimiento. El usuario decidió el 2026-11-20 mantener las migas de 8 px (`docs/MONSTERLITE_DECISIONES.md`).

#### Dos arreglos tras la verificación (2026-11-20)

**1. Sprites por línea al nacer el gusano.** Los eslabones nacen todos en la cabeza (`headScr.Start`) y se separan de uno en uno, una miga cada 8 px: el último tarda unos 96 px de recorrido de la cabeza. Mientras tanto la pila eran 12 sprites en las mismas líneas, y con dos gusanos naciendo a la misma altura el arnés de estrés llegaba a **31 sprites y 496 px en la línea 145** a +4 frames de START+A (el VDP pinta 20 por línea): se perdían los paquetes del suelo, entre ellos los de (90,130) y (210,130). Lo encontró el verificador.

Arreglo (`spread` en `src/snake.c`): cuenta las migas puestas desde que nació el gusano. Tras k migas, los eslabones 0 .. k−2 ya están en su sitio y del k−1 en adelante siguen apilados en el punto de nacimiento; sólo se coloca el primero de la pila. Los demás siguen aparcados desde `hideSprites` al nacer, así que no hay que esconderlos ni comparar posiciones: el bucle del cuerpo sólo llega hasta `spread`, y oCu1 y oCu2 no se colocan hasta que les toca. En pantalla es lo mismo: los apilados están en el mismo píxel. Las colisiones no cambian (siguen usando las posiciones).

Medido con `vs` en el arnés, en el mismo momento: **la peor línea pasa de 31 sprites (496 px) a 9 (144 px)** y ninguna supera los 20; se ven los 8 paquetes. A +30 frames, 6 eslabones de cuerpo ya separados; a +80, 20 (de 25); a +120 la captura enseña el gusano entero.

**2. La holo era inmortal fuera de la partida.** En el game over y en la tienda los enemigos en ATTACK van a por la holo (bien: `enemiesNoPlayer`), pero ahí no corre `Proj_update`, que es donde se mira si la tocan, así que no moría y pasaba a la partida siguiente (`Proj_clearKeepTris`). En MonsterLite `collideAndDie` la destruiría al primer contacto, porque la física no se para en el game over. Lo vio el verificador: 800 frames con 5-8 enemigos encima y la holo viva.

Arreglo: `Proj_holoContact()` (`src/projectile.c`) hace sólo la prueba de contacto de `updateHolo` (todas las holos, ver el punto 3; mismos radios: 13,8 px con la araña y 15 px con la cabeza) y la explosión, sin tocar el resto de `Proj_update`. La llama `enemiesNoPlayer` cuando hay holo, así que corre en la rama fuera de `GAME_PLAY` y también en los 40 f del arranque, que tampoco corren `Proj_update`. Cada frame en vez de uno de cada dos: fuera de la partida no hay más carga.

Comprobado con `PORT_STRESS_MORTAL` (una holo activa, la nave puede morir), leyendo `holoN` y `g_state` cada 40 frames: la nave muere hacia +120 (estado `GAME_MENU`) y la holo desaparece entre +200 y +240, en el game over. Control con la misma ROM sin la llamada: la holo sigue viva a +1200.

**3. La holo inactiva también muere al contacto** (decisión del usuario, 2026-11-20, `docs/MONSTERLITE_DECISIONES.md`). `S/collideAndDie.cs` es sólo `OnTriggerEnter`: si lo que entra lleva el tag `snake` (la cabeza) o `arana`, explosión azul y `Destroy`. El componente empieza desactivado y lo habilita `setWorking` (`S/naveProxyController.cs:51`), y por eso el bloque 7 hacía morir sólo a las activas. Pero Unity manda los eventos de trigger también a los MonoBehaviour desactivados, y la holo inactiva tiene el mismo `SphereCollider` de 5,4 px que la activa (es con el que la nave la toca para activarla, `S/playerMovement.cs:224`). Así que en MonsterLite la inactiva también muere. En el port, además, `Proj_anyHolo` puede elegir una inactiva como destino de ATTACK sin jugador, y en el game over los enemigos la rodeaban para siempre. Lo vio el verificador.

Arreglo: `updateHolo` mira el contacto (uno de cada 2 frames, como antes) antes de separar activa e inactiva, y `Proj_holoContact` ya no filtra por `released`. Mismos radios y misma explosión. No cambia nada más del bloque 7: la nave sigue activándolas al tocarlas (`Proj_holoTouch`), una holo muerta libera su sitio en el tope de 10 (`hide` baja `holoN`), y `Proj_clearKeepTris` sigue conservando las que quedan vivas. Con el tiempo parado no se mira (la física de Unity está parada; `Proj_tickFrozen` sólo gira la inactiva).

Comprobado en una copia del arnés con tres holos inactivas, en (218,85) (encima de un agujero), (160,148) y (190,120) (cerca de la nave), y sin las holos de las oleadas, leyendo `holoN`: con el cambio quedan 3 en el primer frame, 1 a +40 y ninguna a +80. Control con la misma copia y el comportamiento anterior (sólo mueren las activas): las 3 siguen vivas a +600.

**Rendimiento de los arreglos.** `tools/measure.sh`: **497/600 y 32 bajas** (antes de los arreglos, 501/32; dos pasadas iguales). La holo fuera de la partida no corre en el arnés; quitándola da 498. Sin aparcar la pila, 500. Pero el perfil por etapas (`PORT_PROF=2`) dice que con la pila aparcada el frame es **más barato**, no más caro: el gusano de 30,3 a 27,2 líneas por frame y el montaje de sprites de 72,6 a 71,5. Con la misma escena (las mismas 32 bajas), la diferencia de 3 vueltas de 600 (0,5 %) es de fase: el arnés va al borde del frame y cuánto trabajo cae a cada lado del vblank cambia con poco. Una variante que sólo mueve la disposición de `snake.c` da 497 igual.

Con el punto 3: **508/600 y 37 bajas** (dos pasadas iguales). Es otra escena: las holos inactivas que dejan las oleadas en el arnés (la nave no se mueve y no las recoge) mueren ahora cuando pasa un enemigo, y hay menos que mover y dibujar. Con 10 holos activas (`PORT_STRESS_HOLO`), donde la escena no cambia: **333/26**, frente a 334/26.



### Bloque 9: HUD, mensajes de partida y fondo por nivel (2026-11-20)

`src/hud.c` e `inc/hud.h` (el HUD y los mensajes, reescritos), `src/hole.c` e `inc/hole.h` (texto encima de los agujeros: varias filas a la vez, composición en RAM, caché y filas ocultas; `greenTile` en paralelo y las celdas de cada fila precalculadas), `src/game.c` (disparadores de los mensajes, `HUD_tick`, el nivel también con el tiempo parado), `src/main.c` (`Hole_textUpdate` una vez por frame), `src/menu.c` (`HUD_clear` al entrar, `Hole_forgetText` al salir), `src/vramspr.c` e `inc/vramspr.h` (`VTile_read`), `inc/arena.h` (`ML_WAVE_ALERT`, `ML_ALERT_F`, `ML_LEVEL_MSG_F`, `ML_COINS_BUMP_F`).

Decisiones del usuario que aplican: **Q16** (se queda la imagen de ruinas, **sin color por nivel**, y los textos nuevos van con la fuente del sistema), Q8 y los botones A y C del 2026-11-16 (el aviso es "HOLD A/B/C").

**El HUD** (`S/HUD.cs:284-398`, en apaisado; estilos leídos del prefab `controller` y de las modificaciones de la escena con UnityPy):

| Qué | MonsterLite | Port | Dónde |
|---|---|---|---|
| BULLETS | arriba a la izquierda, `BULLETS<n>` (la cifra pegada a la palabra), `bulletsSt` a W/18 (17,8 px), la palabra gris (0,784) y la cifra blanca | glifos grandes de sFont2 del GML (16 px), filas 0-1, la cifra pegada a la palabra (hasta 9999; ver la línea de arriba) | `drawTopLine` |
| Arma | debajo, el icono a W/20 (16 px) y el nombre (`currentWeapon.ToString()`) a 1,4 iconos del borde, con `bestkillsSt` (W/70, 4,6 px) | el icono del menú (32×32) reducido a 16×16 al empezar la partida (de cada 2×2 píxeles, el más claro: el marco y la raya de un píxel del láser no se pierden), filas 2-3; el nombre en la fila 2 desde la columna 3 (24 px; MonsterLite empieza en 23,7). PIVOT y PARA con el nombre que ya tienen en la tienda | `buildIcon`, `drawStatic` |
| SCORE | arriba a la derecha, `<n>SCORE `, `killsSt` (W/18, el mismo tamaño que BULLETS), alineado a la derecha | **cifra y palabra con los glifos grandes**, la palabra en los glifos 15-19 y la cifra justo delante (hasta 999 999). La hoja del GML no tiene C ni R: se dibujan con su trazo (ver abajo) | `drawTopLine` |
| Línea de arriba | `BULLETS<n>` y `<n>SCORE` en la misma línea, en los extremos | `BULLETS<n> <n>SCORE` en 20 glifos de 16 px: 12 de palabras y 8 para las cifras, con un glifo de hueco siempre. Si las dos cifras no caben con el hueco (balas de 2 cifras con 6 de puntuación, de 3 con 5, de 4 con 4), manda la puntuación y las balas salen con las cifras que quepan, a 9: **pregunta Q23** | `drawTopLine` |
| BEST | `<best>BEST `, la cifra en amarillo; es `maxKills1`, que sólo se actualiza al morir (`setScores`, `S/gameControl.cs:720, 797`) | fila 2, alineado a la derecha, cifras amarillas; `g_bestAtStart` (el récord de antes de la partida), que no cambia en la partida. Antes, el LAST provisional subía en directo | `drawStatic` |
| LEVEL | `LEVEL <n>` | fila 3, a la derecha | `drawLevel` |
| UPGRADE AT | `UPGRADE AT: <factorLevel·(level+1)>`, con el factor ya duplicado | fila 4, a la derecha; con 5 cifras o más llega al agujero de arriba (columnas 18-23) y va compuesta encima, sin cortarlo | `drawLevel` |
| COINS | `COINS <saldo>` (saldo total), cifra en amarillo, `coinsSt` centrado en W/2 + W/36 y a H − 8 − W/36 | centrado en 169 px, en la fila 28 en PAL y 26 en NTSC (la misma fórmula con el alto que se ve: 228,8 y 213,5 px) | `drawCoins`, `coinsRow` |
| Salto de COINS | al recoger, 10 llamadas de `OnGUI` con el rectángulo más alto y más abajo: el texto se va casi entero por debajo de la pantalla | la línea se borra 4 f y vuelve con el saldo nuevo; otra moneda en esos 4 f no lo alarga (como `t`) | `HUD_tick`, `coinsHide` |
| Deslizamiento | `run()`/`runOut()` en cada `Update` (S/HUD.cs:156-206): `MoveTowards` lineal del grupo hacia (0, 0) a 1000 px/s del aparato en partida y hacia (0, −H) a 300 fuera; empieza arriba. Va con `Time.deltaTime`: con el tiempo parado no se mueve. Dentro del grupo (`BeginGroup`, 213-366) van BULLETS, arma, SCORE, BEST, LEVEL, UPGRADE AT y el mensaje de nivel | **Decisión del usuario (2026-11-20)**. Con los 768 px de alto por defecto del proyecto: 6,25 px/f al entrar (38,4 f) y 1,875 px/f al salir (128 f), sólo con el tiempo corriendo. Las filas 0-4 se reescriben una fila más abajo (o arriba) cada vez que la posición cruza una fila de 8 px: entran en los frames 34-39 de la partida y salen en 18 f del game over (ver "Comprobado"). COINS y los mensajes, fuera del grupo, como en MonsterLite | `drawGroup`, `HUD_tick`, `HUD_menuTick` |
| Sombra | los textos se pintan dos veces, en negro desplazados y en color encima: BULLETS y SCORE a +8 px de 768 (2,5 px), el mensaje de nivel y el aviso a +4 (1,25 px), ARENA! a (+4, +4); `offsetLetterShadow` = `SCREENW / SCREENW + shadowValue` = 4 (`setSizes`, `shadowValue` 3 [prefab controller]) | **Decisión del usuario (2026-11-20): todas las fuentes llevan sombra, pintada en los tiles.** Glifos grandes: 2 px hacia abajo, en negro (índice 1 de PAL2), dentro de la celda de 16 px (la letra llega a la fila 13). Fuente del sistema: 1 px hacia abajo, en negro (índice 12 de PAL0), en los 96 glifos y en su sitio (las letras ocupan las filas 1-6): la llevan también el menú, el game over, la tienda y la marquesina. Letras sobre agujeros: en su tile compuesto. Los dígitos que vuelan al HUD ya traían sombra en el arte del GML | `HUD_initGraphics`, `shadowFont` |

**Los mensajes:**

| Mensaje | MonsterLite | Port |
|---|---|---|
| "ARENA!" | mientras `HUD.counter < 60` (una llamada de `OnGUI` por evento, dos por frame: 30 frames de 60 Hz), `readySt` centrado en y = H − H/1.2 = 40 px más medio alto de línea | 25 f desde el frame en que empieza la partida, fila 5 (40 px), centrado. Es el mismo contador que la gracia de la congelación (Q8, `ML_FREEZE_GRACE_F`), que ahora cuenta también en el arnés para que el cartel se quite |
| "LEVEL n / NEXT UPGRADE: SCORE x!" | `showNewLevel` lo pone `levelUpgrade()` y lo quita la **primera** corrutina `ChangeLevelWait` que acaba (3 s con `Time.deltaTime` + `WaitForSeconds(0.1)`, tiempo escalado: 155 f); los números se leen al dibujar | 155 f de tiempo corriendo (con el tiempo parado no cuentan, con el hitstop tampoco); se guardan las cuentas de hasta 8 subidas y la primera que acaba lo quita; si sube otra vez con el mensaje puesto, se reescribe con los números nuevos. Filas 9-10 (72-87 px; MonsterLite, ~74-92 centrado en ~83), centradas. Incluye el "!" final del original |
| " !!!" | la oleada 27 pone `OLEADA = "!!!"`; `oleadaT` empieza en 30, baja 1 por `Update` también con el tiempo parado y lo borra al pasar de 0 (31 frames de 60 Hz); si la oleada vuelve a salir con el texto puesto, no se alarga | **26 f** (25,8), contados también con el tiempo parado y no en el hitstop; no se alarga. En rojo (`<color=red>`), centrado, en la **fila 12** (96-103 px; decisión del usuario, Q22): ver divergencias |
| Aviso de tiempo parado | "KEEP YOUR FINGER\n ON SCREEN!" y la textura `dedo` mientras `slowMotion && counter >= 60`, y = H − H/2.9 = 157 px | "HOLD A/B/C" (Q8), fila 20 (160 px), centrado, compuesto encima del agujero de abajo. Definitivo: misma posición y texto que en el bloque 2, con la fuente del sistema (Q16) |

**Fondo por nivel: no se porta.** `setCameraLevelColor` (`S/globales.cs:293-303`) desvía el color plano de la cámara al subir de nivel. Por **Q16** el port mantiene la imagen de ruinas sin color por nivel: no hay color plano que desviar. Decisión del usuario, divergencia deliberada.

**El texto encima de los agujeros.** El anillo de agujeros pasa por las filas 3-25, y UPGRADE AT, "ARENA!", el mensaje de nivel y HOLD caen encima. Una letra de la fuente del sistema ocupa la celda entera del plano A, y donde no tiene píxel se ve el plano B: el agujero sale cortado en rectángulos. Desde el bloque 2 la letra se compone en un tile propio (agujero debajo, letra encima), con 10 tiles para un solo texto a la vez. Ahora puede haber hasta cuatro filas a la vez (UPGRADE AT, las dos del nivel y HOLD) y hasta 28 letras sobre agujero (medido con todo forzado a la vez: nivel 20, número de 7 cifras y HOLD). Se cambia en `src/hole.c`:

- Una tabla de hasta 6 filas apuntadas, cada una con su texto de 40 columnas; reserva de **32 tiles** con un tile por columna mientras la letra siga encima de un agujero. Si un agujero cambia de variante (el reborde rojo de cuando sale un gusano), sus celdas con letra no se pisan: la fila se recompone.
- **Coste, medido.** La primera versión leía la letra y el agujero de la VRAM y subía el tile por la CPU: **~5000 ciclos por letra** (el puerto del VDP con la pantalla dibujándose va a ~50 ciclos por palabra leída y ~120 por palabra escrita; `SYS_disableInts` + `SYS_enableInts`, ~750). Una subida de nivel, con 21 letras sobre agujeros, se comía **2,5 frames**; el cambio de reborde de tres agujeros, 2,8. Medido con `getSubTick` en copias del scratchpad. Ahora: siluetas de la fuente y agujeros ya verdes en RAM, composición de 8 AND/OR por fila; caché por columna (no se recompone si no cambian la letra ni el agujero de debajo); las filas que se ocultan guardan sus tiles por si el texto vuelve igual (`Hole_hideTextRow`; si la reserva se llena, se desaloja una oculta: medido, 0 letras sin componer con todo forzado y 1 desalojo); todo se compone en un frame (`Hole_textUpdate`, sólo si hay algo pendiente) y los tiles y el tramo de fila que cambia van por la cola de DMA al mismo vblank, así que nunca se ve una fila apuntando a tiles a medias.
- Repartir la composición en varios frames (3 o 6 letras por frame) se probó y salía peor en el arnés: 500 y 501-502 vueltas, frente a 504 de una vez en ese momento. Con la arena llena cada frame va al límite, y lo que cuenta es en cuántos frames hay trabajo de más.
- `Hole_rowTiles` recorría los 6 agujeros celda a celda, ~5000 ciclos por fila (medido con 100 llamadas); ahora usa las celdas de cada fila precalculadas al arrancar (~3400 con el borrado de la fila).
- `greenTile` (los índices 1-3 de los agujeros a los verdes) iba píxel a píxel, ~2800 ciclos por tile; ahora es en paralelo, los 8 píxeles de una fila a la vez.

**Lo que se escribe cada frame.** Cada línea del HUD se escribe sólo cuando cambia su valor, en un campo de ancho fijo, y por la cola de DMA (`DMA_QUEUE_COPY`): nadie más escribe en esas celdas, así que no hay orden que respetar con lo que va por la CPU. Por el camino se quitaron tres costes que venían de antes o de la primera versión:

- `HUD_drawBig` escribía tile a tile con `VDP_setTileMapXY` (~1000 ciclos por tile): 16 000 ciclos por cada cambio de BULLETS, o sea en cada disparo, desde la FASE 6. Ahora son dos filas por la cola.
- Las cifras salían con divisiones de 32 bits (una llamada a libgcc por cifra y operación); ahora, restando potencias de 10.
- `buildIcon` píxel a píxel costaba ~440 000 ciclos (3 frames al empezar la partida); fila a fila con desplazamientos fijos, menos de uno. Y sólo cuando cambia el arma.
- `HUD_init` ya no borra las 11 filas del HUD al empezar (~90 000 ciclos): el HUD ya está fuera desde que se entró en el menú.

**VRAM.** Primera entrega: de **1433 a 1434 de 1440** tiles (`nextTile` leído por RAM con la ROM normal y con el HEAD, a los 400 frames): +22 de la reserva de texto (10 → 32), +11 de las cifras amarillas y la admiración roja (copias de la fuente del sistema con los índices 14 y 13 de PAL0, que no usaba nadie), +4 del icono; −36 de los 9 glifos grandes que ya no se usan (la hoja trae `BULETSKIA0123456789OPNXre` de cuando el HUD y el menú del GML escribían KILLS, TOP y NEXT en grande; se suben sólo los 16 de BULLETS y las cifras). Quedan 6. El fondo no cambia (`VTILE_SHARED_TILES` 528).

Con el deslizamiento, la sombra y SCORE en grande: **1432 de 1440** (`nextTile` por RAM). +12 de C, O y R en la fuente grande (la O sale de la hoja del GML, que la traía para TOP); −14 de la variante roja de los agujeros, que tenía 14 celdas idénticas a la normal (el relleno de dentro): ahora usan el tile de la normal (`holeRedTile`), sin cambiar nada de lo que se ve (comprobado con el reborde rojo forzado). La sombra no gasta tiles: va pintada en los que ya había. Quedan 8.

**RAM.** `.bss` de 0xFF3728 a 0xFF5C04 (+9,4 KB): agujeros verdes 3 KB, siluetas de la fuente 2 KB, tiles compuestos 1 KB, filas de texto ~2,5 KB, celdas por fila ~0,9 KB. Libre en el montón de SGDK tras arrancar: 29 938 bytes (`MEM_getFree` en una copia).

**Rendimiento** (`tools/measure.sh`, vueltas de 600; las copias intermedias, en el scratchpad; la final, en dos pasadas iguales):

| Build | Vueltas | Bajas |
|---|---|---|
| HEAD (bloque 8) | 508 | 37 |
| Sin actualizar el HUD ni componer texto (cota) | 516 | 37 |
| Primera versión (composición por la VRAM, 6 letras por frame) | 503 | 37 |
| HUD nuevo sin componer texto | 507-508 | 37 |
| Composición en RAM, caché, filas por DMA | 504 | 37 |
| **Final** (`Hole_textUpdate` sale enseguida sin nada pendiente) | **509** | **37** |
| Final, con dos disposiciones distintas del código (control de fase) | 509, 509 | 37 |
| 10 naves holo (`PORT_STRESS_HOLO`): HEAD / final | 333 / 334 | 26 / 26 |
| Con deslizamiento, sombra y SCORE en grande, primera versión | 501 | 37 |
| … con la sombra de la fuente pintada al arrancar y no al empezar la partida | 505 | 37 |
| … con la línea de arriba entera (C, O, R) y sin préstamo de minúsculas | 503-504 | 37 |
| … con salida rápida de `HUD_update` (en reposo abría un marco de 264 bytes y guardaba 10 registros: 2-3 líneas de VDP por frame, medido con `GET_VCOUNTER`) | 507-508 | 37 |
| … con cifras por `divu` y borrado de filas vacías sin `Hole_rowTiles` | **509** | **37** |
| Ídem, con dos disposiciones distintas del código | 509, 509 | 37 |
| Ídem, 10 naves holo | 333 | 26 |

En el arnés el nivel sube 5 veces en 600 frames; una copia instrumentada de la versión con caché cuenta 87 letras compuestas y 17 tramos de fila escritos en esos 600 frames. La última mejora fue quitar el recorrido de las 6 filas en los frames sin nada que componer: con la arena llena, cientos de ciclos por frame bastan para que un frame más se pase del vblank.

**Comprobado:**

- **Duraciones por RAM** (copia con los mensajes forzados en el frame 60 de la secuencia de arranque, sólo en el scratchpad; `arenaWant` y `alertWant` cada frame): "ARENA!" en los frames 0-24 de la partida (25); "!!!" en los frames de juego 60-85 (26), con 3 frames de emulador perdidos en medio (el forzado lo pone todo en el mismo frame) que no cuentan. El salto de COINS: `coinsHide` 4, 3, 2, 1 y la línea vuelve en el quinto.
- **Mensaje de nivel en partida normal** (ROM normal, manteniendo B): sube a nivel 2 hacia los 160 f y a 3 hacia los 275; el mensaje se reescribe con los números nuevos ("LEVEL 3 / NEXT UPGRADE: SCORE 16!") y el game over lo quita.
- **Reborde rojo con texto encima** (copia que fuerza el rojo en los agujeros 1, 2 y 4 con el mensaje puesto): las letras siguen enteras y compuestas sobre el rojo.
- **Reserva**: con todo forzado a la vez (nivel 20, número de 7 cifras, "!!!", HOLD), 32 tiles en uso, 1 fila oculta desalojada y **0 letras sin componer**.
- **Segunda partida**: el HUD del game over quitado, "ARENA!" y el mensaje de nivel otra vez bien (con los tiles de las filas ocultas de la partida anterior).
- **NTSC** (BlastEm con `-r U`, copia de `mdrun.py` en el scratchpad): COINS en la fila 26, dentro de las 224 líneas; el resto del HUD arriba, en las filas 0-4.
- **Depurador de gráficos** (`GFXDEBUG=1`): al abrir y cerrar el panel el HUD se repinta entero, también la fila de UPGRADE AT, que primero no volvía (su caché creía tenerla en pantalla: arreglado con `Hole_textTouched`).
- **Capturas** (en el scratchpad de la sesión, `shots/`): splash `blastem_20261120_165045.png`; "ARENA!" `165101` (y `165153` en NTSC); nivel 2 `160917`; nivel 3 `165107`; nivel 20 con "!!!" y los rebordes rojos `165010`; HOLD con el mensaje de nivel `165012` y `165808` (y `165154` en NTSC); game over sin HUD `165055`; HUD tras cerrar el depurador `165650`.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| **Sin color de fondo por nivel** (`setCameraLevelColor`) | **Q16, decisión del usuario**: se queda la imagen de ruinas | El fondo no cambia al subir de nivel |
| **" !!!" centrado en la fila 12** (96-103 px) y no centrado en la esquina de abajo a la derecha, casi fuera de pantalla, como en MonsterLite | El estilo centrado sobre un rectángulo de toda la pantalla que empieza en el centro lo saca de la pantalla. **Decisión del usuario (Q22, 2026-11-20)**: centrado y un poco más arriba que y = H − H/1.9 = 114 px, para que no lo tape la nave al nacer en (160, 120) (su sprite ocupa y = 108-131). La fila 12 no pasa por agujeros en sus columnas (18-21) y deja la 11 libre entre el mensaje de nivel (filas 9-10) y "!!!" | Se ve entero, también con la nave en su sitio de aparición |
| "!!!" dura 26 f y no 25 | `oleadaT < 0`: 31 frames de `Update`, no 30 (§2.8 corregido) | — |
| Todos los textos nuevos a 8 px (fuente del sistema) | Q16. En MonsterLite: ARENA! 20 px, el mensaje de nivel 13 y 5 px, BEST/LEVEL 4,6 px, UPGRADE AT 12 px de dispositivo | Los grandes se ven más pequeños y los pequeños más grandes |
| SCORE hasta 999 999; las balas hasta 9999 y recortadas a 9 si no caben junto a la puntuación | 20 glifos de 16 px en 320; con SCORE en grande quedan 8 para las cifras y un hueco. El nivel 17 empieza en 1 179 648 | Por encima de 999 999 la cifra se queda en 999 999. Con muchas balas y mucha puntuación, la cifra de balas sale en 9 (**Q23**) |
| **C y R de la fuente grande dibujadas para el port** | sFont2 del GML no las tiene y el usuario pidió SCORE del mismo tamaño que BULLETS. Invención de arte (sólo forma): trazo de 4 px y escalones de 2 px como la K, la C con la barra de la E | — |
| El HUD se desliza a saltos de 8 px (5 escalones al entrar y 5 al salir), no píxel a píxel | El plano A lleva también los agujeros y el texto de partida: no se puede desplazar; la ventana tiene su mapa ocupado por los tiles del menú | Se ve entrar y salir, a escalones |
| Al abrir la tienda el HUD se quita de golpe | La tienda borra el plano entero | En MonsterLite seguiría subiendo detrás |
| El mensaje de nivel no se desliza con el grupo | En partida el grupo ya está en su sitio antes de que pueda subir el nivel (la nave entra a los 40 f), y fuera de partida el mensaje no se dibuja | — |
| Sombra de 1 px (fuente del sistema) y 2 px (grande) hacia abajo, también en ARENA! (que en MonsterLite va en diagonal) y en los textos que en MonsterLite no la llevaban (BEST, LEVEL, UPGRADE AT, COINS, "!!!", menús) | Decisión del usuario: todas las fuentes con sombra, pintada en los tiles. 1,25 px y 2,5 px de MonsterLite redondeados; un solo glifo por carácter | — |
| La sombra de los glifos grandes sustituye a la de 1 px en diagonal que traía sFont2 del GML | MonsterLite la pone vertical a 2,5 px | — |
| Los glifos con descendente de la fuente del sistema (`, ; g j p q y`) suben 1 px, y la barra `|` pierde su última fila | Llegaban a la fila 7 de su celda y la sombra salía cortada (lo vio el verificador en la tienda: "ways", "energy", "frecuency"). Así cabe en el mismo tile: 0 tiles | Esas letras quedan 1 px más altas que las demás |
| Sin el temblor de ±2 px del mensaje de nivel (`yU`) | Sería desplazar el plano entero | Texto quieto |
| Sin la textura `dedo` en el aviso | Q8 (no hay pantalla táctil), como en el bloque 2 | — |
| Las palabras BULLETS y SCORE no van en gris (0,784) | Son el arte blanco del GML (índice 8 de PAL2, compartido con los agujeros y el logo) | Un poco más claras |
| El salto de COINS se hace borrando la línea 4 f | En MonsterLite la línea baja casi entera por debajo de la pantalla; en el port la siguiente fila hacia abajo ya está fuera o es campo | Igual a la vista: parpadea al recoger |
| BEST es el récord de antes de la partida | Como `maxKills1` en MonsterLite. Antes del bloque, el LAST provisional subía en directo | Se queda fijo durante la partida |
| `levelUpgrade()` también con el tiempo parado | Va en `Update`, que sigue con `timeScale` 0 (antes el port sólo miraba el nivel con el tiempo corriendo) | Si los rayos suben de nivel con el tiempo parado, el mensaje sale ya y su cuenta espera |
| Se ven nombres de arma del GML (PIVOT, PARA) | MonsterLite sólo tiene 5 armas; las del GML conviven (Q9) | — |


### Bloque 10: logros (2026-11-21)

`inc/achieve.h` y `src/achieve.c` (nuevos: la lista, la máscara, los contadores y la cola de avisos), `src/game.c` (las comprobaciones), `src/menu.c` (la entrada ACHIEVEMENTS de la tienda y la pantalla de lista), `src/hud.c` e `inc/hud.h` (el aviso en partida y en el game over).

**Todo este bloque es invención.** MonsterLite no define ningún logro (`GameCenterSingleton.cs:76-96` tiene funciones genéricas que nadie llama; `docs/MONSTERLITE_GAP.md` §2.11) y el GML tampoco. La lista, las cantidades, el aviso y la pantalla los decide el usuario (**Q18**, 2026-11-15: todos los grupos, cada uno con monedas; cantidades cerradas el 2026-11-21, `docs/MONSTERLITE_DECISIONES.md`) o, donde él no dijo nada (posición y duración del aviso, cómo se entra en la lista, textos), el desarrollador; todo va en la tabla de divergencias de abajo.

**Los 17 logros y dónde se comprueba cada uno.** La propuesta de Q18 tenía 17, no 18 (la decisión del 2026-11-21 lo corrige); WBLOCK no tiene logro porque viene comprada. Bit = `AchieveId` (`inc/achieve.h`); al conseguir uno se pone su bit en `g_achMask`, su recompensa se suma a `g_coins` (el saldo, no a `g_coinsEarned` ni a las recogidas) y su aviso se encola.

| Bit | Grupo | Logro (texto) | Monedas | Se comprueba en |
|---|---|---|---|---|
| 0 | PROGRESO | FIRST GAME: terminar una partida | 50 | `Game_killPlayer` (`src/game.c:561`); el aviso sale en el game over |
| 1-3 | PROGRESO | REACH LEVEL 5 / 10 / 15 | 100 / 500 / 2000 | `Game_levelUpgrade`, sólo cuando sube (`src/game.c:379`); si salta varios niveles, salen todos |
| 4-6 | PROGRESO | SCORE 100 / 1000 / 10000 | 100 / 300 / 1000 | donde se suma puntuación: las bajas de disparo y metralla (`src/game.c:950`) y las de bomba y rayos (`addKills`, `src/game.c:365`). Una comparación de `u32` con `g_achNextScore`, sin llamada, en los frames con bajas |
| 7-10 | TIENDA | BUY LASER / TWAY / CIRCULAR / MOIRE | 600 / 1200 / 2400 / 4800 | al comprar en la tienda (`storeInput`, `src/menu.c:987`); la recompensa entra ya en el saldo |
| 11-12 | TIENDA | COLLECT 1000 / 10000 COINS | 100 / 1000 | al recoger una moneda en partida (`src/game.c:862`): `g_coinsCollected`, `u32` acumulado entre partidas, sólo con las monedas cogidas (ni recompensas ni saldo) |
| 13 | POWER-UPS | USE A BOMB | 100 | al tocar la bomba (`src/game.c:885`) |
| 14 | POWER-UPS | USE THE RAYS | 100 | al tocar los rayos (`src/game.c:904`) |
| 15 | POWER-UPS | ACTIVATE A HOLO SHIP | 100 | cuando la nave toca una holo y la activa (`Proj_holoTouch`, `src/game.c:922`) |
| 16 | POWER-UPS | 10 KILLS, ONE BOMB | 500 | bajas de esa detonación, lo que devuelve `MLPow_clearEnemies` (`Enemy_explodeAll` + `Snake_explodeAll`: arañas y cabezas, los eslabones no cuentan) (`src/game.c:889`). Los rayos no cuentan aunque acaben en `clearEnemies` |

**Vive en RAM** (sin SRAM, decisión del usuario del 2026-11-20): `g_achMask` (u32), `g_coinsCollected` (u32), `g_achNextScore` (u32), la cola (17 + 2 bytes). `Achieve_init` lo pone a cero en `Game_init`, que corre al encender y tras un RESET (`SYS_hardReset` borra la RAM). ~37 bytes de `.bss` en total con los del HUD y el menú.

**El aviso.** "ACHIEVEMENT! <nombre> +<monedas>", con la fuente del sistema y su sombra (Q16 y la decisión de la sombra del bloque 9), las monedas en amarillo como COINS, centrado en la **fila 26** (208-215 px), **100 f (2 s)**. La fila 26 no pasa por ningún agujero (el de abajo, en (166, 178), acaba en la 25), ni por los mensajes del bloque 9 (ARENA! 5, nivel 9-10, "!!!" 12, HOLD 20, COINS 28), ni por el panel del depurador de gráficos (27-29): se escribe directamente, de una vez y por la cola de DMA, sin usar la reserva de letras encima de los agujeros. Varios en el mismo frame se encolan (por orden de llegada; como cada uno sale una sola vez, la cola de 17 no se desborda) y salen uno detrás de otro. El temporizador cuenta en cada frame de partida, también con el tiempo parado (es un texto del port, como "!!!"), no en el hitstop, y en el game over; en la tienda y en la lista se para y sigue en la partida siguiente (las dos pintan en esa fila: PLAY y BACK). En el bucle de partida sólo añade una comparación a la salida rápida de `HUD_update` y otra en `HUD_tick`.

**La lista.** Se entra desde la tienda GEAR, con una entrada nueva, **"ACHIEVEMENTS n/17"**, en la fila 24 (que estaba vacía, entre "NEXT GEAR AT…" y SOUND/MUSIC). Cambio visible del menú: esa línea, y que arriba/abajo pasan por ella entre el estante y SOUND (la tienda tiene 9 entradas en vez de 8; el resto de la navegación no cambia). Por qué en la tienda y no como tercera opción del game over: el game over sólo tiene PLAY y GEAR en una fila con izquierda/derecha, y volver de la lista al game over obligaría a repintar su banda (filas 12-19) desde la grande (3-27); la tienda ya usa la banda grande y se repinta entera con `stDirty`. La pantalla usa la misma banda, la misma fuente y el mismo estilo que la tienda: título centrado "ACHIEVEMENTS n/17" (fila 4), una cabecera por grupo (PROGRESS, SHOP, POWER-UPS) y una línea por logro con `[*]` (conseguido) o `[ ]`, el nombre y "<n> COINS" alineado a la derecha; ">BACK" en la fila 26. A, B, C o START vuelven a la tienda, en la misma entrada. Se pinta una vez al abrir (22 líneas por la CPU, en el menú).

**VRAM: 0 tiles.** Todo es fuente del sistema, ya cargada (las cifras amarillas son las del bloque 9). `nextTile` **1432 de 1440** (`userTileMaxIndex`), leído por RAM a los 400 frames con la ROM normal: igual que antes del bloque.

**Rendimiento** (`tools/measure.sh`): antes **505 / 34**, después **505 / 34**. Igual: en el arnés se consiguen logros (puntuación y niveles) y su aviso se escribe, pero son pocas escrituras de una fila en 600 frames.

**Comprobado en BlastEm** (ROM normal salvo donde se dice; un conductor del depurador en el scratchpad que lee y escribe RAM con `p/d` y `set`, manteniendo B para que corra el tiempo):

- **Puntuación**: `g_factorLevel` a 100 000 (para que no suba el nivel) y `g_score` a 99, 999 y 9999: en la siguiente baja se ponen los bits 4, 5 y 6 (máscara 0x10, 0x30, 0x70) y el saldo sube 100, 300 y 1000 más las monedas recogidas entre medias.
- **Niveles**: `g_factorLevel` a 1 con 10 001 de puntuación: el nivel salta a 10 001 y en el mismo frame se ponen los bits 1-3 (0x7E), +2600 monedas exactas, y quedan 3 avisos más en cola; salen uno detrás de otro (capturas).
- **Monedas recogidas**: `g_coinsCollected` a 990 y una moneda de 10 debajo de la nave (escrita en `coins[]`): bit 11 y +110 (10 + 100); a 9990 y otra: bit 12 y +1010.
- **Bomba, rayos**: una bomba y unos rayos escritos en `pows[]` debajo de la nave: bits 13 y 14, +100 cada uno (la bomba mató a lo que hubiera, pero menos de 10).
- **Holo**: se espera a que una oleada saque una holo inactiva y se pone la nave encima: bit 15, +100.
- **Primera partida**: la nave encima de una araña: game over, bit 0, +50, y el aviso en el game over.
- **Compras**: en la tienda, saldo a 100 000 y A en las cuatro armas de pago: bits 7-10, saldo final 19 000 = 100 000 − 90 000 + 9000.
- **10 bajas con una bomba** (ROM `PORT_STRESS=1`, que llena la arena): con las 8 arañas vivas y los gusanos, una bomba debajo de la nave mata 11: bits 13 y 16, más los niveles 5 y 10 que suben esas bajas; +1200 = 100 + 500 + 100 + 500.
- **Los 17 bits** puestos al final (0x1FFFF entre las dos ROM) y la lista con 16/17 en la ROM normal.
- Las variantes normal, `PORT_STRESS=1` y `GFXDEBUG=1` compilan sin avisos.
- Capturas (en `$HOME`): aviso "SCORE 100 +100" en partida `~/blastem_20261121_134427.png`; la cola ("SCORE 1000 +300" detrás del anterior) con el mensaje de nivel puesto `~/blastem_20261121_134431.png`; aviso en el game over `~/blastem_20261121_134438.png`; la lista `~/blastem_20261121_134451.png`; la tienda con la entrada ACHIEVEMENTS `~/blastem_20261121_134452.png`.

**Divergencias que introduce este bloque (todas invención: MonsterLite y el GML no tienen logros):**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| **Los 17 logros y sus recompensas** (50 a 4800 monedas, 14 950 en total) | Decisión del usuario (Q18, 2026-11-15; cantidades, 2026-11-21) | Monedas de más: con las cuatro compras, el 10 % del precio vuelve como recompensa |
| "Recoger monedas" cuenta las cogidas en partida, acumuladas entre partidas | Decisión del usuario | Las recompensas no ayudan a conseguir el logro de monedas |
| "10 bajas con una bomba" cuenta arañas y cabezas de una detonación | Decisión del usuario; es lo que cuenta `clearEnemies` | Con los topes del port (8 arañas, 2 gusanos hasta el nivel 9 y 3 desde el 10) caben 10 u 11: hace falta la arena casi llena |
| **Aviso en la fila 26, 2 s, en cola**; también en el game over | Invención del desarrollador: la única fila libre de agujeros y de los mensajes del bloque 9 por debajo de HOLD; 2 s para poder leer 39 columnas | Tres logros a la vez tardan 6 s en verse todos |
| El aviso se para en la tienda y en la lista y sigue en la partida siguiente | Las dos pantallas pintan en esa fila | Lo comprado se anuncia al empezar la partida siguiente; en la lista ya sale marcado |
| **La lista se abre desde la tienda** (entrada ACHIEVEMENTS en la fila 24) | Invención; ver arriba por qué no en el game over | Dos pasos desde el game over (GEAR, ACHIEVEMENTS) |
| Textos en inglés, como el resto del juego | Invención | — |
| En NTSC el aviso caería en la fila 24, encima del agujero de abajo, y lo cortaría mientras se ve | El port es PAL (decisión 1); la fila sale de la de COINS menos 2 | Sólo en NTSC |

### Bloque 12: efectos y sonido (2026-11-21)

Hecho todo, con las decisiones del usuario del mismo día: las líneas de las naves holo de puntos muy juntos, fuera la muerte del GML y los PCM del GML sin uso, y **sin música en partida** (Q21 revocada: no se convierten las pistas `.xm`, ni ahora ni después; el estudio que se hizo queda al final, descartado). El usuario pidió hacer este bloque antes que el 11. Tras la verificación se corrigieron el reparto de canales (se perdían casi todos los disparos), la franja de la sacudida grande, la duración de cada muestra y varios comentarios (ver "Correcciones tras la verificación", al final de la sección).

`inc/shake.h` y `src/shake.c` (las dos sacudidas de MonsterLite, el `stShake` del GML para lo que sólo es del GML), `inc/audio.h` y `src/audio.c` (los sonidos de MonsterLite, prioridades y reparto de canales), `inc/trail.h` y `src/trail.c` (nuevos: la estela), `src/game.c` e `inc/game.h` (hitstop, estado `GAME_DYING`, nave roja, sacudidas, voces), `src/projectile.c` e `inc/projectile.h` (sonidos de disparo e impacto, bajas por bala, sacudida de las bolas), `src/mlpow.c`, `src/effect.c` e `inc/effect.h`, `src/menu.c` (ding, CARALLUDO, AWESOME, botones), `src/beam.c` e `inc/beam.h` (líneas holo en tramos, cola del pool), `src/bg.c` (el fondo envuelto en la sacudida), `inc/vramspr.h` (fondo de 528 a 504 tiles), `src/spr.c`, `src/spr_a.s` e `inc/spr.h` (pool de 320, `Spr_check`, cola), `inc/arena.h` (constantes del bloque), `res/resources.res`, `res/sound/ml*.wav`, `res/sprite/trail.png` y `res/sprite/lineseg.png` (nuevos), `res/sprite/bg_ruins.png` y `res/bg_ruins.h` (regenerados), `tools/ml2md_audio.py`, `tools/ml_trail.py` y `tools/ml_lineseg.py` (nuevos), `tools/gm2md_audio.py` y `res/sound/` (fuera nueve PCM del GML), `src/rom_header.c`.

**Hitstop (Q17).** El original duerme el hilo (`Thread.Sleep`) 30 ms por cada baja con bala (`globales.milisecsEnemyDestroyed = 30`, `S/globales.cs:81`; llamadas en `S/bulletScript.cs:62, 80` y `S/bulletThinSrc.cs:45, 65`) y 100 ms al tocar la bomba o los rayos (`S/playerMovement.cs:200, 213`). El port lleva la cuenta en milisegundos (`hitstopMs`, `src/game.c`) y cada frame congelado paga 20: 100 ms son 5 frames exactos y cada baja con bala, **1 y 2 frames alternos** (30 → un frame y quedan 10; la siguiente, 40 → dos frames), 1,5 de media, los 30 ms justos. Por eso ni 1 ni 2 fijos: 1 se queda en 20 ms (un tercio corto) y 2 en 40 (un tercio largo). Varias bajas en el mismo frame suman, como varios `OnTriggerEnter` con su `Sleep`. Cuentan las balas finas (también las de las naves holo, que son `BulletThin`) y las gordas de WBLOCK y TWAY (`Proj_takeBulletKills`); no LASER, MOIRE, CIRCULAR, la metralla, las armas del GML, la bomba ni los rayos. Medido en BlastEm (ROM de estrés, `now` de la estela como sonda de "frame de partida que corre"): una baja con bala congela 1 frame (`hitstopMs` 30 → 10), la siguiente 2 (40 → 20 → 0); una baja de metralla, ninguno.

**Cámara lenta de la muerte** (`S/gameControl.cs:690-725`). `finishGame` pone el estado en GAMEOVER y `wait()` hace dos `WaitForSeconds(0.1)` con `Time.timeScale = 0,2`: 0,2 s de juego, **1 s real = 50 frames** (`ML_DEATH_SLOW_F`), en los que `FixedUpdate` corre una vez de cada cinco (`ML_DEATH_SLOW_DIV`). Estado nuevo `GAME_DYING`: la nave se queda donde murió, **en rojo** (el frame que tiene subido, recoloreado con el color de PAL1 más parecido a (r, 0, 0), que es lo que hace `SpriteRenderer.color = Color.red`, y subido por DMA encima del suyo: 0 tiles), y uno de cada 5 frames avanza el mundo: enemigos (que siguen persiguiendo a la nave), gusanos, balas en vuelo (sus bajas suman: en MonsterLite `setScores` va después), metralla, lo del suelo, líneas, explosiones, números, la estela y la salida del HUD (`runOut` va con `Time.deltaTime`). Al acabar, `shake(4)` y `clearPlayer()`: la nave desaparece, sacudida grande de 3 frames y después el game over. Medido: estado 3 durante 50 frames, la cabeza de gusano que la mató avanza cada frame antes y un paso de cada 5 durante la cámara lenta (x 8142 → 8145 → 8151 → 8159…), luego 3 frames de sacudida (por ejemplo (−36, 20), (−33, 22), (21, 39)) y el menú.

**La nave a ×4 (`landingAnim`) no se porta.** `wait()` la reproduce en cada vuelta (`S/gameControl.cs:700-701`), y también cada frame con el tiempo parado (`:270`): con el `Animator` en modo Normal la nave se queda a ×4 (4,00 × 3,82) mientras dura, y vuelve a ×1 en 0,23 s. El VDP no escala: harían falta frames pre-escalados. A ×4 la nave de 24 px son 96×96 = 144 tiles por frame (12×12 celdas, 9 sprites de 32×32), con un hueco de VRAM de 144 tiles para subirlos por DMA: quedan 6 libres. No cabe. Alternativa, si se quiere: ×2,67 (64×64, 64 tiles) reutilizando el hueco del fogonazo de `oMuerte` cuando éste ha acabado, con 32 rotaciones pre-escaladas en ROM (64 KB), o bajar más el fondo. **Divergencia**: la nave se queda a su tamaño, en rojo en la muerte.

**Sacudidas (Q20).** `S/cameraScript.cs`: `shake(t)` pone la cámara cada frame en un punto al azar de ±4 u × ±3 u (**±80 / ±60 px**) durante t frames de Update y cambia su tamaño (zoom, que no se porta: Q20); `shakeSmall(t)`, **±0,2 u = ±4 px**. Las dos devuelven la cámara a (0, 0) al acabar. Llamadas, con t pasado a frames (÷1,2 redondeado; el bucle `while (t > 0)` da una vuelta con 0,5 y 0,3):

| Llamada | Dónde en MonsterLite | t → frames | Dónde en el port |
|---|---|---|---|
| `shake(4)` al tocar en el menú | `S/gameControl.cs:219` | 4 → 3 | `Game_update` al salir del splash (`src/game.c`, `fromSplash`) |
| `shake(4)` al acabar la cámara lenta | `S/gameControl.cs:714` | 4 → 3 | `dyingUpdate` |
| `shake(1)` bomba | `S/playerMovement.cs:206` | 1 → 1 | `playUpdate`, `POW_BOOM` |
| `shakeSmall(10)` rayos | `S/playerMovement.cs:217` | 10 → 8 | `playUpdate`, `POW_RAYS` |
| `shakeSmall(1)` WBLOCK / MOIRE | `S/disparo.cs:61, 104` | 1 → 1 | `playUpdate`, al disparar |
| `shakeSmall(0.5)` LASER, `shakeSmall(0.3)` TWAY | `S/disparo.cs:83, 122` | una vuelta → 1 | `playUpdate`, al disparar |
| `shakeSmall(3)` por frame de bola que persigue | `S/circularScr.cs:59` | 3 → 3 | `updateBall` (`src/projectile.c`) |

La bala fina no sacude en MonsterLite: se quita el `stShake(1, 1, 0.1)` del GML, que eran 0,2 px y no se veía. Se quedan los `stShake` del GML de lo que sólo existe en el GML (metralla, disparo de PIVOT y PARA). Se quitan `Shake_cruz` (la bomba y el fogonazo de la muerte del GML: el de la muerte lo cortaba `Shake_stop` en el acto), `Shake_ray` y `Shake_thinShot`, sin llamadas ya. Medido: al salir del splash, `bigLeft` 3 → 0 en 3 frames con desplazamientos (−46, −34), (19, 16), (−28, −15).

**La franja de la sacudida grande** (corregida tras la verificación): el fondo (plano B) mide 64×32 celdas y el campo 40×30; con ±80 px asomaba una franja de hasta 80 px con la última columna repetida. Ahora las columnas 40-51 del plano son las 0-11 del campo y las 52-63 son las 28-39 (`drawRow`, `src/bg.c`): en horizontal se ve el otro lado envuelto, exacto hasta 96 px. En vertical sólo sobran 2 filas (el plano tiene 32): las filas 30 y 31 son la 0 y la 1, que es exacto los primeros 16 px por debajo; por encima asoman esas dos filas antes que las de abajo del campo. El plano A (agujeros, HUD) no tiene nada en las columnas de sobra y por ahí se ve el fondo. Capturas: `~/blastem_20261121_173347.png` y `~/blastem_20261121_173348.png` (sacudida alargada por RAM).

**Estela de la nave** (`TrailRenderer` del hijo "nave" de `P/player.prefab`, leído con UnityPy: `m_Time` 0,26 s = **13 f**, `m_MinVertexDistance` 0,1 u = 2 px, ancho 0,08 u = 1,6 px → 0,001, blanca con alfa 0,44 en la cabeza y 0,94 en la cola). Un vértice cuando la nave se ha alejado más de 2 px del anterior; cada uno vive 13 frames de juego (se para con el tiempo parado y en el hitstop, y va a la quinta parte en la cámara lenta). El VDP no traza líneas: un sprite de 8×8 por vértice, con un punto de 2×2 en gris 146 los 7 primeros frames y de 1×1 en gris 219 los demás (`tools/ml_trail.py`; blanco con poco alfa sobre el verde del campo es gris). Cada vértice tiene su sprite fijo: se coloca al nacer, cambia de punto una vez y se aparca al caducar; sólo con la sacudida se recolocan todos. **13 sprites como mucho; `trail.png` son 2 tiles de VRAM.**

**Voces y efectos.** La biblioteca de `SoundManager` está en la escena (`Assets/Scenes/MonsterLite.unity`), no en el prefab, que es viejo y no tiene las voces de mujer ni el "ding". Leída con UnityPy, el clip de cada campo y cuándo suena (sólo lo que tiene llamada en el código; `MonsterLite_ultimo_commit`):

| Sonido (campo) | Clip | Cuándo suena en MonsterLite | Dónde suena en el port | Prioridad |
|---|---|---|---|---|
| bala fina (`nBulletSnd`) | `shot.wav` | cada disparo sin munición, `S/disparo.cs:149` | `Proj_fireThin` | 5, grupo de disparos |
| WBLOCK (`wBulletSnd`) | `shotBasic.wav` | `S/disparo.cs:57` | `spawn`, `PROJ_WBLOCK` | 5, grupo de disparos |
| LASER y MOIRE (`laserBulletSnd`, `moireBulletSnd`) | `laser.wav` | `S/disparo.cs:69, 95` | `fireBeam` | 5, grupo de disparos |
| TWAY (`triBulletSnd`) | `3way.wav` | `S/disparo.cs:115`, una vez por bala (3 en el mismo frame) | `Proj_fireWeapon`, una vez | 5, grupo de disparos |
| CIRCULAR (`circularBulletSnd`) | `retroUFO.wav` | `S/disparo.cs:129` | `Proj_fireWeapon` | 5, grupo de disparos |
| impacto de bala (`bombaXPLSnd`, `playBombaClip`) | `explosionDePolvo2.wav` | cada impacto de bala contra araña o cabeza, mate o no (`S/bulletScript.cs:41`, `S/bulletThinSrc.cs:33`) y cada enemigo de `clearEnemies` si no suena nada (`S/enemyController.cs:175`) | `hitML` (`src/projectile.c`) y `MLPow_clearEnemies` (una vez) | 5, grupo de impactos |
| recoger paquete o moneda (`pillaPaqueteGameSnd`) | `powUp3Wah_d.wav` | `S/playerMovement.cs:184, 190` | `playUpdate`, `POW_PACK` y `POW_COIN` | 9 |
| bomba: "BOOM" (`boomSpeechSnd`) | `Boom.wav` (el `sfxBoom` del GML, la misma toma) | `S/playerMovement.cs:202` | `POW_BOOM` | 13 |
| bomba y rayos (`bombaAmarillaXPLSnd`) | `bombaamarilla.wav` | `S/playerMovement.cs:208, 215` | `POW_BOOM`, `POW_RAYS` | 11 |
| "EXPLOSIONS" (`explosionWomanSnd`) | `xplosions.wav` | bomba, `S/playerMovement.cs:209` | `POW_BOOM` | 13 |
| "RAYS" (`raysWomanSnd`) | `rays.wav` | rayos, `S/playerMovement.cs:218` | `POW_RAYS` | 13 |
| repetea (`repeteaSpeechSnd`) | `repeteaSpeech.wav` | al activar una nave holo, `S/naveProxyController.cs:43` | `Proj_holoTouch` en `playUpdate` | 13 |
| "ARENA" (`arenaWomanSnd`) | `woman voice_short.wav` | `menu.Start`, `S/menu.cs:27`: el menú se crea una vez, al cargar la escena (`initMenu`, `S/gameControl.cs:158`) | al final de `Game_init`: tras el logo, al salir el splash | 13 |
| "ding" (`dingSnd`, tono 3) | `ding.wav` | cada paso de la cuenta de SCORE del game over, `S/gameOverSrc.cs:128` | uno por frame en que sube SCORE (Q15), `Menu_update` | 8 |
| "CARALLUDO" (`caralludoSpeechSnd`, tono 2) | `Fafarre.aiff` (una fanfarria, no la voz) | cuando la cuenta pasa por `kills − 1`, `S/gameOverSrc.cs:129-131` | igual, `Menu_update` (con 0 o 1 punto no suena, como allí) | 13 |
| "AWESOME" (`awesomeWomanSnd`) | `awe3.wav` | en cada `OnGUI` del game over mientras sale NEW RECORD!, `S/gameOverSrc.cs:262` | una vez, al entrar en el game over con récord (`Menu_enter`) | 13 |
| botón corto (`buttonShortSnd`) | `buttonShort.wav` | comprar (`S/StoreMenu.cs:226`), cambiar de arma en el estante (`:475`), encender el sonido (`S/globales.cs:461`) | `storeInput` | 9 |
| botón largo (`buttonLongSnd`) | `buttonLong.wav` | GEAR del game over (`S/gameOverSrc.cs:381`) | `Menu_update`, GEAR | 9 |

**Sin llamada en MonsterLite, no se añaden:** risa (`S/enemyController.cs:184`, comentada), crispado (`S/bulletScript.cs:124`, comentada), tres vías (`:133`, comentada), fantasma, bolas, láser hablado, bomba hablada, interferencia (`playInterferenciaClip` no la llama nadie), polvo (`playdustClip`), bomba pequeña (`playSmallBombaClip`) y `circularSpeechSnd` (vacío). Tampoco suenan la muerte (`finishGame`), los disparos de las naves holo (`disparoProxy`), los del LASER y MOIRE al matar (`RaycastScr`), las bolas al matar (`circularScr`), la explosión de una holo (`collideAndDie`) ni la de los rayos al acabar (`RayoScr`): el port los deja mudos donde antes sonaban los del GML (`aranamorteSnd` y `dustSnd` en cada `Effect_spawn`).

**Se quedan los del GML** donde MonsterLite no define nada: el disparo del PARA (`shotSnd`) y de la BOMBA del GML (`laserSnd`), las explosiones de lo que sólo es del GML (metralla, PIVOT, PARA, BOMBA: `aranamorteSnd` + `dustSnd`), la cascada de explosiones al morir un gusano (`morteGusano`, `aranamorteSnd`), las voces de arma de la tienda (`triggerSpeech`, ahora junto al botón corto de MonsterLite) y la música del menú. El tick del PSG que hacía de "ding" se sustituye por el `ding.wav` de MonsterLite (Q15). **Se quitan** (decisión del usuario del 2026-11-21): la muerte (`morteSnd` de `oMuerte`; en MonsterLite la muerte no suena) y los PCM del GML que ya nadie llamaba, `sfxWBlock`, `sfxThreeWay`, `sfxCircular`, `sfxBomba`, `sfxRayos`, `sfxPowUp`, `sfxRepetea`, `sfxRisa` y `sfxMorte` (fuera de `res/resources.res`, de `tools/gm2md_audio.py` y de `res/sound/`; comprobado con grep que no quedaba ninguna referencia).

**Conversión.** `tools/ml2md_audio.py`: `afconvert` (de macOS) lee los WAV y el AIFF, baja a 16 bits mono, aplica el tono de `SoundManager` declarando la frecuencia ×2 o ×3 en la cabecera y remuestrea a 8 bits a 13 300 Hz (con el filtro de `afconvert`), y recorta el silencio del final. 17 clips, **261 KB**.

**Presupuesto de canales y prioridades.** XGM2 mezcla 3 canales PCM. MonsterLite toca todo por un solo `AudioSource` con `PlayOneShot`, que superpone sin límite. En el port, el **canal 1 es de la música del menú** (en partida no hay música: Q21 revocada) y los efectos y voces se reparten el **2 y el 3** (`pickChannel`, `src/audio.c`): primero un canal libre; si no hay y el sonido es de los que se repiten (disparos o impactos, que llevan grupo), el canal que tenga otro de su mismo grupo, el más viejo; si no, el que tenga el sonido de menos prioridad, siempre que no supere la del nuevo, y entre iguales el más viejo; si los dos tienen más, el nuevo no suena. Cada canal guarda en RAM cuándo acaba lo suyo y con qué prioridad: el `SOUND_PCM_CH_AUTO` del driver mira el canal 3 antes que el 2 sin comparar prioridades (con la bomba, "EXPLOSIONS" cortaba a "BOOM" teniendo el estallido, de menos prioridad, en el otro) y no ve lo pedido en el mismo frame. La duración: medido en BlastEm (PAL), ARENA (69 376 muestras) suena 261 frames, 265,8 muestras por frame: 266 está bien; el frame de más que vio el verificador venía de truncar `len / 266`. Ahora se redondea hacia arriba y se suma uno por el arranque del Z80, todo resuelto al compilar (sin división por llamada). Prioridades (**invención del port**, todo tiene la 128 en MonsterLite): música 15; voces 13; estallido de bomba y rayos 11; recoger, botones 9; ding 8; disparos e impactos 5, cada uno en su grupo. Qué se corta: en combate, un canal acaba con los disparos y el otro con los impactos, y cada uno corta al anterior de su grupo; con uno solo libre (la voz ARENA ocupa el otro los 5 primeros segundos de partida) se turnan; nada de lo que se repite corta una voz. Con la bomba (4 sonidos en un frame) suenan "BOOM" y "EXPLOSIONS"; con los rayos, el estallido y "RAYS". El canal 1 no se usa para efectos aunque esté libre en partida: un tercer canal sonando cuesta ~14 vueltas de 600 en el caso peor (476 → 462; el Z80 lee cada muestra de la ROM por el bus del 68000).

**Cuántos disparos suenan** (contadores en RAM en una copia del scratchpad: sonidos pedidos y sonidos que llegan al driver). Partida normal manteniendo B hasta morir (~5 s): con los disparos a prioridad 3 por debajo de los impactos, **1 de 18** disparos de bala fina y 18 de 18 impactos (el impacto, 1,42 s, se quedaba los dos canales); con los disparos a 6 por encima, 18 de 18 disparos pero 3 de 18 impactos; con el reparto final, **18 de 18 disparos y 18 de 18 impactos**. Arnés de estrés (600 f, arena llena): antes 6 de 48 disparos y 51 de 58 impactos; ahora **40 de 47 y 49 de 57**.

**Las líneas a las naves holo (decisiones del usuario del 2026-11-21).** Antes, 3 puntos de 3×3 (`dot.png`) por línea. El usuario pidió el grosor de la estela y puntos muy juntos, y después, quitarle tiles al fondo para que salgan a 3 px en cualquier dirección. Cada sprite lleva ahora un **tramo de varios puntos de 2×2 ya dibujados** (`tools/ml_lineseg.py`, `res/sprite/lineseg.png`) en una de 7 direcciones entre 0° y 90°; de 90° a 180°, la misma con volteo vertical:

| Dirección | Paso entre puntos | Sprite | Puntos | Avance del tramo | Tiles |
|---|---|---|---|---|---|
| 0° | (3, 0): 3 px | 32×8 | 10 | 30 px | 4 |
| 18,4° | (3, 1): 3,2 px | 16×8 | 5 | 15,8 px | 2 |
| 33,7° | (3, 2): 3,6 px | 16×16 | 5 | 18,0 px | 4 |
| 45° | (2, 2): 2,8 px | 16×16 | 7 | 19,8 px | 4 |
| 56,3° | (2, 3): 3,6 px | 16×16 | 5 | 18,0 px | 4 |
| 71,6° | (1, 3): 3,2 px | 8×16 | 5 | 15,8 px | 2 |
| 90° | (0, 3): 3 px | 8×32 | 10 | 30 px | 4 |

`updateLines` (`src/beam.c`) elige la dirección más cercana a la de la línea (como mucho a 9,2°) y pone los tramos uno detrás de otro sobre la línea de verdad, cada uno apoyado de nuevo en ella. La longitud es la proyección sobre esa dirección (dos multiplicaciones; se queda corta un 1,3 % como mucho) y los tramos se centran: lo que sobra, menos de un tramo, queda en los dos extremos, debajo de la nave y de la holo. Separación entre puntos: **2,8-3,6 px según la dirección**, la misma en el salto de un tramo a otro. Una línea de 150 px son 5 sprites en horizontal o en vertical, 8 en diagonal y 10 a 18°. Topes (`inc/beam.h`): 40 tramos entre todas las líneas (`BEAM_LINE_SPRITES`), a partes iguales, y 6 sprites de línea por franja de 8 px (`BEAM_LINE_BAND`). Sólo muerden con muchas holo: entonces los tramos de una línea se reparten a lo largo con huecos entre ellos.

**Qué pierde el fondo.** Los 24 tiles salen del diccionario del fondo, `VTILE_SHARED_TILES` de 528 a **504**, regenerado con el mismo comando (`--tiles 504`). Con 24 entradas menos, las celdas de detalle que tenían entrada propia pasan a usar la más parecida: el error medio sube de 45,9 a **47,4 por canal (+3 %)** y cambian 14 073 de los 76 800 píxeles del campo (18 %), en celdas sueltas de detalle (hojas, piedras); la composición, los colores y las ruinas grandes no cambian. A tamaño real no se distingue. Capturas, mismo frame: antes `~/blastem_20261121_173725.png`, después `~/blastem_20261121_174011.png`. `trail.png` vuelve a 2 tiles (el punto azul suelto ya no hace falta).

**Sprites por línea de pantalla** (tabla del VDP volcada con `vs` y contada línea a línea, arneses en el scratchpad): 1 holo en horizontal, 4 tramos, 9 entradas en total y como mucho 7 sprites en una línea; 1 holo en diagonal, 7 tramos, 12 entradas, 4 por línea; 3 holo (horizontal y dos diagonales), 18 tramos, 25 entradas, 9 por línea; 10 holo con la arena llena, 28 tramos, 77 entradas, 18 por línea (el VDP pinta 20). Capturas: `~/blastem_20261121_175027.png` (1 horizontal), `~/blastem_20261121_175047.png` (1 diagonal), `~/blastem_20261121_175107.png` (3), `~/blastem_20261121_175128.png` (10). Antes, con puntos sueltos, en el caso de 10 casi no salía ninguno; ahora salen 28 tramos, con huecos.

**Un fallo previo arreglado de paso: el pool de sprites estaba lleno.** `MDSPR_POOL` era 256 y se pedían 293 ranuras (los bloques 8-10 sin commit): las 37 últimas, los puntos de los rayos y parte de los de las líneas a las holo, caían en la ranura sumidero y **no se dibujaban**. La estela fue la primera en notarlo (sus 13 sprites salían todos al sumidero). Ahora el pool es de 320 (se piden 316) y `Spr_check` para el juego al arrancar si vuelve a pasar, como `VTile_check`. Recorrer 60 ranuras aparcadas más costaba ~12 vueltas, así que los puntos de líneas y rayos (73) son ahora la **cola** del pool: el montador no la recorre mientras `Beam` diga que no hay ninguno a la vista (`Spr_tailVisible`), que es casi siempre. Con eso el pool más grande no cuesta nada en el caso peor.

**VRAM y RAM.** `nextTile` **1434 de 1440** (`userTileMaxIndex`), leído en ejecución: la estela 2 tiles y los tramos de las líneas 24, pagados con los 24 que pierde el fondo. `Spr_check` pasa (316 ranuras pedidas de 320). RAM: `.data` + `.bss` = **27 148 bytes** (pool +64 ranuras, nave roja 288 B, estela, canales, tabla de inversos de las líneas 802 B).

**ROM: 1 179 648 bytes (0x120000, 1,125 MB)**, antes del bloque 1 MB: +261 KB de PCM de MonsterLite y −150 KB de los nueve PCM del GML que se quitan. `rom_end` de `src/rom_header.c` pasa de 0x0FFFFF a **0x11FFFF**; `tools/check_header.py` compara con el tamaño real, no hace falta tocarlo. Hasta 4 MB no hace falta mapper.

**Rendimiento** (`tools/measure.sh`, vueltas de 600 y bajas; antes del bloque, 505/34). *Nota de la sexta entrega (2026-11-27): 505/34 es el bloque 8 (`1838619`) con la SRAM de entonces; HEAD `cbe33ae`, el bloque 12 tal como se subió, da **482/25**, y ésa es la referencia de la cuarta entrega en adelante.* La medida va al borde del frame: un coste pequeño mueve decenas de vueltas, y cualquier cambio en el uso de `random()` cambia la partida entera. Cada fila quita una cosa a la anterior, con la misma partida:

| Build | Vueltas / bajas |
|---|---|
| Bloque 12 completo | **481 / 25** |
| Sin hitstop (`PORT_NO_HITSTOP=1`) | 496 / 25 |
| … y sin estela | 501 / 25 |
| … y sin sonido | 523 / 25 |
| 10 holo activas (`PORT_STRESS_HOLO`) | 308 / 24 (sin líneas, 361 / 28; en el bloque 9, con 3 puntos por línea, 333 / 26) |

Lo que se puede decir: el **coste de CPU seguro es la estela** (~5 vueltas en el caso peor, 13 ranuras más y su código) y, sólo con muchas holo, las líneas (los tramos ahora se dibujan de verdad: 28 frente a casi ningún punto). El hitstop no gasta CPU: cambia el ritmo de la partida. **Quitar el sonido recupera 22 vueltas** en el bloque 12; el verificador midió 13 en el HEAD, donde había menos llamadas al driver (200 frente a 359 en su medida): que la parte del sonido "ya se pagaba antes" no está demostrado, sólo que el reparto de canales nuevo no cuesta más que el de antes con las mismas llamadas (498 = 498, medido antes de ampliar el pool). La bajada de bajas (34 → 25) es la partida, que es otra: con los `random()` de antes las bajas vuelven a 32.

**Comprobado en BlastEm:**

- Hitstop, ROM de estrés: ver arriba.
- Cámara lenta, ROM normal manteniendo B: ver arriba. Nave roja: `~/blastem_20261121_154224.png`; sacudida al acabar, ya sin nave, con el HUD saliendo: `~/blastem_20261121_154226.png`.
- Sacudida al empezar desde el splash: `~/blastem_20261121_154803.png` (los planos desplazados y envueltos).
- Estela, en diagonal y en horizontal: `~/blastem_20261121_155040.png` y `~/blastem_20261121_155042.png`; en la tabla del VDP, 6 entradas de 8×8 detrás de la nave.
- Líneas holo: ver arriba (`~/blastem_20261121_175027.png`, `175047`, `175107`, `175128`). Las de puntos sueltos de la primera entrega (`~/blastem_20261121_162533.png`, `163038`, `163122`) quedan como referencia del antes.
- Sonido: punto de parada en `XGM2_playPCMEx` leyendo de la pila muestra, canal y prioridad, y contadores de sonidos pedidos y sonados en una copia (resultados arriba y en la tabla de pruebas). En partida no suena la música: `Audio_menuMusic` sólo se llama al entrar en el game over (y al salir, para pararla) y desde el interruptor de la tienda; la captura de llamadas no tiene `musMenu` hasta el game over.
- `make`, `make GFXDEBUG=1` y `PORT_STRESS=1` (y `PORT_STRESS_HOLO`) sin avisos; `tools/check_header.py` bien.

**Divergencias que introduce este bloque:**

| Qué | Por qué | Consecuencia en juego |
|---|---|---|
| Hitstop de 1 y 2 frames alternos (30 ms de media) | La consola va por frames de 20 ms (Q17) | Igual de media; cada baja suelta congela 20 o 40 ms |
| El hitstop no se recupera | En Unity el `Sleep` congela la imagen pero `FixedUpdate` se pone al día en el frame siguiente (el tiempo real avanza); aquí el juego se para de verdad (Q17: "frames de pausa") | Una ráfaga de bajas alarga la partida unos frames |
| El hitstop va después de la baja, no antes | Como la bomba (bloque 7) | Se congela con la explosión ya puesta |
| Sin hitstop en las bajas de la cámara lenta | Simplificación | En MonsterLite alargaría la muerte 30 ms por baja |
| La cámara lenta frena el mundo y no el fogonazo de `oMuerte` (del GML) ni las cuentas de frames del Update | `timeScale` frena lo escalado; las cuentas por frame de Update (paquetes, rayos, láser) en MonsterLite siguen a ritmo normal, el port frena todo lo del mundo | Durante 1 s los paquetes caducan a la quinta parte |
| Sin la explosión roja de la muerte (`explosionSmall` en rojo) | No hay arte de MonsterLite y el del GML es el fogonazo de `oMuerte`, que se queda | Se ve el fogonazo del GML y la nave roja |
| El game over sale al acabar la sacudida de la muerte (3 frames más tarde) | En MonsterLite sale en el mismo frame pero no cuenta mientras la cámara se sacude; así la banda del menú (plano A) no se sacude | La banda aparece 60 ms después |
| Sacudida grande sin zoom | El VDP no escala (Q20) | — |
| El HUD y el texto se sacuden con el campo | Van en el plano A; en MonsterLite son OnGUI | Con la grande (±80 px), el HUD salta 1-3 frames |
| Las sacudidas no corren con el tiempo parado ni en el hitstop | Mover los planos con los sprites quietos los descuadraría | Una sacudida de 1-8 frames puede quedar a medias hasta que corre el tiempo |
| Estela de puntos, gris, y sin línea de un borde al otro al hacer wrap | El VDP no traza líneas ni mezcla; en Unity el wrap tira una línea de lado a lado | La estela es punteada |
| Líneas a las holo: tramos pre-dibujados de puntos de 2×2 a 2,8-3,6 px en 7 direcciones, con un tope de 40 tramos y sólo en las entradas de la tabla que deje libres el juego, detrás de todo (decisión del usuario; direcciones y topes, invención; sin el tope de 6 por franja desde la quinta entrega) | El VDP no traza líneas; 80 sprites por frame y 20 por línea | Pequeños escalones donde la dirección de la línea cae entre dos de las 7; con muchas holo, huecos entre tramos; con la arena llena, las líneas (las últimas holo primero) no se ven |
| Sin el tono aleatorio 0,9-1,1, sin los tonos que bajan (0,25 en el game over, que sube 0,1 por frame en partida) y la fanfarria de CARALLUDO a tono 2 fijo (en MonsterLite baja 0,1 por frame hasta 0,25) | XGM2 no cambia el tono de una muestra; los tonos fijos (ding ×3, fanfarria ×2) se aplican al convertir | Todo suena a su tono |
| Una voz donde MonsterLite superpone varias (TWAY ×3, `bombaXPL` por enemigo, AWESOME en cada `OnGUI`) | 2 canales de efectos | Suena una vez |
| La bomba (BOOM + estallido + EXPLOSIONS) y los rayos (estallido + RAYS) suenan como una sola muestra mezclada en la conversión (quinta entrega, decisión del usuario del 2026-11-26) | 2 canales de efectos: separadas, el estallido de los rayos se quedaba sin canal | Igual que la suma de Unity (sin recorte); no se puede cortar una sin la otra |
| La sacudida más fuerte manda hasta que se apaga: una más floja (metralla, disparo) no sustituye a la que está en marcha (quinta entrega, 2026-11-26) | "Super screenshake" de la bomba (decisión del usuario): la metralla de la siguiente baja la cortaba a los 18 frames | En el GML `stShake` sustituye siempre; durante la bomba, la metralla y la del disparo no se notan |
| Prioridades, grupos y reparto de canales | Invención del port | Con mucho a la vez, se pierden disparos e impactos antes que voces; un disparo nuevo corta al anterior |
| Fondo de 504 tiles en vez de 528 | Decisión del usuario: VRAM para las líneas holo | +3 % de error medio en el fondo, en celdas de detalle sueltas |
| Las voces de arma del GML siguen en el estante de la tienda, junto al botón corto | MonsterLite sólo toca el botón | — |
| Las líneas holo salen de una tabla precalculada en celdas de 8 px de distancia holo-nave y se anclan en la holo (cuarta entrega, 2026-11-22; sustituye al reparto de 2 en 2 por frame de la tercera) | Que no se noten en el rendimiento (petición del usuario) | Sin retraso: cada línea está siempre hecha con la celda en que está la nave; su extremo de la nave se desvía como mucho ±4 px (debajo de la nave) |
| La bomba vuelve a tener el blancazo de `fffe0af` (la paleta a medio camino del blanco 50 frames, `oExplosionCruz` del GML) y la sacudida de 30 px del bloque 8 (`1838619`, `Shake_cruz`), que se apaga en ~4 s, encima del `shake(1)` de MonsterLite (decisión del usuario del 2026-11-22). Desde la quinta entrega ninguna sacudida más floja la corta (2026-11-26) | "Blancazo en pantalla y super screenshake" | MonsterLite sólo sacude un frame; el blancazo tapa también el HUD (comparte paleta); en el GML `stShake` sustituye siempre, aunque la nueva sea más floja |
| A 45° los puntos de las líneas van cada 4,2 px (paso (3,3)) | Con (2,2) los puntos de 2×2 se tocaban por la esquina y se veía una escalera continua | La diagonal, algo más espaciada que el resto (2,8-3,6 px) |
| ARENA suena al salir el splash tras encender o RESET | En MonsterLite el menú sólo se crea al cargar la escena | No vuelve a sonar tras el game over |
| **Sin música en partida** | Decisión del usuario del 2026-11-21 (Q21 revocada) | MonsterLite tiene una pista `.xm` al azar en cada partida, en bucle, que baja de tono al parar el tiempo y de volumen al morir; el port, ninguna. En el menú y el game over sigue `musMenu` del GML |
| La nave no se agranda a ×4 (`landingAnim`) en la muerte ni con el tiempo parado | 144 tiles por frame y quedan 6 | Se queda a su tamaño |
| El fondo envuelto en la sacudida grande: exacto en horizontal; en vertical asoman 16 px de las filas 0-1 por encima | El plano mide 32 filas para 30 del campo | Sólo en la sacudida grande, 1-3 frames |

#### Sexta entrega (2026-11-27): la bomba a la mitad, arreglos de la verificación, C para todo el ensamblador y arquitectura portable

Tras la verificación de la quinta, con decisiones nuevas del usuario. Entregada por pasos (cada uno compila y está documentado).

**1. La sacudida de la bomba dura la mitad** (decisión del usuario). `Shake_bomb` (`inc/shake.h`) desvanece 1,2 por frame del GML en vez de 0,6: el mismo pico (un frame a ±79 del `shake(1)`, luego ±30) y el mismo blancazo, y la caída al doble de ritmo. Medido con una bomba en el arnés y la arena llena (`out/bench/bomb.py`, |`g_shakeX`| vuelta a vuelta): ±79, nueve vueltas a ±30 y bajando ~1 px cada 3-4 vueltas hasta 1. En frames (medido por el verificador, de la bomba a la cámara quieta): **137 frames, frente a 284 antes**. Lo que se ve después son las metrallas de bajas posteriores (3 px), que ya pueden sustituirla cuando ha bajado de 3.

**Segunda reducción (2026-11-27, decisión del usuario: "acorta el tiempo de la bomba otra vez").** `SHAKE_FADE(2,4)`: cuatro veces el desvanecimiento de oExplosionCruz, mismo pico y mismos frames arriba. Medido por el verificador (misma partida y arnés que la medida de 137): un frame a ±79, ocho a ±30 y bajada de 0,576 px por frame; **70 frames de la bomba a la cámara quieta**, contando dos pausas de hitstop.

**2. Arreglos de la verificación.**
- *Arranque de la rotación* (`src/spr.c`, `src/spr_a.s`). Los cuartos de la tabla salen ahora del `nr` del primer frame de cada vuelta de cuatro, recortado a las entradas que haya: ya no se cuentan entradas dos veces ni ninguna. Y la rotación arranca en cuanto una franja pasa de 20 sprites con lo sumado hasta ese frame: el contador de sprites de cada franja empieza en 0x8000 − 21, así que al pasar de 20 se le pone el signo y `Spr_bandAdd` lo ve con un `bmi` por franja (~8 ciclos), sin esperar al cuarto frame. Las celdas (320 px por línea) se siguen mirando al cuarto frame. Las franjas fuera de la pantalla también pueden dar el aviso temprano (una rotación de más, sin otro efecto). `tools/measure.sh`: 540/25, igual.
- *DMA sin comprobar* (`src/spr.c`). Si `DMA_queueDmaFast` falla (cola llena), `Beam_linesDmaLost` da por perdidas todas las ranuras de las líneas en la VRAM y se vuelven a subir en el frame siguiente.
- *`linesRemap` con dos holo en el mismo píxel*. Cada línea vieja se empareja con una holo nueva que no tenga ya pareja; y si al acabar no cuadra el número, se rehacen todas (no debería pasar: el orden del pool no cambia).
- *`_Static_assert`* del tamaño de `SprBandRow` (8 bytes, lo lee `Spr_bandAdd`).
- *La referencia de `measure.sh`*: HEAD `cbe33ae` da 482/25 (nota añadida donde se citaba 505/34, que es del bloque 8).
- *"Ruido" en las filas de moverse*: el verificador midió con un arnés determinista (la nave la mueve el propio juego con un patrón fijo, `-DAUTOMOVE`) y la diferencia es real: −4,4 / −4,2 / −5,2 % moviéndose. Las filas de la quinta entrega que decían "ruido" se quedan como estaban medidas, pero esa lectura era incorrecta: ver la tabla del punto 3.

**3. Las líneas: subir sólo las coordenadas.** El usuario lo aprobó (DMA de paso 8, o lo más barato). Medido antes de hacerlo: cada transferencia encolada aparte cuesta ~850 ciclos de CPU (`DMA_queueDmaFast`, medido con `out/bench/cyc.py`), y con la sacudida harían falta dos (las y y las x); lo que se ahorra son 4 bytes de DMA (~10 ciclos) y ~15 ciclos de copia por ranura a la vista, ~775 ciclos con 31 ranuras. **No compensa**: lo más barato sigue siendo lo de ahora, una copia de las ranuras a la vista en el mismo DMA que la tabla del juego. No se ha cambiado. Figuras con el arnés determinista (`out/bench/det/`, tomado del verificador: `-DAUTOMOVE`, `HOLO_SET` 1-3, `HOLO_SAFE`, 6 partidas por build, con y sin líneas):

| Escenario (estrés, 10 holo salvo `sh`) | Con líneas | Sin líneas | Diferencia (vueltas) |
|---|---|---|---|
| `s1` (colocación 1, no mueren), quieta | 401,0 | 410,5 | −2,3 % |
| `s1`, moviéndose | 358,2 | 373,2 | −4,0 % |
| `s2` (fila horizontal, no mueren), quieta | 384,2 | 398,5 | −3,6 % |
| `s2`, moviéndose | 364,0 | 384,3 | −5,3 % |
| `k1` (colocación 1, mueren), quieta | 390,2 | 398,2 | −2,0 % |
| `k1`, moviéndose | 350,8 | 367,0 | −4,4 % |
| `PORT_STRESS_HOLO`, quieta | 364,8 | 369,2 | −1,2 % |
| `PORT_STRESS_HOLO`, moviéndose | 339,2 | 348,8 | −2,8 % |
| Sin holo (`plain`) | 540,8 | — | — |

Las bajas son iguales (o difieren en 1) con y sin líneas: es la misma partida, y la diferencia es coste de verdad, no ruido. **El objetivo de <1 % no se alcanza**: −1,2 a −3,6 % con la nave quieta y −2,8 a −5,3 % moviéndose. El coste es el que mide `out/bench/cyc.py` (0,6-1,1 % del tiempo de CPU más el DMA de las ranuras a la vista), amplificado porque el estrés va al borde del frame: cada frame que se pasa pierde una vuelta entera. Lo que queda por probar, de más a menos ganancia y todo sin cambio visual: que la copia con la sacudida no vuelva a subir las ranuras que no se ven en ninguna línea de pantalla con sitio (no medido), y hacer más ligero el resto del frame (plan de 50 fps), que aleja el estrés del borde.

**4. Todo el ensamblador tiene su versión en C** (requisito del usuario: arquitectura que facilite otro port). `-DPORT_NO_ASM=1` compila sin `src/spr_a.s` ni `src/beam_a.s` y con sus equivalentes en C, que hacen la misma cuenta: `Spr_build`, `Spr_bandAdd`, `Spr_bandOver` (`src/spr.c`) y `Beam_linesScan`, `Beam_linesCopy`, `Beam_linesFlush` (`src/beam.c`). `PORT_SPR_C` desaparece (era lo mismo sólo para el montador). Comprobado con `out/bench/check_lines.py` sobre builds `PORT_NO_ASM=1` (10 holo que mueren y que no, 3 holo, quieta y moviéndose, 120 paradas): 0 diferencias con el modelo, igual que el ensamblador. Si se toca uno, hay que tocar el otro.

**5. Topes del hardware en un sitio**: `inc/hwlimits.h` (sólo `#define`, lo incluyen también los `.s`): 80 entradas de la tabla, 20 sprites y 40 celdas por línea, 8 bytes por entrada, las 40 ranuras de las líneas y la Y de aparcar. Los usan ya `src/spr_a.s`, `src/spr.c`, `inc/spr.h` e `inc/beam.h`.

**6. El plan de 50 fps, pasos 2 a 5: no han entrado en esta entrega** (rejilla de colisiones, gusanos, VBlank/DMA, lo del suelo). Siguen como en la tabla del plan.

#### Arquitectura para portar: lo que hoy la rompe (2026-11-27)

Requisito del usuario: la lógica del juego no toca el VDP, el DMA, la tabla de sprites ni SGDK; todo eso detrás de una capa fina (`spr`, `beam`, `audio`, `vramspr`, entrada) con interfaces que tengan sentido en otra plataforma; cada rutina en ensamblador con su versión en C (`PORT_NO_ASM`); tablas precalculadas sólo con su generador documentado; los topes del hardware con nombre y en un sitio. Aplicado a lo tocado en esta entrega (puntos 4 y 5). Sin coste: los cambios son `#define`, y las versiones en C sólo entran con `PORT_NO_ASM`. Lo que queda, con propuesta (no hecho; sin refactor grande ahora):

| Dónde | Qué lo rompe | Propuesta |
|---|---|---|
| `src/game.c` | Llama a `VDP_*` y `PAL_*` directamente (textos del HUD, paletas del blancazo, `VDP_showFPS` de depuración) y usa `vtimer`/`JOY_readJoypad` de SGDK | Una capa `gfx_text`/`gfx_pal` y `input` con funciones en línea; el juego sólo llama a ellas |
| `src/effect.c` (`Effect_bombFlash`) | Escribe las paletas del VDP | `Gfx_flash(nivel)` en la capa de render |
| `src/shake.c` | Escribe el scroll de los planos (`VDP_setHorizontalScrollVSync`) | La cámara como dato (`g_shakeX/Y`) y un `Gfx_camera(x, y)` en la capa |
| `src/hole.c`, `src/bg.c`, `src/hud.c` | Componen tiles en planos con SGDK | Ya son módulos de render: mover a una carpeta `render/` y que el juego llame por interfaz |
| `src/audio.c` | Bien aislado (XGM2 sólo aquí), pero los canales y prioridades son de XGM2 | Dejar `Audio_sfx(id)` como interfaz; el reparto de canales, de la implementación |
| `src/enemy.c` (`SPIDER_DMA_WINDOW`), `src/snake.c` | Deciden subidas de frames por DMA (`VStream`) dentro de la lógica | Que la lógica pida "frame N" y la capa (`vramspr`) decida cuándo sube; el tope por frame, en `inc/hwlimits.h` |
| `inc/spr.h` | `MDSpr` tiene la forma de la entrada del VDP (es lo que hace barato el montador) | Aceptable como detalle de la capa; la lógica sólo usa `sprPos`/`sprHide`/`sprAnim` |
| Topes repartidos | `VTILE_SHARED_TILES`, `MDSPR_POOL`, `SPIDER_DMA_WINDOW`, el tamaño de la cola de DMA, los 266 PCM por frame de `audio.c` | Llevarlos a `inc/hwlimits.h` |
| Ensamblador sin C | Ninguno tras esta entrega (`spr_a.s`, `beam_a.s` tienen versión en C) | Mantenerlo: cada `.s` nuevo, con su C y su comprobación |
| `src/menu.c` (27 llamadas `VDP_`/`PAL_`) y `src/logo.c` (6) | Las pantallas de menú y logo dibujan directamente con SGDK | Son render: que hablen con una capa de texto/planos/paletas (la misma que `game.c`), no con el VDP |
| `random()` de SGDK y `fix16`/`sinFix16` en la lógica (`entity.c`, `projectile.c`, `debris.c`, `effect.c`) | El generador de números y la aritmética fija son los de SGDK: en otra plataforma, otra secuencia y otros redondeos, y la partida deja de ser la misma | Un `rng.h` propio (el mismo algoritmo en todas partes) y un `fixmath.h` con el formato y las tablas del port; SGDK sólo detrás |
| `src/beam.c` | La geometría de las líneas (celdas, tramos, reparto por holo) va mezclada con la disposición de la tabla del VDP (ranuras al revés, enlaces, copia con sacudida, DMA) | Partir en `lines.c` (qué tramos van, en coordenadas del campo) y la parte de `spr` que los pone en la tabla |

#### Quinta entrega (2026-11-26): líneas holo por debajo del 1 %, la bomba que no se corta, mezclas y el plan de 50 fps

Tras la verificación de la cuarta, con decisiones nuevas del usuario: las líneas, **menos del 1 % en todos los casos** (también nave moviéndose, holo en posiciones irregulares y holo que no mueren) y sin cambio visual; primero la velocidad, después la ROM; las mezclas por evento, aprobadas; el plan de 50 fps, aprobado, sin tocar el juego.

**1. Las líneas holo, otra vez desde el principio.** Medido con un arnés nuevo (`out/bench/`, fuera del repo: copia el árbol, mete el contador de vueltas, `HOLO_N` en posiciones irregulares, `HOLO_KILLABLE=0` para holo que no mueren, `-DNO_LINES`, y la partida con la nave quieta o moviéndose; media de 6 partidas que arrancan en frames distintos) y con los ciclos exactos del depurador (`cycle` de BlastEm, que va en ciclos del reloj maestro: /7 para el 68000) en cada llamada:

- *Dónde se iba el tiempo en la cuarta entrega*: los 40 puntos de las líneas eran ranuras del pool, así que el montador las recorría y copiaba cada frame (~140 ciclos cada una a la vista: ~5 600 por frame con 10 holo); rehacer una línea en C, ~5 200 ciclos; y con la arena llena (`satUsed` 80) se calculaban líneas que no entraban en la tabla.
- *El sistema nuevo* (`src/beam.c`, `src/beam_a.s`, `tools/ml_lineseg.py`):
  - las líneas **no van en el pool**: tienen las entradas 40 a 79 de la tabla del VDP, al revés (la ranura 0 es la 79), con los enlaces fijos por ranura. El juego va en las entradas 0 a n−1; las líneas toman las que quedan libres (`80 − n`, 40 como mucho) y van **siempre detrás de todo el juego en la lista**: ni le quitan sitio en la tabla de 80 ni, si una línea de pantalla se llena (20 sprites, 320 px), le quitan sprites al juego; lo que se pierde es de ellas. Con la tabla llena no entra ninguna y **no se calcula nada** de lo que no cabe: el recorrido para en la primera línea que no cabe y lo de detrás se mira cuando vuelva a caber;
  - una **imagen sin sacudida** (`lineSat`) que sólo se toca al rehacer una línea, y la copia que se sube (`vdpSpriteCache[40..79]`) con la sacudida sumada al copiar (`Beam_linesCopy`, ~55 ciclos por ranura). La subida va **en el mismo DMA** que la tabla del juego y sólo llega hasta las ranuras que han cambiado; un `DMA_queueDmaFast` aparte costaba ~850 ciclos por frame;
  - la tabla de tramos (`res/lineseg_tab.h`) guarda cada tramo **ya en la forma de la entrada del VDP y con los cuatro volteos** (16 bytes), la caja de cada celda (si cabe en el campo, ningún tramo se mira contra el borde) y los repartos por número de holo: colocar un tramo son dos lecturas y dos sumas de palabras largas (~120 ciclos, antes ~750);
  - todo el camino caliente en ensamblador (`Beam_linesFlush`, `Beam_linesScan`): con la nave quieta, nada nuevo y las mismas entradas libres, unos 150 ciclos (contados sobre el código, no medidos); con la nave en marcha, una resta y una comparación por línea y eje movido, y rehacer sólo las que cambian de celda;
  - **sin el tope de 6 sprites de línea por franja de 8 líneas** (opción que dio el usuario): ya no protegía nada, porque las líneas van detrás de todo y no cuentan para la rotación. Diferencia visible: en una franja con más de 6 tramos de línea, ahora se ven los que el VDP pueda pintar (antes se quitaban); nunca menos.
- *Comprobado que es lo mismo que antes*: `out/bench/check_lines.py` para el juego en `Spr_flush` cada pocos frames y compara (a) la imagen en RAM con un modelo independiente en Python (la tabla de la cuarta entrega: tramo, dirección, volteo, reparto y recorte de bordes como entonces, sin el tope por franja) y (b) la lista que hay en la VRAM (`vs`): la del juego acaba en la 79, las de las líneas bajan hasta la 80−K y la última enlaza con 0, con sus posiciones iguales a la imagen más la sacudida. Con 1, 3, 6 y 10 holo, quieta y moviéndose, 270 paradas: **0 diferencias**.
- *Coste medido* (ciclos del 68000 de `Beam_linesFlush`, 150 frames seguidos, arena llena): **0,58-0,74 % del tiempo con la nave quieta y 0,74-1,08 % moviéndose** según el caso (tabla de abajo). En la cuarta entrega no se midió así; por las cuentas de entonces eran ~5 600 ciclos por frame de montador para los 40 puntos y ~5 200 por línea rehecha. Lo que queda es casi todo la sacudida (copiar las ranuras a la vista cada frame en que cambia: con la metralla de las bajas, dos de cada tres frames) y rehacer las líneas que cambian de celda.
- *Vueltas de 600* (media de 6 partidas que arrancan en frames distintos; las partidas moviéndose son caóticas y salen en dos grupos, ±5 %). Con holo que mueren, al morir una sin que cambie el reparto (de 10 a 9, de 8 a 7…) las líneas de las demás se quedan como están (`linesRemap`); antes se rehacían todas:

| Escenario (`PORT_STRESS=1`) | Cuarta entrega: con / sin líneas | Quinta: con / sin líneas | CPU de las líneas (ciclos medidos, quinta) |
|---|---|---|---|
| `PORT_STRESS_HOLO`, nave quieta | 359,0 / 366,5 (−2,0 %) | 366,0 / 370,5 (**−1,2 %**) | 0,74 % |
| `PORT_STRESS_HOLO`, moviéndose | 348,2 / 339,0 (+2,7 %, ruido) | 349,5 / 342,7 (+2,0 %, ruido) | 0,74 % |
| 10 holo irregulares que no mueren, quieta | 358,0 / 368,0 (−2,7 %) | 366,5 / 371,0 (**−1,2 %**) | 0,58 % |
| 10 holo irregulares que no mueren, moviéndose | 335,5 / 371,5 (−9,7 %) | 383,8 / 373,8 (+2,7 %, ruido) | 1,05 % |
| 10 holo irregulares que mueren, quieta | (el verificador: −5,2 %) | 377,7 / 393,8 (**−4,1 %**) | 0,64 % |
| 10 holo irregulares que mueren, moviéndose | — | 352,8 / 379,3 (−7,0 %, ruido) | 1,08 % |

La columna de CPU es la parte del tiempo que se lleva `Beam_linesFlush` (todo lo de las líneas menos apuntar la posición de la nave en `Beam_update`), contada ciclo a ciclo en 150 frames seguidos; no incluye el DMA de las ranuras que cambian, que en los frames con sacudida son todas las que se ven (~19 ciclos de vblank por ranura). **No se llega al 1 % en todos los casos**: con la nave quieta y las holo que no mueren, sí en vueltas (−1,2 %); con holo que mueren, −4,1 %: quedan menos líneas y caben más, y con la sacudida de cada baja se copian y se suben todas cada frame (~55 + ~19 ciclos por ranura a la vista). Moviéndose, la CPU medida es ~1,05 % y las vueltas salen en dos grupos según la partida (±5 %): no se puede afirmar nada por debajo de eso. Lo que queda es la sacudida: para bajarla habría que no mover las líneas con ella (cambio visible) o subir sólo las x y las y con un DMA de paso 8 (la mitad de bytes, sin probar).

**2. La ROM.** La tabla pasa de 87 a 239 KB (los cuatro volteos ya hechos, la caja por celda y los repartos) y la ROM a **1 441 792 bytes (`rom_end` 0x15FFFF)**. La simetría de la diagonal o los deltas de 8 bits costarían ciclos en cada tramo: no se hacen (decisión del usuario: primero la velocidad).

**3. Las mezclas** (aprobadas): `tools/ml2md_audio.py` suma en 16 bits lo que MonsterLite toca a la vez y lo deja en una muestra: **la bomba** (`Boom.wav` + `bombaamarilla` + `xplosions`, 24,7 KB, 1,90 s) y **los rayos** (`bombaamarilla` + `rays`, 27,9 KB, 2,14 s), sin recorte (0 % de muestras pasadas). Cada evento ocupa un canal y deja el otro para disparos e impactos. Comprobado en el depurador (`out/bench/pcm.py`, punto de parada en `XGM2_playPCMEx`): con una bomba puesta en el arnés, el driver recibe `mlBombMix` una vez, en el canal 3, y los impactos siguen sonando en el 2. Las sueltas (`mlBombaAm`, `mlXplosions`, `mlRays`, `sfxBoom`, 85 KB) ya no se usaban y se han quitado; las mezclas son 53 KB: 32 KB menos de PCM.

**4. La bomba que no se corta** (hallazgo del verificador). `Shake_start` sustituía siempre, como `stShake` del GML: la metralla de la siguiente baja (`Shake_estallido`, 3 px) o el siguiente disparo del PIVOT o el PARA (`Shake_shot`, 1 px) dejaban los 30 px de la bomba en 18 frames. Ahora **manda la más fuerte hasta que se apague**: una más floja que la que está en marcha no hace nada; una igual o más fuerte sí la sustituye (así el fuego continuo sigue renovando la suya). Medido con una bomba en el arnés y la arena llena: un frame a ±79 (el `shake(1)`), 11 frames a ±30 y bajando 1 px cada ~7 frames hasta 0, unos 210 frames (~4 s), con bajas entre medias. La regla de `src/shake.c` con la sacudida pequeña de MonsterLite (±4 px): también manda la más fuerte; con las que hay, sólo la bomba (más de 4 px durante ~180 frames) gana a la pequeña, y la metralla (3 px) y la del disparo (1 px) no se ven mientras dura un `shakeSmall` (rayos, armas, bolas). Es el efecto de la regla, no un fallo: queda documentado.

**5. Menores.** El comentario repetido de `tools/ml_lineseg.py` ("marca el final.") desaparece con la tabla nueva. El centinela `lineStamp = 0xFFFF`, que podía coincidir con un `Proj_holoStamp` de verdad, es ahora una bandera aparte (`lineHolosOK`). La atribución de la bomba: el blancazo es de `fffe0af`, no del bloque 8 (corregido arriba y en `docs/MONSTERLITE_DECISIONES.md`).

**6. El plan de 50 fps, paso 1 (el montador de sprites).** Medido dentro de `Spr_flush` en el caso peor: ~21 000 ciclos cada frame y **53 000-67 000 uno de cada cuatro**: la comprobación de franjas llenas, en C y entera, costaba ~35 000 ciclos de golpe (un cuarto de frame). Hecho:
  - la comprobación, en ensamblador (`Spr_bandAdd`, `Spr_bandOver`) y **repartida**: un cuarto de la tabla por frame, y se mira al cuarto; el resultado vale para los cuatro siguientes, como antes. Rota parecido (13 y 17 de 30 frames muestreados, en partidas distintas);
  - las ranuras aparcadas, de 44 a 34 ciclos, y las visibles, de ~150 a ~124 (`src/spr_a.s`);
  - la tabla, directa a la cola de DMA (`DMA_queueDmaFast`): `VDP_updateSprites` costaba ~1 600 ciclos y ahora ~850.

  `tools/measure.sh`: **486 → 540 vueltas de 600** (bajas: 25, la misma firma de partida). Los pasos siguientes (rejilla de colisiones, gusanos en ensamblador, VBlank) no han entrado en esta entrega.

| Paso del plan | Estado | `tools/measure.sh` (estrés, vueltas / bajas) | 10 holo quieta, sin líneas → con líneas (vueltas) |
|---|---|---|---|
| Antes (cuarta entrega) | — | 486 / 25 | 382,5 → 375,3 (que mueren); 367,3 → 362,5 (que no) |
| 1. Montador de sprites | **Hecho** | **540 / 25** | 393,8 → 377,7; 371,0 → 366,5 |
| 2. Rejilla de colisiones (proyectiles, nave) | Pendiente | — | — |
| 3. Gusanos en ensamblador | Pendiente | — | — |
| 4. VBlank (repartir las subidas de VStream) | Pendiente | — | — |
| 5. Lo del suelo, explosiones | Pendiente | — | — |

Con el paso 1 el estrés pasa del 81 % al 90 % de frames completos; con 10 holo, del ~62 % al ~65 %. Para 600/600 con 10 holo falta lo grueso (proyectiles: las balas de las holo contra todo, ~29 % del frame en la cuarta entrega). Las dos opciones que tocan el juego (sonido a 6,65 kHz, IA lejana cada 2 frames) no se han tocado.

#### Cuarta entrega (2026-11-22/23): líneas holo replanteadas, la bomba de antes y menores

Tras el rechazo de la tercera entrega por el verificador, con decisiones nuevas del usuario. La muerte se queda como está.

**1. El fallo de `lineRR`** (tercera entrega: el turno avanzaba antes del `break`, y con 3, 6 o 9 holo una línea de cada tres no se rehacía nunca). Desaparece con el sistema nuevo, que no va por turnos. Comprobado con 1 a 10 holo y la nave moviéndose 60 frames, leyendo en RAM la posición de la nave y la celda (`lineQ`) con la que está hecha cada línea: de 1 a 9 holo, ninguna línea desfasada ningún frame; con 10, 2 lecturas desfasadas de 1 frame (frames desbordados en los que el depurador lee a mitad de la actualización). **N = 0 frames** de retraso por diseño (la línea se rehace en el mismo frame en que la nave cambia de celda), 1 medido.

**2. La bomba de antes** ("blancazo en pantalla y super screenshake", decisión del usuario). *Corrección de la quinta entrega (2026-11-26): esta entrega decía "como en el bloque 8", y no es exacto: el bloque 8 (`1838619`) sólo tenía la sacudida (`Shake_cruz`); el blancazo (`Effect_bombFlash`) se lo quitó el bloque 8 y viene de la entrega anterior, `fffe0af` ("surfize 2 bloque"), que tenía las dos cosas.* Comparada frame a frame con una build del bloque 8 (`git archive 1838619`, con una bomba puesta bajo la nave en el arnés): allí `Shake_cruz` daba ±30 px durante 11 frames y luego bajaba ~1 px cada 7 frames (±24 a los 50 frames); ahora sólo había un frame a ±80/±60. La sección 9 de esta auditoría (2026-11-12) describe además el blancazo de `oExplosionCruz` (`Effect_bombFlash`: las 4 paletas a medio camino del blanco 50 frames), que tenía `fffe0af` y la bomba de MonsterLite había perdido. Ahora la bomba hace las tres cosas: el `shake(1)` de MonsterLite (un frame a ±80/±60), la sacudida del `oExplosionCruz` (`Shake_bomb`, `inc/shake.h`: ±30 px y bajando, medido igual que en el bloque 8: 30 ×11 frames, 29, 28… hasta 24 a los 50) y el blancazo. Las sacudidas arrancan al acabar el hitstop de 5 frames (en él no corre nada); el blancazo se ve ya en el primero. Para que la sacudida pequeña de los disparos no la apague un frame cada vez que dispara el TWAY, si la del GML es más fuerte que la pequeña, manda ella (`src/shake.c`). La franja de columna repetida que se veía en el bloque 8 no vuelve: el fondo envuelto de la segunda entrega. Capturas: bloque 8 `~/blastem_20261122_210832.png`, `210833`, `210835`; ahora `~/blastem_20261122_211905.png`, `211906`, `211907`.

**3. Las líneas holo, replanteadas para que cuesten lo mínimo.**

*Perfil del caso peor* (`PORT_PROF=2`, con dos etapas añadidas en una copia: las líneas y lo del suelo; líneas de pantalla por frame, 313 por frame PAL), en HEAD (`cbe33ae`):

| Etapa | Estrés | 10 holo, nave quieta | 10 holo, moviéndose |
|---|---|---|---|
| Montaje de la tabla de sprites (`Spr_flush`) | 88,8 (28 %) | 111,6 (36 %) | 109,0 (35 %) |
| VBlank (`SYS_doVBlankProcess`: DMA, driver) | 66,8 (21 %) | 51,8 (17 %) | 59,5 (19 %) |
| Gusanos | 56,1 (18 %) | 39,7 (13 %) | 28,4 (9 %) |
| Proyectiles | 34,3 (11 %) | 90,8 (29 %) | 73,4 (23 %) |
| Nave (disparo, apuntado) | 28,4 (9 %) | 35,5 (11 %) | 38,0 (12 %) |
| Lo del suelo | 17,3 (6 %) | 20,7 (7 %) | 19,7 (6 %) |
| Explosiones | 17,1 (5 %) | 18,0 (6 %) | 17,8 (6 %) |
| Arañas | 11,5 (4 %) | 4,9 (2 %) | 5,8 (2 %) |
| Líneas y haces (`Beam_update`) | 7,4 (2 %) | 17,9 (6 %) | 52,8 (17 %) |
| HUD | 3,2 (1 %) | 5,8 (2 %) | 5,8 (2 %) |

(El cronómetro de cada etapa se lleva también la interrupción de VBlank si cae dentro, así que las etapas pequeñas tienen ruido; las cifras grandes son fiables.)

*Medidas exactas* (depurador de BlastEm, variable `cycle`, con las interrupciones cortadas, en una copia): rehacer una línea en C costaba **~6 300 ciclos** (unas 80 instrucciones por tramo; el compilador vectorizaba los bucles de las franjas con trucos SWAR de 32 bits). El 68000 va a unos 12-15 ciclos por instrucción: un frame da para unas 10 000-12 000 instrucciones.

*El sistema nuevo* (`src/beam.c`, `src/beam_a.s`, `tools/ml_lineseg.py` → `res/lineseg_tab.h`):

- **Tabla precalculada en ROM**: para cada distancia de la holo a la nave en celdas de 8 px (|dx| 0-320, |dy| 0-240: 1271 celdas), la lista de tramos ya colocados respecto a la holo, calculada en coma flotante fuera de la consola (sin el `recip` que truncaba y dejaba empalmes a 2 px). Es el cuadrante de abajo a la derecha; los otros tres salen volteando el sprite.
- **Cada línea se ancla en su holo** (las holo no se mueven) y sólo se rehace cuando la nave pasa a otra celda respecto a ella: cada ~5 frames por línea a la velocidad de la nave, 1,8 líneas por frame con 10 holo. Sin retraso: siempre con la celda en que está la nave. El extremo de la nave se desvía como mucho ±4 px.
- **Mirar si hay que rehacer una línea**: cuatro comparaciones con el rectángulo de posiciones de la nave en que su celda no cambia.
- **Las posiciones de las holo** sólo se piden cuando alguna nace, se activa o muere (`Proj_holoStamp`, nuevo en `src/projectile.c`): recorrer el pool de 56 proyectiles cada frame costaba 4 vueltas con la nave quieta y 7 moviéndose.
- **El bucle de los tramos en ensamblador** (`Beam_placeSegs`, ~50 instrucciones por tramo): rehacer una línea de 4 tramos baja de ~6 300 a **~5 200 ciclos** (~2 000 fijos y ~750 por tramo).
- **Franjas por sprite**: cada sprite guarda su franja y cuántas toca, y al rehacer se resta sólo eso (antes, el rango entero de la línea, hasta 30 franjas).
- **45° con paso (3,3)**: puntos sueltos en vez de la escalera continua.
- **Al cambiar el número de holo** se rehacen todas en ese frame (ya no desaparecen todas y vuelven de 2 en 2).
- Siguen: el grosor de la estela, ~3 px entre puntos (2,8-3,6; 4,2 a 45°), 40 tramos entre todas las líneas y 6 por franja de 8 px, y la cola fuera de la rotación por franjas llenas.

*Resultado* (vueltas de 600 y bajas, 10 holo, con y sin líneas, la misma build salvo las líneas):

| Build | Nave quieta | Nave moviéndose |
|---|---|---|
| HEAD (`cbe33ae`), sin líneas / con líneas | 366 / 354 (**−3,3 %**) | 373 / 332 (**−11 %**) |
| Ahora, sin líneas / con líneas | 367 / 359 (**−2,2 %**) | 374 / 347 (**−7,2 %**) |

`tools/measure.sh` (sin holo): 486/25 (antes 482/25). Con la nave quieta queda en el borde del ±2 %; moviéndose cuesta aún un 7 %: rehacer ~1,8 líneas por frame (~9 400 ciclos). Lo que queda por probar, todo con un precio que decide el usuario:

- **Sin el tope de 6 sprites de línea por franja**: +4 vueltas moviéndose (medido). Desde la segunda entrega los sprites de las líneas van al final de la tabla y fuera de la rotación, así que si una franja se llena los que se pierden son ellos: el tope ya no protege a los sprites del juego.
- **Celdas de 12 o 16 px**: la mitad de líneas rehechas por frame (estimado, −3 a −4 % moviéndose), con el extremo de la nave desviado hasta ±6-8 px.
- **Toda `buildLine` en ensamblador** (los ~2 000 ciclos fijos): estimado, la mitad del coste que queda.

*Capturas*: 1 holo en horizontal `~/blastem_20261122_215127.png` (4 tramos, como mucho 7 sprites en una línea de pantalla), 1 en diagonal `~/blastem_20261122_215147.png` (6 tramos, puntos sueltos), 3 con la nave moviéndose `~/blastem_20261122_215207.png` (19 tramos, 8 por línea), 10 con la arena llena `~/blastem_20261122_215228.png` (26 tramos, 75 entradas, 17 por línea). **Parpadeo con la arena llena**: con 10 holo y la arena llena la tabla del VDP va por 75-80 entradas y algunas líneas de pantalla pasan de 20 sprites; los de las líneas van los últimos, así que son ellos los que el VDP deja de pintar en esas líneas, y como los enemigos se mueven, cambia de frame a frame. No afecta a los sprites del juego. Quitarlo pide menos tramos (la decisión pendiente de la densidad con 10 holo).

*ROM*: la tabla son **88 874 bytes** (1271 celdas: índice de 2 542 bytes, 62 B de filas y 14 378 tramos de 6 bytes). El contenido pasa de ~1,10 a ~1,19 MB y la ROM, que se rellena a múltiplos de 128 KB, de 1 179 648 (0x120000) a **1 310 720 bytes (0x140000)**; `rom_end` a 0x13FFFF. Se puede reducir:
  - aprovechando la simetría respecto a la diagonal (la celda (x, y) es la (y, x) con las direcciones cambiadas), la mitad, ~44 KB, y la ROM volvería a 0x120000;
  - guardando cada tramo como diferencia con el anterior en 8 bits, 3 bytes por tramo, ~45 KB;
  - con celdas de 16 px, ~22 KB.

  No se ha hecho: cuesta algo de CPU al leer (la simetría, un cambio de dirección por tramo) y la ROM sigue muy lejos de los 4 MB sin mapper.

**4. Menores.**

- `recip` (truncaba y dejaba empalmes a 2 px): ya no existe; las posiciones salen de la tabla, calculadas en coma flotante. Sólo queda `lineRecBlk` (32768 / tramos por línea) para repartir los tramos cuando no caben todos, que no afecta a la separación.
- Comentarios de la música (Q21) en `src/audio.c` y `docs/MONSTERLITE_GAP.md` (tabla de diferencias y bloque 12), corregidos. El comentario duplicado de `src/beam.c` desapareció con la reescritura.
- **El estallido de los rayos no suena si BOOM y EXPLOSIONS ocupan los dos canales** (prioridad 11 contra 13). Propuesta, sin hacer: mezclar fuera de la consola, en una sola muestra, lo que MonsterLite toca a la vez en cada evento (`bombaamarilla` + `rays` para los rayos; `Boom` + `bombaamarilla` + `xplosions` para la bomba), a prioridad de voz. Reproduce exactamente la superposición del original, cada evento ocupa un solo canal y no se puede perder el estallido. Coste: unos 60 KB de ROM.

#### Plan para 50 fps siempre (decisión del usuario del 2026-11-22; sólo el plan)

Objetivo: 600 vueltas de 600 en `PORT_STRESS` (hoy 486) y con 10 holo (hoy 359 quieta, 347 moviéndose). Un frame PAL son ~152 000 ciclos del 68000 y a 12-15 ciclos por instrucción da para ~10 000-12 000 instrucciones. El perfil de arriba (HEAD) suma ~330 líneas de pantalla por frame en estrés y ~400 con 10 holo, para 313 disponibles: hay que quitar ~20-25 % en estrés y ~40 % con 10 holo. Por partidas, de más a menos:

| Partida (estrés / 10 holo) | Qué se puede hacer | Ganancia estimada | ¿Toca el juego? |
|---|---|---|---|
| Montaje de la tabla de sprites, 28 % / 36 % | Lista de ranuras vivas en vez de recorrer las ~240 aparcadas (44 ciclos cada una: ~10 000 ciclos); el chequeo de franjas llenas incremental en vez de recorrer la tabla; sprites de varias piezas (cabezas, explosiones) precompuestos | 10-15 % | No |
| VBlank (DMA y driver), 17-21 % | Medir dentro: cola de DMA (tabla de sprites, frames de VStream de arañas, cabezas y nave), paletas; repartir las subidas de VStream entre frames (ya se hace con las arañas); que el SAT sólo suba las entradas usadas | 3-6 % | No |
| Gusanos, 9-18 % | Bucle de colocación de eslabones en ensamblador (es el más repetido después del montador); migas con aritmética de 16 bits | 5-8 % | No |
| Proyectiles, 11 % / 23-29 % | Rejilla de enemigos por celdas de 32 px para las colisiones (cada bala mira 1-4 celdas en vez de 8 arañas y 3 cabezas); las balas de las holo con la misma rejilla; bucle en ensamblador | 5 % / 12-15 % | No |
| Nave, 9-12 % | El enemigo más cercano con la rejilla; ángulo por tabla | 3-4 % | No |
| Lo del suelo, 6 % | Animar y caducar con contadores por grupo en vez de cada objeto cada frame | 2-3 % | No |
| Explosiones, 5-6 % | Tabla de animación precalculada, sin `random()` por frame | 2 % | No |
| Líneas holo, 2 % / hasta 7 % | Las opciones de la cuarta entrega (sin tope por franja, celdas de 12-16 px, `buildLine` entera en ensamblador) | 2-4 % con 10 holo | Las celdas más grandes, sí (aspecto) |
| Sonido (el Z80 leyendo muestras por el bus del 68000) | Muestras de impacto y disparo a media velocidad (6,65 kHz: la mitad de lecturas) | 2-4 % (medido: el sonido cuesta ~22 vueltas en estrés) | Sí: el sonido pierde agudos |
| Repartir trabajo entre frames | IA de arañas y gusanos lejos de la nave cada 2 frames | 5-10 % | Sí: MonsterLite mueve todo cada `FixedUpdate` |

Sin tocar el juego (las filas con "No") se llega, sumando las estimaciones bajas, a ~30 % en estrés, suficiente para 600 de 600, y a ~35-40 % con 10 holo, en el límite. Si no basta, lo siguiente tocaría el juego (sonido a media velocidad, IA cada 2 frames) o los topes (no se cambian sin el usuario). Orden propuesto: montador de sprites, rejilla de colisiones, gusanos en ensamblador, y medir después de cada paso con `tools/measure.sh` y el perfil.

#### Tercera entrega (2026-11-22): las líneas holo sin coste, y la explosión de la muerte

**1. Las líneas a las naves holo no deben notarse en el rendimiento (petición del usuario).**

Perfil antes de tocar nada (variantes en copias del scratchpad, `PORT_STRESS_HOLO`, vueltas de 600):

| Build | Nave quieta (el arnés) | Nave moviéndose (mantiene la derecha) |
|---|---|---|
| Sin líneas | 362 / 28 | 366 / 26 |
| Segunda entrega, con líneas | 308 / 24 (−15 %) | 317 / 23 |
| Segunda entrega, calculando todo pero con los sprites aparcados | 310 | — |

Es decir, el coste era **el cálculo**, no el dibujo: 4-5 multiplicaciones y un bucle por línea, y unas cuantas más por tramo, las 10 líneas en cada frame. Y con la nave quieta se recalculaba casi siempre: contado en RAM, de 259 llamadas en 600 frames, 159 recalculaban, 155 de ellas sólo porque había cambiado la sacudida (disparos y metralla la cambian casi cada frame).

El sistema nuevo (`updateLines` y `buildLine`, `src/beam.c`):

- **Lo hecho se guarda de un frame al siguiente.** Cada línea tiene su bloque fijo de tramos (40 entre las que haya) en un array propio, fuera del pool, con su posición sin sacudida, y su cuenta de sprites por franja (el tope global de 6 por franja se mantiene, restando y sumando sólo lo de la línea que se rehace).
- **Si sólo cambia la sacudida**, se desplazan los sprites ya puestos (dos restas por sprite).
- **Si se mueve la nave**, se rehacen 2 líneas por frame (`LINES_PER_FRAME`), por turnos. Con 10 holo cada línea va como mucho 4 frames tarde, unos 6 px a la velocidad de la nave, y su extremo queda debajo de ella. Si cambia el número de holo, se borran y vuelven a salir de 2 en 2 (rehacer las 10 de golpe se comía el frame).
- **Si nada se mueve, no se hace nada**: la nave se mira cada frame y las holo, que no se mueven, una vez cada 16.
- **Cálculo más barato** cuando sí toca: la dirección sale de una tabla de 32×32 (en vez de 6 multiplicaciones), los tramos que caben con una multiplicación (en vez de un bucle de hasta 25 vueltas) y, por tramo, dos sumas de 32 bits (en vez de 3 multiplicaciones).
- **El montador sólo ve lo visible**: los tramos a la vista se copian seguidos al principio de la cola del pool (sólo en los frames en que se rehace alguna línea), y la cola se recorre hasta el último (`Spr_tailVisible` con un número de ranuras en vez de sí o no), sin pasar por huecos aparcados ni por los puntos de los rayos si no hay rayos.
- **Los sprites de la cola ya no cuentan para la rotación por franjas llenas** (`overflowing`/`rotate`, `src/spr.c`): van al final de la tabla, así que si una franja se llena los que se pierden son ellos. Antes podían hacer rotar, y parpadear, los sprites del juego por culpa de una línea.

Después (misma tabla):

| Build | Nave quieta (el arnés) | Nave moviéndose |
|---|---|---|
| Sin líneas | 362 / 28 | 366 / 26 |
| **Con líneas, sistema nuevo** | **354 / 28 (−2,2 %)** | **332 / 24 (−9 %)** |
| Con líneas, 1 línea por frame | 354 / 28 | 342 / 24 (−6,6 %) |

`tools/measure.sh` (sin holo): 482/25, igual que antes. Con la nave quieta, la diferencia queda en el borde del ±2 % pedido; lo que queda es sobre todo enseñar ~28 sprites más (su paso por el montador y su desplazamiento con la sacudida), que no se puede quitar sin enseñar menos. Con la nave moviéndose sigue costando un 9 %: dos líneas rehechas por frame. Si se acepta más retraso, una línea por frame lo deja en un 6,6 % con hasta 9 frames (unos 13 px) de retraso con 10 holo. Con 1-3 holo el retraso es de 1-2 frames. Capturas con el sistema nuevo: 1 holo `~/blastem_20261122_012910.png`, 3 `~/blastem_20261122_012930.png`, 3 con la nave moviéndose `~/blastem_20261122_012949.png`, 10 con la arena llena `~/blastem_20261122_013009.png` (25 tramos, como mucho 16 sprites en una línea de pantalla).

VRAM sin cambios (1434 de 1440); pool sin cambios (316 de 320, `Spr_check` pasa); RAM +2,2 KB (tabla de direcciones 1 KB, tramos fuera del pool 720 B, franjas por línea 310 B): 29 368 bytes.

**2. La explosión al morir (el usuario la prefería como antes del bloque 12).**

Comparada frame a frame con una build de HEAD (`git archive HEAD` en el scratchpad, con los `.wav` del GML regenerados ahí con `tools/gm2md_audio.py`), leyendo `deathTimer`, `deathWhite` y `g_shakeX` en cada frame y con capturas:

- **El destello blanco es igual en las dos.** El sprite de `oMuerte` crece los 4 primeros frames y después el color de fondo se pone blanco 11 frames (`deathWhite`), en HEAD y ahora, al mismo ritmo (en la cámara lenta el fogonazo va a su velocidad, `Effect_updateDeath`). Píxeles blancos en el mismo momento de la fase blanca: 6 885 en HEAD y 7 501 ahora. Capturas de HEAD: `~/blastem_20261122_005934.png`, `005935`, `005936`, `005937`; de ahora: `~/blastem_20261122_010036.png`, `010037`, `010039`.
- **En HEAD no había sacudida.** `Effect_death` llamaba a `Shake_cruz()`, pero `Game_killPlayer` hacía `Shake_stop()` en el mismo frame y el menú no actualiza la sacudida: `g_shakeX` vale 0 en todos los frames tras morir. Es así desde el bloque 1 (`4628c56`); antes (`870597a`) no había `Shake_stop`.
- **Lo que sí cambia** respecto a HEAD: la nave no desaparece, sigue en rojo 1 s con el mundo a cámara lenta y después hay 3 frames de sacudida grande; el game over (y su fogonazo rojo del fondo, con más de 10 puntos) sale 53 frames más tarde; y no suena `morteSnd` (quitado por decisión del usuario del 2026-11-21).

No se ha cambiado nada: no hay sacudida que recuperar, y el destello está. Opciones, a falta de que el usuario diga qué echa de menos:

- (a) dejarlo así;
- (b) poner la sacudida del `oExplosionCruz` del GML (30 px, decayendo durante ~4 s) en el momento de morir, que es lo que el código de HEAD pretendía. Choca con la cámara lenta: mover los planos cada frame con los sprites quietos 4 de cada 5 frames los descuadraría. Se puede hacer que la sacudida avance sólo en los pasos del mundo (uno de cada 5 frames, a cámara lenta como todo) y recolocar la nave en esos pasos;
- (c) quitar la cámara lenta y volver a la muerte de HEAD (game over en el acto), que es lo que más se parece a "como antes".

**La bomba**: en HEAD usaba `Shake_cruz` (sacudida del GML de 30 px durante 8 frames y que después baja 0,14 px por frame, unos 4 s en total) y ahora `Shake_big(1)` de MonsterLite (un frame a ±80/±60 px). Visiblemente, la bomba ha perdido los ~4 s de temblor que se apagan poco a poco; a cambio da un golpe más fuerte de un frame y 5 frames de hitstop. No se ha cambiado.

#### Correcciones tras la verificación (2026-11-21)

| Observación del verificador | Qué se ha hecho |
|---|---|
| En combate sólo sonaba 1 de cada ~18 disparos de bala fina: el impacto (1,42 s, prioridad 5) se quedaba los dos canales | Disparos e impactos a la misma prioridad, cada uno en su grupo, y un sonido con grupo corta antes a otro de su grupo (`pickChannel`). Medido: 18/18 disparos y 18/18 impactos en una partida normal; 40/47 y 49/57 en el arnés de estrés (antes, 1/18 y 6/48 disparos) |
| La nave a ×4 (`landingAnim`) en la cámara lenta | No cabe en VRAM (144 tiles por frame, quedan 6): divergencia documentada, con la alternativa (arriba y en la tabla) |
| Franja con la última columna repetida en la sacudida grande | El fondo rellena las columnas de sobra con el otro lado del campo: envuelto exacto en horizontal; en vertical, lo más cercano que permiten 2 filas de sobra (arriba) |
| `chEnd` con 266 muestras por frame: ARENA sonaba un frame más | Medido el ritmo real en PAL: 265,8 muestras por frame, 266 vale; el frame de más era truncar la división. Ahora se redondea hacia arriba, resuelto al compilar |
| Comentarios desfasados (`src/game.c` sobre la sacudida y FIRST GAME, `inc/trail.h` y este documento sobre los tiles, la separación de 3 px en diagonal, la tabla de rendimiento) | Corregidos; la tabla de rendimiento dice ahora sólo lo medido |
| Algunas líneas casi verticales salían a 2,87 px | Ya no aplica: con los tramos pre-dibujados la separación la fija el dibujo (2,8-3,6 px según la dirección). En la versión de puntos sueltos se corrigió con una longitud que nunca se pasa de la real (comprobado en Python en todas las líneas: mínimo 3,000 px) |

#### Estudio de la música (Q21): descartado

**Descartado por decisión del usuario del 2026-11-21:** no se convierten las pistas `.xm`, ni ahora ni después; en partida no hay música y en el menú y el game over sigue `musMenu`. Se deja el estudio tal como se hizo, por si se vuelve a abrir.

**Qué hay.** `songsBatch` (`S/songsBatch.cs:12-16, 35-50`) tiene cinco campos `track_1`…`track_5`; en la escena apuntan a `Assets/Sound/track_1.xm`…`track_5.xm` (leído con UnityPy). `track_6.xm`, `My1stChiptuneSong.xm`, `My2ndChiptuneSong.xm`, `songTracker*.xm` (4,4 MB) y `serpentSong.wav` no los usa nadie. El `AudioSource` "Music" tiene `Loop` encendido y volumen 0,09 en la escena. `selectSong()` elige una al azar (`Random.Range(0, 5)`) en `Awake` y en cada `toGame()` (`S/gameControl.cs:375`): en el menú suena una al azar, y en cada partida otra. En partida (`S/gameControl.cs:268-311`, Update): con el tiempo parado el tono baja 0,1 por frame hasta 0 (~8 frames: se "para la cinta"); con el tiempo corriendo vuelve a subir 0,1 por frame hasta 1 y el volumen sube 0,2 por frame hasta 1; al morir (`slowDead`) el volumen baja 0,2 por frame (de 1 a 0 en 5 frames de Update, ~4 frames).

Las cinco son módulos de MilkyTracker de 8 canales (5 en uso), un patrón de velocidad 6 a 125 BPM, sin efectos de salto:

| Pista | Tamaño | Duración del bucle | Instrumentos con muestra | PCM a 13 300 Hz | a 6 650 Hz |
|---|---|---|---|---|---|
| track_1 | 28 715 B | 23,0 s | charles, caja, bombo, bajo y trémolo (bucles de 500 B) y uno de 300 B | 299 KB | 150 KB |
| track_2 | 28 132 B | 38,4 s | charles, caja, bombo y una onda de 50 B en ping-pong | 499 KB | 249 KB |
| track_3 | 27 548 B | 23,0 s | charles, caja, bombo, dos ondas de 500 B | 299 KB | 150 KB |
| track_4 | 28 703 B | 46,1 s | sólo charles, caja y bombo | 598 KB | 299 KB |
| track_5 | 28 092 B | 7,7 s | sólo charles, caja y bombo | 100 KB | 50 KB |
| **Total** | 141 KB | | las tres de batería son las mismas en todas (22 KB) | **1,75 MB** | 0,88 MB |

(Duraciones de un lector de XM en el scratchpad; Furnace renderiza `track_5` en 7,68 s, igual.) Es chiptune: batería muestreada y ondas cortas en bucle (cuadradas y de sierra), que es justo lo que hacen el PSG y el YM2612.

**Opciones:**

| | a) FM/PSG nativo (XGM2 desde VGM) | b) Renderizar a PCM | c) Híbrido: XGM2 con la batería en PCM y el resto en PSG |
|---|---|---|---|
| Cómo | Furnace (**instalado**, 0.6.8.3 en `/Applications/Furnace.app`) importa `.xm` ("detected a FastTracker module") a un sistema PCM genérico; se cambia el sistema a Genesis (YM2612 + SN76489), se rehacen los instrumentos (onda cuadrada → PSG, bajo → FM, batería → canal DAC con las tres muestras), se exporta VGM (`-vgmout`) y rescomp lo pasa a `XGM2` (con varias pistas en un recurso, comparten las muestras). DefleMask no importa XM y no está instalado | Furnace renderiza sin interfaz (`-output pista.wav`, probado con `track_5`) y se convierte como los efectos; `XGM2_playPCMEx(..., SOUND_PCM_CH1, 15, FALSE, TRUE)` en bucle | Como a), pero con los canales de onda pasados al PSG tal cual (sus bucles ya son cuadradas) y sin FM: menos arreglo |
| ROM | ~5-15 KB por pista + 22 KB de batería: **< 100 KB** | **1,75 MB** (0,88 MB a media velocidad); la ROM pasaría de 1,25 a ~3 MB (cabe en 4 MB sin mapper) | < 100 KB |
| Fidelidad | La de quien la arregle: notas y ritmo exactos, timbres parecidos | Exacta salvo 13 kHz y 8 bits (a 6,65 kHz pierde los platillos) | Muy alta en la batería y las cuadradas; el bajo sin FM suena más fino |
| Trabajo | Manual, de horas por pista, con Furnace; no se automatiza | Automático: un script como `ml2md_audio.py` | Manual, menos que a) |
| CPU | Z80 (el driver hace todo); el 68000 sólo arranca y para. La batería usa el canal 1 a ratos | Z80 y **bus del 68000**: el canal 1 sonando siempre; medido, un tercer canal PCM cuesta ~14 vueltas de 600 en el caso peor | Como a) |
| Tono que baja al parar el tiempo | No hay cambio de tono en el driver: se aproxima con `XGM2_setMusicTempo` bajando a 0 y `XGM2_pause`, o con `XGM2_pause` directamente | Tampoco: lo más parecido es relanzar la muestra desde su posición a media velocidad (`halfRate`, una octava abajo) un par de frames y pararla | Como a) |
| Volumen al morir | `XGM2_fadeOutAndStop(4)` (FM y PSG; ~4 frames, como el original) | XGM2 no tiene volumen de PCM: se para en seco (el original tarda ~4 frames: casi igual) | Como a) |
| Pista al azar por partida | `XGM2_playTrack(random() % 5)` en `Game_start`, y otra en el splash | `XGM2_playPCMEx` de una de cinco muestras | Como a) |
| Riesgos | El arreglo (timbres) depende de quien lo haga; xgm2tool tiene sus límites de PCM (las muestras de batería a 13,3 o 6,65 kHz) | Tamaño de la ROM; coste de bus en el caso peor; y la música del menú (`musMenu`, 333 KB del GML) habría que decidir si sigue | Pocos |

**Recomendación (ya sin efecto: estudio descartado): c), y a) si se quiere más.** Las pistas ya son batería muestreada más ondas cuadradas: con Furnace se pasan casi tal cual a batería en el canal DAC/PCM del XGM2 y melodía en el PSG, pesan menos de 100 KB las cinco, no tocan el bus del 68000 más que la batería y dejan a mano `XGM2_fadeOutAndStop` para la muerte y `XGM2_pause`/`setMusicTempo` para el tiempo parado. Encaja con el reparto de canales de este bloque: el canal 1 queda para la batería de la música y los efectos siguen en el 2 y el 3. La b) es la única automática y la más fiel, pero cuesta 1,75 MB y un canal PCM sonando siempre, que en el caso peor se nota. Antes de empezar hay que decidir con el usuario: quién arregla las pistas en Furnace; si `musMenu` (del GML) se sustituye por la pista al azar del menú, como en MonsterLite; y si el tono que baja al parar el tiempo puede ser una pausa.


## Pruebas realizadas

| Fecha | Prueba | Resultado |
|---|---|---|
| 2026-11-23 | Bloque 12, cuarta entrega: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1`, `tools/check_header.py`, `tools/measure.sh`; arranque en BlastEm hasta la partida | ✅ sin avisos; ROM 1 310 720 bytes, `rom_end` 0x13FFFF; 486/25; `nextTile` 1434 de 1440; pool 316 de 320 (`Spr_check` pasa); `.data` + `.bss` 27 044 bytes; `save.sram` sin tocar |
| 2026-11-23 | Líneas holo: desfase con 1 a 10 holo y la nave moviéndose 60 frames (celda de cada línea en RAM frente a la que le toca) | ✅ 0 frames de 1 a 9 holo; con 10, 2 lecturas de 1 frame (frames desbordados) |
| 2026-11-22 | Líneas holo: banco de `buildLine` con las interrupciones cortadas (`cycle` del depurador), perfil por etapas, variantes con y sin líneas, quieta y moviéndose | C: ~6 300 ciclos por línea; ensamblador: ~5 200 (~2 000 + ~750 por tramo); −2,2 % quieta y −7,2 % moviéndose (HEAD: −3,3 % y −11 %) |
| 2026-11-22 | Bomba frente al bloque 8 (`g_shakeX/Y` frame a frame y capturas) | ✅ ±30 px decayendo igual que en el bloque 8, con un primer frame a ±80/±60 y el blancazo (el de `fffe0af`; corregido en la quinta entrega) |
| 2026-11-22 | Bloque 12, tercera entrega: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1`, `tools/check_header.py`, `tools/measure.sh` | ✅ sin avisos; ROM 1 179 648 bytes; 482/25; `nextTile` 1434 de 1440; pool 316 de 320 |
| 2026-11-22 | Líneas holo: perfil (calcular sin enseñar, contar recálculos y sus motivos en RAM) y medidas con la nave quieta y moviéndose, con y sin líneas | ✅ 354/28 frente a 362/28 quieta (antes 308/24); 332/24 frente a 366/26 moviéndose (antes 317/23) |
| 2026-11-22 | Muerte frame a frame contra una build de HEAD (`deathTimer`, `deathWhite`, `g_shakeX`, capturas) | Destello blanco igual en las dos (11 frames de fondo blanco); en HEAD `g_shakeX` siempre 0 |
| 2026-11-26 | Quinta entrega: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1` (con `touch` antes de cada una), `tools/check_header.py`, `tools/measure.sh`, arranque en BlastEm | ✅ sin avisos; `rom_end` 0x15FFFF; 540/25; `sprPoolCount` 276 de 288; VRAM 1434 de 1440 |
| 2026-11-26 | Líneas holo: imagen y lista del VDP contra un modelo independiente (`out/bench/check_lines.py`, 1, 3, 6 y 10 holo, que mueren y que no, quieta y moviéndose, 270 paradas) | ✅ 0 diferencias |
| 2026-11-26 | Líneas holo: ciclos de `Beam_linesFlush` (150 frames) y vueltas con y sin líneas (`out/bench/bench.sh`, 6 partidas por caso) | Ver la tabla de la quinta entrega: −1,2 % quieta con holo que no mueren, −4,1 % con holo que mueren; CPU 0,6-1,1 % |
| 2026-11-26 | Bomba con la arena llena (`g_shakeX` frame a frame) | ✅ ±79 un frame, ±30 once, y bajando hasta 0 en ~210 frames sin cortes |
| 2026-11-26 | Mezclas: muestras que recibe el driver alrededor de una bomba (`out/bench/pcm.py`) | ✅ `mlBombMix` una vez, en el canal 3; impactos en el 2 |
| 2026-11-21 | Bloque 12: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1` (y `PORT_STRESS_HOLO`), `tools/check_header.py` | ✅ sin avisos; ROM 1 179 648 bytes, `rom_end` 0x11FFFF |
| 2026-11-21 | Bloque 12: `tools/measure.sh` y variantes en copias del scratchpad (sin hitstop, sin estela, sin sonido, política de canales de antes, 10 holo con y sin líneas) | ✅ 481/25 (antes 505/34); 10 holo 308/24; desglose en la sección del bloque 12 |
| 2026-11-21 | Hitstop por RAM (ROM de estrés, `hitstopMs`, bajas y `now` de la estela frame a frame) | ✅ baja con bala: 1 frame (30 → 10) y la siguiente 2 (40 → 20 → 0); baja de metralla: 0 |
| 2026-11-21 | Muerte por RAM (`g_state`, `dyingFrames`, `bigLeft`, `g_shakeX/Y`, x de la cabeza de gusano) y capturas | ✅ 50 frames de cámara lenta con el mundo a un paso de cada 5, nave roja, 3 de sacudida dentro de ±80/±60 y game over |
| 2026-11-21 | Sacudida al salir del splash | ✅ 3 frames: (−46, −34), (19, 16), (−28, −15); sin sacudida con el PLAY del game over |
| 2026-11-21 | Estela: tabla del VDP y capturas | ❌ los 13 sprites caían en la ranura sumidero: el pool (256) estaba lleno desde antes del bloque, con 293 pedidas (los puntos de rayos y líneas no se veían); ✅ pool de 320 con `Spr_check`, 316 pedidas, 6 puntos detrás de la nave |
| 2026-11-21 | Sonido: punto de parada en `XGM2_playPCMEx` (muestra, canal y prioridad de la pila), tres tramos de partida | ✅ ARENA al salir el splash (canal 3, 13); impactos `mlXpl` (5) y metralla del GML (`sfxAranaMorte` + `sfxDust`); la bala fina (3) no entra mientras los dos canales tienen algo de más prioridad; `sfxMorte` (7) al morir; 54 frames después, `musMenu` (canal 1, 15) y AWESOME (13) con récord; con 2 puntos, CARALLUDO en el primer paso de la cuenta; ❌ con el ding a prioridad 1 la cuenta salía muda (la muerte del GML ocupaba el otro canal); ✅ a prioridad 8, un ding por paso en el canal 2 |
| 2026-11-21 | Líneas holo de puntos sueltos (primera versión, sustituida): arneses de 1, 3 y 10 holo | 1 holo: 17 puntos a ~8 px en una línea casi plana; 3 holo: ~12 px; 10 holo: los puntos casi no salen. 327/24 con 10 holo |
| 2026-11-21 | VRAM y RAM en ejecución | ✅ `nextTile` 1434 de 1440 (fondo de 504); `Spr_check` con 316 de 320; `.data` + `.bss` = 27 148 bytes |
| 2026-11-21 | Disparos y impactos que suenan (contadores en RAM en una copia) | ❌ reparto de la primera entrega: 1/18 disparos en partida normal, 6/48 en estrés; ❌ disparos por encima: 3/18 impactos; ✅ grupos: 18/18 y 18/18; 40/47 y 49/57 |
| 2026-11-21 | Ritmo del driver XGM2 en PAL (copia que mira cuándo deja de sonar ARENA) | ✅ 69 376 muestras en 261 frames: 265,8 por frame |
| 2026-11-21 | Sacudida grande con el fondo envuelto (`bigLeft` alargado por RAM) y capturas | ✅ sin franja repetida; el otro lado del campo por cada borde |
| 2026-11-21 | Fondo de 504 tiles: capturas antes y después en el mismo frame | ✅ 14 073 de 76 800 píxeles distintos, en celdas sueltas de detalle; RMSE 45,9 → 47,4 |
| 2026-11-21 | Líneas holo en tramos: arneses de 1 (horizontal y diagonal), 3 y 10 holo; `vs` contado línea a línea | ✅ 4 / 7 / 18 / 28 tramos; como mucho 7 / 4 / 9 / 18 sprites en una línea de pantalla |
| 2026-11-21 | Fuera la muerte del GML y nueve PCM del GML: grep de cada nombre | ✅ ninguna referencia fuera de la tabla de `audio.c` antes de quitarlos; compila sin avisos |
| 2026-11-21 | Botón RESET: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1`, `tools/check_header.py`, `tools/measure.sh`; en BlastEm `ui.soft_reset` a los 200 f del splash, en partida (dos RESET seguidos) y tras comprar LASER en GEAR y apagar el sonido | ✅ sin avisos; 505/600 y 34 bajas (igual que antes); sin el arreglo, "VRAM: 1753 de 1425 tiles" (`nextTile` 1432 → 1769); con él, `nextTile` 1432, splash, saldo 0, compradas 0x1, arma 0, sonido y música encendidos, y la partida siguiente arranca bien |
| 2026-11-21 | Sin SRAM: `make`, `make GFXDEBUG=1`, `PORT_STRESS=1`, `tools/check_header.py`, `tools/measure.sh`; RAM a los 200 f de encender; captura del splash | ✅ sin avisos; cabecera sin SRAM (12 espacios en 0x1B0); 505/600 y 34 bajas; `save.sram` sin cambios; saldo 0, récord 0, arma 0 (WBLOCK), compradas 1, sonido y música encendidos |
| 2026-11-20 | Bloque 9: `make`, `make GFXDEBUG=1`, `PORT_BENCH=1` con `PORT_VRAM_REPORT=1`, `PORT_STRESS=1` (también con `PORT_STRESS_MORTAL` y `PORT_STRESS_HOLO`), `PORT_PROF=2`, `tools/check_header.py` | ✅ sin avisos; cabecera bien |
| 2026-11-20 | Bloque 9: `tools/measure.sh` (dos pasadas y dos disposiciones del código) y con `PORT_STRESS_HOLO` | ✅ 509/600 y 37 bajas (HEAD 508/37); con 10 holos 334/26 (HEAD 333/26) |
| 2026-11-20 | Bloque 9, arreglos finales: "!!!" en la fila 12 (Q22), descendentes con sombra; las 7 variantes, `tools/check_header.py`, `tools/measure.sh` | ✅ capturas de "!!!" con la nave en (160, 120) y con el mensaje de nivel, y de la tienda; medidas en el texto |
| 2026-11-20 | Bloque 9, deslizamiento, sombra y SCORE en grande: las 7 variantes, `tools/check_header.py`, `tools/measure.sh` (y dos disposiciones), `PORT_STRESS_HOLO`, capturas en PAL y NTSC, game over, tienda, depurador | ✅ sin avisos; 509/37 (509, 509) y 333/26; VRAM 1432 de 1440 |
| 2026-11-20 | Bloque 9: mensajes por RAM y en capturas (ARENA!, nivel, "!!!", HOLD, salto de COINS), reborde rojo con texto encima, segunda partida, NTSC, depurador de gráficos | ✅ 25 / 155 / 26 / 4 f; letras compuestas sin cortar los agujeros; 0 letras sin componer con todo forzado; VRAM 1434 de 1440 |
| 2026-11-20 | Bloque 8: `make`, `make GFXDEBUG=1`, `PORT_BENCH=1` con `PORT_VRAM_REPORT=1`, `PORT_STRESS=1` (también con `PORT_STRESS_MORTAL` y `PORT_STRESS_HOLO`), `PORT_PROF=2`, `tools/check_header.py` | ✅ sin avisos |
| 2026-11-20 | Araña por RAM en el arnés, niveles 1 y 9 (velocidad, rumbo, destino, temporizador, fracción de giro) | ✅ dentro de los rangos de §2.6; re-sorteo a los 167 f de juego y luego cada 26 f (ATTACK) y 14 f (WANDER, nivel 1) |
| 2026-11-20 | Gusano con el tiempo parado (ROM normal, soltando B) | ✅ quieto 40 frames con el temporizador bajando 6 por frame |
| 2026-11-20 | `tools/measure.sh`, dos pasadas | ✅ 501/600 y 32 bajas las dos (antes del bloque, 482/21) |
| 2026-11-20 | Holo inactiva al contacto: copia del arnés con 3 holos inactivas; `tools/measure.sh` y `PORT_STRESS_HOLO`; las 7 variantes | ✅ las 3 mueren antes de +80 (control: vivas a +600); 508/37 y 333/26; sin avisos |
| 2026-11-20 | Arreglos de la verificación: `vs` a +4 frames en el arnés; holo con `PORT_STRESS_MORTAL` en el game over; las 7 variantes de compilación; `tools/measure.sh` dos pasadas | ✅ peor línea de 31 a 9 sprites; la holo muere en el game over (control sin el arreglo: viva a +1200); sin avisos; 497/600 y 32 bajas (fase, ver "Bloque 8") |
| 2026-11-17 | Bloque 7: `make`, `make GFXDEBUG=1`, `PORT_BENCH=1`, `PORT_STRESS=1` (también con `PORT_STRESS_MORTAL` y `PORT_STRESS_HOLO`), `tools/check_header.py` | ✅ sin avisos |
| 2026-11-17 | Bomba junto a la nave en el arnés (8 arañas y 3 gusanos), nave a la derecha | ✅ 11 bajas y +11 de puntuación en el nivel 1; la X de la nave queda fija mientras las bajas pasan de 0 a 11 (congelación) |
| 2026-11-17 | Rayos junto a la nave en el arnés | ✅ rayos de puntos hacia los enemigos (también dentro de los agujeros); al acabarse, bajas de 3 a 12 |
| 2026-11-17 | Nave holo junto a la nave | ✅ azul e inactiva; al tocarla, línea de puntos azul a la nave y bala hacia el agujero de arriba |
| 2026-11-17 | Arnés 1500 f con las oleadas reales | ✅ nivel 7, ~8 holo inactivas y 3 cajas de bomba o rayos en el campo |
| 2026-11-17 | Rendimiento (ver la tabla del bloque 7) | ✅ el código nuevo no cuesta; 10 holo activas: 314/600 |
| 2026-11-16 | Botones A y C (decisión del usuario): `make` sin avisos; en BlastEm, A en el splash empieza la partida; sin botón sale "HOLD A/B/C" sobre el agujero de abajo y el tiempo se para; con C mantenido el tiempo corre (la nave pasiva acaba en el game over) | ✅ |
| 2026-11-16 | Logo y bloque 6: `make`, `GFXDEBUG=1`, `PORT_BENCH=1`, `PORT_STRESS=1`, `tools/check_header.py` | ✅ sin avisos; cabecera con `rom_end` 0x0FFFFF y SRAM 0x200001-0x203FFF |
| 2026-11-16 | Logo: capturas a los 20, 35 y 52 f de encender | ✅ en negro hasta que carga el driver de sonido; "SCREEN IMPLOSION" y el recuadro de círculos centrados, en gris subiendo; después el splash con la fuente en blanco |
| 2026-11-16 | Persistencia (copia `savetest.bin`): comprar LASER con 10000, salir con `quit`, volver a arrancar | ✅ saldo 4000, compradas WBLOCK y LASER, 1 partida; ❌ arma equipada WBLOCK (se guardaba antes de equipar); ✅ corregido: con TWAY, 18000, compradas 7, arma 2 |
| 2026-11-16 | Corrupción a mano del `save.sram` | ✅ con la copia más reciente estropeada carga la anterior (4000, WBLOCK y LASER); con las dos, valores por defecto (0, WBLOCK) |
| 2026-11-16 | Bloque 5: `make`, `GFXDEBUG=1`, `PORT_BENCH=1`, `PORT_STRESS=1` | ✅ sin avisos |
| 2026-11-16 | Modelo de la cuenta del game over contra la tabla de §2.8 (Python) | ✅ 14/42, 45/117, 100/173, 200/417 frames de Update: iguales |
| 2026-11-16 | Recorrido con el pad: splash, START, partida, game over, GEAR, comprar LASER con 10000 por RAM, PLAY | ✅ PRESS START parpadea; game over con SCORE, COINS COLLECTED, LEVEL, BEST SCORE, PLAY/GEAR; tienda; saldo 10000 → 4000, arma 1 (LASER), "AVAILABLE!", la partida siguiente empieza con el LASER |
| 2026-11-16 | Ritmo de la cuenta con 5000 / 500 forzados por RAM | ❌ 136 y 198 f (la banda repintándose entera perdía frames); ✅ 88 y 150 f (MonsterLite: 83 y 144) |
| 2026-11-16 | Bloque 4: `make` | ✅ sin avisos |
| 2026-11-16 | Reparto moneda/paquete, contadores temporales en una copia, arnés, 3000 f | ✅ WBLOCK 55/56 (119 bajas), LASER 64/59 (128 bajas); la diferencia son bajas por metralla |
| 2026-11-16 | Moneda: nave puesta por RAM encima | ✅ saldo y partida +60; vuela a 3,14 px/f hacia abajo |
| 2026-11-16 | Paquete: nave puesta por RAM encima, WBLOCK | ✅ munición 154 → 164 (+10); sube y se acaba en 2 f (le quedaban 57 de vida) |
| 2026-11-16 | Capturas en partida | ✅ monedas doradas girando, paquetes, COINS abajo |
| 2026-11-16 | Caso peor tras el bloque 4 | fina 471/600 (20 bajas), TWAY 375 (21), CIRCULAR 503 (29); antes 465, 384, 516 |
| 2026-11-16 | Desplazamiento del WBLOCK, con UnityPy | `distanceToGun` es media anchura del sprite de la nave (`nave.png`, 58 px): 5,8 px, no 11,6. Corregido a 6 |
| 2026-11-16 | Bloque 3: `make` | ✅ sin avisos |
| 2026-11-16 | Cada arma con 200 de munición forzada por RAM, 200 f, capturas | ✅ WBLOCK, LASER, TWAY, CIRCULAR y MOIRE disparan y gastan; ❌ el haz con el segmento de láser del GML no se veía (estaba en la tabla de sprites); ✅ con puntos amarillos |
| 2026-11-16 | CIRCULAR, 400 f | ❌ las bolas iban al eslabón más cercano y se quedaban pegadas (2 bajas); ✅ con arañas y cabezas (`getClosest`) |
| 2026-11-16 | Bala fina, 600 f, nave quieta | ❌ 0 bajas apuntando al eslabón más cercano; ✅ apuntado a arañas y cabezas; con la nave inmortal, 27 bajas |
| 2026-11-16 | Impactos por muerte de gusano, contador temporal en una copia (900 f, arnés) | ✅ fina 6, 6, 6, 6 (y 3, 5: gusanos tocados por la metralla); gorda 3, 3, 3, 3, 3, 3 (y 4, 4) |
| 2026-11-16 | Caso peor por arma | ver la tabla del bloque 3 |
| 2026-11-16 | Selector tras una partida: derecha cuatro veces | ✅ 0 → 1 → 2 → 5 → 6 (salta PIVOT y PARA), marquesina " MOIRE ", NEXT 80, iconos nuevos en su sitio |
| 2026-11-16 | VRAM y RAM | ✅ `nextTile` 1411 de 1440 (29 libres; los iconos se llevan 32); `.data` + `.bss` = 14 524 bytes |
| 2026-11-16 | Agujeros verdes: `make` y capturas del menú y de la partida | ✅ sin avisos; agujeros verdes, reborde rojo al salir un gusano; logo, HUD y CREDITS con sus colores de antes |
| 2026-11-16 | Movimiento del GML y fondo verde: `make` | ✅ sin avisos |
| 2026-11-16 | Nave por RAM, derecha + abajo durante 10 f | ✅ (10240, 7680) → (11160, 8600): 920/64 = 14,375 px por eje, 1,4375 px/f en cada eje también en diagonal |
| 2026-11-16 | Capturas del menú y de la partida con el fondo verde | ✅ verde oscuro, nave, agujeros, HUD y texto bien distinguibles |
| 2026-11-16 | Reversión de agujeros y aparición: `make` | ✅ sin avisos |
| 2026-11-16 | Aparición de la nave, por RAM a los 45 f de pulsar START | ✅ (10240, 7680) en fix16 = (160, 120) |
| 2026-11-16 | VRAM con el anillo | ✅ `nextTile` 1379 de 1440 (**61 libres**, antes 36); `holeLoaded` 43; 4 celdas compuestas |
| 2026-11-16 | Capturas: splash y partida con B pulsado | ✅ anillo de seis agujeros sin recortes; CREDITS compuesto encima del agujero de abajo; logo entero; reborde rojo al salir los gusanos |
| 2026-11-16 | `tools/measure.sh` | 484/600, 24 bajas (HEAD, hexágono) → **533/600, 15 bajas**. No es la misma partida (otra disposición, otras bajas), así que sólo dice que no hay un retroceso visible |
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
