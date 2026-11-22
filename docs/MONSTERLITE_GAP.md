# MonsterLite → port de Mega Drive: inventario y lo que falta

Análisis del 2026-11-15, revisado el mismo día tras la verificación (contadores del game over, balas fuera de pantalla, bloque del jugador, decisiones 3 y 4, regla de niveles, hitstop de los rayos, monedas sobre agujeros y bolas CIRCULAR). El 2026-11-15 se implementaron los bloques 1 y 2 (§5); el estado de cada bloque vive en `docs/PORT_AUDIT_GML.md`, sección "MonsterLite".

**Fuente de verdad (decisión del usuario):** MonsterLite manda en gameplay, progresión y armas. El GML queda como segunda referencia. Referencia: `~/MonsterLite/MonsterLite_ultimo_commit` (último commit `e3dac3f` "gold android+iOS freemium 1.0", julio de 2015; `bundleVersion 1.3.5`, `productName Arena`). La copia `~/MonsterLite/MonsterLite` (v1.3.3, commit `2cb9783`) sólo se ha usado para comparar.

Rutas abreviadas en este documento:

- `S/` = `~/MonsterLite/MonsterLite_ultimo_commit/Assets/Scripts/`
- `W/` = `~/MonsterLite/MonsterLite_ultimo_commit/Assets/Plugins/Soomla/Store/` (sólo `WeaponsController.cs`, `GameAssets.cs`, `CoinsManager.cs` y `EventHandler.cs`, que son lógica del juego aunque estén dentro del plugin)
- `P/` = `~/MonsterLite/MonsterLite_ultimo_commit/Assets/Prefabs/`

Un valor marcado **[prefab]** sale del inspector serializado y manda sobre el valor por defecto del código. Uno marcado **[no encontrado]** no está en el proyecto.

---

## 0. Cómo se han leído las escenas y los prefabs

Las escenas (`Assets/Scenes/MonsterLite.unity`, `Assets/logo.unity`, más `Main.unity` y `scene.unity`, que son viejas y no tienen lógica), los prefabs y `ProjectSettings/*.asset` están en **serialización binaria** de Unity (4.6.1f1 y 5.0.2p3), no en YAML. Llevan el árbol de tipos incluido, así que se pueden leer sin Unity:

- UnityPy 1.25.4, instalado en un venv temporal del scratchpad (`python3 -m venv venv && venv/bin/pip install UnityPy`). No se ha instalado nada en el repo.
- Cada `MonoBehaviour` apunta a su script por GUID. En el fichero binario el GUID va con los nibbles de cada byte intercambiados respecto al `.meta` (`698a224b…` en el binario = `96a822b4…` en `disparo.cs.meta`). Con esa conversión se resuelven todos los scripts.
- Las escenas sólo instancian prefabs. Se han revisado todas las `m_Modifications` de las instancias de `MonsterLite.unity`: tocan posición, rotación, `m_RootOrder`, `m_IsActive`, `m_Name`, estilos de GUI y referencias a anuncios. **Ningún valor de juego se sobrescribe en la escena**, así que los valores de los prefabs son los finales.
- Los valores de juego de los prefabs coinciden en las dos copias (v1.3.3 y último commit). Sólo cambian los valores de las monedas (ver §5.3).

Ajustes del proyecto que afectan a la conversión:

| Ajuste | Valor | Origen |
|---|---|---|
| `Fixed Timestep` | 0.02 s → `FixedUpdate` a **50 Hz** | `ProjectSettings/TimeManager.asset` |
| `Application.targetFrameRate` | **60** [prefab] (el código pone 30 por defecto) | `P/controller.prefab` → `globales.frameRate`; `S/globales.cs:14,74` |
| Cámara | ortográfica, `orthographic size` 9 en la escena; en dispositivo **6 en apaisado y 9 en vertical** | `MonsterLite.unity` (Main Camera); `S/globales.cs:208-236` |
| Orientación | rotación automática, las cuatro permitidas; resolución por defecto 1024×768 | `ProjectSettings/ProjectSettings.asset` |
| Píxeles por unidad de los sprites | 100 | `.meta` de `Assets/Art/*.png` (estos sí son texto) |

---

## 1. Conversión de unidades

Las reglas de `port-arena.md` (×0.2, ×0.24, ×1.2, ÷1.2) son para el GML (room de 1600×1200 a 60 fps). MonsterLite usa unidades de mundo y segundos, y dos relojes distintos. Las reglas equivalentes son estas:

**Espacio.** En apaisado 4:3 la cámara ortográfica de 6 ve 12 unidades de alto y 16 de ancho. 320×240 / 16×12 → **1 unidad = 20 px**. Un sprite de `n` px de arte a escala `s` mide `n · 0.01 · s · 20 = 0.2 · n · s` px. (Si se tomara la orientación vertical, de 9, saldrían 13,3 px/unidad: ver la pregunta Q1.)

**Tiempo.** Hay dos relojes:

| Dónde cuenta | Ritmo | A 50 Hz |
|---|---|---|
| `FixedUpdate`: movimiento del jugador y de los enemigos, disparo del jugador (`S/gameControl.cs:333-350`) | 50 Hz | **1:1**, un tick = un frame de Mega Drive |
| `Update` y `OnGUI`: temporizadores de vida, paquetes, monedas, láser, rayos, naves holo, órbita de las bolas, sacudidas | 60 Hz (`targetFrameRate`) | **÷1.2**, la misma regla de `port-arena.md` |
| `WaitForSeconds` y `Time.deltaTime` dentro de `Update` | segundos | **× 50** frames |

**Velocidades.**

| Unidad de origen | A px/frame de Mega Drive |
|---|---|
| unidades/s | × 20 / 50 = **× 0.4** |
| unidades por tick de `FixedUpdate` | × 20 |
| unidades por frame de `Update` | × 20 × 1.2 = × 24 |
| `Rigidbody.AddForce(F)` aplicado una vez (modo `Force`) | Δv = F · 0.02 / masa en unidades/s, y luego × 0.4 |
| grados por frame de `Update` | × 1.2 |
| `Quaternion.Slerp(…, k · Time.deltaTime)` en `FixedUpdate` | fracción `k · 0.02` del ángulo que falta, por frame (1:1) |

Las velocidades en `fix16` tienen precisión 1/64. Las que salen exactas (12 px/f, 4 px/f) no tienen error; las demás se indican.

---

## 2. Inventario de MonsterLite

### 2.1 Flujo de pantallas

`gameControl.State` (`S/gameControl.cs:26-37`) declara MENU, INTERLUDE, WEAPONROOM, MAP, INGAME, NEWWEAPON, GAMEOVER, STORE y TUTORIAL. **En la versión final sólo se usan MENU, INGAME, GAMEOVER y STORE:**

```
logo.unity (0,8 s) → MENU "TAP TO START" → INGAME → (muerte) → GAMEOVER ─PLAY─→ INGAME
                                                                   └─GEAR─→ STORE ─PLAY─→ INGAME
```

| Pantalla | Qué hace | Origen |
|---|---|---|
| Logo | Logo de Screen Implosion (`screenImplosionLogo_inv_Square_1800`). Espera 0,8 s (**40 f**) y carga la escena `MonsterLite`. El fundido está a medias: `fadeTime` 2 [prefab] se pone a 0 en `Start`, el fundido de entrada sube 1/s y el de salida está comentado | `S/logoScene.cs:12-53`; `logo.unity` |
| Menú | Textura `splash` dibujada tres veces (gris, negra y blanca), las dos primeras oscilando en vertical con `sin(v)`, `v += 8 · dt`. "TAP TO START" parpadea (`t % 100 < 50`, con `t` contado por llamada a `OnGUI`). Voz "ARENA" al entrar. Tocar → sacudida grande de 4 frames y partida | `S/menu.cs:23-66`; `S/gameControl.cs:214-231` |
| Partida | Ver §2.2 | `S/gameControl.cs:362-686` |
| Muerte | Cámara lenta ×0.2 durante dos esperas de 0,1 s escaladas (1 s real), nave y explosión en rojo, sacudida grande, se guarda el récord y se pasa al GAMEOVER | `S/gameControl.cs:690-725` |
| Game over | Ver §2.8 | `S/gameOverSrc.cs` |
| Tienda "GEAR" | Ver §2.9 | `S/StoreMenu.cs` |
| Tutorial | **Desactivado en la versión final**: `checkTutorial()` devuelve siempre `false`, `resetTutorial()` y `finishTutorial()` tienen el cuerpo comentado, y en la rama táctil la comprobación también está comentada. Ver §2.11 | `S/gameControl.cs:355-359, 942-986, 222-230` |
| Interludio, sala de armas, mapa, arma nueva | **Código muerto.** `InterludeScr` y `newWeaponRoomSrc` no se instancian en ningún sitio; `toWeaponRoom()` nunca se llama; `MapScr.cs` y `stMachine.cs` están enteros comentados; `buttonTriggered.cs` no está puesto en ningún prefab ni escena | `S/InterludeScr.cs`, `S/newWeaponRoomSrc.cs`, `S/weaponRoom.cs`, `S/MapScr.cs`, `S/buttonTriggered.cs` |

Lo que había en el código muerto, por si se quiere recuperar: el interludio pintaba KILLS, MAX y TIME (`stageTime`, segundos en partida) y "STAGE"; la sala de arma nueva enseñaba "NEW WEAPON UNLOCKED!!!" con el icono girando y creciendo (`v += 0.05`, giro `v·30`) e incrementaba `WeaponsController.unlockedWeapons`. Ninguna de las dos se alcanza.

### 2.2 Estructura de la partida y oleadas

**Inicio** (`toGame` + `startGame`, `S/gameControl.cs:362-455`):

1. `numberOfGames += 1`, `kills = 0`, `level = 1`, `factorLevel = 1`, `earnedCoins = 0`, `stageTime = 0`, color de fondo a (166,184,144). Pista de música al azar (§2.12).
2. Espera 0,8 s (**40 f**). Crea el jugador en `highPos` y el controlador de enemigos con el hexágono de agujeros en `posH` (§2.6).
3. Espera 1 s (**50 f**). Crea **un gusano**.
4. Bucle de oleadas: **cada 0,4 s (20 f)**, mientras la partida siga.

**El bucle de oleadas** (`S/gameControl.cs:457-685`) no depende de qué ha muerto, sino de la puntuación:

```csharp
int index = globales.kills % 40;      // línea 460
int r = Random.Range (0, index);      // línea 461: entero, extremo superior EXCLUIDO
```

Con `index` 0 o 1, `r` vale 0. El valor máximo es 38, así que los casos 39 y 40 son inalcanzables. No hay caso 15.

| `r` | Qué crea |
|---|---|
| 0, 1, 5, 7, 9 | 2 gusanos |
| 8 | 1 gusano |
| 2, 10, 17, 24, 26, 35 | 1 araña + 1 gusano |
| 11, 12 | 2 gusanos + 1 araña |
| 13, 34, 38 | 2 arañas |
| 14, 18, 22, 23, 25, 29, 32 | 2 arañas + 1 gusano |
| 28, 30, 37 | 1 araña |
| 33 | 1 araña + 2 gusanos |
| 27 | texto rojo "!!!" + 2 arañas + 1 gusano; **+2 gusanos si `level > 5`** |
| 31 | 1 araña + 2 gusanos; +2 si `level > 5` |
| 36 | 2 arañas + 2 gusanos; +2 si `level > 5` |
| 3 | bomba en posición aleatoria con probabilidad 1/6 (`Random.Range(0,6) == 1`) |
| 4 | rayos en posición aleatoria con probabilidad 1/7 |
| 6, 16, 19, 20, 21 | una nave holo en posición aleatoria si hay menos de `maxNaveProxy` = **10** [prefab] |
| 15 | nada |

**Topes** (`S/enemyController.cs`):

- Gusanos: `createSnake()` sólo crea uno si hay menos de `maxNumSnakes = (level / 10) · 3 + 2` cabezas (líneas 141-157, 215-218): **2 en los niveles 1-9, 5 en el 10-19, 8 en el 20-29**…
- Arañas: **sin tope** (`createSpider()`, líneas 122-130).
- Nacen en un agujero al azar de los 6, **sin retardo de aparición** (se instancian ya activos).

**Posiciones aleatorias** (`globales.getRandomPos`, `S/globales.cs:270-279`): viewport `Random.Range(0.1, 0.9)` en cada eje → x ∈ [32, 288], y ∈ [24, 216] px.

### 2.3 Puntuación y niveles

La variable se llama `globales.kills` pero en pantalla es **SCORE**: cada baja suma el nivel actual.

| Cómo muere | Suma | Origen |
|---|---|---|
| Araña por bala fina (`bulletThin`, la de sin munición y la de las naves holo) | **`2 · level`** | `S/bulletThinSrc.cs:49` |
| Araña por bala gorda (WBLOCK, TWAY), bola CIRCULAR, LASER o MOIRE | `level` | `S/bulletScript.cs:69`, `S/circularScr.cs:93`, `S/RaycastScr.cs:63` |
| Gusano (cabeza), con cualquier arma | `level` | `S/bulletScript.cs:87`, `S/bulletThinSrc.cs:68`, `S/circularScr.cs:115`, `S/RaycastScr.cs:82` |
| Bomba o rayos: cada enemigo en pantalla | `level` | `clearEnemies()`, `S/enemyController.cs:176` |

**Niveles** (`levelUpgrade`, `S/gameControl.cs:781-795`, comprobado en cada `Update`):

```csharp
if (kills / factorLevel > level) { level = kills / factorLevel; factorLevel *= 2; … }
```

Empieza con `level = 1`, `factorLevel = 1`. **La regla es `level = kills / factorLevel`, no "un nivel más"** (`S/gameControl.cs:786-788`): en cada comprobación el nivel pasa directamente al cociente y `factorLevel` se duplica **una sola vez**, aunque se hayan saltado varios niveles. El HUD enseña como "UPGRADE AT" el siguiente umbral, `factorLevel · (level + 1)`, ya con el factor duplicado.

Si la puntuación sube de poco en poco y cae justo en cada umbral, sale esta tabla:

| Puntuación | 2 | 6 | 16 | 40 | 96 | 224 | 512 | 1152 | 2560 | 5632 | 12288 | 26624 | 57344 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Nivel | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |

**La tabla es sólo ese camino, no la regla.** Si una baja o una bomba hace saltar la puntuación por encima de varios umbrales, el nivel salta y los umbrales siguientes cambian:

- Nivel 1 (factor 1), puntuación 0 → una bomba mata a 3 enemigos (+3) → 3 / 1 = 3: **del 1 al 3** de golpe, factor 2, siguiente umbral 2 · 4 = **8** (no 16 como en la tabla).
- Nivel 2 (factor 2), puntuación 5 → araña por bala fina (+2 · 2 = 4) → 9 / 2 = 4: **del 2 al 4**, factor 4, siguiente umbral 4 · 5 = **20**. Pasa en partida normal, sin bombas.

Así que la progresión real depende del orden de las bajas y no se puede tabular; el port tiene que aplicar la regla tal cual.

Al subir de nivel: mensaje "LEVEL n / NEXT UPGRADE: SCORE x" durante 3 s + 0,1 (`ChangeLevelWait`, líneas 768-779 → **155 f**) y el color de fondo se desvía ±10 por canal (`setCameraLevelColor`, `S/globales.cs:293-303`).

Lo que cambia con el nivel:

| Efecto | Fórmula | Origen |
|---|---|---|
| Puntos por baja | × `level` | tabla anterior |
| Velocidad de los enemigos | `f = (level % 10) + 0.8` (vuelve a empezar en el 10) | `S/enemyMov.cs:44` |
| Tope de gusanos | `(level / 10) · 3 + 2` | `S/enemyController.cs:217` |
| Re-sorteo del estado de vagar | el temporizador baja además `level % 100` por tick | `S/enemyMov.cs:104-105` |
| Oleadas 27, 31 y 36 | +2 gusanos si `level > 5` | `S/gameControl.cs:603, 626, 658` |
| Valor de las monedas | tabla mayor si `level > 10` | `S/Coin.cs:49-104` |

Por el camino de la tabla, la puntuación pasa de 65535 **dentro del nivel 14** (que empieza en 57344; el 15 empieza en 8192 · 15 = 122880). Con saltos de nivel puede pasar antes o después, pero en cualquier caso hace falta `u32`.

### 2.4 El jugador

| Aspecto | MonsterLite | A Mega Drive | Origen |
|---|---|---|---|
| Control táctil | Arrastre relativo: `acc = delta del dedo · dt · 4` (`acceleartionTouch` 4 [prefab]), desplazamiento `acc · speed` | no aplica (ver Q7) | `S/playerMovement.cs:67-121` |
| Control por teclas | flechas, `speed` = **0.2 unidades por tick** [prefab] **por eje**, sin inercia. Cada flecha suma su eje por separado y no se normaliza: **en diagonal va a 4·√2 ≈ 5,66 px/f**. Esta rama se compila en el editor **y en Android** (`#if UNITY_EDITOR \|\| UNITY_ANDROID`); el proyecto trae `Art/android_tv.png` | **4 px/f** por eje (exacto en fix16), 5,66 px/f en diagonal | `S/playerMovement.cs:35-65` |
| Wrap | toroidal en los bordes exactos de la cámara, **sin margen** | 0 px de margen | `S/playerMovement.cs:129-146` |
| Apuntado | gira al instante hacia el enemigo más cercano (`getClosest`) | igual | `S/playerMovement.cs:151-164` |
| Colisión | esfera de radio 0.18 [prefab] (el prefab tiene escala 1) | **radio 3,6 px** | `P/player.prefab` |
| Muere al tocar | araña, cabeza o **eslabón** del gusano (`arana`, `snake`, `snakeBody`) | igual | `S/playerMovement.cs:171-178` |
| Agujeros | **no matan** (no hay caso `hole`) | — | `S/playerMovement.cs:168-244` |
| Nace en | `highPos = ScreenToWorldPoint(W/2, H/1.4)`: en apaisado, 2,57 unidades por encima del centro | (160, 69) | `S/gameControl.cs:156` |
| Estela | `TrailRenderer`, 0,26 s, ancho 0.08 → 0.001 | 13 f | `P/player.prefab` |

**La congelación al soltar.** Pasados 60 ciclos de `HUD.counter` (los que dura el cartel "ARENA!"), si no hay ningún dedo en la pantalla, `Time.timeScale = 0`: el juego entero se para y aparece "KEEP YOUR FINGER ON SCREEN!" con la textura `dedo`. Al volver a tocar sigue. La música baja de tono mientras está parado (`S/gameControl.cs:255-302`; `S/HUD.cs:489-499`). Es una mecánica central del original y depende del táctil: ver Q8.

`HUD.counter` sube una vez por llamada a `OnGUI` (`S/HUD.cs:472`), y Unity llama a `OnGUI` al menos dos veces por frame (eventos Layout y Repaint). **Cuántos frames dura "ARENA!" no se puede saber sin ejecutar Unity**: entre 30 y 60 frames de 60 Hz, o sea 25-50 f.

### 2.5 Armas

`WeaponsController.WEAPONS` (`W/WeaponsController.cs:25-33`): **WBLOCK = 0, LASER = 1, TWAY = 2, CIRCULAR = 3, MOIRE = 4**. Hay una sola arma equipada (`currentWeapon`); el disparo es automático, se dispara sólo si existe un enemigo (`S/disparo.cs:41`).

Disparo (`S/disparo.cs:32-160`, llamado desde `FixedUpdate`): mientras `shootTime > 1` baja 1 por tick; cuando llega a 1, dispara y lo pone a la cadencia. **El periodo es la cadencia en ticks = frames de Mega Drive.** Valor inicial `shootTime` = 4 [prefab]. Si el arma equipada tiene munición (`bullets[currentWeapon] != 0`), dispara el arma y gasta 1; si no, dispara la bala fina.

Proyectiles físicos: `AddForce(transform.right · shotForce)` con `shotForce` = 150 [prefab] y masa 0.1 [prefab] → Δv = 150 · 0.02 / 0.1 = 30 u/s = **12 px/f**. No tienen vida ni hacen wrap, pero **se destruyen al salir de pantalla**: `Bullet.prefab` y `BulletThin.prefab` llevan en el hijo con el sprite `bulletScrKillSprite`, cuyo `OnBecameInvisible` destruye la bala entera (`S/bulletScrKillSprite.cs:8-11`). En el port: se libera cuando el sprite queda entero fuera de 320×240.

Consecuencias de vivir sólo en pantalla:

- **Vida máxima**: la diagonal de la pantalla son 400 px; a 12 px/f una bala vive como mucho ~34 f (más 1-2 f hasta que el sprite sale del todo), y desde el centro, ~17 f.
- **Balas vivas a la vez** (cadencia × vida, en el peor caso de la nave en una esquina): TWAY 3 balas cada 3 f → **≤ ~35**; WBLOCK 1 cada 4 f → ≤ 9; fina 1 cada 8 f → ≤ 5. Las de las naves holo van a 6,4 px/f (vida ≤ ~63 f) cada 33 f → ≤ 2 por nave, **≤ 20 con 10 naves**. Con la nave en el centro, la mitad. Esto dimensiona el pool de balas (§4).
- **Sacudida grande** (§2.12): cambia el tamaño de la cámara a un entero de 0 a 5, menor que el 6 normal, así que la vista se encoge y las balas que quedan fuera de esa vista reducida también se destruyen. Deducido del código, no comprobado en Unity.

| | Cadencia [prefab] | Qué dispara | Contra araña | Contra cabeza de gusano | Munición por paquete [prefab] |
|---|---|---|---|---|---|
| sin munición | `shotTimeNormal` **8 f** | `BulletThin`, 12 px/f | muere, `2·level` puntos, bala destruida | `hp -= 1`; muere al **6.º** impacto; bala destruida | — |
| WBLOCK | `shotTimewBock` **4 f** | `Bullet` (sprite `fatBullet`), 12 px/f, nace desplazada media anchura de sprite en X **del mundo** (sin rotar) | muere, bala destruida | `hp -= 3`; muere al **3.er** impacto; **la bala no se destruye** si no mata (atraviesa) | `wBlockBonus` **10** |
| TWAY | `shotTimeTWay` **3 f** | 3 × `Bullet` a −30°, 0°, +30° | como WBLOCK | como WBLOCK | `TWayBonus` **10** |
| LASER | `shotTimeLaser` **5 f** | haz visual (`Laser`, línea de 200 u × escala 0.1 = 20 u, toda la pantalla; vive `timer` 2 [prefab] frames de `Update`) + `RaycastObj` de impacto instantáneo que vive **30 frames de Update → 25 f** pegado a la nave | muere al primer contacto | **muere al primer contacto** (sin mirar hp) | `laserBonus` **4** |
| MOIRE | `shotTimeMoire` **2 f** | haz de 24 u, ancho 0.01 → 0.1, vive **8 frames de Update → 7 f** (6,67, +5 %), con su `RaycastObj` de 8 | como LASER | como LASER | `moireBonus` **3** |
| CIRCULAR | **3 f**: usa `shotTimeTWay`, no `shotTimeCircular` (4, que no se usa) | `circCont` pegado a la nave con bolas a 1 u (**20 px**) en 0°, 120°, 240° **y 360°** (el bucle es `i <= 360`: salen 4 bolas, dos superpuestas) | muere, `level` puntos, **siempre suelta paquete** y el número volador; la bola se destruye | `hp -= 3`; bola destruida | `circularBonus` **4** |

Detalle de cada una:

- **Impacto instantáneo (LASER, MOIRE)** (`S/RaycastScr.cs:33-87`): cada frame de `Update` lanza `Physics.Raycast` desde la nave en la dirección de apuntado, **sin distancia máxima** (el campo `length` no limita nada). Mata **sólo al primer** collider que encuentra si es araña o cabeza, y entonces se destruye. Si el primero es otra cosa, no hace nada ese frame. Como el proyecto tiene `m_RaycastsHitTriggers: true` (`ProjectSettings/DynamicsManager.asset`), el rayo choca también con los triggers, así que tapan el láser: **los eslabones del gusano**, los agujeros, los paquetes, las monedas, las bombas, los rayos, las naves holo y **las balas en vuelo** (las finas que quedaran del disparo sin munición y las de las naves holo). Como las balas se destruyen al salir de pantalla, una bala sólo tapa mientras está en pantalla (≤ ~34 f). Como va pegado a la nave y gira con ella, durante su vida mata al primer enemigo que se cruce en la línea. Suelta moneda o paquete como las balas.
- **Bolas (CIRCULAR)** (`S/circularScr.cs:44-68`, `S/circCont.cs:23-37`): cada bola tiene un `release` que empieza en `false` (línea 24). En cada frame de `Update`, si queda munición del arma equipada, `release` pasa a `true` (49) y **ya no vuelve a `false` nunca**. Con `release` a `true`, la bola **persigue al enemigo más cercano** a 1 u por frame de `Update` (**24 px/f**, prácticamente instantáneo) y sacude la pantalla cada frame; con `release` a `false`, **orbita la nave** a 2° por frame de `Update` (**2,4°/f**) (53). El disparo gasta la munición justo después de crear las bolas (`S/disparo.cs:127-132`), así que en su primer `Update` sólo ven la munición a 0 las bolas **del disparo que gastó la última**: son las únicas que orbitan. Una bola que ya se soltó sigue persiguiendo aunque la munición llegue a 0, y una que orbita se suelta en cuanto llega munición y no vuelve a la órbita. Como mucho hay **4 bolas orbitando** a la vez. Esfera de radio 0.2 → **4 px**. Si no hay enemigos, las sueltas se destruyen.
- **Balas gordas y finas** (`S/bulletScript.cs:59-111`, `S/bulletThinSrc.cs:42-88`): caja de 0.8 u → **16×16 px** [prefab]. Sólo reaccionan a `arana` y `snake`: **los eslabones (`snakeBody`) no les afectan**, las balas los atraviesan. Cada impacto crea una explosión. Cada baja por bala congela el juego 30 ms (`Thread.Sleep(milisecsEnemyDestroyed)`, valor en `S/globales.cs:81`; llamadas en `S/bulletScript.cs:62, 80` y `S/bulletThinSrc.cs:45, 65`): ~1,5 frames. LASER, MOIRE y CIRCULAR no lo hacen.
- **Sacudida pequeña por disparo** (`shakeSmall`, `S/cameraScript.cs:47-76`): desplazamiento aleatorio de ±0,2 u (**±4 px**) durante `t` frames, con `t` = 1 (WBLOCK, MOIRE), 0,5 (LASER, o sea 1 frame), 0,3 (TWAY, 1 frame); CIRCULAR, 3 por frame mientras persigue.

**Munición** (`W/WeaponsController.cs:7`, `S/paqueteScr.cs:28-81`): un contador por arma (`bullets[5]`). El paquete da munición **del arma equipada en el momento en que aparece** (`bulletType = getWeaponType()` en `Start`). **No se reinicia nunca al empezar partida** (ni `toGame` ni `startGame` la tocan): la munición sobrante pasa a la partida siguiente mientras la aplicación esté abierta. Tampoco se guarda: se pierde al cerrar. Los campos `triBonus` 10, `bombaBonus` 4 y `rayoBonus` 12 [prefab] existen pero no se usan.

**Selección y desbloqueo**: ver §2.9. WBLOCK está siempre comprada (`weaponPurchased[0] = true`). En una instalación nueva el arma equipada es WBLOCK (valor por defecto del enum).

### 2.6 Enemigos y agujeros

**Movimiento común** (`S/enemyMov.cs`, `move()` desde `FixedUpdate`):

- Al nacer: rotación aleatoria 0-360°, destino aleatorio, estado al azar entre ATTACK y WANDER (`Start`, 32-39).
- `setSpeed` (41-53): `f = (level % 10) + 0.8`; `speed = Random.Range(f, maxSpeed + f/10)` en u/s; `turningSpeed = Random.Range(1, turningSpeedMax / 1.4)`. (`Random.Range` con mínimo mayor que máximo devuelve un valor entre los dos.)
- Cada tick avanza `speed · 0.02` en su dirección (`Translate(right)`) y gira con `Slerp(actual, hacia el destino, turningSpeed · 0.02)`: **una fracción del ángulo que falta**, no grados fijos.
- ATTACK: destino = jugador; si no hay jugador, una nave holo. WANDER: destino = punto aleatorio y el temporizador baja `level % 100` más por tick.
- `Update` (55-69, 60 Hz): `timer -= 1`; al pasar de 0 re-sortea estado y destino y lo pone a 30. Empieza en 200 [prefab].
- No hacen wrap.

| | Araña (`aranaPrefab`) | Gusano (`culobicho`) |
|---|---|---|
| `hp` [prefab] | 10, **no se usa**: las balas la matan sin mirarlo | **4** (`decreaseHP` −1, `decreaseDoubleHP` −3; muere cuando `hp < 0` al recibir otro impacto) |
| `maxSpeed` [prefab] | 1 | 4 |
| Velocidad en nivel 1 (f = 1.8) | 1,18-1,8 u/s → **0,472-0,72 px/f** | 1,8-4,18 u/s → **0,72-1,672 px/f** |
| Velocidad en nivel 9 (f = 9.8) | 1,98-9,8 u/s → 0,79-3,92 px/f | 4,98-9,8 u/s → 1,99-3,92 px/f |
| `turningSpeedMax` [prefab] | 8 → fracción por frame 0,02-0,114 | 10 → 0,02-0,143 |
| `timer` [prefab] | 200 → **167 f**; después 30 → **25 f** | igual |
| Colisión | esfera 0.28 × escala 1.5 = 0.42 u → **radio 8,4 px** | cabeza 0.32 × 1.5 = 0.48 u → **radio 9,6 px** |
| Cuerpo | — | **16 eslabones** (`mybodyLenght` [prefab]) de `culobicho2`, radio 9,6 px, que matan al tocar al jugador y que las balas atraviesan |
| Seguimiento | — | "migas": cada vez que la cabeza se aleja `segmentSpacing` = 0.4 u (**8 px**) [prefab] de la última, se añade una y cada eslabón salta a la suya (interpolación a 0, sin suavizado) (`S/headScr.cs:25-64`) |
| Al morir | explosión | explosión en cada eslabón (`destroyParts`) |

`culobichoScr` y `cabezaScr` existen pero no hacen nada en la versión final.

**Agujeros** (`P/AgujerosPadre.prefab`): **6**, en posiciones locales (0, 2), (2, −1), (−2, 1), (0, −2), (2, 1), (−2, −1) u, escala 1.1, esfera de 0.8 → **radio 17,6 px**. En px desde su centro: (0, ±40), (±40, ±20). El grupo se coloca en `posH = ScreenToWorldPoint(W/2, H − H/1.4)`: en apaisado, **2,57 u (51 px) por debajo del centro**, o sea el hexágono centrado en (160, 171) px (`S/gameControl.cs:167-169`, `S/globales.cs:241-258`). En el editor se queda en el centro. No matan al jugador.

**Moneda que nace encima de un agujero o de otra moneda** (`S/Coin.cs:123-140`): `OnTriggerEnter` llama a `animOffset()`, que hace `Translate(0, i, 0)` para i = 0…9, es decir, 0 + 1 + … + 9 = **45 u = 900 px** hacia arriba (las monedas nacen con `Quaternion.identity`, así que su Y local es la del mundo). La moneda sale de la pantalla, no hace wrap y **se pierde**: no se puede recoger y caduca a los 167 f. Si choca con otra moneda, las dos se van. La comprobación busca además el tag `"paguete"` (línea 127), con errata: el tag del proyecto es `"paquete"` (`ProjectSettings/TagManager.asset`), así que **los paquetes no la activan**. No he comprobado si una moneda recogida, que vuela hacia abajo por encima del hexágono de agujeros, también salta al cruzarlos.

### 2.7 Lo que hay en el suelo

**Al morir un enemigo por disparo** (`leavePow`, en las balas, el impacto instantáneo y las bolas): **50 % moneda, 50 % paquete** (`Random.Range(0,2)`). Excepción: araña muerta por bola CIRCULAR → siempre paquete. Las muertes por bomba o rayos no sueltan nada.

| Objeto | Valores | A Mega Drive | Origen |
|---|---|---|---|
| Paquete (`Paquete`) | vida `lifeTime` **200** frames de `Update` [prefab]; al recogerlo sube `aumento` 0.1 u/frame y la vida baja 30 por frame; caja 0.5 × escala 2 = 1 u | **167 f** (3,3 s); sube 2,4 px/f; caja **20×20 px** | `S/paqueteScr.cs:37-52`; `P/PowUps/Paquete.prefab` |
| Moneda (`Coin`) | vida **200** [prefab], caja 1 u; al recogerla vuela hacia el centro de abajo a 8 u/s creciendo ×1.1 por frame hasta escala 2 | **167 f**; 3,2 px/f; caja **20×20 px**; arte `coinAnimation.png`, 6 frames de 32×32 a escala 2 → **~13 px** | `S/Coin.cs`; `S/playerMovement.cs:246-257`; `P/PowUps/Coin.prefab` |
| Bomba "INSTANT BOMB" (`Bomba`) | aparece por la oleada 3 (1/6); **no caduca**. Al tocarla: congela 100 ms, `clearEnemies()` (todos los enemigos explotan y suman `level` cada uno), salpicadura, sacudida grande de 1 frame, voces "BOOM" y "EXPLOSIONS" | caja 0.7 u → **14 px**; congelación ~5 f | `S/playerMovement.cs:199-210`; `S/triggerSplashBomb.cs:16-20` |
| Rayos "DELAYED RAYS" (`RayosBomba`) | aparece por la oleada 4 (1/7); no caduca. Al tocarla: un rayo (línea de 9 puntos con ruido ±0,4 u y color aleatorio) hacia **cada** araña y cabeza; al tocarla **congela 100 ms** (`Thread.Sleep(100)`, igual que la bomba); a los `timerRays` **30** frames de `Update` [prefab] cada rayo llama a `clearEnemies()` (mata **todo** lo que haya entonces, también lo que nació después) y suma `level` por enemigo; sacudida pequeña de 10 frames; voz "RAYS" | caja 1 u → 20 px; congelación ~5 f; **25 f**; ruido ±8 px | `S/playerMovement.cs:212-222` (`Sleep` en la 213); `S/triggerSplashBomb.cs:23-45`; `S/RayoScr.cs:26-60` |
| Nave holo "HOLO SHIPS" (`naveProxy`) | oleadas 6, 16, 19, 20, 21, tope 10 [prefab]. Inactiva: gira 1° por frame de `Update`. Al tocarla el jugador se activa: apunta al enemigo más cercano y dispara `BulletThin` con `shotForce` 80 [prefab] (16 u/s) cada `shotTimeNormal` **40** frames de `Update` (primer disparo a 20), **sin gastar munición** y no mientras el juego está congelado. Muere al tocarla una araña o una cabeza (explosión azul), **activa o no**: `collideAndDie` sólo se habilita al activarla, pero Unity manda `OnTriggerEnter` también a los componentes desactivados (port: bloque 8, decisión del usuario del 2026-11-20). Se dibuja una línea azul entre el jugador y cada nave activa. Los enemigos en ATTACK sólo van a por ella si no hay jugador | inactiva 1,2°/f; bala **6,4 px/f** (410/64, +0,1 %); cadencia **33 f** (33,3), primera a 17 f; radio 0.18 × 1.5 → **5,4 px** | `S/naveProxyController.cs`, `S/disparoProxy.cs`, `S/movementProxy.cs`, `S/collideAndDie.cs`, `S/gizmosProxy.cs`, `S/LineScr.cs`; `P/PowUps/naveProxy.prefab` |

`clearPowerUps()` (`S/gameControl.cs:841-865`) existe pero no se llama: lo que hay en el suelo sobrevive a la muerte y a la partida siguiente (bombas, rayos y naves holo; paquetes y monedas caducan solos).

### 2.8 HUD, game over y puntuación en pantalla

**HUD de partida** (`S/HUD.cs`; entra deslizándose desde arriba a 1000 px/s de pantalla y sale a 300):

| Elemento | Texto | Origen |
|---|---|---|
| Arriba a la izquierda | `BULLETS<n>` (munición del arma equipada), debajo el icono del arma y su nombre (`WBLOCK`, `LASER`, `TWAY`, `CIRCULAR`, `MOIRE`) | 285-314 |
| Arriba a la derecha | `<n>SCORE`, debajo `<best>BEST` (récord en amarillo), `LEVEL <n>`, `UPGRADE AT: <factorLevel·(level+1)>` | 323-363 |
| Abajo en el centro | `COINS <saldo>` (saldo total guardado, no el de la partida), centrado en W/2 + W/36; al recoger, 10 llamadas de `OnGUI` (4 f) con un rectángulo 2,5 veces más alto y más abajo: el texto, centrado en él, se va casi entero por debajo de la pantalla (no "se agranda": la fuente es la misma) | 373-398 |
| Centro | "ARENA!" al empezar; "LEVEL n / NEXT UPGRADE: SCORE x!" al subir; "!!!" en rojo en la oleada 27, 31 frames de `Update` (26 f: `oleadaT` empieza en 30 y se borra al pasar de 0) y, por el estilo centrado, en realidad en la esquina de abajo a la derecha, casi fuera de la pantalla (ver Q22); "KEEP YOUR FINGER ON SCREEN!" al congelarse | 236-246, 468-507 |

La leyenda de iconos (COINS, AMMUNITION, DELAYED RAYS, INSTANT BOMB, HOLO SHIPS, `S/HUD.cs:91-103`) se prepara pero **no se dibuja** en ningún sitio.

**Game over** (`S/gameOverSrc.cs`): entra deslizándose a 1600 px/s.

**El ritmo de los contadores.** `gameOverSrc.Update()` lanza una corrutina `Show` **nueva en cada frame** (`StartCoroutine("Show")`, 85-88) y no para las anteriores. Cada corrutina viva hace, por su cuenta:

- mientras `counterK < kills`: `counterK += 1`, un "ding", y espera 0,001 s (= al frame siguiente) (126-136);
- después, mientras `counterC < earnedCoins`: `counterC += 1` y espera 0,4 s (24 frames de `Update`) (138-141);
- nada de eso si la cámara está sacudiéndose (`!globales.shaking`, 125).

En el frame t de la pantalla hay t corrutinas vivas, así que la puntuación sube t puntos en ese frame: **`counterK` ≈ t²/2**. Las monedas suben 1 por frame (la corrutina recién creada) más 1 cada 24 frames por cada corrutina que ya esté en ese bucle. Simulado con ese modelo (frames de `Update` a 60 Hz → f de Mega Drive):

| Puntuación / monedas | SCORE termina | COINS termina | "dings" en el último frame |
|---|---|---|---|
| 100 / 60 | 14 (12 f) | 42 (35 f) | 13 |
| 1000 / 300 | 45 (38 f) | 117 (98 f) | 44 |
| 5000 / 500 | 100 (83 f) | 173 (144 f) | 99 |
| 20000 / 3000 | 200 (167 f) | 417 (348 f) | 199 |

O sea: el contador acelera, 5000 puntos se cuentan en menos de 2 s, las monedas en pocos segundos, y **el "ding" suena tantas veces por frame como corrutinas haya** (`SoundManager.playDing`, superpuestos). "CARALLUDO" suena una vez, cuando `counterK` pasa por `kills − 1` (129).

- Si la puntuación es > 10, el fondo se pone rojo puro al entrar (54-59) y cada corrutina nueva lo acerca a su color con `Lerp(…, Time.deltaTime · counterK)` (111-119). Como `counterK` crece tan rápido, el factor llega a 1 en cuanto `counterK` ≥ 60 (unos 11 frames): **es un fogonazo rojo de ~0,2 s**, no un retorno lento. Con puntuaciones de 11 a 59 el factor se queda en `kills/60` y vuelve más despacio.
- `SCORE:` muestra `counterK`; `COINS COLLECTED:` muestra `counterC`, que llega a las monedas ganadas en la partida (`CoinsManager.earnedCoins`, la suma de valores, no el número de monedas).
- `LEVEL: n!`, `NEW RECORD!` con voz "AWESOME" si se batió (257-264), `BEST SCORE:` (214).
- `LAST SCORE:` se calcula con `counterL`, que **nunca sube**: siempre 0 (213). Y ni siquiera se dibuja.
- Botones: `◊` (leaderboard: se descarta), **PLAY** (sólo cuando el contador de SCORE ha terminado), **GEAR** (tienda) (342-393). En la versión gratuita, además, "REMOVE ADS AND GET 1000 COINS" (se descarta).

### 2.9 Progresión: monedas, tienda y desbloqueo de armas

**El desbloqueo es por compra con monedas**, no por puntuación. Precios (`W/GameAssets.cs:133-155`, `PurchaseWithVirtualItem("currency_coin", n)`):

| Arma | Nombre en tienda | Descripción en tienda | Precio |
|---|---|---|---|
| WBLOCK | — | — | gratis, siempre comprada (`W/WeaponsController.cs:86, 120`) |
| LASER | Laser | "fries it all" | **6000** |
| TWAY | Tway | "Shoots in 3 ways" | **12000** |
| CIRCULAR | Circles | "Intelligent energy balls" | **24000** |
| MOIRE | Moire | "High frecuency laser" | **48000** |

**Monedas que se ganan jugando** (`S/Coin.cs:13-104`). El valor se elige al aparecer:

| Nivel | Valores posibles (equiprobables) |
|---|---|
| 1-10 | 50, 60 |
| 11+ | 50, 60, 150, 180, 100 |

(En v1.3.3 eran 60, 80 / 60, 80, 180, 200, 100.) Los valores 180-320 de `COIN_40` a `COIN_100` están en el enum pero comentados en el `switch`.

**Monedas que venían de fuera del juego** (todas se descartan): packs de pago de 2500, 10000, 17500, 40000 y 100000 monedas por 0,89 a 17,99 (`W/GameAssets.cs:85-128`); vídeo "EARN COINS! WATCH FULL VIDEO!" que daba **420** (`S/StoreMenu.cs:405-418`); "REMOVE ADS AND GET 1000 COINS" por 0,89, aunque el código que daba las 1000 está comentado (`W/EventHandler.cs:91-93`). Ver Q14.

**Tienda "GEAR"** (`S/StoreMenu.cs:171-618`), en la versión gold (sin anuncios):

- Título "GEAR".
- Una fila por arma de pago con su nombre y descripción, y un botón con el precio (`<n> COINS`) o "AVAILABLE!" si ya está comprada (278-316). Comprar descuenta el saldo y **equipa el arma** (`W/EventHandler.cs:108-125`, `setMeOnly`). La línea `currentWeapon = (WEAPONS)i` del botón usa el índice sin el +1 (equiparía el arma anterior), pero el evento de compra lo corrige justo después.
- "YOUR COINS: <saldo>" (402, sólo en la gratuita; ver Q14).
- Estante de las 5 armas: icono conmutable si está comprada, "LOCKED" si no. Sólo puede haber una activa (`setMeOnly`) (462-488). "SELECT YOUR GEAR: <arma>" (496-503).
- Iconos de BOMBA y RAYOS, decorativos (512-525).
- Interruptores de sonido y música (`musicToggle`, `AudioToggle` en `P/States/StoreRoom.prefab`; `S/globales.cs:457-494`).
- Botones PLAY, `◊` (leaderboard, se descarta) y GEAR (no hace nada).

### 2.10 Qué se guarda

`S/globales.cs:380-440` (PlayerPrefs) y el inventario de Soomla (persistente):

| Dato | Dónde | Cuándo se guarda |
|---|---|---|
| Récord (`savedMaxKills`) | PlayerPrefs | al entrar en game over (`toGameOverRoom` → `setData`, `S/gameControl.cs:906-914`) |
| Arma equipada (`currentWeapon`) | PlayerPrefs | ídem |
| Partidas jugadas (`numberOfGames`) | PlayerPrefs | ídem (sólo se usaba para los anuncios) |
| Sonido y música apagados | PlayerPrefs | ídem |
| Saldo de monedas | Soomla | al recoger cada moneda (`W/CoinsManager.cs:28-36`) |
| Armas compradas, versión gold | Soomla | al comprar |
| **Munición** | **no se guarda** | — |
| Nivel, puntuación de la partida | no | — |

### 2.11 Logros y tutorial

**Logros: MonsterLite no define ninguno.** `GameCenterSingleton.cs:76-96` tiene funciones genéricas de progreso de logros que nadie llama, y el único identificador de Play Games del proyecto es el del leaderboard (`CgkI44DYkKITEAIQAA`). La lista de logros hay que diseñarla (Q18).

**Tutorial: desactivado en la versión final** (§2.1). Lo que queda de él:

- `runTutorial` (`S/gameControl.cs:747-766`): 4 arañas y 1 gusano desde un único agujero; acaba cuando no queda ninguno (`checkRemoveTutorial`, 1028-1037); si te tocan, "AVOID BUGS!" y vuelve a empezar (`failingTutorial`, 1015-1024).
- Textos: "SWIPE GENTLY / DODGE BUGS" (`S/HUD.cs:417`), "AVOID BUGS!" (444), "CONTROL YOUR SHIP HOLDING YOUR FINGER ON SCREEN" (`S/Tutorial.cs:23`), "I AM THE SHIP!" en un bocadillo junto a la nave (`S/HUD.cs`, comentado; `S/tutorialController.cs`), y una tarjeta final comentada: "AVOID THE HOLES. / CONTROL THE SHIP BY HOLDING YOUR FINGER ON SCREEN. / THE SHIP SHOOTS AUTOMATICALLY THE CLOSEST TARGET. / COLLECT AMMO. / COLLECT COINS / GOOD LUCK! / TAP TO CONTINUE" (`S/HUD.cs:458-459`).
- Contradicción: la tarjeta dice "AVOID THE HOLES", pero en la versión final los agujeros no matan.

### 2.12 Sonido y efectos

- **Música**: 5 pistas (`track_1` a `track_5`, módulos `.xm` en `Assets/Sound/`), una al azar en cada partida (`S/songsBatch.cs:45-50`). Al congelarse baja de tono 0,1 por frame; al morir baja de volumen. **El port no la tiene en partida** (Q21 revocada, 2026-11-21).
- **Efectos** (`P/SoundManager.prefab`, biblioteca de `S/SoundManager.cs`): disparo normal, WBLOCK, láser (también para MOIRE), TWAY, CIRCULAR, explosiones (polvo, bomba, bomba pequeña, bomba amarilla), recoger paquete o moneda, interferencia, botones corto y largo, "ding". Voces: risa, bolas, tres vías, láser, repetea (al activar una nave holo), rayos, fantasma, bomba, boom, caralludo, crispado, y las de mujer "ARENA", "RAYS", "EXPLOSIONS", "AWESOME". `circularSpeechSnd` está vacío.
  - **Corrección (bloque 12):** la biblioteca que vale es la del `SoundManager` de la **escena** (`MonsterLite.unity`); la del prefab es vieja y no tiene las voces de mujer ni el "ding". "CARALLUDO" es `Fafarre.aiff`, una fanfarria. **Sólo tienen llamada** los disparos, `bombaXPL` (impacto de bala y `clearEnemies`), bomba amarilla, recoger, boom, repetea, las cuatro de mujer, caralludo, ding y los dos botones. Risa, crispado y tres vías están comentadas; fantasma, bolas, láser hablado, bomba hablada, interferencia, polvo y bomba pequeña no las llama nadie. La tabla completa, con archivo y línea, en `docs/PORT_AUDIT_GML.md`, bloque 12.
- **Sacudida grande** (`shake`, `S/cameraScript.cs:15-45`): durante `t` frames cambia el tamaño de la cámara a un entero aleatorio entre 0 y 5 (**zoom**) y la desplaza ±4 u en X y ±3 u en Y (**±80 / ±60 px**). Pequeña: §2.5.
- **Estela de la nave**: `TrailRenderer` del hijo "nave" de `P/player.prefab`, 0,26 s (13 f), de 1,6 px de ancho a nada, blanca con alfa 0,44-0,94, un vértice cada 2 px.
- **Cámara lenta de la muerte**: `timeScale` 0,2 durante 1 s real (§2.1).
- **Fondo**: color de cámara (124,181,138) en la escena y (166,184,144) al empezar partida, más un fondo de tiles (`BackgroundScript`) y polvo (`backgroundDetails`).

---

## 3. Tabla de diferencias con el port actual

Estado: **falta** (no existe en el port), **distinto** (existe con otro comportamiento o valores), **igual**.

| Sistema | MonsterLite | Port actual | Estado | Adaptación a Mega Drive |
|---|---|---|---|---|
| Flujo de pantallas | logo → menú "TAP TO START" → partida → game over → (GEAR) tienda → partida | splash con créditos de recreativa → partida → interludio KILLS/TOP/NEXT con selector de arma (`src/menu.c`, `inc/game.h:21-26`) | distinto | START empieza. Game over y tienda con el pad (Q13) |
| Créditos de recreativa | no existen | START mete crédito, A/B/C empieza (`g_credits`) | distinto | quitarlos requiere autorización (Q13) |
| Logo de empresa | escena de 0,8 s con el logo de Screen Implosion | splash con logo del GML y firma SCIM | distinto | pantalla de 40 f |
| Inicio de partida | espera 40 f, nave y agujeros; 50 f más y 1 gusano; luego oleadas | **bloque 1**: nave a los 40 f, gusano y primera oleada a los 90 f (`startSequence`, `src/game.c`); **bloque 2**: la nave nacía en (160, 17), respecto al hexágono centrado (Q6); **desde el 2026-11-16 nace en el centro, (160, 120)**, como en el GML (decisión del usuario) | igual | contadores de frames |
| Oleadas | temporizador de 20 f, tabla por `kills % 40` (§2.2) | **bloque 1**: tabla `mlWaves` cada 20 f; **bloque 7**: las de power-ups | igual | tabla en ROM |
| Niveles | `level`/`factorLevel` (§2.3) | **bloque 1**: `Game_levelUpgrade()` | igual | aritmética entera |
| Puntuación | `kills += level` (×2 araña por bala fina) | **bloque 1**: `g_score` `u32`, `+level` por baja, sin el ×2 (Q5) | divergencia deliberada | `u32` |
| Tope de gusanos | 2 / 5 / 8 según nivel | **bloque 1**: `min((level/10)·3 + 2, 3)` | recortado (medido) | 4 y 5 gusanos medidos: no caben ni en CPU ni en VRAM (Q3) |
| Tope de arañas | ninguno | 8 (`MAX_SPIDERS`) | distinto | mantener un tope (Q4) |
| Tipos de gusano | 1: cabeza + 16 eslabones a 8 px, hp 4, velocidad por nivel | 2 (GML): 13 y 11 partes (cabeza + 12 y 10), hp 4 y 2; **bloque 8**: los dos con hp 4, velocidad, IA y migas cada 8 px de MonsterLite | divergencia deliberada (longitud, Q2) | migas en vez de `stepdelay`, por decisión del usuario (2026-11-20) |
| Eslabones | matan al tocar; **las balas los atraviesan**; tapan el láser | matan al tocar; **bloque 3**: las balas de MonsterLite los atraviesan y tapan el láser; siguen siendo escudo contra las armas del GML. **Bloque 8**: esferas de 9,6 px | igual (+ armas del GML) | — |
| IA de enemigo | ATTACK/WANDER al azar, re-sorteo a los 167 f y luego cada 25 f, giro con Slerp (fracción), velocidad por nivel | **bloque 8**: igual; re-sorteo a 167 f y cada 26 f, temporizador en quintos, Slerp como fracción Q12 por frame (un `muls.w`), sigue con el tiempo parado | igual | — |
| Aparición de enemigos | inmediata | **bloque 8**: inmediata | igual | — |
| Agujeros | 6 en hexágono centrado en (160,171) px, radio 17,6 px, no matan; arañas y gusanos nacen en uno al azar | **bloque 2**: los 6 del prefab, en hexágono centrado en (160, 120) por decisión del usuario (Q6), sin efecto sobre el jugador; arañas y gusanos nacen en uno al azar; el radio de 17,6 px no se usa todavía (la IA del GML usa su máscara); **Revertido el 2026-11-16** (decisión del usuario, sustituye a Q6): vuelve el anillo de 6 del port (los 5 del room del GML + el reconstruido) y la nave nace en (160, 120); se conservan las celdas con dibujo y el texto compuesto; 4 celdas compuestas, 61 tiles libres. **Bloque 8**: la IA del GML (vagar hacia los agujeros y saltar al 0) desaparece; los agujeros sólo sirven para nacer, el reborde rojo y el rayo (17,6 px) | divergencia deliberada (centrado) | sólo se suben y se pintan las celdas con dibujo; el logo, CREDITS y HOLD B se componen encima sin cortarlos; 36 tiles libres (ver la auditoría) |
| Velocidad del jugador | teclas 4 px/f por eje, 5,66 px/f en diagonal (táctil: arrastre relativo) | **bloque 2**: 4 px/f en las ocho direcciones (Q7); **desde el 2026-11-16, la del GML** (decisión del usuario, sustituye a Q7): 1,44 px/f por eje, sin normalizar | divergencia deliberada (diagonal) | constantes en fix16 |
| Wrap del jugador | bordes exactos | **bloque 2**: margen 0; **desde el 2026-11-16, margen de 3 px** (`offSet = 15` del GML) | igual | — |
| Congelación al soltar | `timeScale 0` sin dedo en pantalla | **bloque 2**: sin B pulsado (pasados 25 f) se para todo menos el HUD, con el aviso "HOLD B"; no se congela con el panel de `GFXDEBUG=1` abierto ni en el arnés de medida | divergencia deliberada (B en vez del dedo, Q8) | lo que en MonsterLite sigue en `Update` (caducidad, LASER y MOIRE, bolas, rayos, IA) tiene que quedar fuera del corte cuando llegue con su bloque |
| Láser congela al jugador | no (`isLaserState` nunca se activa) | **no**: el GML sí lo hace (`oPlayer/Step_0.gml:2, 88`), pero el port nunca lo implementó: `Player_update` se llama siempre | igual | documentado en la auditoría ("Mecánicas del original" y bloque 2) |
| Disparo automático al más cercano | sí, sólo si hay enemigo | sí | igual | — |
| Bala por defecto | fina, 12 px/f, cadencia 8 f | **bloque 3**: fina, 12 px/f, cadencia 8 f | igual | 12 px/f exacto en fix16 |
| Juego de armas | WBLOCK, LASER, TWAY, CIRCULAR, MOIRE (§2.5) | **bloque 3**: WBLOCK, LASER, TWAY, CIRCULAR y MOIRE de MonsterLite; PIVOT y PARA del GML (por récord) | igual + PIVOT y PARA | LASER y MOIRE pasan a impacto instantáneo; CIRCULAR se parece al PIVOT (bolas) pero con otras reglas; PARA, PIVOT, BOMBA y RAY no existen como armas en MonsterLite (Q9) |
| Daños | fina −1, gorda/bola −3 (atraviesa si no mata), láser mata | **bloque 3**: fina −1, gorda/bola −3 (la gorda atraviesa si no mata), rayo mata; PIVOT, PARA y metralla con el GML | igual | — |
| Munición | por arma, sólo del arma equipada; **no se reinicia entre partidas** | **bloque 3**: por arma (`g_ammo`), se conserva hasta apagar (Q10) | igual | Q10 |
| Munición por paquete | 10 / 4 / 10 / 4 / 3 | **bloque 3**: 10 / 4 / 10 / 4 / 3; PIVOT 5 y PARA 8 (GML) | igual | — |
| Paquetes | 50 % por baja, caducan a los 167 f | **bloque 4**: 50 % por baja con disparo, caducan a los 167 f, suben al recogerlos | igual | — |
| Monedas | 50 % por baja, valor por nivel, saldo persistente; se pierden si nacen sobre un agujero u otra moneda | **bloque 4**: 50 % por baja, valor por nivel, saldo `g_coins` (hasta apagar; sin SRAM, decisión del usuario del 2026-11-20), COINS en el HUD; no se pierden (Q12) | igual | sprite nuevo (§4); el GML no tiene arte de moneda (Q11) |
| Bomba | power-up de oleada; mata **todo** y puntúa | **bloque 7**: oleada 3, mata todo y suma `level` por enemigo, congela 5 f | igual (sacudida y voces del GML hasta el bloque 12) | reutiliza el arte |
| Rayos | power-up de oleada; a los 25 f mata todo y puntúa | **bloque 7**: oleada 4, un rayo a cada enemigo, a los 26 f (25,8) mata todo y puntúa, congela 5 f | igual (3 puntos por rayo) | reutiliza el arte; la línea del rayo con puntos (`dotSprite`) |
| Naves holo | hasta 10, se activan al tocarlas, cadencia 33 f sin munición, mueren al contacto | **bloque 7**: oleadas 6, 16, 19-21, hasta 10, valores de MonsterLite, arte de la réplica del GML | igual | 10 activas con la arena llena: 314/600 en el arnés (medido, Q3) |
| HUD | BULLETS + icono y nombre del arma; SCORE, BEST, LEVEL, UPGRADE AT; COINS | **bloque 9**: la línea `BULLETS<n> <n>SCORE` con los glifos grandes del GML (C y R dibujadas en su estilo); icono del arma a 16×16 y su nombre, BEST (cifras en amarillo), LEVEL, UPGRADE AT y COINS (en amarillo) con la fuente del sistema (Q16); COINS abajo con el alto que se ve (fila 28 en PAL, 26 en NTSC); el grupo de arriba entra y sale deslizándose y todas las fuentes llevan sombra (decisión del usuario); sólo se escribe lo que cambia, por la cola de DMA | igual, con divergencias menores (deslizamiento a escalones de 8 px, SCORE hasta 999 999, Q23) | ver la auditoría, "Bloque 9" |
| Mensajes de partida | ARENA!, LEVEL n / NEXT UPGRADE, !!!, KEEP YOUR FINGER | **bloque 9**: "ARENA!" (25 f), "LEVEL n / NEXT UPGRADE: SCORE x!" (155 f de tiempo corriendo), " !!!" en rojo (26 f) y "HOLD A/B/C" (Q8), con la fuente del sistema y compuestos encima de los agujeros sin cortarlos | igual; "!!!" centrado en la fila 12 en vez de en la esquina (Q22, decisión del usuario) | reserva de 32 tiles para las letras encima de los agujeros |
| Game over | cuenta SCORE y COINS COLLECTED con ritmo que acelera (§2.8), LEVEL, NEW RECORD!, BEST SCORE, PLAY/GEAR | interludio KILLS/TOP/NEXT | distinto | ritmo y "ding" (Q15) |
| Tienda | comprar armas con monedas, elegir una, sonido/música | no existe | falta | pantalla nueva con la fuente del sistema |
| Desbloqueo | compra: 6000 / 12000 / 24000 / 48000 monedas | récord de kills > 40 / 60 / 80 / 100 (`src/game.c:110-122`) | distinto | Q14, Q9 |
| Guardado | récord, arma, partidas, sonido, música, monedas, armas compradas | **sin guardado, por decisión del usuario (2026-11-20)**: récord, saldo, armas compradas, arma equipada, sonido y música viven en RAM hasta apagar, y al encender arrancan como una partida nueva; la cabecera no declara SRAM (`src/rom_header.c`). El bloque 6 llegó a guardarlos en SRAM y se retiró | divergencia decidida | — |
| Logros | ninguno | ninguno | **bloque 10**: 17 logros inventados (Q18) con recompensa en monedas (14 950 en total), aviso en la fila 26 y lista desde la tienda ("ACHIEVEMENTS n/17"); en RAM hasta apagar o RESET | diseño nuevo (invención) | — |
| Tutorial | desactivado | no existe | falta | Q19 |
| Hitstop | 30 ms por baja con bala (fina o gorda), 100 ms por bomba y por rayos | no | falta | 1-2 f y 5 f (Q17) |
| Cámara lenta de la muerte | ×0,2 durante 1 s | fogonazo `oMuerte` (GML) | distinto | ralentizar = actualizar 1 de cada 5 frames |
| Sacudidas | pequeña ±4 px por disparo; grande con zoom y ±80/±60 px | sacudida del GML por scroll | distinto | el zoom no existe en el VDP (Q20) |
| Fondo | color plano que deriva con el nivel; rojo en game over | la imagen de ruinas (Q16); el fogonazo rojo del game over desde el bloque 5; **sin color por nivel**: `setCameraLevelColor` no se porta (Q16, bloque 9) | divergencia deliberada (Q16) | — |
| Música | 5 pistas `.xm` al azar | `musMenu` del GML | distinto | XM → XGM, coste de ROM (Q21) |
| Voces y efectos | lista de §2.12 | 20 sonidos del GML | distinto | WAV a 13300 Hz por XGM2 |

---

## 4. Coste estimado de recursos de lo que falta

**Situación de partida** (dato del usuario y `inc/vramspr.h`): la VRAM de tiles está llena, con **1 tile libre**. (Desde el bloque 2 hay **36 libres**: los agujeros sólo suben las celdas con dibujo, y 10 se reservan para el texto compuesto encima de los agujeros; ver la auditoría. La VRAM sobrante de `VTile_allocSpare` está llena.) El fondo del campo tiene **528** reservados (`VTILE_SHARED_TILES`), y los tiles del menú (iconos de arma 80, logo 49, barra 1) van a la VRAM sobrante fuera del espacio de SGDK (mapa del plano window, 128 tiles, y la cola tras la tabla de sprites, 76). RAM: `.data` + `.bss` = **13 740 bytes** (`m68k-elf-size out/release/rom.out`). ROM: 896 KB (`out/rom.bin`, 917 504 bytes), por debajo de los 2 MB, así que la SRAM en 0x200001 no choca con el ROM. (Desde el 2026-11-20 no hay SRAM: decisión del usuario, ver el bloque 6.)

| Elemento | VRAM (tiles) | Sprites de hardware | RAM | SRAM | Notas |
|---|---|---|---|---|---|
| Puntuación `u32`, nivel, `factorLevel`, temporizador de oleadas, tabla de oleadas | 0 | 0 | ~20 B (tabla en ROM) | — | — |
| Mensajes LEVEL / !!! / ARENA! / UPGRADE AT | **Hecho (bloque 9)**: fuente del sistema (Q16); +22 para la reserva de letras encima de los agujeros (de 10 a 32), +11 para las cifras amarillas y la admiración roja | 0 | ~9,4 KB (siluetas de la fuente, agujeros verdes y filas de texto en RAM) | — | ver la auditoría |
| Icono del arma en el HUD | **Hecho (bloque 9)**: 4 (2×2), el icono del menú reducido a la mitad al empezar la partida | 0 (plano A) | — | — | pagado, con lo anterior, quitando de VRAM los 9 glifos grandes que ya no se usaban (36 tiles): 1433 → 1434 de 1440 |
| SCORE en grande y sombra | **Hecho (bloque 9)**: +12 (C, O y R grandes); la sombra va pintada en los tiles que ya había (0) | 0 | — | — | pagado con 14 celdas de la variante roja de los agujeros que eran iguales a la normal: 1432 de 1440 |
| Iconos MOIRE y CIRCULAR del menú/tienda | 0 netos: sustituyen a los de PIVOT y PARA en `weapons.png` (5 iconos de 4×4 = 80) | 0 | — | — | el GML no tiene arte para estas dos armas; tomarlo de `Assets/Art/iconMoire.png` e `iconCirculo.png` **choca con la decisión 3** (Q11) |
| Moneda | **4** (1 frame de 2×2) a **24** (los 6 frames precargados); o 4 por hueco con VStream | hasta ~8 a la vez | ~100 B (8 × x, y, vida, valor, frame) | — | lo único imprescindible en partida que no tiene arte cargado; el GML no tiene moneda, así que o se toma `coinAnimation.png` de Unity (**choca con la decisión 3**) o se dibuja (Q11) |
| Balas (pool) | 0 | se destruyen al salir de pantalla (§2.5): TWAY ≤ ~35 con la nave en una esquina, ~18 desde el centro; WBLOCK ≤ 9; fina ≤ 5; naves holo ≤ 2 por nave | ~35 × 12 B | — | el pool actual es `MAX_PROJECTILES` 14 y el total de objetos `Sprite` de SGDK va por 79 de 80 (`inc/arena.h:372-376`): TWAY no cabe sin recortar o sin reorganizar los pools (Q3) |
| Haz de LASER y MOIRE | 0: reutiliza `beamSprite` o los segmentos del láser | ~10-14 por haz | ~16 B | — | el impacto instantáneo es un segmento contra círculos, barato |
| Bolas CIRCULAR | 0: reutiliza el satélite del PIVOT | ≤ 4 en órbita (§2.5) + las sueltas, que van a 24 px/f y viven lo que tardan en llegar al objetivo (≤ ~17 f cruzando la pantalla): con una tanda de 4 cada 3 f, **4-8 lo normal, ~20 en el peor caso** | ~20 × 12 B | — | cada bola suelta busca el más cercano cada frame: ~8-20 × ~11 enemigos |
| Bomba, rayos y nave holo | 0: reutilizan `groundSprite` y la réplica | rayos: un punto por segmento × enemigos; naves: hasta 10 + líneas | ~10 × 16 B | — | 10 naves y sus líneas no caben en sprites por línea: tope |
| Gusano de 17 partes (sólo si se reabre la decisión 4, Q2) | 0 (mismo arte) | +4 por gusano frente al de 13 | migas: 17 × 4 B por gusano (menos que el historial de 128 posiciones actual) | — | CPU: +30 % por gusano |
| Tienda, game over, logros, tutorial | 0 con la fuente del sistema; el arte del menú ya está en VRAM sobrante | 0 | ~32 B de estado | — | sólo texto |
| Logros | **Hecho (bloque 10)**: 0 (fuente del sistema y cifras amarillas del bloque 9; `nextTile` sigue en 1432 de 1440) | 0 | ~37 B (máscara, monedas recogidas, siguiente umbral, cola de 17 avisos y estado del aviso y de la lista) | ninguna (sin SRAM) | `tools/measure.sh` sin cambio: 505/34 |
| Guardado | 0 | 0 | — | **ninguna** (decisión del usuario del 2026-11-20: el progreso vive en RAM hasta apagar) | el bloque 6 llegó a usar ~130 B en 8 KB declarados en 0x200001-0x203FFF; retirado, la cabecera ya no declara SRAM |
| Música XM | 0 | 0 | — | — | **No se porta** (Q21 revocada, 2026-11-21) |
| Efectos y voces de MonsterLite, estela, líneas holo | **Hecho (bloque 12)**: estela 2 tiles y tramos de las líneas 24, pagados bajando el fondo de 528 a 504: `nextTile` 1434 de 1440 | estela ≤ 13, líneas ≤ 40 tramos | ~2,6 KB (pool de sprites de 256 a 320, nave roja, estela, inversos) | — | ROM +261 KB de PCM de MonsterLite y −150 KB del GML: 1,125 MB |

**Total en partida**: entre **4 y ~60 tiles** según se elija (moneda, icono del HUD y glifos). Con 1 libre, hay que sacarlos de algún sitio:

1. **Bajar el presupuesto del fondo** de 528 a, por ejemplo, 504 o 480, y regenerar con el mismo comando de `res/resources.res` (`tools/bg2md.py … --tiles <n>`). Es lo más directo; el coste es fidelidad del fondo, que hay que mirar en captura.
2. **Recuperar lo que deja el GML**: si se quitan PIVOT, PARA, BOMBA y RAY como armas (Q9), sus frames en `projectiles.png` y el haz del PARA (`beam.png`) liberan tiles, pero sólo si no se reutilizan para el láser o las bolas. Cuántos exactamente hay que medirlo con `PORT_VRAM_REPORT=1`.
3. **Fuente del sistema** para los textos nuevos en vez de ampliar la grande (0 tiles; la grande ya ocupa 100).

**CPU.** El caso peor ya va al borde del frame (`PORT_STRESS=1`). Lo que más empuja: gusanos de 17 partes (si se reabre la decisión 4), el tope de 5 gusanos en el nivel 10 (no cabe: con 3 ya se perdían frames, ver `inc/arena.h`), arañas sin tope, hasta ~20 bolas CIRCULAR buscando objetivo, hasta ~35 balas de TWAY y 10 naves holo disparando. Cada bloque que lo toque se mide con `tools/measure.sh` antes y después.

---

## 5. Propuesta de bloques de implementación

En orden. Cada bloque compila, se prueba en BlastEm y se documenta en `docs/PORT_AUDIT_GML.md` (sección nueva "MonsterLite") antes del siguiente. La prioridad del usuario (estructura, progresión, armas) son los bloques 1 y 3-6; el 2 (jugador) va antes de las armas porque es pequeño y porque el 3 da por hecho el control y el apuntado definitivos.

Cada bloque dice qué preguntas de §6 tiene que tener respondidas antes de empezar. Los fallos del original (§6, Q5, Q12 y Q15) se deciden en el bloque al que pertenecen.

### Bloque 1. Estructura de partida: puntuación, niveles y oleadas por tiempo

- **Estado (2026-11-15)**: **hecho**. Q1-Q5 respondidas (§6). Implementado y comprobado en BlastEm; detalle, mediciones y divergencias en `docs/PORT_AUDIT_GML.md`, sección "MonsterLite". Quedan fuera, en su bloque: las oleadas de power-ups (3, 4, 6, 16, 19-21: bloque 7), el texto "!!!" de la 27 (bloque 9), la posición de aparición de la nave (bloque 2), el giro, la IA y la aparición inmediata de los enemigos (bloque 8).
- **Bloqueado por**: Q1, Q2, Q3, Q4, Q5 (resueltas).
- **Incluye**: `g_score` en `u32` con `+level` por baja (`+2·level` araña por bala fina, según Q5); `level`/`factorLevel` con la regla `level = score / factorLevel` de §2.3, saltos incluidos; inicio de 40 f + 50 f con un gusano; temporizador de 20 f con la tabla `kills % 40` (§2.2), incluido el +2 gusanos con `level > 5`; tope de gusanos por nivel acotado por el tope de CPU (Q3); tope de arañas (Q4); `generaMalos()` deja de llamarse al morir una cabeza; velocidad de los enemigos con `f = (level % 10) + 0.8` (sólo la fórmula: la IA completa es el bloque 8).
- **Archivos**: `inc/arena.h` (constantes `ML_*` con su origen), `inc/game.h`, `src/game.c`, `src/enemy.c`, `src/snake.c`, `src/hud.c` (SCORE en vez de KILLS, mientras llega el bloque 9).
- **Verificación**: comprobar **la regla**, no la tabla. Con `tools/mdrun.py`, escribir por RAM pares (`g_level`, `g_factorLevel`, `g_score`) y leer el resultado tras un frame, contra un modelo de la regla en Python del scratchpad. Casos mínimos: (1, 1, 2) → nivel 2, factor 2; (1, 1, 3) → nivel 3, factor 2, UPGRADE AT 8; (2, 2, 9) → nivel 4, factor 4, UPGRADE AT 20; (2, 2, 5) → sin cambio; y una puntuación de 70000 para el `u32`. Además, contar con el arnés cuántas arañas y gusanos nacen en 3000 frames con la puntuación forzada a valores de `kills % 40` bajos y altos; `tools/measure.sh` antes y después.

### Bloque 2. El jugador

- **Estado (2026-11-15)**: **hecho**. Velocidad (Q7), wrap sin margen, aparición en (160, 17) con el hexágono centrado (Q6), colisión con un círculo de 3,6 px, congelación con B (Q8; no se congela con el panel de `GFXDEBUG=1` abierto, decisión del usuario), enemigos naciendo en un agujero al azar y la nota del láser, comprobados en BlastEm; detalle y mediciones en `docs/PORT_AUDIT_GML.md`, sección "MonsterLite". La parte de la verificación de abajo que pide 12 px entre centros con la araña necesita el radio de la araña (8,4 px), que es del bloque 8: hasta entonces la nave (círculo) choca con las cajas del GML. **2026-11-16**: por decisión del usuario, agujeros y aparición vuelven a los de antes del bloque 2 (anillo del port, nave en (160, 120)); ver `docs/PORT_AUDIT_GML.md`, "Reversión de agujeros y aparición".
- **Bloqueado por**: Q6, Q7, Q8.
- **Incluye**: velocidad de la cruceta (Q7: 4 px/f por eje, que en diagonal son 5,66 px/f como en el original, salvo que se decida normalizar); wrap en los bordes exactos (margen 0 en vez de los 3 px del GML); aparición en (160, 69), coordinada con la posición de los agujeros (Q6); colisión circular de **radio 3,6 px** en vez de la caja de 7×11 del GML; muerte al tocar araña, cabeza o eslabón (ya es así) y agujeros que no matan (ya es así); la congelación al soltar (Q8) y su aviso en pantalla (el texto va con la fuente que se decida en Q16; hasta el bloque 9, provisional). **La congelación del láser no hay que quitarla**: el port nunca la tuvo (§3). Sólo se documenta en la auditoría que ahora coincide con MonsterLite y no con el GML.
- **Archivos**: `inc/arena.h` (`PLAYER_*` nuevos con su origen, `PLAYER_WRAP_OFFSET` a 0), `src/player.c`, `inc/player.h`, `src/game.c` (posición inicial y, según Q8, la pausa), `src/collision.c`/`inc/collision.h` si hace falta un círculo contra círculo, `docs/PORT_AUDIT_GML.md`.
- **Verificación**: leer `player.e.x`/`y` por RAM con una dirección y con dos pulsadas durante 50 frames (200 px y 200 px por eje); cruzar cada borde y comprobar que reaparece en el opuesto sin margen; posición inicial en (160, 69) (con el hexágono centrado de Q6, (160, 17)); con el arnés, acercar una araña hasta que muera la nave y comprobar la distancia (3,6 + 8,4 = 12 px entre centros); captura.

### Bloque 3. Las armas de MonsterLite y su munición

- **Estado (2026-11-16)**: **hecho**, salvo la compra (bloques 4-5): hasta entonces las cinco armas de MonsterLite están disponibles en el selector. Detalle, divergencias y medición del caso peor por arma en `docs/PORT_AUDIT_GML.md`, "Bloque 3: armas y munición". El TWAY es el arma más cara (384 vueltas de 600 en el caso peor).
- **Bloqueado por**: Q9, Q10, Q11, Q12.
- **Incluye**: enum WBLOCK, LASER, TWAY, CIRCULAR, MOIRE con las cadencias de §2.5 (8 / 4 / 5 / 3 / 3 / 2 f, la de CIRCULAR según Q12); balas a 12 px/f con caja de 16 px que **se destruyen al salir de pantalla**, y el pool redimensionado para TWAY (§4); daños −1 / −3 / instantáneo; bala gorda que atraviesa si no mata; eslabones atravesables por las balas; impacto instantáneo de LASER (25 f) y MOIRE (7 f) contra **el primer objeto de la línea, triggers incluidos** (§2.5); bolas CIRCULAR (4 o 3 por disparo según Q12, órbita a 20 px y 2,4°/f para las que nacen sin munición, persecución a 24 px/f para el resto, sin volver a la órbita); munición por arma con los valores de paquete del prefab; la que sobra, según Q10; WBLOCK desplazado según Q12. Las armas del GML que no están en MonsterLite, sólo si el usuario lo autoriza (Q9). Iconos de MOIRE y CIRCULAR según Q11.
- **Archivos**: `inc/projectile.h`, `src/projectile.c`, `src/beam.c` (haz), `src/enemy.c` y `src/snake.c` (daño, eslabones), `src/game.c`, `inc/arena.h` (`MAX_PROJECTILES` y el reparto de los 80 objetos `Sprite`), `res/sprite/weapons.png` + `tools/gm2md.py` (iconos).
- **Verificación**: forzar cada arma y munición con `set` en `tools/mdrun.py` y capturar; contar impactos hasta matar un gusano (6 con la fina, 3 con WBLOCK); comprobar con el arnés que un eslabón, un paquete o una bala entre la nave y una cabeza tapan el láser; leer cuántas balas de TWAY hay vivas con la nave en una esquina (≤ ~35) y que ninguna sobrevive fuera de pantalla; con la munición de CIRCULAR a 1, comprobar que sólo las bolas del último disparo orbitan; medir el caso peor con TWAY, CIRCULAR y MOIRE.

### Bloque 4. Economía en partida: drops, paquetes y monedas

- **Estado (2026-11-16)**: **hecho**. Detalle y divergencias en `docs/PORT_AUDIT_GML.md`, "Bloque 4: economía en partida". Quedan 7 tiles de VRAM.
- **Bloqueado por**: Q11 (arte de la moneda).
- **Incluye**: 50 % moneda / 50 % paquete por baja (paquete siempre con araña muerta por bola); caducidad de 167 f; paquete con la munición del arma equipada al aparecer; monedas con valor por nivel (§2.9), vuelo hacia abajo al recogerlas, saldo total y monedas de la partida; moneda que nace sobre un agujero u otra moneda y se pierde (§2.6; si se porta tal cual o se corrige va en Q12); contador COINS en el HUD; `generaPaquete()` del GML deja de usarse.
- **Archivos**: `inc/powerup.h`, `src/powerup.c`, `src/game.c`, `src/hud.c`, `res/resources.res`, `res/sprite/` (moneda), `tools/gm2md.py` o un conversor nuevo en `tools/`; si hace falta VRAM, `inc/vramspr.h` + regenerar el fondo con `bg2md.py` y el mismo número.
- **Verificación**: `PORT_VRAM_REPORT=1` antes y después (de dónde salen los tiles); arnés que mata N enemigos y cuenta monedas y paquetes (~50 %); leer saldo por RAM; captura del HUD.

### Bloque 5. Pantallas: menú, game over y tienda

- **Estado (2026-11-16)**: **hecho**, con el logo convertido de MonsterLite por decisión del usuario. Detalle en `docs/PORT_AUDIT_GML.md`, "Bloque 5: pantallas".
- **Bloqueado por**: Q13, Q14, Q15, Q16.
- **Incluye**: menú "PRESS START" en vez de créditos (si se autoriza, Q13); logo de 40 f; game over con SCORE y COINS COLLECTED contando al ritmo que se decida (Q15), LEVEL, NEW RECORD!, BEST SCORE y las opciones PLAY (sólo cuando SCORE ha terminado) / GEAR; el fogonazo rojo del fondo al entrar si la puntuación es > 10 (según Q16); tienda GEAR con las 4 armas de pago, precio (Q14) o AVAILABLE!, saldo, compra que descuenta y equipa, estante para elegir entre las compradas, interruptores de sonido y música; el arma elegida vale para las partidas siguientes. El campo y los enemigos siguen detrás, como ahora.
- **Archivos**: `inc/menu.h`, `src/menu.c` (o un `src/store.c` nuevo si `menu.c` crece demasiado), `inc/game.h`, `src/game.c`, `src/audio.c` (sonido y música apagables).
- **Verificación**: recorrido completo con el pad en BlastEm y capturas de cada pantalla; dar saldo por RAM, comprar LASER, comprobar saldo − 6000 (o el precio de Q14), arma equipada y que la siguiente partida dispara láser; con puntuación y monedas forzadas por RAM (5000 / 500), contar los frames hasta que cada contador termina y compararlos con lo que diga Q15 (con el ritmo original: 83 f y 144 f, §2.8).

### Bloque 6. Guardado en SRAM

- **Estado (2026-11-21)**: **retirado por decisión del usuario (2026-11-20)**: el juego no usa SRAM; el progreso vive sólo en RAM y se pierde al apagar. Se hizo el 2026-11-16 y se quitó entero (`src/save.c`, `inc/save.h` y la SRAM de la cabecera). Lo que sigue describe lo que se hizo entonces. Detalle en `docs/PORT_AUDIT_GML.md`, "Bloque 6: guardado en SRAM" y "Sin SRAM".
- **Bloqueado por**: nada nuevo (lo que se guarde de la munición sale de Q10).
- **Incluye**: estructura con firma, versión, récord (`u32`), saldo (`u32`), armas compradas (máscara), arma equipada, partidas jugadas, sonido y música, logros (máscara + contadores) y suma de control; dos copias alternas para que un apagado a mitad de escritura no lo pierda todo; carga al arrancar con valores por defecto si la firma o la suma no cuadran; guardado al entrar en game over (como `setData`) y al comprar. Cabecera del cartucho con SRAM declarada.
- **Archivos**: `inc/save.h` y `src/save.c` nuevos, `src/rom_header.c` (`sram_sig`, `sram_type`, `sram_start`, `sram_end`, y el comentario que explica por qué antes no había), `src/game.c`, `src/menu.c`.
- **Verificación**: en BlastEm, jugar, comprar, reiniciar el emulador y comprobar que récord, saldo y armas siguen; corromper la SRAM a mano y comprobar que arranca con los valores por defecto; `tools/check_header.py`.

### Bloque 7. Power-ups de MonsterLite

- **Estado (2026-11-17)**: **hecho**. Detalle y mediciones en `docs/PORT_AUDIT_GML.md`, "Bloque 7: power-ups". Tope de naves holo: el de MonsterLite, 10 (medido).
- **Bloqueado por**: Q17 (y el tope de naves holo de Q3).
- **Incluye**: bomba de la oleada 3 (mata todo y puntúa), rayos de la oleada 4 (rayo hacia cada enemigo, a los 25 f mata todo y puntúa), las dos con la congelación de 100 ms al tocarlas según Q17; naves holo de las oleadas 6, 16, 19-21 (activación al tocarlas, cadencia 33 f sin munición, mueren al contacto, línea con el jugador), con un tope medido. Lo del suelo sobrevive a la muerte (ya es así en el port).
- **Archivos**: `src/powerup.c`, `src/projectile.c` (réplica → nave holo), `src/effect.c`, `src/game.c`, `inc/arena.h`.
- **Verificación**: forzar cada power-up con el arnés, contar puntos sumados con N enemigos en pantalla, contar los frames de congelación, capturas, `tools/measure.sh` con 10 naves.

### Bloque 8. Enemigos con la IA y los valores de MonsterLite

- **Estado**: ✅ hecho el 2026-11-20 (ver `docs/PORT_AUDIT_GML.md`, "Bloque 8"), con las migas de 8 px por decisión del usuario y los dos arreglos de la verificación (pila de eslabones al nacer y holo fuera de la partida).
- **Bloqueado por**: Q2 (gusano), Q6 (agujeros).
- **Incluye**: ATTACK/WANDER al azar con re-sorteo a 167 f y luego cada 25 f (y `level % 100` más por frame en WANDER), giro por fracción (Slerp), velocidades por nivel, aparición inmediata; el gusano que se decida en Q2 (uno de 16 eslabones con migas cada 8 px, o los dos del GML con los valores de MonsterLite); radios de colisión de §2.6; agujeros según Q6.
- **Archivos**: `src/enemy.c`, `inc/enemy.h`, `src/snake.c`, `inc/snake.h`, `src/hole.c`, `inc/hole.h`, `inc/arena.h`.
- **Verificación**: muestrear posición y ángulo de una araña por RAM y comparar la velocidad con la tabla de §2.6 en niveles 1 y 9; capturas de los agujeros; `tools/measure.sh` (los gusanos son lo más caro).

### Bloque 9. HUD, mensajes de partida y fondo por nivel

- **Estado**: hecho (2026-11-20), ver `docs/PORT_AUDIT_GML.md`, "Bloque 9". El color de fondo por nivel no se porta (Q16). Q22 resuelta (fila 12); queda Q23 (balas que no caben junto a la puntuación). Con el deslizamiento del HUD, la sombra en todas las fuentes y SCORE en grande, por decisión del usuario.
- **Bloqueado por**: Q16 (fondo y fuente).
- **Incluye**: BULLETS + arma, SCORE, BEST, LEVEL, UPGRADE AT, COINS; "ARENA!", "LEVEL n / NEXT UPGRADE: SCORE x" (155 f), "!!!" (25 f; 26 contando bien, ver §2.8) y la versión definitiva del aviso de congelación del bloque 2. **Fondo que cambia con el nivel** (§2.3, `setCameraLevelColor`): en cada subida, cada canal se desvía un entero de −10 a +9 sobre 255 y se acumula. Un paso de la Mega Drive son ~36 sobre 255 (3 bits por canal), así que el color se guarda en 8 bits por canal y sólo se ve el cambio cuando cruza un paso; con el fondo de color plano, es el color de fondo del VDP; si se quedan los fondos de imagen (Q16), no hay nada que portar o se aplica a algún índice de su paleta, a decidir.
- **Archivos**: `inc/hud.h`, `src/hud.c`, `tools/gm2md.py` (glifos, si se amplía la fuente grande), `res/gm_art.h`; `src/bg.c` e `inc/bg.h` para el color.
- **Verificación**: capturas en varios niveles; leer el color de fondo por RAM antes y después de subir de nivel; que el HUD no se redibuje entero cada frame (tráfico al VDP).

### Bloque 10. Logros

- **Estado**: hecho (2026-11-21), ver `docs/PORT_AUDIT_GML.md`, "Bloque 10". Los 17 logros de Q18 con las monedas que decidió el usuario el 2026-11-21; aviso en la fila 26 (2 s, en cola) y lista desde la tienda (entrada ACHIEVEMENTS). Todo invención.
- **Bloqueado por**: Q18 (resuelta).
- **Incluye**: la lista que decida el usuario, comprobada en los mismos puntos donde ya pasa lo que miden (bajas, nivel, compra, power-ups), pantalla de lista desde el menú, aviso al conseguir uno; se conservan en RAM hasta apagar (sin SRAM, decisión del usuario del 2026-11-20).
- **Archivos**: `inc/achieve.h` y `src/achieve.c` nuevos, `src/game.c`, `src/menu.c`.
- **Verificación**: provocar cada logro con el arnés y comprobar el bit en RAM.

### Bloque 11. Tutorial

- **Bloqueado por**: Q19.
- **Incluye**: lo que decida el usuario, adaptado al pad.
- **Archivos**: `src/game.c`, `src/menu.c`. Sin SRAM (decisión del usuario del 2026-11-20), "la primera vez" quiere decir **desde que se enciende la consola** o desde un RESET (confirmado por el usuario el 2026-11-21, `docs/MONSTERLITE_DECISIONES.md`): el tutorial sale en la primera partida tras cada encendido o RESET.
- **Verificación**: primera partida tras encender; captura de cada paso.

### Bloque 12. Efectos y sonido

- **Bloqueado por**: Q17 (hitstop por baja), Q20, Q21.
- **Hecho el 2026-11-21, antes que el 11 (decisión del usuario), sin música**: Q21 revocada el mismo día, en partida no hay música (MonsterLite sí tiene). Incluye además las líneas a las naves holo en tramos de puntos de 2×2 (con el fondo bajado a 504 tiles), la retirada de la muerte del GML y de nueve PCM del GML sin uso (decisiones del usuario), y el arreglo del pool de sprites, que estaba lleno.
- **Incluye**: hitstop de 30 ms por baja con bala (Q17), cámara lenta de la muerte, sacudidas de MonsterLite (Q20), estela de la nave (13 f), voces nuevas ("ARENA", "AWESOME", "EXPLOSIONS", "RAYS", "CARALLUDO"…), "ding" del contador con la regla de Q15, música (Q21).
- **Archivos**: `src/shake.c`, `src/effect.c`, `src/audio.c`, `res/resources.res`, `res/sound/`.
- **Verificación**: escuchar en BlastEm; tamaño del ROM; `tools/measure.sh`.

---

## 6. Preguntas abiertas para el usuario

Numeradas en el orden en que bloquean los bloques de §5: las del bloque 1 primero. Entre corchetes, el número que tenían en la primera versión de este documento.

**Todas respondidas por el usuario el 2026-11-15**, salvo las del bloque 9 (2026-11-20): Q22 resuelta ese día y Q23 pendiente. Las respuestas están en `docs/MONSTERLITE_DECISIONES.md`, que manda sobre este apartado, y se han copiado al principio de cada pregunta.

### Bloque 1

**Q1. Escala.** [Q1] **Resuelta (2026-11-15): apaisado, 1 unidad = 20 px; `FixedUpdate` 1:1 y `Update` ÷1.2.** MonsterLite gira con el dispositivo: en apaisado la cámara ve 16×12 unidades (1 u = 20 px en 320×240) y en vertical 18 de alto (13,3 px/u). Propongo el apaisado, que es el formato de la Mega Drive. ¿De acuerdo? Todas las distancias y velocidades de este documento usan 20 px/u.

**Q2. ¿Se reabre la decisión 4 de `port-arena.md` (gusanos con longitud fiel)?** [nueva] **Resuelta (2026-11-15): opción (b).** Se mantienen los dos gusanos del GML con su longitud (12 y 10 partes además de la cabeza); la decisión 4 sigue vigente. Se les aplicará el resto de valores de MonsterLite (velocidad, IA, hp, eslabones que no paran balas) en el bloque del gusano (8). En el bloque 1, cada "gusano" de la tabla de oleadas se reparte con el criterio de `generaMalos()` del GML: tipo 1 hasta su tope, luego tipo 2 (ver la auditoría). La decisión fija los dos gusanos del GML, de 12 y 10 partes además de la cabeza, y dice que si falta CPU se recortan los topes y nunca la longitud. MonsterLite tiene **un solo** gusano de cabeza + **16 eslabones** a 8 px, hp 4. Opciones: (a) se reabre y se porta el de MonsterLite (17 partes, +30 % de coste por gusano frente al de 13, así que el tope de Q3 baja); (b) se mantienen los dos del GML con su longitud y se les aplican el resto de valores de MonsterLite (velocidad, IA, hp, eslabones que no paran balas); (c) otra. Bloquea el bloque 1 porque el tope de gusanos que se mide ahí depende de la longitud; la implementación va en el bloque 8.

**Q3. Topes de población.** [Q9] **Resuelta (2026-11-15):** se implementan los topes de MonsterLite (gusanos 2/5/8 según nivel) y se recortan hasta donde aguante la consola, midiendo con `tools/measure.sh`; cada recorte se documenta con su medición. Gusanos: recortado a 3 (medido en el bloque 1, ver la auditoría). Naves holo, balas y bolas: en sus bloques. MonsterLite permite 5 gusanos desde el nivel 10 y 8 desde el 20; hasta 10 naves holo; CIRCULAR puede tener ~20 bolas en el peor caso; y TWAY hasta ~35 balas en pantalla (§4), con un pool actual de 14 y 79 de los 80 objetos `Sprite` de SGDK ya repartidos. El port ya va al límite con 3 gusanos de 13. Propongo dejar los topes que salgan de medir y documentarlos como recorte. ¿Vale?

**Q4. Arañas.** [Q10] **Resuelta (2026-11-15): se mantiene el tope de 8.** MonsterLite no tiene tope de arañas. ¿Mantengo el 8 del port?

**Q5. Fallo del original en la puntuación.** [Q13(e)] **Resuelta (2026-11-15): se corrige.** Toda baja suma `level`, también la de la bala fina contra araña. Divergencia deliberada, documentada en la auditoría. La bala fina da doble puntuación con las arañas (`2·level`) y la gorda no. ¿Se porta tal cual o se corrige?

### Bloque 2

**Q6. Agujeros y punto de aparición.** [Q2] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`) y sustituida el 2026-11-16: vuelven el anillo del port y la nave en el centro.** Respuesta original: hexágono de MonsterLite **centrado en pantalla**; la nave nace donde la sitúa MonsterLite respecto al hexágono, recalculado para el hexágono centrado. Corrección (bloque 2): en el dispositivo apaisado la nave está 2,57 u (51 px) por encima del centro de la **pantalla** y el hexágono 2,57 u por debajo, así que respecto al hexágono la nave está **5,14 u = 102,9 px por encima** de su centro, no 51 px como decía esta línea. Con el hexágono centrado sale (160, 17), justo debajo del HUD provisional (ver la auditoría). Lo que sigue es el planteamiento original de la pregunta. MonsterLite pone los agujeros en hexágono, 51 px por debajo del centro, y la nave nace 51 px por encima, en (160, 69); en el editor de Unity los agujeros quedaban centrados. ¿Hexágono abajo y nave arriba, como en el dispositivo apaisado; todo centrado; o se quedan los 6 del anillo actual (y entonces, dónde nace la nave)?

**Q7. Control.** [Q3] **Sustituida el 2026-11-16: movimiento del GML (1,44 px/f por eje, sin normalizar, wrap de 3 px).** **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **4 px/f normalizado**: la misma velocidad en todas las direcciones (sin los 5,66 px/f de la diagonal). Lo que sigue es el planteamiento original de la pregunta. El táctil mueve la nave arrastrando el dedo (movimiento relativo). La rama de teclado de MonsterLite, que también se compilaba en Android, mueve a 0,2 u por tick = **4 px/f por eje**, sin inercia, y como suma los dos ejes sin normalizar, **en diagonal va a 5,66 px/f**. El port va a 1,44 px/f (GML), también sin normalizar. ¿4 px/f con la cruceta, con la diagonal más rápida como en el original o normalizada a 4?

**Q8. Congelación al soltar el dedo.** [Q4] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **el tiempo sólo corre mientras se mantiene pulsado un botón**, B por defecto (el usuario puede pedir otro). Sustituye a "KEEP YOUR FINGER ON SCREEN". **Segunda decisión (bloque 2, `docs/MONSTERLITE_DECISIONES.md`):** el depurador de gráficos (`make GFXDEBUG=1`) usa B con su panel abierto; con el panel abierto el tiempo no se congela y B sigue siendo del depurador. La ROM normal no cambia. Lo que sigue es el planteamiento original de la pregunta. En MonsterLite el tiempo se para si no tocas la pantalla ("KEEP YOUR FINGER ON SCREEN!"). Con la rama de teclado el juego se quedaría parado siempre, así que en dispositivo era sólo táctil. Opciones: (a) el tiempo se para si no pulsas ninguna dirección, (b) se para si no mantienes un botón, (c) se quita. ¿Cuál?

### Bloque 3

**Q9. Armas del GML.** [Q7] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **conviven**: las armas de MonsterLite se compran en la tienda y las del GML se mantienen con su desbloqueo por récord. No se quita ninguna arma. Lo que sigue es el planteamiento original de la pregunta. MonsterLite no tiene PIVOT ni PARA, y BOMBA y RAY son power-ups, no armas. ¿Se quitan del port como armas? ¿Y se elimina el desbloqueo por récord (40/60/80/100), o convive con la compra?

**Q10. Munición entre partidas.** [Q8] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se conserva hasta apagar la consola**; no se guarda en SRAM. Lo que sigue es el planteamiento original de la pregunta. En MonsterLite la munición sobrante pasa a la partida siguiente mientras la aplicación está abierta, pero no se guarda. En consola equivale a: se conserva hasta apagar. ¿Así, guardada en SRAM, o a 0 en cada partida como ahora?

**Q11. ¿Se reabre la decisión 3 de `port-arena.md` (arte desde los PNG de GameMaker)?** [nueva] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **opción (a), sólo para la moneda y los iconos de MOIRE y CIRCULAR**, que se convierten desde el arte de MonsterLite; el resto sigue saliendo de GameMaker. La decisión 3 se reabre sólo para eso. Lo que sigue es el planteamiento original de la pregunta. El GML no tiene arte para tres cosas que MonsterLite necesita: la **moneda**, y los **iconos de MOIRE y CIRCULAR**. MonsterLite sí los tiene (`Assets/Art/coinAnimation.png`, `iconMoire.png`, `iconCirculo.png`). Opciones: (a) se reabre sólo para lo que el GML no tiene, y se convierte desde el arte de Unity; (b) no se reabre: se dibujan a mano en el estilo del arte del GML (invención, marcada en la auditoría) o se reutiliza arte del GML (por ejemplo, otro icono de arma); (c) se reabre del todo y pasa a mandar el arte de Unity. Bloquea los bloques 3 (iconos) y 4 (moneda).

**Q12. Fallos del original en las armas y los drops.** [Q13(a), (b), (f) y nuevo] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se corrigen los cuatro**: CIRCULAR con su cadencia propia (4 f), sin la cuarta bola superpuesta, WBLOCK hacia donde apunta, y las monedas ya no se pierden. Lo que sigue es el planteamiento original de la pregunta. ¿Se portan tal cual o se corrigen?
- (a) CIRCULAR usa la cadencia del TWAY (3 f) y no la suya (4 f).
- (b) Salen 4 bolas por disparo y dos van superpuestas (bucle `<= 360`).
- (c) WBLOCK sale desplazado en X de pantalla, no hacia donde apunta.
- (d) Una moneda que nace sobre un agujero u otra moneda sale disparada 900 px hacia arriba y se pierde, y por la errata `"paguete"` los paquetes no lo provocan (§2.6). Afecta al bloque 4.

### Bloque 5

**Q13. Créditos de recreativa.** [Q6] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se quitan**: START empieza la partida. Lo que sigue es el planteamiento original de la pregunta. El port arranca con créditos (START mete uno, A/B/C empieza), que vienen del GML. MonsterLite no los tiene. ¿Los quito y dejo START para empezar? Es quitar funcionalidad, por eso pregunto.

**Q14. Precios.** [Q5] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se mantienen**: 6000 / 12000 / 24000 / 48000. Lo que sigue es el planteamiento original de la pregunta. 6000 / 12000 / 24000 / 48000 monedas estaban pensados con packs de pago (2500-100000) y vídeos (+420). Sin ellos sólo quedan las monedas de partida: 50-60 cada una hasta el nivel 10, la mitad de las bajas. El LASER pide unas 110 monedas recogidas y el MOIRE, unas 870. ¿Se mantienen los precios o se reequilibran? Si se reequilibran, ¿con qué criterio? No es un valor que pueda sacar del código.

**Q15. Ritmo de los contadores del game over.** [Q13(c) y (d), replanteada] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **opción (a), el mismo ritmo acelerado que el original**, con un "ding" por frame. "LAST SCORE" no se dibuja, igual que en el original. Lo que sigue es el planteamiento original de la pregunta. En MonsterLite los contadores no suben de uno en uno: cada frame nace una corrutina nueva que también cuenta, así que el frame t suma t puntos (5000 puntos en ~83 f, 20 000 en ~167 f), las monedas terminan unos segundos después y el "ding" suena tantas veces por frame como corrutinas haya (hasta ~200 superpuestos, §2.8). PLAY espera a que SCORE termine. ¿Cómo se porta?
- (a) Igual: el frame t suma t, a la puntuación y, con su regla, a las monedas. Es aritmética entera y se reproduce exacto. El "ding" no puede sonar t veces por frame: uno por frame (o cada pocos frames, para que se oiga como tal).
- (b) Duración fija (por ejemplo, 2 s para cada contador, sea cual sea el valor).
- (c) De uno en uno por frame, sin acelerar (5000 puntos = 100 s: no lo recomiendo).

Y aparte: "LAST SCORE" se calcula con un contador que nunca sube (siempre 0) y no se dibuja. ¿Se quita o se enseña la puntuación de la partida anterior?

**Q16. Fondo y fuente.** [Q16] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se queda la imagen de ruinas del port, sin color por nivel**; los textos nuevos van con la fuente del sistema. Lo que sigue es el planteamiento original de la pregunta. MonsterLite tiene fondo de color plano que deriva con el nivel y da un fogonazo rojo al entrar en el game over; el port tiene tus fondos de imagen. ¿Se quedan los fondos del port (y el color por nivel se omite o se aplica a algún color de su paleta), o pasa a color plano? Y los textos nuevos (SCORE, LEVEL, COINS, UPGRADE AT, tienda): ¿con la fuente del sistema (0 tiles) o ampliando la grande (unos 20 tiles más en partida)? Bloquea los bloques 5 y 9.

### Bloque 7

**Q17. Hitstop.** [Q14] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **sí**: 1-2 frames por baja con bala y 5 frames con la bomba y los rayos. Lo que sigue es el planteamiento original de la pregunta. El original congela 30 ms en cada baja **por bala**, fina o gorda (`S/bulletScript.cs:62, 80`, `S/bulletThinSrc.cs:45, 65`; no con LASER, MOIRE, CIRCULAR ni las muertes por bomba o rayos), y 100 ms al tocar la bomba **y al tocar los rayos** (`Thread.Sleep`, `S/playerMovement.cs:200, 213`). ¿Lo porto como 1-2 frames y 5 frames de pausa? Lo de la bomba y los rayos va en el bloque 7; lo de cada baja, en el 12.

### Bloque 9

**Q22. Posición de "!!!".** (Nueva, del bloque 9.) **Resuelta (2026-11-20, `docs/MONSTERLITE_DECISIONES.md`): centrado y un poco más arriba, para que no lo tape la nave al nacer; va en la fila 12 (96-103 px).** Lo que sigue es el planteamiento original de la pregunta. MonsterLite dibuja " !!!" con `readySt`, que está centrado (`MiddleCenter`), en un rectángulo que empieza en (W/2, H − H/1.9) y mide la pantalla entera (`S/HUD.cs:505`). El texto queda centrado en (W, 0,97·H): en la esquina de abajo a la derecha, casi todo fuera de la pantalla (en 1024×768 se ve medio signo de admiración; en NTSC, nada). El port lo pone donde parece apuntar el código, centrado en x y a la altura y = H − H/1.9 = 114 px (fila 14), justo encima de donde aparece la nave. ¿Se queda así, se copia tal cual (en la esquina, casi invisible) o se pone en otro sitio?

**Q23. Balas y puntuación en la misma línea.** (Nueva, del bloque 9; sin responder.) Con SCORE en grande, como pidió el usuario, la línea de arriba tiene 20 glifos de 16 px: BULLETS y SCORE ocupan 12 y quedan 8 para las dos cifras y un hueco. Si no caben (balas de 2 cifras con 6 de puntuación, de 3 con 5, de 4 con 4), ahora manda la puntuación y las balas salen recortadas a 9 ("BULLETS9 123456SCORE" con 35 balas). Otras opciones: las cifras de las balas con la fuente pequeña cuando no caben; quitar el hueco y que las dos cifras se toquen; o subir la palabra SCORE a otra fila sólo entonces. ¿Cuál?

### Bloque 10

**Q18. Logros.** [Q11] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **todos los grupos propuestos** (progreso, tienda, power-ups) y **cada logro se recompensa con monedas**. **Cantidades cerradas por el usuario el 2026-11-21:** primera partida 50; niveles 5, 10 y 15: 100, 500 y 2000; puntuación 100, 1000 y 10 000: 100, 300 y 1000; comprar LASER, TWAY, CIRCULAR y MOIRE: 600, 1200, 2400 y 4800; recoger 1000 y 10 000 monedas en total (sólo las cogidas en partida, acumuladas entre partidas): 100 y 1000; usar una bomba, unos rayos y activar una nave holo: 100 cada uno; 10 bajas con una sola bomba: 500. Son 17 (la propuesta de abajo tiene 17; la decisión decía 18 por error). Todo en RAM, sin SRAM. Implementado en el bloque 10. Lo que sigue es el planteamiento original de la pregunta. MonsterLite no define ninguno. Propuesta, para que decidas (todo inventado, marcado así en la auditoría):
- primera partida; llegar a los niveles 5, 10 y 15;
- 100, 1000 y 10 000 de puntuación;
- comprar cada arma (4);
- recoger 1000 y 10 000 monedas en total;
- usar una bomba, unos rayos y activar una nave holo;
- matar 10 enemigos con una sola bomba.

¿Cuáles van, y se recompensan con monedas o no?

### Bloque 11

**Q19. Tutorial.** [Q12] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **sólo una pantalla de instrucciones** la primera vez, sin la línea "AVOID THE HOLES". Sin SRAM, "la primera vez" es la primera partida desde que se enciende la consola o desde un RESET (usuario, 2026-11-21). Lo que sigue es el planteamiento original de la pregunta. Está desactivado en la versión final de MonsterLite. Lo que queda es una práctica con 4 arañas y 1 gusano que se repite si te tocan, más los textos "DODGE BUGS", "AVOID BUGS!" y la tarjeta final "AVOID THE HOLES / … THE SHIP SHOOTS AUTOMATICALLY THE CLOSEST TARGET / COLLECT AMMO / COLLECT COINS". Dos dudas: ¿se porta esa práctica o basta una pantalla de instrucciones la primera vez? Y la tarjeta dice "AVOID THE HOLES", pero en la versión final los agujeros no matan: ¿se quita esa línea, o los agujeros deben matar?

### Bloque 12

**Q20. Sacudida grande.** [Q15] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **sólo desplazamiento, al tamaño original** (±80/±60 px), sin zoom. Lo que sigue es el planteamiento original de la pregunta. El original hace zoom (tamaño de cámara aleatorio de 0 a 5) y desplaza hasta ±80/±60 px. El VDP no tiene zoom. ¿Sólo el desplazamiento, a ese tamaño o reducido?

**Q21. Música.** [Q17] **Resuelta (2026-11-15, `docs/MONSTERLITE_DECISIONES.md`):** **se convierten las 5 pistas `.xm`** de MonsterLite. **Revocada por el usuario el 2026-11-21:** no se convierten las pistas, ni ahora ni después; en partida no hay música y en el menú y el game over sigue `musMenu` del GML (`docs/MONSTERLITE_DECISIONES.md`). El estudio del bloque 12 queda descartado. Lo que sigue es el planteamiento original de la pregunta. MonsterLite tiene 5 pistas `.xm` que cambian en cada partida. ¿Se convierten (coste de ROM y de trabajo) o se queda la música actual?

---

## 7. Notas

- En el último commit, la línea de control táctil de Android tiene un carácter suelto (`acceleartionTouch º;`, `S/playerMovement.cs:89`) que no compila en Android. O la última build fue de iOS, o se compiló antes de ese cambio. No afecta a los valores.
- La versión gold sólo quita la publicidad (`globales.goldenVersion` no toca nada de juego). La tienda que se toma como referencia es la de la versión gold, sin packs de pago, sin "RESTORE PURCHASES", sin vídeo y sin "REMOVE ADS". En el código, "YOUR COINS" sólo se dibuja en la versión gratuita; en el port hace falta igualmente para poder comprar.
