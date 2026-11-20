#ifndef _SAVE_H_
#define _SAVE_H_

#include <genesis.h>

// ===========================================================================
// Guardado en SRAM (bloque 6 de MonsterLite, docs/MONSTERLITE_GAP.md §2.10)
//
// MonsterLite guarda en PlayerPrefs el récord, el arma equipada, las
// partidas jugadas y el sonido y la música (setData, al entrar en game over:
// S/gameControl.cs:906-914, S/globales.cs:380-440), y en el inventario de
// Soomla el saldo (al recoger cada moneda) y las armas compradas (al
// comprar). La munición no se guarda (Q10).
//
// Aquí todo va en un bloque con firma, versión, número de secuencia y suma
// de control, en DOS copias que se escriben alternando: un apagado a mitad
// de escritura sólo puede estropear la que se estaba escribiendo, y al
// arrancar se carga la copia válida más reciente. Si ninguna vale (cartucho
// nuevo o SRAM corrupta), valores por defecto.
//
// La SRAM es de 8 KB en los bytes impares de 0x200001-0x203FFF (cabecera,
// src/rom_header.c). El ROM mide menos de 2 MB, así que no hace falta
// conmutar bancos.
// ===========================================================================

// Carga lo guardado en las variables del juego (g_lastScore, g_coins,
// g_weaponsBought, g_weaponIndex, g_maxKills, sonido y música). Si no hay
// nada válido, las deja como estén (los valores por defecto de Game_init).
void Save_load(void);

// Escribe el estado actual en la copia que toque.
void Save_write(void);

// Partidas jugadas (numberOfGames): sólo se cuenta y se guarda.
extern u32 g_gamesPlayed;

#endif
