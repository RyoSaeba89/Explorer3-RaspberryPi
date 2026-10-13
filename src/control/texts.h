/* Mission control's page texts, French and English (UTF-8): those of the web
 * page's TX object, word for word (the copy of the game's diffusion.cpp in
 * page.inc), extracted at build time into page_texts.inc in this order
 * (tools/prepare.sh on the Pi, tools/build.sh on Android). The players
 * read the same thing as in a web browser. The language is the crew's
 * (message L). */
#ifndef TEXTS_H
#define TEXTS_H

enum {
    TX_CONNECTING,       /* cx: "Connexion à l'équipage…" */
    TX_TEAM,             /* eq: at the top of each page */
    TX_RULES_TITLE,      /* tr */
    TX_RULES_1,          /* r1 (yellow) */
    TX_RULES_2,          /* r2 */
    TX_RULES_3,          /* r3 (red) */
    TX_RULES_4,          /* r4 (grey) */
    TX_P1_TITLE,         /* t1 */
    TX_P1_TEXT,          /* p1 */
    TX_P1_NOTE,          /* q1 (grey) */
    TX_P2_TITLE,         /* t2 */
    TX_P2_TEXT,          /* p2 */
    TX_P3_TITLE,         /* t3 */
    TX_P3_TEXT,          /* p3 */
    TX_P3_NOTE,          /* q3 (grey) */
    TX_P4_TITLE,         /* t4 */
    TX_P4_TEXT,          /* p4: the planets */
    TX_P4_HINT,          /* h4 (yellow): the planets */
    TX_P4_THEME_TEXT,    /* p4t: a maze theme (message M), whose hint comes with it */
    TX_P4_WRONG,         /* e4 (grey) */
    TX_ENTRANCE,         /* ent */
    TX_EXIT,             /* sor */
    TX_PLANET_1,         /* pl[0] .. pl[7], Mercury to Neptune */
    TX_PLANET_2,
    TX_PLANET_3,
    TX_PLANET_4,
    TX_PLANET_5,
    TX_PLANET_6,
    TX_PLANET_7,
    TX_PLANET_8,
    TX_PANEL_TITLE,      /* tc */
    TX_PANEL_HINT,       /* pc (grey) */
    TX_PANEL_TYPED,      /* sa */
    TX_COUNT
};

const char *tx(int id, int lang);   /* lang: 0 French, 1 English */

#endif
