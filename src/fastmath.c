#include "fastmath.h"

// ===========================================================================
// atan2 por tabla
//
// F16_getAngle de SGDK acaba en F16_atan2, que hace una división de 32 bits
// (rutina software del compilador, cientos de ciclos) y un polinomio de grado
// 7 con cuatro multiplicaciones más. Medido: ~3000 ciclos por llamada, y el
// bucle de juego la usa una vez por enemigo y frame.
//
// Esto hace lo mismo plegando el vector al primer octante y consultando una
// tabla de 257 entradas indexada por la razón menor/mayor en 1/256. La
// división es la del 68000 (divu.w, 32/16, ~140 ciclos) en vez de la de
// software, y no hay polinomio: ~200 ciclos por llamada.
//
// Error máximo frente a atan real: 0,23 grados (comprobado contra atan2 en
// 200000 vectores al azar; los ejes y las diagonales salen exactos). El rumbo
// sólo se usa para girar a velocidad limitada y para elegir uno de los
// 8 sprites pre-rotados, así que no se ve; queda anotado como desviación en
// docs/PORT_AUDIT_GML.md.
// ===========================================================================

// atanTab[i] = atan(i/256) en grados fix16 (0 a 45 grados = 0 a 2880).
static const u16 atanTab[257] =
{
       0,   14,   29,   43,   57,   72,   86,  100,
     115,  129,  143,  157,  172,  186,  200,  215,
     229,  243,  257,  272,  286,  300,  314,  329,
     343,  357,  371,  385,  399,  414,  428,  442,
     456,  470,  484,  498,  512,  526,  540,  554,
     568,  582,  596,  610,  624,  638,  652,  666,
     680,  693,  707,  721,  735,  749,  762,  776,
     790,  803,  817,  831,  844,  858,  871,  885,
     898,  912,  925,  939,  952,  965,  979,  992,
    1005, 1019, 1032, 1045, 1058, 1071, 1085, 1098,
    1111, 1124, 1137, 1150, 1163, 1176, 1188, 1201,
    1214, 1227, 1240, 1252, 1265, 1278, 1290, 1303,
    1316, 1328, 1341, 1353, 1366, 1378, 1390, 1403,
    1415, 1427, 1440, 1452, 1464, 1476, 1488, 1500,
    1512, 1524, 1536, 1548, 1560, 1572, 1584, 1596,
    1607, 1619, 1631, 1642, 1654, 1666, 1677, 1689,
    1700, 1712, 1723, 1734, 1746, 1757, 1768, 1779,
    1791, 1802, 1813, 1824, 1835, 1846, 1857, 1868,
    1879, 1890, 1901, 1911, 1922, 1933, 1944, 1954,
    1965, 1975, 1986, 1996, 2007, 2017, 2028, 2038,
    2048, 2059, 2069, 2079, 2089, 2099, 2109, 2120,
    2130, 2140, 2150, 2159, 2169, 2179, 2189, 2199,
    2209, 2218, 2228, 2238, 2247, 2257, 2266, 2276,
    2285, 2295, 2304, 2313, 2323, 2332, 2341, 2350,
    2360, 2369, 2378, 2387, 2396, 2405, 2414, 2423,
    2432, 2441, 2450, 2458, 2467, 2476, 2485, 2493,
    2502, 2511, 2519, 2528, 2536, 2545, 2553, 2562,
    2570, 2578, 2587, 2595, 2603, 2611, 2620, 2628,
    2636, 2644, 2652, 2660, 2668, 2676, 2684, 2692,
    2700, 2708, 2715, 2723, 2731, 2739, 2746, 2754,
    2762, 2769, 2777, 2784, 2792, 2800, 2807, 2814,
    2822, 2829, 2837, 2844, 2851, 2858, 2866, 2873,
    2880,
};

fix16 sinTab[450];

void Fast_init(void)
{
    u16 d;

    // Con el F16_sin de SGDK: la tabla queda con sus mismos valores.
    for (d = 0; d < 450; d++) sinTab[d] = F16_sin(FIX16(d % 360));
}

// divu.w del 68000: dividendo de 32 bits, divisor de 16, cociente de 16.
// GCC no la emite solo (llama a __udivsi3) porque no puede demostrar que el
// cociente cabe; aquí sí cabe siempre, porque num <= den.
static FORCE_INLINE u16 ratio256(u16 num, u16 den)
{
    u32 q = ((u32)num) << 8;

    asm volatile ("divu.w %1,%0" : "+d"(q) : "d"(den) : "cc");

    return (u16)q;
}

fix16 Fast_angle(fix16 x1, fix16 y1, fix16 x2, fix16 y2)
{
    const s16 dx = x2 - x1;
    const s16 dy = y2 - y1;
    const u16 ax = (dx < 0) ? -dx : dx;
    const u16 ay = (dy < 0) ? -dy : dy;
    fix16 a;

    if ((ax | ay) == 0) return 0;

    // Primer octante: la razón del lado corto sobre el largo.
    if (ax >= ay) a = (fix16)atanTab[ratio256(ay, ax)];
    else          a = FIX16(90) - (fix16)atanTab[ratio256(ax, ay)];

    // Y hacia abajo: el ángulo crece en sentido horario en pantalla.
    if (dx >= 0) return (dy >= 0) ? a : (FIX16(360) - a);
    return (dy >= 0) ? (FIX16(180) - a) : (FIX16(180) + a);
}
