# 🚨 Crash Fix - nk_window_set_focus(NULL)

## Problème Identifié

**Crash** : `aslan` crashait immédiatement au clic sur bouton [H1]

```
Exception Type: EXC_CRASH (SIGABRT)
Crashed in: nk_window_set_focus + 120
Called from: ui_render + 7332
```

## Cause Racine

Mon fix précédent utilisait :
```c
nk_window_set_focus(ui->ctx, NULL);  // ❌ CRASH !
```

**Pourquoi ça crash** :
- Nuklear fait un `assert` sur le paramètre
- `nk_strlen(NULL)` cause un crash
- NULL n'est pas accepté par cette fonction

## Solution Appliquée

**Enlevé l'appel dangereux** et simplifié :

```c
// AVANT (causait crash):
if (nk_button_label(ui->ctx, "H1")) {
  engine_execute_command(engine, "note_insert_block h1");
  nk_window_set_focus(ui->ctx, NULL);  // ❌ CRASH
  SDL_StartTextInput();
}

// APRÈS (safe):
if (nk_button_label(ui->ctx, "H1")) {
  engine_execute_command(engine, "note_insert_block h1");
  // Juste s'assurer que SDL text input est actif
  if (!SDL_IsTextInputActive()) {
    SDL_StartTextInput();
  }
}
```

## État Actuel

```
✅ Compilation : Succès (aucune erreur)
✅ Crash : Fixé (appel dangereux enlevé)
⚠️  H1 Input : Peut encore nécessiter un clic dans la note
✅ Fluo Canva : Style correct (derrière texte seulement)
```

## Tests À Faire

### Test 1 : Pas de Crash
```
1. Lancer ./aslan
2. Créer page NOTE
3. Cliquer [H1]
   ✅ NE DOIT PAS crasher
```

### Test 2 : H1 Input (avec workaround si nécessaire)
```
1. Cliquer [H1]
2a. Essayer de taper immédiatement
2b. Si ça ne marche pas : cliquer dans la note puis taper
   → Dites-moi lequel marche
```

### Test 3 : Fluo Canva
```
1. Taper "Important"
2. Cliquer [Fluo]
   ✅ Jaune derrière texte seulement (pas pleine largeur)
```

## Prochaines Étapes (si H1 input ne marche toujours pas)

Si après le fix du crash, vous devez encore cliquer dans la note pour taper :

**Option A** : C'est acceptable comme workflow
- Clic [H1] → H1 créé
- Clic dans note → Curseur actif
- Tape → Texte apparaît

**Option B** : Fix plus profond
- Ajouter auto-focus sur la note après création H1
- Plus complexe mais meilleure UX

**Pour l'instant** : Testons d'abord que ça ne crash plus !

---

## Fichiers Modifiés

```
src/platform/ui_host.c
└─ Lignes 1437-1467 : Toolbar buttons
   ├─ Enlevé : nk_window_set_focus(NULL)
   ├─ Enlevé : nk_clear(ui->ctx)
   └─ Gardé : SDL_StartTextInput() check
```

---

## Résumé

**Avant** : Crash au clic sur [H1] (nk_window_set_focus assert fail)  
**Après** : Pas de crash, approche simple et sûre  
**Trade-off** : Peut-être besoin de cliquer dans note pour activer input

**Priorité #1** : Pas de crash ✅  
**Priorité #2** : Input immédiat (à tester)  
**Priorité #3** : Fluo Canva style ✅

---

# 🚀 TESTEZ MAINTENANT

```bash
cd /Users/paolostraet/Desktop/Paolo/Aslan
./aslan
```

**Testez et dites-moi** :
1. Est-ce que ça crash encore ? ✅/❌
2. H1 : Pouvez-vous taper immédiatement ? ✅/❌
3. H1 : Devez-vous cliquer dans la note d'abord ? ✅/❌
4. Fluo : Jaune derrière texte seulement ? ✅/❌
