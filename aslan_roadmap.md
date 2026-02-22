# Aslan : Plan d'Action et Évaluation (Roadmap to v1)

## 1. Plan d'Action (Step Plan)

### Phase 1 : Amélioration majeure de la version "Design" (Objectif v1)
Actuellement, le module Design (`ui_design.c`) utilise Nuklear (un framework GUI en mode immédiat) pour faire de l'édition vectorielle. C'est fonctionnel mais rudimentaire par rapport à Canva/Figma.
*   **Étape 1.1 : Amélioration de l'UX/UI spatiale.** Implémenter le "snapping" (magnétisme) intelligent entre les objets, des guides d'alignement dynamiques et des boîtes de sélection multiples claires.
*   **Étape 1.2 : Tolérance et Undo/Redo fin.** S'assurer que chaque micro-action (déplacement, redimensionnement) est bien gérée par l'AOF (Append-Only File) de `engine.c` sans surcharger l'historique (gestion par batch lors du relâchement de la souris).
*   **Étape 1.3 : Gestion avancée du texte et de la typographie.** Intégrer un vrai moteur de rendu de texte (ex: FreeType) supportant le formatage riche, l'alignement, et l'espace inter-lettres, car Nuklear est trop limité pour un "vrai" outil de design.
*   **Étape 1.4 : Masques et Groupes.** Permettre de grouper des éléments de façon hiérarchique et d'appliquer des masques de découpe (clipping masks) pour des images à l'intérieur de formes.
*   **Étape 1.5 : Export haute fidélité.** Finaliser l'export PDF/SVG natif de l'artboard entier avec précision mathématique.

### Phase 2 : Version Téléchargeable (Desktop Release)
*   **Étape 2.1 : Abstraction de l'OS.** Remplacer les appels spécifiques à macOS (comme `osascript` pour les dialogues) par une librairie cross-platform comme `tinyfiledialogs` ou `nfd` (Native File Dialog).
*   **Étape 2.2 : Packaging macOS (.app ou .dmg).** Créer un bundle macOS natif avec une icône, signer l'application (Code Signing) et la notarier pour éviter les alertes de sécurité d'Apple.
*   **Étape 2.3 : Packaging Windows/Linux.** Compiler avec MSVC ou MinGW pour Windows, créer un installeur (.exe/.msi).
*   **Étape 2.4 : Gestion de la persistance.** S'assurer que le fichier `history.log` et les `assets/` (images) sont stockés dans les dossiers standards de l'OS (`~/.config/aslan` ou `%APPDATA%`) et non dans le dossier d'exécution.

### Phase 3 : Version Web (WebAssembly / Emscripten)
Étant donné que le projet est en C99 avec SDL2, il se prête *parfaitement* à WebAssembly.
*   **Étape 3.1 : Compilation avec Emscripten.** Configurer le Makefile pour utiliser `emcc`. SDL2 a un port natif via Emscripten (`-s USE_SDL=2`).
*   **Étape 3.2 : Boucle principale.** Adapter `main.c` pour utiliser `emscripten_set_main_loop` au lieu de la boucle `while(1)` classique, requis pour le navigateur.
*   **Étape 3.3 : Système de fichiers asynchrone.** Le plus grand défi : le navigateur ne permet pas l'I/O synchrone classique. Il faudra monter un `IDBFS` (IndexedDB) pour sauvegarder `history.log` de manière persistante dans le navigateur.
*   **Étape 3.4 : Dialogues de fichiers Web.** Créer des "ponts" JavaScript (JS hooks) pour déclencher `<input type="file">` lors de l'import d'images ou de l'export CSV/SVG au lieu des dialogues OS.

---

