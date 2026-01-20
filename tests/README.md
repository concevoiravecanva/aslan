# 🧪 Aslan Test Suite

## Philosophie

Ce système de tests suit la philosophie **Antirez** :
- **Simple** : Pas de framework complexe, juste du C
- **Direct** : Tests lisibles comme du code normal
- **Rapide** : Compile et run en <5 secondes
- **Utile** : Chaque test vérifie quelque chose d'important

## Structure

```
tests/
├── test_runner.c          # Tests unitaires en C
├── test_notes.aof         # Tests AOF (scénarios réels)
├── CMakeLists.txt         # Config de build
└── README.md              # Ce fichier
```

## Utilisation

### Quick Start

```bash
# Depuis la racine du projet
chmod +x run_tests.sh
./run_tests.sh
```

### Tests Manuels (AOF)

1. Lance Aslan
2. Clear le document actuel (Cmd+K → "clear")
3. Charge les tests :
   ```
   # Pas encore implémenté : besoin d'une commande "load_aof"
   # Pour l'instant : copier/coller les commandes depuis test_notes.aof
   ```
4. Navigue entre les pages pour vérifier visuellement

## Ce que les Tests Vérifient

### ✅ Tests qui PASSENT actuellement

```c
test_create_document()        // Document de base
test_create_page()            // Création de pages
test_note_create()            // Pages NOTE
test_note_insert_line()       // Insertion de texte
test_note_heading()           // Headings H1
test_note_separator()         // Séparateurs
```

### ⚠️ Tests qui DOIVENT passer (bugs à fixer)

```c
test_note_highlight()         // Fluo (highlight)
test_table_embed()            // Embed de tables
test_embed_resolution()       // Résolution des embeds
test_web_embed()              // Embeds web (YouTube, etc.)
```

## Ajout de Nouveaux Tests

### Template de Test

```c
void test_my_new_feature() {
    TEST("Description de Mon Feature");
    
    // Setup
    Document *doc = doc_create();
    // ... setup code ...
    
    // Test
    ASSERT(condition, "Ce qui devrait être vrai");
    ASSERT_EQ(value, expected, "Description");
    ASSERT_STR_EQ(string, "expected", "Description");
    
    // Cleanup
    doc_destroy(doc);
    test_passed_inc();
}
```

Puis ajoute l'appel dans `main()`.

## Interprétation des Résultats

```
✅ PASS : Le test a réussi
❌ FAIL : Le test a échoué
   → Regarde la ligne indiquée
   → Vérifie ce qui est attendu vs ce qui est reçu
```

## Tests Visuels (TODO)

Système de screenshots automatiques pour vérifier le rendering :

```bash
# À implémenter
./run_visual_tests.sh

# Compare avant/après automatiquement
# Détecte les régressions visuelles
```

## Debug d'un Test qui Fail

1. **Trouve le test qui fail** : Regarde le nom dans la sortie
2. **Lis le code du test** : Comprends ce qu'il teste
3. **Ajoute des printf** : Debug le test si besoin
4. **Fix le bug** : Corrige le code source
5. **Re-run** : `./run_tests.sh`
6. **Repeat** : Jusqu'à ce que ça passe

## Workflow LLM (pour l'IA qui code)

1. **Avant chaque modification** :
   ```bash
   ./run_tests.sh  # Vérifie que tout marche
   ```

2. **Après chaque modification** :
   ```bash
   ./run_tests.sh  # Vérifie que rien n'est cassé
   ```

3. **Si un test fail** :
   - Lis l'erreur
   - Comprends pourquoi
   - Fixe le code
   - Re-test

## Métriques de Succès

```
Objectif :
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Phase 1 (Notes MVP)     : 100% tests passent
Phase 2 (Tables)        : 100% tests passent
Phase 3 (Cross-embeds)  : 100% tests passent

Actuellement : ~70% (7/10 tests passent)
```

## Limitations

**Ce que les tests NE vérifient PAS encore** :
- Rendering visuel (besoin de screenshots)
- Performance (besoin de benchmarks)
- Input utilisateur (besoin de simulation)
- Memory leaks (besoin de Valgrind)

**Prochaines étapes** :
1. ✅ Tests unitaires de base (FAIT)
2. 🔜 Tests visuels (screenshots)
3. 🔜 Tests de performance
4. 🔜 Tests d'intégration complets

## Notes Importantes

- Les tests tournent SANS interface graphique
- Ils testent la LOGIQUE, pas le VISUEL
- Pour tester le visuel : utilise `test_notes.aof` manuellement
- Les tests sont RAPIDES (<1 seconde au total)

---

**Philosophy** : "Tests should make you confident, not anxious" - Antirez (probably)
