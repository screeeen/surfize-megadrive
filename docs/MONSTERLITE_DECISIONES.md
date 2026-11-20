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
