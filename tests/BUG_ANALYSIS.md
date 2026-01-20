# 🐛 Bug Analysis - Notes Features

## État Actuel (Avant Fixes)

### Bug #1 : HIGHLIGHT (Fluo) ⚠️

**Symptômes** :
- Le surlignage jaune ne s'affiche pas correctement
- Background peut être mal positionné
- Peut ne pas apparaître du tout

**Analyse du Code** :

```c
// Dans renderer.c (lignes 366-388)
if (rt == ASLAN_ROW_HIGHLIGHT) {
    SDL_Rect highlight_rect = { 
        (int)margin_left - 4, 
        (int)(y_acc - scroll_y + 2),  // ← Possible bug ici
        (int)(r->width - margin_left - margin_right + 8), 
        (int)h - 4 
    };
    // ...
}
```

**Problème Probable** :
1. `y_acc` est calculé en boucle mais peut être décalé
2. Le rect est calculé AVANT le texte (2 passes)
3. Sync entre renderer.c et ui_host.c peut être cassé

**Test qui Doit Passer** :
```c
test_note_highlight()  // Actuellement : structure OK, rendering ?
```

**Fix Prévu** :
1. Vérifier sync de `y_acc` entre renderer.c et ui_host.c
2. S'assurer que le rect couvre toute la ligne
3. Vérifier l'ordre de rendering (background → text)

---

### Bug #2 : TABLE EMBED ❌

**Symptômes** :
- Table embed ne s'affiche pas dans une note
- Blank space ou crash
- Grid data not found

**Analyse du Code** :

```c
// Dans ui_host.c (lignes 2252-2324)
else if (row->type == ASLAN_ROW_GRID) {
    AslanTableViewBlock *tvb = (AslanTableViewBlock *)row->block;
    uint32_t src_page_id = tvb ? tvb->source_page_id : 0;
    
    // Find the source Grid page
    AslanGrid *grid = NULL;
    // ...
    for (int pi = 0; pi < 32; pi++) {
        if (engine->doc->pages[pi].page_id == src_page_id) {
            grid = engine->doc->pages[pi].grid;
            // ...
        }
    }
    
    if (grid && grid->rows > 0 && grid->cols > 0 && grid->data) {
        // Render grid preview
    }
}
```

**Problèmes Probables** :
1. `row->block` peut être NULL (pas initialisé)
2. `tvb->source_page_id` peut être 0 ou invalide
3. Grid page peut ne pas avoir `.grid` initialisé
4. Condition `grid->data` peut être NULL même si grid existe

**Test qui Doit Passer** :
```c
test_table_embed()         // Structure OK
test_embed_resolution()    // Résolution page OK
```

**Mais** : Ces tests vérifient juste la structure, pas le rendering.

**Fix Prévu** :
1. Vérifier que `row->block` est bien alloué lors de `note_ins_block`
2. Vérifier que `source_page_id` est set correctement
3. S'assurer que la page Grid a bien `.grid` non-NULL
4. Ajouter defensive checks avant de render

---

### Bug #3 : WEB EMBED ❌

**Symptômes** :
- Embed YouTube/web ne s'affiche pas
- Blank space
- URL pas visible

**Analyse du Code** :

```c
// Dans ui_host.c (lignes 2352-2445)
else if (row->type == ASLAN_ROW_EMBED) {
    float y_pos = top_y + get_line_y_position(active_note, i) - scroll_y;
    float embed_h = 320.0f;
    
    AslanEmbedBlock *eb = (AslanEmbedBlock *)row->block;
    const char *url = eb ? eb->url : "No URL";
    
    // Background
    nk_fill_rect(canvas, bounds, 4.0f, nk_rgb(245, 245, 243));
    nk_stroke_rect(canvas, bounds, 4.0f, 1.0f, nk_rgb(220, 220, 218));
    
    // Icon
    nk_fill_circle(canvas, icon_circle, nk_rgb(100, 100, 200));
    
    // URL text
    nk_draw_text(canvas, url_rect, url, ...);
}
```

**Problèmes Probables** :
1. `row->block` (AslanEmbedBlock) peut être NULL
2. `eb->url` peut être vide ou corrompu
3. Rendering peut être hors écran (y_pos mal calculé)
4. Font size peut être trop petit (URL illisible)

**Test qui Doit Passer** :
```c
test_web_embed()  // Structure OK, rendering ?
```

**Fix Prévu** :
1. Vérifier allocation de AslanEmbedBlock
2. Vérifier que l'URL est bien copiée
3. Vérifier positionnement (y_pos)
4. Augmenter font size pour URL

---

## Priorités de Fix

```
Priority 1 : TABLE EMBED (blocking feature)
├─ Fix allocation de AslanTableViewBlock
├─ Fix resolution de source_page_id
└─ Fix defensive checks dans rendering

Priority 2 : WEB EMBED (nice-to-have)
├─ Fix allocation de AslanEmbedBlock
├─ Fix URL display
└─ Fix positioning

Priority 3 : HIGHLIGHT (polish)
├─ Fix y_pos sync
└─ Fix rect dimensions
```

---

## Plan de Fix (Étape par Étape)

### Jour 1 : Diagnostic Approfondi

```
1. Run tests existants
   ./run_tests.sh
   
2. Identifier quel test fail exactement
   
3. Ajouter printf debug dans le code
   → renderer.c : printf y_pos de chaque highlight
   → ui_host.c : printf grid pointer lors de embed
   
4. Run Aslan manuellement avec test_notes.aof
   → Observer visuellement ce qui fail
   
5. Documenter findings
```

### Jour 2-3 : Fix TABLE EMBED

```
Étape 1 : Vérifier allocation (engine.c)
  → note_ins_block avec type GRID
  → S'assurer que AslanTableViewBlock est malloc()
  → S'assurer que source_page_id est set

Étape 2 : Vérifier Grid existence
  → Ajouter check : grid page existe ?
  → Ajouter check : grid->data non-NULL ?

Étape 3 : Fix rendering (ui_host.c)
  → Ajouter fallback si grid NULL
  → "Table not found" message
  → Proper bounds checking

Test après chaque étape :
  ./run_tests.sh && manual test
```

### Jour 4 : Fix WEB EMBED & HIGHLIGHT

```
Similar process :
1. Diagnostic
2. Fix allocation
3. Fix rendering
4. Test
```

---

## Metrics de Succès

**Avant** :
```
test_note_highlight()     : ⚠️  Structure OK, rendering suspect
test_table_embed()        : ⚠️  Structure OK, rendering broken
test_embed_resolution()   : ✅ OK (mais pas de render test)
test_web_embed()          : ⚠️  Structure OK, rendering broken
```

**Après (Goal)** :
```
test_note_highlight()     : ✅ Full pass (structure + rendering)
test_table_embed()        : ✅ Full pass
test_embed_resolution()   : ✅ OK
test_web_embed()          : ✅ Full pass

+ Visual tests confirmés manuellement
```

---

## Notes pour l'IA (moi)

**Workflow** :
1. Avant de coder → run tests → baseline
2. Code 1 fix
3. Run tests → vérifier pas de régression
4. Test manuel → vérifier visuel
5. Commit si OK
6. Repeat pour next bug

**Ne PAS faire** :
- Fixer tout en même temps (trop de variables)
- Coder sans tester (coding blind)
- Ignorer les tests qui fail
- Changer du code qui marche

**Faire** :
- 1 bug à la fois
- Test après chaque change
- Documenter findings
- Demander feedback visuel à l'humain si needed

---

**Next Step** : Run `./run_tests.sh` pour voir l'état actuel.
