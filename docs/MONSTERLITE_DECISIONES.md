# Decisiones del usuario sobre MonsterLite (2026-11-15)

Respuestas a las preguntas abiertas de `docs/MONSTERLITE_GAP.md` §6. Mandan sobre lo que diga ese documento. El desarrollador las incorpora a §6 al trabajar cada bloque.

| Q | Tema | Decisión |
|---|---|---|
| Q1 | Escala | Apaisado, 1 unidad = 20 px. FixedUpdate 1:1, Update ÷1.2. |
| Q2 | Gusanos | Se mantienen **los dos gusanos del GML con su longitud** (12 y 10 partes); la decisión 4 sigue vigente. Se les aplica el resto de MonsterLite (velocidad, IA, hp, eslabones que no paran balas). |
| Q3 | Topes de población | Valores de MonsterLite, **recortados hasta donde aguante la consola** (medido con `tools/measure.sh`). Cada recorte se documenta. |
| Q4 | Arañas | Tope de **8**. |
| Q5 | Doble puntuación de la bala fina contra araña | **Se corrige**: toda baja suma el nivel. |
| Q6 | Agujeros y aparición | **Hexágono de MonsterLite centrado en pantalla.** La nave nace donde lo sitúe MonsterLite respecto al hexágono, recalculado para el hexágono centrado. |
| Q7 | Control | **4 px/f normalizado**: la misma velocidad en todas las direcciones. |
| Q8 | Congelar el tiempo | **El tiempo corre solo mientras se mantiene pulsado un botón**: B por defecto, pendiente de que el usuario pida otro. Sustituye a "KEEP YOUR FINGER ON SCREEN". |
| Q9 | Armas del GML | **Conviven**: las de MonsterLite se compran en la tienda, y las del GML se mantienen con su desbloqueo por récord. |
| Q10 | Munición entre partidas | **Se conserva hasta apagar la consola**; no se guarda en SRAM. |
| Q11 | Arte | **Solo la moneda y los iconos de MOIRE y CIRCULAR** se convierten desde el arte de MonsterLite; el resto sigue saliendo de GameMaker. La decisión 3 se reabre solo para eso. |
| Q12 | Fallos de armas y drops | **Se corrigen los cuatro**: cadencia propia de CIRCULAR (4 f), sin la cuarta bola superpuesta, WBLOCK hacia donde apunta, y las monedas ya no se pierden. |
| Q13 | Créditos de recreativa | **Se quitan**: START empieza la partida. |
| Q14 | Precios | **Se mantienen**: 6000 / 12000 / 24000 / 48000. |
| Q15 | Contadores del game over | **Mismo ritmo acelerado que el original**, con un "ding" por frame. "LAST SCORE" no se dibuja, igual que en el original. |
| Q16 | Fondo y fuente | **Se queda la imagen de ruinas del port**, sin color por nivel. Los textos nuevos van con la fuente del sistema. |
| Q17 | Hitstop | **Sí**: 1-2 frames por baja con bala, 5 frames con bomba y rayos. |
| Q18 | Logros | **Todos los grupos propuestos** (progreso, tienda, power-ups) y **cada logro se recompensa con monedas**. Las cantidades son invención: el desarrollador las propone y se le consultan al usuario antes de cerrarlas. |
| Q19 | Tutorial | **Solo una pantalla de instrucciones** la primera vez, sin la línea "AVOID THE HOLES". |
| Q20 | Sacudida grande | **Solo desplazamiento, al tamaño original** (±80/±60 px), sin zoom. |
| Q21 | Música | **Se convierten las 5 pistas `.xm`** de MonsterLite. |

## Decisiones añadidas durante el bloque 2 (2026-11-15)

| Tema | Decisión |
|---|---|
| Q8: botón B y el depurador de gráficos (`GFXDEBUG=1`) | **Con el panel del depurador abierto, el tiempo no se congela** y B sigue siendo del depurador. La ROM normal no cambia. |
| Aparición de la nave | **(160, 17)**, como en el original: 103 px por encima del centro del hexágono. Que quede bajo el HUD provisional se acepta hasta el bloque 9. |
| Orientación inicial de la nave | **Hacia arriba**, como ahora (desviación deliberada respecto a MonsterLite, que mira a la derecha). |

## Decisiones añadidas al cerrar el bloque 2 (2026-11-15)

| Tema | Decisión |
|---|---|
| Agujeros y aparición de la nave (**sustituye a Q6** y a la aparición en (160, 17)) | **Vuelven como estaban antes del bloque 2**: el anillo de 6 agujeros del port y la aparición de la nave del commit `4628c56` (bloque 1, anterior al bloque 2). El resto del bloque 2 se mantiene: velocidad, wrap, radio, congelar con B y arañas en un agujero al azar. |
| Armas que están en los dos juegos (WBLOCK, LASER, 3WAY/TWAY) | **Una sola versión de cada una, con los valores de MonsterLite**, y se consiguen **por compra** (WBLOCK gratis). PIVOT, PARA, BOMBA y RAY siguen desbloqueándose por récord, como en el GML. CIRCULAR y MOIRE, por compra. |

## Decisiones del 2026-11-16

| Tema | Decisión |
|---|---|
| Movimiento de la nave (**sustituye a Q7** y al wrap sin margen del bloque 2) | **Como en el GML (surfize)**: `acc = 6` por eje (1,44 px/f), sin normalizar la diagonal, y wrap con margen `offSet = 15` (3 px). |
| Color del fondo | Las ruinas en **verde oscuro y con menos brillo**. |
| Color de los agujeros | **Verdes, un poco más claros** que el negro original. El reborde rojo de cuando sale un gusano se queda. |
| Logo de Screen Implosion al arrancar | **Se convierte desde el arte de MonsterLite** (`screenImplosionLogo_inv_Square_1800.png`). La decisión 3 se reabre también para él. |
| Botones A y C | **Hacen lo mismo que B en partida** (el tiempo corre mientras se mantiene A, B o C; el aviso pasa a "HOLD A/B/C") **y lo mismo que A en el menú** (en el splash, A y C empiezan la partida igual que START; en el game over y la tienda ya aceptaban A, C y START). |

## Decisiones del 2026-11-20 (bloque 8)

| Tema | Decisión |
|---|---|
| Nave holo inactiva | **También muere cuando la toca una araña o una cabeza**, como en MonsterLite: `collideAndDie` sólo se habilita al activarla, pero Unity manda `OnTriggerEnter` también a los componentes desactivados. Mismos radios y misma explosión que la activa, en partida y fuera de ella. |
| Seguimiento de los eslabones del gusano | **Se mantienen las migas de 8 px de MonsterLite** (`segmentSpacing` 0,4 u) en lugar del `stepdelay` del GML. Los gusanos siguen siendo los dos del GML con 12 y 10 eslabones (Q2); la decisión 4 queda en "longitud fiel" en número de eslabones. |

## Decisiones del 2026-11-20 (bloque 9)

| Tema | Decisión |
|---|---|
| Fondo por nivel (Q16) | **No se porta `setCameraLevelColor`**: se queda la imagen de ruinas sin color por nivel. |
| Deslizamiento del HUD | **El HUD entra y sale deslizándose como en MonsterLite** (en el port, a escalones de una fila de 8 px). |
| Sombra | **Todas las fuentes llevan sombra** (la grande, la del sistema y las demás del HUD, los mensajes, los menús y las pantallas), **pintada en los tiles de la fuente**, sin segunda capa ni escrituras por frame. Sin sacrificar nada visible por VRAM. |
| Línea de arriba | **`BULLETS<n> <n>SCORE`**: la cifra de las balas pegada a BULLETS y la de la puntuación pegada a SCORE, alineada a la derecha; **SCORE del mismo tamaño que BULLETS** (fuente grande; las letras que falten, creadas con el mismo estilo). |
| Posición de "!!!" (**Q22**) | **Centrado, como estaba, pero un poco más arriba** para que la nave, que nace en (160, 120), no lo tape: fila 12 (96-103 px). |

## Decisiones del 2026-11-20 (guardado)

| Tema | Decisión |
|---|---|
| Guardado (**sustituye al progreso en SRAM** y al bloque 6) | **El juego no usa SRAM.** Récord, saldo, armas compradas, arma equipada, sonido y música se guardan sólo en RAM y se pierden al apagar; al encender, los valores de una partida nueva. La cabecera del cartucho no declara SRAM. |

## Decisiones del 2026-11-21 (guardado)

| Tema | Decisión |
|---|---|
| Botón RESET | **Reiniciar es lo mismo que apagar y encender**: récord, saldo, armas compradas, arma equipada, munición, sonido y música vuelven a los valores por defecto. |
| Tutorial "la primera vez" (bloque 11, **Q19**) | Sin SRAM, "la primera vez" quiere decir **desde que se enciende la consola** (o desde un RESET): el tutorial vuelve a salir en cada encendido y tras cada RESET. |

## Decisiones del 2026-11-21 (logros, bloque 10)

| Tema | Decisión |
|---|---|
| Recompensas de los logros (**cierra Q18**) | **17 logros**, cada uno con monedas que se suman al saldo al conseguirlo. PROGRESO: primera partida (terminar una) 50; llegar al nivel 5, 10 y 15: 100, 500 y 2000; puntuación 100, 1000 y 10 000: 100, 300 y 1000. TIENDA: comprar LASER 600, TWAY 1200, CIRCULAR 2400 y MOIRE 4800 (WBLOCK viene comprada); recoger 1000 y 10 000 monedas en total: 100 y 1000. POWER-UPS: usar una bomba 100, usar unos rayos 100, activar una nave holo 100, matar 10 enemigos con una sola bomba 500. (La propuesta decía 18 por error: son 17.) |
| Qué cuenta cada uno | "Recoger monedas en total": sólo las cogidas en partida (ni las de recompensa ni el saldo), en un contador `u32` acumulado entre partidas. "Usar una bomba / unos rayos": el power-up se dispara. "10 enemigos con una sola bomba": las bajas de una misma detonación. "Activar una nave holo": recogerla o activarla. |
| Dónde viven | **En RAM**: una máscara de bits y los contadores. Se pierden al apagar o con RESET; sin SRAM. |
| Qué es invención | Todo: MonsterLite no define logros. Va marcado en la tabla de divergencias de `docs/PORT_AUDIT_GML.md`. |

## Decisiones del 2026-11-21 (bloque 12)

| Tema | Decisión |
|---|---|
| Orden de los bloques | **El bloque 12 (efectos y sonido) va antes que el 11** (tutorial). |
| Q17, Q20, Q15 en el bloque 12 | Las de 2026-11-15, sin cambios. |
| **Q21 revocada: música** | **No se convierten las pistas `.xm`**, ni en el bloque 12 ni después. **En partida no hay música**; en el menú y el game over sigue la del GML (`musMenu`). El estudio que se hizo queda descartado (`docs/PORT_AUDIT_GML.md`, bloque 12). Sustituye a Q21 del 2026-11-15. |
| Líneas a las naves holo | **El mismo grosor que la estela de la nave**, con **puntos muy juntos que formen una línea**. Implementado con el punto de 2×2 de la estela en azul, uno cada 3 px como mínimo; como cada punto es un sprite (80 por frame, 20 por línea de pantalla), hay dos topes del desarrollador: 40 puntos entre todas las líneas y 6 por franja de 8 px. Las líneas casi horizontales y las de muchas holo salen más separadas; el detalle, en `docs/PORT_AUDIT_GML.md`, bloque 12. |
| Líneas a las naves holo a 3 px en cualquier dirección | **Se quitan tiles al fondo** (`VTILE_SHARED_TILES` de 528 a 504) para dibujar las líneas con **tramos de varios puntos de 2×2 ya dibujados en 7 direcciones** (24 tiles): ~3 px (2,8-3,6) en cualquier dirección. Sustituye a la fila anterior. |
| Sonido de la muerte | **Se quita el `morteSnd` del GML**: en MonsterLite la muerte no suena. |
| PCM del GML sin uso | **Se borran** de `res/resources.res` y de `res/sound/`: `sfxWBlock`, `sfxThreeWay`, `sfxCircular`, `sfxBomba`, `sfxRayos`, `sfxPowUp`, `sfxRepetea`, `sfxRisa` y `sfxMorte`. |

## Decisiones del 2026-11-22 (bloque 12)

| Tema | Decisión |
|---|---|
| Coste de las líneas a las naves holo | **Las líneas no deben notarse en el rendimiento**: con 10 holo, la diferencia con y sin líneas, dentro del ruido (±2 %), y el caso normal sin empeorar. Implementado guardando lo hecho de un frame al siguiente y rehaciendo 2 líneas por frame cuando se mueve la nave (`docs/PORT_AUDIT_GML.md`, bloque 12, tercera entrega): −2,2 % con la nave quieta (el arnés de medida) y −9 % moviéndose. Se mantienen el grosor de la estela, los puntos a ~3 px y los topes por línea de pantalla. |
| Explosión de la muerte | **El usuario la prefería como antes del bloque 12** (con su destello blanco y la sacudida). Comparada frame a frame con HEAD, el destello blanco es igual y en HEAD no había sacudida (se cortaba en el mismo frame): no se ha cambiado nada. **Pendiente de que el usuario elija**: dejarla así, poner la sacudida del GML a cámara lenta, o volver a la muerte de HEAD sin cámara lenta (`docs/PORT_AUDIT_GML.md`, bloque 12, tercera entrega). |
