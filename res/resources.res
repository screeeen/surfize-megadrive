# Generado a partir del arte del proyecto GameMaker con tools/gm2md.py.
# Todos los sprites comparten la misma paleta de 16 colores (PAL1), así que
# basta cargar spritePalette de res/gm_palettes.h.
#
# Cada celda lleva el origen del sprite original en su centro, igual que
# GameMaker, porque la rotación es alrededor del origen: de ahí las celdas de
# 24x24 para un arte que sólo ocupa unos 13 px.

# oPlayer: 32 rotaciones de sprite173 (38x58, origen 19,19). SIN comprimir:
# los frames viven en ROM y se suben por DMA al cambiar (VStream, inc/vramspr.h)
SPRITE playerShip   "sprite/player.png"      3 3 NONE
# oSpider: 8 frames de animación x 8 rotaciones de sprite185 (64x64). SIN
# comprimir: cada araña sube su frame por DMA al cambiar (VStream)
SPRITE spiderSprite "sprite/spider.png"      2 2 NONE
# oCabeza1: spr_rtype_outslay_head (64x64), 32 rotaciones, sin comprimir (VStream)
SPRITE snakeHead1   "sprite/snake_head1.png" 2 2 NONE
# oCabeza2b: spr_rtype_outslay_head_2 (70x70), 32 rotaciones, sin comprimir (VStream)
SPRITE snakeHead2   "sprite/snake_head2.png" 3 3 NONE
# oCu2 (el último eslabón, con el arte de la cabeza): 8 rotaciones precargadas
SPRITE snakeCu2     "sprite/snake_cu2.png"   2 2 NONE
# oCu1 / oCabeza2: spr_rtype_outslay_tail (64x64)
SPRITE snakeTail    "sprite/snake_tail.png"  2 2 NONE
# oCorpo: spr_rtype_outslay_body (51x64), sin rotación en el original
SPRITE snakeBody    "sprite/snake_body.png"  2 2 FAST
# Todo lo que lanza el jugador, en una hoja: una fila por arma y rotación
# (oBulletThin, oWBlock, o3way, oLaser, oBulletPara, oBomba)
SPRITE projectiles  "sprite/projectiles.png" 3 3 FAST
# oExplosion + el polvo de oca: fila 0 fogonazo, filas 1-3 humareda
SPRITE explosionSprite "sprite/explosion.png" 8 8 FAST
# oDestro: la metralla de oEstallido, dos colores x cuatro tamanos
SPRITE debrisSprite "sprite/debris.png" 2 2 FAST
# oMuerte: el fogonazo de la muerte del jugador
SPRITE muerteSprite "sprite/muerte.png" 8 8 NONE
# object1: el haz del arma PARA, una animacion por rotacion
SPRITE beamSprite   "sprite/beam.png"   2 2 FAST
# oPuntoMorte: los digitos que vuelan al HUD
SPRITE digitSprite  "sprite/digits.png" 1 1 FAST
# oPlayer/Draw_0: los puntos de la linea azul a las replicas
SPRITE dotSprite    "sprite/dot.png"    1 1 FAST
# La moneda, desde el arte de MonsterLite (Q11): python3 tools/ml_coin.py
SPRITE coinSprite   "sprite/coin.png"   2 2 FAST

# Fondos del campo. NO salen del original (el campo es un color plano): son
# imagenes que pidio el autor (originales en res/bg_src). tools/bg2md.py las
# reduce a un diccionario de tiles que cabe en la VRAM. Ver inc/bg.h.
#   python3 tools/bg2md.py res/bg_src/ruins_md.png --tiles 528 --name ruins --native --soft --focus 8
#     (ruins_md.png es arte de Mega Drive a 1:1, 320x224; el fondo anterior
#      salia de ruins.png con --crop43 --pixel 5.904x5.728)
TILESET bgRuinsTiles       "sprite/bg_ruins.png" NONE NONE ROW

# oAgujerSuper: sprAgujero a escala 1.3 tintado con c_gray, al plano A
TILESET holeTiles   "sprite/hole.png" NONE NONE ROW
# El mismo agujero con el reborde rojo de cuando sale un gusano (gus = 1)
TILESET holeRedTiles "sprite/hole_red.png" NONE NONE ROW
# Los cuatro objetos que esperan en el suelo (oPaquete, oBoomPowUp,
# oNaveReplicaPowUp, oRayosPowUp): una animacion cada uno, tres frames de
# latido. Van como SPRITE y no al plano porque en el plano el color 0 pinta el
# fondo y recortaban un rectangulo en el agujero que tuvieran debajo.
SPRITE groundSprite "sprite/ground.png" 3 3 FAST
# Menu: iconos de arma (oMenu/Draw_0), logo del splash y barra del interludio
TILESET weaponTiles "sprite/weapons.png" NONE NONE ROW
# CIRCULAR y MOIRE, desde el arte de MonsterLite (Q11): python3 tools/ml_icons.py
TILESET weaponMlTiles "sprite/weapons_ml.png" NONE NONE ROW
# oInGameHUD: BULLETS y KILLS van con global.myFont, que es sFont2 a 16 px
TILESET hudFont    "sprite/hudfont.png" NONE NONE ROW
TILESET logoTiles   "sprite/logo.png" NONE NONE ROW
# El logo de Screen Implosion, desde el arte de MonsterLite: python3 tools/ml_logo.py
IMAGE silogoImage "sprite/silogo.png" NONE ALL
TILESET barTile     "sprite/bar.png" NONE NONE ROW

# ---------------------------------------------------------------------------
# Sonido. Los originales son Ogg Vorbis; tools/gm2md_audio.py los baja a PCM
# de 8 bits mono a 13300 Hz, que es lo que reproduce el driver XGM2.
# ---------------------------------------------------------------------------
WAV sfxShot       "sound/sfxShot.wav"       XGM2
WAV sfxWBlock     "sound/sfxWBlock.wav"     XGM2
WAV sfxLaser      "sound/sfxLaser.wav"      XGM2
WAV sfxThreeWay   "sound/sfxThreeWay.wav"   XGM2
WAV sfxCircular   "sound/sfxCircular.wav"   XGM2
WAV sfxBomba      "sound/sfxBomba.wav"      XGM2
WAV sfxRayos      "sound/sfxRayos.wav"      XGM2
WAV sfxBoom       "sound/sfxBoom.wav"       XGM2
WAV sfxPowUp      "sound/sfxPowUp.wav"      XGM2
WAV sfxRepetea    "sound/sfxRepetea.wav"    XGM2
WAV sfxRisa       "sound/sfxRisa.wav"       XGM2
WAV sfxAranaMorte "sound/sfxAranaMorte.wav" XGM2
WAV sfxMorte      "sound/sfxMorte.wav"      XGM2
WAV sfxDust       "sound/sfxDust.wav"       XGM2
WAV musMenu       "sound/musMenu.wav"       XGM2
# triggerSpeech(): una voz por arma al cambiarla en el menu
WAV spk0          "sound/spk0.wav"          XGM2
WAV spk1          "sound/spk1.wav"          XGM2
WAV spk2          "sound/spk2.wav"          XGM2
WAV spk3          "sound/spk3.wav"          XGM2
WAV spk4          "sound/spk4.wav"          XGM2
